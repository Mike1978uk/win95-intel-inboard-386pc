# Next session - from 2026-10-11

Start from `docs/worklist.md`; this page is the state at the end of 10-11.

## Done 10-11

- XT-CF measured-wait driver `a67fefc4`: bed + 5160 pass, published in `dist/` (`xtcf_measured_wait.md`).
- Card configuration recorded in `docs/5160_tuned_config.md`; REFR64 confirmed (RSOAK2 clean, floppy OK).
- MinTimeSlice=54 on trial: +6% idle CPU, +60% during writes (`captures/2026-10-10_timer/`).
- Mach8 PR: A6 regression - Mach32 M8CONF 82/82 = branch point after gating `8f520484d` (`27e57fcfe`) and
  `46d9ad164` (`89942be5e`) to the Graphics Ultra; remaining diffs (M8CMP8, M8MIX 1Fh) are documented fixes.
  LINE_SYNC `5897f6a48`. M8A2 gaps fixed: scissor reads FFh `e8d4dc5c2`, pattern fill rows `125bfddc8`.
  Fork branch `inboard-ext-256k-diag` pushed to `mike` at `89942be5e`.
- Card probes: M8XMEM (halves do not share memory), M8LIN (no 128 KB window), M8FLIP/FLIP2 (8514 side
  never reached the monitor) - `captures/2026-10-11_m8probes/`.

## Waiting on the owner at the 5160 (all staged on the card)

1. DOS: `M8BLEND` (blend cost), `M8SHOW` (watch: 1024x768 then 640x480 bars), `TICKDEF`.
2. Windows DOS box: `CPYTIME`. Then DOS `TICK54`, Windows, `CPYTIME` again. Results in `C:\CPYTIME.TXT`.
3. Decide MinTimeSlice from the copy times; update `5160_tuned_config.md`.

## PR (owner 10-11: A1 chapter 9 and A4 appendices come AFTER the PR)

1. 8514/A rerun `head_8514a_r3` - expect only M8CMP8 and M8MIX to differ from `base_8514a`.
2. A5 Windows 95 capture (owner, `vm_5160_now`, MACH8_WLOG=1).
3. A7: strip in-file diagnostics (M8TRACE, MACH8_COUNT, MACH8_RLOG, DIAG7) and DIAGNOSTIC commits
   (incl. `c3a8c6430` IDE_COMMIT_US); rebase on upstream (conflict expected with `6a77fe831`); build; rerun
   probes; G1-G10; PR text drafted for the owner.

## Open questions

- Why `vm_xtide_mpd`'s image no longer loads any XTIDEMP build (Init Failure) - use `vm_5160_now` meanwhile.
- The 8514 output path: M8SHOW decides whether anything reaches the monitor; then M8FLIP v3.
- `SYSTEM.BSR` on the card was overwritten on 10-11 (old contents unknown; B02-B07 untouched).
