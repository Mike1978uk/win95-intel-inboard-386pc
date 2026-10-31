# ATIM8 against ours: the tests still below ATI

Static reading, 2026-10-31. Nothing here was built or run. ATIM8 listings: `references/atim8_disasm/segNN.asm`
(notes: `docs/atim8_disassembly.md`). Ours: master `6b02fde` (`drivers/mach8_display/src`); line numbers are HEAD's.
Card numbers: `docs/captures/2026-10-31_card/m8_card_bench.txt` - ATIM8 = `card_ati31` (10-23, one run),
ours = `card_sp1`/`card_sp2` (10-31 16:50), profile = `card_ps1` (16:22).

Cost model: a port access ~7.6 us and flushes the L1; then each 16-byte line touched ~1.3 us.
Derived from the card, not the listings: a word streamed by `rep outsw` to PIX_TRANS costs ~4.1-4.3 us
(ATIM8's bmp_changing, 16.2 ms for 3,456 words; our s52 upload, 14.8 ms for the same).
Profile caveat: every `m8_pf_part` slot (s45-s63) includes one timer stamp, ~83 us (`m8_pf_now` reads the
PIT through VTD); subtract it. `e1` (ExtTextOut) includes the stamps of the parts nested in it.

## Where we stand (ms/op, ATIM8 / ours sp1, sp2)

| test | ATIM8 | ours | ratio | note |
|---|---|---|---|---|
| text_varied | 1.990 | 2.157, 2.214 | 0.91 | gap ~200 us/op |
| dib_8bpp | 29.82 | 36.93, 37.36 | 0.80 | gap ~7.3 ms/op |
| mem_to_mem | 2.559 | 2.801, 3.357 | 0.91, 0.76 | swings run to run |
| bmp_changing | 16.20 | 18.28, 17.92 | 0.90 | gap ~1.9 ms/op |
| cursor_move | 0.982 | 1.082, 1.025 | 0.93 | |
| fill_small | 1.567 | 1.671, 1.644 | 0.95 | inside the +-8% noise band |
| setpixel | 0.510 | 0.505, 0.496 | 1.02 | at ATI in these runs |
| dstinvert | 0.602 | 0.575, 0.568 | 1.05 | ahead in these runs |
| ellipse | 14.59 | 14.16, 14.10 | 1.03 | ahead after the span-walk merge |

## Correction to a cited address

`seg01:15c4-15d9` is ExtTextOut's device gate (no 8000h, or 10h, -> `DIB_EXTTEXTOUT` at `1b88`), not BitBlt.
BitBlt's memory-destination routing is `seg01:14d4-14e3` (destination not "PR"/8000h -> `1568`) and
`1568-159f`: only a source that is the screen goes to its own code (`[1380]` = seg16:2e07); every other
memory destination is `ljmp DIB_BITBLT` (`159f`). The conclusion stands - mem_to_mem is a DIBENG RAM copy
every call - only the address was wrong (`docs/next_session_2026_10_24.md` item 4 repeats it).

## 1. dib_8bpp - SetDIBitsToDevice 100x100, 8bpp, DIB_RGB_COLORS (M8BENCH.C:373)

Entry: DIBTODEVICE. GDI passes a translate table (`lpTranslate`) for the RGB colour table.

ATIM8 (`seg12:0435`, Dib2DevEngine): bit count picks `cs:0400[2]` = `seg12:094d` (8bpp).
- Table: `0bd9-0bff` copies the low byte of 256 `lpTranslate` words (`lodsw/stosb`) into seg17 scratch. No DIBENG call.
- Set-up `07cc`: idle wait (9AEE, 62EE bit 13, 9AE8 bit 9), FIFO poll, then 12 writes: CEEE 5211, BAEE, B6EE,
  C2EE, scissors DAEE/E2EE, 86E8, A6EE, AAEE, 82E8, AEEE. One extended blit, y direction from C2EE (bottom-up DIB).
- Per row `0dec`: 4 pixels a step - dword load, four `fs:[eax+ebx]` lookups, `ror`, dword store - into seg17+100h,
  then one `rep outsw` (unpolled when `[5a]==0`). End: FIFO poll, C2EE=1, scissors reset (`04be-04d3`).
- Per row ~0x5c bytes of code, 16 table lines, ~7 source and ~7 scratch lines refetched: ~47 us + 50 words.

Ours: `DibToDevice` (screen.c:4145) -> `m8_d2d_fast` (4104) -> `m8_d2d_table` (4069) -> `m8_hw_write_xlat` (m8hw.c:1137).
- Table: every call copies the header + 1 KB colour table and runs `DIB_DibToDevice` on a 256x1 DIB into RAM. Cost
  not timed; a table cache was tried 10-23 and showed no gain (`2026-10-23_card/README.md`, f7r11).
- Set-up: wait idle (2 reads a loop), FIFO empty, `m8_plain`, scissors, FRGD_MIX, 5 writes, 8514 HOST_WRITE: ~15-20 port operations, as ATIM8.
- Per row: a C loop a pixel at a time (wdis of `m8hw.obj`): about 20 instructions a pixel with **two segment-register
  loads** (`mov es,dx`; `mov es,[bp+16h]`, ~9 clocks each in protected mode) and the `c >= 0 && c < cols` test on
  every pixel. ~36 clocks a pixel = ~43 us a row before the line refetches, ~90 us a row in all.
- Profile: e5 ~29.5-32.6 ms a call, 1.1 calls an op.

Difference: per-row CPU, ~40 us x 100 rows = ~4 ms; the rest of the 7.3 ms gap is not attributed (the table build
is the candidate; time it).

Proposal, in order of size:
1. **Better than ATI - DIB content cache.** The bitmap cache already serves bmp_card/bmp_icon by content. A DIB
   seen twice (hash of the bits plus the translate table) is drawn by one VRAM->screen copy. Hash cost ~10 KB =
   625 lines + ~5,000 hash steps, ~1.1 ms; a hit then costs ~1.5 ms against ATIM8's ~25 ms (estimate ~10x the
   call; ~3x the op). A changing DIB pays ~1.1 ms extra (~4%) - keep the seen-twice rule. Confidence: medium
   (the cache exists; DIB keys and the translate table are new).
