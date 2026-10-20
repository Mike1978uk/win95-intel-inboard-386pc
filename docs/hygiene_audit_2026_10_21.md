# Hygiene audit - 2026-10-21

Read-only. Nothing posted, edited, staged or committed. Sources: README.md, FIXES.md, docs/worklist.md, docs/next_session_2026_10_21.md, docs/contributor_input_ledger.md, `git log`, `gh issue list`, `gh pr list --repo 86Box/86Box`.

## 1. Base

- `git status --porcelain`: empty (0 lines).
- HEAD `2e60862` 2026-10-20 "Docs: shadow setup registers leads plan item 6". `git fetch -q && git status -sb`: `## master...origin/master`, level.
- Publication check (*.VXD/DRV/SYS/PDR/MPD under vxd-patches, custom_vkd, ivt68fix, dist, drivers): nothing untracked and unignored (`git ls-files -o --exclude-standard` returns none). 36 files are untracked only because `build/` is gitignored:
  - `drivers/imation_ls120_mpd/build/*` (25 .MPD, `config_norem.sys`): retired driver, fine.
  - `drivers/xtide_mpd/build/` (5 .MPD), `drivers/shadowram/build/` (2), `drivers/swapcomp/build/` (3), `drivers/nmiwatch/build/NMIWATCH.VXD`, `drivers/mach8_display/src/build/M8DISP.DRV`.
  - Shipped copies match builds for SHADRAM (`bd529d50`) and SWAPCOMP (`9fde4934`).
