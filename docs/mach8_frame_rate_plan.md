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
| M8CUBE 3 cubes, `v` (T1, boot 189, `m8vbl`) | 68.5 fps, 535 faces/s | - | locked to the 72 Hz refresh. The owner's stills (`photos/dxcubes/IMG_2095-2111`) show no tear line: what looked like tearing was the polygon fill leaking where both edges share a pixel (spurs, combs, notches), fixed by an XOR outline plus the outline drawn over the fill |
| M8CUBE 3 cubes, fill fixed (`m8solid`, boot 191) | 101.8 fps, 787 faces/s | 100.1 fps, 775 faces/s | owner: solid, clean edges, no issues. The outline over each face costs ~12 %: A1 wins it back. `v` 69.2 fps. Bench: pixels = ATI in 35, geomean 1.79 |

### Scoreboard baseline `solid` (`tools/m8_score.ps1`, card, `m8solid`, 800x600x8, 2026-11-02)

Every lever is compared against this, run the same way (over the agent: agent-run M8CUBE is slower than
the owner's console runs, 91-93 vs 100-102 fps, so never mix the two).

| Test | Run 1 | Run 2 |
|---|---|---|
| M8CUBE 3 cubes | 90.8 fps, 703 faces/s | 92.9 fps, 721 faces/s |
| dd_fill_small / dd_fill_large | 1184 / 451 ops/s | 998 / 448 |
| dd_vid_small / dd_sprite / dd_scroll | 1229 / 802 / 441 | 1202 / 806 / 420 |
| dd_sys_small / dd_sys_large | 56.2 / 4.08 | 56.9 / 4.02 |
| dd_lock_small / dd_lock_large | 24.0 / 1.07 | 26.7 / 1.11 |
| OUT / IN | 4.51 / 4.52 us | same |

### Scoreboard `ddstr` (card, `m8ddstr`, 2026-11-03): string I/O in M8DD32, M8CUBE fixed point by default

| Test | Run 1 | Run 2 | vs `solid` |
|---|---|---|---|
| M8CUBE 3 cubes | 102.6 fps, 793 faces/s | 104.8 fps, 810 | +13 % (M8CUBE's fixed point, not the driver): new cube baseline |
| dd_sys_small / dd_sys_large | 75.2 / 5.68 | 64.2 / 5.58 | +24 % / +39 % |
| dd_lock_small / dd_lock_large | 39.6 / 1.72 | 36.3 / 1.64 | +50 % / +55 % |
| dd_fill, dd_vid, dd_sprite, dd_scroll | 932-1028 / 410-424, 1192-1201, 748-795, 410-454 | | unchanged (engine only) |

M8BENCH `card_ddstr1`: pixels = ATI in 35, geomean 1.79x ATI (GDI does not use M8DD32).

### Where a cube frame goes (card, `m8solid`, 2026-11-03)

| Cubes | fps | Frame | Faces per frame |
|---|---|---|---|
| 1 | 232.0 | 4.3 ms | 2.0 |
| 2 | 129.2 | 7.7 ms | 5.2 |
| 4 | 68.4 | 14.6 ms | 10.2 |
| 8 | 35.3 | 28.3 ms | 20.8 |

~1.3 ms a face, <1 ms fixed a frame. A face's ~45 writes are ~0.2 ms and its fill ~0.3 ms, so ~0.8 ms a face
is unexplained. Not the scratch clear (already the union of the faces' boxes); not float maths (an FPU is
fitted, owner; M8FPU: 3 us a float multiply-add). Next: the profile slots 45-50 (poly3d.c, m8hw.c FIFO waits,
read by `M8CUBE ... p`), which need the code-segment split below.

**Split, card, 11-03:** M8CUBE's own maths (`n`, no driver call) 4.2 ms a frame = 40 %; profile (`m8prof`,
`M8CUBE 10 3 p`; each stamp ~84 us, so driver slots read high): escape 7.8 ms, of it polygons 6.2, FIFO waits 0.4,
background fill 0.3, present 0.4. The engine rarely makes the CPU wait.

| Lever | Result (card, two runs) | Verdict |
|---|---|---|
| Outlines to LINEDRAW by `rep outsw` (`m8stream`) | cube 86.1 / 86.1 fps vs 90.8 / 92.9 | **slower, reverted**: the queue's loop was already near the bus floor; the extra FIFO read and copy cost more. The ~10 us a write was M8SPAN's own C loop, not the driver's queue. Diff kept outside the tree |
| Fixed-point maths in M8CUBE (`x`: Q14 sine table, Q8 coordinates, integer projection) | maths only 235-240 -> 605-607 fps (4.2 -> 1.65 ms); full frame 83-87 -> 104.7 / 104.7 fps (on `m8stream`) | **2.6x on the maths, +22 % a frame**: the method for every 3D routine of ours |

**String I/O, measured (M8P0 arms, card, 11-03, two runs):** 14 words to FRGD_COLOR then one FIFO wait: values in
registers 67.7 us (4.5 us a word, the floor); from memory by `rep outsw` 69.2 us (4.6); from memory by a C loop of
single OUTs 84-86 us (5.7). The L1 refill after each OUT costs ~1.2 us a word in a tight loop (not ~5: M8SPAN's
10 us was its own loop). String I/O pays on long runs (pixel transfers), not on short register runs.

**Cross-driver audit (11-03, Sonnet, read-only; gains corrected by the line above):**
- `m8dd32.c:297, :312, :952, :961`: DirectDraw lock/unlock and system-memory blits send pixels one OUT/IN a word:
  `rep outsw`/`rep insw` per row, ~20 % off their bus time (dd_lock, dd_sys rows). Do first.
- `m8hw.c` `m8_hw_set_palette`: three byte OUTs per entry from an RGBQUAD loop; packed buffer + `rep outsb` for O3/G5.
- `m8hw.c` `m8_hw_line_fast`: up to 12 LINEDRAW words looped; small (~14 us a polyline).
- `m8_hw_wait_vblank` polls back to back for up to a frame: pace it (~1 ms looks) so SB DMA and the XT-CF keep the bus (#47).
- `drivers/swapcomp/src/SWAPCOMP.ASM:392` LZ4 hash is one IMUL a position (#54): a shift-xor hash, re-check the ratio.
- `custom_vkd/src/VKD.ASM:423-432` `VKD_Wait_Empty` runs all 65,536 reads of port 64h (no 8042 on the XT):
  ~0.3 s of boot, once (#48); confirm by BOOTLOG timestamp.
- Already fine: XTIDETR.ASM data path and paced polls, spans/blt/pixel asm, hash.asm, the GDI pixel paths' `rep outsw`.

**Code-segment ceiling (blocks A1 and D1):** the plain driver's `_TEXT` is 0xFD59 = 64,857 of 65,536 bytes
(679 free); the `-Prof` build is 1,871 bytes over and does not link. New asm, Direct3D and profiling all need
the cold code (dd16.c, fb16.c, poly3d.c) moved into a second segment behind far entry points first.

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

Reopened 2026-11-06 for test only: the 128 KB window (BDh bit 2 never written) and "VGA half cannot flip" (no source). See docs/mach8_vga_half_findings_2026_11_06.md.

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
- [x] **F3** 16 bpp HAL Flip with a RAM back buffer (2026-11-03), done as descriptions, not fingerprints
      (owner: "why redraw what hasn't changed"). A buffer is held as a colour plus dirty rectangles; a full
      clear is one value, never written; Flip fills the card's old rectangles (only where the new frame does
      not cover them) and sends the new ones. A Lock, or a blit we decline, puts the buffer in RAM and the
      next Flip uses the fingerprint path. DirectDraw swaps the buffers' memory after each Flip
      (`80312100 -> 80167760`); descriptions follow the memory. A new back buffer is described black.
      Card, `M8FLIP`, 640x480x16, M8DDB `fs16`: `mx_box` (dxdiag's frame through a lock) 7.37 fps;
      `mx_flip` (clear + box fill + Flip) **164.6**; `mx_flipblt` (box from a sprite) **199.9**; card = RAM
      in every test. Faults found on the way and fixed: static after test 5 (an undrawn back buffer flipped:
      now described black); white background and black trail (a declined blit drew behind the
      description: now handled; fills with a stale colour and mix: registers written afresh each flip,
      from an idle engine, as the rewrite of DP_CONFIG halts a running command, guide 9-41: the card
      lost fills without the idle wait, the bed did not). Card, `M8FLIP4`, two agent runs: `mx_box` 6.8,
      `mx_flip` 208 / 220, `mx_flipblt` 174 / 116 fps, card = RAM in all. Open: why the shadows went
      stale; dxdiag test 3 stutters now and then on the card (owner).
- [ ] **F4** (only if sparse writers matter) page Dirty bits through a small VxD: on Flip, hash only pages
      written. No gain for programs that rewrite the whole frame.

Ceiling, for a program that writes every pixel itself: its own 600 KB write stays, ~6-8 fps *estimate*.
A program that draws through Blt and Flip is not bound by it (F3: 165-200 fps).
- [ ] **F5** Adaptive palette in full-screen exclusive mode (owner, 11-03): each colour a fill or blit
      uses takes its own entry (one DAC write), learned from the calls, never a screen scan. Flat, exact
      colours (the dither shows as fine dots on random fills); every one-colour block one engine fill.
      Past 256 colours, the fixed palette and one repaint.
- [ ] **F6** Active region (owner, 11-03: dxdiag only draws in the centre): the box where frames change,
      observed over recent frames. A Lock fallback compares it first; outside it, only pages whose 486
      Dirty bit is set (F4's VxD), so the rest is proven unchanged, not assumed. Feeds G11 (present in the
      blank) and G12 (background kept in card memory).

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
| G11 | **Present inside the blank** (owner, 11-02, after the cursor fixes): copy only each object's old and new boxes, not one box around all of them, so the whole present fits the ~1.2 ms blank (~24,000 px at engine speed) and cannot tear at any speed; fewer pixels copied also frees bus and engine time | blit, V_BLANK latch | save-under, dirty rectangles |
| G12 | **A background for free**: a picture kept in spare VRAM; erase each object by engine-copying its patch back instead of filling black. A starfield or scene costs what black costs | blit | Amiga restore-background blits, arcade boards |
| G14 | **Translucent objects by the fill's mix** (seen by accident, card, 2026-11-02, `m8leak`, owner's stills `photos/dxcubes/IMG_2114-2117`): the polygon fill run with ALU_FG_FN = XOR made overlapping cubes look see-through. One register write per face picks the look: XOR (cheap, inverts, the accident), 17h (50 % blend, M8MIX), 1Bh/18h (saturating light and shadow), MIN/MAX. The colour ramps must be laid out so the blend lands on a sensible index | ALU_FG_FN, arithmetic mixes | Amiga minterms, SNES colour math |
| G13 | **Background and objects in separate bit-planes** (dual playfield): erase by clearing only the object planes, the background is never touched; the palette composes them. WRITE_PER_BIT = 0, so the masked-write cost is measured first | WRT_MASK, DAC | Amiga dual playfield |

Order: G1, G2 (M8CUBE exists), G5, G4, G3, G6, then the rest. Each is one probe or demo, card-measured,
pixels checked in the bed first (the bed has no POLY_MODE via LINEDRAW: fill pixels are card-only).

## Characters as textures (owner, 2026-11-05)

A texture is a set of characters replayed: use the card's ROM font as the source. Desk reasoning plus a read of
`roms/video/mach8/BIOS.BIN`; no probe run. Numbers marked *estimate* are arithmetic from measured rates.

**What is on the ROM:** 8x8 font at 0x3B04 (2,048 B), 8x14 at 0x4304 (3,584 B); no 8x16 table follows it. Chars
B0h-B2h are 25/50/75 % dither, DBh solid, C0h-DFh box and line pieces: ready-made shading and panel patterns.

**What the ROM is not:** free on every use. Only the CPU reads it (2.71 us a byte over the bus, measured,
`mach8_card_coverage.md:246`); no engine source selects it, and the two halves share no path. It is free in
system RAM and on disk: the driver carries no assets. The 8x8 font costs ~5.5 ms once. The EEPROM is out: 128 B,
read serially, and it holds the monitor timings.

**So there are two real versions:**

| # | Version | Silicon | Bus cost | Limit |
|---|---|---|---|---|
| CT1 | **VRAM as CHR**: copy the font into spare 8514 memory once, then each cell is one engine stamp. Compose bricks, panels and dither into an 8 bpp atlas once at start, for M8COLS's column blits | mono expansion, blit | ~28 us a stamp (6 writes, *estimate*); 1,000 8x8 cells ~28 ms against ~190 ms for 320x200 by upload | 8514 half; two colours a stamp; plane-masked expansion from VRAM untimed (WRITE_PER_BIT = 0) |
| CT2 | **The VGA half's character generator**: the only hardware here that turns codes into pixels with no CPU and no engine. BIOS already loads the font into plane 2. A frame is 4,000 B at 80x25, 8,000 B at 80x50; font memory rewritten once makes 256 (512 with attribute bit 3) custom tiles. This is the PETSCII idea exactly | 28800 text mode, max scan line | 4,000 B ~7.8 ms at 1.96 us a byte (measured on A0000, *estimate* for B8000); mode 13h is 64,000 B ~130 ms | whole screen on the VGA half, one half per mode; two colours a cell |

CT2 is the strongest result for video: 80x25 frames plus an XT-CF read (~10.4 ms, 2.6 us a byte) leave room for
30 fps with sound, *estimate*, where the plan in `docs/video_on_the_5160.md` sees ~17 fps at 160x120. The PC
picks each cell's glyph and colours; only changed cells need sending.

**The CPU cache (owner):** any OUT empties the L1 (SW1), so the 8514 half, which has no aperture, can never run a
CPU texture loop from L1 without a refill per write. The VGA half takes pixels by memory write, not OUT, so a 4 KB
texture can stay in L1 through the loop: VGATEX (#59 item 9) tests this. Whether a write to A0000 leaves the L1
intact is the question it must answer.

**Direct3D textures in silicon (owner, 2026-11-05: engine first, software only as the last resort).** The engine
cannot step a source per pixel (M8TEXL is the one open test), so exact textures on any triangle are out of its reach
unless that probe surprises us. Order for a textured triangle:

| # | Path | Case | Silicon |
|---|---|---|---|
| TX1 | Blit from card memory, 1:1 or pre-scaled | upright quads, sprites, walls by column | blit, source compare |
| TX2 | M8TEXL's line or span from a source, if it works | floors, runs at 1:1 | `DP_CONFIG` source (unknown) |
| CT3 | **Cell texturing:** split the triangle into 8x8 screen cells; the CPU finds each cell's texels (perspective at the corners) and picks a tile from a codebook built at texture load, ROM glyphs included, plus two colours; the engine fills or stamps it | any triangle, approximate | fill, mono expansion from card memory |
| TX3 | Pixels from the CPU | exact mode only, or small triangles where it is cheaper | none |

CT3 *estimate*: ~25-30 us a cell, so a 100x100 triangle is ~5 ms against ~30 ms of pixels (3 us each); a 4x4 cell
~17 ms; below about 10 px on a side, sending pixels may be cheaper. Rotated textures lose detail. Probe M8CELL, with
step 4's probes. Lighting and blending stay on the engine too: palette ramps, write mask, arithmetic mixes, DEST_CMP_FN.

**Prior art (web search, 2026-11-05; shallow, most pages read as search snippets):**
- [8088 Corruption file format](https://wiki.multimedia.cx/index.php/8088_Corruption_DAT): CGA 40x25 text with the BIOS 8x8 font as the
  codebook, 2,000 B a frame, 30 fps on a 4.77 MHz 8088. The direct exemplar for CT2. [Trixter's page](http://www.oldskool.org/pc/8088_Corruption), not read.
- [NES CHR](https://nesdev.org/wiki/CHR): the PPU fetches 256-tile tables from cartridge ROM itself. The hardware path this card lacks; CT1's VRAM copy stands in for it.
- [PET streaming, Hackaday](https://hackaday.com/2016/08/08/commodore-pet-mods-at-vcf-west-2016) (bitfixer, 5.8 fps; secondary: his own posts not found),
  [PetPix](https://hackaday.com/2014/04/06/vcf-east-petpix-streaming-images-to-a-commodore-pet) (8x8 blocks matched to PETSCII, on a Pi),
  [BlixTerm](https://hackaday.com/2022/07/01/blixterm-brings-full-speed-youtube-video-to-the-commodore-pet) (~30 fps). All do the matching off the machine, as CT2 would.
- [Escape from PETSCII Castle, write-up](https://setsideb.com/?p=10521): a raycaster in PETSCII on C64 and PET, ~12-15 fps.
- [8088 MPH](https://trixter.oldskool.org/2015/04/07/8088-mph/): CGA text cells retuned for 160x100; no register values read.
- Not found: a font ROM's glyphs used as texture tiles on a blitter or 3D surface, a VGA text-mode raycaster, or a named sizecoding intro using the ROM font. A shallow search: "not found", not "never done".

## Direct3D to 60 fps (owner's goal, 2026-11-05)

Budget 16.6 ms a frame. Now, card, 16 small triangles: ~15 ms at 8 bpp in a 256x128 target, 55-120 ms at 16 bpp full screen, where
a DirectDraw flip alone is ~5 ms. The gains below are *estimates* until measured; measure first (V1), one lever at a time, two runs.

| # | Lever | Why | Exemplar |
|---|---|---|---|
| V1 | Time slots in a frame, and a null HAL (RenderPrimitive returns at once) | Splits D3DIM's own cost from ours; nothing else is known | M8CUBE's profile |
| V2 | Flip's fills and replay directly from M8DD32 | Today GetDC + ExtEscape + a 16-bit thunk; 8 bpp has none | ATI's ring-3 path |
| V3 | A box per triangle (or merged neighbours) replacing one around all | 16 spread triangles make one screen-wide fill a frame | dirty rectangles, G11 |
| V4 | Row fingerprints dropped once on leaving the triangle path, not per frame | Op 5 walks every row of the box each flip | - |
| V5 | Scratch clear by FIFO room, not an idle wait and full register set | Paid per batch and per 8 triangles | O6, O15 |
| V6 | Spans for small triangles, the polygon fill above a size | An 8 px triangle is ~9 x 3 writes against ~44; a 100 px one is the reverse | G7, Amiga |
| V7 | Join coplanar triangles of a colour into one polygon | A quad is two triangles today, one polygon fill if joined (12 vertices allowed) | M8CUBE's faces |
| V8 | Unchanged triangles not drawn: compare the list with the last frame's | Static scene costs nothing; owner's describe-don't-scan, move detection | isometric games, Knight Lore |
| V9 | Background in card memory: erase by copying its patch back, tiles repeated by the extended blit | A scenery costs what black costs | G12, S3, Neo Geo |
| V10 | Two pages on the card (pitch 1024: rows 0 and 512), Flip = CRT_OFFSET, each page's own boxes | No flicker, no copy, 15 us; scratch right of x 640 | T2, B1, consoles |
| V11 | 320x240 by CRTC (P9, P10) offered as a Direct3D mode | A quarter of the pixels at the same line rate; games offer it | O1, 8088 MPH |
| V12 | Asm emitters, values in registers before the first OUT | ~1.2 us a word of L1 refill; A1 | demoscene |
| V13 | DrawOnePrimitive and DrawPrimitives through GetDriverInfo | Skips the runtime's execute-buffer path if V1 shows it matters | velocity9x |
| V14 | Refresh and raster: present in the blank, refresh chosen so its divisor suits the frame rate | 72 Hz halves to 36, so a slow frame costs a whole refresh; lower refresh leaves the engine more VRAM cycles (S9) | CRT racing, O14 |

| V15 | Shape cache: a shape and colour seen before is kept in card memory and placed by keyed copy (owner: identical shapes repeat) | A few writes, no outline, against ~44 writes | sprites, O7, G6 |

Order: V1; V2-V5, which should need no new ideas; then V10 if the owner sees flicker; then V6-V9, V12; V11 if the rest falls short.

**Colour (owner, 2026-11-05, dxdiag stills `photos/flicker1-8.PNG`):** the "flicker" is a face's two triangles in different block colours, stepping as it
turns: first-vertex colour through a 5x9x5 cube, red and blue only 5 levels. The owner prefers the crisp hardware look to the software renderer's dither, and
the driver must still honour the colours a program gives it, so gradients are required, by the engine and undithered:

| # | Step | Cost, *estimate* |
|---|---|---|
| C1 | Average the three vertex colours; a triangle whose vertices agree stays one fill | none |
| C2 | Adaptive palette (F5): the frame's colours and ramps in the DAC, red no longer 5 levels | a few DAC writes when colours change |
| C3 | Claim `COLORGOURAUDRGB`: a triangle whose vertices differ is drawn as bands, rows or row segments, each one colour | ~3x a flat triangle at 8-row bands; only where colours differ |

Band size is the speed-for-smoothness dial; bench it with a gradient test added to M8D3DF.

### Direct3D levers: every one is ruled in or out by a card number (owner, 2026-11-05)

Bench: `M8D3DF` twice after `M8IDLE`; dxdiag's hardware test by the owner's video (20.78 s on `M8D3D2`). Status: IN measured gain, OUT measured none, OPEN not tried.
A cheaper agent can run the prescribed benches and probes; diagnosis and engine sequences stay with the main session.

| # | Technique | Status |
|---|---|---|
| V1 | Stage timing | IN: `M8D3DF` splits scene, clear, DirectX calls, Flip |
| V1b | Null HAL for DirectX's own floor | OPEN; hal_static suggests ~5 ms for 16 triangles |
| V2 | No 16-bit trip on a black-clear flip | IN: with V3 and V4, hal_tiny 8-18 to 28-46 fps |
| V3 | A box per triangle | IN, with V2 |
| V4 | Fingerprints dropped once | IN, with V2 |
| V8a | A frame identical to the card's draws nothing | IN: hal_static 42 to 96-152 fps |
| - | USER call per triangle removed | OUT as a cost: no change; kept out anyway |
| V16 | The black clear's 5-35 ms | CLOSED: other threads, not the clear (V24). 0.8-1.4 ms with the agent stopped |
| V5 | Scratch clear without an idle wait | Built, `M8D3DA`; gain not separable with the agent running |
| V6 | Spans for small triangles | Built, `M8D3DA`, up to 14 rows; `hal_tiny` engine ~11 to ~9 ms, agent running |
| V7 | Coplanar triangles joined into one polygon | OPEN |
| V8b | Per-triangle reuse when only some moved | OPEN |
| V9 | Background and repeated tiles in card memory | OPEN |
| V10 | Two pages, Flip = CRT_OFFSET | IN on the card by the owner's eye, 11-06: black overlaps gone, page wait 3.9-7.6 ms. 640x480 only. A write takes effect once a frame, about the blank's start, no tear seen (M8PAGE). Agent-free fps still to measure: `docs/captures/2026-11-06_v10/README.md` |
| V11 | 320x240 Direct3D mode | OPEN |
| V12 | Asm emitters, values in registers before the first OUT | OPEN |
| V13 | DrawOnePrimitive and DrawPrimitives by GetDriverInfo | OPEN; DirectX calls cost ~0.3 ms a triangle |
| V14 | Refresh rate and presenting in the blank | OPEN |
| V15 | Shape cache: same outline and colour, keyed copy | OPEN |
| V17 | **Shade and hue split:** low 4 bits shade, high 4 hue; a face's colour is one fill with WRT_MASK F0h; light changes are DAC writes (owner: same gradient, different colour). Masked-write cost unknown (WRITE_PER_BIT = 0) | OPEN, probe first |
| V18 | **High-byte-only writes:** one low-byte latch, the odd byte commits; ~5.7 against 7.64 us on the bus, so order writes to share low bytes (owner: words and bytes, as XT-CF; P13) | OPEN, probe first |
| V19 | **Fewer status reads:** a read costs what a write does; a triangle does 3-4; wait on known engine time (XT-CF's measured waits; O15). A full FIFO locks the card, so a table first | Built, `M8D3DA`: room counted, read only when spent. No lock in two runs |
| V20 | Triangle arrays sized sensibly: tens of KB for 512 a buffer | OPEN; RAM, not speed |
| V21 | Engine as decompressor: fills as run length, mono expansion 8:1, copies as a dictionary (owner: compression on the card; G9, O8). The card has no CPU, so LZ4 itself is OUT | OPEN, for textures and video |
| V22 | Patched D3DIM beside a program for 8 bpp | IN: works on the bed and card, stock system DirectX |
| V23 | 8 bpp mode as an option, by display depth | OPEN; a bench decides if it earns its place |
| V24 | Background threads | IN: the V9x agent, not delivered with the driver, takes up to half the CPU; stopped, hal_tiny 70 fps at normal priority. Systray and Internat no measurable cost; `high` not needed |
| V25 | **Fill colour from elsewhere:** DP_CONFIG 6213h (VRAM colour, mask in plane 7), A213h (colour pattern), E213h (source 7 lookup); gradients and textures near flat-triangle cost. Findings: `docs/mach8_vga_half_findings_2026_11_06.md` | **WORKS on the card (M8GRAD, 11-06):** all three fills give per-pixel colour; 1:1 shift only, 7 bits. Next: a triangle, timed. `docs/captures/2026-11-06_probes/README.md` |
| V26 | **Field-confined arithmetic:** masked planes drop out of the ALU, so saturating add or subtract acts on only the shade field in one pass (`references/ati_mach32/guide.txt:11284-90`) | OPEN; unprobed |
| V27 | **BG_COLOR_SRC** (DP_CONFIG bits 8:7), absent from this plan | OPEN; unprobed |
| V28 | **VGA half for Mode X:** latch copies (M8LATCH), page flip (M8VFLIP; "cannot flip" has no source), BDh bit 2 128 KB window (M8W128) | Card, 11-06: flip works, latch copy 0.65 us a pixel, BDh bit 2 separates A000 and B000 (which memory B000 reaches is not shown). A Mode X path is open, not built |
| V29 | **MEM_BNDRY write test on the Mach8** (M8BNDRY). Expected no effect; closes a closure that rested on the Mach32 guide alone | OUT, card 11-06: reads 0000 after writes, no effect |
| C1 | Mean of corner colours | Built on `M8D3D5`; dxdiag video pending |
| C2 | Adaptive palette | OPEN |
| C3 | Gradient bands, `COLORGOURAUDRGB` | OPEN; required (owner) |
| CT3 | Cell texturing from a glyph codebook | OPEN, after dxdiag's needs are known |

## Order of work (2026-11-06, owner: frame rates and system-wide RAM and disk together)

Reason for the order: the card has 0 KB free and 6.7-6.9 MB paged with the agent idle, so RAM and paging touch every
program, while frame-rate levers touch one. Stages 1-3 run alongside stage 4, each measured on the card, two runs.

**Stage 0, agent-free runs (owner), `docs/next_session_2026_11_06.md`.** dxdiag by eye, `NOAGWTCH`, `NOAGDISK`. Then
`NOAGDISK` with the agent, relabelled, for the difference. Decides whether the agent explains the LZ4 mismatch
(11-05 compress ~5,060 us against 10-02's 4,712) and what it costs paging.

**Stage 1, driver RAM.** Census: `docs/mach8_ram_census_2026_11_06.md`.

| # | Lever | Size, from source | Test |
|---|---|---|---|
| R1 | Back buffer: a described frame need not write it, so reserve the address and commit on Lock. Saves nothing for a program that Locks | 614,400 B at 640x480, 960,000 B at 800x600, DirectDraw only | `paged` in NOAG.TXT and PERFLOG's locked count, during and after a DirectDraw program |
| R2 | Fingerprint table: smaller hashes or wider segments, or none (whole sends cost bus time) | 60,000 B; 15,000-30,000 B if kept | bench and `paged` together |
| R3 | The price of the 16 bpp desktop: frame buffer, page-locked, never freed | 1,142,770 B against 253,810 B at 8 bpp | PAGERUN at 16 bpp against 8 bpp |
| R4 | 8 bpp caches in statics, idle at 16 bpp: allocate at Enable. Trace first | ~35 KB, from reading | `paged` |
| R5 | 8 bpp DirectDraw shadow, never freed | 691,200 B | `paged`, if 8 bpp is kept as a mode |
| R6 | Direct3D arrays (V20) | 31,744 B | `paged` |

**Stage 2, paging.**

| # | Lever | Evidence | Test |
|---|---|---|---|
| P-a | `sectors.py` over the card's `WIN386.SWP`: is LZ4 still right? Never commit the file | #59 item 3; deflate 2.92:1 on an old sample, decode untimed | CF in the host reader, no card time |
| P-b | Cheaper compress: `LZ4B12FN` | 3 % on the page-in model, 11-05; re-baseline in stage 0 first | checksum build in the bed, PAGERUN A/B |
| P-c | Stop compressing past 3,070 B | ~12 % of writes go raw after a full compress | with P-b |
| P-d | Zero and repeated-dword pages: no I/O | 1.5 % of writes all zero; `sectors.py` counts repeats | with P-b |
| P-e | Whole pages per request | `SwapComp_Hook` issues one command per page; each pays first DRQ 232-313 us. SWAPCNT sizes it | counters first |
| P-f | VCACHE cap sweep: 256, 512, 1024 KB | 512-1024 measured 10-03, before the 16 bpp desktop. Swap I/O bypasses VCACHE, so the cap trades file cache for working set | SYSTEM.INI, PAGERUN each |
| P-g | Console programs: each costs a DOS VM | M8NOAG, 11-06: `SLEEP 1` took 12 s and paged 188 KB more, agent idle | one run, agent-free, then decide what must not be a console program |

**Stage 3, XT-CF (#59 item 1).** `-TightTaskfile`, `-SkipDrvHd`: bed, then card. Compare by min (11-05 baseline note).

**Stage 4, frame rates.** `M8D3DF` agent-free targets as before: hal_large 39.9, hal_many 22.5 fps must reach 60.
Flip's engine is ~0.5 ms a triangle, 60 % of a frame. Order: V19, V5, V6, V18 (probe), V15; V10 after M8PAGE; then C2,
C3 for the blocks; V7, V8b, V9, V11-V14 by what the profile shows.

**Stage 5, textures.** Probes M8TEXL, M8GLYPH, M8TEXT, M8CELL, VGATEX (#59 item 9), none built. Publish a texture format
when dxdiag needs it. CT1 before CT3; TX1 first, since it is plain blits.

**What each driver can lend the other:**

| Learned on | Lesson | Applies to | Row |
|---|---|---|---|
| Mach8 | Values in registers before the first OUT | XT-CF taskfile | stage 3 |
| Mach8 | Skip a repeated write where nothing else changes the register | XT-CF DRV/HEAD | stage 3 |
| Mach8 | Describe, do not scan or send | SWAPCOMP zero and repeated pages | P-d |
| XT-CF | Wait the measured time, then look | Mach8 status reads | V19, O15 |
| XT-CF | One latch, the odd byte commits | Mach8 words sharing a low byte | V18, P13 |
| SWAPCOMP | Divide by the bytes; decode cheaply on the far side | Mach8 engine as decompressor | V21 |
| T130B audit | Pace the spin | Mach8 vblank wait (done), T130.MPD block-ready spin | #41 |

## What a frame's time is made of (card, M8SHAPE, 2026-11-06 evening)

Results: `docs/captures/2026-11-07_shape/README.md`. Budget 13.85 ms at 72 Hz, 16.67 ms at 60 Hz.

- **Engine commands set the cost, ~40-60 us each however fast they are sent.** Asm queue no faster than C (V12, A1 out for triangles); high-byte latch (V18) ~2 us a triangle, out.
- **A mask or polygon fill costs ~69 ns a pixel of its box; a plain fill ~13-20 ns.** Whole-page fill 6.3 ms with the CPU held 190 us: one command runs alone.
- Pixel clock has no effect on engine speed (S9 out). 60 Hz costs fps until a frame fits in 16.67 ms.

| Lever | Result | Status |
|---|---|---|
| V15 masks: a repeated shape as one mono copy from a plane of a strip | `hal_many` 21.6 to 33-35 fps, `hal_small` 55 to 68-69 | IN, `7a92f99` |
| **Core:** a mask's inscribed rectangle as a plain fill, only the boxes around it from the mask (owner: bring it down in 3D by thinking outside the box) | 100 px 1,043 to 450 us in the probe; `hal_large` 33.6 to 51 fps, `hal_grad` 51 | IN for bench shape; other shapes being checked |
| Batched edges, unchanging writes once | 0.8x at 8-32 px, 0.9x at 100 | Next |
| Same-row scratch, row bands, outline-free fill | 8 % at 100 px / nil / wrong pixels on slopes | OUT |
| E213h 8-shade lookup | 1.7x flat against the stored ramp's 1.09-1.35x | OUT for gradients |

Open ideas, not yet measured:

| # | Idea | Why it might pay | Probe |
|---|---|---|---|
| Y1 | **Start frame 2 before frame 1 lands (owner).** Erase or draw the hidden page in the program's own CPU time | **Measured, card, `M8D3DS`: little to gain with two pages.** The hidden page is on screen until the flip lands: landed at the program's clear in 2-27 % of frames, at RenderPrimitive in 2-68 %. DirectX does most of its work before RenderPrimitive, so the window left is our own 1-3 ms. Best case ~2 ms of 32 on `hal_many`, ~0.5 ms on `hal_large`. Waiting at the clear loses the CPU's overlap with the wait, which it already has. A third page frees the erase, but costs the room stamps and masks use | Not built. Reopen only with a third page that keeps stamps |
| Y2 | Third page, pitch 640 | No wait on a late frame; needs low and high CRT_OFFSET writes, 198 rows of scratch | M8PAGE variant |
| Y3 | Refresh as a free parameter (the screen is an LCD, not a CRT): 60, 56 or 72 Hz chosen to fit the frame | A frame of 15-17 ms shows at its own rate | Register row, FB bench |
| Y4 | Padded mask, BKGD black: one expand erases and draws (subagent, inference) | Saves ~74 us a small triangle | M8SHAPE arm |
| Y5 | Field rendering: alternate rows each frame on a doubled pitch | Half the pixel time; combing on motion, owner's eye | Probe with GE_PITCH |
| Y6 | Joined quads (V7) and axis-aligned pairs as plain fills | Half the fill on cube faces; invisible in M8D3DF | `hal_quad` test |
| Y7 | Unchanged triangles not drawn (V8b) | Static scenery | dxdiag and a game |
| Y8 | 320x240 and 400x300 by CRTC (V11) | 2-3x on large shapes; games pick the mode | P9, P10 |

| Y9 | **Paint once, stamp many (owner):** a gradient triangle drawn once in spare card memory, then a colour-keyed copy, one command | 100 px ~2.5 to ~0.7 ms *estimate*, when shape and colours both repeat; ~12 stamps of 100 px fit | Arm against bands |
| Y10 | **What is true everywhere (owner: pi, phi, fractals).** A straight colour ramp is the same at every place, so one stored ramp serves many triangles at an offset | The 6213h fill: 1.09x flat at 100 px, against bands at ~2.5 ms. Needs ramps by direction and rate, and a palette in shades; suits lit surfaces of one hue | M8SHAPE arm with a field per direction |
| Y11 | Mirrored masks (owner's repeats): one mask for a shape and its mirror by a reversed copy, if the engine allows | More mask hits on cube faces | One mirrored copy, pixels read back |
| Y12 | Golden-ratio band shift (owner's phi): band edges moved by a low-discrepancy step each frame, so steps average out over time | More apparent shades for no commands. Owner likes the crisp look, so by eye | Bench option |
| Y13 | Background over foreground (owner): BKGD_MIX on the mask copy, so one command writes the shape and the box around it. Black gives erase and draw together (Y4); a stored picture restores scenery (G12) | Small triangles; scenery for what black costs | M8SHAPE arm |
| Y14 | **Work the frame out before the first OUT.** Any port access empties the L1, so C between OUTs runs from RAM: lookups ~11 us warm, ~150 us cold | **IN for mask and stamp copies, `M8D3DV`:** `hal_many` 31-35 to 41 fps. Flat triangles, bands, mask builds and erase boxes still run cold: next. All replay writes now queue, boxes of 1,500 px sent at once so C overlaps the engine (`M8D3DX`). GDI needs no build: it already queues fills and sends pixels by `rep outsw` (capture README) | Done |
| Y15 | Tell programs: D3DDP_DONOTCLIP and DONOTUPDATEEXTENTS save ~5.4 ms at 64 triangles (card). Ours, in M8CUBE and demos, can use them; a shipped game's flags are its own | Our own programs | Done in `M8D3DF` as `hal_many_c`, `hal_many_e` |
| Y16 | **Line envelope, string art (owner).** Two axes joined by lines leave a curve; the arithmetic is a quadratic Bezier, additions only. On the card 32 tangents cost the same as 32 chords (1,416 us at r 30, 60, 120), a line being ~44 us up to at least 240 px; the arithmetic is still additions only. Its value is the cheap points, and chords + one polygon fill as the 2D engine's own curve | Own programs and demos. GDI hands us curves already flattened | `M8CURVE`: card figures in `docs/captures/2026-11-07_curves/README.md` |
| Y17 | **Pythagoras (owner).** No square root anywhere in the driver or tools. Circle rows walk x^2 + y^2 = r^2 by differences, no root. GDI already merges equal rows, so no lever there. Left: the inscribed square, side r x 1.41, as a core rectangle for masked circles; alpha-max beta-min for a length, if a gradient ever needs one | Small; no call for it now | Only with a masked-circle user |
| Y18 | **What the curve test found:** at 16 bpp GDI's outline was ~290 one-pixel sends. Spans in exact colours as engine rows, short segments gathered | Ellipse, polygon, rounded shapes on the desktop | `M8D3DY`; capture `2026-11-07_curves`; card pending |
| Y19 | Stored dither tile: one 4x4-phase bar off-screen per colour, so a span in a dithered colour is one copy, not its pixels | Fills in non-system colours | After Y18 on the card |

Noted, not useful: Mandelbrot itself (detail costs computation, nothing to reuse). Already in use: phi in SWAPCOMP's hash (`SWAPCOMP.ASM:392`, 2^32 / phi); self-similar triangle as the core at one level (deeper loses to command cost); sine tables in M8CUBE.

**Reuse to check after each win (owner):** masks and core rectangles for M8CUBE's polygon escape (`poly3d.c`), DirectDraw sprites (colour-key copies are mask-rate fills), and GDI's mono expansion from card memory.

## Earlier order of work (2026-11-05; superseded above, kept for its probe lists)

Mach8 (M = needs the card, B = bed, C = no machine):

1. **D1 gate (B):** DirectX 7 lists our 8 bpp HAL but strips 8 bpp at `CreateDevice` (proven, `next_session_2026_11_05.md`);
   with that one instruction patched out the device creates and a triangle reaches RenderPrimitive. Route to agree:
   HAL offered at 16 bpp over the engine (recommended), 8 bpp by patched D3DIM for scaffolding.
2. **D2 (B, then M):** triangles to the polygon fill; hidden-surface sort (D3).
3. **First 3D bench (M):** same scene through DX7 RGB emulation and our HAL, in fps (#58). Worth having only if ours wins.
4. **Probes, one sitting (B for pixels, M for time):** M8TEXT (CT2: frame rate, font rewrite, 80x50), M8GLYPH
   (CT1: stamp cost), M8COLS, M8TEXL, VGATEX, pattern and write-mask lighting (#59 item 9). These decide the
   next two lines. No driver change.
5. **B1** 8 bpp full-screen flip, measure `v`; **F5** adaptive palette; profile the ~0.8 ms a face (`-Prof -os`), then A1 asm.
6. **Video (M, owner's clip):** AVIINFO and the two re-encodes (#59 item 8), no build; then a text-mode player
   (CT2) or a Video 1 player, whichever the numbers pick.
7. **Textured demo:** raycaster by M8COLS with CT1's atlas, if step 4 supports it.
8. G11, G12, D4, then F6, sprite cache, move detection (owner's levers); Phase 6 alongside.

Disk and swap (separate from the Mach8; none of it needs the driver):

1. **Card, no build:** LZ4B variants beside `SWAPPAGE.BIN`, XTSECT baseline x3, IDLECOST `base` x3 (#59 items 1-3).
2. **PC, no machine:** `sectors.py` over the card's `WIN386.SWP`; never commit that file.
3. **DDK builds, bed first:** XT-CF `-TightTaskfile`, `-SkipDrvHd`; then SWAPCOMP's winning hash, the 3,070-byte
   compress abort and zero pages. Expected 0.5-1.5 % on one-sector reads; the owner wants even a fraction.
4. F0000 cacheable (item 5), DINPROBE (item 7). BlueSCSI (item 6) stays parked until after the Mach8.
