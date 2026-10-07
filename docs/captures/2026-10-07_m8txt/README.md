# Mach8 text and 86Box#6695, 2026-10-07

## Text garbled under Windows 3.1 `8514.DRV`

Bed `vm_6695` (copy of `vm_mach8_w311/default`, `SYSTEM.INI` switched to Microsoft's `8514.DRV`;
ATI's copy kept as `WINDOWS\SYSTEM.ATI`). Text was garbled on `86box_3c509b` at `ee29d00c8`, clean on
upstream (`86box_master`, 2026-09-25).

- The driver loads its font into off-screen memory (y 832 on) with `CMD 43B3` and `PIX_CNTL A080`
  (monochrome host data), then expands glyphs with `C0B2` blits and `PIX_CNTL A0C0`.
- Bisect (`bisect_log.txt`, owner judged each round): first bad commit `a0bad65cc`, the high-byte-first
  swap for 16-bit host data. Measured on colour data; it also caught monochrome data.
- `M8MONO` (`tools/m8seq/M8MONO.ASM`) on the 5160: `43B3` takes the high byte first and upstream already
  does that; the swap reversed it. `53B3` matches everywhere. Files: `M8MONO_5160.BIN`, `_master`, `_ours`
  (before the fix), `_fixed`.
- Fix: fork `02c3b70cb`, no swap for monochrome host data. `M8MONO` matches the 5160 and the text is clean.
  Regression: the other probes are byte-identical before and after; TEST.COM unchanged.

Also from `M8MONO` on the 5160: over the 8-bit bus each byte of planar monochrome data gives 4 pixels,
from bits 4 down to 1 (rule 3 below).

## Monochrome host data on the 8-bit Graphics Ultra (M8LINE, fixed)

Measured on the 5160 (`M8LINE_5160.BIN`; `tools/m8seq/M8LINE.ASM`), PIX_CNTL A080h:

1. Only bits 1-4 of each byte are used. The pixel at X takes bit 4 - (X mod 4) - screen X, not the
   count from the start.
2. Pixel mode (CMD bit 1 clear, lines `3319`, rectangles `5319`): one byte per pixel.
3. Planar mode (bit 1 set, `43B3`): one byte per aligned group of four pixels; a start inside a group
   uses the remaining bits; a row ending inside a group drops the rest of the byte; rows keep their
   start X and width.
4. 16-bit transfers (bit 9): low byte first when LSB_FIRST (bit 12) is set, high first when clear.
   8-bit transfers (bit 9 clear, `3119`): the low byte of each word only.
5. A vectored rectangle with direction 0 (`5319`) draws rows in +X and steps up one line.

Both 86Box builds took bits 7 and 6 in pixel mode, did not step rows in (5), and widened unaligned
planar rows. Fixed in the fork: `96859258f`, `ecea8636e`, `0e9796012`. M8LINE, M8TXT and M8MONO now match
the 5160; the other 13 probes are byte-identical before and after; M8SEQ still matches to checkpoint
134; Windows 3.1 text clean.

Still different: when a command is short of data the card stays busy waiting, the models go idle.

The decoder first read the 53B0h read-back high byte first; it is low byte first (LSB_FIRST set). The
comparisons between machines were unaffected.

## 86Box#6695 (Windows/386 2.11, 8514/A fill)

Bed `vm_6695_w2` (opti495 AMI, 486DX/33, 16 MB, Mach8 16-bit, Windows/386 2.11 with its own
`8514.DRV` 37,328 bytes, Paint). Not reproduced on either build: the fill stays inside the outline
(`w2_master_fill.png`, `w2_ours_fill.png`). With DOS 6.22 `HIMEM.SYS` and `SMARTDRV` loaded, Windows/386
froze solid (mouse too) starting Paint; without them it works. Not yet shown to involve the display.
Windows 3.11 Paintbrush fill also works on both builds.
