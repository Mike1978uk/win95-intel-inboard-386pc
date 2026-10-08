#!/usr/bin/env python3
"""Print TS2 sub-tests side by side: each write table, then each compare with TEST.COM's expected value,
the card's value and the bed's.

    python m8ts2_table.py TEST.COM CARD.BIN BED.BIN [first [last]]

Write tables are (port, value) pairs; FFFF alone is a wait for idle; a port with low byte 0 ends the table
(TEST.COM pointer table 5A31h). '*' marks a compare where the bed differs from the card.
"""
import struct
import sys

NAMES = {0x9AE8: 'GP_STAT', 0x82E8: 'CUR_Y', 0x86E8: 'CUR_X', 0x8AE8: 'DESTY', 0x8EE8: 'DESTX',
         0x92E8: 'ERR', 0x96E8: 'MAJ', 0xA6E8: 'FRGD_C', 0xAAE8: 'WRT_MASK',
         0xAEE8: 'RD_MASK', 0xB6E8: 'BKGD_MIX', 0xBAE8: 'FRGD_MIX', 0xBEE8: 'MULTI', 0xE2E8: 'PIX_TRANS',
         0x62EE: 'EXT_GE_STATUS', 0x72EE: 'BOUNDS_L', 0x76EE: 'BOUNDS_T', 0x7AEE: 'BOUNDS_R',
         0x7EEE: 'BOUNDS_B', 0x82EE: 'PATT_DATA_IDX', 0x8EEE: 'PATT_DATA', 0x9AEE: 'LD_INDEX',
         0xA2EE: 'LINEDRAW_OPT', 0xA6EE: 'DEST_X_START', 0xAAEE: 'DEST_X_END', 0xAEEE: 'DEST_Y_END',
         0xCEEE: 'DP_CONFIG', 0xD2EE: 'PATT_LENGTH', 0xD6EE: 'PATT_INDEX', 0xDAEE: 'SC_LEFT',
         0xDEEE: 'SC_TOP', 0xE2EE: 'SC_RIGHT', 0xE6EE: 'SC_BOTTOM', 0xFEEE: 'LINEDRAW'}
CMP_NAMES = {**NAMES, 0xDAEE: 'R_SRC_X', 0xDEEE: 'R_SRC_Y'}


def name(p, table=NAMES):
    return table.get(p, '%04X' % p)


def subtests(tc):
    o = 0x5A31 - 0x100
    while True:
        wt, ct = struct.unpack_from('<HH', tc, o)
        o += 4
        if wt == 0:
            return
        w = []
        i = wt - 0x100
        while True:
            p = struct.unpack_from('<H', tc, i)[0]
            if p == 0xFFFF:
                w.append('wait')
                i += 2
                continue
            if (p & 0xff) == 0:
                break
            w.append('%s=%04X' % (name(p), struct.unpack_from('<H', tc, i + 2)[0]))
            i += 4
        c = []
        i = ct - 0x100
        while True:
            p = struct.unpack_from('<H', tc, i)[0]
            if (p & 0xff) == 0:
                break
            e, m = struct.unpack_from('<HH', tc, i + 2)
            c.append((p, e, m))
            i += 6
        yield w, c


def load(path):
    d = open(path, 'rb').read()
    assert d[:4] == b'M8Z1', path
    n = struct.unpack_from('<H', d, 4)[0]
    return list(struct.unpack_from('<%dH' % n, d, 8))


def main():
    tc = open(sys.argv[1], 'rb').read()
    card, bed = load(sys.argv[2]), load(sys.argv[3])
    first = int(sys.argv[4]) if len(sys.argv) > 4 else 1
    last = int(sys.argv[5]) if len(sys.argv) > 5 else 999
    k = 0
    for s, (w, c) in enumerate(subtests(tc), 1):
        if first <= s <= last:
            print('%2d %s' % (s, ' '.join(w)))
        for p, e, m in c:
            if first <= s <= last:
                print('     %-13s exp %04X/%04X card %04X bed %04X %s' %
                      (name(p, CMP_NAMES), e, m, card[k], bed[k], '*' if card[k] != bed[k] else ''))
            k += 1


if __name__ == '__main__':
    main()
