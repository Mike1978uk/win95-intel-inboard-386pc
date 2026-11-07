# Curves and shapes at 16 bpp, 2026-11-07

Owner's ideas: curves from a line envelope (string art), and Pythagoras. Bed `vm_5160_mcp` counts and pixels only; **no card timing yet**, the card hung first.

## One shape at 800x600x16, bed, per shape

| Shape | Span lists | Polylines | Begin/end | Sends | Pixels sent |
|---|---|---|---|---|---|
| Ellipse 200x200 | 7 | 1 | 1 + 1 | 289 | 6,042 |
| Polygon, 5 points | 10 | 1 | 1 + 1 | 25 | 7,840 |
| Rectangle 200x200 | 0 | 1 | 0 | 0 (5 solid) | 0 |

The ellipse's cost is its outline: GDI gives it as about 290 segments a pixel or two long, and each was one send. Ratio to 8 bpp on the card: ellipse 14 ms on 11-01 (depth not checked in the log), 84-113 ms on `M8D3DX` at 16 bpp.

## Build `M8D3DY` (from HEAD, so pageable buffers)

- **Span lists in a colour the palette holds exactly go as engine rows**, as at 8 bpp. RAM decides: every span is compared, and anything else is sent as before.
- **Short outline segments share a box**, up to 144 pixels (`M8_FB16_GATHER`, not tuned).

Bed: all 36 M8BENCH hashes equal `card_xb1`, FB16CHK 0 differ before and after. Bed ellipse 91 ms (spans only) to 63 ms; bed timings do not price the card's bus.

## M8CURVE

Circle radius 30, 60, 120: rows, SCAN_TO_X rows, 32 chords, 32 tangents (string art), a 33-line parabola, polygon fill. Bed: runs clean, 0 waits lost; bed's times are not card costs. Card: **hung during its first run** (below); no figures.

## Locked buffers cost speed (card, agent on, high, boot 255)

| fps | `M8D3DX` pageable (11-06) | Locked `zl` | Locked `zl2` |
|---|---|---|---|
| hal_many | 41.2 | 31.2 | 30.8 |
| hal_large | 41-51 | 38.0 | 38.3 |
| hal_grad | 42.2 | 35.9 | 36.2 |
| hal_turn100 | 33.3 | 30.7 | 30.6 |

Different boots, so not yet a clean A/B. FB16CHK 0 differ in both locked runs. `M8D3DY` run through `M8D3DF` gives the pageable side.

## Card hang, 11:3x

Boot 255, about 18 minutes up, after two `M8D3DF` high runs and `M8IDLE`, during `M8CURVE`'s first run. Agent stopped answering; ping went on, the port then refused, later ping stopped too. `M8CURVE.TXT` to be read from the card: a header line with no arm lines says it died at the mode set or the first arm. Fits the older pattern after high-priority benches; the probe not cleared.
