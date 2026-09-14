# Next session - 2026-09-15

Supersedes the earlier 09-15 draft, which covered only the presence gate.
`next_session_2026_09_14.md` is now history: its truncation work is answered
below and its `/di` / `/w0` plan is overtaken.

## Start here

**The LS-120 reads and writes correctly from DOS on our own code path, with the
vendor driver removed.** Proven on real media. And the measurement that matters:

> **A single ATAPI data burst on this drive is 3584 bytes - 7 sectors.** A
> 4096-byte WRITE(10) is answered with a 3584-byte burst, the transfer is left
> mid-flight with DRQ still asserted, and the next command dies ABORTED COMMAND.

**`LS_MAX_XFER = 4096` is one sector over that, and neither the probe nor
`LS_PacketCommand` loops bursts.** So the driver would move 3584 bytes of a
4096-byte write, leave 512 behind, and report success. That is silent data loss
and it is the first thing to fix.

## Machine state

- CF in the 5160. **`CONFIG.SYS` has both `SD120PPD` lines REM'd** - `D:` does
  not exist. Restore with `copy C:\CONFIG.B4 C:\CONFIG.SYS` (backup verified,
  crc `18043ce5`, 397 bytes). `INBRDPC.SYS` untouched.
- The NOS disk is still in the drive and **still has its original filesystem** -
  `EB 3E 90 MSWIN4.0`, 512 b/sector, 4 sectors/cluster, 2 FATs of 241, 32
  sectors/track, 8 heads. `D:\FC.EXE` and the session's test files are gone from
  DOS's view only because the driver is unloaded, not because anything erased
  them.
- LBA 100000 holds a 00..FF ramp from the media write test.
- Driver binary on the card is still `220be39e`. The presence-gate build
  `9d89bcc3` (commit `560e9fd`) is **still not deployed and still never run**.
- Repo clean, nothing pushed.

## Proven today, on hardware

Vendor driver unloaded for all of it. Captures in
`docs/captures/2026-09-14_ls120/`.

| | evidence |
|---|---|
| register reads | status `50` (DRDY\|DSC), ATAPI signature |
| register writes | SRST pulses, drive goes BSY, raises unit attention |
| **block read** | LBA 0 = real FAT boot sector, `MSWIN4.0` … `WINBOOT SYS` `55 AA` |
| **block write, drive buffer** | WRITE BUFFER / READ BUFFER 512 bytes byte-exact, correct order |
| **block write, MEDIA** | WRITE(10) → READ(10) at LBA 100000, 512 bytes byte-exact |
| **it is on the platter** | re-read after SRST, buffer poisoned to `EE`, no write that run - same pattern |
| multi-sector | 7 sectors (3584) clean both ways; 8 sectors (4096) short-bursts |
| unit attention | two queued, ASC `29` then `28`, one cleared per REQUEST SENSE |

**All of it is SPP/nibble. ECP has not been exercised at all.**

## What the driver must change, with numbers

1. **Transfer ceiling.** Either cap at **3584** (7 sectors) or implement burst
   looping. Capping is one constant; looping is correct and is what `pf.c` does.
   Until one of them lands, any write over 3584 bytes loses data silently.
2. **The reset settle is a LONG wait and must not share a budget with a
   per-command poll.** Measured: `--spin 2000` (~0.29 s) aborts every command
   after SRST, deterministically, byte-identical across runs. `0xFFFF` (~2.3 s)
   works first time. So `LS_SPIN_RESET = 2000` (~92 ms), shipped this morning,
   cannot work - and `lt_coldstart` re-pulses SRST every retry, restarting the
   reset forever. `DESIGN.md` I3 and change 5.
3. **Drain the sense until it comes back clean**, not a fixed count. The drive
   queued two unit attentions and reads only worked once both were gone.
4. **Calibrate per transport** (`DESIGN.md` §5.2) - and note 3584 is an SPP
   number. ECP may differ; nothing says they share it.

## The format: attempted, void, and how to do it properly

FORMAT UNIT (CDB `04 00 …`, FmtData=0, no parameter list) was issued and the
packet ran to completion. **The result is void and the disk is unchanged.**

Two reasons, both mine:

- **`MARK` sat on the FORMAT UNIT result slot** (`SLOTS+0xA0` = `1EA0`), so its
  status and error were overwritten before they could be read. Fixed: `MARK` is
  `1F00` and the layout check now tests it against `SLOTS`.
