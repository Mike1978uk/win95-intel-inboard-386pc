# RAM baseline on the 5160 - 2026-09-28, run A

First measurement for the RAM-footprint track: how much a 5 MB Windows 95 pages under ordinary
use, before anything is trimmed. Setup as shipped today: VCACHE capped 512-1024 KB, `1000h:1 = 8C`,
full SCSI stack powered, CD writer auto insert off.

## Method

`C:\RAMBASE.BAT A`, run first after boot from an MS-DOS Prompt, hands off:
`PERFLOG` (`tools/perflog/`) sampling every 2 s for 300 s; the `T130AB` copies
(`C:\WINDOWS\COMMAND` onto C:, three times); Paint Shop Pro, WordPad, Notepad and Paint opened in
turn; the copies again with all four open. Raw log: `docs/captures/perflog_5160_2026-09-28_A.csv`
(151 rows). Summary: `python tools/perflog/perflog_table.py <csv>`.

## Result

| | after boot | four programs open |
|---|---|---|
| committed memory | 10.3 MB | 16.3 MB |
| swapfile in use | 2.8 MB | 5.6 MB |
| locked (never paged) | 1.9 MB | 2.0-2.5 MB |
| disk cache | 0.55 MB | 0.52 MB (the 512 KB floor) |
| free | 0 | 0 |
| page-ins, running count | 1854 | 5227 |
| page-outs, running count | 787 | 1969 |

- **Opening the four programs cost ~3,100 page-ins and ~1,100 page-outs**, about 12 MB and 4.5 MB
  at 4 KB a page. Page-ins include pages read from the program files themselves, not only swap.
- **The copies did not page.** The count was flat through both sets. They took 7.74 / 7.53 /
  7.14 s straight after boot and 7.09 / 6.70 / 6.70 s with the programs open; the first set shares
  the machine with the end of startup.
- **Locked memory is ~2 MB of the 5 MB**: VxDs, drivers and buffers that can never be paged. That
  is the first thing to break down - it bounds what any trimming can win.
- **The disk cache sits on its 512 KB floor** once programs load, so the cap's minimum is what the
  cache actually gets.
- **`KERNEL\CPUUsage` reads 100% while the batch runs**: `CHOICE /T` spins in its DOS box. It does
  not affect the paging figures, but the next workload should wait some other way.

## Next

1. Break down the 2 MB locked: which VxDs and drivers hold it (`MEM /D` equivalents for Windows:
   `SYSTEM.INI [386Enh]` and `BOOTLOG.TXT` list what loads; drop what this machine does not use).
2. Rerun `RAMBASE` after each trim and compare page-ins for the same four programs.

## Run 2 - 2026-09-29, 800x600

Same `RAMBASE A` (the label was reused, so the card's `RBA1/2.TXT` from 09-28 were overwritten;
the timings above are the record). Not first after boot: page-ins started at 3,447, not 1,854.
Dial-Up Adapter was removed in Control Panel, but the boot this ran on still loaded `PPPMAC` and
`SPAP` (`BOOTLOG.TXT` 22:19), so this is not the trim's result.

| | run 1 | run 2 |
|---|---|---|
| locked after boot | 1.93 MB | 1.79 MB |
| page-ins to open the four programs | ~3,370 | ~3,150 |
| copies after boot / programs open | 7.7 7.5 7.1 / 7.1 6.7 6.7 s | 7.7 8.0 7.0 / 7.0 6.9 6.6 s |
| CPU between steps | 100% (`CHOICE`) | 0-2% (`SLEEP.EXE`) |

`PERFLOG` appends: `docs/captures/perflog_5160_2026-09-29_800x600.csv` holds run 1 then run 2.
Timings `rambase_5160_2026-09-29_RBA*.txt`; `MEM /C` and both boot logs alongside. No
`VREDIR` in either log: Client for Microsoft Networks is not installed. Next: reboot, check
`PPPMAC` is gone, `RAMBASE B` first after boot.

## Runs 3 and 4 - 2026-09-29 late, Dial-Up removed, boot menu

Both first after boot, 800x600. B on the FULL menu entry, C on LEAN (`SD120PPD.MPD` and
`NEROCD95.VXD` renamed to `.OFF`). Neither boot loads `PPPMAC` or `SPAP`; the 3C509B stack
(`ELNK3`, `VTDI`, `VIP`, `MSTCP`, `VNETBIOS`) initialises on both. "Locked" below excludes the
disk cache (technique 136), median over the run.

| run | setup | locked excl. cache | page-ins to open the four programs | swap at end |
|---|---|---|---|---|
| 1 (A, 09-28) | 1024x768, Dial-Up | 1.46 MB | 3,373 | 5.61 MB |
| 2 (A, 09-29) | 800x600, Dial-Up, not first after boot | 1.34 MB | 3,145 | 5.73 MB |
| 3 (B) | 800x600, Dial-Up removed, FULL | 1.33 MB | 3,035 | 5.53 MB |
| 4 (C) | same, LEAN | 1.24 MB | 2,700 | 5.29 MB |

- **LEAN saves ~0.09 MB locked and ~11% of page-ins** against FULL, same boot conditions.
- **Removing Dial-Up is not visible against run 2** (0.01 MB, expected ~0.1). Run 2 was not
  first after boot, and the 1024x768 -> 800x600 change landed between runs 1 and 2, so the
  0.12 MB drop there cannot be split between the two.
- Copy times unchanged in every run (6.5-8.0 s): the copies do not page.
- LEAN still logs `Initing sd120ppd.mpd` / `Init Failure` (the registry names the port driver;
  the file is `.OFF`) - 1 tick. On FULL its init takes 897 ticks, about 49 s: the #46 stall
  (`BOOTLOG.TXT` times are BIOS ticks of ~55 ms, not ms).

Raw: `docs/captures/perflog_5160_2026-09-30_runs1-4.csv` (all four runs, appended),
`rambase_5160_2026-09-29_RB[B,C][1,2].txt`, `bootlog_5160_2026-09-29_FULL_RB-B.txt`,
`bootlog_5160_2026-09-30_LEAN_RB-C.txt`.
