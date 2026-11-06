# Next session, after the evening of 2026-11-06

Earlier handoff: `docs/next_session_2026_11_07.md` (its "Open" list still stands, less what is below). **The cloud session's tests are in #59**: carry this line on until every box there is ticked. Results: `docs/captures/2026-11-07_shape/README.md` and `docs/captures/2026-11-07_r892/README.md`. Levers: `docs/mach8_frame_rate_plan.md`, "What a frame's time is made of", Y1-Y13.

## Card

`m8d3dr.drv`, 640x480 at 72 Hz for Direct3D, agent ON. Way back: `C:\V9XREMOTE\REG\M8D3DD.REG` and `C:\V9XREMOTE\REG\M8DD32.DD` (rename in by WININIT.INI). The 5160 is in the cellar, the owner on the ground floor: batch trips, say exactly what is needed.

**Scripts are in the old scratchpad, not the repo:** `install_h.ps1`, `cycle.ps1`, `shape.ps1`, `bench.ps1`, `grab_run.ps1`. Rewrite from `tools/m8_card_cycle.ps1` if gone: build into its own folder, put `<NAME>.DRV`, `M8DD32.NEW` + `WININIT.INI`, a `.REG`, restart through Start > Shut Down, wait, bench. A PowerShell line with a `.EXE` destination or a regex can trip the safety check: use a script file. Long patches: a Python file, not a heredoc.

## Figures to beat (high priority, agent on, comparable with each other)

| fps | Morning `h1hi` | `M8D3DR` |
|---|---|---|
| hal_tiny, last | 67.4 | 67.7 |
| hal_small | 55.2 | 58-70 |
| hal_large | 33.6 | 45-51 |
| hal_many | 21.6 | 31-35 |
| hal_turn32 | 48.6 | 56-66 |
| hal_turn100 | 35.7 | 32-34 |
| hal_grad, real corner colours since `M8D3DM` | - | 40.2 |

High priority with the agent on runs close to agent-free (`hal_large` 33.6 against 36.0): use it for iterations. Agent-free only for milestones. **One bench at a time:** three hangs followed a second high-priority `M8D3DF`. Owner's view: the agent is implicated; every hang had it running, none agent-free. Not proven.

## What is now true

| Finding | Evidence |
|---|---|
| Engine commands set the cost, ~40-60 us each; asm emitters are no lever for triangles | M8SHAPE `queue`, `qerase` |
| Mask or polygon fill ~69 ns a pixel of its box, plain fill ~13-20 ns | M8SHAPE slopes, page fill 6.3 ms |
| Whole-page fill holds the CPU 190 us: one long command runs alone | M8SHAPE `page` |
| Pixel clock does not change engine speed; 60 Hz costs fps until a frame fits 16.67 ms | `e1hi` |
| hal_large is not locked at 36: frames alternate one and two refreshes, so every ms shows | `M8D3DK` onward |
| Masks, core rectangles, bands, stamps: in the driver, pictures read from the card | `card_*.png` |
| The caps value named flat was Gouraud; both claimed now | `fff9853` |
| Paging: pageable buffers, swap writes +74 %, page-ins +1.5 %, open -3 %, one run each, builds differ | r892 README |
| LZ4 still the best modelled; 25 % of used swap slots are one repeated dword, per slot, not per I/O | `sectors.py` |

**`M8D3DF ... grab`** reads both pages from the card to `C:\M8D3DF.GRB`; `tools/m8d3dtest/m8grab_png.py` makes pictures. About 10 s a test: allow several minutes. Use it instead of a trip to the cellar.

## Open

- **Ghost images on the desktop (photo `desktop painting issue.jpg`, `NOAGDISK`, `M8D3DD`).** Seen once. Card run of `NOAGPAGE` with `FB16CHK` still to do: 0 differ means RAM holds them, which fits the owner's Start menu test. Bed `vm_5160_mcp` on `M8D3DH`, 3 loops: 0 differ, 0 sends overlapped; did not reproduce. A wrong-looking result from a bed run that never restarted was caught by the DLL md5: check it every time. Then alternate `M8D3DH`-style pageable against `M8D3DL`-style locked (`GMEM_FIXED` in `m8_fb16_pageable`).
- Counter `m8_fb16_reenter` in `fb16.c`: delete once that is answered.
- Restart hang on `M8D3DN` boot 244, black screen and egg timer after `M8D3DK`'s second bench, boot 241. Restart stalls are older than today's work.
- `hal_turn100` no better than the morning: masks do not help a shape that changes. Batched edges (0.9x) not in the driver; build limiter is rough.
- Bands are by rows only. Fixed 5 x 9 x 5 palette, steps show: C2 next. **Owner's eye on bands and dxdiag still to come.** Dxdiag not run since the caps fix: it may now take the Gouraud path.
- Desktop bench `M8BENCH` not rerun since V10: pixels by hash against `noag1`, before any of this ships.
- SWAPCOMP and XT-CF: no change made; `XTIDEMP.MPD` a67fefc4, `SWAPCOMP.VXD` 9FDE4934 verified on the CF. Owner imaged the CF on 11-06. Order: XT-CF `both` (built, `%TEMP%\xtcf_builds`), counter build for zero pages, `LZ4B12FN`, file cache sweep. Test by restart, same order, two runs, deltas only, alternating builds (owner: memory varies).
- Bed `vm_5160_mcp` left running on `M8D3DH`, menu timeout attempted (`C:\CONFIG.BED` holds the old file). Its Enter-at-menu stop is not explained.

## Order of levers

1. **Y1 start frame 2 early (owner):** one long erase from RenderPrimitive once the flip has landed, overlapping DirectX. Many boxes block on the FIFO; one rectangle does not. *Estimate* 2-4 ms.
2. **Y10 stored ramps (owner: what is true everywhere):** 1.09x flat at 100 px against stamps only when colours repeat. Needs fields by direction and rate, palette in shades.
3. **DirectX's own time:** hal_many `d3d` 9.5 ms, ours ~3 of it: V13, V1b, then our per-triangle arithmetic (`M8D3DF` column `ours in d3d`).
4. **Y13 background over foreground, Y11 mirrored masks, Y12 golden-ratio band shift** (owner). Y2 third page, Y6 joined quads for dxdiag, Y8 low-res modes.
5. **Reuse (owner, after each win):** masks, core rectangles and stamps for M8CUBE's polygon escape (`poly3d.c`), DirectDraw sprites, GDI's mono expansion. Then textures: stamps are the path for TX1.
6. **60 Hz** retest once hal_large's work is under 16.67 ms: row in the capture README.

Ideas the owner has thrown out have paid repeatedly: core rectangle came from "think outside the box", stamps from "paint once, copy". Record tangents in the plan's Y table rather than dropping them.
