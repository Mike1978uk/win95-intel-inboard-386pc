# Card runs 2026-10-23

`m8_card_bench.txt`: every run of the day (labels in `docs/next_session_2026_10_24.md`).
`card_*r1-3`: 3 runs per build; `card_p*`: profile builds (their ops/s carry the timer, ~83 us a call);
`card_ati31`: ATI the same day (reproduces card_ati1/2 within 1.1%).

Afternoon, one run each against `card_ati1/2` (pixels = ATI in all 35 every time):

| Run | Build | Overall | What moved |
|---|---|---|---|
| card_f1r11 | M8F1 `61fe29a` ScanLR abort | 1.341x | floodfill 0.68 -> 0.93 |
| card_f2r11 | M8F2 `ad11835` DIB rows through DIBENG's table | 1.367x | dib_8bpp 0.61 -> 0.82 |
| card_f3r11 | M8F3 StretchDIBits by the engine | 1.401x | stretch_dib 0.45 -> 1.17 |
| card_f4r11 | M8F4 `d5e84c3` + StretchBlt | 1.440x | stretchblt 0.40 -> 1.01 |
| card_f5r11 | M8F5 `3291c34` mono unshifted | 1.473x | mono_bmp 0.62 -> 0.82, mem_to_mem 0.57 -> 0.78 |
| (none) | M8F6 spread through copy_fast | - | restart into it stalled: ping answers, agent never started (second stall of the day) |

Probes: `tools/m8probe/M8ABORT.C` (the 900Fh/500Fh abort keeps WRT_MASK, scissors, FRGD_COLOR,
GE_PITCH, DP_CONFIG and ALU_FG_FN), `M8UPRATE.C` (100x100 host upload: 30.04 ms per-row `rep outsw`,
31.45 ms one `rep outsw`, 21.72 ms the same words to 0278h).
