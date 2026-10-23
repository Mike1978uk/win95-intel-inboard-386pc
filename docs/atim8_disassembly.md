# ATIM8.DRV: how it draws

Source: `references/atim8_disasm/segNN.asm` (objdump i8086 sweep of `vxd-patches/ati/ATIM8.DRV`). Static reading only; the one runtime cross-check is the 2026-10-16 register tap (`docs/captures/2026-10-16_atim8_tap/`).
Addresses are `segNN:offset`. Port names follow `drivers/mach8_display/src/m8.h`.

## 0. Structure (needed to read the rest)

- Exports in `seg01` are thin dispatchers. Anything the driver does not handle does `ljmp DIBENG.DIB_xxx`.
- The listing only annotates the head of each relocation chain. Far calls inside the driver show a chain link, not the target. Targets here were resolved by walking the chains with `tools/ne_disasm.py`'s `NE` class.
- Depth selects a code group at `seg02:21e2-21f9` (`[3c]` = bits per pixel):

| bpp | init call | blit engine | helpers |
|---|---|---|---|
| 4 | `seg02:2265` | seg15 | seg11, seg06 |
| 8 | `seg02:2233` | **seg16** | seg12, seg07 |
| 16 | `seg02:220f` | seg16 (init `3661`) | seg13, seg09 |

  seg08 (Output) and seg14 (StretchBlt) are common to all depths. **Everything below about blits is the seg16 (8bpp) copy.** Seg15 is the same design with 4bpp pixel unpacking.
- SYSTEM.INI `[display]` keys are strings at `seg02:0bc7-0d17`: `BltEngine TxtEngine OutEngine PslEngine PlgEngine BslEngine PixEngine ScanLREngine UCEngine EngineOnly Dib2DevEngine BlockWrite MemMapRegs IgnoreRegistry ...`. They are copied to flags `[4d..57]` at `seg02:2164`. A zero flag sends that operation to DIBENG. The seg01 gates test them: `[4f]` OutEngine, `[53]` PixEngine, `[57]` Dib2DevEngine.
- Only one write to the 8514 `CMD` port (`9AE8`) exists in the whole driver (`seg02:327c`, init). **All drawing goes through the Mach8 extended registers**: set `DP_CONFIG` (CEEE), the rectangle (86E8/82E8/A6EE/AAEE), then write `DEST_Y_END` (AEEE) to start.
- Table-driven ROP engine: BITBLT picks a handler in `seg01:14c8-159f` by device type. The handler indexes `cs:0x6c` (no source and screen to screen), `cs:0x26c` (memory to screen) or `cs:0x46c` (mono memory to screen) by the ROP byte. Each entry is a list of 3-byte micro-ops (`flags|op, handler word`) ending in a stub that does `add sp,4` (`seg16:2a9e` / `2c3d` / `2ddc`). SRCCOPY in `0x26c` is a single op, `47:330d`.
- BITBLT dispatch (`seg01:14d4-159f`): destination is screen ("PR" signature, flag 0x8000) and no source needed -> `[1370]` = `seg16:2812`. Source on screen -> `[1374]` = `seg16:28b4`. Source in memory with `bmBitsPixel==1` -> `[137c]` = `seg16:2c68`. Other memory source -> `[1378]` = `seg16:2ac9`. Destination in memory, source on screen -> `[1380]` = `seg16:2e07`. Everything else (memory to memory, flag busy, `BltEngine=0`) -> `DIB_BITBLT` (`seg01:159f`).

## 1. BitBlt memory -> screen (8bpp source)

**Method.** One extended blit for the whole rectangle. The CPU feeds it with a `rep outsw` per scanline from a run-time generated stub. Path: handler `seg16:2ac9` -> micro-op `330d` -> `seg16:3909`.

