# Next session - from 2026-10-17

Start from `docs/worklist.md` (its first item is this one). Owner's ethos (10-17, worklist "Driver ethos"): the
whole card; the best part of every driver fitted to this card; fewer fallbacks is a decision in itself.

## Done 10-17 (all in the bed, `vm_5160_now`; none on the 5160 yet)

- Bitmap cache: LRU eviction by shelf, area to the string cache, stored on second sight (`ce290a0`).
- Lines by LINEDRAW (`9768594`); Pixel sets as 1x1 fills, DIB_Pixel returns 0 for a set (`763dc81`).
- Mono bitmaps expanded by the engine: Scrolling Marquee "much faster" (`8d68563`).
- Fixed: cache stores while DOS had the card drew stale slots (deck glitch, since `c19476f`) - `1c015cd`.
- Held bitmaps (proxies): save, copies between held bitmaps, fills, growth by overwrite - built, measured, **off**
  (`c71432d`). Sol start fault: a deleted bitmap's header read through a freed selector; pointer now tested
  (`af31497`), no fault in 4 starts.
- Timing: PIT channel 2 around each GDI entry plus checkpoints (trace build). Tools: `m8_timing.py`,
  `m8_time_by_path.py`, `m8_mem_trace.py`, `m8_offscreen_audit.py`. `bed_launch.ps1` now resolves `-Log`.
- DirectDraw plan revised: Mach8 HAL first, VGA half optional (worklist D5).

## The finding

The drag is slow because of our 486-side per-byte C loops, not the bus. A cache hit's hash-and-copy loop is ~70 ms
per 71x96 card (~40 instructions and a `__U4M` call per byte); ATIM8 on the same drag is "night and day" while
moving plenty of pixel-port data. Design doc "Timing the drag".

## Next (updated late 10-17)

Benchmark and ATI's own code now say what to do - worklist first item, `docs/atim8_disassembly.md`, M8BENCH results
`docs/captures/2026-10-16_atim8_tap/m8bench_1017_first.txt`:
1. Uploads as ATI does them: one extended blit, `rep outsw` per row, no polling. Read-back: one poll, `rep insw`.
2. Hatched/mono/colour brushes and dashed lines on PATT_DATA (both ours fall back today: 0.01-0.02 of ATI).
3. Cheap 32-bit hashing so our caches (string, bitmap) beat ATI's no-cache paths; then proxies back on.
4. Re-run M8BENCH under ours (ATI's run is stored; re-run it only after changing the bench). Bench in image:
   `C:\M8BENCH.EXE <label>`; results `--get M8BENCH.TXT`; `tools/m8_bench_compare.py`.
5. Open: polygon/ellipse fault in DIBENG (not through our strips - design doc). Cheap first step: find selector
   15E7's owner.

Tools from 10-17: `tools/ne_disasm.py` (Win16 NE with names; listings stay in `references/*_disasm/`, ignored),
`tools/m8bench/`, `tools/m8_timing.py`. Bed image holds our PLAIN build (for the bench), not the trace build.

Bed: on m8disp.drv (owner imported `M8DISP.REG` in Explorer after the ATIM8 comparison - check it boots ours).
`C:\ATIM8.REG` / `C:\M8DISP.REG` switch drivers (F8, command prompt, `REGEDIT`).
