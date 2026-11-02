# Frame rates on the Mach8: plan (2026-11-02)

The owner's goal is demoscene and 8/16-bit game-scene work on the 5160, using silicon ATI's drivers never
touched. This file sets the order. Ideas and costs come from `docs/mach8_3d_research.md` (ranked approaches,
M8SPAN card numbers), `docs/mach8_graphics_vision.md` (card as decoder, layers, 3D around the card),
`docs/mach8_levers.md` (unused registers), worklist C and X, and `docs/next_session_2026_11_02.md` ("Final").
Method: `docs/mach8_driver_method.md`. Every rate here is from the card unless marked *estimate*.

## Where the time goes now

| Path | Card cost | Limit |
|---|---|---|
| 16 bpp full-screen frame (`fs16`) | app write ~80 ms + unlock read 64 ms + send | Inboard RAM (~7.5 MB/s write) |
| Engine register write in a loop | ~10 us effective vs 7.64 us bus (M8SPAN B) | ~2.4 us per write is ours: loop, FIFO checks, C |
| Polygon fill | 0.2 ms + bbox area x 35 ns | bus for small, engine for large |
| M8CUBE, 3 cubes, 8 bpp window | 121.5 fps, tears | present copy not synchronised to the beam |
| Pan / 64 palette entries | 15 us / 0.85 ms | - |

So: at 16 bpp the CPU's own RAM is the wall; at 8 bpp with the engine drawing, the bus carries commands and
frame rates are high. The fast route for games and demos is 8 bpp, engine-drawn, page-flipped.

## Phase 0: one card sitting of probes (cheap, decide the rest)

| # | Probe | Answers | Cost |
|---|---|---|---|
| P1 | RAM read floor: `rep lodsd` over 600 KB, and the same with the one-at-a-time hash | Is the 64 ms unlock the read itself (floor) or our hashing? Bounds F2 | new arm in M8DDB, one run x2 |
| P2 | M8SPAN arm B and C rewritten as a tight asm loop (`out dx,ax` runs, FIFO check per 16) | How much of the ~2.4 us/write overhead asm recovers; sizes A1 | one probe |
| P3 | Raster counter as the vblank source (A9h/A8h, M8RAST) replacing arm F | Lines per frame at 800x600, where vblank starts; arm F's pulse is ~3 lines and a pre-empted Windows loop misses it | one probe |
| P4 | CRT_OFFSET written mid-frame: does it take effect at the next line, the next frame, or immediately? | Page flip latching (T3) and per-line scroll splits (G4) | one probe, owner watches |
| P5 | M8VGAW (built, unrun) | VGA-half write cost for Mode X (X1, X8) | run only |
| P6 | Does DDRAW/D3D on the card enumerate an 8-bit HAL? DirectX version on the card first | Gates D1 | bed, stub HAL |
| P7 | DEST_CMP_FN honoured by the polygon fill's blit and by SCAN_TO_X | Gates the depth-band trick (D3) | one probe |
| P8 | Raw OUT cost from a Windows program (14 writes, one FIFO wait) and one IN, against the 7.64 / 3.75 us measured in DOS | Splits M8SPAN's ~2.4 us/write overhead between our loop and Windows (a trapped port costs a ring transition per write: the driver pays it too) | arm of P1-P4's probe |
| P9 | DISP_CNTL double scan on the 8514 CRTC (guide 8-7, 9-1: the Y counter counts each line twice; vertical registers re-encoded) | A 400- or 300-row mode: half the rows to draw, same monitor timing | bed first (86Box models it), then card, owner watches |
| P10 | CLOCK_SEL pixel clock divided by 2 with the horizontal registers halved | Half the pixels per row at the same line rate; with P9 a 400x300 mode on the 8514 side | guide 9-4 first (divide bit: model says bit 6, a guide table says bit 7) |

P1, P3, P4, P8 and O12 are one Win32 probe, `tools/m8probe/M8P0.C` (arms R, O, H, V; S with `M8P0 s`).

## Thinking like the demoscene: the bus is the enemy

Every byte that crosses the XT bus stalls the CPU (CPU and bus time add). Speed comes from the card's own
silicon, so each technique below sends commands or register writes, not pixels. Closed already, not
re-proposed: dword OUTs to PIX_TRANS (M8DWORD: the upper word lands in the DAC mask), a memory aperture,
the hardware cursor (Mach32 only), the 8x8 mono pattern (Mach32 only). `docs/mach8_static_sweep_2026_10_31.md`.