Evidence and ports:
- `seg16:3909-3935` waits for the engine to be idle: `9AEE == 0`, `62EE` bit 13 clear (GE_ACTIVE), `9AE8` bit 9 clear. Three reads per loop.
- `3937-3973` sets up the blit, 9 writes: `CEEE = 0x5211` (host data through PIX_TRANS, 16-bit, write), `BAEE` = mix, `DAEE/E2EE` scissors, `86E8`, `A6EE`, `AAEE`, `82E8`, then `AEEE` (start).
- `3974-3983`: `dx = E2E8`, `di` = row count, `lgs esi = source row 0`, then `lcall seg17:0`.
- Seg17 is a 0xC40-byte code segment whose first 0xC00 bytes are zero in the file. Its alias (`[136c]:[136e]`, `seg17:0c00` calls ALLOCSELECTOR/PRESTOCHANGOSELECTOR) is a 3 KB scratch buffer. **The driver writes a small program there on every blit** and calls it. Generator `seg16:45ca` -> `4645`. It copies a template with `rep movsl/movsw/movsb` (`46c3-46d4`, `4741-4751`), patches in the constants (word count, row stride, row count) and appends `retf` (`45d1`).
- Template used when `[5a]==0` (`seg16:4704-4731`):

```
mov ecx,<words per row>
rep outsw gs:[esi],dx        ; dx = E2E8
add esi,<row stride adjust>
dec bx / jne                  ; plus a split for rows that cross a 64 KB segment
```

  The stub is `rep outsw` of N words per row. N = row bytes / 2 after the source x is aligned down to a dword (`seg16:31e8`: `cx = srcX & 3`, scissors clip the extra pixels). There is **no per-word status check**; ISA I/O wait states pace the FIFO.
- Template used when `[5a]!=0` (`4674-46b7`): poll `9AEE`, `not ax`, then one `outsw` per free FIFO slot by shifting the mask (`add ax,ax / jne`), up to 16 words per poll.
- `[5a]` is the `BlockWrite` setting. `seg02:2020-202e` forces `[5a]=1` and then clears it unless `[3cd]` is 0 or 3. `[3cd]` comes from `12EE` bit 4 (`seg02:2dba-2dc8`; branch gated by `[1bf]==1`, which I did not trace). Our card's `12EE` low byte is 0x21 (bit 4 clear), so `[3cd]=1` and `[5a]=0` is the likely outcome here, but I did not verify the `[1bf]` branch. The tap (`atim8_tap_1016.log.gz`) shows E2E8 write bursts with no 9AEE read inside them, which agrees.
- Epilogue (`3988-399d`): poll `9AEE & 0xC000`, then `DAEE=0`, `E2EE=0x5FF`.
- The `[13d0]!=0` variant (`45d5` -> `48ca`) adds `xlat` colour translation (`4924`) per word, only when a palette translate is active. The 4bpp variants (seg15) expand a nibble with `shr 4` and send **one** `out` per source byte.

**Cost.** Fixed: about 3 reads + 9 writes before, 1 read + 2 writes after = ~15 port operations, plus building the stub (host RAM only). Payload: **1 word `outsw` per 2 pixels, no per-word reads**. A 71x96 bitmap is about 38 words x 96 rows = ~3600 outsw.

## 2. BitBlt screen -> memory

**Method.** Read the rectangle into a `GlobalAlloc` temp buffer with `rep insw`, then let DIBENG finish. Entry `seg16:2e07`.

Evidence:
- `seg16:2f90` allocates the temp buffer (`GlobalAlloc`/`GlobalLock`, size = row bytes x rows, halving the rows on failure; handle `[13a6]`).
- `seg16:3ba0` is the read. It waits for idle (`3bc3-3bdc`, same 3 reads), then `CEEE = 0x5210` (read), `BAEE = 7`, `86E8`, `A6EE`, `AAEE`, `82E8`, `AEEE` (start). `ecx` = bytes x rows, halved when `[3c]==8`.
- Unpolled path (`3c27-3c33`): poll `9AE8` bit 8 (DATA_READY) once, then `dx=E2E8; rep insw es:[edi]` for the **whole chunk**. Polled path (`3c13-3c25`, `[5a]!=0`): poll DATA_READY before every `insw`.
- `seg16:33a6` builds a fake source header in the scratch buffer (signature `0x5250`) and calls `DIBENG.DIB_BITBLT` (`346a`) from temp to the destination bitmap. DIBENG does the ROP and format conversion in host RAM.

