# DIBENG.DLL font layout (Win95 DIB Engine, ExtTextOut)

Source: `references/atim8_disasm/bin/DIBENG.DLL`, disassembled with
`python tools/nedis.py <dll> dis <seg> <off> <n>` (capstone 16-bit). Addresses are
`seg:off` in DIBENG's NE code segments. Exports: DIB_EXTTEXTOUT 4:000B, DIB_STRBLT 4:0000,
DIB_GETCHARWIDTH 3:3C84.

## 1. Pointer: segment base only, not lpFontInfo

- ExtTextOut frame (after DIBENG's 4:000B prologue, bp-relative): lpDevice `[bp+32h]`,
  x `[bp+30h]`, y `[bp+2Eh]`, lpClipRect `[bp+2Ah]`, lpString `[bp+26h]`, count `[bp+24h]`,
  **lpFont `[bp+20h]` (off) : `[bp+22h]` (sel)**, lpDrawMode `[bp+1Ch]`, lpDx `[bp+14h]`,
  lpOpaqueRect `[bp+10h]`, options `[bp+0Eh]` (4:0016-4:004F, 4:0318).
- Only the selector is used: `mov fs,[bp+22h]` (4:01D2, 4:01E9) and `mov ds,[bp+22h]`
  (4:0901, 4:0A9C). `[bp+20h]` is never read (no other reference in segment 4).
- Every font offset below is absolute in that segment, i.e. FontSeg field offsets from
  `FontSeg segment at 0` (`Windows95_ddk/DISPLAY/SAMPLES/PLANAR/FONTSEG.INC`). The
  DDK sample requires `FP_OFF(lpFont) == offset fsType (0x42)`: `STRBLT.ASM:539`.
  So FP_OFF = 66 is correct and expected; the table is at segment offset, **not**
  lpFont + 148. Table offsets relative to lpFont would be `0x94 - 0x42 = 0x52`.

## 2. Version branch (the only one)

`cmp word ptr fs:[0], 200h` selects the table: 4:06E7 (width sum), 4:0868 (width,
string-bitmap build), 4:0D02 (glyph fetch). GetCharWidth uses `cmp [0],300h; jl` (3:3CB3,
3:3CFE). Raster path never tests dfFlags/ABC (no `fs:[76h]` read as flags), and never
tests dfType bit 4 (PF_BITS_IS_ADDRESS) or reads dfBitsOffset (`fs:[71h]`).

| dfVersion | table base | entry | width | bits |
|---|---|---|---|---|
| 0x200 | 0x76 | 4 bytes | WORD at +0 | WORD at +2 (`fs:[eax*4+78h]`, 4:0D3A) |
| >= 0x300 | **0x94** | **6 bytes** | WORD at +0 (`fs:[ebx*2+94h]`, ebx = 3*idx; 4:0D0A-0D0F) | **DWORD at +2** (`[ebx*2+96h]`, 4:0D17) |

## 3. Index, first/default char

Raster byte-char path (options & 0x300 == 0; 4:0CF0, 4:06F0-0700):

```
idx = char - dfFirstChar        ; fs:[5Fh]
if (idx > dfLastChar - dfFirstChar) idx = dfDefaultChar   ; fs:[61h], used as-is (already first-biased)
entry = fs:[0x94 + idx*6]
```

Evidence: `sub bl,dl` (dl = first) / `cmp bl,dh` (dh = last-first) then
`mov bl,fs:[61h]` without a further subtract (4:06F0-0715, 4:0CF0-0D2D). Break char
(`fs:[62h]`) is compared against the biased index (4:0E98-0E9B), so it is also relative to
first. A table indexed by raw char (no bias) reads entries 32 too high for a font starting
at 32, and past the end for high chars.

Fixed pitch: if `fs:[56h]` (dfPixWidth) is nonzero, width = dfPixWidth, the table is not read
(4:068C-0693, 3:3CA0-3CA7). Character spacing: `[bp-80h]` = lpDrawMode+16h (CharExtra)
added per char; break extra `[bp-82h]` = lpDrawMode+0Eh (4:0E98-0EB2). Height = `fs:[58h]`.
`dfType & 0x20` (PF_GLYPH_INDEX) switches to 2-byte chars (`lodsw`, 4:0850-0859,
4:0D51 with the same 6-byte entry); not set for SSERIFE.

## 4. Glyph bits: column-major 8-pixel strips, as in FNT 3.0

Standard raster blitter is 4:11A0 (selected at 4:0463; 4:0F23 / 4:1084 are the
ETO_BYTE_PACKED / ETO_BIT_PACKED row-major glyph paths, options 0x100/0x200, not used by
GDI raster text).

- `esi` = the DWORD from the entry (relative to the font segment, read through `ds:[esi]`).
  Not combined with dfBitsOffset.
- 4:11A0-11BD: `esi += rows_skipped_top`; for each 8 px skipped on the left
  `esi += height` (`ebx = [bp-0BEh]` = glyph height, = `fs:[58h]`). Strip k of the
  glyph is at `bits + k*height`, byte r of the strip is row r.
- 4:11CA-11E7 (width <= 8): one byte per row, `esi++`, ror by `x & 7`, OR into the
  string bitmap (row stride `es:[3136h]`).
- 4:11EA-1226 (width > 8): `ch = (w+7)/8` strips; per row, bytes at `esi`,
  `esi+height`, `esi+2*height`...; then `esi++` for next row.
- Number of strips = `(width+7)>>3`; each strip is dfPixHeight bytes; the 3.0 FNT layout.

## 5. Callback bitmap x = x0 - 8 and extra rows above

- Bitmap left = `(rect.left & ~7) - 8`; right = `(rect.right & ~7) + 15` (4:0946-095C,
  in `[bp-0ACh]`, `[bp-0AEh]`). The -8 is a fixed margin in DIBENG, so x is x0 rounded
  down to a multiple of 8, minus 8, whenever the rect is the string extent.
- Bitmap row stride = `((right-left+1)>>3)+1+3 & ~3` (4:0960-096D); band height
  `0x2800 / stride`, minus rows already done (4:0972-098C). The buffer is cleared at
  4:09A4-09AB, so glyphs are OR-ed into zero.
- Callback (`lcall [bp+0Ah]`, 4:0CB6-0CEC) receives: lpDevice, bitmap ptr, `x = [bp-0ACh]`,
  `y = [bp-64h]`, stride, rows `[bp-0A8h]`, a rect at `[bp-66h]`, brush colours.
  `y = [bp-64h]` is the top of the rect, not the glyph y.
- That rect is string extent `(x, y, x+width, y+height)` (4:0207-022A) clipped against
  lpClipRect (4:0237-0291, 4:02E8), unioned with the clipped lpOpaqueRect when options
  has ETO_OPAQUE (union code 4:0292-02E7, called at 4:0158 / 4:0180). So extra rows
  above y0 come from the opaque rectangle / clip rect top, not from the font.
  Glyph y inside the bitmap = `y - rect.top` (4:09FD-0A04, `[bp-0B6h]` rows skipped).

## 6. Not determined

- Why the driver's own reading gave ~120 px too much: DIBENG agrees with the 148 / 6-byte
  / WORD-width reading, so the cause is in the driver (pointer base: lpFont+148 instead of
  segment+148; missing `- dfFirstChar`; or counting from an unconverted default).
  Run-time check: the first entry at segment offset 0x94 should be the width of char
  dfFirstChar.
- What GDI.EXE does to convert a 2.x font to the 3.0 copy: not read (GDI.EXE in
  `references/atim8_disasm/bin/` not searched).
- Meaning of `[bp-52h]` (4 or 6) beyond "bytes per pointer-table entry for packed glyph
  paths": only used by the 0x100/0x200 paths.
- PF_BITS_IS_ADDRESS handling: DIBENG does not read it in the text path; any handling must
  be in GDI before the call. Not verified.
- Whether `velocity9x` has FONTINFO notes: only `references/xga-win9x/MINI/STRBLT.ASM`
  mentions lpFont (parameter declaration only); the DDK planar sample STRBLT.ASM 527-650 /
  BUILDSTR.ASM 351-362 uses the same `fsCharOffset[bx][PROP_WIDTH=0 / PROP_OFFSET=2]`
  with bx = idx*6 (`STRBLT.INC:58-59`).

## 7. ETO_BYTE_PACKED path (options & 0x100)

Selection (4:0441-0466): `options & 0x100` -> blitter 4:0F23, `[bp-52h]` = 4;
`options & 0x200` -> blitter 4:1084, `[bp-52h]` = 6; neither -> 4:11A0 (section 4). The
`[bp-52h]` value is chosen by those option bits only, not by the font.

**The lpFont segment is not an FNT image in this mode.** The raster code in sections 2-4
(`fs:[0x94]`, `fs:[0x5F]`...) is bypassed. Evidence: with `options & 0x300` the fetch
routine is picked at 4:0922-0942 from `cs:[bx+8C0h]` (words 0D80h, 0DB6h, 0DEAh, 0E23h)
using `ds:[2]` and options, and the advance routine at 4:04B... (4:04E8-04F4) is
0F09h or 0EEEh. Also at 4:0070-007A the string rect comes from lpTextXForm `[bp+18h]`
(a RECT), not from `fs:[..]` width sums (the 4:01E9 branch is skipped).

Header fields read from the segment (ds = `[bp+22h]`):

| offset | use |
|---|---|
| `[2]` | bit 0x80 set: 4-byte glyph pointers, word glyph header; clear: 2-byte pointers, byte header (4:0927-092F) |
| `[6]` | DWORD, offset (same segment) of the pointer table (4:0D8C, 0DC0, 0DF6, 0E2D) |
| `[0Ah]` | DWORD, offset of a WORD-per-index advance table (4:0EF2-0EF7) |

1. **Width/advance and bits pointer.** Fetch routine = `cs:[8C0h + (([2]&0x80)==0)*4 +
   ((options&0x10)==0)*2]` (4:0927-093D):
   - 0D80 / 0DB6 (`[2]&0x80`, WORD / BYTE index): `esi = dword [[6] + idx*4]`; header at
     `[esi]` x-origin, `[esi+2]` y-origin, `[esi+4]` width cx, `[esi+6]` height dx (WORD
     each, 4:0D9A-0DA9, 0DCA-0DD9); `esi += 2*[bp-52h]` -> bits (8 bytes for 0x100).
   - 0DEA / 0E23 (`[2]&0x80` clear): `esi = word [[6] + idx*2]`; header `movsx byte [esi]`
     x-origin, `[esi+1]` y-origin, `[esi+2]` width, `[esi+3]` height (BYTEs,
     4:0E01-0E19); `esi += [bp-52h]` -> bits (4 bytes for 0x100; for 0x200 it skips 6).
   The pointer is a segment-relative offset in the lpFont selector (ds), 32-bit via 67h prefix.
   Advance (4:0EEE-0EF7): `ax = word [[0Ah] + idx*2]`, added to x. With lpDx present
   (`[bp-1Ah]&0x20`) it is instead `gs:[lpDx]` words (4:0F09-0F16, chosen at 4:04E8-04F1).
2. **Bits (0x100, 4:0F23-1083).** Row-major, top row first, `bytes_per_row = (width+7)>>3`
   (`[bp-56h]`, 4:0F34-0F40), no row padding beyond that and no header in between. Left clip
   `esi += clip_x>>3` (4:0F2E-0F31); top clip adds `bytes_per_row` per row (4:0F44-0F4F).
   Width <= 8 reads 1 byte/row (pairs of rows as consecutive bytes, 4:0F86-0FB6); <= 16 two
   bytes/row (4:0FC4-0FFB); <= 24 three (4:1006-1043); wider: per row `ceil(w/8)` bytes,
   `esi += bytes_per_row - copied` (4:1044-1083). MSB = leftmost pixel (ror by x&7).
3. **Index.** `idx = (unsigned) char` straight from lpString: BYTE (0DB6/0E23) or WORD
   (0D80/0DEA) per `options & 0x10` (ETO_GLYPH_INDEX), 4:0DB6, 0E23, 0D80, 0DEA. No
   dfFirstChar subtraction and no default-char substitution in these routines; lpString
   holds glyph indices into GDI's own table, whether or not the bit is byte-wide.
4. **Advance vs width.** Different: advance comes from the `[0Ah]` table (or lpDx), the
   ink width is the per-glyph cx from the header. Glyph placed at `x + x_origin` and
   `y - y_origin` (4:0D9A-0DA1, 0E06-0E10); no A/B/C fields. Break/CharExtra is added in
   0EEE only via `[bp-80h]`-based code in the 0E98 path (not the 0EEE/0F09 path used here).
5. **Height** is per glyph: header `dx` (4:0DA5, 0DD9, 0E14, 0E4B), not dfPixHeight
   (`[58h]` is used only by the 0CF0/0D51 raster fetch).

0x200 (4:1084-11A0): bit-packed rows, no padding: row stride in bits = width (`add esi, ebx`
with `ebx = width`, 4:1099-10AA, 10EA), `esi` is a bit address (`shl esi,3`, 4:1095), header
size 6 (`[bp-52h]`).

Not determined for this section: what in GDI builds the `[0]/[2]/[6]/[0Ah]` structure;
the layout of the 2 extra bytes in a 0x200 header; the byte-wide-index width case with `[2]&0x80`
set was read from code only, no data seen.
