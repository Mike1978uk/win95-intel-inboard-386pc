# Real 5160 M8BENCH runs, 2026-10-22

`m8bench_card.txt` is the card's `C:\M8BENCH.TXT` after the CF was restored from the 10-12 image and
the 10-21 files put back (it carries every earlier run too). 800x600x8, unattended `FULL` boot. ATI's
reference runs (`card_ati*`, 10-20) were after a `LEAN` boot, with more free RAM: speeds against them
are not like for like.

| Label | Driver | Result |
|---|---|---|
| card_hatch1 | `-oi` + hatch: ATIM8's per-row pattern, doubling engine copies | first run after boot; 1.035x card_oi2, fill_hatch 327/s |
| card_hatch2 | same boot, second run | slow in every test (text_sys 112/s): something else was running; discard |
| card_hatchb1, b2 | same boot, `-BenchOnly` | b2: pixels = ATI in all 35; fill_hatch 316/s (ATI 298, card_oi2 119); 1.072x ATI |
| card_scanp | + dithered scanline spans through the pattern registers | FULL boot hung at `Initing sd120ppd.mpd` (LS-120, #46), twice; not the driver |
| card_scanl1 | same driver, `LEAN` boot (as ATI's), after M8IDLE | pixels = ATI in all 35; polygon 21.9/s (was 12.5, ATI 73.2); 1.107x ATI |
| card_batch1 | + one set-up per scanline call, then position and pattern per span | LEAN default, after M8IDLE; pixels = ATI in all 35; polygon 37.0/s, ellipse 51.4/s; 1.147x ATI |
| card_bcache1, card_bcacheb1 | + last pattern brush remembered (no CPU re-render per scanline call) | pixels = ATI in all 35; fill_hatch 378-380/s (1.27x ATI), ellipse 55/s, polygon 38-39/s; mem_to_mem 187-199 is the low end of its 206-390 spread across all runs (bitmap-cache state), watch it |
| card_scanx1 | + one-row spans through SCAN_TO_X (CAEE), as ATIM8 seg08:0dcd | pixels = ATI in all 35; polygon 42.8/s (0.59x ATI); the bed differs on polygon only: 86Box SCAN_TO_X ignores PATT_INDEX |
| card_loop1 | + one loop over GDI's span list in m8hw.c (no per-span helpers) | pixels = ATI in all 35; polygon 54.6/s (0.75x ATI), ellipse 62.7/s (0.88x); 1.143x ATI |
| card_trim1 | + span bound only when the cursor overlaps; SaveScreenBitmap check in trace builds only | pixels = ATI in all 35; no measurable change (polygon 54.1/s) |
| card_ox1 | trim build compiled -ox instead of -oi (not adopted) | pixels = ATI in all 35; 0.998x card_trim1: no gain; drag_frame 0.71x, dstinvert 0.90x. Bed 1.108x: the bed rewards CPU the card does not |
| card_slots1 | + wait for 6 free FIFO slots per span, not an empty FIFO (reverted) | polygon 0.89x: a status read every span costs more than the overlap gains |
| card_str1 | + stretches read the screen only if the ROP needs it or the box is not covered | no change (the write, not the read, is the cost) |
| card_dupr1 | + write-back sends a row equal to the one above as an engine copy | pixels = ATI in all 35; stretch_dib 4.2/s (was 2.9), stretchblt 5.6/s (was 3.6); 1.175x ATI |
| card_scan21 | + ScanLR on the card: a SCAN_TO_X read from x, compared as it arrives (ATIM8 seg07:0147) | pixels = ATI in all 35; floodfill 2.7/s (was 1.3); a first build counted left scans from x + 1 and differed |
| card_getpx1 | + GetPixel on the card: a one-pixel SCAN_TO_X read (ATIM8 seg07:00aa) | pixels = ATI in all 35; getpixel 1864/s (was 1357); a self-test build matched the old path on all 1722 calls |
| card_curp1 | + cursor masks kept as bit-planes off-screen, drawn by plane blits | pixels = ATI in all 35; cursor_move 690/s (was 488) |
| card_setpx1 | + pixel sets without the DIB Engine call on a scratch strip | pixels = ATI in all 35; setpixel 1696/s (was 1443); 1.206x ATI overall |
| card_nocur1 | throwaway: MoveCursor records the position and draws nothing | cursor_move 2350/s: everything outside the driver costs 0.43 ms a move, so ATI's driver takes ~0.53 ms |
| card_keyc1 | + cursors that invert nothing drawn by one keyed copy (source compare 92EEh 28h) | pixels = ATI in all 35; cursor_move 879/s (was 612) |
| card_key21 | + no idle waits in the cursor's copies | pixels = ATI in all 35; cursor_move 1014/s (0.97x ATI); M8CURRD reads a clean arrow from video memory |
| card_pair1 | + restore and save in one paired copy (reverted) | cursor_move 1013/s: no change |
| card_keyf1 | keyed build again, after the Start-menu restart | pixels = ATI in all 35; cursor_move 889/s (boot-to-boot spread ~12%); 1.227x ATI |
| card_seen1 | + a string goes into the string cache the second time it is seen | pixels = ATI in all 35; text_varied 412/s (was 328, ATI 505); repeated text unchanged; 1.248x ATI; Start-menu restart ok |
