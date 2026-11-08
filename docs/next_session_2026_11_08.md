# Next session, after the evening of 2026-11-06

Earlier handoff: `docs/next_session_2026_11_07.md` (its "Open" list still stands, less what is below). **The cloud session's tests are in #59**: carry this line on until every box there is ticked. Results: `docs/captures/2026-11-07_shape/README.md` and `docs/captures/2026-11-07_r892/README.md`. Levers: `docs/mach8_frame_rate_plan.md`, "What a frame's time is made of", Y1-Y13.

## START HERE: 2026-11-08, afternoon (owner out; resume when he has time)

**Card state unknown.** Last known good: `M8D3EN`, boot 285. `M8D3EO` was being installed or run when the session's tool call failed ("agent froze"): first `v9xctl info` for the boot counter and `drv` in the registry; if wedged, ask the owner to restart. Reference picture of the intended look: `docs/captures/2026-11-08_tunnel/` (README has every run today).

**Committed today:** `3d02cab` joined path for every tunnel frame + banded fills; `ced163d` texture palette (16 texture colours x 16 light levels; owner: "colours infinitely better", depth from the light steps, still big flat triangles, yellow triangular).

**Not committed:** `drivers/mach8_display/wip/2026-11-08/tess_m8d3eo.patch` (apply to HEAD), two steps:
1. `M8D3EN` (ran, owner did not see it): colour by the texture rectangle a triangle's corners span, cached; a quad's halves agree, so the diagonal cancels. Card: ours 90 ms (144-162), joined 194 (115-135), writes 8,209, frame 420 ms.
2. `M8D3EO` (built, never measured): each textured triangle as 2 x 2 cells in texture space (`M8D3D_TESS`), texel at the cell centre, light blended from the corners (16-step Gouraud); grid points summed in a fixed corner order so shared edges land on the same pixels and cancel. `M8DD_TRIS_MAX` 1024 -> 2048 (~300 KB more DLL data: trim later) and `M8D3D_JOIN_EDGES` 6144. The per-quad sampler of step 1 is removed in this patch.

