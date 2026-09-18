# Handoff 2026-09-18f — the vendor transport model, and byte mode half-proven

## Live on the card

`LS120MP.MPD` in `C:\LS120MP\` and `C:\WINDOWS\SYSTEM\IOSUBSYS\` is
**`a61aec4f` / code `f8375ad1` / commit `2b734bb`**, built
`-Phase 2 -Mode spp -ByteMode`. **It does not enumerate.**

**Revert first, from DOS:**

```
COPY C:\LS120MP\LS120MP.SPP C:\LS120MP\LS120MP.MPD
COPY C:\LS120MP\LS120MP.SPP C:\WINDOWS\SYSTEM\IOSUBSYS\LS120MP.MPD
```

`LS120MP.SPP` is `d8154f1d`, the build that mounts at `J:`. Spares on the card:
`.E18` = `52b7bf23` (the old ECP build), `.B29` = today's terminate fix,
`.B30` = this byte-mode build.

## ⭐ The strategic finding: stop mixing transports

Read out of `SD120PPD.SYS`, not inferred. The vendor and this driver differ in
one structural way, and it explains a family of symptoms:

| | vendor | us |
|---|---|---|
| task-file reads | **data port**, one access | **nibble**, two status reads + merge |
| ECR parked at | `34h` — mode 001, bidirectional | `and 1Fh` — mode **000**, unidirectional |
| ECP teardown | `ctrl 04` then `ECR 34h`, **nothing else** (`0x481D`) | + a full 1284 terminate |

We park at mode 000 *because* we read registers as nibbles. That forces a
mode transition the vendor never makes, and the 1284 terminate exists only to
survive it. **Remove the nibble register read and the terminate problem may
stop existing rather than needing to be fixed.** That was the plan:

1. registers to PS/2 byte mode (`epat.c` mode 2)
2. park at `34h`, drop mode 000, drop the terminate
3. ECP bulk, now that the transport is homogeneous

## Stage 1: proven in DOS, fails in the driver

**Proven on the 5160** (`docs/captures/2026-09-18_ls120/`):

- `BM1_bytemode_matches_nibble.OUT` — one register, byte mode vs nibble in the
  same run: both `51h`, plus ATAPI signature `14 EB`.
- `BM2_inquiry_bytemode.OUT` vs `BM2_inquiry_nibble_control.OUT` — a full
  INQUIRY, byte-identical output, same boot. `MATSHITA LS-120 COSM   04`.

**The driver build does not enumerate.** `BOOTLOG_bytemode_b30.TXT` shows
`Initing ls120mp.mpd` → `Init Success` in 2 ticks, which is normal for this
driver's non-blocking init and therefore says nothing.

## ⛔ Three explanations tried and KILLED — do not re-derive

| theory | killed by |
|---|---|
| the ECR park does not persist, so each read must set it | `INQPARK.OUT` — park set once *before* connect, read back `35h` at the end, INQUIRY correct |
| single port accesses are too fast; the vendor repeats every access 2-4x (`0x3A90`, `0x3B17`, `0x3B52`) and our nibble path doubles its writes | same run — single accesses returned correct data |
| parking bidirectional breaks the nibble BULK read, which is inline and not via `LS_RegRead` | same run — INQ9's data loop at `0550` *is* inline nibble and returned correct data at ECR `34h` |

Also checked and clean: `ECR_KEEP_MASK` is `034h`, which **keeps** bit 5, so
the masking in `LS_OpenReset`/`LS_BringUp`/`LS_PortOpen` preserves mode 001.
And **no vendor register handler touches the ECR at all** — 15 handlers, read
and write, every mode. Park-once is the vendor's own model.

The byte-mode read is verified present in the linked binary at `0x696`
(technique 91). So the primitive is right and the driver's *use* of it is not.

## ⭐ Next, in order

1. **Bisect it properly.** Revert to `d8154f1d` and confirm `J:` returns. We
   have NOT established that the byte-mode change is the cause — the drive was
   also not power-cycled before that boot. One boot settles it. Technique 88.
2. If `J:` returns, the cause is in the driver's use of byte mode, and **we
   have no way to see it**: the trace ring is dead on hardware and there is no
   readback under Windows. That is the structural blocker, and it is worth
   solving before another build. The drive's own buffer (`WRITE BUFFER` /
   `READ BUFFER`) is the standing candidate — its capacity is still unmeasured,
   and `READ BUFFER` mode 3 returns it in 4 bytes.
3. Only then Stage 2/3.

## Traps paid for tonight

- **A DEBUG `a` directive needs a BLANK LINE before it.** Without one DEBUG
  never leaves assembly mode, swallows the directive as a failed instruction,
  assembles the whole next block into the *previous* block's space, and leaves
  the target address empty — `g=` then runs garbage and the console is
  unrecoverable (its stdin is the file, so Ctrl+C cannot reach it). Cost: one
  reboot. Guarded in `tools/gen_bytemode.py`.
- **A layout guard must COMPUTE extents, not assert them.** `a E300` assembled
  to `E351` where the guard had been told `E33F`. Guarded now by instruction
  count with a conservative bound — and scoped to blocks the generator itself
  creates, because that bound false-positives on hand-packed existing code.
- **Do not round-trip a PowerShell file through Python text mode.** `build.ps1`
  contains a regex with a literal CR in it (`\r?$` written as an actual
  carriage return); universal-newline reading turned it into `\n` and silently
  broke INF stamping. Read and write in binary.
- **`md5` identifies a LINK, not a build** — see technique 89a. The PE
  `TimeDateStamp` moves every link. Use the code hash.

---

## ⛔ POST-REVERT: `d8154f1d` NO LONGER ENUMERATES EITHER

The bisect in "Next, in order" was run and **did not clear the driver**. With
`d8154f1d` byte-verified in both locations - the exact binary that mounted at
`J:` earlier on 2026-09-18 - there is still no drive letter, and Explorer shows
the hourglass again.

**So tonight's byte-mode result is uninterpretable.** The byte-mode build may
be fine; we cannot tell, because the control does not work either.

### Checked, and all clean - do not re-check these

| | |
|---|---|
| driver binary | `crc32 2a64e85c` at BOTH destinations, matches `dist/ls120_mpd/LS120MP.MPD` |
| LS-120 adapter node | ONE node, `ForcedConfig 0278-027F`, `AdapterSettings PORT=0x378`, `ConfigFlags = 0x04` (MANUAL_INSTALL - not disabled, not failed) |
| competing claim on `0378` | **none** - the deleted LPT1 printer port has NOT been re-detected |
| the drive | `MATSHITA LS-120 COSM   04` present TWICE as a device node, `ConfigFlags = 0` - Windows has enumerated it before and remembers it |
| `IOS.LOG` | does not exist - IOS is not refusing anything |
| the drive itself | power-cycled before the run, and a full INQUIRY from DOS returns `MATSHITA` |
| bridge left dirty by a probe | ruled out - the owner power-cycled before this boot |

⚠ An earlier scan of this hive reported TWO LS-120 nodes at `0278` and `0378`.
**That was a scan artefact** - the context windows were +-420 bytes and the two
`LS-120 EPAT` strings are 522 bytes apart, so one node was counted twice
(technique 65's record-bleed warning). There is one node.

### What that leaves

The last CONFIRMED `J:` predates this session. Between then and now the card
has taken several ECP builds that strand the peripheral, a DEBUG run that
executed unassembled memory for minutes, and multiple boots. Something in the
install has drifted in a way `SYSTEM.DAT` does not show.

**Next session should not build a DRIVER blind.** But the readback problem is
NOT a wall - the owner's point, and he is right: **the trace ring works in the
bed**, and everything learned tonight can be emulated. Build the MODEL, then
the driver question answers itself with full visibility and no hardware.

### ⭐ FIRST JOB: teach the bed's EPAT model PS/2 byte mode

`86box_upstream/src/device/lpt_epat.c` implements nibble register access only:

```
read : w0(r); w2(1); w2(3); a = r1(); w2(4); b = r1();
write: w0(0x60 + r); w2(1); w0(val); w2(4)
```

The `0x60` WRITE tag exists; there is **no `0x20` byte-mode READ path**. So
running the byte-mode driver in the bed today fails for a MODEL reason and
looks like a driver bug - technique 90 exactly.

Adding it is fully specified by tonight's captures, which are byte-identical
hardware ground truth:

- `BM1_bytemode_matches_nibble.OUT` - one register, byte mode == nibble
- `BM2_inquiry_bytemode.OUT` - a whole INQUIRY, byte mode
- `BM2_inquiry_nibble_control.OUT` - the same INQUIRY, nibble, same boot

The sequence to model, from `epat.c` mode 2 and verified on the 5160:
`w0(0x20 + r); w2(1); w2(0x25); a = r0(); w2(4)` - the value returns on the
DATA port in one access, with the ECR parked at `34h` (mode 001).

**Validate it the way technique 116 requires: replay `BYTEMODE.SCR` and
`INQBYTE.SCR` UNMODIFIED against the model and require the same bytes the
hardware returned.** If the probe has to be edited for the bed, the two sides
are no longer exercising the same path and the result proves nothing.

Then build the driver `-ByteMode -Trace`, run it in the bed, and read the ring.
That is the visibility that does not exist on hardware.

⚠ The bed CANNOT answer why the real install stopped enumerating - it has its
own image. That is a separate hunt (below), and the two must not be conflated.

Two further candidates, in order:

1. **Get a readback channel under Windows.** Technique 123 - it was the right
   call a week ago and it is still unpaid. Without it, every Windows result is
   one bit and the diagnosis cannot advance. The drive's own buffer
   (`WRITE BUFFER` / `READ BUFFER`) remains the candidate; its capacity is
   still unmeasured and `READ BUFFER` mode 3 returns it in four bytes.
2. **Compare against a known-good image** rather than the live install. If a
   snapshot from when `J:` worked exists, diff `SYSTEM.INI`, `IOS.INI` and
   `USER.DAT` against it - none of which were examined tonight.

### And a genuinely new direction, from the owner

**EPP, not ECP.** `reference_gpl/epat.c` has **no ECP path at all**: modes 0-2
are nibble/byte/PS2 and modes **3-5 are EPP** (`w3()`/`r4()`), with a negotiate
sending `0x40` - "Request EPP Mode" in AN062's table. The Linux driver for this
exact Shuttle bridge chose EPP for its fast modes, and this project already
measured the port as EPP-capable (`EPP7_port_is_epp_capable.OUT`, 2026-09-14)
and never followed it up. EPP costs one access per byte, the same as ECP and
half of nibble, and needs no 1284 phase management at all.

⛔ Do not repeat these from the same source: "ECP requires a DMA channel" is
false here - the vendor's ECP block path is PIO with `dmaEn=0`, read from the
binary. The 2 / 2.5 MB/s mode figures are irrelevant on this bus, where the
measured 5.77 us per 8-bit access caps everything at about 173 KB/s.
