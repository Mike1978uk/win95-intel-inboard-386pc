#!/usr/bin/env python3
"""Compare two M8CONF.BIN results (card and bed) test by test, using the M8CONF.DAT that made them.

    python m8conf_cmp.py <M8CONF.DAT> <card.BIN> <bed.BIN>            list the tests that differ
    python m8conf_cmp.py <M8CONF.DAT> <card.BIN> <bed.BIN> 28 58      dump those tests

Tests are numbered from 0 in .DAT order, which is gen_m8conf.py's sorted key order. A dump prints the
test's replayed state and operation, then each differing row of the two 64x16 read areas with the card,
the bed and the background M8CONF drew (pixel = x XOR 5y).
"""
import struct
import sys

# Write-side names, from the ATI Mach32 guide's register-by-address table (REG688000-15, appendix A).
NAMES = {
    0x36EE: 'FIFO_OPT', 0x6AEE: 'MAX_WAITSTATES', 0x6EEE: 'GE_OFFSET_LO', 0x72EE: 'GE_OFFSET_HI',
    0x76EE: 'GE_PITCH', 0x7AEE: 'EXT_GE_CONFIG', 0x82E8: 'CUR_Y', 0x82EE: 'PATT_DATA_INDEX',
    0x86E8: 'CUR_X', 0x8AE8: 'DEST_Y/AXSTP', 0x8EE8: 'DEST_X/DIASTP', 0x8EEE: 'PATT_DATA',
    0x92E8: 'ERR_TERM', 0x96E8: 'MAJ_AXIS_PCNT', 0x96EE: 'BRES_COUNT', 0x9AE8: 'CMD',
    0x9AEE: 'LINEDRAW_INDEX', 0xA2E8: 'BKGD_COLOR', 0xA2EE: 'LINEDRAW_OPT', 0xA6E8: 'FRGD_COLOR',
    0xA6EE: 'DEST_X_START', 0xAAE8: 'WRT_MASK', 0xAAEE: 'DEST_X_END', 0xAEE8: 'RD_MASK',
    0xAEEE: 'DEST_Y_END', 0xB2E8: 'CMP_COLOR', 0xB2EE: 'SRC_X_START', 0xB6E8: 'BKGD_MIX',
    0xB6EE: 'ALU_BG_FN', 0xBAE8: 'FRGD_MIX', 0xBAEE: 'ALU_FG_FN', 0xBEE8: 'MULTIFUNC',
    0xBEEE: 'SRC_X_END', 0xC2EE: 'SRC_Y_DIR', 0xCAEE: 'SCAN_TO_X', 0xCEEE: 'DP_CONFIG',
    0xD2EE: 'PATT_LENGTH', 0xD6EE: 'PATT_INDEX', 0xDAEE: 'EXT_SCISSOR_L', 0xDEEE: 'EXT_SCISSOR_T',
    0xE2E8: 'PIX_TRANS', 0xE2EE: 'EXT_SCISSOR_R', 0xE6EE: 'EXT_SCISSOR_B', 0xFEEE: 'LINEDRAW',
}


def read_dat(path):
    """Tests from M8CONF.DAT: state and operation entries (port, value, width) and the two read areas."""
    d, i, tests = open(path, 'rb').read(), 0, []
    while struct.unpack_from('<H', d, i)[0] != 0xFFFF:
        ns, no, ax, ay, bx, by = struct.unpack_from('<6H', d, i)
        i += 12
        ents = [struct.unpack_from('<3H', d, i + 6 * k) for k in range(ns + no)]
        i += 6 * (ns + no)
        tests.append(dict(a=(ax, ay), b=(bx, by), st=ents[:ns], op=ents[ns:]))
    return tests


MAXRD = {b'M8C2': 2048, b'M8C3': 4096}


def read_bin(path):
    """Records from M8CONF.BIN: idle timeouts, data timeouts, then 2 x 1024 pixel bytes. "M8CF" files
    have 2 pad bytes before the pixels; "M8C2" and "M8C3" files have the count of words read back from
    a read operation there, and MAXRD words of that data after the pixels."""
    d = open(path, 'rb').read()
    rec = 2052 + 2 * MAXRD[d[:4]] if d[:4] in MAXRD else (2052 if d[:4] == b'M8CF' else None)
    if rec is None:
        raise SystemExit('%s is not an M8CONF result' % path)
    return [d[k:k + rec] for k in range(4, len(d) - rec + 1, rec)]


