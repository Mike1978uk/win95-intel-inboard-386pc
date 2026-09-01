#!/usr/bin/env python3
r"""Create or replace a file inside a raw FAT12/16 image, any size.

fatput.py deliberately refuses to change a file's length, because on the CF card
in the 5160 a bad run must not be able to damage the volume. That safety is the
wrong trade for the emulator loop, where the whole point is dropping a NEW file
into IOSUBSYS and re-reading the boot log. This one allocates and frees clusters
and writes directory entries, so it can do that - use it on VM images, and prefer
fatput.py for the card.

  python tools/fatcp.py <image> <path-in-image> <local-file> [--attr rhs] [--yes]
  python tools/fatcp.py <image> --rm <path-in-image> [--yes]
  python tools/fatcp.py <image> --mkdir <path-in-image>

Every write is verified by reading the file back through fatls.
"""
import struct, sys, os, time, hashlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from fatls import Fat


class FatRW(Fat):
    """Fat, reopened read/write, with cluster allocation and directory writes."""

    def __init__(self, path):
        Fat.__init__(self, path)
        self.fat = bytearray(self.fat)   # Fat reads it immutable; we rewrite entries
        self.f.close()
        self.f = open(path, 'r+b')
        # re-read the geometry fields Fat does not keep
        self.f.seek(self.base)
        b = self.f.read(512)
        self.nfat = b[16]
        self.spf = struct.unpack_from('<H', b, 22)[0]
        tot16 = struct.unpack_from('<H', b, 19)[0]
        tot32 = struct.unpack_from('<I', b, 32)[0]
        self.totsec = tot16 or tot32
        self.nclus = (self.totsec - (self.data_start - self.base) // self.bps) // self.spc + 2
        self.fat12 = self.nclus < 4085
        self.eoc = 0xFFF if self.fat12 else 0xFFFF

    # ---- FAT entry access, in the in-memory copy ----
    def get(self, c):
        if not self.fat12:
            return struct.unpack_from('<H', self.fat, c * 2)[0]
        o = c + c // 2
        v = struct.unpack_from('<H', self.fat, o)[0]
        return (v >> 4) if (c & 1) else (v & 0xFFF)

    def set(self, c, v):
        if not self.fat12:
            struct.pack_into('<H', self.fat, c * 2, v & 0xFFFF)
            return
        o = c + c // 2
        cur = struct.unpack_from('<H', self.fat, o)[0]
        cur = ((v << 4) | (cur & 0x000F)) if (c & 1) else ((cur & 0xF000) | (v & 0xFFF))
        struct.pack_into('<H', self.fat, o, cur & 0xFFFF)

    def flush_fat(self):
        for i in range(self.nfat):
            self.f.seek(self.fat_start + i * self.spf * self.bps)
            self.f.write(self.fat)

    def chain(self, c):
        out = []
        while 2 <= c < (0xFF8 if self.fat12 else 0xFFF8):
            out.append(c)
            c = self.get(c)
        return out

    def free_chain(self, c):
        for x in self.chain(c):
            self.set(x, 0)

    def alloc(self, n):
        got = []
        for c in range(2, self.nclus):
            if self.get(c) == 0:
                got.append(c)
                if len(got) == n:
                    break
        if len(got) != n:
            raise SystemExit("not enough free clusters: need %d, found %d" % (n, len(got)))
        for i, c in enumerate(got):
            self.set(c, got[i + 1] if i + 1 < len(got) else self.eoc)
        return got

    # ---- directory access ----
    def dir_slots(self, clus):
        """[(image_offset, raw32)] over every 32-byte slot of a directory."""
        out = []
        if clus == 0:
            self.f.seek(self.root_start)
            d = self.f.read(self.rootent * 32)
            for i in range(self.rootent):
                out.append((self.root_start + i * 32, d[i * 32:i * 32 + 32]))
            return out
        for c in self.chain(clus):
            self.f.seek(self.coff(c))
            d = self.f.read(self.csize)
            for i in range(self.csize // 32):
                out.append((self.coff(c) + i * 32, d[i * 32:i * 32 + 32]))
        return out

    def resolve_dir(self, path):
        """Directory cluster for the parent of `path`, plus the 8.3 leaf name."""
        parts = path.replace('/', chr(92)).lstrip(chr(92)).split(chr(92))
        if parts and len(parts[0]) == 2 and parts[0][1] == ':':
            parts = parts[1:]
        leaf = parts[-1]
        clus = 0
        for p in parts[:-1]:
            for e in self.listdir(clus):
                if e['dir'] and e['name'].upper() == p.upper():
                    clus = e['clus']
                    break
            else:
                raise SystemExit("no such directory: " + p)
        return clus, leaf.upper()


def name83(leaf):
    b, _, e = leaf.partition('.')
    if len(b) > 8 or len(e) > 3:
        raise SystemExit("not an 8.3 name: " + leaf)
    return (b.ljust(8) + e.ljust(3)).encode('cp437')


def fat_time():
    t = time.localtime()
    return (((t.tm_hour << 11) | (t.tm_min << 5) | (t.tm_sec // 2)),
            (((t.tm_year - 1980) << 9) | (t.tm_mon << 5) | t.tm_mday))



def make_dir(fs, dest):
    """Create one directory. Win95 Have Disk wants the INF in its own directory,
    the way PORTPDR is laid out on the real card."""
    dclus, leaf = fs.resolve_dir(dest)
    tag = name83(leaf)
    free = None
    for off, raw in fs.dir_slots(dclus):
        if raw[11] == 0x0F:
            continue
        if raw[0:11] == tag and (raw[11] & 0x10):
            print("already a directory: " + dest)
            return
        if raw[0] in (0x00, 0xE5):
            if free is None:
                free = off
            if raw[0] == 0x00:
                break
    if free is None:
        raise SystemExit("parent directory is full")
    c = fs.alloc(1)[0]
    fs.f.seek(fs.coff(c))
    fs.f.write(bytes(fs.csize))
    tm, dt = fat_time()

    def ent(nm, attr, clus):
        e = bytearray(32)
        e[0:11] = nm
        e[11] = attr
        struct.pack_into('<H', e, 22, tm)
        struct.pack_into('<H', e, 24, dt)
        struct.pack_into('<H', e, 26, clus)
        return bytes(e)

    fs.f.seek(fs.coff(c))
    fs.f.write(ent(b'.          ', 0x10, c))
    fs.f.write(ent(b'..         ', 0x10, dclus))
    fs.f.seek(free)
    fs.f.write(ent(tag, 0x10, c))
    fs.flush_fat()
    fs.f.flush()
    fs.f.close()
    print("created %s (cluster %d)" % (dest, c))


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    opts = [a for a in sys.argv[1:] if a.startswith('--')]
    attr = 0x20
    for o in opts:
        if o.startswith('--attr'):
            v = o.split('=', 1)[1] if '=' in o else sys.argv[sys.argv.index(o) + 1]
            attr = 0x20 | (0x01 if 'r' in v else 0) | (0x02 if 'h' in v else 0) \
                        | (0x04 if 's' in v else 0)
    if '--mkdir' in sys.argv:
        make_dir(FatRW(args[0]), args[1])
        return
    remove = '--rm' in sys.argv
    if remove:
        image, dest = args[0], args[1]
        local = None
    else:
        if len(args) < 3:
            raise SystemExit(__doc__)
        image, dest, local = args[0], args[1], args[2]

    fs = FatRW(image)
    dclus, leaf = fs.resolve_dir(dest)
    tag = name83(leaf)

    slot = None
    free = None
    for off, raw in fs.dir_slots(dclus):
        if raw[11] == 0x0F:
            continue
        if raw[0] in (0x00, 0xE5):
            if free is None:
                free = off
            if raw[0] == 0x00:
                break
            continue
        if raw[0:11] == tag:
            slot = (off, raw)
            break

    if slot:
        old_clus = struct.unpack_from('<H', slot[1], 26)[0]
        old_size = struct.unpack_from('<I', slot[1], 28)[0]
        print("existing %s: %d bytes, first cluster %d" % (leaf, old_size, old_clus))
        fs.free_chain(old_clus)
        if remove:
            fs.f.seek(slot[0])
            fs.f.write(b'\xE5' + slot[1][1:])
            fs.flush_fat()
            fs.f.flush()
            print("removed " + dest)
            return
        off = slot[0]
        attrib = slot[1][11] if '--attr' not in ' '.join(opts) else attr
    else:
        if remove:
            raise SystemExit("not found: " + dest)
        if free is None:
            raise SystemExit("directory is full: " + dest)
        off = free
        attrib = attr

    data = open(local, 'rb').read()
    n = (len(data) + fs.csize - 1) // fs.csize
    clusters = fs.alloc(n) if n else []
    for i, c in enumerate(clusters):
        fs.f.seek(fs.coff(c))
        fs.f.write(data[i * fs.csize:(i + 1) * fs.csize].ljust(fs.csize, b'\x00'))

    tm, dt = fat_time()
    ent = bytearray(32)
    ent[0:11] = tag
    ent[11] = attrib
    struct.pack_into('<H', ent, 22, tm)
    struct.pack_into('<H', ent, 24, dt)
    struct.pack_into('<H', ent, 26, clusters[0] if clusters else 0)
    struct.pack_into('<I', ent, 28, len(data))
    fs.f.seek(off)
    fs.f.write(bytes(ent))
    fs.flush_fat()
    fs.f.flush()
    fs.f.close()

    back = Fat(image)
    e = back.resolve(dest)
    got = back.read(e)
    ok = got == data
    print("wrote %s: %d bytes, %d clusters, first %d, md5 %s  readback %s"
          % (dest, len(data), n, clusters[0] if clusters else 0,
             hashlib.md5(data).hexdigest(), "OK" if ok else "MISMATCH"))
    if not ok:
        raise SystemExit(1)


main()
