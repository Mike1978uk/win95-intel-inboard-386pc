# SDK tunnel on the card, 2026-11-08

`reference_host_gpu.png`: the unmodified DX 6.1 `tunneldp.exe` windowed on the host GPU, the look its author intended: grey-blue riveted metal, yellow and black hazard stripes, radiation symbols, a point light at the camera fading with distance. `tex2.bmp` is 256 x 256, 16 colours; every ring is 24 sides, each side one fixed slice of the texture.

Card, tunnel at normal priority, agent on:

| Build | Colours a frame | Writes | Ours | Engine | Frame | Owner's eye |
|---|---|---|---|---|---|---|
| `M8D3EL` joined, banded fills | 7 | 4,470 | 83 ms | 166 ms | 355 ms | no holes, no flicker, flat grey blocks |
| `M8D3EM` texture palette (16 colours x 16 light levels) | 38-41 | 8,600-10,000 | 144-162 ms | 232-265 ms | 470-548 ms | "colours infinitely better", depth from the light steps; still big flat triangles, yellow triangular and random where the original has rectangles |

Cause of the triangles: each triangle takes the texture colour it shows most, so a quad's two halves can differ. Next: the texture itself, stored once in card memory, lit by the palette (D5).
