#!/usr/bin/env python3
r"""Verify the miniport's write path by comparing two trees INSIDE the image.

WRTEST.BAT has the guest copy C:\DOS to C:\WRDST. The source was already on the
medium; the destination was written by Windows through our miniport. Comparing
them from the HOST - neither read going through the driver under test - is the
only way to see a write-path fault, because a round trip applies the same error
twice and cancels (technique 79).

This is the check the .PDR failed in September: it misread the scatter/gather
descriptor list and wrote the list itself into the volume's root directory. A
self-test inside the guest reported success.

  python drivers/xtide_mpd/test/verify_write.py <image>

Exit status is 0 only if every file matched.
"""
import hashlib
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                '..', '..', '..', 'tools'))
from fatls import Fat  # noqa: E402

SRC = "DOS"
DST = "WRDST"


def collect(fat, root):
    """name (lowercased, relative) -> (size, md5) for every file under root."""
    out = {}
    ent = fat.resolve(root)          # returns the ENTRY, or None - it does not raise
    if ent is None:
        return None
    stack = [(ent['clus'], "")]
    while stack:
        c, rel = stack.pop()
        for e in fat.listdir(c):
            name = e['name']
            if name in ('.', '..'):
                continue
            sub = (rel + "/" + name) if rel else name
            if e.get('dir'):
                stack.append((e['clus'], sub))
            else:
                data = fat.read(e)
                out[sub.lower()] = (len(data), hashlib.md5(data).hexdigest())
    return out


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    img = sys.argv[1]
    fat = Fat(img)

    src = collect(fat, SRC)
    dst = collect(fat, DST)

    if src is None:
        raise SystemExit("C:\\%s is not on this image - wrong image?" % SRC)
    if dst is None:
        print("RESULT: FAIL - C:\\%s does not exist. The copy never ran; the run" % DST)
        print("        is VOID rather than a result about the write path.")
        return 1

    print("source  C:\\%-8s %4d files, %10d bytes" %
          (SRC, len(src), sum(s for s, _ in src.values())))
    print("written C:\\%-8s %4d files, %10d bytes" %
          (DST, len(dst), sum(s for s, _ in dst.values())))
    print()

    if not dst:
        print("RESULT: FAIL - nothing was written. The copy never ran.")
        return 1

    missing = sorted(set(src) - set(dst))
    extra = sorted(set(dst) - set(src))
    mismatch = sorted(k for k in (set(src) & set(dst)) if src[k] != dst[k])

    for k in missing[:10]:
        print("  MISSING   %s" % k)
    for k in extra[:10]:
        print("  UNEXPECTED %s" % k)
    for k in mismatch[:20]:
        print("  CORRUPT   %-24s src %d/%s  dst %d/%s"
              % (k, src[k][0], src[k][1][:8], dst[k][0], dst[k][1][:8]))

    ok = len(set(src) & set(dst)) - len(mismatch)
    print()
    print("matched %d, corrupt %d, missing %d, unexpected %d"
          % (ok, len(mismatch), len(missing), len(extra)))

    if mismatch or missing:
        print("RESULT: FAIL - the write path does not reproduce the source bytes.")
        return 1
    print("RESULT: PASS - every file written through the miniport matches the source.")
    return 0


if __name__ == '__main__':
    sys.exit(main())
