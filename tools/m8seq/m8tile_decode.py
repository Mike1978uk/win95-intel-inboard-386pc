#!/usr/bin/env python3
"""Decode M8TILE.BIN: did the ATI blit move its source independently of its destination?

    python tools/m8seq/m8tile_decode.py M8TILE.BIN [OTHER.BIN]
"""

import sys

ROWS = [0, 1, 200, 201, 202, 203]
# The source is one stream: at SRC_X_END it moves to SRC_X_START on the next source row (guide 9-62),
# whatever the destination's shape. A: rows 100-104 in turn fill destination row 0's 40 pixels (rows
# 103-104 are empty), and row 1 continues from source row 105. Card = model, 10-10.
EXPECT = {
    0: [0x10 + x for x in range(8)] + [0x20 + x for x in range(8)] + [0x30 + x for x in range(8)] + [0] * 16,
    1: [0] * 40,
    200: [0x30, 0x31, 0x32, 0x33],
    201: [0x34, 0x35, 0x36, 0x37],
    202: [0x38, 0x39, 0x3A, 0x3B],
    203: [0x3C, 0x3D, 0x3E, 0x3F],
}


def load(path):
    d = open(path, "rb").read()
    if d[:4] != b"M8TL":
        sys.exit(f"{path}: not an M8TILE.BIN")
    return {r: d[4 + i * 256:4 + (i + 1) * 256] for i, r in enumerate(ROWS)}


def main():
    a = load(sys.argv[1])
    b = load(sys.argv[2]) if len(sys.argv) > 2 else None
    for r in ROWS:
        exp = EXPECT[r]
        got = list(a[r][:len(exp) + 4])
        ok = got[:len(exp)] == exp and not any(got[len(exp):])
        line = f"row {r:3}: {' '.join('%02X' % v for v in got)}   {'as expected' if ok else 'NOT as expected'}"
        if b is not None:
            line += "   = other" if a[r] == b[r] else "   differs from other"
        print(line)


if __name__ == "__main__":
    main()
