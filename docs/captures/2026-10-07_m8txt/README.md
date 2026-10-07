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

## TS1 commands 133-141: the source colour compare (fixed)

`M8ROW4` (TS1 commands 133-141 read back directly) showed the card writing only some 4-pixel groups of a
one-row blit. `M8SCMP` (`tools/m8seq/M8SCMP.ASM`) measured it directly on the 5160 (`M8SCMP_5160.BIN`):

- 92EEh bits 3-5: compare function, DEST_CMP_FN's encoding (08 never, 10 src < key, 18 >=, 20 =, 28 !=,
  30 >, 38 <=, 00 off), applied to each blit source pixel. EAEEh: the key colour.
- A true result writes the background (BKGD_COLOR through the background mix), not "leave unchanged" as the
  8514/A destination compare does (Sanchez and Canton p. 175).
- An engine reset (42E8h or 32EEh) does not clear 92EEh. So M8SEQ/M8ROW4 checkpoints after 133 inherit the
  previous checkpoint's compare; `M8SEQ5` and `M8ROW5` clear both ports at each checkpoint. On the card that
  changes checkpoints 134-140 only.

Fork `8283ed2bc` models it in the BitBLT path. `M8SCMP` and `M8ROW5` now match the card exactly; `M8SEQ5`
first differs at checkpoint 143 (was 134), 30 checkpoints differ. Next: command 142, a colour-pattern blit
(`PATT_DATA` 1111..8888, DP_CONFIG E211h/EA11h).

The Windows runs above used a blank Mach8 EEPROM (1024x768 interlaced only); the beds have the FlexView image
from the regression run of 14:16 on.

## TS1 commands 142-172 (afternoon, batched)

| commands | feature | card probe | fork commit | state |
|---|---|---|---|---|
| 142-143 | foreground source 7: VRAM source byte indexes the colour pattern, as source 6 does for host data (bit 11 picks `byte >> 2`, else `((byte & 7) << 2) \| (x & 1)`) | M8ROW6 | `554cfeddc` | fixed |
| 144 | 16-bit monochrome host data, LSB_FIRST clear: bit 15 first | M8ROW6 | `de789efb6` | fixed |
| 145-148 | foreground source 4: 4:1 count of set source pixels (below), ALU 13h (add, no saturation) | M8ROW7, M8SRC4-4E | `8193af4b3` | fixed |
| lines before 149 | LINEDRAW_OPT POLY_MODE vector lines: every pixel, X clamped to the left scissor, dropped past the right (CLIP_MODE 2 "polygon boundary lines") | M8ROW7 | `4e9ef658d` | fixed |
| 149-172 | blits, scan-to-X, 8514/A rectangle and blits | M8ROW7 | - | match |

After `8193af4b3` M8SEQ5 matches the card at all 173 checkpoints (`M8SEQ5_bed_8193af4b3.BIN`), M8ROW6 at all ten,
M8ROW7 in its first two areas; the third (X >= 1024) is the scissor read-back below. Still different: M8SRC4/4B
tests with mono source "always 1", where the card waits for host data and the model does not.

Source 4 history (M8SRC4/M8SRC4B on the 5160): values are `11h x n`, n 0-4, in 16-pixel units; the colour
pattern plays no part (TS1's pattern and a ramp give identical rows); with every pixel foreground the first 16
pixels are `44h` and only the last pixel of each later group is; mono source "always 1" leaves the engine not
taking host data. Not in the Mach32 guide (sources 0,1,2,3,5 only).

`M8SRC4C`, `M8SRC4D`, `M8SRC4E` on the 5160 (BINs alongside) give the whole rule, 8 bpp:

- Destination pixel k of a row (counted from the destination start) takes source pixels 4k to 4k+3 from the
  row's source start: a 4:1 horizontal reduction. TS1's own bits (nibbles 0-F) give `00 11 11 22 11 22 22 33
  ... 44`, 11h times the population count.
- A source pixel counts when all its read-mask bits are set: `(pixel & RD_MASK) == RD_MASK`. Under mask FFh
  only FFh counts (01h, 80h, 0Fh, F0h do not); under 01h, 01h and 0Fh count.
- The result is always the foreground: a lone set pixel at position 1 still gives 11h, where the per-pixel
  monochrome source would have picked the background.
- Past the source (k >= source width / 4) the four reads are not VRAM: each is (k mod 16) x 11h. So under mask
  FFh destination pixels 31, 47 and 63 read 44h; under 01h every odd k from 17 (from 9 with a 32-wide source).
  Moving the destination by 5 changes nothing; VRAM past the source end read 00h before and after.
- With mono source "always 1" (DP_CONFIG 8219h) the engine waits for host data and draws nothing (M8SRC4).

For the driver: a hardware 4:1 coverage count. A 1-bit glyph or shape drawn 4x wide off-screen, then blitted
with source 4, gives 0-4 coverage per pixel in one pass: antialiased text or edges, or a horizontal
downscale, with no CPU work per pixel. Vertical needs four passes with ALU add (TS1 uses ALU 13h, add).

Reads beyond X 1023 return FFh on the card when the 8514 scissor is 3FFh (M8ROW7 third area); the model
returns memory. Re-read with a wider scissor before concluding anything about writes there.

Method: `REGR.BAT` runs at boot from `AUTOEXEC.BAT` (bed `vm_6695`), so bed runs need no one at the keyboard;
several fixes per bed run, attributed checkpoint by checkpoint.
