# Next session - handoff from 2026-09-26 evening

## Done today

- **T130B A/B on the 5160: IRQ 5 took 23% off a 1 MB copy to the Zip** (18.80 -> 14.48 s).
  Recorded in `docs/t130_mpd_review_2026_09_26.md`; Andrew told on #42. #42 proceeds.
- `tools/timerres/TIMERRES.EXE` built (gcc -m32 + ld i386pe + dlltool, `build.sh`). Measures the
  real Windows tick and its CPU cost. Not yet run on the 5160.
- VC++ 4.2 cloned to `..\MSVC420` (not in the repo); recorded in `resources_and_sources.md`.
- **VPICD IRQ 2 patch written, untested**: `vxd-patches/patch_vpicd_irq2.py` ->
  `VPICD_INBOARD_IRQ2.VXD` (md5 `6c304f8c`). Why and how: `docs/vpicd_irq2_patch_2026_09_26.md`.

## Waiting on the owner

- **Issue draft** `docs/drafts/issue_win95_settings_and_timer.md` - wording approval, then post.
- The 5160 is being rebuilt (CF restored, SB Pro refitted, T130B jumper off IRQ 5).

## Next, in order

1. **Bed test of the VPICD patch** - ask first. `vm_3c509b`, card `irq = 9`, exe
   `86box_3c509b` (carries `ef082884b`). Pre-monolith deploy; prove `VPICD` loaded from the
   file. Control run with stock `VPICD_INBOARD.VXD`. The 09-24 failure: log ends at
   `3C509B: IRQ 9 up`, black screen with cursor, 86Box still running.
2. `TIMERRES` on the 5160, then the polled `T130AB` with and without `TIMERRES HOLD`.
3. The settings issue's A/Bs, one variable each.
