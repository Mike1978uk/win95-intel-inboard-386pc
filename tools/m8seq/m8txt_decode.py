#!/usr/bin/env python3
"""Decode M8TXT.BIN: for each command, which CPU data bit each pixel came from.

    python m8txt_decode.py M8TXT.BIN [OTHER.BIN]

Prints, per command, the 64x32 area as a map: '..' not drawn, otherwise the bit index g (hex,
00-7F) the pixel took, worked out from runs 0-6. With a second file, prints only whether the
two machines agree and where they first differ.
"""

import struct
import sys
from pathlib import Path

W, H, NRUN = 64, 32, 9
REC = 8 + W * H


def load(p):
    d = Path(p).read_bytes()
    if d[:4] != b"M8T1":
        raise SystemExit("%s: not an M8TXT result" % p)
    n = struct.unpack_from("<H", d, 4)[0]
    out = {}
    for k in range(n):
        r = d[6 + k * REC:6 + (k + 1) * REC]
        cmd, run, ti, td = struct.unpack_from("<HBBB", r, 0)
        px = bytearray(r[8:])
        # 53B0h reads with BYTE_SEQ: the first pixel of each word is in the high byte
        for i in range(0, len(px), 2):
            px[i], px[i + 1] = px[i + 1], px[i]
        out.setdefault(cmd, {})[run] = (bytes(px), ti, td)
    return out


def bitmap(runs):
    cov = runs[7][0]
    m = []
    for i in range(W * H):
        if cov[i] == 0:
            m.append(None)
            continue
        g = 0
        for r in range(7):
            if runs[r][0][i] == 0x0F:
                g |= 1 << r
        m.append(g)
    return m


def show(cmd, runs):
    t = [(r, runs[r][1], runs[r][2]) for r in range(NRUN) if runs[r][1] or runs[r][2]]
    print("CMD %04X  timeouts (run, idle, data): %s" % (cmd, t or "none"))
    m = bitmap(runs)
    odd = {runs[7][0][i] for i in range(W * H)} - {0, 0x0F}
    if odd:
        print("  run 7 holds values other than 00/0F: %s" % sorted(odd))
    rows = [y for y in range(H) if any(m[y * W + x] is not None for x in range(W))]
    cols = [x for x in range(W) if any(m[y * W + x] is not None for y in range(H))]
    if not rows:
        print("  nothing drawn")
        return
    print("  x %02X-%02X, y %02X-%02X (start 40,48)" % (cols[0] + 0x20, cols[-1] + 0x20,
                                                        rows[0] + 0x38, rows[-1] + 0x38))
    for y in rows:
        print("  %02X " % (y + 0x38) + " ".join(
            ".." if m[y * W + x] is None else "%02X" % m[y * W + x] for x in range(cols[0], cols[-1] + 1)))


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    a = load(sys.argv[1])
    if len(sys.argv) == 2:
        for cmd in a:
            show(cmd, a[cmd])
        return
    b = load(sys.argv[2])
    for cmd in a:
        ma, mb = bitmap(a[cmd]), bitmap(b[cmd])
        diff = [i for i in range(W * H) if ma[i] != mb[i]]
        print("CMD %04X: %s" % (cmd, "same" if not diff else "%d pixels differ, first at x %02X y %02X"
                                % (len(diff), diff[0] % W + 0x20, diff[0] // W + 0x38)))


if __name__ == "__main__":
    main()
