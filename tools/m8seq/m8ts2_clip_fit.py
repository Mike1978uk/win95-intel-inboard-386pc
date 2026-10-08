#!/usr/bin/env python3
"""Simulate the Mach8 LINEDRAW pre-clip status against TEST.COM's TS2 sub-tests 25-71.

    python m8ts2_clip_fit.py TEST.COM [-v]

Replays each sub-test's writes through a model of the pre-clip hardware and compares EXT_GE_STATUS (62EEh,
TEST.COM's mask BF8Fh), CUR_X/CUR_Y and PATT_INDEX with TEST.COM's expected values.

From the ATI guide: CLIP_MODE = LINEDRAW_OPT[10:9] (p. 9-23); the classification table (p. 7-10) and
pp. 9-29 to 9-34 - a trivially rejected line moves CUR to its end, an accepted line is drawn, an exception
leaves CUR, sets CLIP_OVERRUN and counts every later endpoint until a new start point (Y written from
LINEDRAW) or CLIP_MODE 0; EXT_GE_STATUS fields (p. 9-68); device space -512..1535.
From TS2's own values (no source found):
  - CLIP_FLAGS 12:9 are the last point's outcodes, 1 = outside: 12 above, 11 below, 10 left, 9 right
  - POINTS_OUTSIDE (15) is set by any point outside device space, cleared by a start point
  - CLIP_INSIDE (8): an accepted line that is not wholly beyond one scissor edge sets it (polygon mode:
    lines left of the scissor count as inside, they are clamped onto it); a rejected line clears it; an
    exception keeps it; a start point clears it only if it would itself be rejected; a bounds point
    (index 4/5) clears it; never set in CLIP_MODE 0
  - CLIP_MODE 0 drops a line with an endpoint outside device space without moving CUR
  - CUR_X/CUR_Y read back 11 bits; PATT_INDEX advances by the line's major length for each drawn line
"""
import struct
import sys

DEV_LO, DEV_HI = -512, 1535


def s16(v):
    return v - 0x10000 if v & 0x8000 else v


def s12(v):
    v &= 0xfff
    return v - 0x1000 if v & 0x800 else v


class Clip:
    def __init__(self):
        self.mode = 0
        self.poly = 0
        self.sc = {'l': -512, 't': -512, 'r': 1535, 'b': 1535}
        self.idx = 0
        self.arr = [0] * 6
        self.cur = (0, 0)
        self.oc = 0
        self.inside = 0
        self.pout = 0
        self.ovr = 0
        self.exc = False
        self.patt = 0
        self.plen = 0

    def outcode(self, p):
        x, y = p
        c = 0
        if y < self.sc['t']:
            c |= 8
        if y > self.sc['b']:
            c |= 4
        if x < self.sc['l']:
            c |= 2
        if x > self.sc['r']:
            c |= 1
        return c

    @staticmethod
    def indev(p):
        return DEV_LO <= p[0] <= DEV_HI and DEV_LO <= p[1] <= DEV_HI

    def rejected(self, a, b):
        ca, cb = self.outcode(a), self.outcode(b)
        if self.mode == 1:
            return (ca & cb) != 0
        if self.mode == 2:
            return (ca & cb & 0xd) != 0          # above, below or right; not left
        return False

    def status(self):
        return (self.pout << 15) | (self.oc << 9) | (self.inside << 8) | (self.ovr & 0xf)

    def point(self, p):
        self.oc = self.outcode(p)
        if not self.indev(p):
            self.pout = 1

    def start(self, p):
        self.pout = 0
        self.ovr = 0
        self.exc = False
        self.point(p)
        if self.mode == 0 or self.rejected(p, p):
            self.inside = 0
        self.cur = p

    def bounds_point(self, p):
        self.pout = 0
        self.ovr = 0
        self.exc = False
        self.point(p)
        self.inside = 0
        self.cur = p

    def draw(self, a, b):
        n = max(abs(b[0] - a[0]), abs(b[1] - a[1]))
        self.patt = (self.patt + n) % (self.plen + 1)
        self.cur = b

    def end(self, p):
        a = self.cur
        self.point(p)
        if self.exc:
            self.ovr += 1
            return
        if self.mode == 0:
            if self.indev(a) and self.indev(p):
                self.draw(a, p)
            self.inside = 0
            return
        if self.rejected(a, p):
            self.inside = 0
            self.cur = p
            return
        if self.indev(a) and self.indev(p):
            ca, cb = self.outcode(a), self.outcode(p)
            m = 0xd if self.mode == 2 else 0xf
            self.inside = 0 if (ca & cb & m) else 1
            self.draw(a, p)
            return
        self.exc = True
        self.ovr = 1

    def write(self, port, v):
        if port == 0xA2EE:
            self.mode = (v >> 9) & 3
            self.poly = (v >> 1) & 1
            if self.mode == 0:
                self.ovr = 0
                self.exc = False
        elif port == 0x9AEE:
            self.idx = v & 7
        elif port == 0xD2EE:
            self.plen = v & 0x1f
        elif port == 0xD6EE:
            self.patt = v & 0x1f
        elif port == 0xDAEE:
            self.sc['l'] = s12(v)
        elif port == 0xDEEE:
            self.sc['t'] = s12(v)
        elif port == 0xE2EE:
            self.sc['r'] = s12(v)
        elif port == 0xE6EE:
            self.sc['b'] = s12(v)
        elif port == 0xFEEE:
            i = self.idx
            self.arr[i] = s16(v)
            if i == 1:
                self.start((self.arr[0], self.arr[1]))
            elif i == 3:
                self.end((self.arr[2], self.arr[3]))
            elif i == 5:
                self.bounds_point((self.arr[4], self.arr[5]))
            self.idx = {1: 2, 3: 2, 5: 4}.get(i, i + 1)

    def read(self, port):
        if port == 0x62EE:
            return self.status()
        if port == 0x86E8:
            return self.cur[0] & 0x7ff
        if port == 0x82E8:
            return self.cur[1] & 0x7ff
        if port == 0xD6EE:
            return self.patt
        return None


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
                i += 2
                continue
            if (p & 0xff) == 0:
                break
            w.append(struct.unpack_from('<HH', tc, i))
            i += 4
        c = []
        i = ct - 0x100
        while True:
            p = struct.unpack_from('<H', tc, i)[0]
            if (p & 0xff) == 0:
                break
            c.append(struct.unpack_from('<HHH', tc, i))
            i += 6
        yield w, c


def main():
    tc = open(sys.argv[1], 'rb').read()
    verbose = '-v' in sys.argv
    m = Clip()
    bad = total = 0
    for s, (w, c) in enumerate(subtests(tc), 1):
        for p, v in w:
            m.write(p, v)
        if s < 25:
            continue
        for p, e, mask in c:
            got = m.read(p)
            if got is None:
                continue
            total += 1
            ok = (got & mask) == (e & mask)
            bad += not ok
            if verbose or not ok:
                print('%s sub %2d %04X exp %04X got %04X' % ('  ' if ok else '!!', s, p, e & mask, got & mask))
    print('%d of %d compares differ' % (bad, total))


if __name__ == '__main__':
    main()
