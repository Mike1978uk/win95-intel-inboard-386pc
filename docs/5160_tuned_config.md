# The 5160's tuned configuration

What the real machine's CF carries, read from the card in the host reader on 2026-10-11, and why. Update
this page in the commit that changes the card. The hardware itself is in
`docs/5160_checklist_2026_09_29.md`; downloads in `FIXES.md`.

| Area | Setting on the card | Evidence | Status | Undo |
|---|---|---|---|---|
| CPU | Blue Lightning 2x, L1 on, 1-16 MB cacheable, XTOUT clear: `C:\CTCHIP\CPUSET.BAT` (CTCHIP34) from `AUTOEXEC.BAT` | `what_worked_and_what_didnt.md`, #40 | confirmed | `C:\CTCHIP\CPUOFF.BAT`. Never set SNP (`1000h:0` bit 3): hangs the 5160 |
| DRAM refresh | divisor 64: `C:\REFR64.COM` in `AUTOEXEC.BAT` | 3-6% faster bus; VGA dword writes 1.98 -> 1.86 us/B; RSOAK2 clean boot 0 errors; floppy format and compare OK (10-11) | confirmed | `C:\REFR18.COM` |
| Boot disk | `XTIDEMP.MPD` `9C64B4D1` (was `A3F3EEAD`): waits scaled to the measured 100 ns loop, 26 us polls, 100 us first read wait (`2e17fc5`), CDB length / `PORT=` / stride-2 latch checks, FLUSH CACHE at shutdown when the drive reports it | boot 389: XTSECT unchanged, read-back identical | kept 11-14 | `XTIDEMP.B84` in `C:\V9XREMOTE\STAGE` via `WININIT.INI`, 8.3 paths only (copy to `C:` root first) |
| Timer tick | `MinTimeSlice=54` in `SYSTEM.INI [386Enh]` (55 ms tick) | idle CPU +6%, during a big copy +60% (`captures/2026-10-10_timer/`) | confirmed: a 36 MB copy 0.7% faster (CPYTIME 10-11); DOS boxes and serial unchecked | `SYSTEM.BSR` over `SYSTEM.INI` |
| SCSI DMA | `SCSIPORT.PDR` `3f85fa2b`, buffer below 1 MB | boots; no SCSI DMA test yet | loaded, unproven | `C:\SCSIPORT.ORG` from DOS |
| Floppy DMA | `HSFLOP.PDR` `8e695d00` | loads (`FIXES.md`) | loaded, unproven | `HSFLOP_stock.PDR` |
| Paging | `SWAPCOMP.VXD` = `SWAPZFN` `99B7BD5B` (was `SWAPZF` `07154EB7`), 32 MB fixed swap on C:; 4-bit SlotTab, locked object three pages (84 bytes spare, `tools/le_census.py`) | card PAGETIME clean on the checksum build | kept 11-14; 45-minute soak before `dist/` | `SWAPCOMP.B84` in `C:\V9XREMOTE\STAGE` via `WININIT.INI`, 8.3 paths only (copy to `C:` root first) |
| Display | `m8f3.drv` + `M8DD32.DLL` (Mach8, F3 findings 1-3, `6ccc10a`), 800x600x16; `C:\M8D3D.INI` `[feed] on=1`, `[diag] trace=1` | boot 390: pixels = GX in 36, 1.045x; tunnel + Alt+F4 3 of 3 clean, bench after them unchanged | kept 11-14 | registry `drv` back to the previous `.drv` |
| Memory | `SHADRAM.VXD`, 112 KB of spare shadow RAM | `FIXES.md` | confirmed | remove the `SYSTEM.INI` line |
| Boot | `WAIT86.COM`, `IVT68FIX.COM` (last before Windows), boot menu FULL/LEAN/OFFLINE/SAFE | required, not tuning | confirmed | - |

Tried on this card and dropped: the 28800's zero-wait-state bits (no change, 10-10); clock above 2x (3x fails).

Next measurements that could change this page: DOS boxes and serial at the 55 ms tick (worklist T2).
