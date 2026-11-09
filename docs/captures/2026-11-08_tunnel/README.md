# SDK tunnel on the card, 2026-11-08

`reference_host_gpu.png`: the unmodified DX 6.1 `tunneldp.exe` windowed on the host GPU, the look its author intended: grey-blue riveted metal, yellow and black hazard stripes, radiation symbols, a point light at the camera fading with distance. `tex2.bmp` is 256 x 256, 16 colours; every ring is 24 sides, each side one fixed slice of the texture.

Card, tunnel at normal priority, agent on:

| Build | Colours a frame | Writes | Ours | Engine | Frame | Owner's eye |
|---|---|---|---|---|---|---|
| `M8D3EL` joined, banded fills | 7 | 4,470 | 83 ms | 166 ms | 355 ms | no holes, no flicker, flat grey blocks |
| `M8D3EM` texture palette (16 colours x 16 light levels) | 38-41 | 8,600-10,000 | 144-162 ms | 232-265 ms | 470-548 ms | "colours infinitely better", depth from the light steps; still big flat triangles, yellow triangular and random where the original has rectangles |

Cause of the triangles: each triangle takes the texture colour it shows most, so a quad's two halves can differ. Next: the texture itself, stored once in card memory, lit by the palette (D5).

## M8D3EO (2 x 2 tessellation), evening

Card, normal priority, agent on, `tunnel_card.ps1 -Settle 90 -Gap 60`, label `eo1`, boot 288.

| Per flip | M8D3EO | M8D3EN |
|---|---|---|
| Ours | 643 ms | 90 ms |
| Engine | 338 ms | |
| Whole frame | 1,673 ms | 420 ms |
| Writes | 11,490 | 8,209 |
| Colours | 32.6 | 7.7 |

flipwhy: slow on 40 of 45 flips (boxes 37). Sizes per flip: 1-4 27.4, 5-16 192.2, 17-64 278.4, 65+ 19.6.

Owner's eye: started on the card, then after a few frames looked like software. The black/yellow stripe rectangles are back to triangles and look wrong.

Reading: 4x pieces overflow the triangle list (`M8DD_TRIS_MAX` 2048), so flips take the slow RAM path. Per-cell float maths on the 387 is the 7x in ours. One texel per cell turns diagonal stripe edges into small triangles.

Negative result: blanket tessellation is out. Patch kept at `drivers/mach8_display/wip/2026-11-08/tess_m8d3eo.patch`, not committed.

## M8D3EP (detail by depth layer), evening

Card boot 289, normal priority, agent on, label `ep1`, `tunnel_card.ps1 -Settle 90 -Gap 60`.

- Per flip ours 128.1 ms, engine 227 ms; flips 140 in the ~60 s window (about 450 ms a frame).
- Writes 8,065, pieces 174; sizes 1-4 15.7 / 5-16 154.7 / 17-64 137.8 / 65+ 37.6.
- Joined same colour 191.2, any colour 322.2, colours 37.7. Flipwhy: slow 1 (first frame), boxes 0.
- Owner: "the run was good, still stepping".

Reading: far merge barely cuts writes; the cost is fill pixels of near large triangles, so Z6 (low-res) and light in the fill (Z2/Z3) are the speed levers. 3x3 float sampling put ours up ~38 ms over M8D3EN.

Hang at exit: the 5160 stopped answering after Alt+F4 closed the tunnel (census already taken); owner restarted it, boot 290. Open: close the tunnel on ATI's driver a few times to split ours (DAC restore, page state) from DirectX.

### tex2.bmp shapes

