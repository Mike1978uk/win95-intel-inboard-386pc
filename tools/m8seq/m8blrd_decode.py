#!/usr/bin/env python3
"""Decode M8BLRD.BIN (8514 BitBLT read, TS2 sub-test 15) and compare with candidate rules.

    python m8blrd_decode.py M8BLRD.BIN [M8BLRD_other.BIN]

For each case prints the PIX_TRANS words read after the BitBLT, what each candidate rule predicts for the
first 4 (one row per word, two nibbles per word, first nibble in the high byte), and the 16x4 readback.
"""
import struct
import sys

TS2 = [0x0102, 0x0304, 0x0506, 0x0708, 0x1112, 0x1314, 0x1516, 0x1718,
       0x2122, 0x2324, 0x2526, 0x2728, 0x3132, 0x3334, 0x3536, 0x3738]
DIS = [0xF00F, 0x3CC3, 0x8142, 0x2418, 0xFF7F, 0x3F1F, 0xFEFC, 0xF8F0,
       0x55AA, 0x55AA, 0x1113, 0x171F, 0x0103, 0x070F, 0x80C0, 0xE0F0]
CASES = [(TS2, 0xFF, 0), (DIS, 0xFF, 0), (DIS, 0x0F, 0), (DIS, 0xFF, 2)]
REC = 84


def rows(data):
    px = []
    for w in data:
        px += [w >> 8, w & 0xff]
    return [px[8 * r:8 * r + 8] for r in range(4)]


def nib_and(n, rm):
    v = 0xff
    for p in n:
        v &= p
    return v


def nib_mono(n, rm):
    b = 0
    for k, p in enumerate(n):
        if ((p | (~rm & 0xff)) & 0xff) == 0xff:
            b |= 1 << (4 - k)
    return b


RULES = [('AND of nibble', nib_and),
         ('pixel 0 & F0', lambda n, rm: n[0] & 0xf0),
         ('pixel 3 & F0', lambda n, rm: n[3] & 0xf0),
         ('mono nugget', nib_mono),
         ('AND & RD_MASK', lambda n, rm: nib_and(n, rm) & rm)]


def load(path):
    d = open(path, 'rb').read()
    assert d[:4] == b'M8BL', 'not an M8BLRD.BIN'
    n = struct.unpack_from('<H', d, 4)[0]
    out = []
    for i in range(n):
        r = d[6 + i * REC:6 + (i + 1) * REC]
        out.append((r[0], r[1], struct.unpack_from('<H', r, 2)[0],
                    struct.unpack_from('<8H', r, 4), struct.unpack_from('<32H', r, 20)))
    return out


def main():
    recs = load(sys.argv[1])
    other = load(sys.argv[2]) if len(sys.argv) > 2 else None
    for c, (ti, td, gp, rd, rb) in enumerate(recs):
        data, rm, sx = CASES[c]
        print('case %d  RD_MASK %02X  source x %d  timeouts idle %d data %d  GP_STAT %04X' % (c, rm, sx, ti, td, gp))
        print('  read      ' + ' '.join('%04X' % w for w in rd))
        if other:
            print('  other     ' + ' '.join('%04X' % w for w in other[c][3]))
        for name, f in RULES:
            pred = [(f(r[:4], rm) << 8) | f(r[4:], rm) for r in rows(data)]
            hit = sum(p == w for p, w in zip(pred, rd[:4]))
            print('  %-14s' % name + ' '.join('%04X' % p for p in pred) + '   %d/4' % hit)
        for y in range(4):
            print('  row %d     ' % y + ' '.join('%04X' % w for w in rb[8 * y:8 * y + 8]))


if __name__ == '__main__':
    main()
