# LS120MP.MPD — build specification

**Everything needed to build this driver, in one place, derived from source.** Written
2026-09-11 after a day lost to trial-and-error on the owner's machine while the answers sat
in binaries we already had.

**Read this before writing code or running anything on hardware.** If something here is
wrong, fix it here — do not discover it again on the bench.

Sources, all **local**, none to be fetched:

| source | what it gives |
|---|---|
| `../imation_ls120/SD120PPD_MPD.asm` | the vendor's Win95 miniport, **fully disassembled** |
| `../imation_ls120/SD120PPD_SYS.asm` | the vendor's DOS driver, disassembled |
| `../imation_ls120/DISASSEMBLY.md` | what was read out of both, with proof of coverage |
| `../imation_ls120/TRANSPORT_SPEC.md` | the wire protocol. **Read its index first** |
| `reference_gpl/epat.c` | Linux paride EPAT — the bridge |
| `reference_gpl/pf_extract.c` | Linux `pf.c` — ATAPI floppy command sequence |
| `Windows95_ddk/BLOCK/SAMPLES/MINIPORT/PC2X/PC2X.C` | a shipping polled parallel miniport |

---

## 1. Architecture — this is the blocker, not a tuning issue

Read out of `SD120PPD.MPD` (`DISASSEMBLY.md` §"Verified facts"), and independently confirmed
by the DDK's `PC2X.C`. **Two independent implementations agree and we match neither.**

| | vendor | PC2X | ours today |
|---|---|---|---|
| `HwInitialize` | **no device I/O**, 20 instructions, returns TRUE | same shape | reset + INQUIRY + sense drain |
| `HwStartIo` | starts work, **returns without completing** | same | runs the whole command inline |
| waiting | **self-re-arming 1 ms timer** | `PC2xTimer` | blocking spin loops |
| SRB functions | 4 | 3 | 1 |

### 1.1 `HwInitialize` must do nothing

```
mov cx,[config word] / stash ConfigInfo / three setup calls
one ScsiPortNotification / zero two fields / mov al,1 / ret 4
```

**No reset. No INQUIRY. No sense. It cannot fail.** Every second spent here is a second the
OS is blocked during boot — that is what turned the sense-drain build into a boot stall.

### 1.2 `HwStartIo` starts and returns

Dispatch on `Srb->Function`:

| function | action |
|---|---|
| `0x00` EXECUTE_SCSI | start the command, arm the timer, **return** |
| `0x02` IO_CONTROL | vendor handles it |
| `0x10` ABORT_COMMAND | the class driver's recovery path |
| `0x12` RESET_BUS | ditto |
| anything else | `SrbStatus = SRB_STATUS_INVALID_REQUEST`, complete immediately |

### 1.3 The engine is a timer callback that re-arms itself

Vendor, rva `0x48bb`:

```
cmp  [state], 0           ; still running?
call [per-tick hook]
push [interval]           ; ~1 ms
push 0x148bb              ; ITS OWN linked address (ImageBase 0x10000 + rva 0x48bb)
push <DeviceExtension>
push 6                    ; RequestTimerCall
call ScsiPortNotification
```

reached from `0x2840`, which calls `HwInterrupt` (`0x2892`) and **re-arms only when that
returns FALSE**. Poll, and try again next tick.

**This is where the unit-attention retry and the spin-up wait belong** — asynchronously,
costing the system nothing. Not in a spin loop.

⚠ `ScsiPortStallExecution` **is** imported by the vendor, so short in-command delays are
legitimate. It is the multi-second waits that must be deferred.

---

## 2. The ATAPI command sequence

Transliterated from `pf.c`'s `pf_command` / `pf_completion` (`reference_gpl/pf_extract.c`).
Already in `LS120TR.ASM`; listed here so it is not re-derived.

```
select device 0 (A0 -> drive/head)
pf_wait(go = BSY|DRQ, stop = 0)            ; BOTH clear before starting
write byte-count low / high                ; how much we will accept
write PACKET (A0) to the command register
pf_wait(go = BSY, stop = DRQ)              ; device asks for the CDB
IF interrupt reason != 1 -> COMMAND PHASE ERROR, abort
block-write the 12-byte CDB                ; NOT twelve register writes
delay ~1 ms                                ; pf_atapi's mdelay(1)
pf_wait(go = BSY, stop = DRQ|READY|ERR)
IF (interrupt reason & 2) AND (status & DRQ):
    n = ((bclo + 256*bchi) + 3) & 0xFFFC   ; what the DEVICE offers
    clamp n to the caller's buffer
    block-read n
pf_wait(go = BSY, stop = READY|ERR)
```

**Four things here were improvised before and were all wrong:**

1. no wait for DRQ to *clear* before a command
2. no check that interrupt reason reads exactly **1** before the CDB
3. no delay between packet and reply
4. **reading the length we asked for instead of the count the device offers** — which leaves
   the bridge mid-stream so the *next* command dies in its command phase (sense `0B`/ASC `4A`)

---

## 3. Error handling — the thing that caused "media is not formatted"

**A CHECK CONDITION is cleared by READING THE SENSE, not by the next command.** Until
REQUEST SENSE is issued, every subsequent command returns the identical error.

