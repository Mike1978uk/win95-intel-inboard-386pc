# The 5160's tuned configuration

What the real machine's CF carries, read from the card in the host reader on 2026-10-11, and why. Update
this page in the commit that changes the card. The hardware itself is in
`docs/5160_checklist_2026_09_29.md`; downloads in `FIXES.md`.

| Area | Setting on the card | Evidence | Status | Undo |
|---|---|---|---|---|
| CPU | Blue Lightning 2x, L1 on, 1-16 MB cacheable, XTOUT clear: `C:\CTCHIP\CPUSET.BAT` (CTCHIP34) from `AUTOEXEC.BAT` | `what_worked_and_what_didnt.md`, #40 | confirmed | `C:\CTCHIP\CPUOFF.BAT`. Never set SNP (`1000h:0` bit 3): hangs the 5160 |
| DRAM refresh | divisor 64: `C:\REFR64.COM` in `AUTOEXEC.BAT` | 3-6% faster bus; VGA dword writes 1.98 -> 1.86 us/B; RSOAK2 clean boot 0 errors; floppy format and compare OK (10-11) | confirmed | `C:\REFR18.COM` |
| Boot disk | `XTIDEMP.MPD` `A3F3EEAD` (was `XTBOTH` `DAEF6830`); source: XT-IDE review branch `worktree-agent-a5f2e4562c5d10217` (`23f38e3`), `build.ps1 -PollBackoff -FastXfer -WaitFirst -YieldCommit -TightTaskfile`, no `-SkipDrvHd` (its gain was nil, its risk a wrong-sector access) | boot 385: XTSECT within run-to-run noise of `abA`/`abB`; KERNEL32, VMM32, EXPLORER read back identical | kept 11-14 | `XTIDEMP.B84` (`DAEF6830`) in `C:\V9XREMOTE\STAGE` via `WININIT.INI`, 8.3 paths only (copy to `C:` root first) |
| Timer tick | `MinTimeSlice=54` in `SYSTEM.INI [386Enh]` (55 ms tick) | idle CPU +6%, during a big copy +60% (`captures/2026-10-10_timer/`) | confirmed: a 36 MB copy 0.7% faster (CPYTIME 10-11); DOS boxes and serial unchecked | `SYSTEM.BSR` over `SYSTEM.INI` |
| SCSI DMA | `SCSIPORT.PDR` `3f85fa2b`, buffer below 1 MB | boots; no SCSI DMA test yet | loaded, unproven | `C:\SCSIPORT.ORG` from DOS |
| Floppy DMA | `HSFLOP.PDR` `8e695d00` | loads (`FIXES.md`) | loaded, unproven | `HSFLOP_stock.PDR` |
| Paging | `SWAPCOMP.VXD` = `SWAPZF` `07154EB7` (was `C2585E38`), 32 MB fixed swap on C:; source: swapcomp review branch `worktree-agent-a5284dbe897a9358e` (`709e116`), target `SWAPZF`; locked object fits four pages, not five | boot 385 checksum build `SWAPZFC` `349776FD`: PAGETIME loop, 7,941 page reads, ChecksumErrors 0, Errors 0, Fallback 0 | kept 11-14 | `SWAPCOMP.B84` (`C2585E38`) in `C:\V9XREMOTE\STAGE` via `WININIT.INI`, 8.3 paths only (copy to `C:` root first) |
| Memory | `SHADRAM.VXD`, 112 KB of spare shadow RAM | `FIXES.md` | confirmed | remove the `SYSTEM.INI` line |
| Boot | `WAIT86.COM`, `IVT68FIX.COM` (last before Windows), boot menu FULL/LEAN/OFFLINE/SAFE | required, not tuning | confirmed | - |

Tried on this card and dropped: the 28800's zero-wait-state bits (no change, 10-10); clock above 2x (3x fails).

Next measurements that could change this page: DOS boxes and serial at the 55 ms tick (worklist T2).