**Next actions, in order:**
1. Card state as above; install `M8D3EO` (`tools/m8cardscripts/install_h.ps1 -Dir <build> -Name M8D3EO -Run eo`), tunnel with `tunnel_card.ps1 -Settle 90 -Gap 60` while the owner watches. If it hangs before full screen (white window, egg timer), that is the DirectX-side hang seen at boot 279, not the build: rerun once at normal priority.
2. Owner's eye against the reference: stripes as rectangles, symbols visible, light grading across walls. Then the cost; if writes or ours rise a lot, try `M8D3D_TESS` by size only where neighbours agree (a shared edge must have the same subdivision both sides, else XOR streaks).
3. Smoothing the 16 light steps: S7 pattern fills (8 x 8 PATT_DATA at plain-fill cost) dithering between level n and n + 1; owner: pick the pattern per cell from an aperiodic (Penrose-like) sequence so the repeat never shows, "same but different for free"; owner: a small designed patch reused everywhere ("crunch anything repeating").
4. Speed: join by wall colour with the light added in the fill (Z2/Z3; the palette's light is the low 4 bits); Z6 low-res modes; third page (Z7). The 7 shade colours were spread over the whole screen, so bands alone did not cut fill time (~135 ms of fill pixels at `M8D3EL`).
5. Texture copy (Z1) is 1:1 only and 7-bit: use it for sprites, far walls and characters, not scaled walls.
6. DirectXT is D14 in `docs/mach8_frame_rate_plan.md`: (a) profile D3DIM's share of the ~100 ms outside our driver. FPU fitted (387 class): float 2.97 us, fixed 1.53 us a multiply-add.

Owner's direction (memory): 60 fps stays the goal, estimates are waypoints; DX 1-7 titles playable; changing DirectX itself allowed; delegate docs, GitHub and research to cheaper-model agents.

## START HERE: 2026-11-08, midday

Card on `M8D3EL` (diagnostic counters; the committed source is the same less them), boot 283. Tunnel, normal priority, agent on: every frame joined (was ~half), 4,400 writes a frame (7,900), engine 166 ms, frame 346-355 ms, ~2.8 flips a second. Owner's eye: no holes, no flicker, flat colour blocks, still slow. Causes found: corners at x = 640 / y = 480 are legal; fallback frames overlapped by 3-9 % of the screen (no Z, painted far to near), now the leading run up to one screen is joined and the rest drawn singly on top. Fills by 32-row bands barely help: ~135 ms of the engine time is still fill pixels, the 7 shades seem spread over the whole screen. One hang (boot 279) froze at the white window before full screen, so in DirectX or the mode set, not the fill. FPU fitted (387 class): `M8FPU` float 2.97 us, fixed 1.53 us. **Next (owner: looks first, "as the developer intended"): software-renderer reference picture of the tunnel; D7 palette as hue x shade with the light added in the fill (Z2, Z3), joining by wall not shade; then the texture (Z1). DirectXT is D14 in the plan.**

## START HERE: 2026-11-08, 00:30

Card on `M8D3EH` (joined path), boot 277. **The bed cannot test polygon fills** (known quirk, 11-02): `tools/m8probe/M8JOIN` fills a two-triangle quad five ways; bed reads 0 in every case, card passes all five, including both joined scratch layouts. Tunnel on the card, high, agent on: writes 7,881 a frame (18,621 per triangle), engine 220 ms (319), frame 439 ms, ~2.3 fps; about 70 % of frames joined (~3,400 writes), the rest fell back, most likely corners off screen. Fill time now rules: each colour's box is near the screen (rings) at ~69 ns/px. **Next: (1) take off-screen corners into the joined path (clip like the seam: x < 0 carried to x = 0, rows cut); (2) fill boxes by bands, a colour's extent in each band (split where it saves > ~1,700 px); (3) skip the erase when the frame covers the screen.** Owner's look at the joined tunnel on the card: solid patches, no flicker, about a frame a second by eye (measured 378 ms a frame, ~2.6 fps).

The "after midnight" section below is superseded: its failed fills were the bed.

## START HERE: 2026-11-08, after midnight

Joined path written, uncommitted (`m8d3d.c`, `m8d3d_join*`, ~390 lines; `git diff`). Bed `vm_5160_mcp` (86Box MCP, windowed) on diagnostic builds; `install_h.ps1 -Target 127.0.0.1` installs there. Results on the bed (`M8D3EH`): 3,439 writes a Flip against 18,621, 12 fills a frame, outlines exactly the patch boundaries; **fills do not show** in either half (`M8D3EK` swapped the halves' rows: no change). `M8D3EL` (scratch planes painted on screen instead of the polygon fill) shows the boundaries, so the scratch looks right and the polygon fill over a patch is what fails, though the same register sequence fills one triangle. Emulator checked: write mask honoured, scissors open, poly parity reset per command. **Next: a probe that fills a two-triangle patch both ways (per-triangle and joined layout), bed then card, pixels read back.** Card untouched since `M8D3EG`.

## START HERE: 2026-11-07, later night

Card on `M8D3EG`, boot 276. Owner: the 5160 froze during a boot with a looping sound (the boot before 275), restarted by hand: another instance of the open hang. Owner's idea, taken up: draw the frame's triangles as joined patches, not one at a time; shared edges are drawn once or not at all. Census per frame on the tunnel (high, agent on): ~365 triangles, 264 share an edge with one of the 4 before in the same colour (72 %), 340 in any colour, 7.7 colours, sizes by smaller side 1-4: 8, 5-16: 122, 17-64: 189, 65+: 46; 18,621 writes. **Next: the joined path**: per colour, the boundary edges (an edge used twice in one colour is inner, skipped) into one scratch plane each, one polygon fill per colour, outer rings first, one outline; scratch as two 320-wide halves in the 640-1023 column, taking the mask rows. Estimate 1,500-2,000 writes a frame; fill time then rules (69 ns/px over each colour's box, ~40-60 ms). Bed for pixels first. Zigzag polyline (one vertex a row) only pays at 5-16 rows: ~2,400 writes. Looks: shading by palette ramps (D7) keeps the patch count low; textures cut joining.

## START HERE: 2026-11-07, night

Card on `M8D3ED`, boot 272. The slow Flip was not a clear: the tunnel never clears ("we redraw over every pixel"), so its triangles piled up across frames, passed `M8DD_TRIS_MAX` and went to RAM. `m8dd_fresh` now starts a flipped-away back buffer from black at its first draw. Tunnel `M8TUNG`, normal priority, agent on: flips with boxes 0, ours 89 ms a frame (was 560-810), engine 318 ms a frame for ~540 triangles, about 2 fps by Flips, 1053 triangles a second (was 240-340). **Next: the engine's 0.6 ms a triangle**, 10x the bench; the tunnel's lit corners take the stamped/banded gradient path in `m8d3d_replay`, and every frame erases ~540 old boxes before drawing. `M8D3EF` counts engine writes: high priority, agent on, 16,242 writes and 334 pieces a frame (~48 writes a piece, ~124 ms of bus), engine 277 ms, frame 407 ms, 2.5 fps by Flips. Flat-only build (`M8D3EE`, diagnostic, not committed) took the same engine time: gradients are not the cost. Register shadowing buys 1-2 writes a piece, not worth it. 60 fps is ~2,200 writes a frame. Levers in order: adaptive palette from the texture's colours (looks, no writes); Z1 textures (same writes as flat); quads as one 4-edge fill; fewer splits (wider scratch); skip the box erase when the frame covers the screen. `tunnel_card.ps1 -High` now works (M8HIGH needs the full path).

## 2026-11-07, late (superseded: the "partial colour fill" below was wrong)

Card on `M8D3EC`. SDK tunnel baseline on the card, normal priority, agent on: about 0.3-0.7 fps, 240-340 triangles a second (`tools/m8cardscripts/tunnel_card.ps1`, copy `M8TUNF`/`M8TUNG` made by `tools/m8d3dtest/tunnel_full_patch.py`, G also turns its fps text off at file 0xCA48). Census stage timers show why: every Flip takes the slow path (`flipwhy`: boxes 22 of 23 flips). The sample clears with a colour fill that is not the whole buffer as one rectangle, so `m8dd_blt16_do` fills RAM and marks it dirty, and Flip sends it over the bus: ~1.2 s a frame. **Next: describe partial colour fills on the back buffer instead of filling RAM** (m8dd32.c, the `back` branch near "A clear of the whole back buffer"), then rerun. Also open: our primitive time 560-810 ms a frame at ~750 triangles, 10x the bench per triangle; GetDC text overlays force the slow path (`M8D3EB` fixed only the later frames). `M8D3EA` draws textured triangles flat (no gain on its own); `m8d3d_flip_why` counters are diagnostic, remove when done.

## START HERE: 2026-11-07, evening

**The plan is D1-D12 in `docs/mach8_frame_rate_plan.md`, "Direct3D: playable approximations".** Worklist X10. Bed on `M8D3E7`: the unmodified SDK tunnel runs on the HAL full screen, reached by F2, at 0.11-0.16 fps; find why first. Card on `M8D3E2`, its bench hung; no card figures since `M8D3E1`. Results in `docs/captures/2026-11-07_silicon/README.md`. Programs fetched to ignored folders: `references/dx61samples`, `references/dx7sdk`, `references/d3d_programs`. Run them by `tools/m8cardscripts/sample.ps1`. Bed first, one thing at a time: a tunnel started over a running script spoiled a run.

## START HERE: 2026-11-07, afternoon

- Card on `M8D3DY` pageable, boot 258.
- M8TEX results in `docs/captures/2026-11-07_silicon/README.md`.
- Locked ran slower than pageable.
- Textured triangle from the atlas 1.05x flat at 100 px.
- Engine steps a source along spans and lines.
- Next is the row and line texture probe, then the HAL's texture formats and handles.
- Desktop bench of `M8D3DY` (ellipse, polygon) still to run.

## START HERE: 2026-11-07, midday

**The curve test is done: two hangs in three runs, cause unknown; card on `M8D3DX` locked, boot 257.** Results: `docs/captures/2026-11-07_curves/README.md`. Levers Y16-Y19.

1. The curve test is done (results in `docs/captures/2026-11-07_curves/README.md`): two hangs in three runs, cause unknown, card on `M8D3DX` locked, boot 257.
2. `tools/m8cardscripts/cycle.ps1 -Name M8D3DY -Label ya -Runs 2`: pageable, `M8D3DF` twice: the locked-against-pageable A/B. Then `tools/m8_card_cycle.ps1` bench twice: ellipse, polygon, lines, rubber_band against `card_xb1`, pixel check by FB16CHK. `install_h.ps1` rewrites `NOAGPAGE.LST` with the run's label.
3. Owner's ghost test (below) is still to do. Locked ran about 25 % slower in `hal_many`, so even if locked cures ghosts it has a price.

Bed `vm_5160_mcp` left running on `M8D3DY`.

## START HERE: tomorrow, 2026-11-07

**Staged on the CF in the reader, 00:50, every file checked by md5.** The owner asked for the card staged; agent on at the next boot (`V9xRemoteAgent` is in SYSTEM.DAT; a raw search, so confirm by `info`).

| On the card | What | md5 |
|---|---|---|
| `C:\WINDOWS\SYSTEM\M8D3DX.DRV` **active** | **Locked 16 bpp buffers (`GMEM_FIXED`), else `M8D3DX`.** Built from HEAD with `m8_fb16_pageable` patched; not in the tree | ED527782AC11383BEFAD2C9EEBF4FAA3 |
| `M8D3DX.PGB` | The pageable build it replaced: the way back, rename over it | A4F3BA38371F133AC2E7DB302B2161DC |
| `M8DD32.DLL` | Unchanged, built with `M8D3DX` | 1349FFF45CF5E3914BE8FB1FD97062BD |
| `C:\SWAPCOMP.VXD`, `XTIDEMP.MPD` | Unchanged, live | 9FDE4934, A67FEFC4 |
| `C:\V9XREMOTE\STAGE\` | **Not active:** `SWAPZF.VXD`, `SWAPZ.VXD`, `XTBOTH.MPD`, originals `SWAPCOMP.ORG`, `XTIDEMP.ORG` | C2585E38, 1CE747ED, DAEF6830 |

The locked build's file is still named `M8D3DX`: the registry needs no change. **`FB16CHK`'s overlap counter stays in `fb16.c`.** The owner took another CF backup before this, and the image is the way back for everything.

### Order

1. **Owner:** eject the CF, into the 5160, boot. Confirm the desktop is fine.
2. **Remote:** `info`; md5 of `M8D3DX.DRV`. `M8D3DF` at high once, `grab` once if pixels are in question.
3. **Ghost and paging test, locked against pageable (one change):** owner runs `AGENTOFF`, restart, `NOAGPAGE` (label `ya`, now closes PERFLOG by `WINCLOSE PERFLOG`; check it takes), watches for ghosts with several programs open. If ghosts: F5, drag a window across, say which cleared. Compare with `docs/captures/2026-11-07_xa/README.md`: page-outs 2,815 locked and old build, 4,889 and 5,817 pageable. **If it hangs or ghosts persist on locked, the pageable change is not the cause: say so and record it.**
4. **Keep whichever wins:** locked needs `m8_fb16_pageable` changed in `fb16.c` and a commit; it gives back the 892 KB. Then delete the counter once the ghost question is answered.
5. **SWAPCOMP and XT-CF on the card, one at a time, agent on, restart between, A B A B, deltas only:** `SWAPZF` over `C:\SWAPCOMP.VXD` by WININIT.INI (it is in use); `XTBOTH.MPD` over `XTIDEMP.MPD` likewise; `XTSECT` x3 and `PAGETIME`. Bed passed both: `SWAPZFC` 25,016 reads 0 checksum errors; `both` file hashes equal the original. `XTSECT` fails in the bed on either driver. Zero pages are under 1 % of paging; `SWAPZ` alone is not worth a run.

### What the owner reported on `M8D3DX`, agent off

Faster to the eye. Ghosts again with several programs open. Script needed closing by hand. Gradient triangles were gradients but large bands, few a triangle: "multi colour" rather than fine. **dxdiag's Direct3D button hung, sound looping, agent off.** After a reboot dxdiag ran well: "colours better than before and no blockiness", good frame rate; software rendering minutes. Owner videoed it all: location not given, ask. Owner's questions: is RAM not flushed (doubted: the cache is write-through; not proven), or OSR1 and heavy virtual memory (open).

### Then, graphics

**C2 adaptive palette**, the owner's fine-gradient complaint: bands are limited by the fixed 5 x 9 x 5 palette as well as by count. Then more bands for stamps, whose cost is once only; Y10 stored ramps; batched edges for `hal_turn100`; small tests 2-5 fps down from `M8D3DD` agent-free, unexplained. #55's hang needs a way to see where it stops: ideas: a breadcrumb written to off-screen card memory, read after reset.

Benchmarks and games: `docs/d3d_benchmarks_and_games.md`. Fps agent-free: `docs/captures/2026-11-07_xa/README.md`. Owner's rule tonight: when anything stops or needs attention, wait for them; they check periodically.

## Card

`m8d3dx.drv` (see above: locked build under that name), 640x480 at 72 Hz for Direct3D, agent on at next boot. Way back: `C:\V9XREMOTE\REG\M8D3DD.REG` and `C:\V9XREMOTE\REG\M8DD32.DD` (rename in by WININIT.INI). The 5160 is in the cellar, the owner on the ground floor: batch trips, say exactly what is needed.

**Scripts: `tools/m8cardscripts/`:** `cycle.ps1 -Name M8D3Dx -Label x -Runs 1` builds, installs under its own name, restarts through Start > Shut Down and benches; `bench.ps1 -Label x` benches what is running; `shape.ps1 -Label x` runs M8SHAPE; `grab_run.ps1 -Label x -Out dir` makes pictures from the card; `restart.ps1`; `bed_swap.ps1`, `bed_xt.ps1`. A PowerShell line with a `.EXE` destination or a regex can trip the safety check: use a script file. Long patches: a Python file, not a heredoc. Restart keystrokes sometimes miss: check the boot counter before calling it a hang.

## Figures to beat (high priority, agent on, comparable with each other)

| fps | Morning `h1hi` | `M8D3DX` (one run; others in the capture README) |
|---|---|---|
| hal_tiny, last | 67.4 | 68.5 |
| hal_small | 55.2 | 70.2 |
| hal_large | 33.6 | 41-51 |
| hal_many | 21.6 | 41.2 |
| hal_turn32 | 48.6 | 68.6 |
| hal_turn100 | 35.7 | 33.3 |
| hal_grad, real corner colours since `M8D3DM` | - | 42.2 |

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
- SWAPCOMP and XT-CF: build options in the tree, staged, not active: step 5 above. File cache sweep still to try. Test by restart, same order, two runs, deltas only, alternating builds (owner: memory varies).
- Bed `vm_5160_mcp`: on `M8D3DH`, `SWAPZFC`, original XT-CF, its boot menu stops at times for Enter, cause not known. Originals `C:\SWAPCOMP.BAK`, `C:\XTIDEMP.BAK`. Left running or closed: see the last line of this file.

## Order of levers

**Done:** Y1 start frame 2 early measured, little to gain with two pages (the plan's row says why). Y14 in for mask and stamp copies: `hal_many` 41 fps. Runtime flags: DONOTCLIP and DONOTUPDATEEXTENTS save ~5.4 ms at 64 triangles, for our own programs.

1. Y14 done: replay writes queue, large commands sent at once. GDI ruled out by reading: it already queues. `M8BENCH` on `M8D3DX`: pixels = `noag1` in all 36, speed 1.0x like for like. A DirectDraw sprite's `m8dd_key_rect` has not been looked at.
1a. Pointer on page 0 only (white mark in every capture): one page in two would lack it. Owner's eye, then show it on both.
1b. Batched edges for changing shapes: `hal_turn100` is still no better than the morning.
2. **Y10 stored ramps (owner: what is true everywhere):** 1.09x flat at 100 px against stamps only when colours repeat. Needs fields by direction and rate, palette in shades.
3. **DirectX's own time:** hal_many `d3d` 9.5 ms, ours ~3 of it: V13, V1b, then our per-triangle arithmetic (`M8D3DF` column `ours in d3d`).
4. **Y13 background over foreground, Y11 mirrored masks, Y12 golden-ratio band shift** (owner). Y2 third page, Y6 joined quads for dxdiag, Y8 low-res modes.
5. **Reuse (owner, after each win):** masks, core rectangles and stamps for M8CUBE's polygon escape (`poly3d.c`), DirectDraw sprites, GDI's mono expansion. Then textures: stamps are the path for TX1.
6. **60 Hz** retest once hal_large's work is under 16.67 ms: row in the capture README.

Ideas the owner has thrown out have paid repeatedly: core rectangle came from "think outside the box", stamps from "paint once, copy". Record tangents in the plan's Y table rather than dropping them.

## Benchmarks and games

`docs/d3d_benchmarks_and_games.md`, research with sources: 3DMark99, 3DMark2000, likely Final Reality will not start (Pentium 166, 32-64 MB against a 486 at 83.5 MHz and 5 MB). DX7 games are not realistic. First: DirectX SDK samples, Quake timedemo software mode, Diablo, Warcraft II or Age of Empires, Hellbender.
