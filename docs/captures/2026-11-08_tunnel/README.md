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
