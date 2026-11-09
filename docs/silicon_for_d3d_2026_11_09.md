# Card silicon the D3D path does not use yet, 2026-11-09

Desk pass, nothing run or built. Every effect is an *estimate* unless a figure is quoted.
Cites: FRP = `docs/mach8_frame_rate_plan.md`; TUN = `docs/captures/2026-11-08_tunnel/README.md`; SIL = `docs/captures/2026-11-07_silicon/README.md`; PRB = `docs/captures/2026-11-06_probes/README.md`; SHP = `docs/captures/2026-11-07_shape/README.md`; CRV = `docs/captures/2026-11-07_curves/README.md`; COV = `docs/mach8_card_coverage.md`; G = `references/ati_mach32/guide.txt`; D3D = `drivers/mach8_display/src/m8d3d.c`.

Used today (D3D:42-79): polygon fill 2213h, LINEDRAW polygon mode, XOR/replace mixes, WRT_MASK/RD_MASK, mono blit 2271h (masks), VRAM blit 6211h with source compare 92EE/EAEE (stamps), SCAN_TO_X. Not used in the HAL: 6213h/A213h/E213h, BG colour source, PATTERN_L/H, PATT_DATA, scissors, DEST_CMP, SHORT_STROKE, FIFO_OPT/MAX_WAITSTATES.

Cost model the ranking rests on: each engine command is ~40-60 us whatever is sent (SHP readings); a line ~44 us at any length (CRV:35); a polygon/mask fill ~69 ns/px of its box, plain ~12.6 ns (SHP:24). At 320x240 the engine time is command count, not pixels. So fewer lines and fills is the engine lever; fewer writes is the bus lever.

## Ranked top 10

### 1. Draw no on-screen outlines: shift the right edges one pixel in the scratch (a, b)
- What: the type C fill is left-inclusive, right-exclusive (G:6936-6941), so the joined path redraws every outer edge on screen after the fills (D3D:2971 "its outline over the rows the fill leaves", loop at D3D:2993). The gradient path already moves right-bounding edges one pixel instead: "bands need no outline in one colour over them" (D3D:1889).
- Evidence: SHP `noline, right edges shifted` 314 us vs flat 422 at 32 px, 896 vs 1,043 at 100 px (SHP:19-20 table), but "wrong: 69 of ~450 edge pixels" at 100 px. Outlines are 35.1 ms (TUN ET, line 83), 55.4 ms (TUN EZ, line 122) of CPU+send; chaining them already saved 1,380 writes (TUN FC, line 132). A `[diag] nooutline=1` switch exists (D3D:2427, 3237).
- Effect: (a) est. 20-30 ms engine (lines are ~44 us each; outlines are a large share of the ~1,500 lines, share unmeasured). (b) est. up to ~2,000 writes (~15 ms bus). (c) the 69/450 wrong pixels are shallow right edges; in a screen-filling scene the neighbour's left-inclusive fill covers them, so holes show only at silhouettes and the screen border (reasoning, not measured).
- Cost: no RAM or VRAM. Risk: cracks at silhouettes, bottom rows, seam carry (D3D:2886 comment: nothing right of the seam at 320x240).
- Cheapest test: one tunnel run with `nooutline=1` bounds the saving and shows by eye which pixels go missing; no build.

### 2. Hazard stripes as stripe polygons, not cells (c, no new silicon)
- What: `tex2.bmp` rows 173-195 hold 16 px wide stripes, period 32, shifting 1 px per 2 rows (read from `references/dx61samples/d3dim/media/tex2.bmp`): about 8 yellow parallelograms across the 256 px band. Each is one quad through the wall's corners (FRP Z13, line 633), joined into the yellow patch by the existing path.
- Evidence: M8D3EO 2x2 tessellation cost 643 ms ours and 11,490 writes because every cell became triangles (TUN:14-27); a stripe list is ~8 quads a band-wall, not 4x pieces. TUN:47-55 gives the region census (63 regions, 20 over 32 px).
- Effect: (c) the stripe look on near walls; (a)(b) up to +8 filled quads x 4 edges per visible hazard wall (est. 30-60 edges a frame if 1-2 walls are near; unmeasured). CPU: a table of stripe corners per texture, tens of bytes.
- Risk: needs the texture-to-wall mapping the HAL has (u,v per corner); perspective across the quad is affine-approximated.
- Cheapest test: draw the 8 stripes of one near wall in M8CUBE or a probe, owner's eye against `reference_host_gpu.png`.

