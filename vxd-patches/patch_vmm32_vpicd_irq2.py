#!/usr/bin/env python3
"""Apply the #42 VPICD IRQ 2 patch inside an already-combined VMM32.VXD.

A post-monolith install (the real 5160's CF) cannot take a new VPICD.VXD from
WINDOWS\\SYSTEM\\VMM32 - a bundled VxD added after the combine is not read. This
applies patch_vpicd_irq2.py's changes to the VPICD image inside the monolith.

  1. patcher9x -force-w3 --vxd-convert VMM32.VXD     (W4 -> uncompressed W3)
  2. python patch_vmm32_vpicd_irq2.py VMM32.VXD      (this script, in place)
  3. patcher9x -force-w4 --vxd-convert VMM32.VXD     (back to W4)

Inside a W3 file each bundled VxD is an LE image: header and fixup tables at the
directory entry's offset, page data at that LE header's own data-pages offset. The
byte list is patch_vpicd_irq2.py's, mapped from standalone-file offsets. Every
original byte must match at its mapped address or nothing is written.
"""
import struct
import sys

from patch_vpicd_irq2 import BYTES, FIX_BLOCK, FIX_LEN, MOVES, build_lists, parse_lists

STANDALONE_LE = 0x80        # LE header offset in VPICD_INBOARD.VXD
STANDALONE_PAGES = 0x1000   # its data-pages offset
PAGE = 0x1000


def vpicd_entry(d):
    e = struct.unpack_from("<I", d, 0x3C)[0]
    if d[e:e + 2] != b"W3":
        raise SystemExit("not a W3 file - convert with patcher9x -force-w3 first")
    count = struct.unpack_from("<H", d, e + 4)[0]
    for i in range(count):
        name, off, hsize = struct.unpack_from("<8sII", d, e + 16 + 16 * i)
        if name.rstrip(b" \0") == b"VPICD":
            return off
    raise SystemExit("no VPICD in this W3 directory")


def main():
    path = sys.argv[1]
    d = bytearray(open(path, "rb").read())
    le = vpicd_entry(d)
    if d[le:le + 2] != b"LE":
        raise SystemExit("VPICD entry does not point at an LE header")
    pages = struct.unpack_from("<I", d, le + 0x80)[0]   # data-pages offset, absolute
    print("VPICD LE at %#x, data pages at %#x" % (le, pages))

    def mapped(off):
        if off < STANDALONE_PAGES:
            return le + (off - STANDALONE_LE)
        return pages + (off - STANDALONE_PAGES)

    for off, old, new, why in BYTES:
        m = mapped(off)
        if d[m:m + len(old)] != old:
            raise SystemExit("%#x (standalone %#x): expected %s, found %s (%s)"
                             % (m, off, old.hex(), d[m:m + len(old)].hex(), why))
    fb = mapped(FIX_BLOCK)
    recs = parse_lists(d, fb, FIX_LEN)

    for off, old, new, why in BYTES:
        m = mapped(off)
        d[m:m + len(old)] = new
        print("  %#08x %s -> %s  %s" % (m, old.hex(), new.hex(), why))
    by_target = {t: offs for t, offs in recs}
    for so, frm, to in MOVES:
        by_target[frm].remove(so)
        by_target[to].append(so)
    block = build_lists(recs)
    assert len(block) == FIX_LEN
    d[fb:fb + FIX_LEN] = block
    check = {t: o for t, o in parse_lists(d, fb, FIX_LEN)}
    for so, frm, to in MOVES:
        assert so in check[to] and so not in check[frm]
    print("  fixup block at %#x rebuilt" % fb)

    print("Patched: %d" % (len(BYTES) + len(MOVES)))
    open(path, "wb").write(d)


if __name__ == "__main__":
    main()
