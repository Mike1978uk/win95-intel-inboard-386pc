#!/usr/bin/env python3
"""Stop ELNK3.VXD opening the 3C509B to every multicast frame on the LAN (#41).

The 3C509B has no multicast hash filter: its receive filter has one bit that accepts all
multicast or none. The driver maps Windows' NDIS packet filter onto it (file 0x44a4):

    44C2  f6 c1 02     test cl, 2          ; NDIS_PACKET_TYPE_MULTICAST (a list of groups)
    44C5  74 03        je   +3
    44C7  83 c8 02     or   eax, 2         <-- PATCHED TO "or eax, 0"
    44CA  f6 c1 04     test cl, 4          ; NDIS_PACKET_TYPE_ALL_MULTICAST - unchanged

Windows 95's TCP/IP asks for a multicast list, so the card ends up with filter 7
(own address, multicast, broadcast; logged in 86Box) and reads every mDNS, SSDP and IPv6
multicast frame on the LAN across the 8-bit bus, only for the stack to discard it. With
the patch a multicast-list request leaves the card on own address plus broadcast; an
explicit all-multicast request still opens it.

  python patch_elnk3_nomcast.py <in.VXD> <out.VXD>
"""

import hashlib
import sys

ANCHOR = bytes.fromhex("f6c102740383c802f6c104")
PATCHED = bytes.fromhex("f6c102740383c800f6c104")
STOCK_MD5 = "301b14c435de0a94d8b2f5aba7bf42f8"


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    src, dst = sys.argv[1], sys.argv[2]
    data = bytearray(open(src, "rb").read())
    md5 = hashlib.md5(data).hexdigest()
    if md5 != STOCK_MD5:
        print("warning: input is not the ELNK3.VXD this was written for (%s)" % md5)
    if data.count(ANCHOR) != 1:
        sys.exit("anchor found %d times, expected 1 - not patching" % data.count(ANCHOR))
    off = data.index(ANCHOR)
    data[off:off + len(ANCHOR)] = PATCHED
    open(dst, "wb").write(data)
    print("Patched: 1 at file offset 0x%x" % (off + 7))
    print("%s  %s" % (hashlib.md5(data).hexdigest(), dst))


if __name__ == "__main__":
    main()
