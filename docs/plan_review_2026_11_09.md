# Plan review, 2026-11-09

Read-only pass over the plan and research docs against today's facts. Nothing built or measured here; every effect is an *estimate* unless a figure is quoted.
Cites: FRP = `docs/mach8_frame_rate_plan.md`; TUN = `docs/captures/2026-11-08_tunnel/README.md`; NS6/7/8 = `docs/next_session_2026_11_06/07/08.md`; SHP, SIL, CRV, XA, R892 = `docs/captures/2026-11-07_{shape,silicon,curves,xa,r892}/README.md`; LEV = `docs/mach8_levers.md`; DDK = `docs/ddk_levers_2026_11_08.md`; FD = `docs/fastdoom_techniques_2026_11_08.md`; JH = `docs/jhhoward_techniques_2026_11_09.md`; SRV = `docs/dx_sdk_ddk_bench_survey_2026_11_09.md`.
Facts: F1 any port I/O empties the cache; F2 write 7.6 us, status read 5.7 us, FIFO 16; F3 FF frame split (card not the limit at 320x240); F4 today's tries; F5 owner: 320x240 target, 60 fps, bus+CPU+RAM together, look matters.

## A. Dismissed or negative items to retry now

Verdict: FLIPS (retry), NARROW (retry a part), STAYS (checked, new facts do not change it).

A1 FRP Z6 / TUN M8D3EQ-ER, "fill pixels were not the main cost". FLIPS, done: FB 320x240 once the card was the limit (F3, F4): flip 248 -> 185 ms, FF 164 ms (TUN FA-FC, FF). Next: per-program choice, FRP Z6. Bus down (7,045 vs 8,438 writes), CPU same, RAM none.
A2 TUN M8D3EO, 2 x 2 tessellation. Dropped: "Per-cell float maths on the 387 is the 7x in ours. One texel per cell turns diagonal stripe edges into small triangles"; writes 11,490 vs 8,209. NARROW: integer maths exists now (F4, EX), but writes cost 7.6 us each (F2) and CPU+bus are the limit (F3), so blanket stays out. Retry only for large near pieces (FRP Z13): bus + a few hundred writes, CPU small. Cost: one build, owner's eye. Patch: `drivers/mach8_display/wip/2026-11-08/tess_m8d3eo.patch`.
A3 TUN ES, queue 8192. Dropped: "no change (173.5 ms). Cache flushes from mid-frame sends are ruled out" and static data +80 KB (TUN "Hangs, 11-08 night"). FLIPS in part: ES ran at 640x480 where the card was the limit; at 320x240 a frame is ~5,550 writes against a 4,096-entry queue (m8d3d.c `M8D3D_Q_MAX`), so a full queue forces a blocking send, and F1 makes the C after each send cold (~11 vs ~150 us a triangle). Part of ~47 ms of blocking sends (F3). Bus same, CPU down (est. 5-15 ms), RAM +16 KB (8192 x 4 B). Cost: one build; keep if CPU falls by more than the RAM is worth (F5).
A4 FRP Y14, `M8D3D_Q_SEND_PX` 1500. Set when the bench was engine-bound ("Holding a whole frame back leaves the engine idle", SHP). STAYS for the tunnel: it gates only mask and stamp copies in the replay (m8d3d.c:2907, 2943, source read), not the joined path. Retune for `hal_*` at 320x240 only if those benches matter. Cost: one bench pair.
A5 FRP Y1 / Z25, start frame 2 early. Dropped: "little to gain with two pages ... best case ~2 ms of 32 on hal_many" (FRP Y1, M8D3DS). FLIPS: the tunnel has ~60 ms of program + D3DIM and 88 ms of ours against a 164 ms flip (F3); if Flip returns while the card fills, frame = max, not sum. CPU/overlap est. up to ~60 ms of ~250. Bus same, RAM none. Cost: one build; the page switch must wait for frame N's last write (NS8 "later afternoon").
A6 FRP Y2 / Z7, third page. Dropped: "costs the room stamps and masks use" (FRP Y1). FLIPS: at 320x240 (F5) a page is 77 KB of 1 MB, so room is not scarce. Removes the page wait (3.9-7.6 ms in `hal_*`; 3.3 ms final wait in TUN EZ, est. 3-8 ms). Bus +2 CRT_OFFSET writes, RAM (VRAM) +77 KB. Cost: M8PAGE variant, then a build; useful only after A5.
A7 Gradient triangles: FRP V25/Z1-Z3/C3/V17; NS8 "11-07 night": "gradients are not the cost" (flat-only M8D3EE, same engine time); FRP "Machine-level" table E213h "OUT for gradients"; SIL tatlas 1.05x flat at 100 px, masked fill 1.8x. FLIPS for the look: the card has slack at 320x240 (F3), so 1.05-1.8x on fill time is hidden while the CPU is the limit. Cost is bus: tatlas XORs edges in and out, up to 2x edge writes on those pieces (F2, est.). Light in fill (FRP Z2/Z3, D7) replaces the 16 flat light steps (TUN EM). CPU same, RAM none. Cost: probe the join path with one hue, then owner's eye.
A8 FRP V18 / P13, high-byte latch. Dropped: "high-byte latch (V18) ~2 us a triangle, out" (FRP "What a frame's time is made of"). FLIPS to a probe: that figure was per triangle at ~45 writes; now ~5,550 writes at 7.6 us (F2, F3) and a shared low byte could cost 5.7 (FRP P13). Upper bound ~1.9 us x shareable writes, est. <= 10 ms of bus; the real share is unknown. CPU same, RAM none. Cost: one probe, M8BYTE3 extended.
A9 Stamp and mask cached paths: FRP V15, Y4, Y9, Y11, Y13. Dropped: "masks do not help a shape that changes" (NS7 Open). STAYS for the tunnel: joined patches are large and change each frame (TUN: 1-4 px 8, 5-16 px 122 before joining). They stay IN for `hal_many` (41 fps, SHP). Revisit only for Z11's far picture (card-to-card copy).
A10 "Card starves" (TUN ER, FRP Z15: "~120 ms/frame of our CPU while FIFO empty"). FLIPPED by F3: FB cut FIFO-full reads 1,441 -> 116; FA "alone neutral". The remaining idle is the CPU computing between passes: that is A5 and the A3 flushes. No separate retry.
A11 FRP Z19 register shadow. STAYS: F4 "66 of 8,200 writes redundant" is the same finding; F1 does not change it.
A12 TUN FD chained scratch XOR outlines. STAYS: "runs are short, the search costs more than it saves"; the search runs cold under F1, which worsens it.
A13 TUN FG 128-row bands. STAYS: "the card is the limit again; no gain" (F3: 128 rows = card limit).
A14 FRP V12 / A1 asm emitters. STAYS for triangles: "asm queue no faster than C" (FRP "What a frame's time is made of"). F1 is why the queue exists; the queue is the asm. A1 for other paths is B13.
A15 FRP Y5 field rendering. STAYS: halves pixel time, but pixels are no longer the limit at 320x240 (F3).
A16 FRP Y7 / V8b unchanged triangles, FD 2 differential frame. STAYS: "A tunnel moves most pixels" (FD section 3); the bus saving is small for this bench.

