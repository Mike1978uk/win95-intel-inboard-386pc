#!/usr/bin/env python3
"""Decode XTWLAT.BIN: the XT-CF's write waits, as poll counts and microseconds.

    python tools/xtlat/xtwlat_decode.py XTWLAT.BIN [us_per_poll]
"""

import struct
import sys

from xtlat_decode import summary

REC = 26


def main():
    d = open(sys.argv[1], "rb").read()
    us = float(sys.argv[2]) if len(sys.argv) > 2 else 5.8
    if d[:4] != b"XTW1":
        sys.exit("not an XTWLAT.BIN")
    first, gaps, commit, flush = [], [], [], []
    flush_declined = bad = 0
    n = (len(d) - 4) // REC
    for off in range(4, 4 + n * REC, REC):
        lba = struct.unpack_from("<I", d, off)[0]
        w = struct.unpack_from("<11H", d, off + 4)
        if any(x in (0xFFFF, 0xFFFD) for x in w[:9]) or w[10] != 0:
            bad += 1
            print(f"LBA {lba}: error or mismatch {w}")
            continue
        first.append(w[0])
        gaps += list(w[1:8])
        commit.append(w[8])
        if w[9] == 0xFFFF:
            flush_declined += 1
        else:
            flush.append(w[9])
    if len(d) - 4 - n * REC:
        print(f"run stopped inside a record ({len(d) - 4 - n * REC} bytes)")
    print(f"{len(first)} commands clean and verified, {bad} bad; {us} us per poll assumed")
    summary("command to first DRQ", first, us)
    summary("between sectors", gaps, us)
    summary("commit (BSY after last)", commit, us)
    summary("FLUSH CACHE", flush, us)
    print(f"FLUSH CACHE declined (ERR) {flush_declined} times")


if __name__ == "__main__":
    main()
