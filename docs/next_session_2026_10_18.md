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

## Next

1. Rewrite the hot pixel loops as 32-bit assembly (Watcom `#pragma aux`, 386 registers in 16-bit code): the cache
   hash+copy, the proxy Fletcher check, the run encoder and uploads, read-back copies, the mono repack.
2. Re-time the same single-card drag (`m8_timing.py --drag --points`), proxies off then on. Compare to ATIM8 by feel.
3. Then the rest of the base list: colour-compare transparency, the timed 5160 run, D8c.

Bed: on m8disp.drv (owner imported `M8DISP.REG` in Explorer after the ATIM8 comparison - check it boots ours).
`C:\ATIM8.REG` / `C:\M8DISP.REG` switch drivers (F8, command prompt, `REGEDIT`).