| # | Idea | Silicon | Bus per frame | Exemplar |
|---|---|---|---|---|
| O1 | **Low-resolution 8514 modes by CRTC reprogramming** (P9, P10): 400x300 or 320x240 in a standard monitor timing. Every fill, span, polygon and 16 bpp frame shrinks 2-4x | DISP_CNTL double scan, CLOCK_SEL divide, mode table | 0 (mode set once) | 8088 MPH's CRTC modes |
| O2 | **Send a quarter, let the engine double it**: a 320x240 frame sent, then engine copies spread columns and rows (the stretch path already proven on the card, 10-23) | blit | ~560 blits, ~20 ms *estimate* vs 4x the pixels | Amiga/ST "chunky" doubling |
| O3 | **Plane animation**: draw up to 8 one-bit frames into the 8 bit-planes once (WRT_MASK), then play them by palette writes only; 16 with two pages, more on a 1024x1024 canvas | WRT_MASK, DAC | 1 palette load (~1-3 ms) | Amiga and ST bitplane animation |
| O4 | **Raster splits on the line counter**: CRT_OFFSET per band for parallax bands, palette per band for copper bars and sky gradients | CRT_OFFSET, DAC, A8h/A9h | a few writes per band | Amiga copper, Mega Drive line scroll, Shadow of the Beast |
| O5 | **Beam racing**: draw or copy behind the beam, never wait for vblank | A9h | 0 | racing the beam (2600, 8088 MPH) |
| O6 | **Engine and CPU overlap**: queue a long engine op (clear, poly fill, big copy) then do maths with no bus traffic until the FIFO has room | FIFO (16, drops when full) | - | every console with a blitter |
| O7 | **VRAM as the asset store**: 1 MB is a fifth of system RAM. Sprites, pre-scaled sizes, tiles, glyphs, previous frames, uploaded once; frames become copy lists (vision doc: blitter as LZ77 decoder) | blit, source compare | commands only | Neo Geo sprite ROM, Lynx |
| O8 | **1 bpp across the bus, 8 bpp on the card**: masks and shapes sent as mono, expanded by the engine with FRGD/BKGD or kept as planes and expanded from VRAM (MONO_SRC 3) | colour expand | 1/8 of the pixels | Amiga blitter cookie-cut |
| O9 | **Engine blending**: 50% translucency (mix 17h), saturating light and shadow (1Bh/18h), MIN/MAX, at engine speed | arithmetic mixes (card = bed, M8MIX) | 1 write | Amiga minterms, SNES colour math |
| O10 | **Auto-advancing registers**: commands chosen so the engine's own CUR_X/CUR_Y after one op is the start of the next (rect fill leaves CUR at x+w,y+h; SCAN_TO_X continues; LINEDRAW 2 writes a vertex; SHORT_STROKE 2 vectors a write) | 8514 state | fewer writes per primitive | compiled sprites |
| O11 | **ROM and EEPROM**: the card ROM's far-call API and the EEPROM's undecoded words (10h-15h, 23h-2Fh) for modes and clocks ATI never offered | ROM, EEPROM | 0 | - |

### Know the capacity, fill it: no polling (owner, 2026-11-02)

