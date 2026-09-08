#!/usr/bin/env python3
"""Find every 16-bit little-endian occurrence of a word value in SD120PPD.SYS.

Used to locate table entries and code references to a known address, e.g.
the mode-name strings 'ECP Read' (0x3CBA) and 'ECP Write' (0x491E).

Usage: python xref.py <word_hex> [word_hex ...]
"""
import sys
import os

DEFAULT = os.path.join(os.path.dirname(__file__), "..", "SD120PPD.SYS.orig")


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    d = open(DEFAULT, "rb").read()
    for arg in sys.argv[1:]:
        v = int(arg, 16)
        pat = bytes([v & 0xFF, (v >> 8) & 0xFF])
        print("=== %04X ===" % v)
        start = 0
        hits = 0
        while True:
            i = d.find(pat, start)
            if i < 0:
                break
            hits += 1
            print("  at %04X   ctx %s" % (i, d[max(0, i - 10):i + 10].hex(" ")))
            start = i + 1
        if not hits:
            print("  none")


if __name__ == "__main__":
    main()
