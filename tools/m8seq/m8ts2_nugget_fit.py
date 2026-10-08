#!/usr/bin/env python3
"""Fit the Mach8 planar (nugget) PIX_TRANS read-back to TEST.COM's TS2 sub-tests 2-11.

    python m8ts2_nugget_fit.py TEST.COM

Sub-test 2 writes 16 pixels at y=201h (8 PIX_TRANS words, DP_CONFIG 4211h); sub-tests 2-11 read them
back with an 8514 planar read (CMD 211Ah 8-bit bus, 231Ah 16-bit bus) under different RD_MASK values.
Richter & Smith p. 299: planar data is nugget-oriented, bits 4,3,2,1 = pixel columns 0-3. A pixel reads
as 1 when (pixel | ~RD_MASK) == FFh (ATI guide, DP_CONFIG POLY_FILL_MODE).

Tries pixel order within a written word, nugget bit order, the RD_MASK rotation (guide p. 8-48) and the
byte order of two nuggets in a 16-bit word, and prints the best fits with their remaining mismatches.
Result: 0 mismatches with pixels high byte first, nugget bits 4..1 = columns 0..3, RD_MASK rotated, and
the first group in the high byte of a 16-bit read (CMD bit 12 clear).
"""
import struct
import sys

WORDS = [0x0102, 0x0408, 0x1020, 0x4080, 0x00FF, 0x1122, 0x4488, 0x55AA]


def subtests(d):
    o = 0x5A31 - 0x100
    out = []
    for s in range(1, 12):
        wt, ct = struct.unpack_from('<HH', d, o)
        o += 4
        i = wt - 0x100
        rm = cmd = None
        while True:
            p = struct.unpack_from('<H', d, i)[0]
            if p == 0xFFFF:
                i += 2
                continue
            if (p & 0xff) == 0:
                break
            v = struct.unpack_from('<H', d, i + 2)[0]
            i += 4
            if p == 0xAEE8:
                rm = v
            if p == 0x9AE8:
                cmd = v
        c = []
        i = ct - 0x100
        while True:
            p = struct.unpack_from('<H', d, i)[0]
            if (p & 0xff) == 0:
                break
            e, m = struct.unpack_from('<HH', d, i + 2)
            c.append((e, m))
            i += 6
        out.append((s, rm, cmd, c))
    return out[1:11]


def rev8(x):
    """ATI guide p. 8-48, RD_MASK note 3: for IBM monochrome reads RD_MASK is the true mask rotated
    left one bit (bit 0 = plane 7). Undo that."""
    return ((x & 1) << 7) | ((x & 0xfe) >> 1)


def run(subs, order, nrev, mrev, wbo, verbose=False):
    px = []
    for w in WORDS:
        hi, lo = w >> 8, w & 0xff
        px += [hi, lo] if order == 'hi' else [lo, hi]
    bad = 0
    for s, rm, cmd, c in subs:
        m = rev8(rm) if mrev else rm
        bit = [1 if ((p | (~m & 0xff)) & 0xff) == 0xff else 0 for p in px]

        def nug(g):
            return sum(bit[4 * g + k] << ((4 - k) if not nrev else (1 + k)) for k in range(4))
        if cmd & 0x200:
            vals = [(nug(2 * i) | (nug(2 * i + 1) << 8)) if wbo == 'lo' else (nug(2 * i + 1) | (nug(2 * i) << 8))
                    for i in range(len(c))]
        else:
            vals = [nug(i) for i in range(len(c))]
        for (e, mk), v in zip(c, vals):
            if (v & mk) != (e & mk):
                bad += 1
                if verbose:
                    print('  sub %d rd %02X cmd %04X exp %04X got %04X' % (s, rm, cmd, e & mk, v & mk))
    return bad


def main():
    subs = subtests(open(sys.argv[1], 'rb').read())
    fits = sorted((run(subs, o, n, m, w), o, n, m, w)
                  for o in ('hi', 'lo') for n in (0, 1) for m in (0, 1) for w in ('lo', 'hi'))
    for f in fits[:4]:
        print('%2d mismatches: pixel order %s, nugget reversed %d, rd_mask rotated %d, word byte order %s' % f)
    run(subs, *fits[0][1:], verbose=True)


if __name__ == '__main__':
    main()
