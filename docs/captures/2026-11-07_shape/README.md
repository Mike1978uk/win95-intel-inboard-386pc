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

## Start frame 2 early (Y1), `M8D3DS`, measuring build, not kept

| | Clear to RenderPrimitive, ms | Ours in it, ms | Flip landed at clear | at RenderPrimitive | of frames |
|---|---|---|---|---|---|
| hal_tiny | 2.82 | 1.12 | 4 | 5 | 187 |
| hal_large | 3.47 | 4.02 (one run, others ~1.1) | 22 | 60 | 107 |
| hal_many | 6.52 | 3.06 | 25 | 63 | 93 |
| hal_turn100 | 2.65 | 1.46 | 8 | 23 | 100 |
| hal_grad | 2.65 | 1.38 | 10 | 42 | 103 |

The CPU already overlaps the wait for the flip to land, so a frame costs about the larger of the CPU's time and the wait, then the engine. `hal_many`'s `d3d` is 9.3 ms and our share ~3: about 6.5 ms is the runtime's, before it calls us.

## What DirectX itself costs, 64 triangles, card, one run (`c1`)

| | fps | ms a frame | `d3d` ms | Flip's engine ms, wait included |
|---|---|---|---|---|
| `hal_many_a`, as `hal_many` | 31.9 | 31.40 | 9.71 | 17.73 |
| `hal_many_c`, D3DDP_DONOTCLIP | 33.8 | 29.56 | 6.41 | 19.40 |
| `hal_many_e`, and D3DDP_DONOTUPDATEEXTENTS | 34.5 | 28.95 | 4.32 | 21.12 |

Clip checks ~3.3 ms, extents ~2.1 ms, in the runtime, per program's flags: ~50 and ~33 us a triangle. ~3 ms of what remains is ours. The frame gains less than `d3d` loses because it nears two refreshes (27.7 ms) and waits instead. A driver cannot set these flags; a program can.

## Work the frame out before the first OUT (Y14), `M8D3DT` measuring, `M8D3DU`, `M8D3DV`

Any port access empties the L1 (module SW1). A triangle's lookups take ~11 us run one after another with no OUT, and a mask triangle cost 190-230 us in the driver against 66 us in M8SHAPE: about 125-165 us a triangle was C running from RAM between writes.

Mask and stamp copies now queue their writes and go out in one asm loop. High priority, agent on, one run each:

| fps | `M8D3DR` | `M8D3DU` chunk 14 | `M8D3DV` chunk 8 |
|---|---|---|---|
| hal_many | 31-35 | 36.9, 40.6 | 41.0 |
| hal_many_e, DONOTCLIP and DONOTUPDATEEXTENTS | 34.5 | 45.6, 51.6 | 44.7 |
| hal_large | 45-51 | 40.1, 40.8 | 45.6 |
| hal_grad | 40.2 | 34.9, 39.9 | 41.9 |
| hal_turn100 | 32-34 | 35.1, 33.5 | 32.1 |

`hal_many` engine ~18-22 to ~12 ms. Chunk 14 waits for the FIFO to empty, leaving the engine idle when a long fill ends with the next command still crossing the bus: `hal_large` lost ~1.7 ms. Chunk 8 gave that back. The earlier asm queue arm in M8SHAPE gained nothing because it had almost no logic to move: **asm alone is no lever; moving logic off the far side of an OUT is.**

Bug found reading the merged code, in the pushed `M8D3DR`: a mask copy could run with a stamp's source compare left on, if a frame mixed them. Fixed. The bench never mixes them.

Pictures of `M8D3DV` from the card look whole. `card_06_hal_many_queued.png` has a small white mark on the left page, likely the pointer: not confirmed.

## All replay writes queued, large commands sent at once (`M8D3DW`, `M8D3DX`)

| fps, high priority, agent on | `M8D3DV` | `M8D3DW` all queued, two runs | `M8D3DX` large sent at once |
|---|---|---|---|
| hal_tiny, last | 67.6 | 65.6, 66.3 | 68.5 |
| hal_small | 59.5 | 67.6, 64.7 | 70.2 |
| hal_large | 45.6 | 41.5, 37.8 | 41.1 |
| hal_many | 41.0 | 36.8, 40.4 | 41.2 |
| hal_turn32 | 64.9 | 57.6, 63.0 | 68.6 |
| hal_turn100 | 32.1 | 31.1, 30.1 | 33.3 |
| hal_grad | 41.9 | 34.1, 41.2 | 42.2 |

Holding a whole frame back leaves the engine idle while the CPU works; before, cold C ran during long fills. So a command whose box is 1,500 px or more goes out at once, and only small ones are held. `hal_large` in `M8D3DX`: one run, `d3d` 5.09 ms against ~3.5, Flip's work ~13.3 ms, lowest yet. Rate not settled.

Pictures from the card look whole. A small white mark at (316, 250) on page 0 only, in every test: likely the pointer, which would show on one page in two. Owner's eye.

## Desktop bench on `M8D3DX` (M8BENCH, 800x600x16, agent on, two runs, `card_x1`, `card_xb1`)

- **Pixels equal `noag1` in all 36 tests:** two pages, masks, bands, stamps and the queue left GDI's drawing alone.
- Speed against `card_d3d81`, agent on, `M8D3D8`: geometric mean 1.017; `card_xb1` against `card_x1` 1.005. No change. Single tests swing a third between runs: `draw_icon` 68 and 90, `stretch_dib` 26 and 36, `getpixel` 1,643 and 2,424. One run of one test says little.
- Against `noag1`, agent-free: 0.877, the agent's cost. Against ATI at 8 bpp the comparison does not hold: different depth.

**Y14 for GDI: not built.** Read, not measured: `m8hw.c` already builds its fills in a queue (`m8_fill_q`, `m8_q_send`) and sends host pixels by `rep outsw`; `m8_fb16_send` converts rows in RAM before the first OUT. What runs cold is a little set-up per send, against milliseconds of DIB Engine work and bus bytes, and a run-to-run swing of a third would hide any gain under ~10 %. `_TEXT` is also near its 64 KB limit.
