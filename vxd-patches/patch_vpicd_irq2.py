#!/usr/bin/env python3
"""Deliver the XT's IRQ 2 to whoever virtualises IRQ 9 (#42).

Input is our VPICD_INBOARD.VXD (stock OSR1 VPICD with the phantom-slave port
accesses neutered by patch_vpicd.py). On a 5160 slot pin B4 is IRQ 2, and a card
set to "IRQ 9" drives it. Stock VPICD cannot deliver it:

  1. The master is programmed ICW3=04h - IR2 has a slave - so an 8259A in
     cascade mode leaves the vector cycle to a slave that does not exist.
  2. Vector 52h (IR2) is hooked to a bare RET: no EOI, so IR2 stays in service
     and blocks IRQ 3-7 behind it.
  3. IRQ 9's in-service and mask bits live in the slave byte, which goes nowhere.

The changes, all same-size:

  A. ICW3 04h -> 00h at init and at exit, so IR2 supplies its own vector.
  B. Stub-table entry 2 -> the IRQ 9 stub, so vector 52h dispatches IRQ 9.
  C. The IRQ 9 stub works on the master: in-service bit 2 of [1ACAh], mask
     written to port 21h. Its EOI path is already a specific EOI to master IR2.
  D. IRQ 9's descriptor mask word 0200h -> 0004h, so Physically_(Un)mask and
     end-of-interrupt act on master IR2.
  E. Three fixup-list entries move between the [1AC8h]/[1AC9h]/[1ACAh]/[1ACBh]
     records to match C. The block stays 126 bytes, so nothing else moves.

Known imprecision: the status services test the IRQ number's own bit, so
VPICD_Get_Complete_Status for IRQ 9 does not reflect master IR2.
"""
import struct
import sys

SRC = sys.argv[1] if len(sys.argv) > 1 else "VPICD_INBOARD.VXD"
OUT = sys.argv[2] if len(sys.argv) > 2 else "VPICD_INBOARD_IRQ2.VXD"
SRC_MD5 = "65fa4757a7f0b653ae063b7e99c8fb84"

LE = 0x80
FIX_BLOCK = 0x358   # page 1 records: [1AC8h], [1AC9h], [1ACAh], [1ACBh]
FIX_LEN = 126

# (file offset, expected, replacement, why)
BYTES = [
    (0x7083, b"\x04", b"\x00", "A  master ICW3 at init"),
    (0x14A6, b"\x04", b"\x00", "A  master ICW3 at exit"),
    (0x7516, b"\xc9\x0f", b"\xc0\x0e", "B  stub table entry 2 (page data)"),
    (0x0B96, b"\xc9\x0f", b"\xc0\x0e", "B  stub table entry 2 (fixup target)"),
    (0x1EC2, b"\xcb", b"\xca", "C  or byte [1ACBh] -> [1ACAh]"),
    (0x1EC6, b"\x02", b"\x04", "C  in-service bit 1 -> bit 2"),
    (0x1ECE, b"\xcb", b"\xca", "C  mov al,[1ACBh] -> [1ACAh]"),
    (0x1ED4, b"\xc9", b"\xc8", "C  or al,[1AC9h] -> [1AC8h]"),
    (0x1ED8, b"\x90\x90", b"\xe6\x21", "C  NOP NOP -> out 21h,al"),
    (0x2D44, b"\x00\x02", b"\x04\x00", "D  IRQ 9 mask word"),
]

# srcoff moves between fixup lists (E)
MOVES = [(0x0ED4, 0x1AC9, 0x1AC8), (0x0EC2, 0x1ACB, 0x1ACA), (0x0ECE, 0x1ACB, 0x1ACA)]


def parse_lists(d, start, length):
    """Parse consecutive 'internal, 32-bit offset, source list' records."""
    out, p = [], start
    while p < start + length:
        src, fl, cnt, obj = d[p], d[p + 1], d[p + 2], d[p + 3]
        if src != 0x27 or fl != 0x00 or obj != 1:
            raise SystemExit("unexpected fixup record at %#x" % p)
        target = struct.unpack_from("<H", d, p + 4)[0]
        offs = list(struct.unpack_from("<%dh" % cnt, d, p + 6))
        out.append([target, offs])
        p += 6 + 2 * cnt
    if p != start + length:
        raise SystemExit("fixup block does not end at %#x" % (start + length))
    return out


def build_lists(recs):
    b = b""
    for target, offs in recs:
        b += bytes([0x27, 0x00, len(offs), 0x01]) + struct.pack("<H", target)
        b += struct.pack("<%dh" % len(offs), *offs)
    return b


def main():
    import hashlib
    d = bytearray(open(SRC, "rb").read())
    if hashlib.md5(d).hexdigest() != SRC_MD5:
        raise SystemExit("%s is not VPICD_INBOARD.VXD md5 %s" % (SRC, SRC_MD5))

    for off, old, new, why in BYTES:
        if d[off:off + len(old)] != old:
            raise SystemExit("%#x: expected %s, found %s (%s)"
                             % (off, old.hex(), d[off:off + len(old)].hex(), why))
        d[off:off + len(old)] = new
        print("  %#06x %s -> %s  %s" % (off, old.hex(), new.hex(), why))

    recs = parse_lists(d, FIX_BLOCK, FIX_LEN)
    by_target = {t: offs for t, offs in recs}
    if sorted(by_target) != [0x1AC8, 0x1AC9, 0x1ACA, 0x1ACB]:
        raise SystemExit("fixup block targets are %s" % [hex(t) for t in by_target])
    for so, frm, to in MOVES:
        by_target[frm].remove(so)
        by_target[to].append(so)
        print("  fixup srcoff %#x: [%Xh] -> [%Xh]" % (so, frm, to))
    block = build_lists(recs)
    if len(block) != FIX_LEN:
        raise SystemExit("rebuilt fixup block is %d bytes, not %d" % (len(block), FIX_LEN))
    d[FIX_BLOCK:FIX_BLOCK + FIX_LEN] = block

    # Re-parse and check every moved site landed on the right target.
    check = {t: o for t, o in parse_lists(d, FIX_BLOCK, FIX_LEN)}
    for so, frm, to in MOVES:
        assert so in check[to] and so not in check[frm]

    patched = len(BYTES) + len(MOVES)
    print("Patched: %d" % patched)
    open(OUT, "wb").write(d)
    print("wrote %s  md5 %s" % (OUT, hashlib.md5(d).hexdigest()))


if __name__ == "__main__":
    main()
