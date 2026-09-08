#!/usr/bin/env python3
"""Dump SD120PPD.SYS's transfer-mode dispatch tables.

The DOS driver picks a parallel-port transfer method through two tables:

    244b  READ  reg:  bx = [0BD9] * 8 ; call word ptr cs:[bx + 4E9Dh]
    2472  WRITE reg:  bx = [0BDB] * 8 ; call word ptr cs:[bx + 4F15h]

[0BD9] and [0BDB] are the selected read/write method numbers. Each table
entry is 8 bytes; the first word is the handler offset. This dumps both
tables, resolves any word that points at a printable string, and names the
ECP entries we need.

A .SYS device driver is a raw binary image, so file offset == CS offset.

Usage: python dump_mode_tables.py [path/to/SD120PPD.SYS]
"""
import sys
import os

READ_TABLE = 0x4E9D
WRITE_TABLE = 0x4F15
ENTRY = 8

DEFAULT = os.path.join(os.path.dirname(__file__), "..", "SD120PPD.SYS.orig")


def word(d, off):
    return d[off] | (d[off + 1] << 8)


def cstr(d, off, limit=48):
    """Return the NUL- or '$'-terminated printable string at off, or None."""
    if off >= len(d) or off == 0:
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
    if len(out) < 3:
        return None
    return out.decode("ascii")


def dump(d, name, base, count):
    print("\n=== %s table at %04X, %d entries of %d bytes ===" % (name, base, count, ENTRY))
    print("%-4s %-6s %-6s  %s" % ("sel", "at", "handler", "rest of entry / resolved strings"))
    for i in range(count):
        off = base + i * ENTRY
        if off + ENTRY > len(d):
            print("  entry %d runs past EOF" % i)
            break
        handler = word(d, off)
        rest = [word(d, off + 2 + 2 * k) for k in range(3)]
        notes = []
        for k, w in enumerate(rest):
            s = cstr(d, w)
            if s:
                notes.append("+%d->%04X %r" % (2 + 2 * k, w, s))
        plaus = "" if 0 < handler < len(d) else "   <-- handler out of range"
        print("%-4d %04X   %04X     %s  %s%s"
              % (i, off, handler,
                 " ".join("%04X" % w for w in rest),
                 "  ".join(notes), plaus))


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT
    d = open(path, "rb").read()
    print("%s  %d bytes" % (os.path.abspath(path), len(d)))

    gap = WRITE_TABLE - READ_TABLE
    read_count = gap // ENTRY
    print("read table -> write table gap = %d bytes = %d entries" % (gap, read_count))

    dump(d, "READ", READ_TABLE, read_count)
    dump(d, "WRITE", WRITE_TABLE, read_count)

    print("\n=== every occurrence of 'ECP' / 'EPP' / 'Nibble' / 'Byte' in the image ===")
    for tag in (b"ECP", b"EPP", b"Nibble", b"Byte", b"PS/2"):
        start = 0
        while True:
            i = d.find(tag, start)
            if i < 0:
                break
            s = cstr(d, i)
            # walk back to the real start of the string
            j = i
            while j > 0 and 0x20 <= d[j - 1] <= 0x7E:
                j -= 1
            s = cstr(d, j)
            if s:
                print("  %04X  %r" % (j, s))
            start = i + len(tag)


if __name__ == "__main__":
    main()
