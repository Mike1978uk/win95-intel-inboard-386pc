#!/usr/bin/env python3
"""32-bit PE disassembler for ANY flat i386 PE - .MPD, .PDR, .SYS, .EXE, .DLL.

Written for SD120PPD.MPD and generalised 2026-09-13, because a tool that reads
one file is not a tool. Addresses printed are RVAs: sections map at
ImageBase+VirtualAddress, and RVA is what the relocation table speaks, so it is
the address that survives the loader moving the image.

Usage:
    python tools/pedis.py <file> sections          # PROVE the sweep covers it
    python tools/pedis.py <file> imports
    python tools/pedis.py <file> io                # every in/out with its port
    python tools/pedis.py <file> str               # printable strings with RVAs
    python tools/pedis.py <file> all > out.asm
    python tools/pedis.py <file> dis <rva_hex> [count]

Run `sections` FIRST and check the sweep spans every executable section end to
end. A linear sweep stops at the first undecodable byte, so a partial dump
looks exactly like a complete one (technique 112).

NOT for LE VxDs - their `CD 20` + inline 4-byte service id desyncs a naive
disassembler; use tools/vxd_disasm.py. NOT for 16-bit DOS .SYS - start at the
strategy/interrupt offsets from the device header, and see tools/sysdis.py.
"""
import os
import struct
import sys

from capstone import Cs, CS_ARCH_X86, CS_MODE_32

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
            chars = struct.unpack_from("<I", d, o + 36)[0]
            self.sec.append((name, va, vs, ra, rs, chars))

    def off(self, rva):
        for name, va, vs, ra, rs, _c in self.sec:
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


IMAGE_SCN_CNT_CODE   = 0x00000020
IMAGE_SCN_MEM_EXECUTE = 0x20000000


def code_sections(p):
    """Executable sections, by CHARACTERISTICS rather than by name.

    The original hardcoded ('.text', 'PNP') because that is what the one file
    it was written for happened to have. Any other driver with an oddly named
    code section silently disassembled to nothing - which reads exactly like a
    clean binary.
    """
    out = [(n, va, vs, ra, rs) for n, va, vs, ra, rs, c in p.sec
           if c & (IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE)]
    if not out:                      # no flags set: fall back, but say so
        out = [(n, va, vs, ra, rs) for n, va, vs, ra, rs, c in p.sec
               if n in (".text", "PNP", "CODE")]
        if out:
            print("; WARNING: no section flagged executable; fell back to "
                  "names " + ", ".join(n for n, *_ in out), file=sys.stderr)
    return out


def sections(p):
    print(f"ImageBase {p.base:#010x}   entry rva {p.entry:#08x}")
    print(f"{'name':10s} {'rva':>10s} {'vsize':>10s} {'rawoff':>10s} "
          f"{'rawsize':>10s}  flags")
    for n, va, vs, ra, rs, c in p.sec:
        tag = []
        if c & IMAGE_SCN_CNT_CODE:    tag.append("CODE")
        if c & IMAGE_SCN_MEM_EXECUTE: tag.append("EXEC")
        print(f"{n:10s} {va:#10x} {vs:#10x} {ra:#10x} {rs:#10x}  "
              f"{c:#010x} {' '.join(tag)}")
    cov = code_sections(p)
    total = sum(min(vs, rs) for _n, _va, vs, _ra, rs in cov)
    print(f"\nthe sweep covers {len(cov)} section(s), {total} bytes "
          f"({total/1024:.1f} KB)")


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
        print(__doc__)
        return
    path = sys.argv[1]
    if not os.path.isfile(path):
        print(f"no such file: {path}\n", file=sys.stderr)
        print(__doc__)
        raise SystemExit(2)
    p = PE(path)
    cmd = sys.argv[2] if len(sys.argv) > 2 else "sections"
    if cmd == "sections":
        sections(p)
    elif cmd == "imports":
        imports(p)
    elif cmd == "io":
        io(p)
    elif cmd == "str":
        strings(p)
    elif cmd == "all":
        for name, ins in walk(p):
            print(f"{ins.address:#08x}  {ins.mnemonic} {ins.op_str}")
    elif cmd == "dis":
        dis(p, int(sys.argv[3], 16), int(sys.argv[4]) if len(sys.argv) > 4 else 40)
    else:
        print(__doc__)


if __name__ == "__main__":
    main()
