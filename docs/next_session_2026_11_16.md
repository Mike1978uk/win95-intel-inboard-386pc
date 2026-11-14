# Next session, after 2026-11-14

Earlier: `docs/next_session_2026_11_15.md` (its last two sections are this session's log). Newest wins.

## Start here: the Mach8 card run, driven through the agent

Opus drives every step over the V9x agent (`v9xctl.ps1 -Host 192.168.68.62`), Alt+F4 included
(`input`/key verb; the earlier exit hangs were agent-driven too). The owner is needed only to
restart a hung card: keep them reachable, and stage the next driver before each risky run.

Nothing since `7e7095e` is pushed. Five local commits wait on this run: F3 1-3 (`6847977`,
`6ccc10a`), XT-CF first read wait (`2e17fc5`, already card-proven on boot 389), bed bench (`7bca168`),
F2 3 (`ffe9df0`). Push all of them once step 1 passes; drop or revert what fails.

1. **F3 build** (findings 1-3: idle wait before a mode set, the DLL resets a wedged engine, compare
   off when GDI cuts the feed). Rebuild from `6ccc10a`'s tree or use `%TEMP%\m8f3` if it survived.
   `tools\m8_card_cycle.ps1 -Drv <M8DISP.DRV> -Name M8F3 -Label card_f3`, bench **twice**, prove idle.
   Pass: pixels = ATI in all 35, no engine resets. Then **GD tunnel, feed on, Alt+F4 sent by the
   agent, three times**: the exit that hung. Three clean exits = F3 finding 1 confirmed as the hang's cause; write it up
   and close the loop on #55. A hang: owner restarts, then `-FetchOnly`.
2. **F2 3 build** (`ffe9df0`): twist, `m8d3d_stat[25]` (writes sent) per flip, old against new.
   Expect up to 112 fewer writes, ~0.9 ms a 32-colour frame.
3. **F2 2** is in every build: twist with `C:\M8D3D.INI [diag] phase=1` then `phase=0`, fps, twice.
   The gap is what the phase marks cost; the 22.6 ms split was measured with them on, so re-split
   with `phase=1` once and keep it as the new baseline.
4. Update `docs/5160_tuned_config.md`: card now runs SWAPZFN `99B7BD5B` and XT-CF `9C64B4D1`
   (rows still name the morning's `07154EB7` / `A3F3EEAD`).

## Then, in order (Opus + bed, card batched)

5. **F2 1**: cancel edges one colour at a time (`m8d3d.c` ~2981-3077), the largest CPU win, ~3 ms
   of the 6.5 ms seam prep. Bed pixels, then card fps. Diff in `docs/fable_review_f2.md`.
6. **F2 4**: count `m8d3d_join` calls per flip first; patch only if > 1.
7. **F3 4-5**: the trace probe (process ids on destroy lines) before the finding 4 patch; read
   `m8dd_mode_changed` before the finding 5 patch.
8. **swapcomp CPU items** (Fable Part 2, 5-7): 7-sector pages compressed, early give-up, compress
   loop. **Each must fit in the 84 bytes left** in SWAPZFN's three locked pages
   (`python tools/le_census.py drivers/swapcomp/build/SWAPZFN.VXD`), or it costs the 4 KB back.
9. `dist/` swapcomp and XT-CF after the owner's 45-minute PAGETIME soak.

## Next Fable job (only after step 1's result)

- If the exit hang is gone: **F4 = the M8CURVE hang** (the card has hung there twice, once on a
  fresh boot with no Direct3D state). F3 found no path in its lines and named where to read: the
  curve and one-pixel paths `m8hw.c` ~1790-2060 (`m8_room_for(7)` to `(15)`) and the interrupt-time
  pointer `screen.c:1503`, `:1678`. Sonnet pack <= 600 lines, **committed before launch**.
- If the exit hang remains: F4 is the exit path again, with the step 1 capture in the pack.
- After that, a re-split of the frame build (step 3) decides whether F2's remaining CPU deserves a
  second performance pass. Not before.

## Owner jobs, no Opus time needed

- `C:\PICCHK.COM` from a DOS boot (SAFE), then `PICCHK /T` (must show `mask_err=65535`); then the
  #10 message to Simon with the program (draft on request).
- Replies owed on #50 and #47 (drafts in `docs/issue_audit_2026_11_14.md`); README open-issues list
  is stale (lists closed #23, #28, #29, #31, #35, #45; omits #50-#59; says 1024x768x256).
- Other CF cards: if any, a host 4 KB random-write test picks a faster commit than the Transcend's
  ~110 ms (writes are CF-bound; reads are bus-bound).

## Learned this session

- **Fable: pack committed before launch.** F2 (pack committed) cost ~118k tokens, 7 calls, 5.4 min.
  F3 (pack not in the worktree's commit) ~147k, 52 calls, 8.6 min. Same contract otherwise: one job,
  pack <= 1,500 lines, <= 5 findings with diffs, report only, 20-minute stop. Both were worth it.
- **Loop-count timeouts drift with the CPU.** The XT-CF waits assumed 220 ns a pass; the card does
  100 ns (`tools/loopcal`). Any driver that times with a loop count needs the same check.
- **`VMM\cpgLocked` cannot show a 4 KB change.** It moves 1,628-1,800 KB within one boot and counts
  the disk cache only in part. Page savings rest on `tools/le_census.py`.
- **The Mach8 bed loads `M8D3GG.DRV`**, not `M8DISP.DRV` (`SYSTEM.INI`); a write to the wrong name
  benches the old driver. Check the bench header's `driver` field.
- **Bed and card both go through `v9xctl.ps1`** (`C:\Users\lycet\tools\v9x-remote-agent\host\
  v9xremote-0.12.0\scripts\`): card `-Host 192.168.68.62`, page bed `-Host 127.0.0.1 -Port 9870`,
  Mach8 bed `-Port 9869`. The MCP servers point only at local ports.
- **Dword I/O to the XT-CF is impossible**: base+2/3 is the error register at stride 2. DMA is the
  only route past ~3 us a byte and this XT-CF has none.
- Measured, card: XTSECT one-sector read ~12 ms, of which CF 0.24 and driver ~0.04: ~9.5 ms per
  command is Windows' disk path or a timer tick. Unexplained; a trace before any theory.

## Research written, not yet acted on

`docs/le_census_2026_11_14.md`, `docs/issue_audit_2026_11_14.md`, `docs/benchmark_choice_2026_11_14.md`
(Doom timedemo, 3DBench, SpeedSys), `docs/showcase_3d_candidates_2026_11_14.md` (worklist X16: CGL's
later games load replaceable files; BRender is MIT), worklist X4 (wedged-drive recovery, bed first).
