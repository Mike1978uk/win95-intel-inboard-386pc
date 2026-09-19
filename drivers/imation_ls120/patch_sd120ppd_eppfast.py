#!/usr/bin/env python3
"""Unlock EPP dword ("EPP Fast") transfers in Imation's SD120PPD.MPD.

The miniport defaults to byte-wide EPP and upgrades to dword only if
ScsiPortReadPortBufferUlong is licensed by a gate at rva 0x80AB:

    80AB  e8 f9 47 00 00   call 0xC8A9      -> [0x20D9D], or 0 if [0x20BD4]
    80B0  3c 02            cmp  al, 2
    80B2  75 0d            jne  keep-EPP-8  <-- PATCHED TO 90 90
    80B4  e8 34 7c 00 00   call 0xFCED      -> CPU class, 3 on the Inboard
    80B9  3c 03            cmp  al, 3
    80BB  72 04            jb   keep-EPP-8
    80BD  b6 0b            mov  dh, 0Bh     read  mode 11 = EPP dword
    80BF  b2 04            mov  dl, 4       write mode  4 = EPP dword

[0x20D9D] is set to 2 only as a by-product of a successful chipset
detection.  On this machine detection is suppressed (/de /db /ni) because
it writes 0x22/0x23, which alias onto the 8259 on an XT, so the byte stays
0, c8a9 returns 0, and dword can never be selected.  Nopping the jne drops
that one test and leaves the CPU-class test below it intact.

Touches no port and no chipset branch: with /fe the transfer routines take
the [0x20BD9] path at 0x8D81 and never reach the 0x22/0x23 writes.  If the
port cannot sustain dword the driver's own recovery ladder at 0xAEEC calls
0xABC6, latches [0x20C7C]=1 and drops back to byte-wide for the rest of the
boot -- so a wrong answer degrades rather than corrupts.
"""

import argparse
import hashlib
import os
import sys

# Anchor on the whole gate, not the two bytes: "75 0d" alone occurs all over
# a 64 KB image, and patching the wrong one is silent.
RVA = 0x80AB
STOCK = bytes.fromhex("e8f9470000" "3c02" "750d" "e8347c0000" "3c03" "7204" "b60b" "b204")
FIXED = bytes.fromhex("e8f9470000" "3c02" "9090" "e8347c0000" "3c03" "7204" "b60b" "b204")
JNE_RVA = RVA + 7  # the two bytes that actually move

# .text has rawoff == rva (0x400), so a .text rva is also its file offset.
# Asserted below rather than assumed.
TEXT_RVA, TEXT_RAWOFF, TEXT_SIZE = 0x400, 0x400, 0xFC00


def md5(b):
    return hashlib.md5(b).hexdigest()


def check_layout(d):
    if d[:2] != b"MZ":
        sys.exit("not a PE image (no MZ)")
    if not (TEXT_RVA <= RVA < TEXT_RVA + TEXT_SIZE):
        sys.exit("gate rva is outside .text")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("infile")
    ap.add_argument("outfile")
    ap.add_argument("--revert", action="store_true",
                    help="undo the patch; the result must match the stock md5")
    args = ap.parse_args()

    d = bytearray(open(args.infile, "rb").read())
    check_layout(d)

    want, make = (FIXED, STOCK) if args.revert else (STOCK, FIXED)
    verb = "revert" if args.revert else "patch"

    here = bytes(d[RVA:RVA + len(STOCK)])
    if here != want:
        if here == make:
            sys.exit("refusing to %s: already in the target state (no-op)" % verb)
        sys.exit("refusing to %s: bytes at rva 0x%05x are\n  %s\nexpected\n  %s"
                 % (verb, RVA, here.hex(" "), want.hex(" ")))

    before = md5(d)
    d[RVA:RVA + len(STOCK)] = make
    patched = sum(1 for a, b in zip(want, make) if a != b)
    if patched == 0:
        sys.exit("refusing to write: 0 bytes changed")

    open(args.outfile, "wb").write(d)
    after = md5(open(args.outfile, "rb").read())

    print("%s: %s" % (verb, args.infile))
    print("  rva 0x%05x  %s -> %s   (%s)"
          % (JNE_RVA, want[7:9].hex(" "), make[7:9].hex(" "),
             "jne -> nop nop" if not args.revert else "nop nop -> jne"))
    print("  Patched: %d byte(s)" % patched)
    print("  md5 in  %s" % before)
    print("  md5 out %s" % after)
    print("  size %d -> %d" % (len(d), os.path.getsize(args.outfile)))


if __name__ == "__main__":
    main()
