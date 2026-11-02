# 3D on the Graphics Ultra - research and ranked plan (2026-11-02)

Desk work only: nothing here is measured. Every rate below is an estimate from measured bus costs; the
first experiment (section 3) is what confirms or kills them. Earlier thinking this builds on:
`docs/mach8_graphics_vision.md` (3D section), `docs/mach8_poly_fill_design_2026_11_01.md`, worklist D3.

## 0. The numbers everything below uses

| Cost | Value | Source |
|---|---|---|
| Mach8 register write (word OUT) | 7.64 us | `iowidth` 2026-09-20 (sync 3.75 + 1.94/byte) |
| Byte OUT (palette DAC, 28800 index) | 5.70 us | same |
| Pixels through PIX_TRANS, 8 bpp | 3.84 us/pixel | `mach8_driver_design.md:43-50` |
| VGA-half memory write | 2.18 / 1.96 / 1.86 us per byte (byte/word/dword) | `mach8_card_coverage.md:246` (ZWS_5160, 10-10) |
| Engine fill / blit on card memory | ~31 / ~16 Mpixel/s | `txtbench_2026_10_04.md:10-11` |
| Engine FIFO | 16 entries, **drops** writes when full | `mach8_levers.md` (M8FIFO, 10-10) |
| Raster position (28800 A9h) | 11.5 us per poll, ~2-line steps | M8RAST, `mach8_levers.md` |
| CPU | 486BL3 ~83.5 MHz, L1 only | CHKCPU |

Two facts shape every design:
- **The CPU stalls for the whole ISA cycle**, so CPU time and bus time add. Only the engine runs in
  parallel with either.
- Budget per frame: 33.3 ms at 30 fps = **~4,360 register writes**; at 60 fps ~2,180. Every write saved
  is 7.64 us; ~640 CPU cycles cost the same.

## 1. What the silicon offers for 3D

| Feature | Port(s) | Writes per use | Card / bed status |
|---|---|---|---|
| Polyline (LINEDRAW) | LINEDRAW_INDEX 9AEE, LINEDRAW FEEE | **2 per connected vertex** (15.3 us); a disjoint line 5 (38 us) | plain LINEDRAW card = bed (TS2); throughput unmeasured; modelled (`vid_ati_mach8.c` cmd_type 3) |
| 8514 Bresenham line | CUR_X/Y, ERR_TERM, AXSTP, DIASTP, MAJ_AXIS, CMD | 7 (53 us) | card-proven; modelled |
| Short strokes | SHORT_STROKE 9EE8 | 1 write = 2 vectors, 8 directions, length 0-15 (3.8 us/vector) | card-proven M8ROW3; modelled (`vid_8514a.c:607`). EXT_SHORT_STROKE C6EE: 45-degree only on the card (M8SSV3), model follows the guide - not a lever |
| One-row span | CUR_Y, CUR_X, SCAN_TO_X CAEE | 1 per span + CUR_X/CUR_Y when they change; consecutive SCAN_TO_X writes continue from the last X | card-proven (M8CONF, M8PATT s, M8PF fill flag); modelled (cmd_type 5) |
| Colour change | FRGD_COLOR A6E8 | 1 | - |
| Rectangle fill | CUR_X/Y, MAJ, MIN, CMD | 4-5 (31-38 us), any size | card-proven |
| Polygon fill, CMD 5 outline + poly blit | see poly-fill design | ~44 (LINEDRAW outline) to ~70 (CMD 5) per polygon, any size, plus engine time for mask clear and blit | outline rule card-proven (M8PL2); poly blit card-proven (M8PF); LAST_PEL_OFF and engine time unmeasured; modelled |
| Destination colour compare | DEST_CMP_FN EEEE, COLOR_CMP | 1-2 per polygon | >=, <, ... on CMD fills = guide in 24 cases (M8CMPC); modelled since fork `c12ab587c` |
| Source colour compare | 92EE / EAEE | 1-2 | card-proven, used by our DirectDraw sprites (869/s) |
| BOUNDS | 72EE-7EEE (read) | 4 reads (~30 us) | bounding box of everything drawn through LINEDRAW: an erase rectangle for free |
| Write / read masks | WRT_MASK AAE8, RD_MASK AEE8 | 1 | bit-plane layers |
| Off-screen memory | rows 480-1023 at 640x480 (pitch 1024) | - | one page + ~544 KB, or two pages + ~64 KB |
| Page flip | CRT_OFFSET 2AEE/2EEE | 2 | **not card-probed**; ADVFUNC_CNTL resets it (`mach8_directdraw_plan.md:31`) |
| Palette | 3C8/3C9 | 4 byte OUTs per entry (~23 us) | lighting and fades by index |
| VGA half | A0000, 28800 | 1.86-2.18 us/byte; Mode X map mask writes 4 pixels per byte | output switch = >1 s monitor resync (M8FLIP4): pick one half per mode, never alternate |