2. **Translate the whole rectangle first, then one `rep outsw`.** Rows in assembly, 4 pixels a step like
   `0dec`, clipping done once per row, not per pixel; all rows into one buffer before any port access, so the
   table and code load once instead of 100 times; the blit is `x`/`w` even, so the buffer streams as one run.
   Saves ~(21 lines x 1.3 + ~38 us CPU) x 100 rows = ~6.5 ms a call -> ~1.05x ATIM8. Bands of ~4 KB keep it in
   the L1 for larger DIBs. Confidence: medium.
3. Build the table from `lpTranslate` as ATIM8 does (`0bd9`) when it is non-NULL - only after checking the
   result equals DIBENG's table on the bed. Saving unknown until the build is timed.

## 2. bmp_changing - SetPixel into the bitmap, then BitBlt 71x96 SRCCOPY to the screen (M8BENCH.C:359)

Entries: PIXEL (memory DC), then BITBLT (memory -> screen).

ATIM8: PIXEL on a memory bitmap goes to DIBENG (`seg01:1e56`). BitBlt -> `[1378]` = `seg16:2ac9` -> micro-op
`330d` -> `seg16:3909`: idle wait, 9 writes, generated stub of 36-word `rep outsw` per row (x aligned down to a
dword, scissors trim), epilogue poll + 2 writes. ~15 port operations + 3,456 words. Implied driver time ~14.2 ms,
i.e. at the bus floor.

Ours: `Pixel` (screen.c:3199): short-path tests fail, `m8_px_touch` drops the bitmap's proxy (unfilled, no
read-back), `DIB_Pixel` - e3 ~168 us incl. stamp. `BitBlt` -> `m8_bitblt` (2540): `m8_px_restore` (s51, ~105 us incl.
stamp, a miss), then `m8_px_upload` (2114: proxy table, `m8_bc_slot`/`m8_bc_alloc`, `m8_hw_write_rect` to an
off-screen slot - idle wait, ~10 writes, 96 `rep outsw` of 36 words, idle wait) and `m8_px_restore` again (a
VRAM->screen copy, ~10 writes). s52 = 14.8 ms. Excess over ATIM8 ~0.9 ms of driver time: the second engine op,
~15 more port operations, the slot allocation, the per-row loop.

Proposal:
1. **Better than ATI - Pixel writes through to a held bitmap.** When `Pixel` hits a bitmap with a live proxy,
   do `DIB_Pixel` on the RAM *and* a 1x1 fill in the slot (as `m8_px_fill` does, but keeping `unfilled` set, since
   the RAM stays current) instead of dropping the proxy. The BitBlt then restores from the slot (~0.1 ms). Saves
   ~14 ms/op -> roughly 6-7x ATIM8. Confidence: medium-high on the mechanism; **owner's call** - the test's
   comment says it measures "what a cache cannot help", and this changes that.
2. Conservative: for a bitmap re-uploaded after a RAM write (it lost its proxy last call), stream it straight
   to the screen as ATIM8 does (no slot, no copy), and when `deDeltaScan` equals the blit width (72 here) send
   all rows in one `rep outsw`. Saves ~0.3-0.9 ms/op -> ~0.95-1.0. Confidence: medium.

