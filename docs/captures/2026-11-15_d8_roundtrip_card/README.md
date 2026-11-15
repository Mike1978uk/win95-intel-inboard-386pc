# DirectDraw round trips on the card, 2026-11-15

Card 192.168.68.62. `run.log` holds every run. Summary in `docs/directdraw_raise_bsod_2026_11_15.md`.

| boot | driver | after wormhole | then |
|---|---|---|---|
| 401 | `m8shf.drv` (`33b1104`) | agent answered | next program: BSOD `0137:BFF9A07C` |
| 402 | `m8shf.drv` | agent answered 5 min | bench: BSOD, same address |
| 404 | `m8nof.drv` (`32bfaac`) | agent answered 5 min | bench: BSOD, same address |

Benches before the round trip: `card_shf1/2` pixels = GX baseline in 36, 1.05; `card_nof1` pixels =
in 36, 0.997.