| # | Idea | What decides it |
|---|---|---|
| O12 | **Latched vertical blank**: SUBSYS_STATUS bit 0 holds V_BLANK until cleared, so a frame is caught by a look every ~1 ms, not a tight loop (guide 8-19) | M8P0 arm V |
| O13 | **Card interrupts**: SUBSYS_CNTL enables V_BLANK and GE_IDLE interrupts in silicon. Every XT IRQ here is taken (2 NIC, 3 T130B, 4 COM1, 5 SB, 6 floppy, 7 LPT), and whether the card's IRQ line is wired is unknown | card manual and jumpers (static first); then a free IRQ or none |
| O14 | **PIT locked to the frame** (8088 MPH): a VxD timer re-phased each frame from the V_BLANK latch gives a raster interrupt with no card IRQ: flips, palette splits and present copies on time while the program runs | O12 first; a VxD (custom_vkd toolchain) |
| O15 | **Open-loop FIFO**: the engine's time per command is known (short commands finish before the next OUT arrives, M8SPAN arm A), so status reads (~3.75 us each, half a write) are needed only after long ops, at a computed time | per-command engine cost table; "wait for free slots" gave 0.97 (10-21) |
| O16 | **Pipeline the frame**: queue frame N's long engine work (clear, big fills, present copy started by the beam), then do frame N+1's transform, sort and command list on the CPU while the engine drains | A1 asm emitters, O15 |
| O17 | **Present without waiting**: triple buffering in VRAM (needs O1's smaller modes: three 640x480 pages do not fit in 1 MB), the flip done at vblank by O14, so the program never waits | O1, O14 |
| O18 | **Ring 0**: a VxD adds page Dirty bits (F4), the frame timer (O14) and the flip queue (O17); it cannot make an OUT cheaper. If P8 shows the 8514 ports are trapped in the System VM, removing the trap is the largest single gain for every driver write | P8 |

### The bus itself: 8-bit XT slot, 16-bit card (owner, 2026-11-02)

The card runs with JU1 at 8-bit; the Inboard splits each word OUT into a low then a high byte cycle and the high
(odd) byte commits (`docs/captures/2026-10-08_m8byte/README.md`). The bus levers left:

| # | Lever | Evidence | Probe |
|---|---|---|---|
| P11 | **Card wait states**: MAX_WAITSTATES (6AEE bits 3:0, POST sets Ah), FIFO_OPT (36EE bits 0-1), HOST_8_ENA writes. PIX_TRANS costs 3.00 us/B against 2.17 for an undecoded port: ~28 % is the card's own IOCHRDY | sweep candidate 1, `iowidth_measured_2026_09_20.md` | timing per setting, pixels checked, then a soak (worklist B3) |
| P12 | **String I/O for same-port runs**: a polyline's vertices to LINEDRAW (FEEE) as one `rep outsw`; MACH.DRV does it to C6EE | sweep candidate 2 | arm against M8SPAN C |
| P13 | **The shared low-byte latch** (the XT-IDE high-speed idea turned round): one latch serves all ports and a lone high-byte OUT commits it, so a run of writes sharing a low byte could cost 5.7 us each, not 7.64 (25 %). Open: what the latch holds after word writes (M8BYTE3 saw 00h or an older colour) | M8BYTE2 3,4 | M8BYTE3 extended: latch after each kind of write; then whether an emitter can order writes to share low bytes |
| B2/B4 | VGA half: 28800 zero-wait-state bits (AB 0/5) and the Inboard write-through cache over A0000 | worklist B | for Mode X (X1) |

Not levers: the slot (the 5160's slots share one buffered bus at the board's 4.77 MHz; slot 8 differs only by
the Card Selected line), JU1 (the slot is 8-bit), dword OUTs (alias onto the DAC mask), the bus clock (the board
crystal also drives the PIT; owner's call, not proposed).

Closed for this card: **DMA**. The 8514/Mach8 has no DMA request path; the XT's 8237 moves bytes between
memory and a device that raises DREQ, not to an I/O port address; channel 0 is refresh, 1 is the SB, 3 the
parallel card (`xtcf_no_dma_channel3_parallel`).

Each O-item becomes a probe or a G-item demo screen once its probe answers.

## Phase 1: full-screen 16 bpp blit (fs16)

In order, each measured with M8DDB `fs16` twice:
- [ ] **F1** `m8_fb16_flush`: skip `m8_fb16_uniform` above a size. It scans every row while the top rows are
      one colour, which is what a game with a black border sends. Measure `mx_lock`.
- [ ] **F2** Whole-rect fingerprint and diff in one asm call (today one far call per row through
      `M8_FB16_ROWDIFF`); read each piece once. Expected gain bounded by P1.
- [ ] **F3** 16 bpp HAL Flip with a RAM back buffer: DirectDraw stops copying the back buffer each frame
      (~80 ms *estimate*); Flip fingerprints the back buffer and sends what changed, then swaps.
- [ ] **F4** (only if sparse writers matter) page Dirty bits through a small VxD: on Flip, hash only pages
      written. No gain for programs that rewrite the whole frame.

Ceiling: the program's own 600 KB write stays. 16 bpp full screen will not pass ~6-8 fps *estimate*; that
is the RAM, not the card.

## Phase 2: tearing

- [ ] **T1** Window present by beam: the engine copies a window row (~300 px) faster than the beam scans one
      (*estimate* 19 us vs 28 us), so start the copy when the line counter passes the window's top and it
      stays ahead; or start just below the window's bottom. No wait for vblank, no lost frames. Poly escape
      `m8p_frame`, before `m8_hw_copy`.
- [ ] **T2** Full-screen 8 bpp: two pages in VRAM, Flip = CRT_OFFSET at vblank (15 us). Per P4.

## Phase 3: Direct3D HAL at 8 bpp (#58, X7)

- [ ] **D1** Stub HAL in `M8DD32.DLL`: `D3DHAL_GLOBALDRIVERDATA`, ContextCreate, RenderPrimitive, DX5
      callbacks2; caps COLORFLATRGB, DDBD_8, no textures, no Z (velocity9x structure,
      `mach8_3d_research.md` "Direct3D HAL"). Gate: device creates in the bed (P6).
- [ ] **D2** Triangles to the polygon fill through the existing escape path (0x4D3F, pointer lift, 8-plane
      scratch batching). Colour: first vertex RGB to a palette ramp index.
- [ ] **D3** Hidden surfaces: sort at EndScene (painter's), then the depth-band compare trick (DEST_CMP_FN,
      one write per polygon) once P7 shows the compare works on the fill path.
- [ ] **D4** Present per T1 (window) or T2 (full screen). Test: our own DX5 test program, then dxdiag's
      Direct3D tests, then real DX5 titles that accept flat shading.

## Phase 4: 8 bpp full-screen DirectDraw (lever b)

- [ ] **B1** Two 640x480 pages at pitch 1024 in VRAM, Flip = CRT_OFFSET; sprites as keyed engine copies
      (869/s measured); Lock of a VRAM surface through a RAM shadow and fingerprints.
- [ ] **B2** Mode X on the VGA half (X1) for existing software that writes pixels, sized by P5.

## Phase 5: asm (owner: "tight, asm")

Move a loop to asm only after its lever is proven and only with a before/after card number.
- [ ] **A1** Engine command emitters: `m8_hw_poly`, spans, lines, fills (P2 sizes the gain; up to ~24% per
      write *estimate*).
- [ ] **A2** fb16 unlock path (F2).
- [ ] **A3** `m8_fb16_uniform` and `m8_fb16_send` per-row set-up (X5).
- [ ] **A4** M8CUBE's transform: fixed point, no FPU calls, so the CPU work overlaps the engine's fill.

## Phase 6: the technique library and demo parts

A DOS + Windows library (vision doc "Where it can land", 2) of what the card does without the bus, each part a
demo screen that proves one technique on the card. Exemplars are for method, not to copy.

| # | Technique | Silicon | Exemplar |
|---|---|---|---|
| G1 | Vector / wireframe: LINEDRAW polylines, short strokes for text, BOUNDS as the free erase rectangle | 9AEE/FEEE, 9EE8, 72EE-7EEE | Vectrex, Atari vector arcade |
| G2 | Flat-shaded polygons, palette-ramp lighting | polygon fill, DAC | Amiga blitter area fill, Starglider 2, Elite |
| G3 | Raycaster: one-pixel-wide fills per column; textured by pre-scaled columns in spare VRAM | fill, blit | Wolfenstein 3D, Catacomb 3-D |
| G4 | Hardware scroll and parallax: CRT_OFFSET, layers in bit-planes with write masks combined by the palette (dual playfield); line splits if P4 allows | CRT_OFFSET, WRT_MASK, DAC, A9h | Amiga dual playfield and copper, Mega Drive line scroll, Shadow of the Beast |
| G5 | Palette cycling, fades, splits on the line counter | DAC, A9h | Mark Ferrari, Amiga copper bars |
| G6 | Scaled sprites: N pre-scaled copies in VRAM, keyed copies | 92EE/EAEE source compare | Lynx Suzy, Sega Super Scaler, Neo Geo |
| G7 | Pseudo-3D road: ~3 spans per row | SCAN_TO_X | OutRun, Lotus |
| G8 | Isometric dirty regions: tiles and sprites in VRAM, redraw what moved | blit, compare | Knight Lore, Head over Heels, Marble Madness |
| G9 | Blitter as decoder: frames as copies from a VRAM dictionary plus residual pixels | blit, colour expand | Trixter's 8088 video streaming, LZ77 |
| G10 | Engine blending: the arithmetic mixes ATI never used | FRGD/BKGD mixes | Amiga minterms |

Order: G1, G2 (M8CUBE exists), G5, G4, G3, G6, then the rest. Each is one probe or demo, card-measured,
pixels checked in the bed first (the bed has no POLY_MODE via LINEDRAW: fill pixels are card-only).

## Order of work

1. Phase 0 probes (one sitting).
2. T1 (small, fixes what the owner saw) and F1 (one line).
3. D1-D2 (Direct3D, windowed, at 8 bpp).
4. F2-F3, then B1 and T2 (full screen at both depths).
5. Phase 5 asm as each lever lands; Phase 6 alongside, one technique per sitting.