## 3. mem_to_mem - BitBlt 40x96 SRCCOPY, memory -> memory (M8BENCH.C:421)

ATIM8: `DIB_BITBLT` (`seg01:159f`, see the correction above). A RAM copy; no port access.

Ours: `m8_bitblt` (2540) -> `m8_px_m2m` (2003): `m8_px_find` (source - none), `m8_px_pieces` on the destination
(bmp_card2, held on the card), `m8_px_touch_rect`, `m8_bc_lookup` - a hash of the 40x96 source, ~3,840 bytes =
240 lines (~0.3 ms) + ~1,900 steps (~0.1 ms) - then an engine copy (~10 writes). c12 = 0.2-0.7 copies an op, so
30-80% of calls fail after the hash and also pay `DIB_BitBlt`. Why they fail: **unknown** (the trace build's
`M8_PX_NO_` reasons would say). e0 1.4-2.4 ms a call across runs.

Proposal: when the source has no proxy, decide before hashing. If the destination proxy is unfilled (RAM
current), drop it and use `DIB_BitBlt`, as ATIM8 does. If it is filled, count consecutive RAM-source copies into
it and after a few (4?) write it back once (~14 ms, amortised over the run) and drop it. Copies whose source is
a proxy (drag_frame) are untouched. Saves ~0.25-0.65 ms/op -> ~1.0-1.1x. Confidence: low-medium until the
failure reason is known.

## 4. text_varied - TextOut "Item n of m", ANSI_VAR_FONT, opaque (M8BENCH.C:278)

Entry: EXTTEXTOUT, a new string every call (the text cache never hits).

ATIM8 `seg01:15af` -> `DIB_EXTTEXTOUTEXT` with its callbacks (`1b63-1b83`): bitmap `1b8d`, rectangle `1d10`.
`1b8d`: FIFO poll, 4 scissor writes, idle wait (3 reads), FRGD_COLOR, BAEE, [BKGD_COLOR], B6EE, CEEE 3251,
C2EE, 86E8, A6EE, AAEE, 82E8, AEEE, FIFO-empty poll, **one `rep outsw`** of the whole bitmap, FIFO poll, 4 scissor
resets. ~25 port operations + payload. No key, no cache.

Ours: `ExtTextOut` (3889): `m8_px_touch`, `m8_tk_key` (3827; s56 ~100 us after the stamp) - font header 34 B,
DRAWMODE 12 B, string, widths, two rects through `M8_HASH_WORD/BYTES` calls - then `DIB_ExtTextOutExt` with
`m8_text_bitmap` (3679; s58 lookup ~10 us, skipped on first sight) and `m8_text_rect`. `m8_hw_mono`
(m8hw.c:1207, s59 ~620 us): idle wait, FIFO empty, cached scissors/masks, PIX_CNTL, FRGD_MIX, FRGD_COLOR, BKGD_MIX,
BKGD_COLOR, DP_CONFIG, 5 coordinates, one `rep outsw`, **then a full idle wait**. ~20-22 port operations - level
with ATIM8. DIBENG layout + opaque fill ~440 us, the same work as ATIM8's.

Difference: the key (~100 us) and the bookkeeping around it (`m8_px_touch`, `m8_tk_*`, `m8_exclude`) account for
most of the ~200 us gap. The trailing idle wait waits for the engine to drain the last FIFO entries; ATIM8 only
polls for room (`1cdb`).

Proposal: (a) `m8_tk_key` whole in assembly, one pass with the state in registers, the field reads inline
(~10 lines touched instead of ~25 plus the call frames): ~-60-80 us. (b) End `m8_mono` with ATIM8's room poll
and mark the colour shadows stale; take the idle wait at the next colour or host-data write: ~-15-30 us.
Together ~-100 us/op -> ~0.95-1.0. Confidence: medium on (a), low on (b). Beyond ATIM8: none found for a
first-sight string - 1 bpp host data is already the fewest bits; a glyph cache costs ~5-7 writes a glyph,
more than this string's ~90 words.

## 5. cursor_move - SetCursorPos, 20 positions (M8BENCH.C:549)

Entry: MOVECURSOR (not timed by the profile build: calls/op 0.01).

ATIM8 `seg01:0414`: unless DIBENG owns the cursor (`[1232]==0x101` / `[121d]` bits -> `DIB_MOVECURSOREXT`),
`0533` -> `[120a]` = `125b`: restore `[120e]` = `11c7` (2 polls + 19 writes: VRAM->VRAM, CEEE 6211) then draw
`[120c]` = `0f6e` (~57 port operations: save, AND pass, XOR pass from the masks in VRAM, DP_CONFIG 2071).
~78 port operations a move.

