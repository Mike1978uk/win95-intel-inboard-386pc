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

# Anchor on the whole gate, not the two bytes: a bare "75 0d" occurs all over a
# 64 KB image and patching the wrong one is silent.
#
# The 32-bit miniport and the 16-bit DOS driver carry the SAME gate from the
# same source -- both end "3c 03 72 xx b6 0b b2 04", read mode 11 / write mode 4.
# Only the call encodings and the jne displacement differ.
TARGETS = [
    dict(name="SD120PPD.MPD (Windows miniport, 32-bit)",
         off=0x80AB, jne=7,
         stock=bytes.fromhex("e8f9470000" "3c02" "750d" "e8347c0000" "3c03" "7204" "b60b" "b204"),
         fixed=bytes.fromhex("e8f9470000" "3c02" "9090" "e8347c0000" "3c03" "7204" "b60b" "b204")),
    dict(name="SD120PPD.SYS (DOS driver, 16-bit)",
         off=0x87D5, jne=5,
         stock=bytes.fromhex("e84e1d" "3c02" "750b" "e80146" "3c03" "7204" "b60b" "b204"),
         fixed=bytes.fromhex("e84e1d" "3c02" "9090" "e80146" "3c03" "7204" "b60b" "b204")),
]


def md5(b):
    return hashlib.md5(b).hexdigest()


def find_target(d, revert):
    """Identify the file by which gate it actually contains, not by its name."""
    hits = []
    for t in TARGETS:
        here = bytes(d[t["off"]:t["off"] + len(t["stock"])])
        if here in (t["stock"], t["fixed"]):
            hits.append((t, here))
    if not hits:
        sys.exit("no known EPP-width gate found -- not a driver this tool handles,\n"
                 "or a different build. Nothing written.")
    if len(hits) > 1:
        sys.exit("ambiguous: matched %d gates" % len(hits))
    return hits[0]


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("infile")
    ap.add_argument("outfile")
    ap.add_argument("--revert", action="store_true",
                    help="undo the patch; the result must match the stock md5")
    args = ap.parse_args()

    d = bytearray(open(args.infile, "rb").read())
    t, here = find_target(d, args.revert)

    want, make = (t["fixed"], t["stock"]) if args.revert else (t["stock"], t["fixed"])
    verb = "revert" if args.revert else "patch"
    off, j = t["off"], t["jne"]

    if here != want:
        sys.exit("refusing to %s: already in the target state (no-op)" % verb)

    before = md5(d)
    d[off:off + len(want)] = make
    patched = sum(1 for a, b in zip(want, make) if a != b)
    if patched == 0:
        sys.exit("refusing to write: 0 bytes changed")

    open(args.outfile, "wb").write(d)
    after = md5(open(args.outfile, "rb").read())

    print("%s: %s" % (verb, args.infile))
    print("  identified as %s" % t["name"])
    print("  offset 0x%05x  %s -> %s   (%s)"
          % (off + j, want[j:j + 2].hex(" "), make[j:j + 2].hex(" "),
             "nop nop -> jne" if args.revert else "jne -> nop nop"))
    print("  Patched: %d byte(s)" % patched)
    print("  md5 in  %s" % before)
    print("  md5 out %s" % after)
    print("  size %d -> %d" % (len(d), os.path.getsize(args.outfile)))


if __name__ == "__main__":
    main()