**Cost.** ~3 reads + 7 writes + 1 DATA_READY read per chunk, then **1 `insw` per 2 pixels**. Then a host-side memory-to-memory blit. The temp buffer is one chunk, not one row.

## 3. ExtTextOut / StrBlt

**Method.** Glyph by glyph, with 1 bpp host data expanded by the engine (`DP_CONFIG = 0x3251`). Entry `seg01:15af` (STRBLT `15a4` only pushes zeros and falls into it). **No glyph cache in card memory.**

Evidence:
- Gates (`15b1-15e8`): fall to `DIB_EXTTEXTOUT` (`seg01:1b88`) for negative count, flag 0x1000, a device that is not the screen, or `TxtEngine==0`.
- Opaque rectangle (`1687-16df`): `CEEE=0x2211`, `A6E8` = background colour, one fill per clip rect (`AEEE`).
- Text set-up, once per string (`1743-1766`): `CEEE=0x3251`, `C2EE=1`, `B6EE=3`, `BAEE=7`, `A6E8` = foreground colour.
- Per glyph (fast path `17ba-186c`, general path `19d9-1ac9`):
  - look up the glyph record in the realised font in host RAM (width table `ds:[0a]`, record-pointer table `ds:[06]`);
  - clip it;
  - wait for FIFO space (`9AEE & 0xF800`);
  - 5 register writes: `86E8`, `A6EE`, `AAEE`, `82E8`, `AEEE` (start);
  - glyph bits as dwords, each sent as `out ax,dx` / `shr eax,16` / `out ax,dx` to E2E8, `(size+31)>>5` dwords per glyph, where `size = [rec+4]`.
- FIFO accounting is by free-slot budget in `ecx` (`shl ecx,5` / `shl ecx,2` with carry to refill, `1803-1865`, `18b8-18fd`). `9AEE` is read only when the budget runs out.
- Scissors are written once before and reset after (`1870-1892`). The `0x1900` branch is the slower variant (read `9AEE` before each dword pair) for fonts whose flag bit `0x40` is clear.

**Cost.** About 5 writes + 2 outs per dword of glyph + roughly one status read per glyph or per 16 FIFO entries. For a 7x13 glyph (3 dwords) that is about 12 port operations per character, ~250 for a 20-character string, plus ~15 once.

## 4. Pattern brushes

**Method.** Engine pattern registers, no DIB fallback. Brush dispatch is `seg16:3317-3355` (brush style `es:[2] & 3` selects `[1396..139c]`).

Ports: `PATT_DATA 8EEE`, `PATT_DATA_INDEX 82EE`, `PATT_INDEX D6EE`, `PATT_LENGTH D2EE` (names from `docs/mach8_card_coverage.md`).

- Solid (`seg16:3c37`): `CEEE=0x2211`, `A6E8` = colour, rectangle, `AEEE`. 1 status read, ~9 writes.
- Hatch (`3ccf`: switch on `es:[8]` through `cs:3cec`, cases `3cfa 3dd6 3e45 3eb4 3f23`): `CEEE=0x2231`, `A6E8/A2E8` = foreground/background, `D2EE` = pattern length (0x4107 or 0x411f), `82EE=0x10`, `8EEE` = pattern word(s), `D6EE` = phase from `(x+y)&7`, then one rectangle blit. About 15 writes for any size. The mono-pattern cases that need per-row phase fall to the per-scanline routine below.
- Two-colour 8x8 mono (`4183`, `4152`, `3fe9`): `CEEE=0x2231`, unrolled over 8 rotated rows (`41ed-4247`). **Per scanline**: `82EE=0x10`, `8EEE` = row byte, `AEEE` = row (a 1-scanline blit), with one `9AEE` read per 4 rows. About 3 writes per scanline. This is the brush that makes tall hatched fills cost I/O.
- Colour 8x8 (`43ba`): `CEEE=0xA211`, then for the first up to 8 scanlines: wait for 7 free slots, `82EE=0`, 4 writes to `8EEE` (one 8-byte pattern row), `D6EE`, `AEEE`. The rest of the height is one VRAM -> VRAM copy (`CEEE=0x6211`, `4470-44b3`) that replicates those 8 rows. About 8x8 + 20 operations for any height.
- Which init variant: `seg16:34eb` (when `[58]!=0`) points `[1398]` at `4183`. `seg16:35a6` (`[58]==0`) points it at `3ccf`. `[58]` is set at `seg02:2033` after a probe I did not identify.

