# Handoff 2026-09-18e — the vendor's ECP paths, decoded and applied

## Staged on the card, never booted

`LS120MP.MPD` = **`52b7bf23` / code `d22aee88` / commit `1a2714d`, clean tree**,
built `-Phase 2 -Mode ecp`. Written to **both** `C:\LS120MP\` and
`C:\WINDOWS\SYSTEM\IOSUBSYS\` (CRC-verified, identical).

**Revert if it misbehaves:** `C:\LS120MP\LS120MP.SPP` is `d8154f1d`, the SPP
build that enumerates and mounts at `J:`. Copy it over `LS120MP.MPD` in both
places.

## The one thing that changed everything

**ECP needs the peripheral negotiated into 1284 ECP mode before the
descriptor.** CPP-connected is not enough. Without the negotiate the first
address cycle never drains the FIFO, the routine spins to its `0x8000` budget
and branches to teardown having emitted nothing — which is what every earlier
ECP attempt was doing. Proven by two runs one variable apart:
`VC1_vendor_descriptor_connected.OUT` (no negotiate, spins) and
`VC2_vendor_descriptor_negotiated.OUT` (negotiated, every wait passes first
poll). Negotiation acknowledged reads status `B8h`.

## The vendor's ECP paths, in full

Bounds came from the dispatch tables at file `0x4E9D` (read) and `0x4F15`
(write). The ECP entries hold **start and end**, not start and length:

| | register handler | block handler |
|---|---|---|
| ECP Read, selector 13 | `0x3CCE`–`0x3D59` | **`0x4627`–`0x4833`** |
| ECP Write, selector 6 | `0x4932`–`0x498B` | **`0x4CA3`–`0x4E4C`** |

Add `0x100` for DEBUG addresses. Disassembled with
`scratchpad/dis16.py` (capstone, 16-bit); data references inside the image are
**not** shifted, which is why `[0BFA]` is the port base in both views.

### Read, dbg `4727`–`4933`

```
park 14h ; ctrl 04 (fwd)
if [0C1A] == CX -> skip the descriptor entirely     ; length cache
ECP 74h ; [0C1A] = CX
  base+0 = 0Eh  / FIFO = 0Bh
  base+0 = 0Fh  / FIFO = count-high
  if [0C5C] > C3h:  base+0 = 0Bh / FIFO = count-low ; JBE, so strictly >
park 34h ; ctrl 04 (fwd) ; ECP 74h
  base+0 = 80h                                      ; block read, FORWARD
park 34h ; ctrl 20h (REVERSE) ; ECP 74h
  stream: aligned -> wait ECR bit1 FULL, rep insb 16
          else    -> wait ECR bit2 serviceIntr, one byte
