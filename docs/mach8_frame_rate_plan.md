# Frame rates on the Mach8: plan (2026-11-02)

The owner's goal is demoscene and 8/16-bit game-scene work on the 5160, using silicon ATI's drivers never
touched. This file sets the order. Ideas and costs come from `docs/mach8_3d_research.md` (ranked approaches,
M8SPAN card numbers), `docs/mach8_graphics_vision.md` (card as decoder, layers, 3D around the card),
`docs/mach8_levers.md` (unused registers), worklist C and X, and `docs/next_session_2026_11_02.md` ("Final").
Method: `docs/mach8_driver_method.md`. Every rate here is from the card unless marked *estimate*.
IDs are local to this file and collide with worklist IDs (A, B, D, T): cite them as `FR-P1`, `FR-T1` in commits.

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
| P4 | CRT_OFFSET written mid-frame: does it take effect at the next line, the next frame, or immediately? | Page flip latching (T2) and per-line scroll splits (G4) | one probe, owner watches |
| P5 | M8VGAW (built, unrun) | VGA-half write cost for Mode X (X1, X8) | run only |
| P6 | Does D3D enumerate an 8-bit HAL? (DirectX 7.0a on bed and card, known) | Gates D1 | bed, stub HAL |
| P7 | DEST_CMP_FN honoured by the polygon fill's blit and by SCAN_TO_X | Gates the depth-band trick (D3) | one probe |
| P8 | Raw OUT cost from a Windows program (14 writes, one FIFO wait) and one IN, against the 7.64 / 3.75 us measured in DOS | Splits M8SPAN's ~2.4 us/write overhead between our loop and Windows (a trapped port costs a ring transition per write: the driver pays it too) | arm of P1-P4's probe |
| P9 | DISP_CNTL double scan on the 8514 CRTC (guide 8-7, 9-1: the Y counter counts each line twice; vertical registers re-encoded) | A 400- or 300-row mode: half the rows to draw, same monitor timing | bed first (86Box models it), then card, owner watches |
| P10 | CLOCK_SEL pixel clock divided by 2 with the horizontal registers halved | Half the pixels per row at the same line rate; with P9 a 400x300 mode on the 8514 side | divide is bit 6 (guide 9-3, `mach8_guide_audit.md:23`; bit 7 forces refresh) |

P1, P3, P4, P8 and O12 are one Win32 probe, `tools/m8probe/M8P0.C` (arms R, O, H, V; S with `M8P0 s`).

### Phase 0 results, card, 2026-11-02 (M8P0, boot 188, 800x600x8, two runs)