### 3. Dither adjacent light levels with the mono pattern registers (c, a flat in cost)
- What: PATTERN_L/H (BEE8 index 8/9) are 4-pixel mono patterns, card-measured ok (COV:70, M8A2 10-11); with FG = level n, BG = level n+1 (BG source = BKGD register, G:10016) a fill gives intermediate steps. 16 light levels become ~32-48 apparent ones with no extra patches.
- Evidence: PATT_DATA/pattern dithers ok on card (COV:229, M8FG6-9); ATIM8 uses it for dithered scanline spans (docs/captures/2026-10-22_card README, `card_scanp`). Not probed on the extended polygon fill; 86Box SCAN_TO_X ignores PATT_INDEX, so the bed cannot test it (same README, `card_scanx1`).
- Effect: (c) smoother light steps (owner: depth from the light steps, TUN line 10). (b) +2 writes per colour change and 2 more colours per patch pair; engine same as plain polygon fill (est.). Fewer light levels needed could cut colours a frame (38-41 now, TUN M8D3EM).
- Risk: 4-px horizontal period shows as vertical lines at 320x240 doubled pixels; owner likes crisp (FRP Y12, line 572). DP_CONFIG becomes 2233h-style (mono source = pattern, bits 6:5 = 1, G:10014): unprobed with polygon fill.
- Cheapest test: M8JOIN variant, one quad, two adjacent colours, read back and look.

### 4. Texture atlas in VRAM, 6213h fill for large near pieces (c)
- What: DP_CONFIG 6213h takes the colour per pixel from a VRAM source; atlas outline XORed into plane 7 (SIL "tatlas").
- Evidence: 1.05x flat at 100 px, 1,064 vs 1,011 us (SIL:11, 52); pixels follow the atlas 1:1 (SIL:26); card works on both rows (PRB:7). Spans and lines step the source (SIL:30-31).
- Effect: (c) true stripes and seams on a piece. (a) +5 % fill at 100 px; (b) edges XORed in and out of plane 7 = up to 2x edge writes on those pieces (SIL line 11). Atlas upload ~64 KB once: ~250 ms at 3.82 us/B (est. from `docs/iowidth_measured_2026_09_20.md`).
- Limits: 1:1 only, no wrap in a row (SIL:29, FRP Z4); 7-bit colours, left edge keeps bit 7 (SIL:26); per-row scale costs 4.6x flat (M8TEXR `rows` 4,655 us, SIL line 50s). So only fronto-parallel pieces unless pre-scaled copies exist (see 5).
- VRAM: pitch-1024 layout already fills 1 MB at 640x480 (D3D:2412 scratch right of both pages); at 320x240 pages are 77 KB (FRP A6, plan_review A6), so room exists only if the layout is repacked. 64 KB texture + ~3 pre-scaled sizes est. 200-300 KB.
- Cheapest test: M8TEX tatlas on the stripe band at 320x240, owner's eye.

### 5. Pre-scaled stripe/texture copies by engine enlarge (c, a)
- What: build N integer-factor copies of a texture patch in spare VRAM with the proven enlarge path (source once, engine copies spread columns then rows).
- Evidence: stretch_dib 1.15x ATI, stretchblt 1.01x (docs/mach8_driver_method.md:113); "whole factors, box unclipped only". FRP O2 (line 134) estimates ~560 blits ~20 ms (estimate).
- Effect: (c) makes 4 usable at more than one size; (a) one-off cost per size, then one 6213h/6211h fill per piece. Not perspective-correct; wall panels in a tunnel change scale continuously, so sizes quantise.
- Cheapest test: build 2x/3x/4x of the 32-px stripe patch, compare against the wall at three distances by eye.

### 6. Hardware scissor in place of CPU clipping (a, CPU)
- What: scissors drop draws outside a region for all operations (G:6678-6688; 8514 scissors BEE8 index 1-4; INSIDE_SCISSOR card = bed, M8A2, FRP S2 line 192).
- Effect: top, bottom and right are free; the left side loses fill parity (the fill counts toggles from the left, D3D:2412-2420 comment), so left clips stay on the CPU. Float clipping was cut 88 -> 74 ms by testing only crossed sides (TUN FH, line 142); the rest is small. Est. 3-8 ms CPU, no writes added beyond 4 per window.
- Risk: LINEDRAW coordinate range on the Mach8 is not in my sources (the guide gives -512..1535, G:10535, for the Mach32); clip modes and CLIP_OVERRUN are Mach32 only (see unusable list).
- Cheapest test: draw one edge that runs off the right and bottom of the scratch window with scissors set; read the neighbour columns.

