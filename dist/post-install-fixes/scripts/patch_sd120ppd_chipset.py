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


# Ports this driver writes that cannot be a real register on an IBM 5160.
#   0x22/0x23 - chipset config index/data; the 8259 answers here (MEASURED:
#               0x21/0x23/0x25/0x31/0x3F all read 0xAC on the real machine).
#   0x24/0x25 - a SECOND chipset config index/data pair.  The driver does
#               `mov al,61h / out 24h / in 25h / or al,01h / out 25h`, which on
#               this machine is: write 0x61 to the PIC COMMAND register, read
#               the mask, set bit 0, write it back - masking IRQ 0, the TIMER,
#               with no restore.  A second site ORs 0x08 (IRQ 3).
#   0x94      - PS/2 system-board setup register; the driver writes 0x7F then
#               0xFF (this is what /fp and /dp suppress in the DOS build).  A
#               5160 has no such register and the DMA page block is write-only,
#               so the alias could not be measured - but the write cannot be
#               correct either way, so it goes.
# NOT touched: out 21h.  Every one of those sites is a paired save/restore
# around the probe (in al,21h / push / out / ... / pop / out), so they are
# already neutral.  Removing only one half would be worse than leaving both.
BAD_PORTS = (0x22, 0x23, 0x24, 0x25, 0x94)


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
        # map entries are "offset:opcode:port" - BOTH original bytes are recorded.
        # The patch replaces two bytes, so the revert must restore two; recording
        # them removes any guessing between the byte (E6) and word (E7) forms.
        n = 0
        for tok in open(args.map).read().split():
            parts = tok.split(":")
            o = int(parts[0], 16)
            op = int(parts[1], 16)
            port = int(parts[2], 16)
            if data[o] == 0x90 and data[o + 1] == 0x90:
                data[o] = op
                data[o + 1] = port
                n += 1
        print("Reverted: %d" % n)
        if n == 0:
            sys.exit("Reverted: 0 - nothing changed, refusing to write")
    else:
        # 0xE6 = out imm8,al (byte)   0xE7 = out imm8,eAX (word, usually with a
        # 0x66 prefix).  A word write on an 8-bit bus splits into TWO byte writes,
        # so E7 hits both registers of the pair - it matters more, not less.
        found = sites(data, 0xE6, BAD_PORTS) + sites(data, 0xE7, BAD_PORTS)
        found.sort()
        reads = sites(data, 0xE4, BAD_PORTS) + sites(data, 0xE5, BAD_PORTS)
        print("out 22/23/24/25/94h sites : %d  (removed)" % len(found))
        print("in  22/23/24/25/94h sites : %d  (left - reads are harmless)" % len(reads))
        orig = {o: (data[o], data[o + 1]) for o in found}
        n = 0
        for o in found:
            data[o] = 0x90
            data[o + 1] = 0x90
            n += 1
        print("Patched: %d" % n)
        if n == 0:
            sys.exit("Patched: 0 - nothing changed, refusing to write")
        with open((args.out or args.target) + ".sites", "w") as f:
            f.write(" ".join("%x:%02x:%02x" % (o, orig[o][0], orig[o][1]) for o in found))
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
