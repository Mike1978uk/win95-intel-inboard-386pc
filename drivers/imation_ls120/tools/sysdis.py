#!/usr/bin/env python3
"""16-bit disassembler for SD120PPD.SYS, with string and table annotation.

A .SYS device driver is a raw binary image loaded at offset 0 of its own
segment, so file offset == CS offset throughout.

Usage:
    python dis.py <start_hex> [count] [file]

Prints `count` instructions (default 40) from `start_hex`, annotating any
immediate or displacement that resolves to a printable string in the image.
"""
import sys
import os

from capstone import Cs, CS_ARCH_X86, CS_MODE_16

DEFAULT = os.path.join(os.path.dirname(__file__), "..", "SD120PPD.SYS.orig")


def cstr(d, off, limit=40):
    if not (0 < off < len(d)):
        return None
    out = bytearray()
    while off < len(d) and len(out) < limit:
        c = d[off]
        if c in (0x00, 0x24):
            break
        if c < 0x20 or c > 0x7E:
            return None
        out.append(c)
        off += 1
    if len(out) < 4:
        return None
    return out.decode("ascii")


def annotate(d, ins):
    notes = []
    seen = set()
    for tok in ins.op_str.replace(",", " ").replace("[", " ").replace("]", " ").split():
        tok = tok.strip("+ ")
        if not tok.startswith("0x"):
            continue
        try:
            v = int(tok, 16)
        except ValueError:
            continue
        if v in seen:
            continue
        seen.add(v)
        s = cstr(d, v)
        if s:
            notes.append("%04X=%r" % (v, s))
    return ("   ; " + "  ".join(notes)) if notes else ""


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    start = int(sys.argv[1], 16)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 40
    path = sys.argv[3] if len(sys.argv) > 3 else DEFAULT

    d = open(path, "rb").read()
    md = Cs(CS_ARCH_X86, CS_MODE_16)
    md.detail = False

    n = 0
    for ins in md.disasm(d[start:start + count * 8], start):
        print("%04X  %-20s %-32s%s"
              % (ins.address, ins.bytes.hex(" "),
                 "%s %s" % (ins.mnemonic, ins.op_str), annotate(d, ins)))
        n += 1
        if n >= count:
            break


if __name__ == "__main__":
    main()
