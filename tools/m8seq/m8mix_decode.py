#!/usr/bin/env python3
"""Decode M8MIX.BIN: which function each FRGD_MIX code computes, s = FRGD_COLOR, d = the row's 00-FFh.

    python tools/m8seq/m8mix_decode.py M8MIX.BIN [OTHER.BIN]

With a second file (bed vs card), also prints the cases where the two differ.
"""

import sys

M = 0xFF
CAND = {
    "0": lambda s, d: 0,
    "1": lambda s, d: M,
    "s": lambda s, d: s,
    "d": lambda s, d: d,
    "~s": lambda s, d: ~s & M,
    "~d": lambda s, d: ~d & M,
    "s^d": lambda s, d: s ^ d,
    "s&d": lambda s, d: s & d,
    "s|d": lambda s, d: s | d,
    "~(s^d)": lambda s, d: ~(s ^ d) & M,
    "~s&d": lambda s, d: ~s & d & M,
    "s&~d": lambda s, d: s & ~d & M,
    "~s|d": lambda s, d: (~s | d) & M,
    "s|~d": lambda s, d: (s | ~d) & M,
    "~(s&d)": lambda s, d: ~(s & d) & M,
    "~(s|d)": lambda s, d: ~(s | d) & M,
    "min": lambda s, d: min(s, d),
    "max": lambda s, d: max(s, d),
    "d-s": lambda s, d: (d - s) & M,
    "s-d": lambda s, d: (s - d) & M,
    "s+d": lambda s, d: (s + d) & M,
    "(d-s)>>1 c": lambda s, d: ((d - s) & 0x1FF) >> 1,
    "(s-d)>>1 c": lambda s, d: ((s - d) & 0x1FF) >> 1,
    "(s+d)>>1 c": lambda s, d: (s + d) >> 1,
    "(d-s)>>1": lambda s, d: ((d - s) & M) >> 1,
    "(s-d)>>1": lambda s, d: ((s - d) & M) >> 1,
    "(s+d)>>1 8b": lambda s, d: ((s + d) & M) >> 1,
    "sat(d-s)": lambda s, d: max(d - s, 0),
    "sat(s-d)": lambda s, d: max(s - d, 0),
    "sat(s+d)": lambda s, d: min(s + d, M),
    "sat(d-s)>>1": lambda s, d: max(d - s, 0) >> 1,
    "sat(s-d)>>1": lambda s, d: max(s - d, 0) >> 1,
    "sat(s+d)>>1": lambda s, d: min(s + d, M) >> 1,
}
REC = 2 + 256


def load(path):
    d = open(path, "rb").read()
    if d[:4] != b"M8MX":
        sys.exit(f"{path}: not an M8MIX.BIN")
    out = {}
    for off in range(4, len(d) - REC + 1, REC):
        out[(d[off], d[off + 1])] = d[off + 2:off + REC]
    return out


def fits(s, row):
    return [n for n, f in CAND.items() if all(f(s, x) == row[x] for x in range(256))]


def main():
    a = load(sys.argv[1])
    b = load(sys.argv[2]) if len(sys.argv) > 2 else None
    for (mix, s), row in sorted(a.items()):
        f = fits(s, row) or ["?  e.g. d=00,7F,80,FF -> %02X %02X %02X %02X" % (row[0], row[0x7F], row[0x80], row[0xFF])]
        line = f"mix {mix:02X} s={s:02X}: {', '.join(f)}"
        if b is not None and (mix, s) in b:
            diff = sum(1 for x in range(256) if row[x] != b[(mix, s)][x])
            line += f"   {'= other' if not diff else f'{diff} pixels differ from other'}"
        print(line)


if __name__ == "__main__":
    main()
