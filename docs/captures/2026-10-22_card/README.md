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
