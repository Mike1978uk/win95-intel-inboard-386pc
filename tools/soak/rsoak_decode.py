#!/usr/bin/env python3
"""Decode RSOAK.BIN: did any static planar block change at refresh divisor 64?

    python tools/soak/rsoak_decode.py RSOAK.BIN
"""

import sys


def blocks(mask):
    return [f"{i:02X}" for i, v in enumerate(mask) if v]


def main():
    d = open(sys.argv[1], "rb").read()
    if d[:4] != b"RSK1" or len(d) < 204:
        sys.exit("not an RSOAK.BIN")
    static = d[4:68]
    print(f"static 1 KB blocks: {sum(static)}/64  ({' '.join(blocks(static))})")
    for name, off in (("divisor 18 (control)", 68), ("divisor 64 (test)", 136)):
        checks = d[off] | d[off + 1] << 8
        events = d[off + 2] | d[off + 3] << 8
        chg = d[off + 4:off + 68]
        print(f"{name}: {checks} checks, {events} changes, blocks {' '.join(blocks(chg)) or '-'}")
    c18, c64 = set(blocks(d[72:136])), set(blocks(d[140:204]))
    only64 = sorted(c64 - c18)
    print("changed only at divisor 64:", " ".join(only64) or "none")


if __name__ == "__main__":
    main()