### 7. Light as a masked field pass (c, b uncertain)
- What: hue in the high nibble, shade in the low; fill hue patches with WRT_MASK F0h, then add light on the low field with mix 1Bh and WRT_MASK 0Fh (FRP V17, line 469; Z3, line 623).
- Evidence: add saturates inside the field on the card (SIL:28); mixes cost no extra time on the polygon fill (SIL:27); masked box fill 347 vs 190 us at 100x100 (SIL:18-20) = 1.8x.
- Effect: (c) smooth light, no 16 light steps. (b) fewer colours a frame in the hue pass (patches merge across light), but every triangle is then in two patches, so edges may double: net could be negative. (a) engine up to 1.8x on masked passes while the send is already 42 ms card-limited.
- Cheapest test: count edges for a tunnel frame grouped by hue only and by light only, from the census data, before any card run.

### 8. Colour-pattern fill for stone grain (c)
- What: A213h repeats a 16-32 entry colour pattern along the row, restarting each row (PRB:8); E213h indexes it by the source byte (PRB:9). FRP Z12 (line 632) wants an aperiodic grain.
- Evidence: M8GRAD on the card; ATIM8 loads 16-word patterns (COV:229).
- Effect: (c) grain in the ~95 % grey stone (TUN:49). (b) 16 PATT_DATA writes per pattern change (est. ~120 us each), so one pattern per colour = about +600 writes at 38 colours: only for the big grey. Screen-aligned, not on the wall.
- Cheapest test: one grey patch with a 16-entry noise pattern, owner's eye.

### 9. FIFO_OPT W_STATE_ENA and MAX_WAITSTATES (b)
- What: FIFO_OPT (36EE, write-only): bit 0 = 0 gives engine-write wait states only when the FIFO is full, not at half (COV:106); MAX_WAITSTATES 6AEE bits 3:0 (COV:105). FRP P11 (line 164): ~28 % of a PIX_TRANS cycle is the card's IOCHRDY.
- Effect: est. up to 13 ms of the ~49 ms write time (28 % of ~6,400 x 7.6 us), less when the send is card-limited anyway. Bed has no bus timing (COV:197-198).
- Risk: IOCHRDY held past DRAM refresh; FRP says soak first (line 207). TEST.COM writes 009Ah.
- Cheapest test: M8SPAN timing per setting on the card, then a soak.

### 10. Dropped to a plain-fill core for big patches (a, small)
- What: a plain fill costs 12.6 ns/px against 69 ns for the polygon fill (SHP:24); a patch's interior rectangle by plain fill, the rim by polygon fill (FRP V15 "core rectangle", D6).
- Effect: est. at most ~4 ms engine (76,800 px x ~56 ns saved if one pass covered the screen), and +9 writes a split. Ranked last: at 320x240 the engine time is commands, not pixels. Y17 (FRP line 577) lists the inscribed core for circles.

## Checked, unusable here
- Block write and 64-bit fill: ATI68800-6/AX only (G:7099-7131); 64-bit fill needs 2 MB; the card is a 28800-5/6 with 1 MB.
- Clip modes 1-3, CLIP_OVERRUN, EXT_GE_STATUS 62EE: Mach32 only; 62EE reads 0 on the Mach8 (`docs/mach8_guide_rules.md` FIFO table; G:6995-7010).
- 8x8 mono tiling, hardware cursor: Mach32 only (FRP line 129).
- SHORT_STROKE 9EE8: 8 directions, up to 15 px; tunnel edges are arbitrary slopes. EXT_SHORT_STROKE C6EE: card does 4 diagonals only (levers doc, M8SSV3).
- DEST_CMP (EEEE) depth bands: the joined path only handles non-overlapping pieces, so Z8 is not needed there; unprobed (FRP P7, line 33).
- Raster palette splits (A8/A9) and halves switching: 11.5 us a poll, and each switch re-syncs the monitor for over a second (levers doc, M8RAST, M8FLIP4). VGA half Mode X: no memory path to the engine (PRB M8XMEM; VGA-half findings summary 1-2); only the shared DAC.
- Engine-idle IRQ (Z22/Z31): no free 8-bit IRQ (FRP O13, line 150).
- High-byte latch sharing (P13/V18): ruled out today (latch does not survive a commit).
- Queue size, background feeding by timer: ruled out today (task brief; TUN FN, line 154).

## Looked for and not found
- No source that the Mach8 polygon fill takes pattern or BG colour source in poly mode; only rect fills and spans are card-probed (COV:229). Item 3 rests on that gap.
- No source for the Mach8 LINEDRAW coordinate range or scissor behaviour on lines past the window (guide gives Mach32 ranges only).
- No evidence a source of one fill can wrap or scale (SIL:29, PRB limits); no per-pixel colour from one flat-colour source row.
- No number for how many of the ~1,500 lines are outlines, nor the fill count per frame (`m8d3d_stat[26]` is in the census but not in these READMEs): items 1 and 10 need it.
- `references/ati_mach32/guide.txt` marks Mach8 vs Mach32 per register with OCR noise ("Macha"); each citation above was read for a Mach8 marking where the table shows one, not for all.
