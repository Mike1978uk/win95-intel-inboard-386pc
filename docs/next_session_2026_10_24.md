# Next session - from 2026-10-23

Supersedes `docs/next_session_2026_10_23.md`. Card log: `docs/captures/2026-10-23_card/m8_card_bench.txt`.

## Start here

Gate unchanged (owner, 10-23): equal or better than ATIM8 in every test before DirectDraw, more colours, 3D.
Method: `docs/mach8_driver_method.md` §4b (profile, shorten) and §4c (divide by the bytes first).

0. **Boot stall, second time.** 10-23 15:00, the restart into M8F6 never reached the agent (ping
   answered, port refused). The first was 11:41 into M8S1. Two in ~9 driver restarts. F6 only
   touches StretchBlt/StretchDIBits code, so the stall is likely not the new code - not proven. Ask the
   owner what the screen showed; it comes before more driver work.
1. **Uncommitted, untested on the card:** F6 (`m8_hw_spread` through `m8_hw_copy_fast`, for
   stretchblt 1.01) and F7 (DIB colour table cached by content and `m8_pal_gen`, for dib_8bpp 0.82).
   Built as `%TEMP%\m8f6`, `%TEMP%\m8f7`; F7 contains F6. Bench F7 when the card is back.
2. Below ATI (card_f5r11): text_varied 0.85, draw_icon 0.81, polygon 0.81, mono_bmp 0.82, dib_8bpp
   0.84, mem_to_mem 0.78, floodfill 0.88, bmp_changing 0.95, cursor_move 0.95, stretchblt 0.95 (1.01
   in f4, noise). Profile build first (`build.ps1 -Prof`), one card run gives all of them.
3. mono_bmp and dib_8bpp: ATIM8 is at the card's upload floor (3.00 us a byte, M8UPRATE); what is left
   of ours is CPU per call. Sending fewer bytes than ATI needs content-aware sends (runs as fills).

## Where we are: 1.473x ATI overall (card_f5r11, was 1.345x), pixels = ATI in all 35

Afternoon 10-23: commits `61fe29a` ScanLR, `ad11835` DIBs, `d5e84c3` stretch, `3291c34` mono
(`docs/captures/2026-10-23_card/README.md`). The morning's figures follow.

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
