# ULTRA.DRV cursor regression from 86Box #8289 (TC1995, 2026-10-12)

## Report

@TC1995, 86Box Discord, 2026-10-12: #8289 broke the Windows 3.x mach8 drivers; the cursor is a black
square. "The first is about extended bitblt's (cmd type 2) on the frgd_sel."

## Reproduced (bed `vm_mach8_ultra`, Win 3.11, 2026-10-15)

| driver | before #8289 (`86box_upstream` 10-05) | #8289 (`86box_prprep`) |
|---|---|---|
| MACHW3.DRV (ATI mach8 rev 3.0, 1994) | cursor fine; slanted window edges, ghost labels (fixed by #8289) | clean |
| ULTRA.DRV (ATI GFXULTRA disk, April 1992) | clean | **cursor a square that inverts what is under it** |

`vm_mach8_ultra` is `vm_mach8_w311` with SYSTEM.INI set from ATI's OEMSETUP.INF: `display.drv=ULTRA.DRV`,
`display=VDDULTRA.386`, `386grabber=VGADIB.3GR` (both drivers were already in the image; original kept as
`WINDOWS\SYSTEM.MW3`). `bisect/w311.img` adds a minimal CONFIG.SYS/AUTOEXEC.BAT that boots straight to `WIN`
(the full ones are kept as `CONFIG.FUL`/`AUTOEXEC.FUL`).

## Cause

Write log (`MACH8_WLOG=1`, `vm_mach8_ultra/ultra_wlog.log`): the cursor is drawn by 8514/A BitBLTs
(`9AE8 C0B3`, `PIX_CNTL A0C0`, mono mask from VRAM, `RD_MASK 02`), AND pass then XOR pass. The masks are
uploaded by extended blits with host data:

```
CEEE 3251   DP_CONFIG: 16-bit mono host data, FG source 1
B6E8 0002   BKGD_MIX, mix code only
BAE8 0001   FRGD_MIX, mix code only
E2E8 ...    the mask, as host data
```

Fork commit `88f53e2aa` (in #8289) made any FRGD_MIX write after DP_CONFIG hand the extended blit's
foreground, background AND monochrome sources to the 8514/A registers. Only the foreground was measured
(TEST.COM op 62, M8ROW). The mono source then came from PIX_CNTL (VRAM, left by the cursor draw), so the
upload stored a wrong mask and every cursor draw inverted the whole square.

Found by bisecting the fork's 125 Mach8 commits (first bad among the tested: `88f53e2aa`), then reading the
write order. TC1995's hint named this code at the start; it was dismissed because the cursor DRAW is an
8514/A command - the upload is the extended blit.

## Fix

`mach_accel_start()` case 2: on the Graphics Ultra, `dp_compat` takes only `frgd_sel` from the 8514/A
registers; `bkgd_sel` and `mono_src` stay with DP_CONFIG. Branch `mach8-fg-source` (`86box_cursorfix`).

Upstream branch `mach8-fg-source` = `068d00e6d` on master `06985f178` (`86box_cursorfix`), not pushed.
Fork test build = `4a79b0b4f` + the same change, uncommitted (`86box_bisect`).

## Tested, and not

| check | build | result |
|---|---|---|
| ULTRA.DRV cursor, owner watching | fork + fix | pass |
| MACHW3.DRV cursor, Accessories, Exit Windows, owner | fork + fix | pass |
| ULTRA.DRV cursor, screenshot read | upstream branch | pass (arrow where the square was) |
| Graphics Ultra A6, all 49 probes incl. M8ROW (op 62) and the TEST.COM sequences | fork base vs fork + fix | 48/49 identical; M8STAT differs between two runs of one build too (timing) |
| Mach32 A6 (35 probes, TEST.COM not staged then) | fork + fix vs #8289 build | 35/35 identical |
| 8514/A A6 | fork + fix | **not run to the end**: the fork's per-fill `[M8P]` trace takes hours; the changed lines are behind `ATI_GRAPHICS_ULTRA` |
| G11: plain VGA (`vm_fenix`), ET4000AX (`vm_6695_w2`) boot | upstream branch | pass (INBRDPC loads; DOS prompt) |
| Build warnings, whole tree | upstream branch | none |
| ATI TEST.COM, all sequences, owner watching (AT bed, new-card default EEPROM) | upstream branch `068d00e6d` | pass |
| Mach32 / 8514/A on the upstream branch | upstream branch | not run |
| Graphics Ultra A6 on the upstream branch | upstream branch | not run |
| Real 5160 | - | not applicable (emulator-only change) |

## Harness gaps found

- `a6_regress.sh` never staged TEST.COM, so 15 probes (M8ROW*, M8SEQ*, M8TS*, M8POLY) failed silently in
  every run since 10-10 ("35/35" was 35 of 50). Fixed: the script stages it (`A6_TESTCOM`).
- A build configured from a `MSYSTEM=UCRT64` shell links both C runtimes and dies at start; `bed_launch.ps1`
  now refuses it (technique 131).
