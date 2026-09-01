#!/usr/bin/env python3
"""Build a partitioned FAT16 disk image for the XT-IDE port-driver test bed.

  python mkfatimg.py <image> [--cyls N --heads N --spt N]

The scratch disk started life as a raw 8 MB surface with no partition table, so
Windows' disk TSD correctly found nothing to mount and issued exactly one read.
A driver that services one read is not a proven driver. This builds a real
volume, so mounting it exercises the partition scan, the boot sector, both FATs
and the root directory - dozens of reads at addresses we can predict.

The partition deliberately stops short of LBA 16000, where the driver's
diagnostic marker lives (tools/pdr_reqmarker.py). Overlapping them would let a
filesystem write destroy the only channel the driver has for reporting.
"""
import argparse
import os
import struct
import sys

MARKER_LBA = 16000
PART_START = 63          # head 1, cylinder 0 - the conventional first track gap
SLACK = 128              # sectors left between the volume and the marker


def chs(lba, heads, spt):
    """LBA -> (head, sector, cylinder) packed as the three MBR bytes."""
    cyl, rem = divmod(lba, heads * spt)
    head, sec = divmod(rem, spt)
    sec += 1
    if cyl > 1023:
        cyl, head, sec = 1023, heads - 1, spt
    return bytes([head, (sec & 0x3F) | ((cyl >> 2) & 0xC0), cyl & 0xFF])


def fat16_size(total, spc, rsvd, nfats, rootents):
    """Smallest FAT size in sectors that describes the volume it creates."""
    rootsecs = (rootents * 32 + 511) // 512
    fatsz = 1
    while True:
        data = total - rsvd - nfats * fatsz - rootsecs
        clusters = data // spc
        need = ((clusters + 2) * 2 + 511) // 512
        if need <= fatsz:
            return fatsz, clusters, rootsecs
        fatsz = need


def build(path, cyls, heads, spt):
    total_sectors = cyls * heads * spt
    if total_sectors * 512 != os.path.getsize(path):
        print('geometry %d/%d/%d = %d sectors, image is %d bytes'
              % (cyls, heads, spt, total_sectors, os.path.getsize(path)))
        return 1

    part_sectors = MARKER_LBA - SLACK - PART_START
    spc = 2
    fatsz, clusters, rootsecs = fat16_size(part_sectors, spc, 1, 2, 512)
    if not 4085 <= clusters < 65525:
        print('cluster count %d is not FAT16' % clusters)
        return 1

    img = bytearray(total_sectors * 512)

    # ---- MBR ------------------------------------------------------------
    mbr = bytearray(512)
    entry = bytearray(16)
    entry[0] = 0x80                                   # bootable
    entry[1:4] = chs(PART_START, heads, spt)
    entry[4] = 0x06                                   # FAT16, as Windows itself writes
    entry[5:8] = chs(PART_START + part_sectors - 1, heads, spt)
    entry[8:12] = struct.pack('<I', PART_START)
    entry[12:16] = struct.pack('<I', part_sectors)
    mbr[446:462] = entry
    mbr[510:512] = b'\x55\xaa'
    img[0:512] = mbr

    # ---- boot sector / BPB ----------------------------------------------
    bs = bytearray(512)
    bs[0:3] = b'\xeb\x3c\x90'
    bs[3:11] = b'MSWIN4.1'
    struct.pack_into('<HBHBHHBHHHII', bs, 11,
                     512,            # bytes per sector
                     spc,            # sectors per cluster
                     1,              # reserved sectors
                     2,              # number of FATs
                     512,            # root entries
                     0,              # TotSec16 unused - Windows uses the 32-bit field
                     0xF8,           # media descriptor
                     fatsz,          # sectors per FAT
                     spt,            # sectors per track
                     heads,          # heads
                     PART_START,     # hidden sectors
                     part_sectors)   # total sectors, 32-bit
    bs[36] = 0x80                                     # drive number
    bs[38] = 0x29                                     # extended boot signature
    bs[39:43] = struct.pack('<I', 0x58544944)
    bs[43:54] = b'XTIDE VOL  '
    bs[54:62] = b'FAT16   '
    bs[510:512] = b'\x55\xaa'

    fat_start = PART_START + 1
    root_start = fat_start + 2 * fatsz
    data_start = root_start + rootsecs

    img[PART_START * 512:(PART_START + 1) * 512] = bs

    # ---- FATs -----------------------------------------------------------
    fat = bytearray(fatsz * 512)
    struct.pack_into('<HHH', fat, 0, 0xFFF8, 0xFFFF, 0xFFFF)   # media, EOC, file
    for i in range(2):
        off = (fat_start + i * fatsz) * 512
        img[off:off + len(fat)] = fat

    # ---- root directory: a volume label and one readable file -----------
    payload = (b'Written by tools/mkfatimg.py on the host.\r\n'
               b'If Windows can read this through PORT.PDR, the 32-bit path\r\n'
               b'is doing the reading - nothing else can reach this disk.\r\n')
    root = bytearray(rootsecs * 512)
    root[0:11] = b'XTIDE VOL  '
    root[11] = 0x08                                   # volume label
    root[32:43] = b'HELLO   TXT'
    root[43] = 0x20                                   # archive
    struct.pack_into('<HH', root, 32 + 26, 2, 0)      # first cluster
    struct.pack_into('<I', root, 32 + 28, len(payload))
    img[root_start * 512:root_start * 512 + len(root)] = root

    off = data_start * 512
    img[off:off + len(payload)] = payload

    with open(path, 'wb') as f:
        f.write(img)

    print('%s  %d/%d/%d  %d sectors' % (os.path.basename(path), cyls, heads, spt, total_sectors))
    print('  partition   LBA %d .. %d   type 06, FAT16' % (PART_START, PART_START + part_sectors - 1))
    print('  FAT         LBA %d and %d, %d sectors each' % (fat_start, fat_start + fatsz, fatsz))
    print('  root dir    LBA %d, %d sectors' % (root_start, rootsecs))
    print('  first data  LBA %d   HELLO.TXT, %d bytes' % (data_start, len(payload)))
    print('  marker      LBA %d, %d sectors clear of the volume' % (MARKER_LBA, SLACK))
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('image')
    ap.add_argument('--cyls', type=int, default=16)
    ap.add_argument('--heads', type=int, default=16)
    ap.add_argument('--spt', type=int, default=63)
    a = ap.parse_args()
    return build(a.image, a.cyls, a.heads, a.spt)


sys.exit(main())
