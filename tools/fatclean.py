#!/usr/bin/env python3
r"""Clear a FAT16 volume's dirty flag so the next boot skips ScanDisk.

The emulator loop kills 86Box rather than shutting the guest down, which leaves
the volume marked dirty; Win95 then runs real-mode ScanDisk and never reaches
"WIN /B", so no boot log is produced. Combined with AutoScan=0 in MSDOS.SYS this
keeps every iteration going straight to the thing under test.

FAT[1] bit 15 is the clean-shutdown bit and bit 14 the hard-error bit; both are
set to mean "clean". All FAT copies are updated.

  python tools/fatclean.py <image>
"""
import struct, sys, os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from fatls import Fat


def main():
    image = sys.argv[1]
    fs = Fat(image)
    base, fat_start, bps = fs.base, fs.fat_start, fs.bps
    fs.f.seek(base)
    b = fs.f.read(512)
    nfat = b[16]
    spf = struct.unpack_from('<H', b, 22)[0]
    fs.f.close()

    with open(image, 'r+b') as f:
        for i in range(nfat):
            o = fat_start + i * spf * bps
            f.seek(o)
            cur = struct.unpack('<H', f.read(2))[0]
            f.seek(o + 2)
            e1 = struct.unpack('<H', f.read(2))[0]
            if e1 == 0xFFFF:
                print("FAT%d: entry1 already 0xFFFF (clean)" % i)
                continue
            f.seek(o + 2)
            f.write(struct.pack('<H', 0xFFFF))
            print("FAT%d: entry1 0x%04X -> 0xFFFF (media 0x%02X)" % (i, e1, cur & 0xFF))


main()
