#!/usr/bin/env python3
"""Neutralise SD120PPD.MPD's chipset writes to ports 0x22/0x23.

Why: on an IBM 5160 the 8259 is decoded across 0x20-0x3F, so 0x22 aliases to the
PIC command register and 0x23 to the PIC mask.  The miniport probes for a chipset
at 0x22/0x23, the alias makes the probe succeed against nothing, and the follow-up
configuration writes land on the interrupt mask - leaving IRQ 1 (keyboard) masked
with no restore.  See docs/ls120_keyboard_root_cause.md and issue #22.

The DOS build of the same driver has /ni ("Skip chipset initialization").  The
miniport has no equivalent, so the writes are removed instead.

What this does: replaces every `out 22h,al` / `out 23h,al` (E6 22 / E6 23) with two
NOPs.  Control flow and instruction lengths are unchanged.  Reads (E4 22 / E4 23)
are left alone - reading the 8259 has no side effect, and leaving them means the
detection simply fails to find a chipset, which is what /ni achieves.

Usage:  patch_sd120ppd_chipset.py SD120PPD.MPD [-o OUT] [--revert]
"""
import argparse, hashlib, shutil, sys

ORIG_MD5 = "08104ffb559ae4b47b84377daee473bc"   # 79,872 bytes, dated 1997-05-26
TEXT_LO, TEXT_HI = 0x400, 0x10000               # .text: raw 0x400, size 0xfc00


def sites(data, first, second_set):
    return [i for i in range(TEXT_LO, min(TEXT_HI, len(data)) - 1)
            if data[i] == first and data[i + 1] in second_set]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("target")
    ap.add_argument("-o", "--out")
    ap.add_argument("--revert", action="store_true",
                    help="turn the NOP pairs back into out 22h/23h (needs --map)")
    ap.add_argument("--map", help="site list written by a previous run")
    args = ap.parse_args()

    data = bytearray(open(args.target, "rb").read())
    md5 = hashlib.md5(data).hexdigest()
    print("input : %s  %d bytes  md5 %s" % (args.target, len(data), md5))
    if md5 != ORIG_MD5:
        print("NOTE  : md5 does not match the original 1997-05-26 build.")
        print("        Continuing, but verify this is the file you meant.")

    if args.revert:
        if not args.map:
            sys.exit("--revert needs --map from the original run")
        offs = [int(x, 16) for x in open(args.map).read().split()]
        n = 0
        for o in offs:
            if data[o] == 0x90 and data[o + 1] == 0x90:
                data[o] = 0xE6
                n += 1
        print("Reverted: %d" % n)
        if n == 0:
            sys.exit("Reverted: 0 - nothing changed, refusing to write")
    else:
        found = sites(data, 0xE6, (0x22, 0x23))
        reads = sites(data, 0xE4, (0x22, 0x23))
        print("out 22h/23h sites : %d  (these are removed)" % len(found))
        print("in  22h/23h sites : %d  (left alone - reads are harmless)" % len(reads))
        n = 0
        for o in found:
            data[o] = 0x90
            data[o + 1] = 0x90
            n += 1
        print("Patched: %d" % n)
        if n == 0:
            sys.exit("Patched: 0 - nothing changed, refusing to write")
        with open((args.out or args.target) + ".sites", "w") as f:
            f.write(" ".join("%x" % o for o in found))
        print("site map written to %s.sites (needed for --revert)"
              % (args.out or args.target))

    out = args.out or args.target
    if out == args.target:
        shutil.copyfile(args.target, args.target + ".orig")
        print("backup: %s.orig" % args.target)
    open(out, "wb").write(data)
    print("output: %s  md5 %s" % (out, hashlib.md5(data).hexdigest()))


if __name__ == "__main__":
    main()
