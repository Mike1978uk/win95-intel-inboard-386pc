# Next session - midday handoff from 2026-09-25

## ▶ NEXT: Andrew's T130B A/B (#42) - planned, not run

Owner's priority from here: **Win95 on the XT, not 86Box quirks** (fix only his own 86Box code).
The A/B decides whether #42 (IRQ 9 -> 2) is worth doing. All in 86Box, no 5160 needed.

- Bed `vm_t130b`: Inboard XT, Win95 `t130b.img`, SCSI disk `scsi_test.img` (64 MB, FAT16,
  `1997_5400rpm`), no NIC, SB Pro on 5, T130B `irq = 00`. Masters: `*_master.img`.
- The installed node is OUR XT INF (`OEM1.INF`, section `T_ISA`, `Polling=1`) and
  `drivers/trantor_t130b/T130-XT.INF` **has no `IRQConfig`** - that node can never get an IRQ.
- Run A: as installed (polled). Run B: bed `irq = 3` (JP3 on 3); owner removes the node, reboots,
  Add New Hardware -> Have Disk with the **vendor `T130.INF`** (`IRQConfig=3,5,7`, no Polling),
  confirms IRQ 3 in Device Manager. Stage `T130.INF` + `T130.MPD` to e.g. `C:\T130VND\` (CRLF).
- Workload: a staged CRLF batch `T130AB.BAT` - `echo.|time` stamp, `copy` a fixed set (e.g.
  `C:\WIN95\*.*`, ~1 MB+, or a larger file) to the SCSI disk, stamp again - guest clock, same in
  both runs. Owner runs it from a DOS box.
- Answer Andrew with the result, with thanks; the #8076-merged note is owed too.
- If B is faster: submit the unsubmitted 4-line `pic.c` fix (`ef082884b` on `86box_3c509b`:
  without a slave, IRQ 9 is IRQ 2) as its own PR, then VPICD + `ELNK3` 9->2 with WDEB386.
- `T130.MPD` registers `HwInterrupt` (`0x10A9C`); JP3 offers 3/5/7 (established).

## ✅ Update 2026-09-25 evening: #8099 MERGED; 3C509B IRQ regression fixed as 86Box#8102 (open)

**#8099 merged** 14:37 UTC by OBattler (`14c5ec7f7`); README updated (`3f5b07c`). #44 on this repo
is still open - owner's call to close.

**86Box#8102** (branch `3c509b-reset-keeps-irq`, `075bb5d30`, worktree `86box_master`): since
#8087, every ID-sequence global reset (C0h) resets the card's PnP state, and the callback wrote
PnP IRQ 0 over the EEPROM's - the card activated with no interrupt, the DOS packet driver timed
out. Fix: apply PnP resources only on PnP activate. Independent of the `pnp` option.
Gates: G1 G4 G5 G6 G7 G8 G9 G10 pass; G2 not run as a pair; G3 n/a (no UI change).
Runs: XT `vm_wfw3c509` PnP off and on (DOS DHCP, WfW 3.11 + `ELNK3.DOS` browses);
AT `vm_3c509b/at95` Win95 PnP on (assigned IRQ 5 / 210h / C8000h, browses).

**The WfW "freeze" on #8076** did not reproduce once the emulated **SB Pro v2** was removed - the
same wallpaper-and-hourglass hang the port plan recorded in July on this image (never
investigated; open). Ruled out on the way: COM2 on IRQ 3, slirp's ICMP replies to NetBIOS
broadcasts, the `pnp` setting. The reporter's WfW **3.1** case is unconfirmed; #8086 fixed an
`ELNK3.386` hang this image never exercises.

Beds: `vm_wfw3c509` (new; copy of `win311_test_copy.img`, 63/16/7785, no SB, COM2 off; configs
`86box.cfg.master` and `86box.cfg.master_withsb`). `vm_3c509b/at95` uuid line dropped (backup
`86box.cfg.pre_run3`); `lpttest3` uuid dropped (`86box.cfg.bak_uuid`). Diagnostics used:
`docs/patches/3c509b_diag_idseq_eeprom_frames.patch` (ID/EEPROM logs, frame dump, ICMP drop).

## ✅ Update 2026-09-25 afternoon: #44 SUBMITTED as 86Box#8099

Head `f361a8a02` (three commits on `lpt-epat-vendor-dos`, fork `Mike1978uk/86Box`), opened by
Claude with the owner's go-ahead; the owner overrode "never post" and the full not-tested list.
The engine was not the blocker: a lost interrupt, an arm-gated unit query, and register value
22h eaten by the unlock matcher. Detail: `docs/sd120ppd_sys_load_path_2026_09_25.md`.

| gate | result |
|---|---|
| G1 | pass |
| G2 | **not run** |
| G3 | not run (no UI change in the diff) |
| G4 | pass - vendor DOS (default and `/di`) and vendor Windows MPD from `dist/ls120_vendor`, writes checked on host |
| G5 | pass |
| G6 | pass |
| G7 | built on base `1c7e3a573` only; **not rebased** onto current master (owner: push as-is) |
| G8 | pass |
| G9 | source at HEAD byte-identical to the tested source; exe predates the commits |
| G10 | partial - owner drove all runs; Settings/Media menu/Device Manager names not checked |

Serial `X713CA0B4594` kept (owner). Test 3 first ran on our RETIRED `LS120MP.MPD` because this
handoff named it - the regression bed is now on the vendor MPD (`vm_ls120win`, backups
`ls120win_preVendorStage.img`, `rd_preRegr44d.img`). `vm_ls120dos44` is on the default line.

## (superseded) #44 was HELD - one task closes before it can be submitted

The vendor LS-120 DOS driver works in 86Box **only with `/di`** (polled). With the line a
stranger copies (`/IRQ:7`, no `/di`) the driver hands transfers to the EPAT's own **transfer
engine**, which is not modelled, and hangs on the splash screen. That breaks repo-hygiene
section 7 ("people will try it and say it's failed"), so the owner held the push.

**Task 1: model the transfer engine.** Read it statically first and write it into
`docs/sd120ppd_sys_load_path_2026_09_25.md` before any code:
- setup `0x3357`: internal regs 6/7 (size), reg `0x14` = bytes/2 - 1, reg `0x12` bits 0-1
  (direction + start), window reg `0x18` = direction code (`0x26`/`0x22` on the `E2` chip);
- wait/finish `0x368C`, `0x36B6` (reg `0x12` bit 1, reg `0x13` bit 1), and how the data is
  then collected from the bridge;
- completion by interrupt: already modelled (`CPP 48` arm, INTRQ -> IRQ 7, `CPP 08|u` byte).
Then implement, and run: default line (pass = one drive letter, files read), then `/di`,
then the Windows regression. Owner drives every check (G10), **no startup batch**.

## State of #44

| | |
|---|---|
| PR branch | `lpt-epat-vendor-dos` in worktree `86box_epat44`, one commit `876b6c53b` on master `1c7e3a573`. **Not pushed** |
| builds | yes, on current master, no new warnings (G7) - `86box_epat44/build_log`, 11:13 |
| tested | on `b3259c62d` (same code before three comment fixes and a dead-code removal): DOS `/di` one drive `D:` reads (B15); Windows `LS120MP.MPD` mounts `J:`, reads, a write lands. **Retest on the final HEAD (G9)** |
| not run | G2 (nothing on LPT, and the BackPack, unchanged) |
| open decision | the identify block carries the owner's drive serial `X713CA0B4594` - keep or genericise |
| PR text | drafted in the session; drop the ECP and LPT2 lines (owner) |
| the read | `docs/sd120ppd_sys_load_path_2026_09_25.md`; measured values `drivers/imation_ls120/MEASURED_FACTS.md` sections 8-9 |

Old branch `epat-cpp-40-50` (`35bf8fe5c`, `b3259c62d`) is the tested history; keep it until
the PR merges.

## After the PR merges

Add it to the README's merged table and the contributor ledger: what was submitted, what merged.

## Other state

- **5160:** `CONFIG.SYS` restored by the owner (09-25). `C:\MEMD.TXT` stays on the card by the
  owner's choice - harmless, do not raise it again.
- `vm_ls120win\86box.cfg.master` corrected from type 6 (SparQ since the renumber) to 7.
- WfW 3.1 report on #8076: not started; our 3.11 image shares the driver base.
- README updated this morning (`d816909`): #8078 merged, IRQ 9 -> 2 is Windows-side, XT caveat.
