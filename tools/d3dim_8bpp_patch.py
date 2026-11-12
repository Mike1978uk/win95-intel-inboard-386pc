#!/usr/bin/env python3
"""Let DirectX 7's Direct3D accept an 8 bpp hardware driver.

D3DIM.DLL and D3DIM700.DLL (7.0a) clear DDBD_8 from a hardware driver's render depths:
`and byte ptr [eax+0xa1], 0f7h` (80 A0 A1 00 00 00 F7), at 56663de9 in D3DIM and 566636e1 in
D3DIM700, so CreateDevice on an 8 bpp target fails with DDERR_INVALIDOBJECT before the driver is
called. Removing that one instruction is enough: proven in the bed on 2026-11-05
(docs/next_session_2026_11_05.md, docs/captures/2026-11-05_d3d_8bpp/).

Writes a patched copy; never touches the input. Refuses unless the pattern occurs exactly once.

    python tools/d3dim_8bpp_patch.py D3DIM.DLL D3DIM.8BP
"""
import hashlib
import sys

PATTERN = bytes([0x80, 0xA0, 0xA1, 0x00, 0x00, 0x00, 0xF7])
NOPS = bytes([0x90]) * len(PATTERN)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    src, dst = sys.argv[1], sys.argv[2]
    data = open(src, "rb").read()
    n = data.count(PATTERN)
    if n != 1:
        sys.exit("FAILED: pattern found %d times in %s, expected 1. Patched: 0" % (n, src))
    at = data.index(PATTERN)
    out = data[:at] + NOPS + data[at + len(PATTERN):]
    open(dst, "wb").write(out)
    print("%s md5 %s" % (src, hashlib.md5(data).hexdigest().upper()))
    print("%s md5 %s" % (dst, hashlib.md5(out).hexdigest().upper()))
    print("file offset 0x%X. Patched: 1" % at)


if __name__ == "__main__":
    main()
