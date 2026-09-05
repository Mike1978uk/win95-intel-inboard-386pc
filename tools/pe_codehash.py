#!/usr/bin/env python3
r"""Hash a PE's CONTENT, neutralising the timestamps the linker stamps into it.

Technique 89 asks that any binary found later on a card or in an image be
identifiable by its hash alone. For an LE VxD that works: MASM+LINK produce a
byte-identical file from identical source. For a **PE** it does not - LINK
writes the build time into the file in several places, so two builds of
identical source differ and the ledger's promise quietly stops holding.

Measured on XTIDEMP.MPD, 2026-09-05 - same source, same flags, two builds, nine
differing bytes, every one of them a timestamp or a checksum over one:

  file 136..137    COFF header TimeDateStamp
  file 216..217    optional header CheckSum
  file 4612, 4640  TimeDateStamp of each IMAGE_DEBUG_DIRECTORY entry, which
                   lives INSIDE .rdata - excluding a section called ".debug"
                   does not catch it, and that mistake was made first
  file 10524       trailing debug data past the last section

Reading that as "the binary changed" would send somebody hunting a
nondeterministic toolchain, which is the archaeology technique 89 exists to
prevent. So zero those fields, drop anything past the last section, and hash
what is left. Two builds of one source agree; one changed instruction does not.

  python tools/pe_codehash.py <file.mpd|.exe|.dll>
"""
import hashlib
import struct
import sys

IMAGE_DIRECTORY_ENTRY_DEBUG = 6
DEBUG_DIRECTORY_ENTRY_SIZE = 28


def code_hash(data: bytes) -> str:
    if data[:2] != b'MZ':
        raise SystemExit("not a PE: no MZ signature")
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    if data[pe:pe + 4] != b'PE\0\0':
        raise SystemExit("not a PE: no PE signature at e_lfanew")

    b = bytearray(data)
    nsec = struct.unpack_from('<H', b, pe + 6)[0]
    optsz = struct.unpack_from('<H', b, pe + 20)[0]
    opt = pe + 24

    # COFF TimeDateStamp, and the optional header's CheckSum (computed over it).
    struct.pack_into('<I', b, pe + 8, 0)
    struct.pack_into('<I', b, opt + 64, 0)

    # Section table: needed both to map the debug directory's RVA to a file
    # offset and to find where real content stops.
    secs = []
    off = opt + optsz
    for _ in range(nsec):
        vaddr, rawsize, rawptr = struct.unpack_from('<III', b, off + 12)
        vsize = struct.unpack_from('<I', b, off + 8)[0]
        secs.append((vaddr, vsize, rawptr, rawsize))
        off += 40

    def rva_to_off(rva):
        for vaddr, vsize, rawptr, rawsize in secs:
            if vaddr <= rva < vaddr + max(vsize, rawsize):
                return rawptr + (rva - vaddr)
        return None

    # Every debug directory entry carries its own TimeDateStamp.
    dd = opt + 96 + IMAGE_DIRECTORY_ENTRY_DEBUG * 8
    dbg_rva, dbg_size = struct.unpack_from('<II', b, dd)
    if dbg_rva:
        base = rva_to_off(dbg_rva)
        if base is not None:
            for i in range(dbg_size // DEBUG_DIRECTORY_ENTRY_SIZE):
                struct.pack_into('<I', b, base + i * DEBUG_DIRECTORY_ENTRY_SIZE + 4, 0)

    # Trailing debug data past the last section is build-time bookkeeping.
    end = max((rawptr + rawsize) for _, _, rawptr, rawsize in secs if rawsize)
    return hashlib.md5(bytes(b[:end])).hexdigest()


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    with open(sys.argv[1], 'rb') as f:
        data = f.read()
    print("code %s  file %s" % (code_hash(data)[:8], hashlib.md5(data).hexdigest()[:8]))


if __name__ == '__main__':
    main()