## 5. Lines

**Method.** Engine, in `seg08` (`OS_POLYLINE`, style 0x12, entry `seg08:012d`). Pen set-up at `seg08:02a0-0429`, per-segment handlers installed in `[13ca/13cc/13ce]`.
- Common: `9AEE & 0xF800` wait, scissors `DAEE/DEEE/E2EE/E6EE`, `A2EE=0x070C`, then first point as two `FEEE` writes (`seg08:01f9-020a`).
- Solid (`CEEE=0x2211`, `A6E8` = pen colour, `seg08:0372-03a4`): horizontal or vertical segments with `SCAN_TO_X` (`CAEE`) after `86E8` (`seg08:043f`, 2-3 writes). General segments with `LINEDRAW` (`FEEE`) end-point pairs (`seg08:052c`, 2 writes plus one status read). No short-stroke port.
- Dashed (`seg08:03a5-0428`): `CEEE=0x2231`, `D2EE=0x1f` (32-bit linear pattern), `82EE=0x10`, two `8EEE` words from a mask table at `cs:042b`, `D6EE` = phase (kept across segments in `[bp-0xdc]`). Segments use the same `CAEE` / `FEEE` handlers (`seg08:0477`, `04e3`, `0541`) with a `D6EE` phase write first. Opaque bkmode: `B6EE=2`, `A2E8` = background.
- Cost: about 3 port writes per segment plus 1 status read, dashed or not.

## 6. Off-screen card memory

- **Not found**: no glyph cache, bitmap cache or brush cache in VRAM; no allocator. Mono-from-VRAM appears once: DP_CONFIG 2071h for the cursor mask (`seg01:1101`, found by the port census, `docs/atim8_port_census.md`), never for glyphs. No code reserves VRAM above the visible area.
- Memory bitmaps are DIBENG objects in host RAM: `SELECTBITMAP`, `BITMAPBITS`, `CREATEDIBITMAP`, `DEVICEBITMAP` forward to DIBENG (section 8). Fonts are the DIBENG realisation in host RAM (`REALIZEOBJECT` -> `DIB_REALIZEOBJECTEXT`). Brushes are read from the realised brush in host RAM each time.
- VRAM -> VRAM (`CEEE=0x6211`) is used for screen-to-screen BitBlt (`seg16:375d-3893`), colour-brush replication (`4470`), and the cursor code (`seg01:0e00-1300`, which reads/writes a 32x32, 0x400-byte region with `rep outsw`). I did not trace whether the cursor parks images off-screen.
- Host scratch: the 3 KB seg17 buffer (stub code, DIB row staging at `+0x100`) and a `GlobalAlloc` temp for screen->memory reads.

## 7. Output (polygons, ellipses)

`seg01:1dca` handles styles 4, 5, 0x12, 0x50, 0x51 itself (jump targets `[0fba] [0fb6] [0fb2] [0fbe] [0fc2]`, filled by `seg08:0590-05b8`) and sends everything else to `DIB_OUTPUT` (`seg01:1e13`). That is rectangles (6), ellipses (7), arcs, pie, chord, round rects, polygons (19, 20), markers: **DIB engine, not the card**. `OutEngine==0` or a busy device also goes to DIBENG. Style 0x12 is the polyline above; 4 is scanlines; 0x50/0x51 are BEGINNSCAN/ENDNSCAN by number; 5 I did not identify.

