#!/usr/bin/env python3
"""Make M8CONF.DAT for M8CONF.COM: one real instance of every engine operation shape a driver used, taken
from 86Box logs made with MACH8_WLOG=1 while the driver ran, so the card and the 86Box model can be given
the same operations and their pixels compared.

    python gen_m8conf.py <M8CONF.DAT> <log> [<log> ...]

An operation is the writes since the previous one ended, its trigger, and any PIX_TRANS (E2E8h) data that
follows: DEST_Y_END (AEEEh) starts a blit, SCAN_TO_X (CAEEh) a raster draw, BRES_COUNT (96EEh) a Bresenham
line, a run of LINEDRAW points (FEEEh) a polyline, CMD (9AE8h) an 8514/A command. Its shape is the trigger,
DP_CONFIG, the foreground and background mixes (ALU_FG_FN BAEEh, ALU_BG_FN B6EEh, low 5 bits), and whether
host data followed. For each shape the largest instance up to 2,048 pixels is kept, with the register
state the driver had set before it (last value per register, MULTIFUNC indices kept apart).

pclog folds identical consecutive lines into "*** N repeats ***", which repeats the line before it; those
are expanded, or a row of host data comes out short (inboard-hw-debug technique 148).

Each test in the .DAT, all little-endian words: count of state entries, count of operation entries, then
two read areas (x, y of the top-left of each 64x16 area), then the entries. An entry is port, value, width
(1 or 2). The file ends with a state count of FFFFh.
"""
import struct
import sys

TRIG = {0xAEEE: 'blit', 0xCAEE: 'scan', 0x96EE: 'bres', 0x9AE8: 'cmd'}
DATA = {0xE2E8}
LINE = 0xFEEE
NOT_STATE = {0xE2E8, 0x9AE8, 0xAEEE, 0xCAEE, 0x96EE, 0xFEEE, 0x9EE8, 0x32EE, 0x42E8,
             0x02E8, 0x06E8, 0x0AE8, 0x0EE8, 0x12E8, 0x16E8, 0x1AE8, 0x1EE8, 0x22E8,
             0x46EE, 0x4AE8, 0x4AEE, 0x5AEE, 0x52EE, 0x56EE, 0x56EF, 0x26EE, 0x2AEE, 0x2EEE}


def read_log(path):
    w, last = [], None
    for line in open(path, errors='replace'):
        if line.startswith('M8W '):
            a = line.split()
            last = (int(a[1], 16), int(a[2], 16), int(a[3]))
            w.append(last)
        elif line.startswith('*** ') and line.rstrip().endswith(' repeats ***'):
            if last is not None:
                w.extend([last] * int(line.split()[1]))
        else:
            last = None
    return w


def s16(v):
    return v - 0x10000 if v & 0x8000 else v


def ops_of(w):
    """Yield (start, end, kind) index ranges: setup since the previous op, trigger, data after it."""
    i, start, n = 0, 0, len(w)
    while i < n:
        p = w[i][0]
        if p in TRIG or p == LINE:
            kind = TRIG.get(p, 'line')
            j = i + 1
            if p == LINE:
                while j < n and w[j][0] == LINE:
                    j += 1
            while j < n and w[j][0] in DATA:
                j += 1
            yield start, i, j, kind
            start = j
            i = j
        else:
            i += 1


def setreg(state, p, v, n):
    """Keep the state in last-write order: on the Graphics Ultra the register written last wins
    between DP_CONFIG and the 8514/A mixes, so the replay must write them in that order."""
    k = (p, v >> 12 if p == 0xBEE8 else None)
    state.pop(k, None)
    state[k] = (p, v, n)


