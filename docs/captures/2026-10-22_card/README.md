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
