"""Print an FS3LOG block (loggra.asm) as counts, histograms and per-frame figures.

    python -I tools/fs3gra/decode.py <fs3log.bin>
"""
import statistics
import struct
import sys

NAMES = ["47E present", "482 rows", "486 horizon", "48A rows", "48E mode", "492 screen",
         "496 ram buffer", "49A ?", "49E text", "4A2 line", "4A6 pixel", "4AA vertex",
         "4AE polygon", "4B2 ?", "4B6 read pixel", "4BA shutdown", "4BE ?"]
COLS = ["lines", "vertices", "polygons", "pixels", "text", "row fills", "horizon", "targets"]


def main():
    d = open(sys.argv[1], "rb").read()
    if d[:8] != b"FS3GRA01":
        sys.exit("not an FS3LOG block")
    inits, nframe, nvert, nmode, _ = struct.unpack("<5H", d[8:18])
    o = 18
    orig = struct.unpack("<34H", d[o:o + 68]); o += 68
    cnt = struct.unpack("<17I", d[o:o + 68]); o += 68 + 16
    vhist = struct.unpack("<32H", d[o:o + 64]); o += 64
    modes = struct.unpack("<16H", d[o:o + 32]); o += 32
    verts = struct.unpack("<2048h", d[o:o + 4096]); o += 4096
    frames = struct.unpack(f"<{nframe * 8}H", d[o:o + nframe * 16])
    print(f"inits {inits}, frames kept {nframe}, vertices kept {nvert}, mode sets {nmode}")
    for i, (n, c) in enumerate(zip(NAMES, cnt)):
        print(f"  {n:15} {c:9}   original {orig[2 * i + 1]:04x}:{orig[2 * i]:04x}")
    print("polygons by corners:", {i: v for i, v in enumerate(vhist) if v})
    print("first vertices (AX, CX, DX, polygons before):")
    for k in range(0, min(nvert, 24) * 4, 4):
        print("  ", verts[k:k + 4])
    rows = [frames[i:i + 8] for i in range(0, len(frames), 8)][5:]
    for j, c in enumerate(COLS):
        v = [r[j] for r in rows]
        if v:
            print(f"per frame {c:10} mean {statistics.mean(v):7.1f}  max {max(v):5}")


if __name__ == "__main__":
    main()