## B. Open items still to do

Speed (CPU)
B1 Integer clipping; ~100 clipped triangles a frame run float `clip_side` (NS8 "later afternoon"): next.
B2 The ~18 ms of the flip outside the timers, incl. the diagnostic `m8dd_tri_census` (NS8 "later afternoon"): next; gate the census off first.
B3 FRP Z24 x87 control word once per call, magic-double float-to-int: partly done by EX (`203c9b3`), the rest not built.
B4 FRP Z16 asm / Cyrix-style pass on hot paths: after Z15; judge speed, size and RAM together.
B5 FRP Z20 / JH 2 merge same-colour neighbours at sampling: not built (edge phase 15 ms, TUN FF).
B6 FRP Z21 / JH 1, 6 frame-time-driven detail step: not built.
B7 DDK 1 DP2 + `GUID_D3DCallbacks3` + `DRAWPRIMITIVES2`, est. 10-30 ms of D3DIM's ~60 ms: not built; needs a trace of which path runs.
B8 DDK 1, 2 `CANRENDERAFTERFLIP` one bit, FVF extcaps, palettised 8-bit texture format: not built.
B9 FRP D14(a) profile D3DIM by symbols; D10 runtime clip 5.6 ms at 64 triangles: (a) next; D10 lever unproven (guard band already set, DDK 1).
B10 FRP V13, V1b null HAL for DirectX's own floor: OPEN.
B11 FRP D14(c)(d) execute buffers whole, integer transform, patched D3DIM: idea.
B12 FD 4 counting sort for edges: not built; low value now the hash takes 13-15 ms.
B13 FRP A1-A3, F2 asm for fb16 unlock and per-row set-up: A1 out for triangles (V12); A2, A3, F2 open.
B14 JH 3 prebuilt command sequences by size class (= Y9 at command level): not measured; RAM up.
B15 JH 9c cursor hidden once per frame: not checked.
Speed (bus writes)
B16 FRP Z23 / SRV 5.1 poll back-off: not built; FIFO-full reads 277 a frame (~1.6 ms, TUN FF).
B17 FRP O15 open-loop FIFO, waits by known engine time: not built (Z18 N-slot waits already in).
B18 FRP V18/P13 latch probe: see A8.
B19 FRP P11 card wait states (MAX_WAITSTATES, FIFO_OPT): sweep not run; needs a soak (worklist B3); refresh bounds IOCHRDY.
B20 FRP "Batched edges, unchanging writes once" (0.8-0.9x, SHP): not in the driver.
B21 FRP Z6 per-program resolution; Y8 400x300: next (NS8 "later afternoon").
B22 FRP Z22 / O13 GE-idle or V_BLANK IRQ: static sources first; the card's IRQ jumper is unknown.
B23 FRP P12 string I/O for same-port runs: measured 4.6 vs 4.5 us a word (FRP Phase 0): no gain on short runs.
B24 JH 9a copy the surviving area on pans: needs a blit probe; a tunnel is a zoom, so low.
Speed (overlap)
B25 FRP Z25 flip queued, `DDERR_WASSTILLDRAWING` only when pending: next (see A5).
B26 FRP Z7 / Y2 third page: not written (A6).
B27 FRP O14, O16, O17 frame-locked timer, pipelined frame, triple buffer: ideas; O14 needs a VxD.
B28 FRP T1 present by beam (window); T2 is done as V10: T1 open.
B29 FRP Z17 remainder: ~47 ms of blocking sends still (F3); FA built, the rest is A3 and A5.
Look
B30 TUN FE see-through stripe, owner's eye: pending.
B31 FRP Z13 near panels as shapes; TUN EX/EY "a yellow square lingers and crawls": not built.
B32 FRP Z10 depth layers: EP built, "still stepping"; light in fill not built.
B33 FRP Z12 / S7 aperiodic 8 x 8 pattern per cell: not built.
B34 FRP Z11 far picture, parallax: not built.
B35 FRP D7 / C2 palette ramps hue x shade, with Z2/Z3 light in fill: not built; unlocks D8.
B36 FRP C3 gradient bands, Y10 stored ramps, V17 shade/hue split, V27 BG_COLOR_SRC: OPEN.
B37 FRP D5, D6, D9 real texture path, keyed cut-outs and mips, bilinear/perspective by row shift; Z1 into the HAL: not built.
B38 FRP D4, D8, Y22, Y23 effect tag, stipple glass, blends, fog: not built; fog not yet seen.
B39 FRP Z14 scanline CRT mode: not written. Y12 golden-ratio band shift, Y20 aperiodic demo: owner's eye.
B40 FRP Z8 / Y21 depth compare, Y24 Z read-back, D12: not written.
B41 FRP CT1-CT3, Stage 5 probes M8GLYPH, M8TEXT, M8CELL, VGATEX: none built.
Compatibility and benches
B42 FRP D3 census on real programs (3D WinBench 97, Monster Truck Madness, Turok): census built, programs not run.
B43 FRP bench ladder / SRV 4: flipcube, DX3a/DX5 tunnel, twist, flip3dtl, tunneldp vs tunneleb, lights: none run on the card.
B44 NS8 exit hang after Alt+F4 (#55) and boot hangs: open; test the close on ATI's driver.
B45 FRP "Open against all of it": intermittent card hang; DirectDraw Z caps lose the whole HAL: open.
B46 FRP Z26, V28 Mode X / 320x240 on the VGA half: not looked at.
B47 FRP Z27-Z30, FD matrix rows 1-12 (#47): DOS 3D path, sound, Glide subset, other APIs: ideas.
B48 FRP D13 windowed HAL; FRP F1, F2, F4-F6, B1, B2 16/8 bpp DirectDraw paths: open, off the tunnel path.
B49 NS7 Open: desktop ghosts (locked vs pageable); delete `m8_fb16_reenter`; JH 8 phase-split bar as a measuring aid.
RAM
B50 FRP R1, R2, R3, R4, R5, R6 (back buffer, fingerprints, 16 bpp desktop, 8 bpp statics, shadow, D3D arrays): census done, levers not applied.
B51 FRP V20 triangle arrays sized sensibly; `M8DD_TRIS_MAX`: OPEN; EV took 30 KB of static data.
B52 FRP P-a to P-g paging; SWAPZF/XTBOTH gain unmeasured (PAGETIME soak): open.
B53 FRP "Whole machine": locked RAM down where the driver can give it back (EV -30 KB): ongoing.

## C. Ranked next 10

Gain estimates are *estimates*; cost is one build unless stated. Cheapest first within gain.
1. B2: gate off the diagnostic census and time the ~18 ms: CPU down by up to ~18 ms, bus and RAM none; no build beyond a flag (NS8).
2. A5/B25 Z25: flip returns while the card fills, program + D3DIM overlap the card: CPU/overlap up to ~60 ms of ~250; bus and RAM none.
3. B1: integer clip for ~100 triangles a frame: CPU down, est. 5-10 ms; bus and RAM none.
4. B8 `CANRENDERAFTERFLIP` bit, then B7 DP2: CPU down in D3DIM's ~60 ms (DP2 est. 10-30); bit costs one line, DP2 a token walker; RAM small.
5. B5 Z20 merge same-colour neighbours in the sampler: CPU edge pass 15 ms down, fewer edges so bus down; RAM none.
6. A3 queue 8192 at 320x240: fewer forced sends and cold-cache runs, CPU est. 5-15 ms; bus same; RAM +16 KB (weigh, F5).
7. A8 M8BYTE3 latch probe (one card probe): bus up to ~10 ms if writes can share low bytes; CPU and RAM none.
8. A7 light in fill on the joined path (Z2/Z3), one hue first: look gains smooth light; engine slack absorbs it; bus up to 2x edge writes on those pieces; owner's eye.
9. B21 per-program resolution, then 400x300: bus and fill down, CPU same; RAM none; owner's eye.
10. A6/B26 third page after Z25: page wait est. 3-8 ms down; bus +2 writes; VRAM +77 KB.

## D. Done

D1 FRP F3 16 bpp Flip with descriptions (`M8FLIP`, `M8FLIP4`): 165-220 fps, card = RAM.
D2 FRP P1, P3, P4, P8, O12 probes (`M8P0`, 11-02); P9/P10 as `M8LORES`, 11-08; P6 via V22 patched D3DIM.
D3 FRP `ddstr` string I/O in M8DD32; M8CUBE fixed point (`x`, +22 % a frame).
D4 FRP V1-V4, V8a (`M8D3DA`): hal_tiny 8-18 -> 28-46 fps; V5, V6 built; V19 built.
D5 FRP V10 two pages at 640x480 (`M8D3DD`): black overlaps gone; 892 KB less locked RAM.
D6 FRP V15 masks (`7a92f99`), core rectangle: hal_many 21.6 -> 33-35, hal_large 33.6 -> 51.
D7 FRP Y1 measured (`M8D3DS`); Y14 (`M8D3DV`-`DX`): hal_many 41 fps; Y15 flags (`hal_many_c/e`).
D8 FRP Y16 curves (`M8CURVE`); Y18 span lists and gathered segments (`M8D3DY`).
D9 FRP Z1-Z5 silicon probes (`M8TEX`, `M8TEXR`); V25 (`M8GRAD`), V26, V28, V29 probed.
D10 FRP D1, D2 tunnel runs on the HAL (`M8D3E7`); DX5 route (`M8D3E1`); census built.
D11 FRP V7 / D11 joined patches (`M8D3EH` to `3d02cab`): writes 18,621 -> ~4,400 (TUN M8D3EL).
D12 FRP D7 first step, texture palette (`M8D3EM`, `ced163d`): owner "colours infinitely better".
D13 FRP Z10 depth layers built (`M8D3EP`, `670d15b`); M8D3EO tessellation tried and shelved (negative).
D14 FRP Z6 320x240 (`M8D3EQ`-`ER`, `M8D3FB`): flip 248 -> 185 ms; owner "looked as good but faster".
D15 FRP Z15 hash edge cancel (`M8D3EU`, `1f4b93c`): edge phase 53 -> 13 ms; in-place reorder (`9b5fe8e`).
D16 FRP Z15 patch colour cache (`M8D3EW`): ours -29 ms; integer coordinates (`M8D3EX`): ours 100 -> 80 ms.
D17 FRP Z17 queue top-up (`M8D3FA`, neutral alone); Z18 N-slot waits already present.
D18 TUN FC on-screen outlines chained: writes 7,045 -> 5,665; FE facing at 1/8 pixel; FF 64-row bands: flip 178 -> 164 ms.
D19 FRP Z19 register shadow tried, negative, removed (`M8D3EY`); FD scratch chaining negative; FG 128 rows no gain.
D20 FRP Z18 / DDK / FD / JH / SRV research written (`7519626`, `594650a`, 11-08 to 11-09).
D21 Tunnel, same DX6.1 scene: ours 129 -> 88 ms, flip 196 -> 164 ms, writes 8,215 -> ~5,550 (NS8 "later afternoon").
D22 SWAPZF.VXD and XTBOTH.MPD kept on the card (owner, 11-09); byte-exact read-back; gain unmeasured (NS8).