**A depth test from the compare unit (new, unmeasured).** Encode depth in the colour index: index =
band x 16 + shade (16 depth bands, 16 shades; or 8 x 32). Clear to the far band. Before each polygon set
COLOR_CMP = band x 16 and a compare code that keeps the destination where `dest < COLOR_CMP`. A nearer
polygon already on screen survives; the same band overwrites (painter's within a band). One extra write
per polygon buys front-to-back or unsorted drawing with occlusion, at the cost of 16-32 visible colours (the
palette maps every band to the same ramp). Needs: the compare honoured by SCAN_TO_X and LINEDRAW (measured
only on CMD fills and the ATI blit path).

**What it cannot do:** no texture mapping, no per-pixel Z, no scaling blit, no CPU-visible 8514 frame
buffer. Anything per-pixel crosses the bus at 3.84 us/pixel.

## 2. Ranked approaches

CPU estimates: a fixed-point transform and projection ~300 cycles (~3.6 us) per vertex; span-buffer hidden
surface removal ~200 cycles (~2.4 us) per span. Resolutions: "320x200" on the 8514 side means a 320x200
viewport in the 640x480 mode (the 8514 side has no 320x200 mode); on the VGA half it is mode 13h / Mode X.

| # | Approach | 320x200 | 640x480 | Main risk |
|---|---|---|---|---|
| 1 | **Vectors / wireframe** (LINEDRAW polylines, short strokes for HUD and text, page flip, engine clear) | refresh-bound (60) for objects up to ~700 edges; ~1,400 edges at 30 fps | same edge budget; clear costs 10 ms of engine time (overlaps CPU) | LINEDRAW vertex measured on the card, 20.6-22.1 us (~1,500 segments per 30 fps frame, section 5); the flip is not card-measured |
| 2 | **Flat-shaded polygons, span buffer** (CPU removes hidden surfaces, the engine draws each visible span once: CUR_Y + CUR_X per row, colour + SCAN_TO_X per span) | ~10 spans/row: 200 x 22 writes = 34 ms bus + ~5 ms CPU: **~25 fps** | ~12 spans/row: 480 x 26 = 95 ms + CPU: **~8-10 fps** | span cost measured on the card: 20.4-21.9 us/span (arm B, predicted 15.3), ~10 us per register write; estimates in this row halve (section 5); CPU HSR time adds to bus time |
| 2a | same, painter's with spans (no HSR, overdraw) | 100 triangles x 20 rows x 3 writes = 47 ms: **~15-20 fps** | 100 x 48 rows: 111 ms: **~8 fps** | overdraw grows with scene |
| 2b | same, CMD 5 polygon fill (size-independent) | 100 polygons x ~50 writes = 38 ms bus; engine ~0.5 ms each also ~40 ms: **~20 fps** | same writes; engine time grows with area: **~10-15 fps** | **measured on the card:** fill works (triangle photographed), 0.2 ms + bbox area x ~35 ns (3.0 ms at 200x200); beats spans above ~10 rows; ~100 mid-sized polygons per 30 fps frame at 320x200 is an estimate; bed fills rectangles; edge rules (`mach8_poly_fill_design`) |
| 2c | add frame-to-frame span delta (draw on the visible page, no clear; send only rows whose span list changed) | static camera, moving objects: refresh-bound | same | tearing (no flip); worthless when the camera moves |
| 3 | **Raycaster, flat walls** (one 1-px-wide rect fill per column + ceiling and floor fills) | 320 x 5 writes = 12 ms: **~60 (refresh)** | 640 columns: 24 ms: **~35 fps** | none known; merge columns of one wall face into spans to cut writes |
| 3a | Raycaster, textured: pre-scaled texture columns cached off-screen, one blit (~10 writes) per screen column | 24 ms + misses: **~30-35 fps** | 49 ms: **~15-18 fps** | memory: 64x64 texture at 32 height steps ~210 KB; a cache miss uploads h pixels at 3.84 us each (100 px = 384 us = 5 blits); at 640x480 only one page fits with a cache |
| 4 | **VGA half, Mode X flat polygons** (map mask writes 4 pixels per byte, CRTC page flip) | full cover 16,000 bytes x 1.86-2.18 = 30-35 ms: **~25-30 fps**; software texturing 64,000 bytes: **~8 fps** | mode 12h, 16 colours planar, set/reset writes 8 px/byte: ~72 ms: **~12 fps** flat; 640x480x256 packed: <2 fps | the 1 s resync at mode change; 28800 unchained mode untested on the card |
| 5 | **Software Direct3D at 16 bpp** (Microsoft's RGB emulation drawing into our fb16 RAM frame buffer; fingerprints send what changed) | window 320x200 = 64,000 px x 3.84 us = 246 ms: **~4 fps** | **<1 fps** | none to try: it may already work (dxdiag test 3); a dead end for speed - every pixel crosses the bus |
| 6 | **Direct3D HAL, flat-shaded, 8 bpp** (rows 2/2a as the back end) | as 2a: **~15-20 fps** for 100 triangles in a small window | as 2a | few applications accept a device with no textures and no Z; whether DDRAW/D3D 5-7 accept an 8-bit HAL is unknown |
| 7 | **Pseudo-3D road** (OutRun, Lotus, Hard Drivin' on 16-bit consoles): road, kerb and grass as engine spans, ~3 per row; pre-scaled roadside sprites as keyed copies | 3 x 200 = ~600 spans x arm B us/span (15.3 predicted) = ~9 ms bus: **refresh-bound (60)**, before sprites | ~1,440 spans = ~22 ms: **~35-40 fps**, before sprites | none beyond span cost (arm B) |
| 8 | **Scaled sprites without scaling hardware** (Lynx "Suzy" scaled in hardware, Sega Super Scaler; weaker ports used pre-scaled copies): N pre-scaled sizes per sprite in spare card memory, each draw one colour-keyed engine copy | 869 keyed 64x64 copies/s measured (DirectDraw 8 bpp, 7bca399): **~29 per frame at 30 fps, ~14 at 60** | same copies; cost does not depend on resolution | card memory per sprite set (sizes x pixels) |
| 9 | **Isometric / dirty-region worlds** (Head over Heels, Marble Madness, Knight Lore): tiles and sprites in card memory, painter's order, redraw only the regions that moved; keyed copies, bus carries commands only | cost = moved regions x copies each; static scene ~0 | same | scenes where most of the screen moves every frame fall back to a full redraw |
| 10 | **Bus-free full-screen effects:** (a) hardware scroll and parallax by the display start address, layers as keyed copies (Turrican, Shadow of the Beast; Amiga copper); (b) page flip between two 640x480 pages in 1 MB; (c) palette cycling / ramp rotation for water, fire, lighting (Amiga/ST art, Mark Ferrari; Doom-era colormaps) | a handful of register writes per frame: pan ~2 writes (15 us); 64 palette entries x ~23 us = ~1.5 ms | same | whether the display start latches at vertical sync (tearing); palette writes without snow. Measured on the card: pan 15.05 us; 64 palette entries 837-869 us/frame (~4.3 us per DAC byte); vsync latching not answered (arm F unreliable) |

Rows 7-10 are estimates from the same bus numbers, taken from 8/16-bit era techniques (Amiga, NES / Mega
Drive, Atari Lynx; Head over Heels, Marble Madness, Turrican, S.T.U.N. Runner, Blue Lightning, Hard Drivin',
Warbirds). They need no new silicon; they use the engine, keyed copies and display-start / palette registers
already listed in section 1.

Note on 2b: the Amiga blitter's area-fill mode (CPU draws one-pixel-per-row edges, the blitter fills between
them) is the same idea as the 8514 polygon fill (outline + poly blit), which ATI's drivers never used.

For comparison, a software renderer sending a full 8 bpp frame: 8514 side 64,000 x 3.84 = 246 ms (4 fps);
VGA half 64,000 x 1.86 = 119 ms (8 fps). For existing games (Doom95 and the like) the VGA half is the
faster target - which is the owner's DirectDraw Mode X item.

**Ranking reasons.** 1 is cheapest, uses card-proven features, and is the one case where the bus is not
the limit; it is also the Vectrex model the owner raised. 2 is the core of every filled-polygon path,
including a HAL; span buffer before painter's because each visible pixel is drawn once and the CPU (cheap
here) does the sorting. 3 is a special case of 2 with fixed geometry. 4 matters for existing software,
not for new silicon tricks. 5 should be run once as the baseline (no code) and then left. 6 comes after
2 works in DOS: a HAL is a front end, not a renderer.

### Direct3D HAL: what it would take and claim

Structure (Microsoft DDK; velocity9x builds the same on Win98): the DirectDraw HAL we already have
(`M8DD32.DLL`) adds `D3DHAL_GLOBALDRIVERDATA` and `D3DHAL_CALLBACKS`. ContextCreate and RenderPrimitive
are mandatory; Execute may be null with `D3DDEVCAPS_EXECUTESYSTEMMEMORY` set, and the runtime breaks execute
buffers into RenderState/RenderPrimitive calls (velocity9x `d3d_soft.c:538-545`, after the DDK ViRGE
sample). DX5 adds DrawOnePrimitive/DrawPrimitives (callbacks2); DX6-7 use DrawPrimitives2 through
GetDriverInfo. The DX5 path is enough for DX5-7 applications.

Claims for a flat-only HAL: colour model RGB (the runtime lights vertices; we map the RGB of the first
vertex to a palette index once per triangle); shade caps `COLORFLATRGB` only; no texture formats
(velocity9x created and drew through such a HAL on a Trio64: `TexFormatCount=0`,
`D3DCreateDeviceHr=0`, 2026-08-30 decision); render depth `DDBD_8` (a legal value per Microsoft's
`D3DDEVICEDESC_V1` page; no shipping 8-bit HAL found); Z depth 0, or Z claimed and done by deferring the
scene to EndScene and resolving it with the span buffer (row 2) - a software Z at span granularity. The
required obsolete bits (FLOATTLVERTEX, DRAWPRIMTLVERTEX, TLVERTEXSYSTEMMEMORY, EXECUTESYSTEMMEMORY) must
be set. At 16 bpp a HAL draws into fb16 RAM - software, no gain over Microsoft's RGB emulation unless ours
ties into the fingerprint send.

### What the 86Box bed models, and does not

Present (`86box_upstream/src/video/vid_ati_mach8.c`, `vid_8514a.c`): LINEDRAW polyline (cmd_type 3),
Bresenham lines, short strokes (9EE8), SCAN_TO_X with the fill flag (cmd_type 5), CMD 5 boundary lines,
the poly blit (DP_CONFIG bit 1), both colour compares. Enough to check pixels for every approach above.

Missing, so the bed cannot give frame rates: I/O is priced ~1 us per OUT against 7.64 on the card
(`bed_misprices_io_vs_memory`); the engine finishes each command instantly (busy is a flag cleared on the
next status read), so engine-bound cases do not exist in the bed. C6EE follows the guide, not the card.
SCAN_TO_X starts a mono pattern at bit 7 (dithered spans differ). Every rate must come from the 5160.

## 3. First experiment (under an hour): M8SPAN

A real-mode `.COM` beside `tools/m8seq/M8PF.ASM` (same set-up, PIT-latched timing, interrupts off, FIFO
polled once per 16 writes). Arms A-C and D-G, 2,000 operations each (arm G per frame), on the 640x480 8514 mode:

| Arm | Operation | Predicted (bus only) |
|---|---|---|
| A | one span per row: CUR_Y, CUR_X, SCAN_TO_X, widths 8 / 32 / 128 / 320 | 22.9 us; width 320 ~10 us engine, hidden behind the writes |
| B | ten spans in one row: FRGD_COLOR + SCAN_TO_X each | 15.3 us per span |
| C | LINEDRAW polyline, 20-px segments | 15.3 us per vertex |
| D | CMD 5 triangle (outline + poly blit) at 20x20 and 200x200 | ~0.35-0.55 ms; the 200x200 case shows engine time |
| E | display-start pan (CRT_OFFSET) | writes per pan; visible tearing |
| F | page flip between two pages, with and without waiting for vertical blank | writes per flip (~2); tearing without the wait |
| G | palette rotation, 64 entries per frame | ~1.5 ms per frame (64 x ~23 us); snow or not |

Reads the screen back for a pixel check (bed = card) as M8PF does.

**The number that decides: arm B, microseconds per span.** <= 20 us: the span buffer holds ~25 fps at
320x200 and approach 2 is the main track. 20-40 us: halve the estimates; 320x200 at ~12-15 fps, still
ahead of the VGA half for flat shading only if HSR saves overdraw. > 40 us (engine latency, not the bus):
drop the span path; flat shading goes to Mode X (approach 4) and the engine keeps vectors (approach 1).
Arm C below ~20 us per vertex confirms ~1,400 edges per 30 fps frame for approach 1. Arm D's 200x200 time
against its 20x20 time gives the engine's fill cost per pixel for CMD 5, which ranks 2b (arm D). Arms E-G confirm approach 10's costs.

Cost: one probe from the M8PF template, one bed run for pixels, one card run over COMrade (two, per the
idle rule). No driver change.

## 4. Open questions only the card answers

1. Engine latency per span, per LINEDRAW vertex, per 1-px-wide fill and per small blit - is the engine
   ever slower than the bus for short work? (M8SPAN arms A-C)
2. Does the 16-entry FIFO stay safe with a long fill (a 640x480 clear, ~10 ms) queued ahead - i.e. how
   long a poll waits, and whether the CPU can transform while the engine clears.
3. CRT_OFFSET flip: latched at vertical sync or immediate (tearing)? Cost of waiting on A9h.
4. DEST_CMP_FN on SCAN_TO_X spans and LINEDRAW (measured only on CMD fills and the ATI blit) - gates the
   depth-band trick.
5. CMD 5 with LAST_PEL_OFF, POLY_MODE through LINEDRAW, and the poly blit's engine time.
6. 28800 unchained (Mode X) write rate with the map mask at 0Fh: still 1.86-2.18 us per byte for 4 pixels?
7. Palette writes per frame without visible snow (lighting by ramp rotation).

Questions for the bed, not the card: whether DDRAW/D3D 5-7 accept an 8-bit HAL with no textures and no Z;
whether Microsoft's RGB emulation runs on our fb16 primary (dxdiag test 3 at 16 bpp).

## 5. M8SPAN on the card, 2026-11-02

Real 5160, Mach8 at 800x600 (desktop 16 bpp, driver M8RESYNC), `tools/m8probe/M8SPAN.C`, three runs.

| Arm | Operation | Measured | Predicted |
|---|---|---|---|
| A | one span per row (CUR_Y, CUR_X, SCAN_TO_X), widths 8 / 32 / 128 / 320 | 25.1-28.1 us, independent of width | 22.9 us |
| B | spans along a row (FRGD_COLOR + SCAN_TO_X) | 20.4-21.9 us/span | 15.3 us |
| C | LINEDRAW vertex | 20.6-22.1 us | 15.3 us |
| D | polygon fill, 20x20 / 200x200 | 196-213 us / 3.01-3.05 ms | 0.35-0.55 ms |
| G | 64 palette entries | 837-869 us/frame (~4.3 us per DAC byte) | ~1.5 ms |
| E | pan (CRT_OFFSET_LO/HI) | 15.05 us | ~15 us |

- **A:** width-independent, so the engine finishes before the next command; the bus is the cost.
- **B:** just over 20 us. By section 3's rule that is the "halve the estimates" band, low end. Effective
  cost is ~10 us per register write including loop and FIFO checks (predicted 7.64).
- **C:** ~1,500 segments per 30 fps frame.
- **D:** outline by LINEDRAW `POLY_MODE | LAST_PEL_OFF` into rows from y 700, then the extended blit
  (DP_CONFIG 2213). Engine ~28 Mpixel/s for clear + fill. The owner's photo of the card's monitor
  (`C:\Users\lycet\OneDrive\Desktop\XT_project\photos\directx.jpg`) shows a correct triangle: the polygon
  fill works on the card. Cost model ~0.2 ms + bbox area x ~35 ns; it beats spans above ~10 rows (40-row
  triangle ~0.26 ms vs ~0.84 ms by spans). Ranks approach 2b first for flat shading: ~100 mid-sized
  polygons per 30 fps frame at 320x200 (estimate).
- **Bed:** the 86Box bed draws the same polygon fill as solid rectangles; it does not model `POLY_MODE`
  through LINEDRAW (an emulator gap). Bed timings are not card timings (~1 us per port write).
- **F (vertical sync count):** unreliable (8, 4, 23, 48 per second across versions; bed also 23). The
  measurement loop is wrong; not answered. DISP_STAT bit 1 is vsync per `tools/m8seq/M8FLIP.ASM`.
- **Pointer:** probe drawing under the mouse pointer left a stale square when the pointer moved (the
  driver restored its saved background). Anything drawing outside the driver must lift the pointer first;
  a Direct3D layer must use the driver's pointer lift. M8SPAN ends with the driver's resync escape 0x4D3E.

## Sources

Local:
- velocity9x (`references/velocity9x`): `src/display32/d3d/d3d_core.c:3044-3110` (callback wiring: Execute
  null, RenderPrimitive, DX5 callbacks2), `d3d_soft.c:504-640` (caps of a CPU-rasterising HAL and why each
  bit is or is not claimed), `docs/decisions/2026-08-30-software-d3d-path-proven.md` (a HAL on a Trio64 with
  no 3D engine created a device and drew). Gave the HAL structure. Not in it: anything at 8 bpp.
- jhrobotics `vmhal9x/d3dhal_ddk.h`: DX5 execute-buffer HAL structures. Gave field layouts only.
- RealDOOM README (`references/web_captures/realdoom_README.md`): column/span/flat drawers with quality
  levels ("potato"); 5150/5160 class "sub 1 FPS" on an 8088. Gave the renderer seam. Not in it: anything
  on accelerators.
- 86Box sources as above; `docs/mach8_card_coverage.md`, `mach8_levers.md`, `mach8_poly_fill_design_2026_11_01.md`.

Web:
- https://learn.microsoft.com/en-ca/windows-hardware/drivers/ddi/d3dhal/ns-d3dhal-_d3ddevicedesc_v1 -
  `D3DDEVICEDESC_V1`: DDBD_8 is a legal render depth; the obsolete-but-required devcaps; transform and
  lighting caps must be zero. Not in it: whether the runtime accepts an 8-bit HAL.
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/content/d3dhal/ns-d3dhal-_d3dhal_callbacks -
  ContextCreate and RenderPrimitive are mandatory; DX7 moved texture and scene calls into DrawPrimitives2.
  Not in it: a stated minimum HAL.
- https://discmaster.textfiles.com/file/33716/PCOPEN15.ISO/SDK/DOCS/WEB/D3DRAMP.HTM?html=true - DirectX
  team, "Running fast with the D3D Ramp Driver": the ramp rasteriser is software, 8-bit, flat 20-30% faster
  than Gouraud. Not in it: any hardware (HAL) ramp or MONO device.
- https://github.com/viti95/FastDoom (via search summaries of its README, DoomWiki and VOGONS threads):
  potato mode at 80x200 for 8-bit ISA cards; 16-bit writes where possible; 8-bit ISA the main bottleneck.
  Not in it: accelerator use.
- https://www.vogons.org/viewtopic.php?p=672336 and the BOOST README
  (https://discmaster.textfiles.com/view/10596/science%20library(b).zip/science%20library(b)/TECHNICA/AUTOCAD/H108.ZIP/BOOST.ZIP/READ.ME):
  AutoCAD display-list drivers, including a protected-mode 8514/A one (BOOS8514.EXP) - the 8514's vector
  use in its day. Not in them: vectors per second or register use.
- https://setsideb.com/wolfenstein-3d-ported-to-the-8088-and-cga/ - Wolfenstein 3D runs on an 8088 with
  CGA. Not in it: how its columns are drawn.
- Searched, nothing found: any 8514/A or Mach8 3D, vector or polygon demo ("8514/A polygon fill 3D demo"),
  and any shipping Direct3D HAL at 8 bpp.
