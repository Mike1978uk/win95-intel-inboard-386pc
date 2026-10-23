# Next session - from 2026-10-23

Supersedes `docs/next_session_2026_10_23.md`. Card log: `docs/captures/2026-10-23_card/m8_card_bench.txt`.

## Start here

Gate unchanged (owner, 10-23): equal or better than ATIM8 in every test before DirectDraw, more colours, 3D.
Method: `docs/mach8_driver_method.md` §4b - profile build first, then shorten what the profile shows.

1. **Count spans in the profile** (polygon, ellipse, floodfill): their scanline calls are now ~1.3-1.9 ms each
   after the assembly walker; whether that is port writes (fewer writes per span) or still CPU decides the lever.
2. **floodfill 0.68**: 960 ScanLR calls at ~790 us each per fill - a ScanLR short path as getpixel's (read
   set-up cached, data-ready empties the FIFO), then the 467 OS_SCANLINES calls.
3. **text_varied 0.85, draw_icon 0.80, mem_to_mem 0.68, bmp_changing 0.92**: profile first (2+ calls per op).
4. **fill_small 0.95, dstinvert 0.96**: short path is ~140 us; ATI's whole driver side is ~150. Next is the
   colour/mix write order and the drain `in` every few calls (read 9AEE free slots instead of waiting for empty).
5. **cursor_move 0.91** (varies 0.87-1.01 run to run): profile before acting.
6. Then uploads (dib_8bpp 0.61, mono_bmp 0.66: ATIM8's generated `rep outsw`) and stretch (0.40-0.46: ATIM8 `seg14`).

## State

- Card: `M8A1.DRV` selected = master `f272261`. `menudefault=LEAN` (driver work). Card `C:\M8BENCH.EXE` is the
  current build (profile v3 + caps line); `tools/m8bench/M8BENCH.EXE` (ignored) is the same file.
- One hang: the restart into M8S1 (same code as master then) stuck loading the desktop, 11:41; owner restarted;
  6 restarts since, all clean. Watch for a repeat.

## Where we are: 1.345x ATI overall (was 1.274x), pixels = ATI in all 35

Below ATI: stretchblt 0.40, stretch_dib 0.46, dib_8bpp 0.61, mono_bmp 0.66, floodfill 0.68, mem_to_mem 0.68,
draw_icon 0.80, polygon 0.82, text_varied 0.85, cursor_move 0.91, bmp_changing 0.92, fill_small 0.95,
dstinvert 0.96, ellipse 0.97. Brought level or ahead today: setpixel 1.00 (0.80), getpixel 1.12 (0.82), lines
1.19 (0.88), lines_hv 1.31 (0.84), rubber_band 1.19 (0.88), rectangle 1.14 (0.76), blit_small 1.19 (0.93),
fill_xor 1.06 (0.91).

## What 10-23 established

- **Each GDI call starts with the L1 empty** (the last call's port writes flushed it; every I/O access flushes,
  `docs/captures/io_cache_flush_2026_09_21.md`). A call's CPU cost is the cache lines it touches, ~1.3 us each.
  Compiler flags do nothing (`-ox`, `-os` neutral); short paths, one hot-state block (`m8_hot`) and assembly for
  the span loop did (Andrew, #50).
- **Profile build** (`build.ps1 -Prof`, `3973538`): our us per operation per test, per entry point, per Output
  style, short-path hits, and a re-read check for pixel reads (190 checked, 0 wrong). The timer costs ~83 us a
  call (VTD emulates the PIT) and is subtracted; never quote a profile build's ops/s.
- **ATIM8 and ours report the same caps** (poly 0008, curve 0000, line 0023): GDI scan-converts shapes for both.
- **ATIM8 reads a pixel without an idle wait** (seg07:00aa); ours now sends 3 writes against its 6.
- Commits: `55d5624` queued fill, `3973538` profiler, `56ee0f4` short paths, `d3c3fee` hot block, `2c6845c`
  pixel reads, `d6646b4` Output styles + caps, `4951a99` span loop, `f272261` assembly span walker.

## People

- Andrew (#50, 10-23): reply posted (issuecomment-6360908020); cross-references on #51, #52, #53. His leads
  (compiled binary, inline asm, size, I/O flushing the cache) drove the day's work - worth telling him the
  numbers once the shapes are level. Owner approves wording; an agent may post on approval.
