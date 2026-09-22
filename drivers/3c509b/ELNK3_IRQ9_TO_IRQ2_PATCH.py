#!/usr/bin/env python3
from pathlib import Path
import shutil
import sys

SRC = Path(sys.argv[1] if len(sys.argv) > 1 else "ELNK3.VXD")
DST = Path(sys.argv[2] if len(sys.argv) > 2 else "ELNK3_IRQ9_TO_IRQ2.VXD")

PATCH_SITE = 0x3A8B
EXPECTED = bytes.fromhex("83 F8 09 74 14")
JUMP_TO_CAVE = bytes.fromhex("E9 EB 14 00 00")

CAVE = 0x4F7B
CAVE_BYTES = bytes.fromhex(
    "83 F8 09 "
    "0F 85 06 00 00 00 "
    "B8 02 00 00 00 "
    "E9 16 EB FF FF "
    "E9 FD EA FF FF"
)
EXPECTED_CAVE = bytes(24)

def main():
    if not SRC.is_file():
        raise SystemExit(f"source not found: {SRC}")
    data = bytearray(SRC.read_bytes())
    if len(data) != 30773:
        raise SystemExit(f"refusing to patch: expected 30773 bytes, got {len(data)}")
    if data[PATCH_SITE:PATCH_SITE + len(EXPECTED)] != EXPECTED:
        raise SystemExit(f"refusing to patch: bytes at 0x{PATCH_SITE:X} do not match {EXPECTED.hex(' ')}")
    if data[CAVE:CAVE + len(CAVE_BYTES)] != EXPECTED_CAVE:
        raise SystemExit(f"refusing to patch: code cave at 0x{CAVE:X} is not zero-filled")
    patched = bytearray(data)
    patched[PATCH_SITE:PATCH_SITE + len(JUMP_TO_CAVE)] = JUMP_TO_CAVE
    patched[CAVE:CAVE + len(CAVE_BYTES)] = CAVE_BYTES
    DST.write_bytes(patched)
    bak = DST.with_suffix(DST.suffix + ".original")
    if not bak.exists():
        shutil.copyfile(SRC, bak)
    print(f"patched: {DST}")
    print(f"original preserved: {SRC}")
    print(f"backup: {bak}")

if __name__ == "__main__":
    main()
