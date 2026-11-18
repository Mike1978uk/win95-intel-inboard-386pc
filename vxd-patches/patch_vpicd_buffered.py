#!/usr/bin/env python3
"""Program the 8259 as Intel's IBVPICD.386 does: single mode, buffered (#10).

Input is VPICD_INBOARD_IRQ2.VXD. Its master init and exit sequences write
ICW1 11h (cascade), ICW2, ICW3 00h, ICW4 01h (non-buffered). IBVPICD and the
IBM BIOS write ICW1 13h (single, so no ICW3 follows) and ICW4 09h (8086,
buffered). In buffered mode the 8259's SP/EN pin is an output; non-buffered
makes it an input, which on a 5150 may let the vector byte collide on the bus
(docs/issue10_ibvpicd_2026_11_18.md, H1a/H1b).

In single mode the 8259 takes the byte after ICW2 as ICW4, so the ICW3 slot
carries 09h and the old ICW4 'out 21h' becomes two NOPs; 'mov al,1' before it
is left dead. Init then writes 13h 50h 09h, exit 13h 08h 09h. These are the
only ICW1 writes in the file; the A0h/A1h copies after them are already NOPed.

  python patch_vpicd_buffered.py [VPICD_INBOARD_IRQ2.VXD] [VPICD_INBOARD_BUF.VXD]
"""
import hashlib
import sys

SRC_MD5 = "6c304f8cd496592e0bb92ad1b480f235"

# (file offset, expected, replacement, why)
BYTES = [
    (0x7077, b"\x11", b"\x13", "init ICW1 11h -> 13h (single, ICW4 needed)"),
    (0x7083, b"\x00", b"\x09", "init ICW3 slot -> ICW4 09h (buffered)"),
    (0x708A, b"\xe6\x21", b"\x90\x90", "init old ICW4 out 21h -> NOP NOP"),
    (0x149A, b"\x11", b"\x13", "exit ICW1 11h -> 13h"),
    (0x14A6, b"\x00", b"\x09", "exit ICW3 slot -> ICW4 09h"),
    (0x14AD, b"\xe6\x21", b"\x90\x90", "exit old ICW4 out 21h -> NOP NOP"),
]

# The instruction runs around each change, checked whole so a different build
# of the file cannot be patched by coincidence.
CONTEXT = [
    (0x7076, "b011e620eb00b050e621eb00b000e621eb00b001e621eb00b06be620"),
    (0x1499, "b011e620eb00b008e621eb00b000e621eb00b001e621eb00b04ae620"),
]


def check(d):
    for off, old, new, why in BYTES:
        if d[off:off + len(old)] != old:
            raise SystemExit("%#x: expected %s, found %s (%s)"
                             % (off, old.hex(), d[off:off + len(old)].hex(), why))


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "VPICD_INBOARD_IRQ2.VXD"
    out = sys.argv[2] if len(sys.argv) > 2 else "VPICD_INBOARD_BUF.VXD"
    d = bytearray(open(src, "rb").read())
    if hashlib.md5(d).hexdigest() != SRC_MD5:
        raise SystemExit("input md5 is not VPICD_INBOARD_IRQ2.VXD's")
    for off, hx in CONTEXT:
        b = bytes.fromhex(hx)
        if d[off:off + len(b)] != b:
            raise SystemExit("%#x: ICW sequence not as expected" % off)
    check(d)
    for off, old, new, why in BYTES:
        d[off:off + len(old)] = new
        print("  %#06x %s -> %s  %s" % (off, old.hex(), new.hex(), why))
    open(out, "wb").write(d)
    print("Patched: %d" % len(BYTES))
    print("%s md5 %s" % (out, hashlib.md5(d).hexdigest()))


if __name__ == "__main__":
    main()
