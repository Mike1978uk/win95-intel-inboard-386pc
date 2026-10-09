#!/usr/bin/env python3
"""Fit line-stepping rules to the LINEDRAW pixels M8CONF read back, for the card and the bed.

    python m8line_fit.py <M8CONF.DAT> <card.BIN> <bed.BIN>

A pixel counts as drawn when it differs from the background M8CONF drew (x XOR 5y). Each rule predicts
the pixels of every segment from the LINEDRAW points; segment end points are left out, because
LINEDRAW_OPT decides whether they are written. All rules step the minor axis when the error term is
>= 0, then add 2*min and, on a minor step, subtract 2*max. They differ only in the starting error term:

    model   min - max, as vid_ati_mach8.c had it at dcfbf6e67
    guide   2*min - max, less 1 when start X < end X (ATI guide, ERR_TERM, p. 8-39)
    xneg    2*min - max, less 1 when start X > end X
"""
import sys

from m8conf_cmp import pattern, pixels, read_bin, read_dat

RULES = {
    'model': lambda mn, mx, x0, x1: mn - mx,
    'guide': lambda mn, mx, x0, x1: 2 * mn - mx - (1 if x0 < x1 else 0),
    'xneg': lambda mn, mx, x0, x1: 2 * mn - mx - (1 if x0 > x1 else 0),
}


def s16(v):
    return v - 0x10000 if v & 0x8000 else v


def line(x0, y0, x1, y1, init):
    a, b = abs(x1 - x0), abs(y1 - y0)
    sx, sy = (-1 if x1 < x0 else 1), (-1 if y1 < y0 else 1)
    mn, mx = min(a, b), max(a, b)
    err, x, y, pts = init(mn, mx, x0, x1), x0, y0, []
    for _ in range(mx + 1):
        pts.append((x, y))
        if err >= 0:
            if a > b:
                y += sy
            else:
                x += sx
            err -= 2 * mx
        if a > b:
            x += sx
        else:
            y += sy
        err += 2 * mn
    return pts


def main():
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    tests, card, bed = read_dat(sys.argv[1]), read_bin(sys.argv[2]), read_bin(sys.argv[3])
    for n, t in enumerate(tests):
        pts = [s16(v) for p, v, _ in t['op'] if p == 0xFEEE]
        if len(pts) < 4:
            continue
        segs = list(zip(pts[0::2], pts[1::2]))
        pc, pb = pixels(card[n], t), pixels(bed[n], t)
        ends = set(segs)
        drawn_c = {k for k, v in pc.items() if v != pattern(*k)} - ends
        drawn_b = {k for k, v in pb.items() if v != pattern(*k)} - ends
        res = []
        for name, init in RULES.items():
            pred = set()
            for (x0, y0), (x1, y1) in zip(segs, segs[1:]):
                pred |= set(line(x0, y0, x1, y1, init))
            pred = (pred & set(pc)) - ends
            res.append('%s card %s bed %s' % (name, 'Y' if pred == drawn_c else '-', 'Y' if pred == drawn_b else '-'))
        print('test %2d %-36s %s' % (n, segs, '   '.join(res)))


if __name__ == '__main__':
    main()
