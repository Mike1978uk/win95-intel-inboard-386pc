# Next session, after 2026-11-18

Earlier: `docs/next_session_2026_11_18.md`. Newest wins.

## START HERE

**First job (owner brings the CF card up):** back up the whole CF card on the host (it mounts at `D:\`), then put the
buffered-mode VPICD on it offline and boot the 5160 with it. Spec, deploy and rollback steps:
`docs/issue10_vpicd_buffered_2026_11_18.md` (written by the agent that built it; check it landed and read its bed result
first - the agent was still running at handoff). Patch script `vxd-patches/patch_vpicd_buffered.py` (expect `Patched: N`).
On the 5160 prove the patched VMM32 loaded (md5 on the guest), then: desktop, NIC (IRQ 9->2), T130B (IRQ 3), keyboard,
mouse, floppy, twist once. If it holds, the next step is Simon's 5150 (one batched ask; his file transfer is slow).

**State.** Card: idle at the desktop, 800x600x16, driver as in the 11-18 handoff, agent build `inboard-trayflash`.
Beds: `vm_5160_dbg` and `vm_5160_mtm` shut down. 86Box `86box_auto` has the X22 commit `26fd9ec4c` (local, not pushed)
plus the uncommitted M8STEP diagnostic. `86box_h2` worktree (branch `diag-issue10-h2`, uncommitted I/O 100-1FFh trace).

## Done 2026-11-17/18

| item | result |
|---|---|
| V9x agent | `tray_flash` PR [michaeldale/V9x-Remote-Agent#1](https://github.com/michaeldale/V9x-Remote-Agent/pull/1) open; README "Other projects" table + ledger |
| #50 | L1 on/off makes no difference: 3.93 ms per KB either way (`captures/2026-11-18_xtinsw_card`); posted to Andrew, ledger closed |
| X22 | 86Box starts the 8 bpp frame at CRT_OFFSET: bed now flips both pages (`captures/2026-11-18_x22_bed`). Not upstream: full regression first (TEST.COM, probes, four 8514-family cards, G11) - raise before running |
| X20 | twist pages 23-24 in / 10-11 out a second vs 1.6 / 1.1 idle, free RAM ~0; est. up to ~half of each second in paging I/O (assumes 4 KB per count, unconfirmed). First attempt hung (twist windowed, black, hourglass); retry did not hang |
| #10 H2 | only the VGA BIOS touches 100h-1FFh (real and V86 mode): weak (`captures/2026-11-18_h2_iolog_bed`, doc section 18) |
| #10 IBVPICD | Intel's VPICD = stock 3.0 cut to one 8259, writes ICW4 09h (buffered) like the BIOS; ours writes 01h. Lead (`docs/issue10_ibvpicd_2026_11_18.md`, doc section 19). Simon = Cimon (one person). Owner messaged Simon: was WfW 3.11 on the 5150, with Harrison's image / IBVPICD? |
| DOS driver | `docs/dos_driver_groundwork_2026_11_18.md`: DOS/32A recommended; risk = extended memory under the 1986 BIOS (HIMEM sees 0 KB); first test a DOS/32A hello under INBRDPC.SYS. Mode from DOS = engine init + shadow set (`OUT 4AE8h,3`). Z27 corrected |
| Direction | "DirectXT" (owner's): our engine takes over the runtime's work wherever an interface allows; X20 and twist's 118 ms of D3D geometry are the numbers behind it |

## Check at the start (agents running at handoff)

- **VPICD buffered** (Opus): reached the bed desktop; was proving the file loaded, then writing the deploy doc. Its files
  (`vxd-patches/patch_vpicd_buffered.py`, `VPICD_INBOARD_BUF.VXD`, the doc) are uncommitted - review, then Haiku commits.
- **MTM dashboard off: done** (`captures/2026-11-18_mtm_dashoff_bed/`): colour runs a row 124 -> 17, but the
  back buffer is still Locked whole (480 rows) every frame - cause unknown (HUD text a guess). Feeds the S9 design.

## 2026-11-19 morning

- #55 again: card locked at first boot to the desktop, startup sound looping; owner reset, next boot normal. Stack unchanged (before the buffered VPICD). Next recurrence: one boot with the sound scheme off.
- VPICD agent left no deploy doc; written as `docs/issue10_vpicd_buffered_2026_11_18.md`. Bed reached the desktop.

## 2026-11-18 midday: games, DOS, bed parity

- **#10 VPICD buffered** on the 5160: holds (devices, twist 6.73 fps). Sent to Simon (link pinned `b20b0ce`); awaiting.
- **DOS route open:** DOS/32A and PMODE/W find ~4.2 MB above 1 MB under INBRDPC.SYS; `M8DOS8.COM` shows 640x480x8
  (256 colours) from a cold DOS prompt. `docs/dos_driver_groundwork_2026_11_18.md`. `tools/dos3d/M8CUBE32.EXE`
  (agent-built cube, DOS/32A) not yet run anywhere and not yet audited.
- **Foxbear** (DDraw+DSound): loads all art, then crawls. Bed register samples: a tight loop in run-time code at
  B0C010D8 walking a pointer through shared memory ~25 KB/s - DirectDraw's software emulation on surfaces that did not
  fit the card (it asks for multiple back buffers at 640x480x8) on a machine with no free RAM. Untested: `-S -2`.
  Restarting the bed with Foxbear running blue-screened: 0D at `0117:00005B0A` (log: illegal instruction 0117:B929)
  - X19 family (`captures/2026-11-18_foxbear_bed/bsod_restart.png`).
- **Doom 95** (DDraw+DSound): on an 8 bpp desktop it runs windowed, correct colours and sound, slow (owner: not
  playable). On a 16 bpp desktop it requests 320x200x8, our driver accepts without changing mode: quarter-size image in
  the corner. On the card, `-timedemo demo1` went full screen and wedged (owner power cycle). Full screen needs a real
  320x200: use the 8514-half lores mode (320x240, `m8d3d_lores_set`), not the VGA half. Keep Doom windowed on the card
  until then.
- **Flight titles:** `docs/flight_titles_2026_11_18.md` - no 486-class Direct3D flight demo exists. FS98 (owner's ISO)
  being installed on `vm_5160_page` (Direct3D optional, stated minimum 8 MB, card has ~4.8 MB).
- **Owner direction:** bed/card parity is required work - prove X22 and upstream it (memory
  `feedback-bed-card-parity-priority`). Showcase wanted: one DirectDraw and one Direct3D title, playable.

## 2026-11-18 afternoon

- **X22 regression** (`tools/a6_regress.sh`, now stops only its own VM, `8514ultra` card, `A6_WAIT`): base `e2c3c0c3b`
  (`86box_x22base`) vs X22 `26fd9ec4c`. Graphics Ultra 48/48 deterministic probes identical (M8STAT is raster-timing
  noise, differs every run); Mach32 49/49 identical. **8514/ULTRA add-on crashes 86Box after M8BYTE4 on BOTH builds**
  (pre-existing, or the `extensions = 1` config) - chase before G11. IBM 8514/A stalls after M8BYTE4 (base; X22 run
  pending); the ATI-only TEST.COM probes never complete on it (also 10-10). Captures `captures/2026-10-10_a6/x22*`.
- **V9x 0.19.0 on the card** (hot update, AGENT.INI kept). Tray redraw count NOT obtained: run at 800x600x16, where
  M8EPOCH's counters (8 bpp Lock path) cannot see a repaint - all three arms read `copy_hw +2`. Redo at 640x480x8 before
  replying to Michael (reply on the PR thread). `captures/2026-11-18_tray019_card/`.
- **#55 twice today**: first boot (old stack) and an agent-initiated reboot. Test: sounds off; open with owner.
- **Foxbear** on a fresh bed (`vm_5160_fox`, from the CF image, DHCP set by the owner): `-S -2` still crawls - same
  DirectDraw run-time loop (`B0C010E2`), screen moving ~every 6 min, not hung. Theory: paging. Test staged: `vm_5160_fox`
  at **17408 KB** on a diagnostic build `86box_x22base/build17` (Inboard RAM cap lifted, source reverted). Run
  `FOXBEAR -S -2` there; if it plays, it is memory, not the driver.
- **FS98** installed on `vm_5160_page` (key from the web; owner's CD for anything published). Runs windowed in software,
  never took input in ~2 h; CPU in KERNEL32 - too heavy at 5 MB, like MTM. Optional: same 17 MB test.
- **DOS cube** `tools/dos3d/M8CUBE32.EXE` audited (`77ce61f`), not yet run on the card.
- Issue audit + drafts `docs/issue_audit_2026_11_18.md`; README fixed (`59b8608`).

## Next, in order

1. VPICD buffered on the 5160 (above). Then Simon.
2. #10 desk: trace the 8259's pin 16 (SP/EN) and CAS0-2 in the 5150 and 5160 Technical Reference schematics.
3. X20 follow-up: S7b (free the 635 KB locked shadow when DirectDraw ends); confirm whether cPageIns counts pages or
   operations; cost RAM compression. Note twist full screen or windowed by eye on the next card run.
4. DOS driver: DOS/32A hello under INBRDPC.SYS on the card (memory found?), then P3.2.
5. Plan items untouched: P1.10 fused asm row pass, S9 HUD/static-art design (MTM result feeds it), X22 regression.
6. `dist/mach8` still `32bfaac`.

## 2026-11-18 evening: X22 rebased, add-on crash found

- **X22 rebased** alone onto `origin/master` `3b7727a1e`: branch `mach8-crt-offset-flip`, worktree `86box_x22pr`,
  `e0a713098`. One conflict (TC1995's `f5cd25425` rewrote `ati8514_recalctimings`); G1 grep clean, no warnings.
  Upstream reworked the mode registers since `e2c3c0c3b`, so the afternoon's regression no longer counts as the base.
- **Build with `MSYSTEM=MINGW64`**, or the exe links msvcrt and UCRT and `bed_launch.ps1` refuses it.
- **8514/ULTRA add-on crash at M8TSX = our #8289 (`942e2de86`)**: the FIFO-test replay calls `dev->accel_out_fifo(mach, ...)`;
  the add-on's handler takes `svga`. Crashes on clean master too. Fix `c68611861` (branch `mach8-fifo-replay-ptr`,
  worktree `86box_fifofix`, local): add-on now runs all 49 probes (`captures/2026-10-10_a6/fifofix_8514ultra`).
  Separate upstream PR from X22; owner's call. IBM 8514/A still stalls at M8TSX (no crash; different handler, not checked).
- **Next:** X22 on top of `c68611861`; four-card regression against `c68611861`; G11 smoke boots; G2 log compare.
- **#55 third time today** (evening): startup sound looping at boot; owner rebooted, next boot clean. Card at desktop,
  agent v0.19.0 up (boot counter 449). `M8CUBE32.EXE` copied to `C:\DOS3D\` (78,658 B, CRC 4FCCFBA8).
- **X22 regression on master + fix** (`fifofix_*` vs `x22r_*`, `captures/2026-10-10_a6/`): Graphics Ultra 48/49 and
  add-on 48/49 identical (M8STAT only, raster noise), Mach32 49/49; IBM 8514/A 5/5 identical, both stop at M8TSX
  (ATI-only probes, as 10-10). X22 changes nothing the probes see. `a6_regress.sh` now stops after 3 min without a
  new result (`A6_STALL`); a full run is ~80 s. Left for X22: G11 smoke boots, G2 log compare, G10 (owner), G6 text.
