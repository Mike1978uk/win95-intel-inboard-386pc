# Next session (written 2026-11-23)

Earlier: `docs/next_session_2026_11_22.md`. Today's findings: `docs/d14a_mode_switch_profile_2026_11_23.md`.

## State

- **Card** boots `M8DC.DRV` + its `M8DD32.DLL` (F1: one display DC per process instead of GetDC/ReleaseDC a send),
  boot 497. Desktop bench `card_dc1` 0.996 vs `card_vgc1,2`, pixels same. `dist/mach8` is still `e3544f3`'s build.
- Card files added: `C:\V9XREMOTE\TUNA.LST`/`TUNA.EXE`, `TUNB.LST`/`TUNB.EXE` (tunnel + census at +30/+60 s),
  `C:\D3DS\EIPRUN2.BAT` (EIPSAMP 150 s).

## D14a result

Mode switches and Win16-lock waits are a start-up cost (load, mode set, first frames), not a steady one: in
steady frames they are ~3-4 % of the main thread. Steady frame: our M8DD32 ~61 %, D3DIM ~27 %.

Afternoon (T3a, T3b): steady tunnel is ~320 ms a frame (draw calls ~77, Flip ~87, rest ~155); paging is a
start-up cost too (~4 page-ins/s steady). The "0.9 s a frame" was the start-up phase, withdrawn. T3b (corner
conversion once) took draw calls to ~72 ms; card boots `M8CW.DRV` with it (boot 506+).
Doom hang (T2) was a Win16 fault dialog in `Msgsrv32` (16-bit sound callbacks), sound looping, power
cycle: SWAPCOMP is not the cause (it was out). Two repeats (with and without the sampler) did not fault:
intermittent. Dr. Watson is on the card to name the module next time.

## Owner's task list (agreed 11-23), revised by today's numbers

| # | Task | State |
|---|---|---|
| T1 | D14a sampler on tunnel | done |
| T2 | Sampler on Doom 95 + sound, and twist | Doom done (`captures/2026-11-23_doom_eipsamp/README.md`): game time is 16-bit GDI/DIB in our driver; the 45 s in DSOUND was Doom behind a `Msgsrv32` fault dialog (16-bit sound path), 1 run in 3. Next: Dr. Watson log of the fault, then `-nosound` runs. Twist open |
| T3 | Take over the largest 16-bit share | F1 built; F2/F3 shorten start-up only. Windowed Doom (GDI) named: fb16 fingerprints ~12 %, conversion 3.6 %, bus 4.6 %; adaptive whole sends tried 3 ways and reverted (bench losses via the background store), see `captures/2026-11-23_doom_eipsamp/README.md` || T3a | Paging during tunnel | done: start-up only (~4/s steady). Agent-off run not done (needs the owner at the card) |
| T3b | Corner conversion once in `m8d3d_draw_on` | done: draw calls 77 -> 72 ms/frame, exact. Left: clipped corners' `fixf` round trip (2 %) |
| T3c | The ~155 ms a frame outside our calls: name D3DIM's block | done: transform+clip 42 %, point light 24 %, colour 9 % of D3DIM, all x87 on the 387. Lever: fixed-point geometry in DirectXT/DOS |
| T4 | S1 sound cost (DMA, IRQs, CPU, silent vs playing) | `M8DDB modex/modexs/modexd` on M8CW: the BAT stalled after run 1 (`t4_x1` only). Rerun each through the agent's `exec` with a timeout. Never Doom (needs the owner to exit it) |
| T5 | DirectSound VxD driver for the SB Pro (#58) | after T4 |
| T6 | Disk: Performance tab compatibility-mode check; paging per title | open (T3a covers part) |
| T7 | Titles as probes (X14) | open |
| T8 | Period benchmarks (WinBench 96, CrystalMark Retro, 3D WinBench 97) | open |
| T9 | Unused-silicon inventory, one table | open, Sonnet desk |
| T10 | Demoscene-tricks register (P1-S) | open, Sonnet desk |
| T11 | Carry each win across m8dos / driver / DDraw-D3D / DirectXT | standing |

## How to name M8DD32 samples

eipsum shows M8DD32 as "flat, no module" (Toolhelp does not list it). Link with `option map`, compile the same
source with `-d1` and `wdis -s`, then `captures/2026-11-23_eipsamp_tunnel/name32.py` (code labels only: a
data label in the listing misnames the code after it). `phase.py` splits a run into time slices.
