# Mach8 Windows 95 display driver - design, 2026-10-06 (#49)

Owner's brief: build the best of every technique we have found, not a copy of any one driver. Ideas and
the card's capabilities are in `docs/mach8_graphics_vision.md`; the text path and source notes in
`docs/mach8_text_and_driver_plan.md`. This file says how the choices are made and from what.

## The rule: the 5160 picks, per drawing job

The bus is the only scarce resource (~3.8 us per 16-bit port write). For every GDI job below there are
two to four ways to make the card do it. **Each candidate is timed on the real 5160 (M8TIME, below) and
the cheapest wins**, whichever driver it came from. Port writes per operation are counted as well as
time. A path is only used once 86Box's model matches the real card on it, so it can be developed in the
bed (`docs/ati_test_com_notes.md`; the 8514/A command path is ~80% measured, the Mach8 extended
`DP_CONFIG` path ~25%).

## Candidates per job

| job | candidate A | candidate B | candidate C |
|---|---|---|---|
| text (glyphs) | XFree86 glyph cache: glyph planes in spare memory, 8514/A plane-select blit, ~4 writes/glyph | ATI: mono expansion fed from the CPU (`DP_CONFIG 3251`), ~13 writes/char | DDK sample (Christian's XGA): DIB Engine lays out the string, one expansion per string, ~8 writes/char |
| solid fill | 8514/A `CMD` rect fill (`40F3`), ~5 writes | Mach8 `DEST_Y_END` fill (`2211`) | `SCAN_TO_X` per span (polygons) |
| screen-to-screen blit | 8514/A bitblt (`C0F3`) | Mach8 non-conforming blit (`6211`/`6011`) | - |
| pattern brush | 8514/A fixed pattern (8x1) | Mach8 colour pattern (`A211`) | brush cached in spare memory + blit |
| bitmap upload (`SetDIBitsToDevice`) | CPU data, 16-bit words (`rep outsw`) | per-scanline encoding: runs as fills, 2-colour spans as expansion, rest raw (vision doc) | - |
| lines | 8514/A Bresenham line | Mach8 `LINEDRAW` (point to point) | short-stroke vectors for short ones |
| cursor | software (DIB Engine) | save-under + blit from spare memory | - |
| screen modes | one mode per boot (ATI) | live same-depth switch via `ReEnable` (velocity9x / vmdisp9x), incl. 8514 <-> VGA for Andrew's DirectDraw idea | - |

## Measured: M8TIME on the 5160, 2026-10-06

`tools/m8seq/M8TIME` (capture `docs/captures/2026-10-06_m8seq/5160_M8TIME3.BIN`). Interrupts off, PIT timed,
a FIFO-room check before each operation and the engine's finish included. The raw upload works out at
7.6 us per 16-bit write, the same as the 2026-09-20 port measurement, so the timer scale holds.

| job | technique | cost | |
|---|---|---|---|
| text, 8x16 glyph | A: glyph cache, plane-select blit from spare memory (XFree86) | **51 us** | best |
| | C: one expansion per string, Mach8 `DP_CONFIG 3251` (DDK pattern) | 68 us | 1.3x |
| | B: one expansion per character, same path (ATI's `ExtTextOut`) | 121 us | 2.4x |
| fill | 64x16 rectangle, 8514/A `40F3` | 61 us | |
| blit | 64x16 card-to-card, 8514/A `C0F3` | 102 us | |
| raw bitmap | 64x16 pixels from the CPU, 16-bit words | 3,931 us = 3.84 us/pixel | |
| VGA memory | writes to the VGA side's memory, `rep stosw` | 2.07 us/byte | |

- **Text: the glyph cache wins.** First version of the text path: XFree86's layout, per-string expansion
  (C) as the fallback for glyphs not yet cached.
- **Bitmaps: never send what the card can make.** A 64x16 area costs 61 us as a fill and 3.9 ms as
  pixels, 64x. The bitmap path encodes per scanline: runs as fills, two-colour spans as expansion, raw
  only for the rest (vision doc).
- **Andrew's VGA point holds for writes:** 2.07 us/byte into the VGA's memory against 3.84 us/pixel through
  the engine. Reads were the slow direction (`docs/isa_memory_vs_io_2026_09_20.md`). The 8514 desktop has no
  memory window, so this only helps a VGA-mode path (DirectDraw, games) - a reason to keep live mode
  switching in the design.
- First attempts (`M8T1`, `M8T2`) sent the 1-bit text tests down the 8514/A path without the right
  command; the engine never finished. XFree86 never feeds 1-bit data from the CPU at all.

## What each source contributes, and on what terms

| source | takes | licence / terms |
|---|---|---|
| XFree86 3.3.6 `XF86_Mach8` (Kevin E. Martin) | glyph cache layout and draw sequence, 8514/A command usage | MIT-style: keep the notices, credit, no endorsement (`docs/mach8_text_and_driver_plan.md`) |
| velocity9x (Michael Dale) | DIB Engine glue, VDD registration, `ReEnable` mode switching, test tools (`V9XGDI`, `V9XMSW`), Open Watcom build | GPL-3.0; author's permission to use parts and to contribute, 2026-10-06. The driver is therefore GPL-3.0 |
| vmdisp9x (JHRobotics) | where velocity9x's switching came from; Win95 support claimed | MIT |
| xga-win9x (Christian Holzapfel) | how the DDK mini-driver sample builds and works, `DrawTextBitmap` | no licence; the author's blessing to use it (reported by the owner, 2026-10-07). His own code only: parts derived from the DDK sample stay out of a GPL-3.0 driver |
| ATI `MACHW3.DRV`, Microsoft `ATIM8.DRV` | the reference for what works on this card: DIB Engine with `NOT_FRAMEBUFFER`, the extended path, init sequences | proprietary: study only |
| TEST.COM, ATI ROM | register behaviour, init tables | proprietary: study only |

GPL-3.0 and the DDK sample code do not mix: the driver is built on velocity9x's framework, not on DDK
sample source. DDK headers and documentation are fine.

## Open before the framework is fixed - answered 2026-10-16

- No linear framebuffer: `NOT_FRAMEBUFFER` is the DDK's documented mode; velocity9x has no such path, so we take its
  build, INF, VDD registration and `ReEnable` and write the screen-drawing core ourselves (section below).
- Windows 95: velocity9x has run on Win95 4.00.950; its Win16 driver builds here against the Win95 DDK.

## Order of work

1. ~~M8TIME on the 5160~~ - done 10-06 (table above).
2. Match 86Box on the paths M8TIME picks - done for fill, blit, 8514/A mono and DP_CONFIG mono (M8DPM, 10-16).
3. ~~velocity9x review~~ - done 10-16 (sections below).
4. Card levers (worklist L, 10-16) - decisions in "Lever decisions" below.
5. The A10 replay harness, then D8: the driver primitive by primitive in M8TIME's order, each measured with
   `TXTBENCH` before and after on the 5160.

## 2026-10-16 additions

- **Base mode 800x600x256 (owner).** Pitch 1024, so 600 KB visible and ~424 KB off-screen (rows 600-1023) plus a
  224x600 strip (x 800-1023) for the glyph cache, brushes, icons and cursor save-under. Higher modes are table
  entries. Aims restated: least RAM, least bus traffic, both halves and unused hardware kept on a measured gain.
- **D0: the DDK FRAMEBUF sample, built with our toolchain, loads and draws in Windows 95** (28800 half, 640x480,
  needs VFLATD). That proves the toolchain only: FRAMEBUF is DDK sample source, so it is not the base of the
  GPL-3.0 driver (section above).
- **No full RAM shadow.** For a screen operation not yet accelerated, a fallback that reads back only the
  bounding rectangle, lets the DIB engine draw it in RAM and writes it back - counted per GDI function - is a
  candidate if `NOT_FRAMEBUFFER` (ATIM8's method) leaves gaps. Check ATIM8 and velocity9x first.
- **8514.DRV / ULTRA.DRV command set** (`docs/mach8_cmd4_axes_2026_10_16.md`) is a third reference for what GDI
  asks of the engine; command 4 (`80B3`) is their text-background fill - a candidate for that job.
- **Unused hardware, each kept only on a measured gain:** DEST_CMP_FN nibble tests and colour compare (colour-keyed
  blits, DirectDraw colour keys); pattern-register dithering (D4); palette splits on retrace (D4); the 28800 half
  as a VFLATD surface with frame flipping by passthrough (D5/D6, C2) and its zero-wait write bits (B2); polygon
  boundary + fill for 3D triangles (D3).

### The two open questions, answered 2026-10-16 (static read)

- **No frame buffer:** the DDK documents it (`DOCS/DESGUIDE/DISPLAY.DOC`; `INC16/DIBENG.INC`): set `NOT_FRAMEBUFFER`
  (0020h) in `deFlags` - "Example: 8514/a or ATIM8". The DIB engine then does memory-bitmap operations only; every
  operation whose destination is the screen is the driver's, through the engine. velocity9x has no such path
  (`references/velocity9x` @ `45d439e2`, no `NOT_FRAMEBUFFER` anywhere): every card it drives has an aperture. So we
  take its build, INF, VDD registration and `ReEnable` switching, and write the screen-drawing core ourselves.
  The read-back rectangle fallback covers screen operations not yet accelerated.
- **Windows 95:** velocity9x has run on a 486 with an S3 Trio64 (VLB) under Windows 95 4.00.950, after Win95-only
  INF fixes; its mini-VDD `v9xmini.vxd` did not load there at the time (`docs/changelog/0.1-to-0.6.0.md`).
  Not yet re-checked against its current state.

## velocity9x read, 2026-10-16 (`references/velocity9x` @ `45d439e2`)

Layout: `src/display16/` is the Win16 driver (`ddi.c` entry points, `enable16.c`, `modes16.c`, `gdi_accel.c` the 2D
engine paths, `runtime.asm`); each card is a small descriptor under `src/chipsets/` (`V9X_HW16_DEVICE`: id, name,
aperture hook, `fill_engine` declaring the engine type and caps). The Trio64 back end is 123 lines.

- **Its Trio64 drives an 8514/A engine by port I/O** - the same register set as the Mach8's 8514/A half. Solid fill
  (`v9x_gdi_trio_fill`: FRGD_MIX, FRGD_COLOR, PIX_CNTL, CUR_X/Y, MAJ, MIN, CMD), screen copy with direction by overlap
  (`v9x_gdi_trio_copy`) and text map nearly one to one. Template for our first three primitives.
- **Text:** the DIB engine lays the string out (`DIB_ExtTextOutExt`) and hands the driver one mono string bitmap; the
  driver expands it through PIX_TRANS with clip in the scissors - M8TIME's candidate C (68 us/char). **Data format
  differs:** the Trio takes 16 pixels per word; the Graphics Ultra in planar 16-bit mode takes one byte per group of
  four pixels (bits 4-1), and in pixel mode one byte per pixel (measured for #8289). Use the Mach8 extended mono path
  (`DP_CONFIG 3251`, as M8TIME C) or convert. Then the glyph cache (A, 51 us).
- **FIFO pacing:** Trio waits for an empty FIFO every 8 words (depth unknown there). The Mach8's is measured: 16 deep,
  and a write to a full FIFO is dropped - pace by 16.
- **Robustness to keep:** bounded engine waits; a session "poison" latch on timeout (everything then declines);
  per-path counters (`v9x_gdi_accel_stats`), fault injection; `ADVFUNC_CNTL` re-asserted before every operation
  because DOS-box activity cleared it on real silicon. The Mach8 equivalent needs checking (ATI.VXD traps).
- **What changes for us:** a decline hands the operation to the DIB engine, which on its cards draws into the
  aperture. Under `NOT_FRAMEBUFFER` it cannot - our decline goes to the read-back rectangle fallback.
- Its house rules (C89, no `enum`, everything not exported `static`, `v9x_<vendor>_<chip>_<verb>`, gates in
  `scripts/`) apply to code we contribute back.

### velocity9x build on this machine, 2026-10-16

- Tree check and host tests: **pass** (Open Watcom snapshot 2026-10-10; `lib386` extracted from the archive already on disk).
- Win16 driver (`build-active-package.ps1 -Family s3`): **compiles and links** to `v9xdisp.drv` (49,444 bytes) against
  the **Windows 95** DDK's `DIBENG.LIB`, MASM 6.11 and VxD `LINK.EXE`, laid out as the scripts expect in
  `C:\Users\lycet\tools\ddk95_as98` (`-DdkRoot`). The post-link audits then fail on output format only: our newer
  `wdump` prints segment flags inline and the version resource as "Version info" (segment 1 is `CODE | FIXED`, as the
  audit requires). One local, uncommitted widening in `scripts/common.ps1`; worth telling Michael (owner sends).
- Mini-VDD: fails to assemble - `GET_MONITOR_POWER_STATE_CAPS` / `SET_MONITOR_POWER_STATE` are Windows 98 additions to
  `MINIVDD.INC`; `-NoDpms` does not guard their registration. Needs the Windows 98 DDK, or a guard. Ours targets
  Windows 95 and would not register DPMS anyway.
- For a faithful build: the Windows 98 DDK and the Open Watcom snapshot velocity9x's audits were written against.

## Only what changed crosses the bus (owner, 10-16)

- GDI already hands the driver only invalidated areas; the read-back fallback and the cursor work on the smallest bounding
  rectangle, never the screen.
- DirectDraw / games / the demo do not get this for free: the Blt and flip paths take a list of dirty rectangles and move
  only those (`mach8_graphics_vision.md`).
- Two renderers (worklist C2): the CPU builds frame N+1 in the 28800's memory while the engine draws frame N on the Mach8's,
  and the output switch flips. The raster clock (A9h, 11.5 us per poll, L4) times a tear-free switch; split banking gives
  no speed (L4), so the VGA side uses plain single-bank writes. Gated on C1 (what the output switch costs).

## Lever decisions for D8 (L5, 2026-10-16)

From `docs/mach8_levers.md` (census, VVESA, ROM, three card sittings) and M8TIME. "Card" = measured on the 5160.

| Job | D8 uses | Evidence | Later |
|---|---|---|---|
| Mode set, 800x600 | **candidate A:** the ROM's own mode set, entry C000:64h AL 0, called through DPMI (INT 31h 0301h, real-mode far call) at Enable - EEPROM timings, no tables of ours. **Candidate B:** read the EEPROM timing record and program the CRTC/shadow set ourselves (as ATIM8 does) | ROM API static only; no driver calls it. B is what every shipping driver does | test A in the bed first; B if A fails |
| Text | DP_CONFIG 3251h host mono expansion, one per string (M8TIME C, 68 us/char) | card = bed byte for byte (M8DPM) | glyph cache, DP_CONFIG mono source 3 (M8TIME A, 51 us; card ok) |
| Text background | RECT fill `40F3` or command 4 `80B3` - measure both | 8514.DRV uses command 4; axes now card-measured (M8NIB2) | - |
| Solid fill | 8514/A RECT `40F3` (61 us for 64x16) | card (M8TIME) | - |
| Screen-to-screen blit | 8514/A BLIT `C0F3`/`C0B3`, direction by overlap (velocity9x Trio64 pattern) | card (M8TIME 102 us) | - |
| Transparent blit / colour key | source colour compare 92EEh/EAEEh | card-measured, modelled (`8283ed2bc`); no ATI driver used it | DirectDraw colour keys (D5) |
| Pattern brush | PATT_DATA colour patterns | card (M8ROW3, M8FG6-9) | brush cache off-screen |
| Lines | 8514/A Bresenham; SHORT_STROKE 9EE8h for short ones | card (M8ROW3) | - |
| Bitmap upload | per-scanline encoding: runs as fills, two-colour spans as mono expansion, raw only for the rest | raw costs 64x a fill (M8TIME) | - |
| FIFO pacing | count our writes; poll 9AEEh at most once per 16 entries | FIFO 16 deep, drops when full (card); ATIM8 has 314 poll sites | - |
| Off-screen memory | 800x600 at pitch 1024: rows 600-1023 and the x 800-1023 strip | layout above | glyph, icon, save-under caches |
| Raster position | line counter A9h, index rewritten per poll | card: 11.5 us/poll, ~2-line steps (M8RAST, M8BITS) | palette changes per band (D4); tear-free flips |
| Unaccelerated screen ops | read back the bounding rectangle, DIB engine draws it, write back; counted per GDI function | design above | counts pick the next primitive |

**Ruled out by measurement:** switching the output between the halves (>1 s blank, M8FLIP4 - so no alternate-frame or
raster-split use of the two chips); split read/write banking for speed (no gain, M8SPLIT); EXT_SHORT_STROKE (diagonals
only, M8SSV3); 28800 zero-wait bits AB (no gain, 10-10); the unknown 28800 bits (no effect, M8BITS).

**Resolution (owner's question, 10-16):** the engine accelerates 640x480, 800x600 and 1024x768 alike; per-operation
cost hardly changes. Resolution sets the off-screen cache (about 424 KB at 800x600, 256 KB at 1024x768 - less cache,
more data on the bus) and whether two pages fit for flipping (640x480 only). Desktop: 800x600 (1024x768 as an option);
flip-based games and the demo: 640x480.

**Both chips, still:** the VGA half drives its own modes (D6, the VFLATD driver, DirectDraw at 640x480/800x600); on the
Mach8 side a double-buffered flip fits at 640x480 (two 480-row pages in 1 MB at pitch 1024), timed by CRT_OFFSET and the
raster position, without switching halves.

## Priorities and the first build (owner, 2026-10-16)

**Order:** (1) a desktop driver at least as fast as ATIM8, using the card more intelligently - the whole card
where a measurement shows a gain; (2) then the experimental work: DirectDraw (no driver for this chipset ever had
it), more colours (D4), 3D (D3), video.

**Home:** our own tree, `drivers/mach8_display/src/`, GPL-3.0, taking velocity9x files one at a time with credit.
velocity9x's Win16 driver (14,750 lines) assumes an aperture; Michael can take back a fork of ours at any time.

**Why a fallback first:** ATIM8 exports its own BitBlt, Output, ExtTextOut, StrBlt, Pixel, ScanLR, StretchDIBits
and the rest (~120 KB of code). Under `NOT_FRAMEBUFFER` the DIB engine draws memory bitmaps only, so every screen
operation needs a path from the first boot.

**D8a:**
1. Enable / Disable / ReEnable from velocity9x; screen PDevice with `NOT_FRAMEBUFFER`; 800x600 by candidate A
   (ROM C000:64h through DPMI), B if A fails.
2. One fallback for every screen operation: read the bounding rectangle back, the DIB engine draws it in RAM,
   write it back. Fixed 64 KB buffer; larger operations in strips. Each strip goes to the DIB engine through a
   selector whose base is offset by the strip's top-left, so screen coordinates pass unchanged - no per-function
   argument rewriting.
3. Solid fill on the engine (RECT `40F3`): the first primitive off the fallback.
4. Counters per GDI function at the fallback; with the tap (`MACH8_WLOG=1`) they give bus cost per function -
   A10 step 1. The counts order the rest of the lever table.

Pass: one boot of `vm_5160_now` draws the desktop correctly at 800x600. Risks: the ROM mode set from protected
mode is unproven; read-back speed for large rectangles is unmeasured.

## D8a result, 2026-10-16 (bed `vm_5160_now`, tap build `e8a50f66a`)

**Passes in the bed:** Windows 95 boots to an 800x600x8 desktop on `M8DISP.DRV`; icon labels, menus, dialogs,
Explorer and Solitaire draw correctly. Not yet run on the 5160. Four bring-up bugs, each one commit:
- `LoadFail`, failure code 0: a `__segname("_TEXT")` cast linked as a fixup to a segment the file lacks;
  the build now fails on any invalid fixup (`wdump`). BOOTLOG only logs the Windows phase on an F8 "Logged"
  boot, not from `WIN` at a DOS prompt.
- Icon labels missing: the DIB Engine's negative-count measure gave a 2-row height; text is bounded by
  the font height instead.
- Radio buttons "C|", folder icons clipped: the even-aligned strip was passed on as the clip, so BitBlt drew
  a neighbouring source column; the DIB Engine now draws only the unwidened rectangle, the extra columns are
  read and written back unchanged.

**Where the bus went** (one session: boot, Explorer, Solitaire, menus; `m8disp_d8a_fix3_1016.log`):

| path | ops | pixels | on the bus |
|---|---|---|---|
| engine fill | 1,410 | 17.5 M | no (5 register writes each) |
| engine copy | 133 | 7.1 M | no (7 writes) |
| fallback read-back | 4,526 | 5.8 M | yes |
| fallback write-back | 4,420 | 6.0 M | yes |
| cursor (read + 2 writes per move) | 1,902 | 2.1 M | yes |

**Next (D8b), by that table:** attribute the fallback pixels to GDI entry points (a marker per entry);
text on the engine (DP_CONFIG mono expansion, then the glyph cache); the cursor saved and restored by
engine copy to off-screen memory instead of read-back; DOS full-screen switching (INT 2Fh 4001h/4002h).

## D8b step 1: bus time per GDI entry, 2026-10-16

`tools/m8_fallback_tally.py` over a -Trace session (boot, Explorer, Solitaire, Start menu, dialogs, window drag;
`docs/captures/2026-10-16_atim8_tap/m8disp_d8b_tally_1016.log.gz`). Bus time estimated at 3.84 us/pixel written,
5.7 us/pixel read back.

| entry | calls | read px | write px | bus s |
|---|---|---|---|---|
| BitBlt to screen (fallback; 1,204 fills and 66 copies went to the engine) | 1,839 | 2.13 M | 2.78 M | 22.8 |
| ExtTextOut | 915 | 1.83 M | 1.99 M | 18.1 |
| cursor move | 663 | 0.69 M | 1.37 M | 9.2 |
| BitBlt screen -> memory (save-under) | 50 | 0.78 M | 0.10 M | 4.8 |
| DibToDevice | 170 | 0.11 M | 0.13 M | 1.1 |
| cursor shape / Pixel | 40 / 2,073 | small | small | 0.5 / 0.3 |
| total | | | | 56.8 |

Order this sets: text on the engine (no read-back; bound by character widths first); BitBlt split by ROP and
source, and edge-column reads instead of whole-rectangle reads when alignment widens a plain copy; cursor
save-under in off-screen memory by engine copy; `RC_SAVEBITMAP` with `SaveScreenBitmap` by engine copy.

### D8b step 1 result, 2026-10-16 (`m8disp_d8b_step1_1016.log.gz`)

Longer session than the first (2,177 text calls vs 915), so compare per call. ExtTextOut 19.8 -> 17.0 ms
(width bound: 2,000 -> 1,720 pixels read per call); the cursor unchanged at 14 ms per move. Also found and fixed:
the DDK structures were not byte-packed, so FONTINFO.dfPixHeight was read 2 bytes late (`m8ddi.h`).

| cost | entry | next |
|---|---|---|
| 37.0 s | ExtTextOut (2,177 calls) | engine text: mono expansion, no read-back |
| 19.4 s | BitBlt DIB -> screen SRCCOPY (669; 4.4 M px uploaded, reads now 0.44 M edge columns) | upload encoding: runs as fills |
| 18.3 s | cursor (1,314 moves) | save-under off-screen, draw by mono expansion |
| 12.8 s | BitBlt screen -> memory (175; menus, Solitaire drags) | SaveScreenBitmap; device bitmaps later |
| 11.9 s | pattern ROPs 5A (504), F0 patterned (275 fallback), 0A (one full screen) | engine mixes + pattern registers |
| 4.4 s | Output (40 calls, large clip bounds) | tighter bound / engine lines |

### Engine text result, 2026-10-16 (`m8disp_d8b_text_1016.log.gz`)

ExtTextOut through `DIB_ExtTextOutExt` + Mach8 mono expansion (`a155f1e`): **17.0 -> 2.0 ms per call** (1,190 calls,
924 expansions, 532 opaque fills, 2 fallbacks). Its remaining read-backs are cursor redraws after text under the
pointer. Owner: text, menus and highlights correct; nothing drawn past window edges (so the extended scissors clip
the extended blit).

Next by cost: cursor 11.9 s (895 moves) - save-under off-screen by engine copy, shape by mono expansion; bitmap
uploads (DIB SRCCOPY) 11.7 s - runs as fills; screen -> memory 7.6 s; one full-screen ROP 0A 4.6 s.

### Engine cursor result, 2026-10-16 (`m8disp_d8b_cursor_1016.log.gz`)

Save-under by engine copy to off-screen x 800, masks by mono expansion (`f2d2d39`): **13.3 -> 1.0 ms per move**
(579 moves; 2 read-backs, both the software path at the top/left edge). Owner: cursor, hourglass and I-beam correct,
no trails, all four edges. Session total ~14 s estimated bus time. Left: one full-screen ROP 0A (shutdown grey-out)
4.6 s; bitmap uploads 3.1 s; screen -> memory 2.7 s; DibToDevice 1.0 s; patterned F0 0.8 s.

### Pattern ROPs and open glitches, 2026-10-16 (evening)

Done (`9f415f8` .. `d2808a4`): BitBlt with no source maps its ROP to the 8514/A mix (model MIX table, card-checked
by M8MIX; key = the ROP's results for (P,D) = 00, 01, 10, 11). Solid brush = one fill (COLORSOLID flag + Bits[0] -
neither dp8BrushBpp nor a scan of all 64 pattern bytes works for these brushes); two-colour pattern from
dp8BrushBits = one mono expansion tiled from the 8-pixel boundary, cut by the scissors; mono-bitmap brushes
(PATTERNMONO) from dp8BrushMono, **one byte per row** (logged: 55 AA 55 AA ... over all 32 bytes), colours from the
draw mode (1 = bkColor, 0 = TextColor). Shutdown grey-out is now fast and dithered (a 50% halftone, as Windows draws
it). ATIM8 does no grey-out at all - a capability difference to look up, not a glitch.
Fixed on the way: a 16-bit overflow (65,520 rows read as -16) hung Windows at startup on the caret.

Disable now does what vmdisp9x does (`1ed3d55`): START_IO_TRAP, passthrough, INT 10h mode 3, return 1. One clean
shutdown since; the earlier blue screen (0D at 0137:BFF72A08, after Disable) has not recurred - one run, not proven.

**Open, both drawn correctly by ATIM8 in the same bed (owner screenshots, `XT_project/photos/`):**
1. **White square beside the Solitaire castle deck** (the animated deck). Pixel trace over
   `m8disp_d8b_pattern3_1016.log.gz` (x 207, y 175): it first turns white at log line 2,637,207, a solid PATCOPY
   (`F0`) through the *fallback* as a 2x109 column at x 206 - the same call drew felt green (02) earlier in the
   session. Solitaire then saves and restores the background under its bats (screen -> memory, DIB SRCCOPY), carrying
   the white along. That run had every solid fill on the fallback (the bug `d2808a4` fixes), so the path that made
   the white no longer runs; **re-test first**. If it persists: compare the brush the F0 call carried (D marker) with
   the colour written.
2. **Default button's black frame drawn dashed** (Notepad "save changes?" dialog, "Yes"): something XORs the frame's
   pixels. Suspect the focus rectangle (PATINVERT `5A`, mono-bitmap halftone, now the engine pattern path) landing on
   the frame - check its rectangle against the frame's in a trace.

Tools for both: `tools/m8_fallback_tally.py` (per-entry cost, brush flags), and the one-pixel history script used
above (to add to tools/ if it is needed again).

**Retest on our driver after `d2808a4` (owner, 10-16 night; `m8disp_d8b_castle_retest_1016.log.gz`):** the castle
square is smaller but still there; Explorer's focus rectangle leaves dots; the shutdown grey-out is striped and smears
text (a clean 50% checkerboard is expected); the default-button frame is still dashed. The last three all go through
the two-colour pattern expansion, which the fallback drew correctly before. **Pattern expansion is off**
(`M8_PATTERN_EXPANSION 0`, screen.c); solid brushes with any mix stay on the engine. To find before turning it back on:
the pattern phase and row mapping (stripes suggest every row got the same byte) - check the bits sent against the
brush in a trace before changing code.

**Pattern expansion off (`3dacc5c`), owner 10-16 night (`m8disp_d8b_nopattern_1016.log.gz`):** button frame and Explorer
selection correct; grey-out better. So the pattern expansion caused those. **Castle deck still wrong:** paints correctly
at first; after a click on the deck the bat sprites show beside the card, outside it.
Grey-out with expansion off: an even checkerboard, dialog clean on top (owner screenshot) - correct.

**Castle, pixel (180,153) history in that log:** deck card at (104,137), 72 wide (right edge ~x 175). Before each bat
frame Solitaire paints the strip beside the card green (engine fill `176,137 10x101`, colour 02 - correct); then the
sprite upload `146,149 38x12` (BitBlt DIB SRCCOPY, our fallback) writes sky/moon colours (FC/FF/F8) at x 176-183.
The DIB Engine draws only inside the rectangle GDI passed, so either GDI's rectangle reaches past the card or the
source bitmap holds card-back art there. **Next:** in `atim8_baseline_castle_1016.log.gz`, find ATIM8's uploads for
the same sprite and compare their width; if ATIM8's is narrower, the difference is in how we read that call's
arguments (log wDestX/wXext/wSrcX for C2CC calls in a -Trace build).

**ATIM8 baseline, same sprite (`atim8_baseline_castle_1016.log.gz`):** ATIM8 draws the deck card as an extended blit
(DP_CONFIG 5211) clipped to x 87-157 (71 wide), and the bat sprite at x 129 (42 into the card) clipped to x 129-157 or
129-154 - **29 and 26 pixels wide, inside the card**. Ours: same 42-pixel offset, uploaded **38 and 36 wide** (alignment
adds at most 1 per side), so about 8 pixels wider - the overhang. The -Trace build now logs each DIB -> screen BitBlt's
arguments after an F0F0 marker (wDestX, wDestY, wXext, wYext, wSrcX, wSrcY): one castle run shows whether GDI passes
us a wider rectangle than ATIM8 draws, or we misread it.

**Cause found (`m8disp_d8b_castle_args_1016.log.gz`):** GDI passes the bat sprite as BitBlt dst (125,127) **36 wide**,
src x 42, from Solitaire's 71-wide card bitmap - only 29 columns exist (= ATIM8's 29). The DIB Engine clips to the
source extent and draws 29; our fallback had skipped the read (SRCCOPY "covers" its rectangle) and wrote the other 7
columns back from a stale buffer: the sky of an earlier frame, beside the card. **Fix:** BitBlt clips the rectangle
to a memory source's extent before any path (deWidth/deHeight = bmWidth/bmHeight offsets). Not yet re-run.
Lesson: "the operation overwrites its rectangle" holds only after clipping to the source.
