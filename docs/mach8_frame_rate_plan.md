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
