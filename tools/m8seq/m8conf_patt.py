#!/usr/bin/env python3
"""List the M8CONF tests whose foreground source is the colour pattern (DP_CONFIG bits 15:13 = 5), with
the values that separate rules for where each row's pattern index starts.

    python m8conf_patt.py <M8CONF.DAT> <card.BIN> <bed.BIN>

A test whose PATT_INDEX differs from its first X modulo the pattern length separates "the index
reloads from PATT_INDEX" from "the index follows the destination X".
"""
import sys

from m8conf_cmp import last, pixdiff, read_bin, read_dat

TRIG = (0xAEEE, 0xCAEE, 0x96EE, 0xFEEE, 0x9AE8)


def main():
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    tests, card, bed = read_dat(sys.argv[1]), read_bin(sys.argv[2]), read_bin(sys.argv[3])
    for n, t in enumerate(tests):
        e = t['st'] + t['op']
        dpc = last(e, 0xCEEE) or 0
        if (dpc >> 13) != 5:
            continue
        trig = [p for p, _, _ in t['op'] if p in TRIG]
        diff = pixdiff(card[n], bed[n])
        print('test %2d DP_CONFIG %04X trigger %s PATT_INDEX %s PATT_LENGTH %s CUR_X %s DEST_X_START %s '
              'DEST_X_END %s CUR_Y %s DEST_Y_END %s  diff %d' % (
                  n, dpc, '%04X' % trig[0] if trig else '-', last(e, 0xD6EE), last(e, 0xD2EE), last(e, 0x86E8),
                  last(e, 0xA6EE), last(e, 0xAAEE), last(e, 0x82E8), last(e, 0xAEEE), diff))


if __name__ == '__main__':
    main()
