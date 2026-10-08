#!/usr/bin/env python3
"""Check an M8TS2.BIN against ATI TEST.COM's own Test Sequence 2 expectations.

    python m8ts2_decode.py TEST.COM M8TS2.BIN [M8TS2_other.BIN]

TS2's pointer table (TEST.COM 5A31h) lists 71 sub-tests as (write table, compare list); each compare
entry is (port, expected, mask). M8TS2 records the value read for every entry in order. Prints each
mismatch against TEST.COM's expectation - the first is where TEST.COM itself stops - and, given a second
file, every entry where the two runs differ.
"""
import struct
import sys

NAMES = {0x9AE8: 'GP_STAT', 0x82E8: 'CUR_Y', 0x86E8: 'CUR_X', 0x92E8: 'ERR_TERM', 0xE2E8: 'PIX_TRANS',
         0x62EE: 'EXT_GE_STATUS', 0x72EE: 'BOUNDS_L', 0x76EE: 'BOUNDS_T', 0x7AEE: 'BOUNDS_R',
         0x7EEE: 'BOUNDS_B', 0xD6EE: 'PATT_INDEX', 0xDAEE: 'R_SRC_X', 0xDEEE: 'R_SRC_Y'}


def entries(tc):
    o = 0x5A31 - 0x100
    sub = 0
    while True:
        wt, ct = struct.unpack_from('<HH', tc, o)
        o += 4
        if wt == 0:
            return
        sub += 1
        i = ct - 0x100
        while True:
            p = struct.unpack_from('<H', tc, i)[0]
            if (p & 0xff) == 0:
                break
            e, m = struct.unpack_from('<HH', tc, i + 2)
            yield sub, wt, ct, p, e, m
            i += 6


def load(path):
    d = open(path, 'rb').read()
    assert d[:4] == b'M8Z1', path
    n = struct.unpack_from('<H', d, 4)[0]
    vals = list(struct.unpack_from('<%dH' % n, d, 8))
    fin = d[8 + 2 * n:]
    return d[6], d[7], vals, fin


tc = open(sys.argv[1], 'rb').read()
ents = list(entries(tc))
ti, td, vals, fin = load(sys.argv[2])
print('%s: %d read-backs (TS2 has %d), timeouts idle %d data %d' % (sys.argv[2], len(vals), len(ents), ti, td))
bad = 0
for (sub, wt, ct, p, e, m), v in zip(ents, vals):
    if (v & m) != e:
        bad += 1
        if bad <= 20:
            print('  sub-test %2d (writes %04X): %-13s read %04X, expected %04X mask %04X' %
                  (sub, wt, NAMES.get(p, '%04X' % p), v, e, m))
print('  %d of %d differ from TEST.COM' % (bad, len(vals)))
fw, fg = struct.unpack_from('<HH', fin, 0)
print('  final read: %d words, GP_STAT %04X' % (fw, fg))
if len(sys.argv) > 3:
    _, _, other, ofin = load(sys.argv[3])
    for (sub, wt, ct, p, e, m), a, b in zip(ents, vals, other):
        if a != b:
            print('  differs: sub-test %2d %-13s %04X vs %04X (expected %04X)' % (sub, NAMES.get(p, '%04X' % p), a, b, e))
    print('  final 32 words', 'same' if fin[6:70] == ofin[6:70] else 'DIFFER')
