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

## Added late 10-11 (unattended)

- PR branch built: fork `mach8-pr` = `6e9978efd` on upstream `1741108a7`; prep branch `mach8-pr-prep`
  (`91afd001f` strip, comments rewritten). G1/G5/G7/G9 pass; gate table and a text draft in
  `docs/mach8_pr_draft.md`. Left: G8 (35 whitespace-only lines to check), G2 non-8514 config (not run),
  G4/G10 owner: ATI's Windows driver and **TEST.COM on the `6e9978efd` build** (watched run).
- Skill `inboard-hw-debug` techniques 149-151 added and pushed.
- M8SHOW lead: if it shows black too, check MAX_WAITSTATES (6AEEh) bit 10 PASSTHROUGH_OVERRIDE - TEST.COM
  sets it and it blanks the picture (10-08 notes).

## Submitted 10-11 night

- **86Box PR #8289 MERGED** (OBattler, 21:42 UTC, `3a0efe82b`) https://github.com/86Box/86Box/pull/8289 - `Mike1978uk:mach8-pr` `942e2de86`, six files.
  TEST.COM passes; Win 3.11/95 checked; Mach32/8514/A regressed. G10 (Settings attach by hand) not done.
- Replies posted: 86Box#6695 (Paintbrush fill, thanks Andrew) and #4898 (TEST.COM). Owner approved wording.
- Done: #8289 merged; Andrew told on our #49. Next: follow-up PR for the INMOS IMSG176 DAC (was: tell Andrew on our #49 that #8289 is up.

## 10-12: #8289 broke every other VGA card

