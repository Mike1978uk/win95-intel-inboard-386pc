#!/usr/bin/env python3
"""Decode M8LEND.BIN or M8LOPT.BIN, and compare two of them (card and bed).

    python m8lend_decode.py <M8LEND.BIN> [<other.BIN>]

Per case: whether the line's end point (30,12) and start point (10,2) were drawn, the line's colour,
and how many pixels changed from the 55h background. With two files, the cases are printed side by side.
"""
import struct
import sys

CASES = {b'M8LE': ['control', 'engine reset (32EE)', 'subsystem reset (42E8)', '8514/A command',
         'FRGD_MIX alone', 'command, FRGD_MIX', 'command, DP_CONFIG', 'command, LINEDRAW_OPT'],
         b'M8LO': ['070C', '070C, CMD bit 2 clear', '070C, CMD bit 2 set', '0708', '0708, CMD bit 2 set',
                   '0708, CMD bit 2 clear']}
BACK = 0x55


def load(path):
    d = open(path, 'rb').read()
    if d[:4] not in CASES:
        raise SystemExit('%s is not an M8LEND or M8LOPT result' % path)
    n = struct.unpack_from('<H', d, 4)[0]
    return d[:4], [d[6 + k * 1028:6 + (k + 1) * 1028] for k in range(n)]


def summary(rec, kind):
    px = rec[4:]
    drawn = [(i % 64, i // 64) for i in range(1024) if px[i] != BACK]
    colours = sorted({px[x + 64 * y] for x, y in drawn})
    end, start = px[30 + 64 * 12], px[10 + 64 * 2]
    opt = ('LINEDRAW_OPT %04X ' % struct.unpack_from('<H', rec, 2)[0]) if kind == b'M8LO' else ''
    return '%send %s start %s colour %s px %d to %d/%d' % (opt,
        'DRAWN' if end != BACK else 'no   ', 'drawn' if start != BACK else 'no', '/'.join('%02X' % c for c in colours) or '-',
        len(drawn), rec[0], rec[1])


def main():
    if len(sys.argv) not in (2, 3):
        raise SystemExit(__doc__)
    loaded = [load(p) for p in sys.argv[1:]]
    kind = loaded[0][0]
    if any(k != kind for k, _ in loaded):
        raise SystemExit('the two files come from different probes')
    files = [f for _, f in loaded]
    names = CASES[kind]
    for k in range(len(files[0])):
        name = names[k] if k < len(names) else 'case %d' % k
        print('%d %-24s %s' % (k, name, '   |   '.join(summary(f[k], kind) for f in files)))
    if len(files) == 2:
        same = [k for k in range(len(files[0])) if files[0][k][4:] == files[1][k][4:]]
        print('pixels identical in cases %s' % same)


if __name__ == '__main__':
    main()
