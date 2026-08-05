import struct

SRC = "../inbrdpc_fixed_v2.bin"
DST = "../inbrdpc_fixed_v4_int15shim.bin"
SHIM = "int15_shim.bin"

data = bytearray(open(SRC, "rb").read())
shim = open(SHIM, "rb").read()

APPEND_OFFSET = 0xC6A3
assert len(data) == APPEND_OFFSET, f"expected file to end exactly at {APPEND_OFFSET:#x}, got {len(data):#x}"

# --- patch 1: detour INIT's first instruction (MOV DX,0x670, 3 bytes) into a
# same-length CALL to init_detour (also 3 bytes) ---
DETOUR_SITE = 0xA731
orig = bytes(data[DETOUR_SITE:DETOUR_SITE + 3])
assert orig == bytes.fromhex("e8aaff"), f"unexpected bytes at detour site: {orig.hex()}"
rel16 = (APPEND_OFFSET - (DETOUR_SITE + 3)) & 0xFFFF
data[DETOUR_SITE:DETOUR_SITE + 3] = bytes([0xE8]) + struct.pack("<H", rel16)
print(f"patch 1: CALL rel16={rel16:#06x} -> target {APPEND_OFFSET:#x}")

# --- patch 2: grow the existing resident-size LEA (LEA AX,[imm16], 4 bytes)
# to cover the new appended shim, with a small margin ---
RESIDENT_SIZE_SITE = 0xA756
orig2 = bytes(data[RESIDENT_SIZE_SITE:RESIDENT_SIZE_SITE + 4])
assert orig2 == bytes.fromhex("8d06b0c6"), f"unexpected bytes at resident-size site: {orig2.hex()}"
new_end = APPEND_OFFSET + len(shim)
new_resident_size = (new_end + 0x20) & 0xFFF0  # round up, small margin
data[RESIDENT_SIZE_SITE + 2:RESIDENT_SIZE_SITE + 4] = struct.pack("<H", new_resident_size)
print(f"patch 2: resident size {0xc6b0:#06x} -> {new_resident_size:#06x} (shim ends at {new_end:#x})")

# --- append the shim itself ---
data += shim
print(f"appended {len(shim)} bytes at {APPEND_OFFSET:#x}, new file size {len(data):#x}")

open(DST, "wb").write(data)
print(f"wrote {DST} ({len(data)} bytes)")
