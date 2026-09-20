"""Flip a built exe's PE subsystem from console (3) to GUI (2).

86Box's own CMakeLists forces CMAKE_WIN32_EXECUTABLE OFF for the SDL
frontend, so a console window opens behind every VM. The cache cannot be
overridden - the force runs on each configure - and changing the link flags
risks the entry point, so the subsystem byte is patched after the link
instead. The entry point stays mainCRTStartup either way; only the console
allocation goes.

Re-run after every rebuild. Idempotent, and refuses anything that is not a
console PE.
"""
import os
import struct
import sys


def patch(path):
    with open(path, "rb") as f:
        d = bytearray(f.read())

    if d[:2] != b"MZ":
        sys.exit(f"FAILED: {path} is not a PE (no MZ)")
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    if d[pe:pe + 4] != b"PE\0\0":
        sys.exit(f"FAILED: {path} has no PE signature at {pe:#x}")

    # Subsystem sits at offset 68 of the optional header, for PE32 and PE32+
    # alike - it precedes the fields whose width differs between them.
    off = pe + 4 + 20 + 68
    cur = struct.unpack_from("<H", d, off)[0]
    if cur == 2:
        print(f"{os.path.basename(path)}: already GUI, nothing to do")
        return 0
    if cur != 3:
        sys.exit(f"FAILED: {path} subsystem is {cur}, expected 3 (console)")

    struct.pack_into("<H", d, off, 2)
    tmp = path + ".tmp"
    with open(tmp, "wb") as f:
        f.write(d)
    os.replace(tmp, path)

    with open(path, "rb") as f:
        f.seek(off)
        back = struct.unpack("<H", f.read(2))[0]
    if back != 2:
        sys.exit(f"FAILED: readback says {back}, not 2")
    print(f"{os.path.basename(path)}: subsystem 3 -> 2 (GUI), verified")
    return 1


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    n = sum(patch(p) for p in sys.argv[1:])
    print(f"Patched: {n}")
