#!/usr/bin/env python3
"""Decode M8WRAP.BIN, and compare two of them (card and bed).

    python m8wrap_decode.py <M8WRAP.BIN> [<other.BIN>]

For each case, the pixels that differ from the control case (case 0), row by row, as runs of
"x0-x1=value". X 960-1023 and 0-63 are read, Y 0-15. Wrapped, a draw past X 1023 continues at X 0 of the
same row; addressed linearly, on the next row.
"""
import struct
import sys

CASES = ['control', '8514/A rect X 1016 w16 Y 2-5', 'ATI blit fill X 1016-1031 Y 2-5',
         '8514/A blit src (1020,8) -> (24,8)', '8514/A blit src (0,8) -> (1020,12)']


def load(path):
    d = open(path, 'rb').read()
    if d[:4] != b'M8WR':
        raise SystemExit('%s is not an M8WRAP result' % path)
    n = struct.unpack_from('<H', d, 4)[0]
    recs = [d[6 + k * 2052:6 + (k + 1) * 2052] for k in range(n)]
    out = []
    for r in recs:
        px = {}
        for a, ox in ((0, 960), (1, 0)):
            for y in range(16):
                for x in range(64):
                    px[(ox + x, y)] = r[4 + a * 1024 + y * 64 + x]
        out.append((r[0], r[1], px))
    return out


def runs(row):
    """[(x0, x1, v)] for a sorted list of (x, v)."""
    out = []
    for x, v in row:
        if out and out[-1][2] == v and out[-1][1] + 1 == x:
            out[-1][1] = x
        else:
            out.append([x, x, v])
    return out


def describe(case, control):
    to_i, to_d, px = case
    lines = ['timeouts %d/%d' % (to_i, to_d)]
    for y in range(16):
        row = sorted((x, v) for (x, yy), v in px.items() if yy == y and v != control[(x, yy)])
        if row:
            lines.append('y%-2d ' % y + '  '.join('%d-%d=%02X' % tuple(r) for r in runs(row)))
    return lines


def main():
    if len(sys.argv) not in (2, 3):
        raise SystemExit(__doc__)
    files = [load(p) for p in sys.argv[1:]]
    for k in range(len(files[0])):
        print('== case %d: %s' % (k, CASES[k] if k < len(CASES) else ''))
        for name, f in zip(('card', 'bed ') if len(files) == 2 else ('',), files):
            control = f[0][2]
            if k == 0:
                vals = sorted(set(control.values()))
                print('  %s background values %s' % (name, ' '.join('%02X' % v for v in vals)))
                continue
            for line in describe(f[k], control):
                print('  %s %s' % (name, line))
        if len(files) == 2:
            print('  identical' if files[0][k][2] == files[1][k][2] else '  DIFFER')


if __name__ == '__main__':
    main()
