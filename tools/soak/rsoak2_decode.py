#!/usr/bin/env python3
"""Decode RSOAK2.BIN: planar, conventional and XMS memory at refresh divisor 18 and 64.

    python tools/soak/rsoak2_decode.py RSOAK2.BIN
"""

import struct
import sys


def main():
    d = open(sys.argv[1], "rb").read()
    if d[:4] != b"RSK2":
        sys.exit("not an RSOAK2.BIN")
    convkb, xmskb = struct.unpack_from("<HH", d, 4)
    static = d[8:72]
    print(f"conventional tested {convkb} KB, XMS tested {xmskb} KB, "
          f"planar static blocks {sum(static)}/64")
    off = 72
    for name in ("divisor 18 (control)", "divisor 64 (test)"):
        checks, pev, conv, xms = struct.unpack_from("<HHII", d, off)
        chg = [f"{i:02X}" for i, v in enumerate(d[off + 12:off + 76]) if v]
        print(f"{name}: {checks} checks, planar changes {pev} {' '.join(chg) or ''}, "
              f"conventional word errors {conv}, XMS word errors {xms}")
        off += 76


if __name__ == "__main__":
    main()