- **The check run reset the drive.** Every probe starts with an SRST, and SRST
  aborts an in-progress format. The check ran 90 s after the fire - on a format
  the owner expects to take ~10 minutes, that run would have killed it.

`--noreset` now exists for exactly this. The correct sequence:

1. `--formatunit` to fire it, and **read the slot** to confirm the drive accepted
   the command rather than rejecting it.
2. Leave it alone for ~10 minutes. Do not run anything that resets.
3. `--noreset` poll runs to watch BSY clear.
4. Then an ordinary probe to read LBA 0 and see a blank surface.

⚠ If the drive rejects FmtData=0, the parameter-list form needs sourcing from a
primary reference before being tried on real media - do not guess a defect-list
header.

## `FORMAT.COM` vs FORMAT UNIT

Different things, and the distinction decided the plan:

- **FORMAT UNIT** is a raw ATAPI command. No drive letter, no mount, no bulk
  transfer - the drive formats its own surface internally, so transport speed
  barely matters. The probe can issue it.
- **`FORMAT D:`** is a filesystem tool and needs a block device. That means the
  vendor driver, or ours once it loads under Windows.

A full "clean bill of health" is really both: FORMAT UNIT for the surface, then
filesystem structures, which need a block driver.

⚠ An earlier claim in this session that a format would take ~47 minutes was
**wrong twice over** - it assumed nibble when the card is ECP-capable, and it
assumed the host streams every sector when FORMAT UNIT is drive-side. Roughly
10 minutes, drive-bound, is the right expectation.

## The probe had four bugs, all of the same family

`gen_rw_probe.py` had never run successfully - there is no `RW.OUT` beside the
09-11 INQUIRY captures. Four defects, every one of them **something that fails
by silently not existing**:

1. `jcxz BRE` is +158 from the jump; a short jump reaches +127. DEBUG prints an
   error, **re-prompts at the same address and carries on without the
   instruction**, so the guard its own comment calls load-bearing was absent.
   With `cx=0` the read loop ran 65536 times over the probe's own code. Three
   wedges, three reboots.
2. `MARK` at `0580` and slots at `0600-067F` sat inside a main block that had
   grown to `0716` - the probe wrote results into its own instruction stream.
3. Adding a CDB pushed the media READ(10) onto `1D00`, which was `SLOTS`.
4. `MARK` at `1EA0` landed on the FORMAT UNIT slot.

The layout check now measures every short jump, and tests data against code,
against the CDB table, and against the other data regions. **Each guard was
verified to fail on the bug it was written for** before being trusted.

## ECP: register level proven, block level not

Added after the SPP work, same session, vendor driver still absent.

**Register level works.** `ECPPROBE.COM` (`drivers/imation_ls120/tools/ecpprobe.py`)
on clean hardware:

| slot | value | meaning |
|---|---|---|
| `neg_flag` / `neg_stat` | `00` / `B8` | **IEEE-1284 ECP negotiation succeeded** |
| `w1_flag` `w2_flag` `w3_flag` | all `00` | forward drain, address cycle **and the reverse wait** all completed |
| `ecp_ecr_mode` / `ecp_ecr_rev` | `75` / `74` | ECR tracks the mode through the turnaround |
| `ecp_byte` | `14` | **correct** - the address cycle targets BCLO (line 492), and the nibble read of the same register also says `14` |

⚠ The slot comment says `expect 50` and the docstring says it replays a status
read. **Both are stale** - the code addresses `CONT_TASKFILE + ATA_REG_BCLO`.
Two transports, one register, same answer is the real result here.

**Block level does not work.** `--ecp --sectors 7` against the media:

- read-back buffer untouched at its `EE` poison - nothing transferred, and the
  poison is why that is unambiguous rather than a stale success
- every nibble status read after the ECP episode returns **`F5`**, which is not
  a valid ATA status
- the phases *before* it are fine: interrupt reason `01` before the CDB, as
  required. So the ECP episode itself is what leaves the port unusable
- **transient** - the next SPP run is byte-exact again, because `LS_PortOpen`
  recovers the port. Nothing is damaged

