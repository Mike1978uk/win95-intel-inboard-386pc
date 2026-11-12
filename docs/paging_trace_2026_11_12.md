# Paging trace on a 5 MB bed, 2026-11-12

Question: what does Windows 95 page while DX5 SDK twist runs: WIN386.SWP or re-read DLL/EXE images?

**Answer: not obtained.** Filemon hung the guest every time. Counters are measured; file names are not.

## Bed

- `vm_5160_page`, a copy of `vm_5160_mcp` (screenshots dropped). `mem_size = 5120` already; unchanged.
- cfg: `uuid` removed (bed_launch refuses shared uuids); SLiRP forward 9869 -> **9870**, because `vm_5160_mtm` held 9869. v9xctl needs `-Port 9870`; install_h.ps1 and twist_card.ps1 have no `-Port`, so patched copies were used.
- Exe `86box_auto\build\src\86Box.exe`, dynarec off.
- Driver: current master build (`build.ps1 -Out %TEMP%\m8bed`), installed as `M8D3PG` (DLL md5 E52B6BB4, DRV md5 F7FCADC8). Desktop 800x600 (agent `ScreenWidth/Height`; its `BitsPerPixel` reads 0, so 16 bpp not confirmed here).
- Twist: `M8TWIST.EXE` + textures from `%TEMP%\dx5sdk_x\bin`, run by batch, closed by `WINCLOSE twist`.

## Filemon

- 4.34 from https://web.archive.org/web/2001if_/http://www.sysinternals.com/Files/Filemon.zip; 7.04 from https://web.archive.org/web/2006if_/http://www.sysinternals.com/Files/Filemon.zip. Logged in `docs/host_tools.md`.
- It loads only when started with `FILEVXD.VXD` in the current directory ("FILEVXD is not loaded properly" otherwise).
- Log shows WIN386.SWP reads and writes by Filemon itself during start-up: the capture buffer is paged on 5 MB.
- Hangs, no recovery, agent dead:
  1. 7.04, after 1 row, on the first agent transfer.
  2. 4.34, at row 1859, on a `put` of twist's textures during capture.
  3. 4.34 started by batch (no agent traffic), hung at row ~1400 while twist loaded DirectDraw.
- Control: the same batch without Filemon ran, agent stayed up. So Filemon, not twist or the bed.
- No Filemon log was saved.

## Counters (PERFLOG, no Filemon)

`docs/captures/perflog_bed_twist_2026-11-12.csv`, 101 samples at 1 s: twist started at ~20 s. Bed CPU is slower than the card; read counts, not rates.

| | before twist | during start-up (25-60 s) | after |
|---|---|---|---|
| page-ins | 4114 | 4261 -> 6546 (+2285 in 35 s, ~65/s; peak 151/s over 2 s) | 6846 at 100 s |
| page-outs | 2135 | 2247 -> 3243 (+~1000) | 3420 |
| discards | 1615 | 1660 -> 2909 (+~1250) | 3033 |
| swap file in use | 4.5 MB | 4.9 -> 5.4 MB | 5.7 MB |
| free | 0 | 0 | 0 |
| disk cache | 524 KB | 524 KB | 524 KB |
| locked | 2.02 MB | 2.1 - 2.14 MB | 2.14 MB |

- Free 0, cache 524 KB (card: 528 KB), locked 2.0-2.1 MB (card: 2.2 MB): the bed's memory state matches the card's.
- Discards rise with page-ins, about one discard per two page-ins: some pages are code or read-only data dropped and read back, not only swap traffic. Counters cannot say which files.
- From ~65 s the counters stay flat for 25 s and the display is black: twist was not animating in that window on the bed, or had stalled. Steady-state paging was therefore **not** sampled.

## Locked memory

- Measured: 2.02 MB locked before twist starts, 2.14 MB after. Twist's own share is ~0.12 MB; the rest is locked at boot on this bed.
- Not measured, candidates: driver 16 bpp frame buffer (800x600x2 = 960 KB if allocated locked; 640x480 = 614 KB), DirectDraw surfaces, VxDs (INBRDPC, SHADRAM, SWAPCOMP, VDD), DMA buffers, V86 memory. No per-component counter exists; a `GMEM_FIXED` vs pageable A/B (`m8_fb16_pageable`) would measure the frame buffer's share.
- Inferred only: the 960 KB figure; no measurement ties it to the locked count.

## Sector trace attempt (stopped)

- Why: Filemon failed, so I patched `hdd_image_read/write` in a scratchpad copy of 86box_auto HEAD to log LBAs, to map them to files through the FAT16 table (`card.img`, partition at 63, FAT16).
- That build crashed at start-up without `-mwindows`; it was never run. Stopped on the coordinator's instruction to use the project exe. Patch and mapper script were not kept in the repo. The blocker was: no file-level data without either Filemon or a sector log in the emulator.
- Next options: a sector-trace option in the project's 86Box build; or a bed with a larger RAM size to see whether Filemon survives (does not match the card).

## Left behind

- `vm_5160_page` (untracked, ~6 GB), Filemon in `C:\FM2` on its disk, `C:\TWIST\*.BAT`.
- `C:\PERFLOG.CSV` on the bed holds this run as label `ctl-S28-000252`.
- Bed 86Box stopped. Nothing was sent to 192.168.68.62.
