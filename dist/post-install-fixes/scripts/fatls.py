#!/usr/bin/env python3
r"""List or extract files from a raw FAT12/16 disk image (MBR + one partition).

Written 2026-08-23 because the CF card is usually in the 5160 or in a running
emulator, and the disk images are the only copy reachable from the host. Read-only
by design - it never writes to the image.

  python tools/fatls.py <image> [path]            list a directory (default C:\)
  python tools/fatls.py <image> --find NAME       find a file anywhere on the volume
  python tools/fatls.py <image> --get PATH OUT    extract one file
"""
import struct, sys, os

BS = chr(92)

class Fat:
    def __init__(self, path):
        self.f = open(path, 'rb')
        mbr = self.f.read(512)
        if struct.unpack_from('<H', mbr, 510)[0] != 0xAA55:
            raise SystemExit("no MBR signature")
        plba = struct.unpack_from('<I', mbr, 0x1BE + 8)[0]
        self.base = plba * 512
        self.f.seek(self.base); b = self.f.read(512)
        self.bps  = struct.unpack_from('<H', b, 11)[0]
        self.spc  = b[13]
        rsvd      = struct.unpack_from('<H', b, 14)[0]
        nfat      = b[16]
        self.rootent = struct.unpack_from('<H', b, 17)[0]
        spf       = struct.unpack_from('<H', b, 22)[0]
        self.fat_start  = self.base + rsvd * self.bps
        self.root_start = self.fat_start + nfat * spf * self.bps
        self.data_start = self.root_start + ((self.rootent * 32 + self.bps - 1) // self.bps) * self.bps
        self.f.seek(self.fat_start); self.fat = self.f.read(spf * self.bps)
        self.csize = self.spc * self.bps

    def nxt(self, c):   return struct.unpack_from('<H', self.fat, c * 2)[0]
    def coff(self, c):  return self.data_start + (c - 2) * self.csize

    def _ents(self, off, n):
        self.f.seek(off); d = self.f.read(n * 32); out = []
        for i in range(n):
            e = d[i*32:i*32+32]
            if e[0] == 0x00: break
            if e[0] == 0xE5 or e[11] == 0x0F: continue
            nm = e[0:8].decode('cp437').rstrip(); ex = e[8:11].decode('cp437').rstrip()
            out.append({'name': nm + ("." + ex if ex else ""), 'attr': e[11],
                        'clus': struct.unpack_from('<H', e, 26)[0],
                        'size': struct.unpack_from('<I', e, 28)[0],
                        'dir':  bool(e[11] & 0x10)})
        return out

    def listdir(self, clus):
        if clus == 0: return self._ents(self.root_start, self.rootent)
        out = []; c = clus
        while 2 <= c < 0xFFF8:
            out += self._ents(self.coff(c), self.csize // 32); c = self.nxt(c)
        return out

    def resolve(self, path):
        parts = [p for p in path.replace('/', BS).strip(BS).split(BS) if p and p != 'C:']
        clus, ent = 0, None
        for part in parts:
            hit = [e for e in self.listdir(clus) if e['name'].upper() == part.upper()]
            if not hit: return None
            ent = hit[0]; clus = ent['clus']
        return ent

    def read(self, ent):
        b = bytearray(); c = ent['clus']
        while 2 <= c < 0xFFF8 and len(b) < ent['size']:
            self.f.seek(self.coff(c)); b += self.f.read(self.csize); c = self.nxt(c)
        return bytes(b[:ent['size']])

    def walk(self, clus=0, path="C:", depth=0):
        if depth > 6: return
        for e in self.listdir(clus):
            if e['name'] in ('.', '..'): continue
            full = path + "\\" + e['name']
            yield full, e
            if e['dir'] and e['clus'] >= 2:
                yield from self.walk(e['clus'], full, depth + 1)

def main():
    if len(sys.argv) < 2: raise SystemExit(__doc__)
    img = sys.argv[1]; fat = Fat(img)
    if len(sys.argv) > 2 and sys.argv[2] == '--find':
        pat = sys.argv[3].upper()
        for full, e in fat.walk():
            if pat in e['name'].upper():
                print(f"  {e['size']:>9}  {full}")
        return
    if len(sys.argv) > 2 and sys.argv[2] == '--get':
        ent = fat.resolve(sys.argv[3])
        if not ent: raise SystemExit(f"not found: {sys.argv[3]}")
        open(sys.argv[4], 'wb').write(fat.read(ent))
        print(f"wrote {sys.argv[4]} ({ent['size']} bytes)")
        return
    path = sys.argv[2] if len(sys.argv) > 2 else "C:\\"
    ent = fat.resolve(path)
    clus = 0 if ent is None else ent['clus']
    for e in fat.listdir(clus):
        print(f"  {'<DIR>' if e['dir'] else e['size']:>9}  {e['name']}")

if __name__ == '__main__':   # importable: tools/fatput.py reuses Fat
    main()