def bbox(w, s, t, e, kind, st):
    reg = {}
    for p, v, _ in w[s:t]:
        reg[p] = v
    g = lambda p: reg.get(p, st.get((p, None), (p, 0, 2))[1])
    if kind == 'blit':
        x0, x1 = s16(g(0xA6EE)) & 0x7FF, s16(g(0xAAEE)) & 0x7FF
        y0, y1 = g(0x82E8) & 0x7FF, w[t][1] & 0x7FF
    elif kind == 'scan':
        x0, x1 = g(0x86E8) & 0x7FF, w[t][1] & 0x7FF
        y0 = y1 = g(0x82E8) & 0x7FF
    elif kind == 'bres':
        x0, y0 = g(0x86E8) & 0x7FF, g(0x82E8) & 0x7FF
        x1, y1 = x0 + (w[t][1] & 0x7FF), y0
    elif kind == 'line':
        pts = [s16(v) for p, v, _ in w[t:e] if p == LINE]
        xs, ys = pts[0::2], pts[1::2]
        x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    else:
        x0, y0 = g(0x86E8) & 0x7FF, g(0x82E8) & 0x7FF
        x1 = x0 + (g(0x96E8) & 0x7FF)
        mn = st.get((0xBEE8, 0), (0, 0, 2))[1]
        for p, v, _ in w[s:t]:
            if p == 0xBEE8 and (v >> 12) == 0:
                mn = v
        y1 = y0 + (mn & 0xFFF)
    return min(x0, x1), min(y0, y1), max(x0, x1), max(y0, y1)


def clampxy(x, y):
    return max(0, min(x, 1023 - 63)), max(0, min(y, 1023 - 15))


shapes = {}
for path in sys.argv[2:]:
    w = read_log(path)
    state = {}
    pos = 0
    for s, t, e, kind in ops_of(w):
        for p, v, n in w[pos:s]:
            if p not in NOT_STATE:
                setreg(state, p, v, n)
        pos = s
        reg = dict(((p, v >> 12 if p == 0xBEE8 else None), (p, v, n)) for p, v, n in w[s:t] if p not in NOT_STATE)
        cur = dict(state)
        cur.update(reg)
        dpc = cur.get((0xCEEE, None), (0, 0, 2))[1]
        fm = cur.get((0xBAEE, None), (0, 0, 2))[1] & 0x1F
        bm = cur.get((0xB6EE, None), (0, 0, 2))[1] & 0x1F
        if kind == 'cmd':
            key = (kind, w[t][1], cur.get((0xBAE8, None), (0, 0, 2))[1] & 0x7F,
                   cur.get((0xB6E8, None), (0, 0, 2))[1] & 0x7F, e > t + 1)
        else:
            key = (kind, dpc, fm, bm, any(w[k][0] in DATA for k in range(t + 1, e)))
        try:
            x0, y0, x1, y1 = bbox(w, s, t, e, kind, state)
        except (ValueError, IndexError):
            continue
        area = (x1 - x0 + 1) * (y1 - y0 + 1)
        if area <= 0 or e - s > 4000:  # M8CONF's buffer holds 4,000 entries plus the state
            continue
        # Prefer the largest instance up to 2,048 pixels: big enough to show edges, pattern alignment
        # and wrap, small enough to read back and run fast; above that, the smallest.
        score = area if area <= 2048 else -area
        if key not in shapes or score > shapes[key][4]:
            shapes[key] = (area, list(state.values()), w[s:e], (x0, y0, x1, y1), score)
        for p, v, n in w[s:e]:
            if p not in NOT_STATE:
                setreg(state, p, v, n)
        pos = e

with open(sys.argv[1], 'wb') as o:
    for key in sorted(shapes, key=str):
        area, st, op, (x0, y0, x1, y1), _ = shapes[key]
        ax, ay = clampxy(x0 - 2, y0 - 1)
        bx, by = clampxy(x1 - 61, y1 - 14)
        o.write(struct.pack('<6H', len(st), len(op), ax, ay, bx, by))
        for p, v, n in st + op:
            o.write(struct.pack('<3H', p, v, n))
        print('%-5s %04X fm %02X bm %02X data %d  area %6d  ops %4d  read (%d,%d) (%d,%d)' % (
            key[0], key[1], key[2], key[3], key[4], area, len(op), ax, ay, bx, by))
    o.write(struct.pack('<H', 0xFFFF))
print('%d shapes' % len(shapes))
