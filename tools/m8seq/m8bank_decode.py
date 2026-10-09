#!/usr/bin/env python3
"""Decode M8BANK.BIN and compare it with what XFree86's page-register layout predicts.

    python m8bank_decode.py <M8BANK.BIN>

XFree86 3.3.6 (atibanks.s) gives the 28800's B2h as: D7-D5 read page bits 2-0, D4 page bit 3, D3-D1 page
bits 2-0, D0 read page bit 3; BEh bit 3 enables separate read and write pages. With BEh bit 3 clear the
page in D4-D1 serves both. The prediction replays M8BANK's writes and reads through that layout. Page 8 is
past the 512 KB on the card, so its result is shown both ways: as its own storage, and as aliasing page 0.
"""
import sys

VALS = [0x00, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x01]


def pages(v, dual):
    write = (v >> 1) & 0x0F
    read = (((v & 1) << 3) | (v >> 5)) if dual else write
    return read, write


def predict(dual, alias8):
    mem = {}
    norm = (lambda p: p & 7) if alias8 else (lambda p: p)
    for v in VALS:
        mem[norm(pages(v, dual)[1])] = 0
    for i, v in enumerate(VALS):
        mem[norm(pages(v, dual)[1])] = 0x10 + i
    return [mem.get(norm(pages(v, dual)[0]), 0) for v in VALS]


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    d = open(sys.argv[1], 'rb').read()
    if d[:4] != b'M8BK':
        raise SystemExit('not an M8BANK result')
    mode, b2, be = d[4], d[5], d[6]
    print('mode after set %02Xh (62h expected)   B2h as found %02X   BEh as found %02X' % (mode, b2, be))
    for k, dual in enumerate((False, True)):
        got = list(d[7 + 9 * k:16 + 9 * k])
        print('\nBEh bit 3 %s (%s pages)' % ('set' if dual else 'clear', 'separate' if dual else 'one'))
        print('  B2h value  ' + ' '.join('%02X' % v for v in VALS))
        print('  card read  ' + ' '.join('%02X' % g for g in got))
        for alias8 in (False, True):
            p = predict(dual, alias8)
            print('  predicted  ' + ' '.join('%02X' % x for x in p) + '   (page 8 %s) %s' % (
                'aliases page 0' if alias8 else 'separate', 'MATCH' if p == got else ''))


if __name__ == '__main__':
    main()