Ours `MoveCursor` (4643): `m8_cursor_hide` (414, copy save->screen) + `m8_cursor_show` (525: copy screen->save, then
`m8_hw_copy_keyed` m8hw.c:1488, 17 writes). ~34 writes + ~3 reads. The keyed copy clears the data-path cache,
so the next hide rewrites DP_CONFIG, PIX_CNTL and FRGD_MIX.

Difference: by port count ours should lead by ~40 operations (~0.3 ms), yet trails by 40-100 us a move.
**Unknown why.** Proposal: time MoveCursor in the profile build (a slot of its own) before changing it. If it
is our side: one flat register list for hide+save+keyed copy written from one loop, and the keyed copy restoring
the 8514 data path so the next hide skips 3 writes (~-30-45 us). Confidence: low.

## 6. fill_small - CreateSolidBrush(PALETTEINDEX), PatBlt 16x16 PATCOPY, DeleteObject (M8BENCH.C:288)

ATIM8: BITBLT `14c8` -> `[1370]` = `seg16:2812` -> solid brush `seg16:3c37`: 1 status read, ~9 writes.
REALIZEOBJECT `seg01:016c` is a 5-instruction `ljmp DIB_REALIZEOBJECTEXT`.

Ours: `BitBlt` (2528) -> `m8_blt_short` (2468) -> `m8_hw_fill_fast` (m8hw.c:443): 5-6 writes, a FIFO read only when
the count runs out - fewer port operations than ATIM8. `RealizeObject` (thunks.asm:61) far-calls `M8PXREALIZE`
(screen.c:1663, C, `__loadds`) before forwarding; GDI realizes and deletes the brush every op, so twice.

Difference: inside the noise band; the one static extra found is the RealizeObject detour (~10-15 us a call).
Proposal: test `m8_px_n == 0` in the thunk and jump straight to DIBENG: ~-20-30 us/op (~1.5%). Confidence:
medium on direction, low on size.

## 7. dstinvert - PatBlt 150x100 DSTINVERT (M8BENCH.C:320)

ATIM8: `[1370]` = `seg16:2812`, ROP 55h micro-op list from `cs:0x6c` (not traced further). Ours: the short path,
one fill with mix NOT D, 5 writes. Ahead (1.05) in sp1/sp2; no proposal.

## 8. setpixel - SetPixel on the screen (M8BENCH.C:497)

ATIM8 PIXEL `seg01:1e32` -> `seg07:0048`: 1 read + 7 writes. Ours `Pixel` (3199) short path ->
`m8_hw_fill_fast`: 5-6 writes. Level (1.02) in sp1/sp2; no proposal beyond the asm short paths already planned.

## 9. ellipse - Ellipse with GRAY/WHITE brush (M8BENCH.C:507)

GDI scan-converts for both drivers (same caps). Fill: Output style 5 - ours ~3.8 calls/op at ~1.1 ms (s5),
ATIM8 `seg08:0b96` (scissors once, then per span FIFO poll + 5 writes). Outline: style 0x12 - ours LINEDRAW,
2 writes a point (s18 ~3.5 ms, ~0.9 a op); ATIM8 `seg08:012d`, ~3 writes + a read a segment. Ahead (1.03) since
the span-walk merge. The polygon set-up cut (handoff 10-31: ~440 us a call; timers s45-s48) applies here too:
~3.8 calls/op x the saving. Confidence: depends on that measurement.

## Ranking (expected saving per op)

| # | test | proposal | saving | confidence |
|---|---|---|---|---|
| 1 | dib_8bpp | DIB content cache, seen twice | ~25 ms (to ~3x ATI) | medium |
| 2 | bmp_changing | Pixel writes through to a held bitmap | ~14 ms (to ~6x ATI); owner's call | medium-high |
| 3 | dib_8bpp | whole-rectangle asm translate + one stream | ~6.5 ms (to ~1.05x) | medium |
| 4 | bmp_changing | direct stream, one `rep outsw` | 0.3-0.9 ms | medium |
| 5 | mem_to_mem | decide before hashing; demote a RAM-fed proxy | 0.25-0.65 ms | low-medium |
| 6 | text_varied | key in asm; no trailing idle wait | ~0.1 ms | medium / low |
| 7 | cursor_move | time MoveCursor first | unknown | - |
| 8 | fill_small | RealizeObject thunk short-cut | 20-30 us | low |

Not established: the cost of `m8_d2d_table`; why mem_to_mem's card path fails 30-80% of the time; why
cursor_move trails with fewer port operations; ATIM8's DSTINVERT micro-op list.