- Flags:
  - `dist/xtide_mpd/XTIDEMP.MPD` is `a67fefc4`. `drivers/xtide_mpd/build_ledger.tsv` shows it built from a DIRTY tree (2026-10-11T16:18Z). A clean rebuild 19 minutes later, same flags, gave `04489332`. The shipped file is tested on the 5160 (`docs/5160_tuned_config.md`) but is not reproducible from a commit. Not wrong; owner may want to say so in `drivers/xtide_mpd/README.md`.
  - `NMIWATCH.VXD` is not in dist (diagnostic, #10). `M8DISP.DRV` is not in dist (not yet run on the 5160, per #49). No action unless the owner wants them published.
  - `dist/ls120_mpd/` still ships the retired `LS120MP.MPD`/`LS120STB.MPD`. Intended? README calls the driver "retired, do not install". Check the dist README says so.

## 2. README and front-page docs

All relative links in README.md and FIXES.md resolve to tracked files (no broken links). Other root .md files (3C509B_PORT_SPEC, BUILD_SETUP, DEVICE_INTEGRATION_SPEC, HARDWARE_CONFIGURATION, IMPLEMENTATION_ROADMAP, AGENTS) were last touched 2026-07-30 to 2026-10-16; not read line by line. Only AGENTS.md is recent (2026-10-16).

Statements out of date (README line numbers):

1. L237 "Twenty-one PRs raised from this project are merged." Table lists 21; `gh pr list` shows 22 merged. Missing: **86Box#8394** (merged 2026-10-18, Mach8 one-pixel line read, Win 3.0 Solitaire hang; `docs/worklist.md` A13, `docs/mach8_data_available_2026_10_18.md`).
   - Replace: "Twenty-two PRs raised from this project are merged."
   - Add row after #8351: `| [#8394](https://github.com/86Box/86Box/pull/8394) | Also from #8289, reported by Mamoru and TC1995: Windows 3.0 Solitaire hung on the Graphics Ultra. A one-pixel line read cleared GE_STAT's data-available bit before the host took the word. Merged 2026-10-18 |`
2. L271-275 "Every Inboard change is now merged, so master carries the whole model". Not quite: the `pic.c` change that delivers IRQ 9 as IRQ 2 on a single-PIC machine (`ef082884b` on `86box_3c509b`, `e2c3c0c3b` on `86box_auto`) has no PR (see section 4). Without it the emulated 3C509B at IRQ 9 never interrupts on the XT.
   - Replace: "Every Inboard change is now merged except one: IRQ 9 delivered as IRQ 2 on a machine with no second PIC (needed by the 3C509B at IRQ 9, #42). It is not submitted."
3. L398 "**127 numbered techniques**". `.claude/skills/inboard-hw-debug/SKILL.md` runs to Technique 151 (last: "151: emulate the device's slow path, and bisect leaks into other cards").
   - Replace: "**151 numbered techniques**" (or "Over 150").
4. L95-96 "Accelerated video - ATI Mach8 (Graphics Ultra) at 1024x768x256, confirmed on real hardware 2026-08-24". True when written; the card now runs 800x600x256 (#34 closing note, 2026-10-03; `docs/5160_tuned_config.md`).
   - Replace: "Accelerated video - ATI Mach8 (Graphics Ultra) with Windows 95's own ATIM8 driver: 1024x768x256 confirmed on real hardware 2026-08-24; the machine now runs 800x600x256 (see below)."
5. No mention anywhere in README of the project's own Mach8 driver `drivers/mach8_display/` (GPL-3.0; desktop at 640x480, 800x600, 1024x768 in 86Box; not yet run on the 5160; `docs/next_session_2026_10_21.md`, #49). The "Repository structure" list also omits `drivers/mach8_display/`, `drivers/shadowram/`, `drivers/swapcomp/`, `drivers/nmiwatch/`, `drivers/3c509b/`, `dist/shadram/`, `dist/swapcomp/`, `dist/refresh/`, `dist/xtide_mpd/`.
   - Add under `drivers/`: "- **`mach8_display/`** - the project's own Windows 95 display driver for the ATI Graphics Ultra. Runs the desktop in 86Box at 640x480, 800x600 and 1024x768 (256 colours); not yet run on the real 5160. Work log on [#49](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/49)."
   - Add: "- **`shadowram/`, `swapcomp/`, `nmiwatch/`** - SHADRAM.VXD (112 KB of the Inboard's idle shadow RAM for Windows), SWAPCOMP.VXD (compressed swap), NMIWATCH.VXD (diagnostic). Shipped builds in `dist/shadram/`, `dist/swapcomp/`; see FIXES.md."
6. L100-103 "No real-mode storage drivers are left... The whole of CONFIG.SYS, read off the machine's own CF card" (block dated 2026-09-21). The 5160 now boots through a FULL/LEAN/OFFLINE/SAFE menu (`tools/bootmenu/`, `docs/5160_tuned_config.md`) and `WAIT86.COM`, `IVT68FIX.COM`, `REFR64.COM` are in AUTOEXEC. Not checked against the card (not read here). Suggest dating the block: "CONFIG.SYS as read from the card on 2026-09-21; the boot menu and AUTOEXEC additions since are in `docs/5160_tuned_config.md`."
7. L499-500 "Most open issues carry a **Status** block at the top". 11 of 14 open issues do. #43, #47 have none; #50 is an idea (owner says current). Wording is acceptable; fix by adding blocks (section 3).
8. Open-issues table (L518-535):
   - #33 row: the issue's acceptance test is met (divisor 64 confirmed, published in `dist/refresh/`, #33 Status 2026-10-11). Remove the row if #33 is closed.
   - #41 row: "T130.MPD, HSFLOP.PDR, ELNK3.VXD" - XTIDEMP done (a67fefc4), HSFLOP not a target, T130 premise stale (IRQ 3). Replace: "#41 Pace the polling in every driver that spins. XTIDEMP.MPD done (2026-10-11); HSFLOP.PDR is not a target; T130.MPD now runs on IRQ 3; ELNK3.VXD not yet looked at."
   - #45 row: "A 1 ms tick costs 15%... the LS-120 is the one driver left that might gain from it". MinTimeSlice=54 was adopted 2026-10-11. Replace tail with: "`MinTimeSlice=54` adopted on the 5160 (2026-10-11). Open: DOS boxes and serial at the 55 ms tick."
   - #49 row: "Mach8 display driver: a glyph cache to cut text traffic on the 8-bit bus" understates it. Replace: "#49 Mach8 display driver: the project's own Windows 95 driver for the Graphics Ultra, working in 86Box; the first timed run against ATIM8 on the 5160 is still to come."
   - #35 row: add that SHADRAM v4 (112 KB) shipped; part 2 (memory-mapped storage) open.
   - Table omits #10's later state and #50 (parked idea; omission fine).
9. "Latest release - win95-sound-fixed-v2" (2026-08-24): the only releases are v1 and v2 (`gh release list`). Everything shipped since (XTIDEMP, SHADRAM, SWAPCOMP, REFR64, SD120PPD package) is in `dist/` only. README says to use v2 images; consistent, but owner may want a release (G10 is the owner's).
10. L128-143 EGACACHE paragraphs are accurate (match #35 Status) but long; no change needed.

FIXES.md (checked against #11/#12/#13 closures):

- "Not fixed" section, L418-427: `bad extended memory: 128k` entry says "Tracked at #11; upstream 86Box#7638 is closed NOT_PLANNED". #11 is closed "Fixed" via 86Box#7765 (merged 2026-08-25). The entry also says the shadow alias is "Root-caused, not yet fixed" (#12/#13). #12 and #13 are closed and fixed upstream in #7761, #7765, #7760. Replace both bullets with a one-line "Fixed upstream in 86Box#7760, #7761, #7765 (2026-08-25); needed only on older builds", or delete. Per CLAUDE.md "amend wrong docs", correct in place.
- No 86Box PR numbers elsewhere in FIXES.md.

## 3. Issues

50 issues: 14 open, 36 closed. Dates are from `gh`. "Status" = body begins with a `> ### Status` blockquote (all 14 open bodies except #43, #47, #50 do).

### Open issues, one line each

| # | Title | Last comment | Status block | Verdict |
|---|---|---|---|---|
| 10 | Idea: BIOS-extension shim (1982 ROMs) | 2026-10-04 | 2026-10-05 | KEEP; draft needed |
| 18 | Floppy garbage / media change | 2026-09-21 | 2026-09-20 | KEEP; draft needed |
| 23 | XTIDEMP cannot drive Hi-Speed map | 2026-09-20 | 2026-09-28 | KEEP; current |
| 28 | Per-component audit | 2026-10-04 | 2026-09-28 | KEEP; draft needed |
| 29 | Measure DMA | 2026-10-02 | 2026-10-03 | KEEP; current |
| 31 | SCSI disconnect and caches | 2026-09-24 | 2026-09-28 | KEEP; current |
| 33 | DRAM refresh tuning | none | 2026-10-11 | CLOSE (measured, shipped) |
| 35 | Shadow RAM inventory | 2026-10-04 | 2026-10-05 | KEEP; current |
| 41 | Pace the polling | 2026-10-11 | 2026-10-11 | KEEP; current |
| 43 | IOCHRDY cost | none | none | KEEP; draft needed |
| 45 | Win95 settings and timer tick | 2026-09-27 | 2026-10-11 | KEEP; current |
| 47 | FastDoom SB sound | 2026-10-05 | none | KEEP; draft needed |
| 48 | Optimisation timeline | 2026-10-11 | 2026-10-11 | KEEP; current |
| 49 | Mach8 display driver | 2026-10-12 | 2026-10-17 | KEEP; draft needed; retitle |
| 50 | BlueSCSI as SCSI bridge | none | none (idea, opened 2026-10-20) | KEEP; relabel idea |

Label gaps: README says issues carry `emulator`/`real-hardware`/`upstream`. Unlabelled open issues: #18, #45, #47, #48, #49, #50. Suggest `real-hardware` on #18 (note: Andrew's symptom is emulation, so also `emulator`), #45, #48; `optimisation` on #45, #47, #48; `idea` (new label) on #10 and #50; `driver` or `real-hardware`+`emulator` on #49. Owner's call whether to create an `idea` label.

### Per-issue detail

**#10 (idea: BIOS shim).** State: the 08NOV82 XT ROM works (merged as 86Box#8217, 2026-10-05). Cimon's real 5150 still stops at the PM switch; NMI ruled out (`docs/issue10_old_bios_2026_09_27.md` sections 9-10b, last commit 2026-10-04). `docs/next_session_2026_10_16.md`: "Simon's two boot logs stop at EBIOS like before; display driver and EMMEXCLUDE ruled out. HIMEM.SYS is stock. Owner replied." That is newer than the Status block and not on the issue. (Doc says "Simon", ledger and issue say "Cimon": check which.) GLaBIOS suggestion from @andrew-hoffman (2026-09-29) is "Not yet tried" in the ledger. Recommend KEEP OPEN, relabel `idea`. Draft:

```
### Status - 2026-10-21
The 08NOV82 XT ROM works on the Inboard (86Box#8217); the 5150 still stops at the switch to protected mode on two boards. NMI is ruled out.
2026-10-15: two further boot logs stop at EBIOS as before; the display driver and EMMEXCLUDE are ruled out, HIMEM.SYS is stock (docs/next_session_2026_10_16.md).
Not tried: GLaBIOS on the 5150 and 5160 machine types (suggested by @andrew-hoffman). Cause on the 5150 still unknown.
```

**#18 (floppy media change).** Status 2026-09-20 PARKED; later comment 2026-09-21 (MULTITRACK note). No work since; README "most useful thing anyone could pick up" is consistent. KEEP OPEN. Evidence the real 5160 is fine: BOOTLOG `Init Success hsflop.pdr`, md5 `8e695d00`. Draft (short):

```
### Status - 2026-10-21
Parked. The 5160 runs HSFLOP_XTDMA.PDR (8e695d00) and floppies work; this is @andrew-hoffman's emulation symptom and has not reproduced here.
The reproduction bed (86Box + Monster Floppy + the patched driver + a media change) is not built. Not measured: any media change on the real 5160.
MULTITRACK is at its default (ON) on the card; noted in the 2026-09-21 comment.
```

**#23 (Hi-Speed map).** Status 2026-09-28 "unchanged, not started"; nothing newer. KEEP (blocked on hardware and an 86Box model). No draft.

**#28 (per-component audit).** Status 2026-09-28; newer comment 2026-10-04 (display driver goes to #49). Since: SHADRAM v4, SWAPCOMP and REFR64 are on the card (`docs/5160_tuned_config.md`, 2026-10-11). KEEP. Draft:

```
### Status - 2026-10-21
Locked-memory pass done at the desk (docs/driver_audit_2026_09_28.md). The display driver is tracked on #49.
Card changes from the audit are listed in docs/5160_tuned_config.md (2026-10-11).
The six-question walk per component is not started.
```

**#29 (DMA).** Status 2026-10-03 current. KEEP; one question open (can DMA land in 0-64 KB). No draft.

**#31 (SCSI).** Status 2026-09-28 current; part 2 (MODE SENSE page 8) open. KEEP. No draft.

**#33 (DRAM refresh).** The issue's own acceptance ("a refresh interval with a passing pattern test and a measured throughput delta") is met: Status 2026-10-11, `dist/refresh/` (REFR64.COM, REFR18.COM), `docs/5160_tuned_config.md`. Recommend CLOSE with a note (owner's decision; the issue stays reopenable). Proposed closing comment:

```
Closing: the acceptance test is met. Divisor 64 passes a soak from a clean boot with no TSRs and a floppy format/copy/compare; bus transfers are 3-6% faster (VGA writes 1.98 -> 1.86 us/byte). Published in dist/refresh/ (REFR64.COM, REFR18.COM to undo); in the card's AUTOEXEC.BAT. Divisors above 64 are not measured. Thanks @andrew-hoffman for the steer on shadow and refresh.
```

(The last sentence is a suggestion only: check #33 has no comments from Andrew; he did not comment here. Drop it if unwanted.)

**#35 (shadow RAM).** Status 2026-10-05 current (SHADRAM v4 112 KB; 86Box #8216/#8219). Part 2 (memory-mapped storage) open, low priority. KEEP. No draft.

**#41 (polling).** Status 2026-10-11 current. KEEP. No draft.

**#43 (IOCHRDY).** No Status block, no comments. Docs: `docs/5160_checklist_2026_09_29.md` row "Remaining half is a probe run on the existing iowidth harness"; `docs/next_session_2026_10_03.md` lists it last. No measurement recorded after 2026-09-21. KEEP (outstanding, measurable). Draft:

```
### Status - 2026-10-21
Not started beyond the three-window fit in the body (1.978 us per bus cycle for the XT-CF ROM, 3.805 us for the Mach8, same sync term).
Remaining: a probe run on the existing iowidth harness (docs/5160_checklist_2026_09_29.md). 86Box still models no bus stall, so the bed cannot show it.
Not measured: the cost of IOCHRDY per device at a fixed byte count.
```

**#45 (timer tick).** Status 2026-10-11 current. KEEP; rows 3, 4, 7 and DOS boxes/serial open. No draft.

**#47 (FastDoom).** No Status block; one comment (Andrew, 2026-10-05, 42 MHz oscillator video; owner reply same day). `docs/5160_checklist_2026_09_29.md`: "settings sweep: bed, then frame rates on the 5160" - no result recorded. FastDoom `-timedemo demo1` was used as a workload for the XTOUT test (`docs/next_session_2026_09_28.md`). KEEP, relabel `optimisation`. Draft:

```
### Status - 2026-10-21
Not started as a sweep. FastDoom 1.2.0 runs with -xt and no sound; with Sound Blaster sound it freezes (body).
The CPU is at 83.5 MHz (CHKCPU) with the 40 MHz crystal and the Blue Lightning at 2x; 3x fails. Settings sweep in the bed, then frame rates on the 5160, are not done.
Not measured: any frame rate with sound.
```

**#48 (timeline).** Status 2026-10-11. Newer entries not yet added: Mach8 work (#49), `ec1d733` pixel transfers. KEEP; optional one-line addition, no block drafted.

**#49 (Mach8 driver).** Status 2026-10-17 predates: DIBENG polygon fault fix `097fc8a` (2026-10-20), shutdown BSOD fix, baseline on `86box_auto`, device-bitmap proxies `0dd281e`/`bf3baf1`, string-keyed text `eb87aa9`, off-left text `8aaa0fd`, hatched brushes `eb6dd90`, scan spans `5fde707`; plan of attack in `docs/worklist.md`; handoff `docs/next_session_2026_10_21.md`. Four 86Box PRs merged since the 10-12 comment (#8294, #8339, #8351, #8394). Title ("a glyph cache") is stale. KEEP; suggest title "Mach8 display driver: the project's own Windows 95 driver for the Graphics Ultra". Draft (numbers from the handoff, all in 86Box, none on the 5160):

```
### Status - 2026-10-21
The driver (drivers/mach8_display/src) runs all 35 M8BENCH tests in 86Box; the DIB Engine polygon fault is fixed (097fc8a) and a shutdown BSOD is fixed. Not yet run on the real 5160.
Against ATIM8 at 1024x768 (docs/next_session_2026_10_21.md): engine paths 0.96-1.17x; hatched fills now 302/s vs ATI 386 (12 before); ellipse 51 vs 68; floodfill 5.8 vs 10.5; polygon 15 vs 99.
Still behind: dashed lines 29x, mono bitmaps 9x, text 1.4-2.8x, large bitmaps 11x. Next: PATT_DATA dashes, setup-register shadowing, per-glyph text cache.
Not measured: bus traffic per draw against ATIM8, and anything on the real card.
```

**#50 (BlueSCSI).** Opened 2026-10-20, current; "parked until the Mach8 driver is done". KEEP, relabel `idea`. No draft.

### Closed issues worth a look

- **#36** (fatcp.py/fatls.py claim FAT12 but are FAT16 only): closed with no comment. `tools/fatcp.py` docstring now says "FAT12/16" and `4c35149 FAT tools: read FAT12 chains and unpartitioned floppies` exists. Fix looks done; add a closing note citing `4c35149` (not tested by this audit).
- **#25** (floppy geometry): closed; last comment is "Fix routes, costed". README cites it as fixed 2026-09-07 (Trantor ROM moved to DA000). Add a one-line closing note pointing to the README paragraph and `docs/evidence/int13_ah08_ROOT_CAUSE_2026-09-07.md`.
- **#12** (shadow alias wrong address): closed; the last comments are a chat about OSR2. The fix landed upstream (86Box#7761, #7765) but is not stated on the issue's final comment. Add a closing note.
- **#34** (display mode as a bus lever): closed "on the owner's decision" with the per-mode benchmark explicitly not done; the closing note says so. Acceptable. Note that #49 now makes the 800x600 vs 1024x768 question live again (`docs/next_session_2026_10_21.md` 1024x768 tests).
- **#46** (SD120PPD stall): closed with fix shipped (`d4e9306`) and the unplugged case recorded as LEAN-boot. Acceptable.
- **#27, #38** closed "not planned" with reasons. Acceptable.
- **#26** closed 2026-09-21 as a tracking issue; the ledger still lists VOGONS replies (disruptor, douglar, red-ray) under "Owed a reply - VOGONS". Consistent with "owner's to send", but see siv/red-ray note in memory: reply not sent.
- **#7** closed with Status block 2026-09-27 (WAIT86). Fine; last comment is older than the block, but the block is the closing note.

## 4. 86Box upstream submissions

`gh pr list --repo 86Box/86Box --author Mike1978uk`: 22 PRs, all MERGED, none open.

| PR | Merged | Title |
|---|---|---|
| 7626 | 2026-08-06 | Add the Intel Inboard 386/PC accelerator card for the IBM XT |
| 7749 | 2026-08-23 | Inboard 386/PC: fix POST 101, non-486BL CPUs |
| 7760 | 2026-08-25 | mem: NULL deref on a page table in unbacked memory |
| 7761 | 2026-08-25 | inboard386: reserved block 128 KB at 0x5E0000 |
| 7765 | 2026-08-25 | inboard386: high shadow alias reads shadow RAM |
| 7766 | 2026-08-25 | inboard386: no 5161 by default |
| 7771 | 2026-08-25 | dma: honour dma_force_xt |
| 7858 | 2026-09-04 | XT-IDE: open a log handle |
| 8010 | 2026-09-21 | Parallel-port LS-120, Shuttle EPAT bridge, ECP path |
| 8012 | 2026-09-21 | cdrom: CDROM_BUS_LPT, BackPack bridge |
| 8076 | 2026-09-24 | Add the 3Com EtherLink III ISA (3C509B) |
| 8078 | 2026-09-25 | Fix the LPT CD-ROM and removable disk bridges |
| 8099 | 2026-09-25 | lpt_epat: vendor LS-120 drivers |
| 8102 | 2026-09-25 | 3C509B: keep the EEPROM's IRQ across a PnP reset |
| 8216 | 2026-10-05 | Inboard: 256 KB extended memory, gate its windows |
| 8217 | 2026-10-05 | Inboard: offer the 08NOV82 XT BIOS again |
| 8219 | 2026-10-05 | Inboard: pace memory on the wait-state field |
| 8289 | 2026-10-11 | ATI Graphics Ultra (Mach8) measured on a real card |
| 8294 | 2026-10-12 | ATI Mach8: default EEPROM at 60 Hz |
| 8339 | 2026-10-15 | ATI Mach8: FRGD_MIX after DP_CONFIG |
| 8351 | 2026-10-16 | ATI Mach8: command 4 width from MAJ_AXIS_PCNT |
| 8394 | 2026-10-18 | mach8: read data available on one-pixel line reads |

State check against the docs:

- README: lists 21 of 22; #8394 missing (section 2, item 1). All 21 listed are described as merged with correct dates. README says #8076 merged 2026-09-24 (matches).
- FIXES.md: no PR numbers except #7638 (not ours).
- `docs/worklist.md`: A7 (#8289), A7b (#8294), A11 (#8339), A12 (#8351), A13 (#8394) all ticked and described as merged. Correct. Header line for A13 has stray `**` after "since #8289." (L105) - cosmetic.
- `docs/next_session_2026_10_21.md` does not mention any 86Box PR as open. Correct.
- #8396 (dafugg, `wait text` fix) is not ours; ledger and `docs/86box_mcp_on_windows.md` describe it as merged. Correct.
- No PR described with the wrong state.

Local work that docs say is not yet upstream:

1. **pic.c: deliver IRQ 9 as IRQ 2 on machines without a slave** - `ef082884b` (`86box_3c509b`), rebased as `e2c3c0c3b` on `86box_auto` branch `auto-irq9`. Recorded "not submitted" in `docs/next_session_2026_09_25.md`; `docs/next_session_2026_09_26.md` L27 says "submit the unsubmitted 4-line pic.c fix" if B is faster. Used in the current bench bed (`docs/next_session_2026_10_21.md`: without it the 3C509B never interrupts). No PR found (`gh pr list` for the author shows none; a title search found nothing). The README claim "every Inboard change is merged" is contradicted by this (section 2, item 2). Needs an owner decision: submit or record why not. PR text is the owner's.
2. **XT-CF stride 2 / Lo-tech XT-CF at 0x300 stride 2** - `docs/worklist.md` A0: carried on the Mach8 branch `inboard-ext-256k-diag`. Whether this is meant for upstream is not stated; the 10-21 handoff does not list it. Ask.
3. **Mach8 follow-ups** (not yet PRs): A8 (siblings of #8289: widen engine fixes to the 8514/A ULTRA add-on and the Mach32; TC1995 asked 2026-10-12), A10 (host-side harness, skiretic), INMOS IMSG176 DAC model (A7 follow-up), A3 items (6)-(9) found 2026-10-11 (fixed-pattern fill draws one row of four; reads outside the scissors must return FFh; blit source past x=1023; SUBSYS_STAT monitor-ID bit). Open in worklist; none submitted.
4. **EPAT EPP wiring** (`epat-epp` branch): #38 closed as not planned; no PR intended.
5. Local branches without a PR in `86box_upstream`: `lpt-epat-bridge`, `cpu-fetch` (upstream fixed it independently, `acc82a87f`), diagnostics branches. Nothing owed.

## 5. Contributor links not under docs/

URLs in comments on open issues by anyone other than Mike1978uk (only @andrew-hoffman has commented). Checked by exact URL, then by site name.

| Issue | Who | Date | URL | In docs/? |
|---|---|---|---|---|
| #18 | andrew-hoffman | 2026-08-28 | https://github.com/user-attachments/files/31559205/corrupt.img.zip | Not by URL or name. The analysis is in the 2026-08-29 comment; no doc names the file. Add to ledger/`docs/` index |
| #35 | andrew-hoffman | 2026-09-29 | https://glabios.org/ | Not by this URL. The ledger links https://github.com/640-KB/GLaBIOS instead (same project). Adequate; add glabios.org to `docs/resources_and_sources.md` |
| #49 | andrew-hoffman | 2026-10-05 | https://github.com/user-attachments/files/33041761/m8utl.zip | Not by URL; `m8utl` is named in `docs/ati_test_com_notes.md` and `docs/INBOARD_86BOX_PORT_PLAN.md` |
| #49 | andrew-hoffman | 2026-10-05 | https://www.dosdays.co.uk/topics/Manufacturers/ati/ati_graphics_ultra.php | Not this page. `docs/resources_and_sources.md` L133 links the ati_mach8.php page. Add this one (two VRAM pools) |
| #49 | andrew-hoffman | 2026-10-05 | https://theretroweb.com/expansioncards/s/ati-graphics-ultra | Site named in `docs/resources_and_sources.md` and `next_session_2026_10_08.md`; exact URL not found. Add |
| #49 | andrew-hoffman | 2026-10-07 | https://github.com/86Box/86Box/issues/6695 | Cited as "86Box#6695" in the ledger and captures; no full URL. Adequate |
| #49 | andrew-hoffman | 2026-10-11 | https://github.com/86Box/86Box/commit/13751de8cb75ff5665e591ba269a8a6fa6092a66 | Hash `13751de` in the ledger and `next_session_2026_10_12.md`; no full URL. Adequate |

URLs already in docs/: faqs.org Win95 FAQ part 11, computeradsfromthepast substack, both YouTube links on #23, os2museum "more fun with ISA DMA", FastDoom YouTube (#47), velocity9x, skiretic/86Box#23. Per CLAUDE.md ("Always cite with the URL"), the 5 above are the gaps; the three most useful to add are the dosdays and retroweb pages (what they gave: two VRAM pools, 512 KB-1 MB in the 38800 plus 512 KB on the VGA ASIC) and the corrupt.img.zip attachment name.

## 6. Other stale information

1. `docs/next_session_2026_10_21.md` contradicts itself:
   - "Next, in order: 1. Plan of attack in docs/worklist.md. Start: DDK device bitmaps (item 1)." Item 1 is done earlier in the same file (`0dd281e`, `bf3baf1`); items 2-4 are under way (`eb87aa9`, `8aaa0fd`, `eb6dd90`, `5fde707`).
   - "Scan spans (next commit)": landed as `5fde707`.
   - Title says "from 2026-10-20" but the file name is 10_21.
   Fix in place: replace the "Next, in order" list with the gaps list from the plan items (lines "Still behind" and commit `16bdb78`).
2. `docs/worklist.md` header: "Items (1)-(2) below done (`ec1d733`: scr_to_mem 8.2x...)" is ambiguous (those are the 10-17 "First" items, not the 10-20 plan-of-attack items 1-2). Plan item 1 (device bitmaps) and parts of items 2-4 are done per the handoff but the list shows no ticks. Suggest "done so far" annotations on items 1-4.
3. `docs/worklist.md` A3 is unticked but carries many struck items; A8, A10 unticked. Fine, but A3's long line mixes history with tasks; consider moving history to a doc.
4. `docs/5160_tuned_config.md` is dated 2026-10-11; `MinTimeSlice=54` "ON TRIAL" in MEMORY.md is "confirmed" in the doc. Fine. Its "Next measurements" line (DOS boxes and serial at the 55 ms tick) matches #45.
5. `docs/5160_checklist_2026_09_29.md` item 3 (Dial-Up Adapter removal): the memory index says the adapter is already gone (no PPPMAC in BOOTLOG) so the item is stale. Not re-verified here; owner to confirm and tick or strike. Also #28 Status still says "First trim: Dial-Up Adapter, 109 KB" - same point.
6. `docs/contributor_input_ledger.md` L26: GLaBIOS "Not yet tried" - still true. L337: "Reply owed on #49 - draft given to the owner 2026-10-05, not posted" - #49 comments on 2026-10-05, 2026-10-07, 2026-10-08, 2026-10-11, 2026-10-12 answer Andrew; check this line is superseded. Ledger also lists "Told?" for @TC1995 and OBattler via Discord 2026-10-18 (L406-408) - up to date.
7. `README.md` sections "Upstream" and "What works" omit that two Mach8-related items are user-visible: the 86Box Graphics Ultra now passes ATI's TEST.COM (covered by the #8289 row), but "What works" does not say that Windows 95 runs the project's own Mach8 driver in 86Box only. Covered by section 2, item 5.
8. `.claude/skills` counts: README names three skills; the repo has three. Correct. Memory index says "techniques 1-145" (not a repo file; ignore).
9. `docs/windows95_on_inboard386pc_writeup.md` (README's main link) was not re-read; it predates the 2026-09/10 drivers. Not checked for contradictions. Recommend a skim before pointing anyone new at it.

## TODO for owner approval (most important first)

1. README: add 86Box#8394 and change "Twenty-one" to "Twenty-two" (section 2, item 1). Pure fact, low risk.
2. Decide the `pic.c` IRQ 9 as IRQ 2 change: submit upstream or record it as local-only; then fix README L271 wording (sections 2 item 2, 4 item 1).
3. Approve status blocks for #10, #18, #28, #43, #47, #49 (section 3 drafts). #49 first: it is the active work and its block is four days of commits behind.
4. Decide to close #33 (acceptance met) and drop its README row (section 3).
5. FIXES.md "Not fixed" bullets for #11/#12/#13 are wrong now; correct in place (section 2, FIXES.md).
6. README: correct the technique count (127 to 151), the #41/#45/#49/#35 table rows, and add `drivers/mach8_display/`, `shadowram/`, `swapcomp/`, `nmiwatch/` to the structure list (section 2, items 3-5, 8).
7. Closing notes for #36, #25, #12; retitle #49; labels on #10, #18, #45, #47, #48, #49, #50 (section 3).
8. Fix `docs/next_session_2026_10_21.md` "Next, in order" and annotate worklist plan items (section 6, items 1-2).
9. Add the five missing contributor URLs to `docs/resources_and_sources.md` / the ledger (section 5).
10. Optional: note that shipped `XTIDEMP.MPD` `a67fefc4` was built from a dirty tree (section 1); consider a release after the `dist/` additions since 2026-08-24 (section 2, item 9; G10 is the owner's).
