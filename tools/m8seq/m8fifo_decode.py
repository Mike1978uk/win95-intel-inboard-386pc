#!/usr/bin/env python3
"""Decode M8FIFO.BIN: command FIFO depth, full behaviour, engine drain rate.

    python m8fifo_decode.py M8FIFO.BIN
"""

import struct
import sys

TICK_US = 1e6 / 1193182
NSTEP = 24


def dt(a, b):
    """PIT counts down; one 16-bit wrap is undone, more cannot be seen."""
    return ((a - b) & 0xFFFF) * TICK_US


def fillarm(d, off, name):
    print(f"{name}:")
    print("  step   us since prev   GE_STAT  occupied  EXT_FIFO  occupied")
    prev = None
    for i in range(NSTEP + 1):
        pit, ge, ext = struct.unpack_from("<HHH", d, off + 6 * i)
        gap = "" if prev is None else f"{dt(prev, pit):10.1f}"
        print(f"  {i:4}  {gap:>14}   {ge:04X}    {bin(ge & 0xFF).count('1'):>3}       "
              f"{ext:04X}    {bin(ext).count('1'):>3}")
        prev = pit
    sub = struct.unpack_from("<H", d, off + 6 * (NSTEP + 1))[0]
    print(f"  SUBSYS_STAT after: {sub:04X}  (bit 2 INVALID_IO = {bool(sub & 4)})")
    return off + 6 * (NSTEP + 1) + 2


def main():
    d = open(sys.argv[1], "rb").read()
    base = 4 if d[:4] == b"M8F1" else 0      # a capture without the header starts at the data
    off = fillarm(d, base, "Arm A: FRGD_COLOR at A6E8h behind a 1024x512 fill")
    off = fillarm(d, off, "Arm B: the same at E6E8h (A14 set)")
    print("Drain: fill size, time from CMD to GE_BUSY clear")
    for i in range(5):
        w, h, t0, t1, polls = struct.unpack_from("<HHHHH", d, off + 10 * i)
        us = dt(t0, t1)
        rate = (w * h) / us if us else 0
        print(f"  {w:5}x{h:<4} {us:9.1f} us  {polls:5} polls  {rate:7.2f} Mpixel/s")


if __name__ == "__main__":
    main()
