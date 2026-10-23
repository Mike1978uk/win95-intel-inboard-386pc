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

Evening (pixels = ATI in all 35 every run):

| Run | Build | Overall | What moved |
|---|---|---|---|
| card_f7r11 | M8F7 `22c50d5` spread via copy_fast (+ DIB table cache, dropped) | 1.485x | stretch_dib 1.62, stretchblt 1.50; table cache no gain |
| card_f7p1 | profile of F7 | - | mono_bmp 6832 us a call, ellipse outline 3551 us |
| card_f8r11 | mono cache, first cut | 1.495x | mono_bmp 1.05 (cache only tried on one branch) |
| card_f9r11 | + polyline merge | 1.480x | none; profile: outline 3551 -> 4080 us, merge dropped |
| card_f10p1 / f11p1 | profiles | - | mono_bmp 4245 -> 1205 us a call once the cache is tried first |
| card_f11r11 | M8F11 `9999167` + `5f6d007` | 1.518x | mono_bmp 2.96 |

Probes: `M8APERT.C` - no Mach8 memory in A0000-BFFFFh (all of it shows the VGA's text page, `20 07`).
`M8DWORD.C` - a 32-bit OUT to PIX_TRANS puts its upper word into the DAC mask (02EAh read 1Eh after,
FFh before): E2EAh decodes as 02EAh, PIX_TRANS takes words only.

Late evening:

| Run | Build | Overall | What moved |
|---|---|---|---|
| card_f12p1 | pattern spans grouped by byte | - | none (polygon 1934 -> 1946 us a call); dropped |
| card_f13p1 / f14q1 | profile v4 counters `8d7b203` | - | polygon 197 spans/op at 63 us each; no cursor lifts in any test |
| card_f15p1 | span walk timed alone (s62) | - | scanline call = walk + ~250-480 us of general-path set-up |
| card_f18p1 | scanline short path, solid only | - | floodfill 258 -> 224 ms/op in the driver, scanline call 590 -> 358 us |
| card_f19r11 | M8F19 `aba3fa9` + two-colour patterns | 1.539x | pixels = ATI in all 35 |
| (none) | M8F14P first run | - | M8IDLE never quiet: steady 12 % CPU for 600 s, screenshot helper timed out; one-off, next boot clean |
| card_f20p1 | `d1a8619` ScanLR by rep insw (+ string-cache hash buckets, dropped) | - | ScanLR 634 -> 587 us, floodfill 224 -> 217 ms/op in the driver; buckets: draw_icon 1873 -> 1875 us, no gain |
