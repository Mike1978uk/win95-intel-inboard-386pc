"""Create a blank, formatted 1.44 MB FAT12 floppy image.

Needed because the #18 bed has to read a floppy whose contents the host
knows exactly. Formatting inside the guest is not an option: the 32-bit
floppy driver is the thing under test, so a guest-side FORMAT would be
writing with the code we are trying to judge.

Files go in afterwards with tools/fatcp.py, which already speaks FAT12.
"""
import os
import sys

SECTOR = 512
SECTORS = 2880           # 1.44 MB
FAT_SECTORS = 9
ROOT_ENTRIES = 224
RESERVED = 1
NUM_FATS = 2


def build(label=b"FDTEST     "):
    img = bytearray(b"\x00" * (SECTOR * SECTORS))

    bs = bytearray(SECTOR)
    bs[0:3] = b"\xEB\x3C\x90"
    bs[3:11] = b"MSDOS5.0"
    bs[11:13] = (SECTOR).to_bytes(2, "little")
    bs[13] = 1                                    # sectors per cluster
    bs[14:16] = (RESERVED).to_bytes(2, "little")
    bs[16] = NUM_FATS
    bs[17:19] = (ROOT_ENTRIES).to_bytes(2, "little")
    bs[19:21] = (SECTORS).to_bytes(2, "little")
    bs[21] = 0xF0                                 # media descriptor
    bs[22:24] = (FAT_SECTORS).to_bytes(2, "little")
    bs[24:26] = (18).to_bytes(2, "little")        # sectors per track
    bs[26:28] = (2).to_bytes(2, "little")         # heads
    bs[28:32] = (0).to_bytes(4, "little")         # hidden sectors
    bs[32:36] = (0).to_bytes(4, "little")
    bs[36] = 0x00                                 # drive number
    bs[38] = 0x29                                 # extended boot signature
    bs[39:43] = b"\x34\x12\x78\x56"               # volume serial
    bs[43:54] = label
    bs[54:62] = b"FAT12   "
    bs[510:512] = b"\x55\xAA"
    img[0:SECTOR] = bs

    # Both FATs: media byte then the two reserved entries.
    for n in range(NUM_FATS):
        off = (RESERVED + n * FAT_SECTORS) * SECTOR
        img[off:off + 3] = b"\xF0\xFF\xFF"

    return bytes(img)



def _fat12_set(fat, n, val):
    """Write entry n of a packed FAT12. Two entries share three bytes."""
    off = n + (n // 2)
    if n % 2 == 0:
        fat[off] = val & 0xFF
        fat[off + 1] = (fat[off + 1] & 0xF0) | ((val >> 8) & 0x0F)
    else:
        fat[off] = (fat[off] & 0x0F) | ((val << 4) & 0xF0)
        fat[off + 1] = (val >> 4) & 0xFF


def add_file(img, name83, data):
    """Place one file contiguously from cluster 2 and chain it in FAT12.

    Deliberately not general: the bed needs one known file on a blank disk,
    and a correct 12-bit chain matters more here than a directory walker.
    tools/fatcp.py cannot do this - its FAT accessor is 16-bit only.
    """
    img = bytearray(img)
    data_start = (RESERVED + NUM_FATS * FAT_SECTORS) * SECTOR + ROOT_ENTRIES * 32
    nclus = (len(data) + SECTOR - 1) // SECTOR
    if nclus == 0:
        raise SystemExit("FAILED: refusing to add an empty file")

    img[data_start:data_start + len(data)] = data

    fat = bytearray(FAT_SECTORS * SECTOR)
    fat[0:3] = bytes([0xF0, 0xFF, 0xFF])
    for i in range(nclus):
        c = 2 + i
        _fat12_set(fat, c, 0xFFF if i == nclus - 1 else c + 1)
    for n in range(NUM_FATS):
        off = (RESERVED + n * FAT_SECTORS) * SECTOR
        img[off:off + len(fat)] = fat

    ent = bytearray(32)
    ent[0:11] = name83
    ent[11] = 0x20
    ent[22:24] = (0).to_bytes(2, "little")
    ent[24:26] = ((2026 - 1980) << 9 | 9 << 5 | 20).to_bytes(2, "little")
    ent[26:28] = (2).to_bytes(2, "little")
    ent[28:32] = len(data).to_bytes(4, "little")
    root = (RESERVED + NUM_FATS * FAT_SECTORS) * SECTOR
    img[root:root + 32] = ent
    return bytes(img)

if __name__ == "__main__":
    if len(sys.argv) not in (2, 4):
        sys.exit("usage: mkfloppy.py <out.img> [NAME----EXT <file>]")
    out = sys.argv[1]
    data = build()
    if len(sys.argv) == 4:
        data = add_file(data, sys.argv[2].encode("ascii"), open(sys.argv[3], "rb").read())
    if len(data) != SECTOR * SECTORS:
        sys.exit(f"FAILED: built {len(data)} bytes, expected {SECTOR * SECTORS}")
    tmp = out + ".tmp"
    with open(tmp, "wb") as f:
        f.write(data)
    os.replace(tmp, out)
    print(f"wrote {out}: {len(data)} bytes, FAT12, 2880 sectors")
