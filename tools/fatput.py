#!/usr/bin/env python3
r"""Replace an EXISTING file inside a raw FAT12/16 disk image, in place, same size only.

The companion to fatls.py. The CF card lives in the 5160 or in a running emulator,
so the raw image is the only copy reachable from the host, and there is no mount
tooling on this box. This writes a replacement file over the SAME cluster chain,
which means:

  - the FAT, the directory entry and the file size are never touched, so a bad run
    cannot corrupt the volume's metadata - the worst case is wrong file contents,
    fixable by putting the original back;
  - reverting is the same command with the original file.

Same size is therefore a hard requirement, not a convenience. It holds for the
maxPhys patches (patch_vxd_dma_maxphys.py changes bytes, never lengths) and for the
VxD patches generally.

  python tools/fatput.py <image> <path-in-image> <local-file> [--yes]

Prints a before/after diff of the changed byte offsets and reads the file back
through fatls to prove what landed.
"""
import sys, os, hashlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from fatls import Fat


def chain(fat, clus):
    out = []
    c = clus
    while 2 <= c < 0xFFF8:
        out.append(c)
        c = fat.nxt(c)
    return out


def extents(fat, ent):
    """[(image_offset, length)] covering exactly the file's `size` bytes."""
    out, left = [], ent['size']
    for c in chain(fat, ent['clus']):
        if left <= 0:
            break
        n = min(left, fat.csize)
        out.append((fat.coff(c), n))
        left -= n
    if left:
        raise SystemExit(f"cluster chain is short by {left} bytes - refusing to write")
    return out


def read_extents(f, ex):
    b = bytearray()
    for off, n in ex:
        f.seek(off)
        b += f.read(n)
    return bytes(b)


def update_dirent_size(img, path, newsize):
    """Rewrite just the 4-byte size field of one directory entry."""
    fat = Fat(img)
    parts = [q for q in path.replace('/', chr(92)).split(chr(92)) if q and not q.endswith(':')]
    name = parts[-1].upper()
    parent = fat.resolve(chr(92).join(parts[:-1])) if len(parts) > 1 else None
    clus = 0 if parent is None else parent['clus']

    def entry_offsets(off, n):
        return [(off + i * 32) for i in range(n)]

    cands = []
    if clus == 0:
        cands = entry_offsets(fat.root_start, fat.rootent)
    else:
        c = clus
        while 2 <= c < 0xFFF8:
            cands += entry_offsets(fat.coff(c), fat.csize // 32)
            c = fat.nxt(c)
    fat.f.seek(0)
    with open(img, 'r+b') as f:
        for off in cands:
            f.seek(off)
            e = f.read(32)
            if not e or e[0] == 0x00:
                break
            if e[0] == 0xE5 or e[11] == 0x0F:
                continue
            nm = e[0:8].decode('cp437').rstrip()
            ex = e[8:11].decode('cp437').rstrip()
            full = nm + ("." + ex if ex else "")
            if full.upper() == name:
                f.seek(off + 28)
                f.write(newsize.to_bytes(4, 'little'))
                f.flush(); os.fsync(f.fileno())
                print(f"  directory entry size field updated to {newsize}")
                return
    raise SystemExit(f"could not find the directory entry for {path} to update its size")


def main():
    if len(sys.argv) not in (4, 5):
        raise SystemExit(__doc__)
    img, path, local = sys.argv[1], sys.argv[2], sys.argv[3]

    new = open(local, 'rb').read()
    fat = Fat(img)
    ent = fat.resolve(path)
    if ent is None or ent['dir']:
        raise SystemExit(f"no such file in image: {path}")
    grow = len(new) - ent['size']
    if grow:
        # Growth is allowed ONLY when it still fits inside the clusters the file already
        # owns, so the FAT chain is untouched and only the directory entry's size field
        # changes. Anything needing a new cluster is refused - allocating is where a
        # buggy writer corrupts a volume.
        chain_bytes = len(chain(fat, ent['clus'])) * fat.csize
        if len(new) > chain_bytes:
            raise SystemExit(f"REFUSING TO GROW {path} from {ent['size']} to {len(new)} bytes: that "
                             f"needs a new cluster ({chain_bytes} allocated). This tool never "
                             "allocates - see its header.")
        if grow < 0:
            raise SystemExit(f"REFUSING TO SHRINK {path} ({ent['size']} -> {len(new)}): shrinking "
                             "would orphan clusters. Not supported.")
        print(f"{path}: growing {ent['size']} -> {len(new)} bytes inside the "
              f"{chain_bytes}-byte cluster chain (FAT untouched)")

    ent_for_extents = dict(ent, size=max(ent['size'], len(new)))
    ex = extents(fat, ent_for_extents)
    with open(img, 'rb') as f:
        old = read_extents(f, ex)

    diff = [i for i in range(len(new)) if old[i] != new[i]]
    if not diff:
        print(f"{path} already byte-identical to {local}; nothing to do.")
        return
    print(f"{path}: {len(diff)} byte(s) differ across {len(ex)} cluster extent(s)")
    for i in diff[:16]:
        print(f"  file 0x{i:06x}: {old[i]:02x} -> {new[i]:02x}")
    if len(diff) > 16:
        print(f"  ... and {len(diff)-16} more")
    print(f"  md5 {hashlib.md5(old).hexdigest()} -> {hashlib.md5(new).hexdigest()}")

    if '--yes' not in sys.argv:
        raise SystemExit("Refusing to write without --yes.")

    fat.f.close()
    with open(img, 'r+b') as f:
        pos = 0
        for off, n in ex:
            f.seek(off)
            f.write(new[pos:pos + n])
            pos += n
        f.flush()
        os.fsync(f.fileno())

    if grow:
        update_dirent_size(img, path, len(new))

    back = Fat(img)
    e2 = back.resolve(path)
    with open(img, 'rb') as f:
        got = read_extents(f, extents(back, e2))
    if got != new:
        raise SystemExit("READ-BACK FAILED: the image does not contain what was written.")
    print(f"Read back through the FAT: OK, md5 {hashlib.md5(got).hexdigest()}")


main()