**The device queues more than one.** Measured: ASC `28` (medium may have changed) and ASC
`29` (power on / reset — raised by our own SRST), reported one per REQUEST SENSE.

⚠ **INQUIRY and REQUEST SENSE are exempt from a pending unit attention.** That is why
bring-up looked healthy — the INQUIRY succeeded, `Init Success` was logged, the drive
enumerated — and the class driver's very first READ was refused.

Required behaviour:

- on ERR: set `Srb->ScsiStatus = 2` (CHECK CONDITION) **and** `SrbStatus = SRB_STATUS_ERROR`
- fill `SenseInfoBuffer` and set `SRB_STATUS_AUTOSENSE_VALID`, unless
  `SRB_FLAGS_DISABLE_AUTOSENSE` is set or no buffer was supplied
- retry up to 5 times (`pf.c`'s `PF_MAX_RETRIES`), **with a REQUEST SENSE between attempts**
- **do all of this from the timer handler**, not inline

`PC2X.C` confirms the reporting half: `srb->ScsiStatus = status` "for the driver above",
`CHECK_CONDITION -> SRB_STATUS_ERROR`. It does not autosense; ATAPI makes that cheap, so we
do.

---

## 4. Timing — measured, do not guess

| thing | value | source |
|---|---|---|
| one 8-bit I/O access | **5.55 us** | technique 109, measured twice |
| one nibble register read (≈8 accesses) | **~44 us** | derived |
| `pf.c` command timeout | **8 s** (`PF_SPIN`) | `pf_extract.c` |
| our `LS_SPIN_BSY` = 22000 | ~1.0 s | **too short** — a spinning-up drive returns BSY with a **clean error register** |

**Do not fix this by raising the constant.** An 8-second wait inside `HwStartIo` is
unacceptable; that is what §1.3 is for.

---

## 5. Transports

| | now | target |
|---|---|---|
| read | `NIBBLE Normal` — selector 0, ~4 accesses/byte | **`ECP Read`** |
| write | `WRITE Normal` — selector 0 | **`ECP Write`** |

The card is **ECP-capable** (`port type = 0C`) and the vendor negotiates **ECP for both
directions on this machine** — `TRANSPORT_SPEC.md` §0, §4f, re-confirmed live 2026-09-11.

⚠ **ECP belongs to the DATA phase only.** Per-register ECP reads time out *by design* — a
register read has no data phase, so the reverse FIFO never fills. Ship **nibble registers +
ECP `rep insb`/`rep outsb` for sector data** (§4f).

⚠⚠ **Do NOT adopt the vendor's DMA block path.** It writes `0x22`/`0x23`, which alias onto
the 8259 on this XT — the #22 keyboard-killer — and those writes are in the **transfer**
path, not the chipset init that `/ni` skips.

---

## 6. Bring-up — solved, do not re-derive

The drive powers up **held in ATA reset**. Pulse SRST (device control, container offset
`0x16`: `0x04` then `0x00`), poll BSY, and it answers cold with the ATAPI signature `14 EB`.
`TRANSPORT_SPEC.md` §5.

**But this belongs in the timer handler or the first SRB, not in `HwInitialize`** (§1.1),
and it raises a unit attention that must then be drained (§3).

---

## 7. Known-wrong things that must not come back

| | why |
|---|---|
| Device I/O in `HwInitialize` | stalls the boot |
| Inline completion in `HwStartIo` | the whole architecture problem |
| Interrupts **on** during a block transfer | drive reports sense `0B` / ASC `4A`, COMMAND PHASE ERROR. `cli` stays on the block paths **in the driver** |
| Twelve register writes for the CDB | malformed packet; drive completes with no data and no error |
| Reading the requested length instead of the offered count | strands the bridge; next command fails |
| Raising spin constants to fix a timeout | see §4 |
| `MaximumTransferLength = 65536` with 16-bit byte-count registers | a 65536-byte request programmes byte count **0** |

---

## 8. Order of work

1. **Restructure to §1.** Nothing else matters until `HwInitialize` is empty and `HwStartIo`
   returns.
2. **§3 error handling in the timer handler.** Unit attention is why the media reads failed.
3. **Prove reads, then writes**, from DOS first where possible — but only against something
   this document does not already answer.
4. **Then ECP (§5).** A working nibble driver is still several times slower than the vendor's.
5. `ABORT_COMMAND` / `RESET_BUS` (§1.2).

## 9. Testing rules

- **Check this document and `TRANSPORT_SPEC.md`'s index before any hardware run.** Twice on
  2026-09-11 a boot was spent re-deriving something already written down.
- Probe and driver must exercise **the same code path**, or the probe proves nothing — the
  probes wrote the CDB as register writes while the driver used block write, and that
  difference hid the real bug for two sessions.
- **No `cli` in a DOS probe.** A runaway inside one never reaches its `sti`, so interrupts
  stay off and the machine cannot even be asked where it died. (The **driver** keeps `cli`
  on block paths — see §7. Different context, opposite rule.)
- Clamp any length that came from the device; validate DEBUG block layout before emitting;
  every bounded loop needs an explicit jump on the timeout path.
- Build with a clean tree so the binary is traceable (`build_ledger.tsv`).
