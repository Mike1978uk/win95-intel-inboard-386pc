# Next session - handoff from 2026-09-24 (evening)

## Priorities

1. **Owner: approve and open the LPT bridges PR.** Branch `fix-lpt-bridges` in `86box_lptfix/`,
   8 commits on master `f7683576a`, head `84ebcdfe9`. Local only - not pushed. Draft text below.
2. **#42 (IRQ 9 -> 2)** - unchanged from the earlier handoff.
3. **The vendor LS-120 DOS driver in the emulator** - a new task, below.

## The PR branch - `fix-lpt-bridges`

| commit | fixes |
|---|---|
| `4ee2015ed` | IDE/SCSI CD-ROMs fell through into the LPT case and each created a BackPack on LPT1; forced `ENABLE_*_LOG 1` in `lpt_bpck.c`/`lpt_epat.c` |
| `b6dc7b55b` | Media menu said "(Unknown Bus)" for LPT CD-ROMs and removable disks |
| `9ce35056b` | SuperDisk 120 INQUIRY answered "86Box 86B_RD00"; now `MATSHITA` / `LS-120 COSM   04` / `0270`, as the real drive (`MEASURED_FACTS.md:23`) |
| `f723baf2d` | Settings offered SCSI models for an LPT CD-ROM, and saving moved a drive whose model named another bus off LPT |
| `04d666a0b` | every LPT CD-ROM command rescheduled `ide_drives[ide_channel]` (IDE channel 0) |
| `bcc0c1951` | an LPT CD-ROM was never initialised: speed 0, "0x speed" fatal on the first long seek (owner hit it browsing a disc) |
| `226a80f46` | `cdrom_NN_lpt_port` was read but never saved |
| `84ebcdfe9` | both bridges took their port from a leftover device option (LPT1), not the drive's port |

## Gate results (repo-hygiene section 10), on head `84ebcdfe9` unless stated

| gate | result |
|---|---|
| G1 debug off | pass - master wrote 42.7 MB of log in a Windows boot, the fix 991 bytes |
| G2 absent means unchanged | pass - no LPT device: master attaches a phantom BackPack, fix none; positive control attaches (run on `4ee2015ed`; later commits do not touch that path) |
| G3 Settings | pass, owner: LPT offered for both, ATAPI models only (Toshiba XM-1502B, no `[SCSI-x]`), LPT2 kept across save and reload |
| G4 working device | pass, owner: LS-120 (Windows miniport) read/write + Device Manager "MATSHITA LS-120 COSM 04" (L1, `9ce35056b`); BackPack alone read past the LBA that crashed master (B1, `bcc0c1951`); BackPack found its drive on LPT2 (C1) |
| G5 comments | pass |
| G6 claims | not tested: LS-120 with the vendor DOS driver (fails on master and fix alike, below); both drives together under Windows (out of scope, owner) |
| G7 builds on master | pass, no new warnings |
| G8 minimal diff | pass |
| G9 last run on HEAD | the LS-120 Windows check ran on `9ce35056b`; C1 (BackPack on LPT2) and the DOS runs on `84ebcdfe9` |
| G10 owner by hand | done for LS-120 (L1), BackPack (B1), Settings (G3) |

## Found, not part of the PR - report, do not fix

- **Removable-disk types are positional and upstream renumbered them** (SyQuest added, ZIP 750 in
  `#if 0`): an old config's 5/6/7 is now a SyJet/SparQ/SuperDisk 120. LS-120 is **7** on master.
- `tools/ls120_bed_run.ps1:95` stops every 86Box. Use `bed_launch.ps1`.
- `vm_bpck/86box.cfg.master` is the old Ditto config; `win95_at_master.img` is pre-first-run.
- `drivers/3c509b/3c509.asm` shows modified - line endings only.

## New task: the vendor LS-120 DOS driver in 86Box

`SD120PPD.SYS` + `ASPIHDRM.SYS` on the AT bed: "Error initialising adapter / Driver not
installed", **identical on master** (so not the PR). The bridge log shows the preamble and the unit
scan (unit 0 answers `FFAA`), then no `CPP(0xE0|unit)` connect. The model logs `CPP(0x40)` and
`CPP(0x50)` as unknown commands; `TRANSPORT_SPEC.md:846-866` decodes the sequence. Next: implement
those two in `lpt_epat.c`, then trace the driver past the scan. Bed: `python tools/bed_stage_at.py
<dir> --lpt1 ls120-dos --dos`, launch with `tools/bed_launch.ps1 -AllowMouse`.

## Tools added today

`tools/bed_launch.ps1` (every check that caused a desktop dialog, stale-exe warning),
`tools/bed_stage_at.py`, `tools/ls120_bed_stage.ps1 -Bed -NoStartup`, fixtures `BPWIN.BAT`,
`LSENUMJW.BAT`, `LSFIND.BAT`. Skills: repo-hygiene G1-G10, inboard-hw-debug 131 section 0 and 132.

## PR draft - owner's to edit and post

**Title:** Fix the LPT CD-ROM and removable disk bridges

#8010 and #8012 (mine) added the LS-120 and BackPack parallel-port bridges. This fixes what they
got wrong:

- every IDE or SCSI CD-ROM also created a BackPack bridge on LPT1 (a fall-through in
  `cdrom_hard_reset()`);
- an LPT CD-ROM was never initialised, so its first long seek stopped the emulator with "0x speed";
- each LPT CD-ROM command rescheduled the callback of IDE channel 0;
- both bridges attached to LPT1 whatever port the drive was set to, and a CD-ROM's LPT port was
  never saved;
- Settings offered SCSI drive models for an LPT CD-ROM, and saving moved such a drive off LPT;
- the Media menu showed "(Unknown Bus)" for LPT drives;
- the SuperDisk 120 reported "86Box 86B_RD00"; it now answers as the real drive does
  (MATSHITA LS-120 COSM 04, read from one over its bridge);
- logging was forced on in both bridges (a 42 MB log in one Windows boot).

Tested in 86Box on current master, with and without this: an AT with Windows 95 and MS-DOS 7, and
the IBM XT with an Intel Inboard 386/PC. LS-120 with the Windows miniport (drive letter, read and
write, Device Manager name); BackPack with the Micro Solutions DOS driver and MSCDEX, alone and on
LPT2; no LPT device alongside a SCSI CD-ROM; attaching both from Settings and saving.

Not tested: the vendor LS-120 DOS driver, which does not initialise in 86Box before or after this
change; both drives in use together under Windows.
