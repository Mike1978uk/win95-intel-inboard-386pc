# Next session - handoff from 2026-09-28 (early hours)

## ▶ START HERE

**Superseded by `docs/next_session_2026_09_29b.md`.**

**Updated 2026-09-28 evening:** items 1 and 2 below are done at the desk. At the machine,
work through `docs/5160_checklist_2026_09_29.md`; findings in `docs/driver_audit_2026_09_28.md`.

1. **RAM track, step 1: break down the ~2 MB locked memory.** The baseline
   (`docs/ram_baseline_2026_09_28.md`) shows ~2 MB of the 5 MB can never be paged. Read
   `BOOTLOG.TXT`, `SYSTEM.INI [386Enh]` and `IOSUBSYS` on the CF for what loads, and list what this
   machine does not use. Desk work first; one trim per boot, `RAMBASE` after each.
2. **Fix the workload's wait.** `CHOICE /T` spins its DOS box at 100% CPU. Swap it for a wait that
   sleeps (a tiny Win32 `SLEEP.EXE` alongside PERFLOG) before the next `RAMBASE` run.
3. **#45 rows 3 and 4** (read-ahead slider, CD-ROM supplemental cache): each a hands-off A/B with
   `T130AB` + `PERFLOG`. Row 4 also frees RAM, so it fits the RAM track.
4. Still waiting on others: Cimon's reply on #10 (DMA probe, 5150 `BOOTLOG.TXT`).

## Done 2026-09-27/28 (details in `docs/next_session_2026_09_28.md`)

| | result | where |
|---|---|---|
| XTOUT clear, `1000h:1 = 8C` | FastDoom 1.1% faster; Windows stable; ships in `dist/` (`a0f7d44`) | #40 **closed** |
| SNP, `1000h:0 = 8A` | hangs the 5160 - never set it | #40, skill technique 135 |
| VCACHE cap 512-1024 KB | copies 8% faster; kept | #45 row 1 |
| CD writer auto insert off | 1.2% idle CPU back; changer was already off | #45 row 2 |
| `HSFLOP.PDR` polling | nothing to pace; A17 downgraded | #41, `docs/hsflop_poll_audit_2026_09_27.md` |
| Floppy B: in DOS mode | 23.2 KB/s against a ~45 KB/s ceiling; disk needs reinserting per use | handoff 09-28 |
| RAM baseline A | commit 10.3 -> 16.3 MB, ~3,100 page-ins for four programs, ~2 MB locked | `docs/ram_baseline_2026_09_28.md` |
| Stale fix-inventory lines | 1982 BIOS, HSFLOP, floppies, 3C509B corrected | `docs/what_worked_and_what_didnt.md` |

## New tools

- `tools/perflog/` - `PERFLOG.EXE label seconds [interval]` logs every System Monitor counter to
  `C:\PERFLOG.CSV`; `perflog_table.py` summarises; `RAMBASE.BAT` is the workload. On the CF.
- `tools/le_poll_scan.py` - poll loops in an LE VxD/PDR.

## On the CF now

`CPUSET.BAT` (8C) and `CPUSET.B9C` (old), `SNPTEST.BAT`, `SNPDUMP.SCR`, `SNP.TXT`, `SNP92.TMP`
(400 KB, deletable), `PERFLOG.EXE`, `PERFLOG.CSV`, `RAMBASE.BAT`, `RBA1.TXT`, `RBA2.TXT`,
`TIMERRES.TXT` (the nine auto-insert runs). `SYSTEM.INI` carries the VCACHE cap; `SYSTEM.BVC` is
the uncapped backup.

## Optimisation order, updated

1-3 done (SW1, #40 registers, VCACHE). 4. #34 display mode - owner's comfort call. 5. done
(auto insert). 6. #33 DRAM refresh - a memory test that can fail first. 7. #41: `T130.MPD` (A18),
owner-approved. 8. #31 SCSI caches and disconnect (Windows allows disconnect on the changer, Zip
and MO). 9. #29 items 2-4. **RAM track** runs alongside from now on.
