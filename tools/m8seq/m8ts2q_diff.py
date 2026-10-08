#!/usr/bin/env python3
"""Diff two M8TS2Q.BIN files (card, bed): the first checkpoint whose 8x8 block differs names the TS2
sub-test that makes the difference (checkpoint k = after sub-tests 1..k).

    python m8ts2q_diff.py CARD.BIN BED.BIN
"""
import struct
import sys

REC = 134


def load(path):
    d = open(path, 'rb').read()
    assert d[:4] == b'M8Q1', path
    n = struct.unpack_from('<H', d, 4)[0]
    out = []
    for k in range(n):
        r = d[6 + k * REC:6 + (k + 1) * REC]
        if len(r) < REC:
            break
        cnt, gp = struct.unpack_from('<HH', r, 0)
        out.append((cnt, gp, r[4], r[5], list(struct.unpack_from('<32H', r, 6))))
    return out


def main():
    a, b = load(sys.argv[1]), load(sys.argv[2])
    print('checkpoints: %d / %d' % (len(a), len(b)))
    first = None
    for k, (ra, rb) in enumerate(zip(a, b)):
        if ra != rb:
            if first is None:
                first = k
            diffs = ['row %d word %d: %04X vs %04X' % (i // 4, i % 4, x, y)
                     for i, (x, y) in enumerate(zip(ra[4], rb[4])) if x != y]
            head = '' if ra[:4] == rb[:4] else ' (count/GP_STAT/timeouts %s vs %s)' % (ra[:4], rb[:4])
            print('checkpoint %2d differs%s: %s' % (k, head, '; '.join(diffs)))
    print('no difference' if first is None else 'first difference after sub-test %d' % first)


if __name__ == '__main__':
    main()
