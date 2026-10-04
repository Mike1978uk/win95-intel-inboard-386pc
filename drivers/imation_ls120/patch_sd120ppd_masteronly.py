#!/usr/bin/env python3
"""Make Imation's SD120PPD.MPD probe only the master position on the bridge (#46).

At start-up the miniport (init routine rva 0x1fc1) probes two ATA positions behind
the parallel-port bridge, master (A0h) and slave (B0h):

    2051  ...probe unit [ebp-74h]...
    2097  fe 45 8c         inc  byte [ebp-74h]
    209A  80 7d 8c 02      cmp  byte [ebp-74h], 2      <-- PATCHED TO 1
    209E  72 b1            jb   2051

The LS-120 is the bridge's only device, as master. A position with no device can
leave the status register's BSY bit set, and the BSY wait at rva 0x1ac7 then runs its
full 500,000 passes (10 us stall plus a status read through the bridge each).
The test: does start-up stop costing ~892 BIOS ticks when the slave is never probed?

A unit is only used if its present flag ([unit*48h + 0x20ff0]) was set by a passing
probe; an unprobed slave keeps the flag at 0, as a failed probe would.

  python patch_sd120ppd_masteronly.py <in.MPD> <out.MPD>
"""

import hashlib
import sys

ANCHOR = bytes.fromhex("807d8c0272b1")
PATCHED = bytes.fromhex("807d8c0172b1")
STOCK_MD5 = "08104ffb559ae4b47b84377daee473bc"


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    src, dst = sys.argv[1], sys.argv[2]
    data = bytearray(open(src, "rb").read())
    md5 = hashlib.md5(data).hexdigest()
    if md5 != STOCK_MD5:
        print("warning: input is not the stock miniport (%s)" % md5)
    if data.count(ANCHOR) != 1:
        sys.exit("anchor found %d times, expected 1 - not patching" % data.count(ANCHOR))
    off = data.index(ANCHOR)
    data[off:off + len(ANCHOR)] = PATCHED
    open(dst, "wb").write(data)
    print("Patched: 1 at file offset 0x%x" % (off + 3))
    print("%s  %s" % (hashlib.md5(data).hexdigest(), dst))


if __name__ == "__main__":
    main()