One fix was applied and is correct in itself: the probe's `LS_EcpLeave`
transliteration stored `34h`, a bare `04h` and a guessed `15h` where the driver
does a **read-modify-write** (control: keep IRQEN, set forward; ECR: `AND 1Fh`).
Technique 118. It changed nothing, so the cause is earlier.

### CAUSE FOUND - it is architectural, and it was in our own spec

**ECP is not a block transport on this bridge.** The vendor's mode tables say `ECP Read`
(handler `3CCEh`) and `ECP Write` (`4932h`) each take a **register number** - per-register ECP
is its normal mode. And Linux `epat.c` has **no ECP mode at all**: 4-bit, 5/3, 8-bit, EPP-8/16/32.

The block path is **EPP**, which `TRANSPORT_SPEC.md` §4f-corrected states and `epat.c` mode 3
confirms byte for byte:

```c
w3(0x80); w2(0x24);                      // 0x80 -> EPP ADDRESS register at base+3
for (k=0;k<count-1;k++) buf[k] = r4();   // data from EPP DATA register at base+4
w2(4); w3(0xa0); w2(0x24);               // 0xA0 = last byte
buf[count-1] = r4();
w2(4);
```

Same `80h`/`A0h` command bytes our ECP code uses - but to **base+3**, with data at **base+4**.
`LS_BlockReadEcp` sends them to the ECP FIFO at base+0 and reads base+400h. Wrong ports.

So the correct architecture is:

| | transport |
|---|---|
| registers | **ECP per register** (`3CCEh` / `4932h`), nibble as fallback |
| block data | **EPP**, `epat.c` mode 3 |

Our driver has **neither**: nibble registers (works, slow) and ECP-block (does not work).
**The speed win for data is EPP, not ECP.**

⚠ This was already written down. `IMPLEMENTATION.md` §5 carried the retracted claim ("ECP
belongs to the DATA phase only... ship nibble registers + ECP `rep insb` for sector data") while
`TRANSPORT_SPEC.md`'s index had retracted it on 09-13. I built the probe from the stale file.
§5 is now corrected and points at the right one.

### Second bug, independent of the above

**Every ECP failure path must restore `ECR = 0x34` and un-reverse the control port.** The spec
is explicit: *"an abandoned reverse channel wedges the port on this machine, so the failure
paths matter more than the happy path."* My transliteration has **no `jc` after `call ECMD`**
and none after `call EWD`, so a failed phase carries on with the port reversed and leaves it
there. That is the `F5`.

### If it still misbehaves, bisect rather than guess

One fix-and-run has already failed. The order:

1. **Negotiation alone** - call `ENEG`, then immediately a nibble status read.
   `50` means negotiation is innocent; `F5` means it is the culprit and the
   transliteration or the sequence is wrong.
2. If innocent, add `EcpDir` alone, re-read status.
3. Then `EcpCmd` alone. Then the FIFO transfer.

Each step is one flag and one run, and each one halves the space. The probe's
blocks are at `1000`-`12FF` and the driver's originals are `LS_Neg1284`,
`LS_EcpDir`, `LS_EcpCmd`, `LS_EcpLeave`, `LS_BlockReadEcp`, `LS_BlockWriteEcp`.

## FORMAT UNIT is refused

`04 00 ...` - FmtData=0, "default parameters", no parameter list - comes back
**status 51, error 54: sense key 5, ILLEGAL REQUEST**. The parameter-list form
(SFF-8070i) is needed and **must be sourced, not guessed** on real media.

Route the owner can take meanwhile: restore the vendor driver briefly,
`FORMAT D:`, re-REM it. The surface format is drive-side so the vendor's
truncation does not touch it, and this disk's current FAT16 came from exactly
that route.

## Next, in order

1. **Fix the transfer ceiling** - the data-loss bug. Cap at 3584 or loop bursts.
2. **Fix the reset settle** - it needs seconds, as a state, pulsed once.
3. **Repeat the whole DOS proof on ECP.** It is the untested half and the owner
   has asked for both. `LS_NegotiateEcp` exists in the driver; the probe has no
   ECP path at all and needs one.
4. **Format properly**, per the sequence above - between the SPP and ECP proofs,
   so ECP verifies against a known-clean surface.
5. Then Windows: deploy `9d89bcc3`, boot with the vendor still REM'd, and see
   whether the driver loads now that nothing else owns `0x378`.