256 x 256, 4 bpp, 14 colours used; ~95 % grey stonework as dithered noise (2,454 single-colour regions). With the 9 greys merged as stone: 63 regions, 20 over 32 px: 7 stone panels between black seam lines, a black/yellow hazard stripe band (rows 173-195), a yellow panel (x 193-250, y 40-97) with black marks (the owner's "clock"), 43 specks under 4 px.

Plan (Z13, owner 11-08): near walls drawn as these ~20 polygons mapped through the wall's corners, stone as one grey plus the Z12 aperiodic pattern.

## M8LORES: 320x240 by CRTC (Z6), evening

`tools/m8probe/M8LORES.C`: DISP_CNTL bit 3 DOUBLE_SCAN, CLOCK_SEL bit 6 divide-by-2, horizontal and
vertical CRTC values halved (vertical skip-2 encoded, guide 8-10), shadow set 1.

Card, two runs (21:4x, 21:53): every stage set, and the desktop came back.

Owner's photos (AOC LCD), from a video:

- stage 1, 640x480: the 320x240 test area in the top-left quarter.
- stage 2, 640x240: stretched to full height, half width.
- stage 3, 320x240: fills the screen, 8 x 6 grid, one-pixel stripes shown 2 pixels wide and tall, even.

Frame time readings were noisy (640x480 read 65.5 Hz, then 51.6 Hz), so not used.

Result: Z6 works on the card. Next: a 320x240 DirectDraw mode in the driver.


## M8D3EQ/ER: 320x240 trial (Z6)

Card. The driver halves the coordinates and sets M8LORES's 320x240 timing at the first frame.

- EQ, normal priority: frame 387 ms vs EP ~450; engine 175 vs 227 ms; ours 116 vs 128 ms; writes 6,200 vs 8,065.
- Owner: filled the screen, a bit quicker, "detail wise not much discernable difference".
- Reading: fill pixels were not the main cost.

## FIFO and Flip phases (ER, ES, ET)

- ER, high priority: frame 360 ms, engine 174. Bus writes ~46 ms (6,067 x ~7.6 us), FIFO-full reads 1,042 a frame (~4 ms), room checks 714 (~3 ms). The card starves.
- ES (queue 8192): no change (173.5 ms). Cache flushes from mid-frame sends are ruled out.
- ET, phases per frame (ms): erase 2.1, edge build + sort 53.4, outlines 35.1, fills 36.2, redraw + clear 29.2, singles 0.1, final engine wait 2.8.

## M8D3EU: edge cancel by hash

- Edge phase 13.2 ms; card share of a frame 138.4 ms.
- Clean A/B, boots 297/298, same drivers: frame 327 / 326 ms, ~3.1 fps at high priority, 320x240 trial.

## Hangs, 11-08 night

- Boot 296 hung on the desktop (wallpaper, egg timer, startup sound looping) after the install, before any 3D ran.
- Boot 299 stopped answering during or after the tunnel; owner powered off.
- Earlier: hang after Alt+F4 at boot 289.
- Possibly #55's open hang. The driver's static data grew ~80 KB in ES/EU (queue 8192, hash tables): shrink it before blaming anything else.

## SWAPZF.VXD and XTBOTH.MPD

Kept on the card by owner's decision; byte-exact read-back, no measured XTSECT gain; tunnel run on them lost to the boot 299 hang (no figure); gain to be measured by PAGETIME soak; dist/ waits for that measurement.

## M8D3EV: static data shrunk, 2026-11-09

Committed hash pass, queue 1024, no second edge array (outer edges reordered in place): 30 KB less static data than EU. No low-res trial, no phase timers; 640x480.

- Boot 302, high priority, two runs: frame 458 / 407 ms; ours 132.6 / 128.9 ms; engine 202.8 / 196 ms; writes 8,523 / 8,215 a frame; ~180 pieces.
- First run after boot slower (start-up still settling). No hang across two runs and two closes.
- Reading: same as EU once its 320x240 1.2x is taken out. Writes per piece ~45 x ~7.6 us = ~62 ms of bus a frame.

## M8D3EW: colour per texture patch kept, 2026-11-09

EV plus a 1,024-slot cache of the sampled colour, keyed by texture and rectangle in 64ths (FastDoom's cached flat colour).

- Boot 303, high priority, two runs: frame 425 / 360 ms; ours 101.6 / 99.8 ms (EV 132.6 / 128.9); engine 200 / 199 ms; writes ~8,360.
- Our time down ~29 ms a frame (22 %); look unchanged by design (same colour for the same patch). Owner's eye not yet asked.
- Still ~100 ms of ours a frame with the card idle: next to find where.

## M8D3EX, EY, EZ, 2026-11-09

- EX: float coordinates, bounds tests and texture keys on the bits, no 387. Ours 100 -> 80 ms (two runs, boot 304). Z19 register shadow in the same build: 66 of ~8,200 writes a frame dropped; negative, removed in EY.
- EY: Z19 out; far triangles take their own cached patch colour (owner, 11-09: stripes appeared only as they came near). Ours 81 ms, flip 199 ms (one clean run, boot 305; the second ran beside a stray copy of the tunnel and is void: two programs' textures pushed the palette past 16 colours, colours lost).
- Owner's eye on EX/EY: near wall panels hold one colour while a stripe slides through their texture, so a yellow square lingers and crawls; same for black outlines top left. Cause: one colour per large near piece. Fix: Z13, split only large near pieces.
- EZ = EY + phase timers. Boot 306, one run, 251 flips, per frame: erase+check 7.4, edge build 15.4, outlines 55.4, fills 99.4, redraw+clear 37.5, singles 3.5, final send 0.7, engine wait 3.3 ms. Sending (q_flush) 124 ms; 8,490 writes (~64 ms of bus); 2,504 FIFO-full reads and 999 room checks a frame.
- Reading: during fills the card is busy and the CPU spins (~60 ms); during edges, outlines and redraw the CPU computes and the card idles (~100 ms). Z17 (compute the next pass while the card fills) is the lever.
- Hang at the tunnel's exit (Alt+F4) on boot 306, after both census readings were written; owner restarted. Same as 11-08's exit hangs (#55).

## M8D3FA, FB, FC, 2026-11-09 (one run each, phase timers on)

| Build | Change | Ours | Flip | Fills | Writes | FIFO-full reads |
|---|---|---|---|---|---|---|
| FA | Z17: queue 4,096, engine topped up without waiting | 81.9 | 247.9 | 131.2 | 8,438 | 1,441 |
| FB | FA + 320x240 (Z6) | 80.4 | 185.1 | 76.5 | 7,045 | 116 |
| FC | FB + on-screen outlines chained (2 writes an edge, not 5) | 81.0 | 173.1 | 74.6 | 5,665 | 117 |

- FA alone neutral: outline sends moved out of the way, the time reappeared in fills. Inside the flip the card was the limit at 640x480.
- FB: fewer pixels took the card off the critical path again (full reads 1,441 -> 116). Owner: "still looked as good but faster"; 320x240 is the target for this kind of demo; the resolution should be a choice by program type.
- Owner's eye on FB: near the end, the left wall's black stripe seemed to draw through the right side (looked transparent). Suspect: halving rounds a thin back-facing sliver into a front-facing one, and joined fills go in colour order. To check.
- Ours + flip: EV 325 ms -> FC 254 ms a frame, before the program's own time.
- FD: FC + scratch outlines chained where each next edge continues downward: writes 5,665 -> 5,373, outlines 31.5 -> 35.4 ms, flip 173 -> 177 ms. Negative (runs are short, the search costs more than it saves); reverted, card left on FD until the next install.
- FE: FC + facing from the corners at 1/8 pixel (a back face rounded round at half size showed through the wall: owner's see-through stripe). Ours 81 -> 88.8 ms, flip 178 ms. Owner's eye on the stripe: pending.
- FF: FE + fill bands 64 rows, join threshold 3,200 px (were 32 / 1,600, set when the card was the limit). Fills 77.6 -> 64.4 ms, flip 178 -> 163.7 ms, full reads 117 -> 277.
- FG: bands 128 / 4,800 px: flip 162.6 ms, full reads 804: the card is the limit again; no gain. Kept FF's 64 / 3,200.
- FH: FF + clip only the sides a triangle crosses (tested on the float bits): ours 88 -> 74 ms; flip 161 ms.
- FI: FH + CANRENDERAFTERFLIP set, diagnostic triangle census off the flip path: flip 161 -> 151 ms (the untimed part of the flip is now ~4 ms); 1,418 triangles a second (EV 1,063).
- Ours + flip: EV 325 ms -> FI 226 ms.
- FJ: repeat of FI (an edit missed its file): flip 151.9 ms, confirms FI.
- FK: FI + engine top-up every 256 held writes, not 24 (each status read empties the cache): flip 151 -> 146 ms.
- FL: FK + large textured pieces (> 48 px drawn) split in four, twice: ours 75 -> 144, flip 146 -> 231 ms. Too costly.
- FM: split once, > 80 px only: ours 87, flip 159 ms (+25 ms a frame over FK). Owner's eye pending: are the lingering squares (one colour per large near piece) better? Keep if clearly better; otherwise remove and sample a smaller texture area for big pieces instead.
- Z25 as first planned does not work: the FIFO holds 16 entries, no DMA or interrupt, and nothing of ours runs while the program and D3DIM do (~60 ms), so nothing feeds the card then. Feeding from every recorded triangle would empty the cache per triangle. The way is a timer-driven VxD feeder (plan Z31).

## 2026-11-09 evening: FM eye run, FN, FO-FR

- FM, boot 320, owner watching: ours 90.4, flip 163.5 ms, whole frame 366 ms, 6,448 writes. Fills phase 86.4 ms of which blocking sends 78.5: the outline writes queued earlier all go out at a pass's first fill. Owner: lingering yellow squares not seen this run; **split kept**. Still seen: occasional stray triangles at the right and bottom edges when the far yellow markers come close (open).
- FN = FM + queue 8,192 + a counter of sends forced by a full queue: forced 0 a flip; ours 93.9, flip 165.9 ms. **Item 4 (queue 8,192) ruled out**: every joined fill sends the queue, so 4,096 never fills.
- FO = FN + per-phase sending time (census `phasesend`, `phases.ps1` prints sending and other per phase). FP = FO + on-screen outline chaining by an endpoint hash, not a scan of the colour's later edges (same edge order chosen, so same pixels). FQ = FP + whole frame worked out before any of it is sent (queue 16,384): the build runs with the cache intact. FR = FQ + `C:\M8D3D.INI [diag] mode=` (1 drop every triangle: the program's and DirectX's own time, plan B10; 2 drop after facing; 3 all but the record).
- FQ (boot 322, two runs): flip 158.6 / 159.4 ms. Build CPU 62 ms (edge 17, outlines 15, fills 14, redraw 13: the endpoint-hash chaining took redraw 33 -> 13), send 90 ms with 4,223 FIFO-full reads: card-limited, ~48 ms bus and ~42 ms engine still drawing.
- FS, whole frame fed by M8FEED.VXD at 1 ms: no hang; ours 92 -> 164, flip 159 -> 135, whole frame 358 -> 471. One FIFO's worth a tick, ~470 ticks a frame, and the 1 ms timer's port I/O empties the cache for everything. Ruled out as built; first load failed (DEF needs `VXD M8FEED DYNAMIC`).
- FT (boot 324): aperiodic light dither by piece (R2 threshold on the texture patch); +115 writes, flip unchanged (ft1 160.2 / ft2 dither off 163.2). Whole frame 425 on ft1 was the first run after boot.
- M8LATCH (item 5, plan A8/V18/P13): a lone high byte uses the latch once; after a commit the next lone high byte takes an older low byte (7Fh, F3h, 23h seen). Word 5.99 us, lone high byte 4.83 us. No gain: ruled out.
- FU (boot 325): second split above 160 px, far light rounded not floored, 8,192 fallback queue: whole frame 341 / 378 ms.
- FV/FW tail feed (M8FEED takes the frame's last N writes; fast timer only while feeding): off 170/394, tail 600 154/380, 1,500 146/407, 3,000 137/344 (FV); FW 3,000 127.5/309, 5,000 112/317, whole frame 98/359 (flip/whole frame, ms). Feed finishes before the next Flip (0 left).
- FW nooutline=1 (look wrong, timing only): flip ~165 -> 105.7, writes 6,532 -> 4,534, FIFO-full reads 0: on-screen outlines kept the card the limit.
- FX (boot 328): right-bounding edges a pixel right in the scratch (fill type A leaves the right edge out, guide 6943-6945), fills from the top row, no on-screen outlines. With tail 3,000: flip 88.7, whole frame 300. Tail sweep with it: none 107.8/317, 2,000 104.9/300, 3,000 93.3/314, **4,000 80.1/292**, 5,000 75.3/340. Owner: speed felt up, no cracks reported; yellow squares still appear but shorter; see-through on bends (far wall over near wall).
- See-through cause: joined patches paint in colour order, and the overlap guard (area <= one 640x480 screen) is four screens at 320x240, where corners are halved. FY: runs stop where a triangle covers 8x8 cells another of the run fully covers; area cap in the target's size.
- Hang, boot 330, run fznolap (FZ, nooverlap=1, feed on, tail 4,000): census fznolap-a written, then the card froze within the next 30 s; owner restarted (boot 331). The run before it had been left going ~10 min by a stopped batch, then closed by Alt+F4. First hang since M8FEED went in; same path ran clean on FY. If it recurs: same run with [feed] on=0.
- FY (boot 329): diag split of our record path: mode 1 (triangles dropped) ours 3.7, whole frame 96 ms = program + DirectX floor; mode 2 (after facing) ours 41.8; mode 3 (all but the record) 81.3. Band 128 / fillpx 1,600 / 6,400: within 3 ms, thresholds ruled out. Overlap test as first written: erase+check 2.4 -> 54 ms.
- FZ (boot 330): 16-unit cells, worst corner per edge by adds: overlap test ~3 ms (flip 84.2 vs ~81 off).
- GA (boot 332): texture patches sampled in 16.16 integers from the float bits: ours 92.0 (387, mode 7) -> 76.1. Overlap off run: whole frame 273 ms.
- GB (boot 333): clipping in integers (1/16 px, 16.16 texture, 64-bit products): mode 5 27.0 -> 21.1, ours 75.3 (387 clip, mode 8) -> 69.6; second run whole frame 280 ms, flip 86.4.
- GC/GD (boot 335): per-process reset of shared DLL state; 8-unit overlap cells; accent colour rule; patch cache cleared per context. Not timed by script: first run is the owner's agent-off run.
