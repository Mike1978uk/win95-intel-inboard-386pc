# Real 5160 M8BENCH runs, 2026-10-20/21

`m8bench_card.txt` is the card's `C:\M8BENCH.TXT`, fetched with the V9x Remote Agent. 800x600x8.
Boot: `card_ati*` and `card_prev1` were taken after a CONFIG.SYS `LEAN` boot, everything from
`card_fix4` on after the unattended `FULL` boot. Pixels compare across both; speeds only within one. The first bench after a reboot is unreliable: run it twice.
Rules behind the fixes: `docs/mach8_guide_rules.md`.

| Label | Driver | Result |
|---|---|---|
| card_ati1, card_ati2 | ATIM8 | reference; identical pixels and timings within 2% |
| card_ours2 | `8e78dde` register cache | text_same at 1.1 s/op, then nothing draws (FIFO locked) |
| card_prev1 | pre-cache driver (`8e78dde`~1) | all 35; ATI's pixels in 34 (not lines_dash); 0.96 of ATI |
| card_fix1 | + DP_CONFIG on every mono blit, BKGD_COLOR forgotten after it | same failure at text_same |
| card_varA | cache bypassed, restores dropped | text passes; scroll on fails |
| card_varB | mixes, colours, scissors forced | fails at text_sys |
| card_fix2 | + mixes and PIX_CNTL forgotten after DP_CONFIG | fails at text_same |
| card_varD | + FRGD_MIX, PIX_CNTL forced in mono | text passes (765/s vs ATI 452); fill_large on fails |
| card_fix4 | data path never cached; engine reset on timeouts; no PIX_TRANS read without DATA_READY | all 35, 4 tests wrong |
| card_fix5 | + BKGD_COLOR forgotten after every host transfer | all 35, 5 wrong, erratic slowdowns |
| card_diag | fix5 + reset recorder (`pad` = D, routine, wait, count) | every reset: idle wait after a host transfer |
| card_fix6 | + host transfers start from full idle; idle = FIFO empty and not busy; empty-transfer guards | all 35, no resets, pixels = card_prev1 in all 35; 0.95 of ATI |
| card_dash1 | + dashed pens on the engine, ATIM8's method | first run after boot: noisy (text 8x slow); discard |
| card_dash2 | same boot, second run | pixels = ATI in all 35; lines_dash 496/s (ATI 492); 1.045 of ATI overall |
| card_queue2 | + FIFO slot counting, bursts not subtracted (bug) | 11 tests wrong: the count overstated the room, writes lost |
| card_queu22 | + every burst subtracted (branch `queue-wip`) | pixels = ATI in all 35; 0.97x card_dash2 - routines still end in idle waits |
| card_noidle2 | + no idle wait at the end of lines, plane blits, mono, host writes (`queue-wip` `3082f32`) | pixels = ATI in all 35, no hang; 0.983x card_dash2 (noise): the engine already waits on the CPU, so overlap buys nothing. Shutdown before the next run hung at "Please wait" under M8NOIDL (owner restarted) |
| card_oi2 | master + `-oi` (inline port writes) | pixels = ATI in all 35; 1.016x card_dash2, 1.062x ATI overall; owner shut down under it cleanly |

Variant C (mixes/colours/scissors cached, data path forced) never ran the bench: the desktop drew
solid squares behind the cursor.
