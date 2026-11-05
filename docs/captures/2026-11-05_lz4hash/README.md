# LZ4 hash and table variants on the 5160, 2026-11-05

Card 192.168.68.62, boot counter 208, uptime 608 s at start, 1,179 s at end.
SWAPPAGE.BIN (262,144 bytes) is in C:\; EXEs and logs ran from C:\. EXEs verified against SHA256SUMS before upload.

Each list was preceded by `M8IDLE 10 10 600` (exit 0). Verbatim:

idle: run 1
```
  1 s  cpu   0 %  quiet 1
  2 s  cpu   2 %  quiet 2
  3 s  cpu   0 %  quiet 3
  4 s  cpu   0 %  quiet 4
  5 s  cpu   1 %  quiet 5
  6 s  cpu   0 %  quiet 6
  7 s  cpu   6 %  quiet 7
  8 s  cpu   7 %  quiet 8
  9 s  cpu   6 %  quiet 9
 10 s  cpu   6 %  quiet 10
```
idle: run 2
```
  1 s  cpu   4 %  quiet 1
  2 s  cpu  49 %  quiet 0
  3 s  cpu   1 %  quiet 1
  4 s  cpu   6 %  quiet 2
  5 s  cpu   6 %  quiet 3
  6 s  cpu   7 %  quiet 4
  7 s  cpu   1 %  quiet 5
  8 s  cpu  14 %  quiet 0
  9 s  cpu   6 %  quiet 1
 10 s  cpu   0 %  quiet 2
 11 s  cpu   7 %  quiet 3
 12 s  cpu   6 %  quiet 4
 13 s  cpu   1 %  quiet 5
 14 s  cpu  13 %  quiet 0
 15 s  cpu   6 %  quiet 1
 16 s  cpu   1 %  quiet 2
 17 s  cpu   7 %  quiet 3
 18 s  cpu   7 %  quiet 4
 19 s  cpu   7 %  quiet 5
 20 s  cpu   0 %  quiet 6
 21 s  cpu   8 %  quiet 7
 22 s  cpu   7 %  quiet 8
 23 s  cpu   1 %  quiet 9
 24 s  cpu   7 %  quiet 10
```

Run 1 / run 2, us per page. Self-test passed in every log.

| variant | ratio | compress us | decompress us | self-test | page-in cost vs LZ4B12I |
|---|---|---|---|---|---|
| LZ4B12 | 2.26 / 2.26 | 5067 / 5048 | 1130 / 1113 | passed / passed | +0.04% / -0.29% |
| LZ4B12I | 2.26 / 2.26 | 5102 / 5129 | 1114 / 1113 | passed / passed | +0.00% / +0.00% |
| LZ4B12F | 2.25 / 2.25 | 4640 / 4652 | 1141 / 1120 | passed / passed | -1.05% / -1.31% |
| LZ4B12N | 2.26 / 2.26 | 4756 / 4744 | 1113 / 1112 | passed / passed | -1.24% / -1.38% |
| LZ4B12IN | 2.26 / 2.26 | 4723 / 4736 | 1131 / 1115 | passed / passed | -1.17% / -1.38% |
| LZ4B12FN | 2.25 / 2.25 | 4194 / 4206 | 1123 / 1108 | passed / passed | -2.82% / -3.03% |
| LZ4B11 | 2.25 / 2.25 | 4734 / 4731 | 1111 / 1111 | passed / passed | -1.02% / -1.13% |
| LZ4B10 | 2.24 / 2.24 | 4803 / 4802 | 1126 / 1107 | passed / passed | -0.32% / -0.60% |
| LZ4B10FN | 2.24 / 2.24 | 4179 / 4191 | 1102 / 1101 | passed / passed | -2.77% / -2.82% |

Page-in cost = decompress + 1.96 ms x sectors + 0.35 x compress. Sectors = 4096 / ratio / 512 (mean; per-page sizes are not in the logs). The bus-time term of the README formula is not in the output and is omitted.

LZ4B12 does not repeat 2026-10-02: compress 5,067 / 5,048 us against 4,712; decompress 1,130 / 1,113 against 1,169; copy floor 754 against 832. Ratio matches (2.26). The run does not count as a repeat.
