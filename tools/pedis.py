#!/usr/bin/env python3
"""32-bit PE disassembler for SD120PPD.MPD, the vendor's Win95 miniport.

A .MPD is a flat i386 PE. Sections are mapped at ImageBase+VirtualAddress, so
addresses printed here are RVAs (subtract nothing; the file's ImageBase is
0x10000 but SCSIPORT relocates, and RVA is what the relocation table speaks).

Usage:
    python pedis.py <file> sections            # PROVE the dump covers everything
    python pedis.py <file> imports
    python pedis.py <file> io                  # every in/out with its port
    python pedis.py <file> dis <rva_hex> [count]
    python pedis.py <file> all  > out.asm
    python pedis.py <file> str                 # printable strings with RVAs

ALWAYS run `sections` first and check that `all` spans every executable
section end to end. And remember RVA != linked address: a callback pushed as
an immediate is ImageBase + RVA.
"""
import os
import struct
import sys

from capstone import Cs, CS_ARCH_X86, CS_MODE_32

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT = os.environ.get("PEDIS_FILE", "")


class PE:
    def __init__(self, path):
        self.d = open(path, "rb").read()
        d = self.d
        pe = struct.unpack_from("<I", d, 0x3C)[0]
        nsec = struct.unpack_from("<H", d, pe + 6)[0]
        optsz = struct.unpack_from("<H", d, pe + 20)[0]
        opt = pe + 24
        self.base = struct.unpack_from("<I", d, opt + 28)[0]
        self.entry = struct.unpack_from("<I", d, opt + 16)[0]
        ndir = struct.unpack_from("<I", d, opt + 92)[0]
        self.dirs = [struct.unpack_from("<II", d, opt + 96 + 8 * i)
                     for i in range(ndir)]
        self.sec = []
        for i in range(nsec):
            o = pe + 24 + optsz + i * 40
            name = d[o:o + 8].rstrip(b"\0").decode("latin1")
            vs, va, rs, ra = struct.unpack_from("<IIII", d, o + 8)
            self.sec.append((name, va, vs, ra, rs))

    def off(self, rva):
        for name, va, vs, ra, rs in self.sec:
            if va <= rva < va + max(vs, rs):
                return ra + (rva - va)
        return None

    def read(self, rva, n):
        o = self.off(rva)
        return self.d[o:o + n] if o is not None else b""

    def u32(self, rva):
        b = self.read(rva, 4)
        return struct.unpack("<I", b)[0] if len(b) == 4 else 0

    def cstr(self, rva, limit=64):
        o = self.off(rva)
        if o is None:
            return ""
        e = self.d.find(b"\0", o, o + limit)
        return self.d[o:e if e > 0 else o + limit].decode("latin1")


def imports(p):
    rva, size = p.dirs[1]
    i = 0
    while True:
        e = p.read(rva + i * 20, 20)
        if len(e) < 20 or e == b"\0" * 20:
            break
        oft, _, _, nm, fst = struct.unpack("<IIIII", e)
        print(f"--- {p.cstr(nm)} (IAT rva {fst:#x}) ---")
        t = oft or fst
        k = 0
        while True:
            v = p.u32(t + k * 4)
            if not v:
                break
            if v & 0x80000000:
                print(f"  {fst + k * 4:#08x}  ordinal {v & 0xFFFF}")
            else:
                print(f"  {fst + k * 4:#08x}  {p.cstr(v + 2)}")
            k += 1
        i += 1


def code_sections(p):
    return [(n, va, vs, ra, rs) for n, va, vs, ra, rs in p.sec
            if n in (".text", "PNP")]


def walk(p):
    """Linear sweep that resyncs. capstone stops at the first undecodable
    byte, and .text carries jump tables and padding, so without the resync
    the sweep silently covers only the first few KB."""
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = False
    for name, va, vs, ra, rs in code_sections(p):
        code = p.d[ra:ra + min(vs, rs)]
        pos = 0
        while pos < len(code):
            n = 0
            for ins in md.disasm(code[pos:], va + pos):
                yield name, ins
                n += ins.size
            pos += n + 1 if n == 0 else n


def io(p):
    for name, ins in walk(p):
        if ins.mnemonic in ("in", "out", "insb", "insw", "insd",
                            "outsb", "outsw", "outsd", "rep insb",
                            "rep outsb"):
            print(f"{ins.address:#08x} [{name}] {ins.mnemonic} {ins.op_str}")


def strings(p):
    for name, va, vs, ra, rs in p.sec:
        blob = p.d[ra:ra + rs]
        cur, start = bytearray(), 0
        for i, c in enumerate(blob):
            if 0x20 <= c <= 0x7E:
                if not cur:
                    start = i
                cur.append(c)
            else:
                if len(cur) >= 5:
                    print(f"{va + start:#08x} [{name}] {cur.decode('latin1')}")
                cur = bytearray()


def dis(p, start, count):
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    o = p.off(start)
    code = p.d[o:o + count * 8 + 32]
    for n, ins in zip(range(count), md.disasm(code, start)):
        print(f"{ins.address:#08x}  {ins.bytes.hex():<16} {ins.mnemonic} {ins.op_str}")


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    path, args = sys.argv[1], sys.argv[2:]
    p = PE(path)
    cmd = args[0] if args else "imports"
    if cmd == "sections":
        print("ImageBase %08x" % p.base)
        for n, va, vs, ra, rs in p.sec:
            print("%-8s VA %08x vsize %6x  raw %06x size %6x" % (n, va, vs, ra, rs))
        return
    if cmd == "imports":
        imports(p)
    elif cmd == "io":
        io(p)
    elif cmd == "str":
        strings(p)
    elif cmd == "all":
        for name, ins in walk(p):
            print(f"{ins.address:#08x}  {ins.mnemonic} {ins.op_str}")
    elif cmd == "dis":
        dis(p, int(args[1], 16), int(args[2]) if len(args) > 2 else 40)
    else:
        print(__doc__)


if __name__ == "__main__":
    main()
