#!/usr/bin/env python3
"""Put files into a bare FAT16 volume image (a superfloppy - no partition table).

  python fat16_put.py <image> NAME.EXT=<path> [NAME.EXT=@<size>:<seed> ...]
  python fat16_put.py <image> --list

Written for the LS-120 test bed. The emulated media was formatted but EMPTY, and
a read of an empty volume returns zeros - which is also what a broken transfer
returns. A test whose passing value is the uninitialised value cannot fail, so
the media has to carry a pattern that a wrong sector or a dropped nibble breaks.

  @<size>:<seed>  generates <size> bytes of a seeded LCG pattern. Every 16-byte
                  block starts with its own offset, so a misplaced read reports
                  exactly where it landed rather than merely comparing unequal.

Operates in place with r+b. It never truncates, and it refuses to write outside
the data region.
"""
import os
import struct
import sys
import hashlib


class Fat16:
    def __init__(self, path):
        self.path = path
        self.f = open(path, "r+b")
        b = self.rd(0, 512)
        u16 = lambda o: int.from_bytes(b[o:o + 2], "little")
        self.bps = u16(11)
        self.spc = b[13]
        self.rsvd = u16(14)
        self.nfat = b[16]
        self.rootent = u16(17)
        self.spf = u16(22)
        tot = u16(19) or int.from_bytes(b[32:36], "little")
        if self.bps != 512 or b[54:59] != b"FAT16":
            raise SystemExit("not a 512-byte-sector FAT16 volume")
        self.rootsec = self.rsvd + self.nfat * self.spf
        self.rootsecs = (self.rootent * 32 + self.bps - 1) // self.bps
        self.datasec = self.rootsec + self.rootsecs
        self.clusters = (tot - self.datasec) // self.spc
        self.label = b[43:54].decode("latin1").strip()

    def rd(self, sec, n):
        self.f.seek(sec * 512)
        return self.f.read(n)

    def wr(self, sec, data):
        if sec < self.rsvd:
            raise SystemExit("refusing to write the boot sector")
        self.f.seek(sec * 512)
        self.f.write(data)

    # --- FAT -------------------------------------------------------------
    def fat(self):
        return bytearray(self.rd(self.rsvd, self.spf * 512))

    def put_fat(self, fat):
        for i in range(self.nfat):            # keep both copies identical
            self.wr(self.rsvd + i * self.spf, bytes(fat))

    def alloc(self, fat, n):
        free, out = [], []
        for c in range(2, self.clusters + 2):
            if int.from_bytes(fat[c * 2:c * 2 + 2], "little") == 0:
                free.append(c)
                if len(free) == n:
                    break
        if len(free) < n:
            raise SystemExit("not enough free clusters")
        for i, c in enumerate(free):
            nxt = 0xFFFF if i == len(free) - 1 else free[i + 1]
            fat[c * 2:c * 2 + 2] = struct.pack("<H", nxt)
            out.append(c)
        return out

    def cluster_sec(self, c):
        return self.datasec + (c - 2) * self.spc

    # --- root directory ---------------------------------------------------
    def root(self):
        return bytearray(self.rd(self.rootsec, self.rootsecs * 512))

    def put_root(self, r):
        self.wr(self.rootsec, bytes(r))

    def add(self, name, data):
        n, _, e = name.partition(".")
        ent = (n[:8].ljust(8) + e[:3].ljust(3)).upper().encode("ascii")
        fat, root = self.fat(), self.root()
        for o in range(0, len(root), 32):
            if root[o] in (0x00, 0xE5):
                break
        else:
            raise SystemExit("root directory full")
        ncl = max(1, (len(data) + self.spc * 512 - 1) // (self.spc * 512))
        chain = self.alloc(fat, ncl)
        pad = data + b"\0" * (ncl * self.spc * 512 - len(data))
        for i, c in enumerate(chain):
            self.wr(self.cluster_sec(c), pad[i * self.spc * 512:(i + 1) * self.spc * 512])
        root[o:o + 32] = ent + bytes([0x20]) + b"\0" * 10 + \
            struct.pack("<HHHI", 0x6000, 0x5A6D, chain[0], len(data))
        self.put_fat(fat)
        self.put_root(root)
        return chain[0], ncl

    def listing(self):
        root, out = self.root(), []
        for o in range(0, len(root), 32):
            e = root[o:o + 32]
            if e[0] in (0x00,):
                break
            if e[0] == 0xE5 or e[11] & 0x08:
                continue
            nm = e[0:8].decode("latin1").strip()
            ex = e[8:11].decode("latin1").strip()
            out.append((nm + ("." + ex if ex else ""),
                        int.from_bytes(e[26:28], "little"),
                        int.from_bytes(e[28:32], "little")))
        return out


def pattern(size, seed):
    """Seeded LCG, with each 16-byte block stamped with its own offset."""
    out = bytearray()
    x = seed & 0xFFFFFFFF
    while len(out) < size:
        off = len(out)
        out += struct.pack("<I", off)
        for _ in range(3):
            x = (1103515245 * x + 12345) & 0xFFFFFFFF
            out += struct.pack("<I", x)
    return bytes(out[:size])


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    img = sys.argv[1]
    v = Fat16(img)
    print(f"{img}: FAT16 label={v.label!r} spc={v.spc} clusters={v.clusters} data@{v.datasec}")
    if sys.argv[2] == "--list":
        for n, c, s in v.listing():
            print(f"  {n:14s} clus {c:6d}  {s:10d} bytes")
        return
    for spec in sys.argv[2:]:
        name, _, src = spec.partition("=")
        if src.startswith("@"):
            size, _, seed = src[1:].partition(":")
            data = pattern(int(size), int(seed or 1))
        else:
            data = open(src, "rb").read()
        c, n = v.add(name, data)
        print(f"  + {name:14s} {len(data):10d} bytes  {n} clusters @ {c}  "
              f"md5 {hashlib.md5(data).hexdigest()[:12]}")


if __name__ == "__main__":
    main()
