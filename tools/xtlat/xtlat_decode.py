#!/usr/bin/env python3
"""Decode XTLAT.BIN: the XT-CF's wait times, as poll counts and microseconds.

    python tools/xtlat/xtlat_decode.py XTLAT.BIN [us_per_poll]
"""

import statistics
import struct
import sys


def summary(name, vals, us):
    if not vals:
        print(f"{name:<26} none")
        return
    v = sorted(vals)
    pct = lambda p: v[min(len(v) - 1, int(p * len(v)))]
    print(f"{name:<26} n={len(v):4}  min {v[0] * us:7.0f}  p10 {pct(.1) * us:7.0f}  "
          f"median {statistics.median(v) * us:7.0f}  p90 {pct(.9) * us:7.0f}  max {v[-1] * us:7.0f} us"
          f"   (polls {v[0]}-{v[-1]})")


def main():
    d = open(sys.argv[1], "rb").read()
    us = float(sys.argv[2]) if len(sys.argv) > 2 else 5.8
    if d[:4] != b"XTL1":
        sys.exit("not an XTLAT.BIN")
    first, gaps, last, errs = [], [], [], 0
    for off in range(4, len(d) - 21, 22):
        lba = struct.unpack_from("<I", d, off)[0]
        w = struct.unpack_from("<9H", d, off + 4)
        if 0xFFFF in w:
            errs += 1
            continue
        first.append(w[0])
        gaps += list(w[1:8])
        last.append(w[8])
    print(f"{len(first)} commands clean, {errs} with ERR; {us} us per poll assumed")
    summary("command to first sector", first, us)
    summary("between sectors", gaps, us)
    summary("after last sector (BSY)", last, us)


if __name__ == "__main__":
    main()
