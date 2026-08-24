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


def main():
    if len(sys.argv) not in (4, 5):
        raise SystemExit(__doc__)
    img, path, local = sys.argv[1], sys.argv[2], sys.argv[3]

    new = open(local, 'rb').read()
    fat = Fat(img)
    ent = fat.resolve(path)
    if ent is None or ent['dir']:
        raise SystemExit(f"no such file in image: {path}")
    if ent['size'] != len(new):
        raise SystemExit(f"SIZE MISMATCH: {path} is {ent['size']} bytes, {local} is {len(new)}. "
                         "This tool only replaces same-size files - see its header.")

    ex = extents(fat, ent)
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

    back = Fat(img)
    e2 = back.resolve(path)
    with open(img, 'rb') as f:
        got = read_extents(f, extents(back, e2))
    if got != new:
        raise SystemExit("READ-BACK FAILED: the image does not contain what was written.")
    print(f"Read back through the FAT: OK, md5 {hashlib.md5(got).hexdigest()}")


main()
