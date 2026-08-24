#!/usr/bin/env python3
r"""Zero every unallocated cluster in a raw FAT12/16 image, so it compresses.

Why: these images are ~1.9 GB files holding ~119 MB of real data. The rest is not
zeroes - it is whatever the card held before - so 7z only manages about 2:1 and the
release asset comes out near 1 GB. Zeroing the free clusters first takes the same
content to well under a tenth of that.

Safety: this only writes to clusters whose FAT entry is 0 (free). It never touches
the boot sector, the FATs, the root directory, or any allocated cluster. It verifies
itself by hashing the CONTENT OF EVERY FILE on the volume before and after and
refusing to report success unless all of them are identical.

Still: run it on a COPY. It is a destructive operation on a disk image, and the whole
point of a golden image is that there is an unmodified original somewhere.

  python tools/zero_free_space.py <image> [--yes]
"""
import sys, os, struct, hashlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from fatls import Fat


def hash_all_files(img):
    """{path: (md5, size)} for every file on the volume."""
    fat = Fat(img)
    out = {}
    for path, e in fat.walk():
        if e['dir']:
            continue
        out[path] = (hashlib.md5(fat.read(e)).hexdigest(), e['size'])
    fat.f.close()
    return out


def free_clusters(img):
    fat = Fat(img)
    n = len(fat.fat) // 2
    free = [c for c in range(2, n) if struct.unpack_from('<H', fat.fat, c * 2)[0] == 0]
    csize, coff = fat.csize, fat.coff
    total = os.path.getsize(img)
    # A cluster whose bytes run past the end of the file is outside the partition's
    # real extent - skip rather than extend the file.
    free = [c for c in free if coff(c) + csize <= total]
    fat.f.close()
    return free, csize, coff


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    img = sys.argv[1]

    print("Hashing every file on the volume (before)...")
    before = hash_all_files(img)
    print(f"  {len(before)} files")

    free, csize, coff = free_clusters(img)
    mb = len(free) * csize / 1024 / 1024
    print(f"Free clusters: {len(free)} x {csize // 1024} KB = {mb:.0f} MB to zero")

    if '--yes' not in sys.argv:
        raise SystemExit("Refusing to write without --yes. Run this on a COPY.")

    zeros = bytes(csize)
    written = 0
    with open(img, 'r+b') as f:
        for c in free:
            f.seek(coff(c))
            f.write(zeros)
            written += 1
            if written % 5000 == 0:
                print(f"  {written}/{len(free)}...")
        f.flush()
        os.fsync(f.fileno())
    print(f"  zeroed {written} clusters")

    print("Hashing every file on the volume (after)...")
    after = hash_all_files(img)

    if before != after:
        missing = set(before) - set(after)
        added = set(after) - set(before)
        changed = {p for p in before.keys() & after.keys() if before[p] != after[p]}
        print(f"VERIFY FAILED: {len(missing)} missing, {len(added)} new, {len(changed)} changed")
        for p in list(missing | added | changed)[:20]:
            print(f"  {p}")
        raise SystemExit(1)

    print(f"Verified: all {len(after)} files byte-identical. Image is unchanged as a filesystem.")


main()