| Arm | Run 1 | Run 2 | Reading |
|---|---|---|---|
| R write 600 KB `rep stosd` | 65.8 ms (9.3 MB/s) | 66.2 ms | the program's own frame write: fixed |
| R read 600 KB `rep lodsd` | 35.4 ms (17.4 MB/s) | 35.4 ms | **the unlock's floor** |
| R one-at-a-time hash 600 KB | 63.5 ms | 64.0 ms | = today's 64 ms unlock: ~29 ms is hash arithmetic above the read. F2's ceiling is ~29 ms of 160 |
| O 14 OUTs + one FIFO wait | 68.5 us | 68.5 us | **4.6 us per OUT** (XTOUT clear, Win95). M8SPAN's loop spends ~10 us per write: **over half of every engine write is our code** (FIFO polls, C, cache refills). A1 is the largest lever |
| O one IN (EXT_FIFO_STATUS) | 4.52 us | 4.52 us | a status read costs a whole OUT: poll less (O15) |
| H A9h | 126 wraps/s, 00-FF | 128 | the counter runs, but the loop managed only ~4,350 samples/s (~230 us each): the 28800 index ports look trapped under Windows (DOS: 11.5 us). Not usable for beam racing in Windows |
| H VERT_SYNC / LINE_SYNC | 13 / 15 edges/s; toggles per frame 41-1068 | - | undersampled by the slow A9h reads: inconclusive |
| V V_BLANK latch, looked at every 1 ms | 170 per s | 159 per s | not one per frame (refresh ~56 Hz); the bed gave 72 = its refresh. Unexplained: next arm logs the hit intervals |
| M8CUBE 3 cubes, 10 s | 111.0 fps, 864 faces/s | 112.2 fps, 875 faces/s | **the baseline for T1, A1 and D2** |

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
| O4 | **Raster splits on the line counter** (not free: A9h is 11.5 us a poll; DISP_STATUS one IN): CRT_OFFSET per band for parallax bands, palette per band for copper bars and sky gradients | CRT_OFFSET, DAC, A8h/A9h | a few writes per band | Amiga copper, Mega Drive line scroll, Shadow of the Beast |
| O5 | **Beam racing**: draw or copy behind the beam, never wait for vblank | A9h | 0 | racing the beam (2600, 8088 MPH) |
| O6 | **Engine and CPU overlap**: queue a long engine op (clear, poly fill, big copy) then do maths with no bus traffic until the FIFO has room | FIFO (16, drops when full) | - | every console with a blitter |
| O7 | **VRAM as the asset store**: 1 MB is a fifth of system RAM. Sprites, pre-scaled sizes, tiles, glyphs, previous frames, uploaded once; frames become copy lists (vision doc: blitter as LZ77 decoder) | blit, source compare | commands only | Neo Geo sprite ROM, Lynx |
| O8 | **1 bpp across the bus, 8 bpp on the card**: masks and shapes sent as mono, expanded by the engine with FRGD/BKGD or kept as planes and expanded from VRAM (MONO_SRC 3) | colour expand | 1/8 of the pixels | Amiga blitter cookie-cut |
| O9 | **Engine blending**: 50% translucency (mix 17h), saturating light and shadow (1Bh/18h), MIN/MAX, at engine speed | arithmetic mixes (card = bed, M8MIX) | 1 write | Amiga minterms, SNES colour math |
| O10 | **Auto-advancing registers**: commands chosen so the engine's own CUR_X/CUR_Y after one op is the start of the next (rect fill leaves CUR at x+w,y+h; SCAN_TO_X continues; LINEDRAW 2 writes a vertex; SHORT_STROKE 2 vectors a write) | 8514 state | fewer writes per primitive | compiled sprites |
| O11 | **ROM and EEPROM**: the card ROM's far-call API and the EEPROM's undecoded words (00h-0Ch, 0Fh-10h; 0Dh/18h/23h/2Eh are monitor-timing records) and the shadow CRT sets. The ROM API is dead for speed (entries 6Ch/70h are `stc; retf`) | ROM, EEPROM | 0 | - |

### Know the capacity, fill it: no polling (owner, 2026-11-02)

| # | Idea | What decides it |
|---|---|---|
| O12 | **Latched vertical blank**: SUBSYS_STATUS bit 0 holds V_BLANK until cleared, so a frame is caught by a look every ~1 ms, not a tight loop (guide 8-19) | M8P0 arm V |
| O13 | **Card interrupts**: SUBSYS_CNTL enables V_BLANK and GE_IDLE interrupts in silicon. Every XT IRQ here is taken (2 NIC, 3 T130B, 4 COM1, 5 SB, 6 floppy, 7 LPT), and the card has an IRQ jumper (8-INFO: level 2/3/5 or not set; TEST.COM tests interrupt generation, `ati_readme_findings_2026_10_23.md:121`); its setting on this card is unknown | read the jumper; a free level needs one of 2/3/5 given up |
| O14 | **PIT locked to the frame** (8088 MPH): a VxD timer re-phased each frame from the V_BLANK latch gives a raster interrupt with no card IRQ: flips, palette splits and present copies on time while the program runs | O12 first; a VxD (custom_vkd toolchain) |
| O15 | **Open-loop FIFO**: the engine's time per command is known (short commands finish before the next OUT arrives, M8SPAN arm A), so status reads (~3.75 us each, half a write) are needed only after long ops, at a computed time | per-command engine cost table; FIFO-room waiting is now a win (`58c8237`, `m8_fifo_need` mem_to_mem +11-14 %) |
| O16 | **Pipeline the frame**: queue frame N's long engine work (clear, big fills, present copy started by the beam), then do frame N+1's transform, sort and command list on the CPU while the engine drains | A1 asm emitters, O15 |
| O17 | **Present without waiting**: triple buffering in VRAM (CRT_PITCH is free-form: at pitch 640 three 640x480 pages fit, 921,600 B; GE_PITCH must match), the flip done at vblank by O14, so the program never waits | O1, O14 |
| O18 | **Ring 0**: a VxD adds page Dirty bits (F4), the frame timer (O14) and the flip queue (O17); it cannot make an OUT cheaper. ATI.VXD switches its traps off after install (`ati_vxd_traps.md:24-33`) and is not in our path, so trapping is unlikely; P8 confirms | P8 |

