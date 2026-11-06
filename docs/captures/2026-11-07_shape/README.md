# What a triangle costs on the card, 2026-11-06 evening

`tools/m8probe/M8SHAPE`, card, agent running, idle first, fastest of 3 x 160 triangles, right triangles as M8D3DF. Log `M8SHAPE.TXT`, labels `h1` (72 Hz), `e1`, `n1`, `q1`, `l1` (60 Hz). Bench log `M8D3DF.TXT`, labels `h1hi`, `e1hi`. Agent-free figures: `docs/next_session_2026_11_07.md`.

| us a triangle | 8 px | 32 px | 100 px | Pixels against flat |
|---|---|---|---|---|
| flat, the driver's way | 411 | 422 | 1,043 | reference |
| span, driver's rows | 306 | - | - | same |
| reuse, one shape's edges kept | 233 | 243 | 844 | same |
| rows, no outline | 198 | 208 | 799 | wrong: right edge missing |
| **expand, whole shape in a plane, 8 writes** | **66** | **91** | **722** | **same** |
| ramp, stored colours (6213h) | 554 | 558 | 1,134 | centres right |
| lookup, 8 stored shades (E213h) | 717 | 752 | 1,783 | centres right once all 16 pattern words are loaded |
| erase, box filled | 74 | 75 | 190 | - |
| near, scratch on the triangle's rows | 409 | 424 | 963 | same |
| bands, near with 3 row bands | 671 | 685 | 951 | same |
| queue, flat's writes by an asm loop | 444 | 472 | 969 | same |
| **batch, 8 triangles' edges first, unchanging writes once** | **320-333** | **329-341** | **920-943** | **same** |
| qerase, 5 writes for erase's 10 | 70 | 70 | 190 | - |
| noline, right edges shifted, end rows only | 310 | 314 | 896 | wrong: 69 of ~450 edge pixels on shape a at 100 px |

## Readings

- **A blit-type fill costs ~69 ns a pixel of its box, a plain fill ~12.6 ns.** From the slopes of expand and erase between 32 and 100 px. Earlier figure of 35 ns was at 8 bpp in another mode and is not comparable.
- **Engine commands are the cost, not our writes.** An erase with half the writes takes the same time; an asm queue is no faster than C. Each command costs ~40-60 us whatever is sent. Consistent with `m8stream`, 11-03: **asm emitters (V12, A1) are not a lever for triangles.**
- **The probe predicts the driver:** `hal_large` 16 x (1,043 + 190) = 19.7 ms against ~20.5 ms of engine time.
- `hal_many`: 64 x (306 + 74) = 24.3 ms of 33 ms engine: about 8 ms a frame unaccounted for.
- Lookup costs 1.7x flat against ramp's 1.09-1.35x: stored ramps for gradients.
- VRAM page distance is not the fill's cost: near gains 8 % at 100 px, 0 % below.

## 60 Hz (`M8D3DE`, CLOCK_SEL 1850h, 59.94 Hz by calculation)

| fps, high priority, agent on | hal_tiny | hal_small | hal_large | hal_turn100 | hal_many | hal_tiny last |
|---|---|---|---|---|---|---|
| `h1hi`, 72 Hz | 60.1 | 55.2 | 33.6 | 35.7 | 21.6 | 67.4 |
| `e1hi`, 60 Hz | 48.6 | 52.0 | 27.2 | 28.4 | 22.3 | 56.4 |

Engine unchanged by the pixel clock: expand 722.6, erase 190.0 at 100 px (S9 closed). 60 Hz costs fps until a frame is under 16.67 ms. Reverted in the source. To rebuild: `m8hw.c` 640x480 row `{ 0x63, 0x4F, 0x52, 0x2C, 0x0418, 0x03BF, 0x03D4, 0x22 }, 0x1850`, and `M8DD_PAGE_TICKS` 21000 in `m8dd32.c`. Monitor lock not seen by eye.

`h1hi` at high priority with the agent on runs close to the owner's agent-free `v10c`/`v10d`: `hal_large` 33.6 against 36.0, `hal_tiny` last 67.4 against 71.5-71.8.

`FB16CHK` on `M8D3DH`, agent on, no paging run: 0 pixels differ and 0 sends entered during a send, before and after the bench. Says nothing yet about the ghosts.

## Gradients and stamps (`M8D3DM` to `M8D3DP`, high priority, agent on)

| | us a triangle at 8 / 32 / 100 px | Pixels |
|---|---|---|
| stamp, a drawn triangle copied with black keyed out | 59 / 91 / 712 | equal to flat, all 8 bits of colour |

| `hal_grad`, real corner colours | fps |
|---|---|
| row bands, floating point (`M8D3DM`) | 19.7 |
| row bands, integer, merged by palette entry (`M8D3DN`) | 22.2 |
| stamps, 16 built, 1,496 used in 3 s (`M8D3DP`) | 33.9 |

- `hal_large` averages 47-51 fps with no steady 36: frames alternate between one and two refreshes, about 20 ms on average. So every millisecond shows, and a flip does not simply halve the rate.
- Pictures `card_*.png`: `M8D3DF ... grab` reads both pages from the card (the bench's lit count is from RAM). Bands, core rectangles at 100 px and turning shapes look whole; both pages agree.
- Bands are by rows only: a purely horizontal change of colour is drawn as the row's middle colour. Colours are the fixed 5 x 9 x 5 cube, so steps show. Owner's eye still to come.
- Restart hangs: `M8D3DN` boot 244 did not come back; black screen and egg timer after `M8D3DK`'s second high-priority bench, owner restarted (boot 241). All with the agent on.