## 8. Entries simply forwarded to DIBENG

Always (`ljmp`): `GETCHARWIDTH` (`seg01:0166`), `REALIZEOBJECT` (`016c`, adds an extra argument), `SELECTBITMAP` (`0181`), `BITMAPBITS` (`0186`), `CREATEDIBITMAP` (`1f40`), `COLORINFO` (`1f46`), `DIBBLT` (`1ea8`, adds a byte argument), `ENUMOBJ`, `INQUIRE`, `SETPALETTETRANSLATE` / `GETPALETTETRANSLATE` / `UPDATECOLORS` (extended variants after own code), `CONTROL`, `ENABLE` (after own code), `SCANLR` (`seg03`).
Conditional (own code only when the engine flag and device test pass): `BITBLT` (`159f`), `EXTTEXTOUT` (`1b83/1b88`), `OUTPUT` (`1e13`), `PIXEL` (`1e56`, `PixEngine`), `STRETCHBLT` (`seg01:1370`; own path `seg14` via `[0fd6]`), `STRETCHDIBITS` (`142c`, via `[0fda]`), `DIBTODEVICE` (`1ebd`, `Dib2DevEngine`, own path `seg12:0435`). Cursor entries call `DIB_SETCURSOREXT` / `MOVECURSOREXT` / `CHECKCURSOREXT` and `DIB_BEGINACCESS` / `ENDACCESS`.

DIBTODEVICE (`seg12:0435`): dispatched by DIB bit count through `cs:0400` (handlers `086c 08df 094d 0a41 09b4 0ae8`). Each converts a row batch into the seg17 scratch (`+0x100`) with `rep movs` / nibble LUT, then pushes with one `rep outsw` after a single `9AEE` check (`seg12:0cdc-0d13`, polled variant `0cf4-0d0a` when `[5a]!=0`). I read the 4bpp and 1bpp converters and the shared push tail; I did not trace the 8bpp-source handler individually.

## 9. Ports and commands

| Item | Used? | Where |
|---|---|---|
| `92EE` / `EAEE` source colour compare | **No** | no `mov dx,0x92ee` / `0xeaee` in any segment |
| `FEEE` LINEDRAW | **Yes** | `seg08:0205`, `0539`, `055a` (first point + segment end points) |
| `9EE8` short stroke | **No** | none |
| `C6EE` EXT_SHORT_STROKE | No | none |
| CMD 5 (polygon fill) / any `9AE8` command | **No** | only `9AE8` write is `seg02:327c` (init) |
| `CAEE` SCAN_TO_X | Yes | horizontal lines, `seg08:043f` |
| `D2EE / D6EE / 82EE / 8EEE` pattern regs | Yes | brushes and dashes |
| `B2EE / BEEE / C2EE` extended blit source | Yes | screen->screen and VRAM replicate, `seg01:1003`, `seg16:4470` |
| `62EE` EXT_GE_STATUS | Yes | idle test before host-data blits |

## 10. CPU-side technique

- String instructions for bulk data: `rep outsw` (memory->screen, DIB, glyph-free paths), `rep insw` (screen->memory), `rep movsl/movsw/movsb` (stub templates, row copies). Template copies use `ror ecx,2 / rep movsl / rol ecx,1 / rep movsw / rol ecx,1 / rep movsb`.
- 32-bit operand and address prefixes (`66h`, `67h`) throughout: `gs:[esi]` source pointer with a 32-bit offset, `mul edx` for row offsets, `add esi,imm32` stride patched into the stub. Segments larger than 64 KB are handled by row chunking (`cmpl $0,[bp-0x44]` branches, `seg16:4645`).
- Run-time code generation in a 3 KB scratch (seg17 alias) for the memory->screen inner loop; templates have `0x1234` / `0x12345678` placeholders.
- ROP interpreter: 3-byte micro-op lists reached through `cs:0x6c/0x26c/0x46c`, called with `call *bx`, ended by a stub that does `add sp,4`.
- FIFO accounting by `9AEE` mask (`not ax`, `shl`/`add ax,ax` through the free slots) in glyph and polled paths. Otherwise one wait before the command and one after.
- Rows are walked by pointer plus patched stride (blits) or by `(x,y)` register writes per glyph or per scanline (text, brushes).

