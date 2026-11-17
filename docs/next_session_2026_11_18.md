# Next session, after 2026-11-17

Earlier: `docs/next_session_2026_11_17.md`. Newest wins.

## START HERE (end of 2026-11-17)

Plan: worklist section **P**, tracker **`docs/playable_3d_plan.md`** (P1-P7, learnings table **P1-S, S1-S9**).

**State.** Card (5160): boot 430, desktop **800x600 16 bpp**, driver `m8rhs.drv` + DLL = `a2f862c`/`92add43` source,
idle. Way back to the 8 bpp desktop: ``C:\V9XREMOTE\REG\DISP8.REG` + restart. Beds `vm_5160_dbg` (port 9891) and `vm_5160_mtm`
(9892, now 800x600x16) shut down cleanly; restart with `tools/bed_launch.ps1 -Automation <port>`.

**Outcomes 11-17.**
- flip2d 0.9 -> **4.33 fps** on the card (store-free mask build, imul-free hash, spans at Flip, partial held clear).
- twist **6.73 fps** at 16 bpp (was ~5.7); ~118 ms of its 149 ms frame is twist + Direct3D geometry, not ours.
- Regression P1.13 passed: wormhole, donuts (now reaches its title), stretch draw right on the bed and run on the card.
- Page-ins: ~80 % outside our driver (flip2d, DirectDraw, GDI on full 5 MB); absent under DOS.
- Lessons (plan P1-S): stores cross the slow bus (write-through L1); no imul in hashes; never rewrite what is right;
  cheapest shape per run; one read per row; count before theorising; locked RAM is the budget; FIFO batching;
  static art stays on the card.
- MTM measured: Locked cockpit art 124 colour runs a row, spans no rescue; too heavy for DirectX on 5 MB (owner);
  try with dashboard off (P2.9); static art on the card goes into the DOS core (P4.5, S9).
- DOS 3D reading (`docs/p5_api_reading_2026_11_17.md`): every API reduces to spans/trapezoids; compatibility layer
  (VBEAF.DRV, GLIDE2X.OVL, BRender .BDD) over one core is the aim.
- Startup hang boot 429 (sound looping, arrow+hourglass), power cycle fixed it; noted, not chased.

**Next, in order.**
1. P1.10 fused asm row pass (hash + extents + spans, no stores) - card back to 8 bpp first.
2. S7b free the 635 KB locked 8 bpp shadow when DirectDraw ends.
3. P2.9 MTM dashboard off, one bed run.
4. Open P1-S rows (S1d GDI caches, S2b 16 bpp detection, S3, S5, S9 design), then P1.11 faces as polygons.
5. `dist/mach8` still `32bfaac`: refresh after the above pass the card (G10, owner verifies).


**Plan from 11-17 evening: worklist section P, tracker `docs/playable_3d_plan.md`.** Start with P1.1 (asm mask build).

## State

- **Card (5160):** boot 409, desktop **640x480 at 256 colours** (owner set it for the mode-change test; Win95 asked
  for a restart). Put back to 800x600 16 bpp when DirectDraw testing ends. Driver `m8nof.drv`. `C:\M8CENSUS.EXE`
  is the build with the `8bpp` line (`7DFBB0DF`).
- **Bed `vm_5160_dbg`:** running on the normal 86Box (`86box_auto\build`), current driver installed as `M8REAL`
  (DLL md5 `B1D80597`), `C:\M8D3D.INI` has `trace=1`. gdb build in `86box_auto\build_gdb` (MINGW64, automation on,
  `-mwindows`); the first attempt is kept as `build_gdb_ucrt_bad`.
- **#10:** PICCHK sent to Simon as a GitHub link (`3355e1c`); 5160 control clean. Waiting on his `C:\PICCHK.TXT`.

## Found 2026-11-17

| item | result |
|---|---|
| RaiseException entry | `BFF9A013`, not `BFF9A00E` (doc fixed, `ad5c5c2`); gdb `hbreak` proven by `tools/raisetst` |
| Blue screen trigger | flip2d at 8 bpp from a 16 bpp desktop, then PERFLOG started: PERFLOG GPF in KRNL386 `0001:49DB` (**AllocSelectorArray**), then `0E at BFF9A07C`. Desktop pre-set to 640x480x8 (no mode change): flip2d -> PERFLOG **3/3 clean**. Theory: a mode-change path in our 16-bit driver damages the LDT free list; the next program to allocate selectors faults. The agent is the victim (it starts the next program). |
| Bed page flips | 86Box's Mach8 model stores `CRT_OFFSET` (2EEE) but applies it only at a mode recalc: the bed never flips 8 bpp pages (odd frames only). Card flips correctly (counter +1 each frame). The 11-15 "flip2d 2.5x on the bed" claim is void. |
| flip2d on the card | <1 fps. 30 s run: 27 flips, 55 Locks, **28 read the whole page back** (307 KB each; bed: 0 of 1,269), ~98 KB sent per Unlock (cube ~36 KB), transfers 30.4 s of 30 s, ~2,900 page-ins per run. `docs/captures/2026-11-17_flip2d_card/` |
| Shape reuse (owner's idea) | `tools/flipshapes/flipshapes.py`: one axis, 7.18 deg/frame, exact frame repeat every 304 frames, 84 % reuse over 20,000 frames but 0 % inside a cycle; ~3 MB of masks a cycle. 1-bit mask sending pays (8x); caching pays for sprites/text/HUD, not this cube. |

## Later on 2026-11-17 (owner out, authorised)

| item | result |
|---|---|
| Agent redraws | M8EPOCH: agent 0.12's tray icon flashes every 250 ms while busy (53 redraws in 20 s, each one read back by our driver); agent off: 1. |
| Agent fixed | Michael Dale's V9x-Remote-Agent 0.17.0 plus `[agent] tray_flash=0` (icon still; refresh only on an address change), branch `inboard-tray-flash` in `RiderProjects\V9x-Remote-Agent` (outside this repo). Bed and 5160 updated (build `inboard-trayflash`); 5160 idle probe now 1 copy in 20 s. The old 0.12 host `v9xctl.ps1` still works. Owner to tell Michael (draft owed). |
| Read-back cause | Not the agent, not the feed, not GDI: the card ran `32bfaac`, before `760f004` (fills keep the Lock shadow). |
| Card driver | Now `m8head.drv` + DLL `B1D80597` = `3aaea3a` (boot 413). Way back: `M8NOF.DRV` still in SYSTEM, old DLL `C:\V9XREMOTE\REG\M8DD32.NOF`. Desktop still 640x480x8. `dist/mach8` stays `32bfaac` until a fuller card run (bench). |
| flip2d on the card | `32bfaac`: 27 flips in 30 s, 28 of 55 Locks read back, ~98 KB per Unlock. `3aaea3a`: **50 flips**, 1 of 102 read back, ~52 KB per Unlock, transfers 13 s of 30. Next: X17 item 1 (deferred clear). |
| Bed | `vm_5160_dbg` now at 640x480x8 (flip2d there: 52 of 53 Locks no read). X22 model fix: analysis in the worklist, not edited (upstream code). |

**End of 11-17:** card driver `m8dfr.drv` (DLL `A254C4DF`) = `1167bdb`, the deferred clear (X17 item 1): flip2d
2.2 fps, ~74 KB a frame, no read-backs (`docs/captures/2026-11-17_dfr_card/`). Way back: `m8head.drv` (`3aaea3a`)
and `m8nof.drv` (`32bfaac`, DLL `C:\V9XREMOTE\REG\M8DD32.NOF`) still in SYSTEM. Bed `vm_5160_dbg` on the same build.
**Later still:** masks built (off by default, `[ddraw] masks=1`), flush timers in census slots 96-103 (`m8d3d_stat` now 112), Lock skips hashing a held page. Card `m8nohsh.drv` (boot 418): flip2d **2.8 fps**. Black dark face = flip2d's own shading (shade clamped at 0), not ours. **Next: one-pass mask build in asm, row hash/extent scan in asm, then the feeder** - measurements in `docs/captures/2026-11-17_mask_card/`.

## Tasks, in order (Opus; commits by Haiku; raise any long run with the owner first)

1. **Why half of flip2d's Locks read back on the card.** `m8dd_epoch()` hashes the driver's GDI counters
   (escape 4D41h); a counter moving each frame (suspect `cur_hide`: the card's real cursor) or a failed escape
   (falls back to GetTickCount) forces the read. Probe `tools/m8epoch/M8EPOCH.EXE 30` alongside flip2d on the card
   names it (owner present). Fix: count only what changes pixels where the shadow claims to match.
2. **Unlock sends ~98 KB, not the cube.** After 1, check the span narrowing on the card; then X17 (deferred clear,
   1-bit masks, recolour in place).
3. **Paging:** hash only the locked rows; shrink the 8 bpp shadow to the two pages.
4. **Mode-change selector damage:** read every Disable/ReEnable/`m8_screen_done` path for a selector used after
   free or freed twice; a 16-bit probe that allocates/frees selectors after a mode change makes it deterministic.
   Until fixed, run DirectDraw tests with the desktop pre-set to the program's mode.
5. **Owner's mode plan:** per-program mode table (launcher) or a driver pop-up asking for the mode; depth change
   needs a restart on Win95 retail.
6. **86Box model:** apply `CRT_OFFSET` writes at the next frame (and check the `ge_offset -= crt_offset` line);
   upstream-able. Owner wants it fixed.

## Not done today

- Fable leftovers (F2 4, F3 4-5, XT-IDE 1 and 7) - untouched.
- BRender `.BDD` (X16) - research doc only (`docs/x16_dos3d_api_sourcing_2026_11_17.md`).
- Paging during a Direct3D demo (RAM-compression question) - not measured.

## End of 2026-11-17 (evening)

Plan: worklist section P, tracker `docs/playable_3d_plan.md` (P1.7-P1.13 added today).

| item | result |
|---|---|
| Card | `m8pcl.drv` + DLL = `3768ba1` (boot 427), desktop 640x480x8, `C:\M8D3D.INI` `[ddraw] masks=1` (spans default on). Way back: `m8r1117.drv` still in SYSTEM |
| flip2d | 2.8 -> **4.33 fps**: store-free mask build, imul-free hash, spans at Flip, partial held clear |
| Lessons | the L1 writes through to a slow bus: no stores in inner loops; ~80 % of page-ins are outside our driver (flip2d, DDraw, GDI on full 5 MB); GMEM_FIXED from a DLL is already page-locked (pin test changed nothing) |
| Tools | `tools/m8cardscripts/flip2d_cycle.ps1` (card: install, restart, run, PERFLOG), `flip2d_bed_shots.ps1` (bed pixels) |
| Reading | `docs/p5_api_reading_2026_11_17.md`: BRender, VBE/AF, Glide ranked; every API reduces to spans/trapezoids |
| Owner | compatibility layer for DOS 3D titles (VBEAF.DRV, GLIDE2X.OVL, .BDD) is the aim, in the plan |

Owner, end of 11-17: the learnings must be carried across the whole driver - plan table **P1-S (S1-S8)**; each session ticks rows.
Next, in order: **P1.13** (regression across DDraw samples and twist, then spread the lessons), **P1.10** (fused asm row pass),
**P1.11** (faces as engine polygons). `dist/mach8` still `32bfaac`: refresh only after P1.13 passes.

**Later 11-17:** P1.13 regression passed (DDraw samples on bed and card, twist 6.73 fps). Card desktop now
**800x600 16 bpp** (boot 428, registry; way back to 640x480x8: `C:\V9XREMOTE\REG\DISP8.REG` + restart). 8 bpp
DirectDraw tests need the desktop at 8 bpp first (X19). Next: P1-S rows S1-S5, S7, then P1.10.

**Card hang, boot 429 (11-17 evening):** after an agent restart into `m8rhs.drv` (overlay hash only changed), startup stopped at a blue desktop with the arrow+hourglass cursor and the startup sound looping; owner power-cycled, boot 430 fine (twist 6.64 fps), startup tone played ~4 min late. Same symptom as 11-05 (`docs/captures/2026-11-05_baselines/README.md`). Not chased: two observations, no cause.

**MTM measured (11-17 late, bed `vm_5160_mtm`, today's build at 800x600x16, `[diag] runs=1`):** race reached
by agent clicks (title, Races, Circuit, RACE); cockpit view on our HAL. 10 frames: 124 colour runs a row, 30 % of rows
<= 4 runs; 12 flips in ~4 min (bed). Spans do not rescue it (plan S4a); static art on the card is the lever (S9).
`docs/captures/2026-11-17_mtm_runs_bed/`. MTM bed left at 800x600x16, `lores=0`, INI `runs=1 trace=0`; shut down.
Bed `vm_5160_dbg` was shut down for it (restart with `tools/bed_launch.ps1 -Automation 9891`).