def read_words(rec):
    """The words a read operation returned, or None for a record without them."""
    if len(rec) <= 2052:
        return None
    n = struct.unpack_from('<H', rec, 2)[0]
    return list(struct.unpack_from('<%dH' % n, rec, 2052))


def pixdiff(c, b):
    return sum(1 for x, y in zip(c[4:2052], b[4:2052]) if x != y)


def pattern(x, y):
    return (x ^ (5 * y)) & 0xFF


def pixels(rec, test):
    """{(x, y): value} for both read areas of one record."""
    out = {}
    for ai, (ox, oy) in enumerate((test['a'], test['b'])):
        for r in range(16):
            for x in range(64):
                out[(ox + x, oy + r)] = rec[4 + ai * 1024 + r * 64 + x]
    return out


def last(entries, port):
    v = None
    for p, val, _ in entries:
        if p == port:
            v = val
    return v


def dump_entries(title, entries):
    print(' %s (%d):' % (title, len(entries)))
    prev, rep = None, 0
    for e in entries + [None]:
        if e is not None and e == prev:
            rep += 1
            continue
        if rep:
            print('     ... x%d more' % rep)
            rep = 0
        if e is not None:
            print('   %04X %-16s %04X w%d' % (e[0], NAMES.get(e[0], ''), e[1], e[2]))
        prev = e


def readback(c, b):
    """'' when neither record holds read data, else a summary of the two."""
    wc, wb = read_words(c), read_words(b)
    if wc is None or wb is None or (not wc and not wb):
        return ''
    k = next((i for i, (x, y) in enumerate(zip(wc, wb)) if x != y), None)
    if k is None and len(wc) == len(wb):
        return '  read %d words, same' % len(wc)
    at = ('first differ at word %d' % k) if k is not None else 'common words agree'
    return '  read card %d bed %d words, %s' % (len(wc), len(wb), at)


def dump(n, test, c, b):
    print('=' * 70)
    print('test %d  pixels diff %d bytes  timeouts card %d/%d bed %d/%d  area A %s  area B %s%s'
          % (n, pixdiff(c, b), c[0], c[1], b[0], b[1], test['a'], test['b'], readback(c, b)))
    dump_entries('state', test['st'])
    dump_entries('operation', test['op'])
    wc, wb = read_words(c), read_words(b)
    if wc or wb:
        for name, w in (('card', wc), ('bed ', wb)):
            print(' read %s %s%s' % (name, ' '.join('%04X' % x for x in (w or [])[:16]), ' ...' if w and len(w) > 16 else ''))
    for ai, (ox, oy) in enumerate((test['a'], test['b'])):
        base = 4 + ai * 1024
        for r in range(16):
            cr, br = c[base + r * 64:base + r * 64 + 64], b[base + r * 64:base + r * 64 + 64]
            cols = [x for x in range(64) if cr[x] != br[x]]
            if not cols:
                continue
            lo, hi = max(0, cols[0] - 2), min(64, cols[-1] + 3)
            print(' area %s y=%d: x %d..%d differ (%d px)' % ('AB'[ai], oy + r, ox + cols[0], ox + cols[-1], len(cols)))
            print('    x    ' + ' '.join('%3d' % ((ox + x) % 1000) for x in range(lo, hi)))
            print('    card ' + ' '.join(' %02X' % cr[x] for x in range(lo, hi)))
            print('    bed  ' + ' '.join(' %02X' % br[x] for x in range(lo, hi)))
            print('    back ' + ' '.join(' %02X' % pattern(ox + x, oy + r) for x in range(lo, hi)))


def main():
    if len(sys.argv) < 4:
        raise SystemExit(__doc__)
    tests, card, bed = read_dat(sys.argv[1]), read_bin(sys.argv[2]), read_bin(sys.argv[3])
    print('%d tests, card %d records, bed %d records' % (len(tests), len(card), len(bed)))
    want = [int(x) for x in sys.argv[4:]]
    for n, (c, b) in enumerate(zip(card, bed)):
        if want:
            if n in want:
                dump(n, tests[n], c, b)
            continue
        diff, rb = pixdiff(c, b), readback(c, b)
        if diff or c[:2] != b[:2] or (rb and not rb.endswith('same')):
            print('test %2d  diff %4d  timeouts card %d/%d bed %d/%d%s' % (n, diff, c[0], c[1], b[0], b[1], rb))


if __name__ == '__main__':
    main()