## Summary table

| Operation | ATIM8 method | Evidence |
|---|---|---|
| BitBlt mem -> screen (8bpp) | one extended blit, generated stub, `rep outsw` per scanline, no per-word check (`[5a]==0`) | `seg16:3909`, `4704-4731`, `0x5211` |
| BitBlt screen -> mem | read into GlobalAlloc temp with one `rep insw`, then `DIB_BITBLT` | `seg16:2e07`, `3ba0`, `33a6` |
| ExtTextOut | per glyph: 5 reg writes + 1 bpp dwords as 2x `out`, `DP_CONFIG=0x3251`, FIFO budget | `seg01:15af`, `17ba-186c` |
| Solid brush | one engine fill | `seg16:3c37` |
| Hatch / 8x8 mono brush | `PATT_*` regs, per-scanline blit for the general case | `seg16:3ccf`, `4183`, `3fe9` |
| 8x8 colour brush | `PATT_DATA` for 8 rows, VRAM->VRAM replicate | `seg16:43ba` |
| Solid line | `CAEE` (axis-aligned), `FEEE` (general) | `seg08:043f`, `052c` |
| Dashed line | 32-bit `PATT_DATA` + same handlers | `seg08:03a5`, `0477`, `0541` |
| Polygon / ellipse / rect / arc | `DIB_OUTPUT` | `seg01:1e13` |
| StretchBlt / DIB blits | seg14 / seg12 own path, else DIBENG | `seg01:1370`, `seg12:0435` |
| Off-screen allocator | not found; cursor only uses VRAM->VRAM | `seg01:0e00-1300` |
| `92EE/EAEE`, `9EE8`, CMD 5 | not used | whole-driver grep |

## Regenerating the listings

The listings are vendor code and stay out of git: `references/atim8_disasm/` (gitignored), with the source binaries
in `references/atim8_disasm/bin/` (ATIM8.DRV 128,800 bytes from the bed's `C:\WINDOWS\SYSTEM`, plus DIBENG.DLL, GDI.EXE,
KRNL386.EXE, USER.EXE for import names; MD5s in `bin/MD5.TXT`). To rebuild:

    python tools/ne_disasm.py references/atim8_disasm/bin/ATIM8.DRV references/atim8_disasm \n        --dll references/atim8_disasm/bin/DIBENG.DLL --dll references/atim8_disasm/bin/GDI.EXE \n        --dll references/atim8_disasm/bin/KRNL386.EXE --dll references/atim8_disasm/bin/USER.EXE

If `bin/` is lost, the same files come out of any bed image with `python tools/fatls.py <img> --get WINDOWS/SYSTEM/<file> <out>`.

## ExtTextOut's string expansion (read 2026-10-23)

`seg01:1b8d` on: no text or glyph cache - every string is DIBENG's rendered bitmap, expanded once.
Per call: one FIFO poll (`1bf1`, 9AEE & F000), the four extended scissors from the clip (`1bfd-1c17`),
a full idle wait (`1c2b`: EXT_FIFO 0, EXT_GE_STATUS bit 13, GE_BUSY), then FRGD_COLOR, ALU_FG_FN 7,
BKGD_COLOR (opaque only), ALU_BG_FN, DP_CONFIG 3251h, C2EEh=1, CUR_X = DEST_X_START, DEST_X_END
(x + width_bytes * 8), CUR_Y, DEST_Y_END (`1c46-1c9d`), and the bits through PIX_TRANS by `outsw`
(`1cbd`; with `[5a]` set, a free-slot poll between words). About 12 writes and one idle wait a string -
the same engine work as our uncached path.

