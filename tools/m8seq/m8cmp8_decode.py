#!/usr/bin/env python3
"""Decode M8CMP8.BIN: which colour-compare rule the card follows.

For each case (WRT_MASK, COLOR_CMP, mode) row 0 held x at pixel x before an EEh fill. A pixel
was written if it now reads EEh (mask FFh) or (x & F0h) | 0Eh (mask 0Fh). The rules tried:

  sense:   guide - written when the compare is FALSE (guide 8-36, R&S p. 246)
           model - written when it is TRUE, modes 2-7 (vid_8514a.c, ibm8514_accel_start)
  operands: raw (dest, cc) / both masked (dest&m, cc&m) / dest masked (dest&m, cc)

    python m8cmp8_decode.py M8CMP8.BIN
"""

import sys

CMP = {
    0: lambda d, c: False, 1: lambda d, c: True, 2: lambda d, c: d >= c, 3: lambda d, c: d < c,
    4: lambda d, c: d != c, 5: lambda d, c: d == c, 6: lambda d, c: d <= c, 7: lambda d, c: d > c,
}
OPERANDS = {
    "raw": lambda d, c, m: (d, c),
    "both&m": lambda d, c, m: (d & m, c & m),
    "dest&m": lambda d, c, m: (d & m, c),
}


def predict(sense, ops, mode, d, c, m):
    a, b = OPERANDS[ops](d, c, m)
    t = CMP[mode](a, b)
    if sense == "guide":
        return not t
    return {0: True, 1: False}.get(mode, t)


def main():
    data = open(sys.argv[1], "rb").read()
    if data[:4] != b"M8C8":
        sys.exit("not an M8CMP8.BIN")
    recs = []
    off = 4
    while off + 259 <= len(data):
        m, c, mode = data[off], data[off + 1], data[off + 2]
        recs.append((m, c, mode, data[off + 3:off + 259]))
        off += 259
    print(f"{len(recs)} cases")

    obs = []  # (m, c, mode, x, written)
    odd = 0
    for m, c, mode, px in recs:
        for x in range(256):
            g = px[x]
            if m == 0xFF:
                if x == 0xEE:
                    continue
                w = g == 0xEE
                if not w and g != x:
                    odd += 1
                    continue
            else:
                new = (x & 0xF0) | 0x0E
                if new == x:
                    continue
                if g == new:
                    w = True
                elif g == x:
                    w = False
                else:
                    odd += 1
                    continue
            obs.append((m, c, mode, x, w))
    print(f"{len(obs)} pixels judged, {odd} unexpected values")

    for sense in ("guide", "model"):
        for ops in OPERANDS:
            bad = sum(predict(sense, ops, mo, x, c, m) != w for m, c, mo, x, w in obs)
            print(f"  {sense:<6} {ops:<7} mismatches {bad}")

    print("\nper case, pixels written (of judged):")
    for m, c, mode, _ in recs:
        sel = [o for o in obs if o[:3] == (m, c, mode)]
        print(f"  mask {m:02X} cc {c:02X} mode {mode}: {sum(o[4] for o in sel):3}/{len(sel)}")


if __name__ == "__main__":
    main()
