# Next session - from 2026-10-15

Start from `docs/worklist.md`. Owner: next session is driver work (section D).

## Done 10-15

- **86Box PR #8339** opened: TC1995's Win 3.x cursor regression from #8289. It shows with ATI's
  ULTRA.DRV (1992), not MACHW3 (our usual driver). Cause, tests, gates and what was not run:
  `docs/mach8_ultra_drv_cursor_2026_10_15.md`. Owner messages TC1995 with the number.
- **86Box PR #8294** (60 Hz EEPROM) found merged 10-12; worklist ticked.
- `bed_launch.ps1` refuses an exe linking both msvcrt and UCRT (a build configured from a `MSYSTEM=UCRT64`
  shell dies at start with no window). Technique 131.
- `a6_regress.sh` now stages ATI TEST.COM: 15 probes had failed silently in every A6 run since 10-10.
- #10: Simon's two boot logs stop at `EBIOS` like before; display driver and EMMEXCLUDE ruled out. HIMEM.SYS
  is stock. Owner replied.

## Driver work (D0) - where it stands

- Our FRAMEBUF build (`OWFB.DRV`) and Microsoft's FRAMEBUF.DRV both fail on `vm_5160_now`, so the control
  failed: the sample cannot drive this card, and our build is not yet shown to load in Windows.
- Cheapest next step, no boots: read the DDK FRAMEBUF sample's chip detection (WONDER.ASM and init) to see
  why it rejects the 28800. Then either a one-line change gets our build loading, or start our own Mach8
  skeleton (`drivers/mach8_display/`).

## Left on disk

- `86box_cursorfix/` (PR branch `mach8-fg-source`) and `86box_bisect/` (fork + the same edit, uncommitted):
  keep until #8339 merges, in case of review.
- `vm_mach8_ultra/`: Win 3.11 + ULTRA.DRV bed; `bisect/w311.img` boots straight to `WIN`.
- `vm_6695_w2/nvr/mach8.nvr.bak_1015`: its EEPROM before the TEST.COM run (restore if a run needs it).

## Parked (owner)

IE4 breaking the disk driver; WebCam II image; Jaz on the T130B to record in `5160_tuned_config.md`.