ctrl 04 ; ECR 34h
```

Every write is bracketed by a wait on ECR bit 0, budget `0x8000`. **Count is
big-endian.** There is **no `A0h` last-byte command anywhere in the ECP read.**

### Write, dbg `4DA3`–`4F4C`

Same descriptor, entry park `14h`, version gate uses `JB` (`>= C3h`) where the
read path uses `JBE` (`> C3h`). Never leaves forward. Before the `C0h` block
command it requires **status `base+1` bit 3 (nFault) SET**, looping on ECR
bit 0. Streams `rep outsb` from `DS:SI` with `DS=ES`. Teardown is `ECR=34h`.

### `[0C5C]` is bridge register `0Bh`

Loaded at dbg `9F8D` (`MOV DX,000B`, then the register-read dispatcher at
`254C`). `epat.c` line 297 calls the same register "the version code".
**Measured on this bridge: `0Bh` = `C6h`**, so count-low is emitted here.

## The bridge's register space, `00h`–`17h`

Destination poisoned `EE` first, so an unread register cannot read as zero.

```
00: 00 99 99 99 99 99 99 00   08: 85 10 00 C6 00 00 00 00
10: F0 FF 00 00 00 00 80 02
```

⚠ Taken **without** the EPAT init writes (`WR(8,10) WR(0C,14) WR(0A,38)
WR(12,10)`, epat.c:241). `0Ah` selects what `0Bh` returns, so this is the
unselected view. `REGDUMP2.SCR` adds the init and has **not been run** — do
that first, and expect `0Bh` to be the authoritative version only then.

⛔ **There is no firmware dump path.** No download/upload switch in the vendor
driver (`/dpc /rx /wy /de /ded /db /dp /di /fe /fev /fp /ni /ix /IRQ /PORT /P
/sf /pd` is the complete list), no firmware blob in it, and `epat.c` knows only
two values for `0Ah` — `0x38` connected, `0x11` disconnected. The EPAT is a
mask-ROM part; `0Bh` is an 8-bit version code, not a window into code space.

## Three defects fixed in our driver (`e5c2fc4`)

1. **The `A0h` last-byte tail on the ECP read.** Ours flipped to forward, sent
   `A0h`, flipped back to reverse and read one byte. That is `epat.c`'s
   **nibble** shape grafted onto ECP, and the flip abandons the reverse channel
   the data arrives on. Removed; `ECP_CMD_LAST` deleted with it.
2. **The ECR park.** Commit `3e6bcf0` made it `14h` everywhere, off the write
   descriptor's *entry* park. Wrong — before a **direction change** the vendor
   parks at `34h`, because ECR mode 000 is not bidirectional and cannot carry a
   reverse request. Reverted; `3e6bcf0` is superseded, do not reapply it.
3. **The descriptor** — necessary, and now measured rather than inferred.
4. **The reverse read gated on the wrong ECR bit.** Ours waited on bit 0,
   FIFO-not-empty. Bit 0 describes the **host** FIFO; the vendor's reverse read
   waits on **bit 2, serviceIntr** (`48B2h`) — the reverse channel reporting a
   byte has arrived. `ECR_SERVICE equ 004h` was already defined in
   `LS120TR.ASM` and **referenced nowhere**: the right bit was worked out once
   and never wired up. Fixed in `13b7dd7`.

⚠ None of the four is confirmed to be *the* cause. Buffers left holding their
`EE` poison are equally consistent with nothing being sent at all. They are four
places where we provably diverge from a driver that works on this hardware.

## Not done, and why

**The corrected read is untested.** The DOS harness hung **four times, always
at the same point** - reproducible, not intermittent. The first hang
was my bug — the per-byte gate at `10E0` sets `CX = 0x8000` as its own spin
budget, so calling it inside a `LOOP` destroys the counter. `ECPDESC4.SCR`
fixes that with `PUSH`/`POP CX` and a `JC` timeout exit, and **still** hangs.
Each attempt needed a 5160 reset; `Ctrl+C` cannot reach a loop that polls no
DOS service.

⛔ **"Cumulative timeout cost" was my theory and it is WRONG.**
`ECPDESC5.SCR` cut the gate budget from `0x8000` to `0x0800`, 16x less, and it
hung identically. So it is a real infinite loop, not accumulated waiting.

What that leaves. The `1790` block is bounded on inspection - `JC 17C0` exits on
the first timeout, `LOOP 1790` is in range, `CX` is preserved across the gate,
and `17C0` sits clear in the `1770`-`1800` gap. Both exits are stack-balanced
against the caller's `PUSH CX`. So either the block is not assembling as read,
or **the hang is not in the read path at all** and removing `DEC CX / JCXZ 11F0`
changed the flow somewhere I have not traced.

**Do not spend more on this harness.** Four attempts, four 5160 resets, no
measurement. The driver is the only consumer that matters and it is already
staged on the card. If a DOS-side ECP check is ever wanted again, build it
fresh and small rather than patching `ECPBULK`'s 80-block descendant.

## Next, in order

1. **Boot the staged build.** It is the whole point of the card write. If the
   drive enumerates over ECP, the loop is closed.
2. If not, **single-step the vendor's `4727` on live hardware** — same recipe
   as `VCONN2.SCR`: relocate the CPP connect and the 1284 negotiate above the
   loaded image (it ends at `DC85`), `g=E000`, then set `IP`. That recipe
   worked for the descriptor and will work here.
3. Find why `ECPDESC4.SCR` hangs, or abandon the DOS harness for ECP and test
   only through the driver.
4. Still open from earlier: **the sense fix** — `ScsiStatus = 02h`,
   `SenseInfoBuffer`, `SRB_STATUS_AUTOSENSE_VALID`. That is what unblocks
   writes, and it is independent of all of the above.
5. Run `REGDUMP2.SCR` for the selected register view.

## Method notes

- ⛔ **`file_stat` can serve a stale directory entry.** It reported 0 bytes for
  three capture files that were 33 KB. Use `DIR` to confirm a size. I called a
  real measurement a harness failure on the strength of it.
- The dispatch tables mix two entry layouts: start/length for most selectors,
  **start/end** for the ECP ones. Read the pair before trusting either.

---

## ⛔ RESULT: the ECP build does NOT enumerate — owner-tested 2026-09-18

Booted `52b7bf23` / code `d22aee88` / commit `1a2714d` on the real 5160.
**No drive letter.** The CD sits at `D:`; the LS-120 never appears, so the
planned big-file copy could not be attempted at all.

The SPP build `d8154f1d` mounts the same drive at `J:` on the same machine.
Same driver, one flag. So **all four ECP fixes together still leave the ECP
path unable to complete enumeration**, and the regression is in ECP, not in
the surrounding miniport.

This does not retract any of the four — each is a measured divergence from the
vendor — but it does say plainly that they are **not sufficient**, and that
something in the ECP path fails before or during INQUIRY.

### Where to pick this up

1. ⭐ **Ship SPP.** `C:\LS120MP\LS120MP.SPP` (`d8154f1d`) is the build that
   works. Copy it over `LS120MP.MPD` in **both** `C:\LS120MP\` and
   `C:\WINDOWS\SYSTEM\IOSUBSYS\`. ECP is an optimisation; a mounting drive is
   the deliverable.
2. The earlier bed result is the sharpest clue available and matches this:
   **ECP failed INQUIRY with `SrbStatus 04` AFTER all 36 bytes had crossed the
   wire** — the failure is in the post-transfer status read, i.e. the 1284
   terminate, not the data phase.
3. **The sense fix is independent of all of this** and is what unblocks writes
   on the SPP path: `ScsiStatus = 02h`, copy sense into `SenseInfoBuffer`
   bounded by `SenseInfoBufferLength`, OR in `SRB_STATUS_AUTOSENSE_VALID`.
   Do that next - it is the one change with a known mechanism and a known
   payoff.
