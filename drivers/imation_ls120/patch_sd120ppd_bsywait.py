#!/usr/bin/env python3
"""Shorten the BSY wait in Imation's SD120PPD.MPD, so a missing drive costs seconds (#46).

Every command the miniport sends first waits for the status register's BSY bit to clear
(rva 0x1ac7):

    1AC8  be 20 a1 07 00   mov  esi, 500000      <-- PATCHED TO 65536
    1ACD  6a 07            push 7                  ; read status (reg 7) through the bridge
          ...              test al, 80h / stall(10) / dec esi / loop

Each pass is a 10 us stall plus a status read through the parallel-port bridge: about
86 us on an XT with an Inboard, so the full count is ~43 s. With the drive unplugged or
unpowered, BSY never clears and start-up sits in this loop. 65,536 passes is ~5.6 s
there - still long against a powered drive, which the driver has already given a fixed
2 s after reset.

Apply on top of the master-only patch (patch_sd120ppd_masteronly.py).

  python patch_sd120ppd_bsywait.py <in.MPD> <out.MPD>
"""

import hashlib
import sys

ANCHOR = bytes.fromhex("be20a107006a07e8")
PATCHED = bytes.fromhex("be000001006a07e8")
MASTERONLY_MD5 = "4fafb2e9f0163635c5ffdea385988925"


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    src, dst = sys.argv[1], sys.argv[2]
    data = bytearray(open(src, "rb").read())
    md5 = hashlib.md5(data).hexdigest()
    if md5 != MASTERONLY_MD5:
        print("warning: input is not the master-only miniport (%s)" % md5)
    if data.count(ANCHOR) != 1:
        sys.exit("anchor found %d times, expected 1 - not patching" % data.count(ANCHOR))
    off = data.index(ANCHOR)
    data[off:off + len(ANCHOR)] = PATCHED
    open(dst, "wb").write(data)
    print("Patched: 1 at file offset 0x%x" % (off + 1))
    print("%s  %s" % (hashlib.md5(data).hexdigest(), dst))


if __name__ == "__main__":
    main()
