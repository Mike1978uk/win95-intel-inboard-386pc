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
