#!/usr/bin/env python3
"""Apply patch_vpicd_buffered.py's changes to the VPICD inside a VMM32.VXD.

The monolith must already carry the #42 IRQ 2 patch (patch_vmm32_vpicd_irq2.py):
the ICW3 slots are checked for 00h. Same route as that script:

  1. patcher9x -force-w3 --vxd-convert VMM32.VXD     (W4 -> uncompressed W3)
  2. python patch_vmm32_vpicd_buffered.py VMM32.VXD  (this script, in place)
  3. patcher9x -force-w4 --vxd-convert VMM32.VXD     (back to W4)

Every original byte must match at its mapped address or nothing is written.
"""
import struct
import sys

from patch_vpicd_buffered import BYTES, CONTEXT
from patch_vmm32_vpicd_irq2 import STANDALONE_LE, STANDALONE_PAGES, vpicd_entry


def main():
    path = sys.argv[1]
    d = bytearray(open(path, "rb").read())
    le = vpicd_entry(d)
    if d[le:le + 2] != b"LE":
        raise SystemExit("VPICD entry does not point at an LE header")
    pages = struct.unpack_from("<I", d, le + 0x80)[0]
    print("VPICD LE at %#x, data pages at %#x" % (le, pages))

    def mapped(off):
        if off < STANDALONE_PAGES:
            return le + (off - STANDALONE_LE)
        return pages + (off - STANDALONE_PAGES)

    for off, hx in CONTEXT:
        b, m = bytes.fromhex(hx), mapped(off)
        if d[m:m + len(b)] != b:
            raise SystemExit("%#x (standalone %#x): ICW sequence not as expected, found %s"
                             % (m, off, d[m:m + len(b)].hex()))
    for off, old, new, why in BYTES:
        m = mapped(off)
        if d[m:m + len(old)] != old:
            raise SystemExit("%#x (standalone %#x): expected %s, found %s (%s)"
                             % (m, off, old.hex(), d[m:m + len(old)].hex(), why))
    for off, old, new, why in BYTES:
        m = mapped(off)
        d[m:m + len(old)] = new
        print("  %#08x %s -> %s  %s" % (m, old.hex(), new.hex(), why))
    print("Patched: %d" % len(BYTES))
    open(path, "wb").write(d)


if __name__ == "__main__":
    main()
