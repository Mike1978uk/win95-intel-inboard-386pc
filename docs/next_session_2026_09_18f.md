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
