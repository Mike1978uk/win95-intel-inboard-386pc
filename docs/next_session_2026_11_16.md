# Next session, after 2026-11-14

Earlier: `docs/next_session_2026_11_15.md` (its last two sections are this session's log). Newest wins.

## Result, 2026-11-14 evening: step 1 passed

Card boot 390 on `m8f3.drv` (`%TEMP%\m8f3`). Bench twice: pixels = GX (`card_gx1,2`) in all 36,
geometric mean 1.045 (run 2's bmp_card/dstinvert dip absent in run 1: noise). The bench against
ATI runs at 8 bpp and cannot gate a 16 bpp card: compare with `-Against 'card_gx1,card_gx2'`.
Tunnel `M8TUNG`, feed on, agent Alt+F4: **3 of 3 clean**, same boot, desktop back at 800x600,
`M8TRACE.TXT` shows every surface destroyed; a bench after the three exits: pixels same in 36,
0.978. ~7.3-7.9 fps, 5,530 writes, 74 ms ours a flip. `v9xctl screenshot` fails on the card
(Win32 1460). #55 is the Windows restart hang, not this exit: still open, restart untested on F3.

**The exit hang is not fixed.** Boot 391, F2 3 build (`m8f2n3.drv`): twist `phase=0` exited clean,
then twist `phase=1` hung at exit. Census `p1a-a`/`p1a-b` were both read and `M8TRACE.TXT` shows
every surface destroyed: the hang is after the Direct3D teardown, in the return to the desktop.
Owner found a plain black screen: `m8_hw_set_mode` turns the display off first and on last, so
the stop is in or just after the desktop mode set. Same run repeated on boot 392: clean. Seven
exits, one hang. The waits before the mode set are bounded (reset on timeout).
F2 3: 3,470 writes a flip against F3's 3,550 (twist), pixels = in 36. Phase marks cost ~0.5-3.6
ms a frame (one run each); split with `phase=1` sums to ~22.9 ms, the old 22.6 ms baseline holds.

## Later the same evening

- **Feed off** (`[feed] on=0`, F2 3 build, boot 392): twist + agent Alt+F4 **7 of 7 clean**, ~5.8
  fps. Feed on: 6 of 7. Every hang so far had the feed on; not decisive at these counts.
- **F2 1** (edge cancel per colour), `m8f2e1.drv`: host test, old against new join over 20,000 random
  frames (18.2 M outer edges): identical edges and order; a broken copy fails it. Card: pixels = in
  36; phase 37 6.7 -> 6.0 ms, split 22.9 -> 22.3 ms (`phase=1`, one run each). ~2 %, not Fable's 3 ms.
  Kept. Card now on `m8f2e1.drv`, INI back to `phase=0`, feed on.
- **Loop timeouts** (lesson above): Mach8 waits count port reads (bus-timed, no drift); `m8_spin`
  only paces the vblank poll; T130B is vendor-only. No change.
- **swapcomp 5**: `WIN386.SWP` from the owner's card image (`win95_latest14112026.img`, 11-14 11:17):
  718 raw slots, 386 (54 %) fit in 7 sectors with LZ4. Build `SC_MAX_SECTORS` 7, A/B in the soak.
  Also 170 raw slots compress to <= 6 sectors (49 to one): explain before that build.
- **X14 step 1** (bed, DX SDK DirectDraw samples, `docs/captures/2026-11-14_x14_ddraw_bed/`): all
  started and the agent survived each. `v9xctl screenshot` fails at 16 bpp in the bed too (Win32 1460):
  pictures need the 86Box camera. 7 of 10 did not move the census: likely windowed or waiting for a key.
  **Wormhole**: `SetDisplayMode(640,480,8)` fails from the 16 bpp desktop (owner read the box); the
  sample's `InitApp` returns the HRESULT as TRUE and goes on with `g_pDD` released, hence the page fault
  in `CreatePalette` (EIP `0x01002f0d`). Cause: `m8dd_build16` (`dd16.c`) lists only the two 16 bpp
  modes. **Next driver item: 8 bpp full-screen modes from the 16 bpp desktop** (the card runs 8 bpp
  anyway; the 8 bpp desktop's full-screen path exists; GDI keeps drawing into fb16, resent at exit).
  First: how Win95 DDRAW asks a driver for a depth change (DDK). Unlocks the 8 bpp X14 titles.
  Fswindow did not close on Alt+F4.

## Evening, last: swapcomp 7 sectors, 8 bpp full screen

- **swapcomp `-DSC_SEVEN`** (`SWAPZF7`): bed checksum PASS (`docs/captures/2026-11-14_swap7_bed/`), card
  checksum 0 and raw reads 25 % -> 4.8 % (`2026-11-14_swap7_card/`). **Card now runs `SWAPZF7`.** Time
  gain unproven: next, fresh-boot PAGETIME x2 per build (owner: no 45-minute soak). PERFLOG's header
  is per run in an appended file (corrected 11-15; reader fixed). Scripts must wait for PERFLOG before a
  restart; the agent's `reboot` verb does nothing on the card.
- **8 bpp full screen from the 16 bpp desktop** (`dd16.c`, `enable.c`): probe showed DDRAW's
  `SetDisplayMode(640,480,8)` arrives as `ReEnable` with the new depth. Now accepted when the desktop is
  16 bpp; both depths listed to DDRAW; back at exit with fb16 cleared and a repaint. Bed: wormhole runs
  and returns, pixels = in all 36 (`bed_d8f1`, fresh boot, 0.953 against `bed_f3B`). **Open:** after one
  round trip the desktop benches 0.63 (`bed_d8w1` against `bed_d8f1`; mem_to_mem 0.01, getpixel 0.26,
  stretchblt 0.24), idle CPU normal, pixels equal; a restart clears it. Likely GDI keeps part of its
  8 bpp view (KB Q127139: Win9x GDI does not change depth live). Find it in the bed before the card.
  Bed `vm_5160_mcp` carries this build; F3 originals in `%TEMP%\mcp_bed_orig`.

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
