"""Full-sweep disassembler for a raw DOS device driver (.SYS, no MZ).

Capstone stops at the first undecodable byte. On a driver with data
interleaved in the code that happens within a few hundred bytes, and the
result is a dump that LOOKS complete - the trap recorded at the head of
drivers/imation_ls120/SD120PPD_SYS.asm, where an earlier dump covered 21%
of the file and nobody noticed.

So: decode, and on failure emit one `db` and advance a single byte, which
resynchronises. Never stop before the end.

    python tools/dosdrv_disasm.py BPCDDRV.SYS --out BPCDDRV.asm
    python tools/dosdrv_disasm.py BPCDDRV.SYS --io
"""
import argparse
import collections
import struct
import sys

from capstone import Cs, CS_ARCH_X86, CS_MODE_16


def sweep(data, base=0):
    md = Cs(CS_ARCH_X86, CS_MODE_16)
    out = []
    pos = 0
    n = len(data)
    while pos < n:
        got = False
        for ins in md.disasm(data[pos:], base + pos):
            out.append((ins.address, ins.bytes, ins.mnemonic, ins.op_str))
            pos += ins.size
            got = True
            if pos >= n:
                break
        if not got:
            out.append((base + pos, data[pos:pos + 1], "db", "0x%02x" % data[pos]))
            pos += 1
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("path")
    ap.add_argument("--out")
    ap.add_argument("--io", action="store_true")
    a = ap.parse_args()

    d = open(a.path, "rb").read()
    nxt, attr, strat, intr = struct.unpack_from("<IHHH", d, 0)
    listing = sweep(d)
    decoded = sum(1 for x in listing if x[2] != "db")
    print("%s: %d bytes, %d instructions, %d db, strategy %04X interrupt %04X"
          % (a.path.split("\\")[-1], len(d), decoded,
             len(listing) - decoded, strat, intr))

    if a.out:
        with open(a.out, "w", encoding="ascii", errors="replace", newline="\n") as f:
            f.write("; %s - FULL sweep, %d instructions over all %d bytes.\n"
                    "; strategy %04X, interrupt %04X, attributes %04X\n\n"
                    % (a.path.split("\\")[-1], decoded, len(d), strat, intr, attr))
            for addr, raw, mn, ops in listing:
                f.write("%06x  %-20s %s %s\n" % (addr, raw.hex(), mn, ops))
        print("wrote %s" % a.out)

    if a.io:
        io = collections.Counter()
        sites = collections.defaultdict(list)
        for addr, raw, mn, ops in listing:
            if mn in ("in", "out"):
                io[mn + " " + ops] += 1
                sites[mn + " " + ops].append(addr)
        print("\ndecoded I/O instructions: %d" % sum(io.values()))
        for k, v in io.most_common(16):
            first = ", ".join("%04X" % s for s in sites[k][:4])
            print("   %-20s %4d   first: %s" % (k, v, first))


if __name__ == "__main__":
    main()
