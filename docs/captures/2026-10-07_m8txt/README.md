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
  M8SEQ/TEST.COM regression run still owed.

Also from `M8MONO` on the 5160: over the 8-bit bus each byte of monochrome data gives 4 pixels, from
bits 1-4 in the order 3,4,1,2. Both 86Box builds already match.

## Gap found, not fixed

`M8TXT` (`CMD 3319` is a line, `5319` a rectangle, monochrome host data): both 86Box builds agree with
each other and differ from the 5160 - which bit selects each pixel, and the rectangle's row step.
Files: `M8TXT_5160.BIN`, `_master`, `_ours`. Also the 5160 stays busy after 8 words of `43B3`/`5319`
(it wants more data); both models go idle.

## 86Box#6695 (Windows/386 2.11, 8514/A fill)

Bed `vm_6695_w2` (opti495 AMI, 486DX/33, 16 MB, Mach8 16-bit, Windows/386 2.11 with its own
`8514.DRV` 37,328 bytes, Paint). Not reproduced on either build: the fill stays inside the outline
(`w2_master_fill.png`, `w2_ours_fill.png`). With DOS 6.22 `HIMEM.SYS` and `SMARTDRV` loaded, Windows/386
froze solid (mouse too) starting Paint; without them it works. Not yet shown to involve the display.
Windows 3.11 Paintbrush fill also works on both builds.
