#!/usr/bin/env python3
from pathlib import Path
import shutil
import sys

SRC = Path(sys.argv[1] if len(sys.argv) > 1 else "ELNK3.VXD")
DST = Path(sys.argv[2] if len(sys.argv) > 2 else "ELNK3_IRQ9_TO_IRQ2_HWREAD.VXD")

PATCH_SITE = 0x3100
EXPECTED_SITE = bytes.fromhex("8D 51 08 66 ED 66 C1 E8 0C")
SITE_JUMP = bytes.fromhex("E9 7B 1E 00 00")

CAVE = 0x4F80
CAVE_CODE = bytes.fromhex(
    "8D 51 08"
    "66 ED"
    "66 C1 E8 0C"
    "66 83 F8 09"
    "75 04"
    "66 B8 02 00"
    "E9 71 E1 FF FF"
)
EXPECTED_CAVE = bytes(len(CAVE_CODE))

def main():
    if not SRC.is_file():
        raise SystemExit(f"source not found: {SRC}")
    data = bytearray(SRC.read_bytes())
    if len(data) != 30773:
        raise SystemExit(f"refusing to patch: expected 30773 bytes, got {len(data)}")
    if data[PATCH_SITE:PATCH_SITE + len(EXPECTED_SITE)] != EXPECTED_SITE:
        raise SystemExit(f"refusing to patch: bytes at 0x{PATCH_SITE:X} do not match {EXPECTED_SITE.hex(' ')}")
    if data[CAVE:CAVE + len(CAVE_CODE)] != EXPECTED_CAVE:
        raise SystemExit(f"refusing to patch: code cave at 0x{CAVE:X} is not zero-filled")
    patched = bytearray(data)
    patched[PATCH_SITE:PATCH_SITE + len(SITE_JUMP)] = SITE_JUMP
    patched[CAVE:CAVE + len(CAVE_CODE)] = CAVE_CODE
    DST.write_bytes(patched)
    bak = DST.with_suffix(DST.suffix + ".original")
    if not bak.exists():
        shutil.copyfile(SRC, bak)
    print(f"patched: {DST}")
    print(f"original preserved: {SRC}")
    print(f"backup: {bak}")
    print(f"hardware-read patch site: 0x{PATCH_SITE:X}")
    print(f"code cave: 0x{CAVE:X}")
    print("IRQ9 is translated to software value 2 only after reading the card.")
    print("The card Resource Configuration register is NOT written with 2 by this patch.")

if __name__ == "__main__":
    main()