- Build 10378 segfaulted on every VGA card except the Mach8 (86Box#8290): our `svga_out` tested
  `ATI_GRAPHICS_ULTRA` (reads `dev->local`) before `dev`. OBattler fixed it (`13751de8c`). G2 had been skipped
  ("Mach8-only files") although `vid_svga.c` is shared. New gate G11 (smoke-test untouched configs), `7d431e3`.
- Same fix committed locally in `86box_prprep` `57f5e6a5f`, `86box_prup` `d4998f8dc`, `86box_3c509b` `4a79b0b4f`.
- 86Box#6695 is NOT fixed: OBattler's VM still pastes the screen's top-left into the canvas. We only tested the
  border-set leak. Next: the top-left paste on the card (skiretic/86Box#23: uninitialised frame slot FFFE).
- Promised on #49: a 60 Hz default EEPROM (the current default is the owner's card verbatim, 72-73 Hz).
- Replies posted 10-12 (owner-approved): #49, PR #8289, 86Box#6695.

## 10-12: 60 Hz default EEPROM - measured, PR not yet raised

- Card EEPROM (PR #8289 default) = FlexVIEW2X name, but refresh flags 72 Hz at 640/800/1024 (Mach32 guide
  EEPROM map, words 7-9). INSTALL "NEC 4D 60 Hz" -> `roms/video/mach8/eeprom_card_then_nec4d_60hz_install.nvr`
  (leaves 640x480 at 72); candidate = that + word 7 = 0: `eeprom_60hz_candidate.nvr`.
- Fork Qt build `86box_3c509b/build_qt` (`4a79b0b4f`, 09:43), candidate in `vm_5160_now`: TEST.COM passes;
  Win95 ATIM8 60 Hz at 800x600 and 1024x768. Win 3.11 not run.
- G11: `vm_fenix` plain VGA boots DOS 70 Hz; `vm_6695_w2` with ET4000AX boots DOS 70 Hz (cfg restored).
- Open: `vm_fenix` INBRDPC banner shows "conventional memory initialized" blank on the fork build; the
  09-11 `86box_full` build fails POST 101 on that bed; upstream-master control not run.
- #8289 left a `-Wdangling-else` at `vid_8514a.c:3307` (unbraced `if` around the `WRITE` macro; behaviour correct).
- Bed hygiene: `WRTEST.BAT` removed from `vm_5160_now` StartUp; launcher now refuses boot hooks and non-Qt
- **86Box PR #8294 opened** (60 Hz default EEPROM, `Mike1978uk:mach8-eeprom-60hz` `8eb305898`, 1 file +9 -8):
  G1-G11 on the PR's Qt build (`86box_prup/build_qt`, 10:08): written nvr = candidate byte for byte, TEST.COM
  passes, Win/386 2.11 60 Hz; VGA and ET4000AX boot DOS. Win 3.11/95 60 Hz on the fork build, same bytes.
  OBattler had already fixed the dangling-else (`7f5afe2fb`). Andrew told on #49 (owner-approved, thanked).
  builds for open runs. `vm_5160_now/default/card.img` differs from the live disk (no `C:\M8UTL` on live until 10-12).

- **CPU fix: no PR needed.** Upstream fixed the same bug independently - Hinoserm, `acc82a87f`, 2026-09-24 ("cpu: fetch code from device memory through the bus"). Checked 10-11: upstream master with the card EEPROM boots the Inboard bed to the Windows desktop. Our `5fce405ab` stays fork-only.

## 10-12: 6695 static step 1 - where it stopped

- Our bed `vm_6695_w2` binds the W2 disk-7 `8514.DRV` (`references/win2_8514/`): its seg 1 sits in
  `WIN386\WIN200.BIN` at 0x10750, 890/919 16-byte chunks identical (rest = fixups). In that driver the
  wrap add `add di,[bp-24h]` (1:1BD9) has one direct caller (1:1615), right after `mov [bp-24h],di`
  (1:1612) - slot set first. Computed jump at 1:15ED not followed.
- The reporter's VHD (86Box#6695 attachment `504-win211.vhd.zip`, the one skiretic and OBattler use)
  binds a DIFFERENT build: same base 0x10750, identical up to seg1 ~0x3050, then differs to ~0x32A0
  and is shifted -0x6E after. skiretic's addresses (3109, 320D) fall in exactly that region - the
  screen-to-mono copy. So the bug lives in code our bed never runs; that is why our 10-09 test passed.
- Next: disassemble the reporter's bound seg1 at file 0x10750+0x3050..0x32A0 and diff against ours
  (1:30A0-1:3300). Download sits in the session scratchpad only; re-fetch from the issue if needed.
- RESULT (static, both binaries): the reporter's screen-to-mono routine (seg1 30FE-3220) is a different
  implementation from ours. It tests the segment wrap with `mov ax,di / add ax,[bp-24h] / jb` at 320D and
  NEVER writes [bp-24h]; the only writers in its seg1 are 1612 (another blit path) and 222B/2285
  (0300h-0340h). Ours computes the slot at 312D before using it. So the driver on the reporter's disk
  has the uninitialised-slot bug; the disk-7 driver does not. Open for 86Box: whether the stale value
  (FFFEh in skiretic's capture) depends on anything the emulator does differently from a real card.
  That needs a write watch on the stack slot in the reporter's VM; cannot be settled statically.
- MEASURED (fork `diag-6695` 48dfb71b1, reporter's VHD/cfg/NVRs in `vm_6695_rep`, Mach32 VLB): reproduced.
  At CMD 41F0 the slot SS=6811 BP=80F0 holds FFFE; its last 32 writers are Windows/386 2.11's own V86
  code at segment FFFE (EIP 0083/0094/AFF5/AFFF, 5548 writes cycling 31B0/AFE5/FFFE). No card read in
  the chain, no HIMEM or DOS=HIGH in the reporter's CONFIG.SYS. A real 386 does the same: driver bug,
  86Box correct.
- Good driver (WinWorld Windows 2.11 (386) 3.5", "Additional Drivers" 8514.DRV, md5 42e8f034 =
  references/win2_8514): `vm_6695_w2`, Mach8, a bounded fill - correct, no paste, and CMD 41F0 is never
  issued (it fills by pattern copies + 3318 reads). Owner, 10-12.
- Our own driver: use this build's fill path; never read an unset frame slot.