### The bus itself: 8-bit XT slot, 16-bit card (owner, 2026-11-02)

The card runs with JU1 at 8-bit; the Inboard splits each word OUT into a low then a high byte cycle and the high
(odd) byte commits (`docs/captures/2026-10-08_m8byte/README.md`). The bus levers left:

| # | Lever | Evidence | Probe |
|---|---|---|---|
| P11 | **Card wait states**: MAX_WAITSTATES (6AEE bits 3:0, POST sets Ah), FIFO_OPT (36EE bits 0-1), HOST_8_ENA writes. PIX_TRANS costs 3.00 us/B against 2.17 for an undecoded port: ~28 % is the card's own IOCHRDY | sweep candidate 1, `iowidth_measured_2026_09_20.md` | timing per setting, pixels checked, then a soak (worklist B3) |
| P12 | **String I/O for same-port runs**: a polyline's vertices to LINEDRAW (FEEE) as one `rep outsw`; MACH.DRV does it to C6EE | sweep candidate 2 | arm against M8SPAN C |
| P13 | **The shared low-byte latch** (the XT-IDE high-speed idea turned round): one latch serves all ports and a lone high-byte OUT commits it, so a run of writes sharing a low byte could cost 5.7 us each, not 7.64 (25 %). Open: what the latch holds after word writes (M8BYTE3 saw 00h or an older colour) | M8BYTE2 3,4 | M8BYTE3 extended: latch after each kind of write; then whether an emitter can order writes to share low bytes |
| B4 | VGA half: the Inboard write-through cache over A0000 (the 28800 zero-wait bits AB 0/1/5 measured no gain, 10-10/10-12) | worklist B4 | for Mode X (X1) |

