# Curves and shapes at 16 bpp, 2026-11-07

Owner's ideas: curves from a line envelope (string art), and Pythagoras. Bed `vm_5160_mcp` counts and pixels only. Card: M8CURVE now has card figures (run c3, boot 257); the bed counts and pixels for M8D3DY stand, and M8D3DY itself still has no card timing.

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

Circle radius 30, 60, 120: rows, SCAN_TO_X rows, 32 chords, 32 tangents (string art), a 33-line parabola, polygon fill. Bed: runs clean, 0 waits lost; bed's times are not card costs. Card (run c3, boot 257, `M8D3DX`): us per shape, fastest of 3 reps x 20 passes, 640x480x16.

| | r 30 | r 60 | r 120 |
|---|---|---|---|
| rows (rectangle command a row) | 2,457 | 4,865 | 9,659 |
| scan (SCAN_TO_X a row) | 2,229 | 4,422 | 9,018 |
| poly (32 edges, polygon fill) | 1,923 | 2,810 | 6,237 |
| chords (32 lines) | 1,416 | 1,416 | 1,416 |
| tangents (32 lines, string art) | 1,416 | 1,416 | 1,417 |
| parabola (33 lines) | 1,459 | 1,459 | 1,459 |

- A line costs ~44 us whatever its length, measured up to 240 px (longest line in the test). The cost is in the writes, so string-art tangents cost the same as chords. This corrects the earlier expectation that they cost more.
- Polygon fill beat both kinds of rows at every size tested. It is a 32-sided approximation of the circle.

Raw card log: `M8CURVE.TXT` in this folder.

## Locked buffers cost speed (card, agent on, high, boot 255)

| fps | `M8D3DX` pageable (11-06) | Locked `zl` | Locked `zl2` |
|---|---|---|---|
| hal_many | 41.2 | 31.2 | 30.8 |
| hal_large | 41-51 | 38.0 | 38.3 |
| hal_grad | 42.2 | 35.9 | 36.2 |
| hal_turn100 | 33.3 | 30.7 | 30.6 |

Different boots, so not yet a clean A/B. FB16CHK 0 differ in both locked runs. `M8D3DY` run through `M8D3DF` gives the pageable side.

## Card hang, 11:3x

Boot 255, about 18 minutes up, after two `M8D3DF` high runs and `M8IDLE`, during `M8CURVE`'s first run. Agent stopped answering; ping went on, the port then refused, later ping stopped too. `M8CURVE.TXT` to be read from the card: a header line with no arm lines says it died at the mode set or the first arm.

Second hang, boot 256: fresh boot, no bench before it, run c2. Log held the header and setmode lines and no result, so it stopped inside the first arm, rows r 30; the owner saw a black screen with one white filled disc. Ping answered, agent did not. Third run c3, boot 257: finished, 0 waits lost. M8CURVE now commits its log to disk per rep and stops on a wait that runs out, logging EXT_FIFO, GP_STAT and SUBSYS_STAT; those limits did not fire. Two hangs in three runs; cause not known, may be intermittent.