Not levers: the slot (the 5160's slots share one buffered bus at the board's 4.77 MHz; slot 8 differs only by
the Card Selected line), JU1 (the slot is 8-bit), dword OUTs (alias onto the DAC mask), the bus clock (the board
crystal also drives the PIT; owner's call, not proposed).

Closed for this card: **DMA**. The 8514/Mach8 has no DMA request path; the XT's 8237 moves bytes between
memory and a device that raises DREQ, not to an I/O port address; channel 0 is refresh, 1 is the SB, 3 the
parallel card (`xtcf_no_dma_channel3_parallel`).

Each O-item becomes a probe or a G-item demo screen once its probe answers.

## The CPU module: every port access costs more than the bus cycle

| # | Lever | Evidence | Test |
|---|---|---|---|
| - | **XTOUT is already clear** (`1000h:1 = 8C`, `a0f7d44`, 2026-09-28; FastDoom +1.1 %, #40): the CPU runs on during each OUT already, and every card number in this file was taken with it clear. Not a lever left | `5160_tuned_config.md:9` | - |
| P0b | **L1 flush on every port access** (module SW1 ON): each 16-byte line touched afterwards costs ~1.3 us to refill, likely most of M8SPAN's ~2.4 us a write. SW1 OFF is closed (#40, DOOM fails); the lever is code shape: emitters that touch one or two cache lines between OUTs, and `rep outsw` runs (~4.1-4.3 us a word in Win95 against 7.6 looped) | `bus_optimisation_plan.md:19-23`, `mach8_static_sweep_2026_10_31.md:116-122` | P2 measures it; A1 is written to it |
| - | RAM floor: `tools/gen_ramstride.py` (cache-hostile stride) never run; working sets over the L1 ~2.3x slower | `bus_optimisation_plan.md:469-471` | with P1 |

## Silicon not yet in use (from the sweep)

| # | Feature | Evidence | Use |
|---|---|---|---|
| S1 | Three CRT shadow sets, chosen by ADVFUNC_CNTL bit 2 in one write | `mach8_card_vs_86box.md:111-132` | pre-load a low-res set (O1) once; switch modes by one write |
| S2 | INSIDE_SCISSOR status + a CMD with DRAW=0: a hit test in 1 write + 1 read | M8A2, bed = card | sprite collision with no CPU maths |
| S3 | Extended blit source stream (B2EE/BEEE/C2EE): a linearly packed sprite unpacks into any rectangle in one blit | M8TILE | VRAM asset store without pitch waste (O7) |
| S4 | DP_CONFIG foreground source 4: hardware 4:1 coverage count (11h x popcount) | `captures/2026-10-07_m8txt/README.md:88-109` | antialiased edges and text; a 4:1 downscale at engine speed |
| S5 | Foreground source 6/7: a byte indexes the 16-entry colour-pattern register | same README:78 | engine-side colour lookup and dither expansion |
| S6 | 4 bpp pixel mode: 4 px a word, six 640x480 pages in 1 MB | worklist D8c | half the bus bytes for 16-colour frames; many flip pages |
| S7 | PATT_DATA dithered fills at plain-fill cost | M8FG6-9 | more apparent colours for flat shading (D2, G2); temporal dither by flipping |
| S8 | CMD 4 nibble fill: rate never timed against `40F3` | `mach8_cmd4_axes_2026_10_16.md` | possibly faster large clears |
| S9 | Display mode and VFIFO_DEPTH may set engine speed (refresh takes VRAM cycles); 31 Mpx/s was measured in an unstated mode | `mach8_guide_audit.md:23` | low-res modes may speed the engine beyond the pixel ratio |
| S10 | Clocks: 33.000, 45.000 and 14.318 MHz crystals, XTAL3 socket empty (ATI18810-2) | `mach8_card_coverage.md:275` | the pixel-clock menu for P10; the empty socket is a hardware option (owner's call) |
| S11 | MAJ/MIN survive a rect fill; ATIM8 streams host data with a run-time-patched `rep outsw` stub and no status check | `mach8_static_sweep_2026_10_31.md:33`, `atim8_disassembly.md:32-42` | O10 and the A1 emitter shape |

Constraints the sweep found: WRITE_PER_BIT = 0, so plane-masked ops (O3, G4) have no fast path and their cost is
untimed; source compare TRUE writes BKGD_COLOR through BKGD_MIX, and an engine reset does not clear 92EE; RD_MASK
is rotated one plane, WRT_MASK is not; SCAN_TO_X does not step CUR_Y; horizontal polygon boundary lines draw
nothing and an apex row or narrow neck leaks (`mach8_poly_fill_design_2026_11_01.md:70-91`), so D2 needs edge
handling; refresh (REFR64 on the card) bounds how long IOCHRDY may be held (P11).

## Closed: do not re-propose

Output switching between the halves (C2, C3: >1 s blank); 28800 zero-wait bits AB 0/1/5 and the unknown 28800
bits; split banking for speed; a 128 KB VGA window; the VGA half's 512 KB as a cache for the 8514; VGA-half
double buffering at 640x480; EXT_SHORT_STROKE C6EE (diagonals only); A14 blind bursts; byte pairs to word
registers; GE_IDLE_INT as an idle test; PATT_LENGTH bits, LINEDRAW_OPT bit 3, the 8x8 tile, block write, split
GE_OFFSET, gray-scale font assist (Mach32); CLIP_MODE pre-clip; the ROM API; an 8-bit or HiColor DAC (the IMSG176
is 6-bit); INSTALL's 16-bit bus option; SW1 OFF (#40); CPU clock above 2x; Inboard wait states (already 0);
XTOUT (already clear since 09-28); hardware polygon fill for GDI `Polygon` (GDI sends spans); a per-glyph VRAM cache; linear fingerprint hashes;
a RAM shadow over PIX_TRANS (D7); VRAM as swap; drawing the pointer into the fb16 buffer; pixel code in asm
alone, and compiler flags; RefreshRate keys, SciTech linear mapping, DDC, ignoring WAIT flags, our own mini-VDD
for speed. MACH.DRV's "aperture" is the VGA half's A000 window, not 8514 memory.

## Dropped items recovered (owner and contributors first)

- Owner (11-01): asm for the hot path of every one of the 35 GDI tests, nearest-1.0x first, benched twice.
- Owner's checks on `m8rowd`/`m8oaat`: 640 <-> 800 at High Color, Explorer text, WinZip icon after deleting
  `ShellIconCache` (only dxdiag 1-3 reported).
- Owner: "DOOM mode", a source port whose walls are pre-scaled column blits and flats span fills (G3's target;
  RealDOOM's drawer split, FastDoom's XT timing).
- Owner: software Direct3D at 16 bpp as stage 1 (worklist X6): run once as the baseline, then leave.
- Andrew (#50): time a 512-word `rep insw` from the XT-CF with the L1 on and off.
- Hangs: #55 (16 bpp restart, SB Pro looping its DMA buffer, also at the DirectX installer), #56 (screenshots
  during DirectDraw tests), boot stalls after driver restarts (10-24, 10-31).
- Bed: 86Box SCAN_TO_X ignores PATT_INDEX (owner decides the fork fix); GDI slowdown after dxdiag until reboot.
- Bench hygiene: re-run the slower single-run tests and ATI's 36-test base; an untimed warm-up pass in M8BENCH.
- Mode X: trace the driver entries DDRAW calls on entry and exit; hand the output to the VGA half; the KERNEL32
  fault after a failed attempt; A0000 not mapped for the app (`nopal`).
- DirectSound HAL and DirectInput (#58); optional VESA via VVESA.COM for DOS games.
- Compiled deltas (Trixter): a frame's changes as code with immediates.
- Uncommitted `M8STEP` pclog in `86box_auto` `vid_ati_mach8.c`: delete when its question is answered.

## Exemplars not yet used

- reenigne's ISA bus sniffer (https://www.reenigne.org/blog/isa-bus-sniffer/): watch the card's real OUT cycle
  and IOCHRDY.
- MartyPC (https://github.com/dbalsom/martypc): refresh DMA and wait states 86Box does not model.
- xms0g/3Dos (https://github.com/xms0g/3Dos): a fixed low-poly scene to time, then port to lines and fills.
- JHRobotics vmhal9x (https://github.com/JHRobotics/vmhal9x): a working Win9x HAL beside velocity9x for D1.
- Win95 DDK ATI mini-VDD (`DISPLAY/SAMPLES/VDD/ATI/ATI.ASM`): X1's output hand-over.
- Richter and Smith, *Graphics Programming for the 8514/A*
  (https://archive.org/details/graphics-programming-for-the-8514-a): compare semantics for P7 and D3.
- PicoGUS (https://github.com/polpo/picogus): holding IOCHRDY halts the bus (P11).

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

- [ ] **B1** Two or three 640x480 pages in VRAM (pitch 1024 or 640), Flip = CRT_OFFSET, re-asserted after any
      ADVFUNC_CNTL/MEM_CNTL write (they reset pitch and offsets); sprites as keyed engine copies (done, `7bca399`, 869/s); Lock of a VRAM surface through a RAM shadow and fingerprints.
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
2. T1 on the card (owner watches M8CUBE with and without `v`), then F1 (one line).
3. One scoreboard script: M8CUBE, M8DDB, M8BENCH, twice each, against the baseline.
4. P6 in the bed (does D3D take an 8-bit HAL?): it decides D1's shape.
5. A1 asm emitters, measured by M8CUBE faces/s (owner: asm before Direct3D; D3D sits on them).
6. D1-D2 (Direct3D, windowed, at 8 bpp).
7. F2-F3, then B1 and T2 (full screen at both depths).
8. The rest of Phase 5 as each lever lands; Phase 6 alongside, one technique per sitting.
