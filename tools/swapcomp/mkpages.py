# Write SWAPPAGE.BIN for LZ4BENCH: 64 non-zero 4 KB pages, evenly spaced through
# a real WIN386.SWP, so the benchmark compresses what this machine pages.
import sys
src = sys.argv[1] if len(sys.argv) > 1 else 'D:/WIN386.SWP'
dst = sys.argv[2] if len(sys.argv) > 2 else 'SWAPPAGE.BIN'
P = 4096
d = open(src, 'rb').read()
nz = [d[i:i+P] for i in range(0, len(d), P) if d[i:i+P].count(0) != P]
pick = [nz[i * len(nz) // 64] for i in range(64)]
open(dst, 'wb').write(b''.join(pick))
print(f'{dst}: {len(pick)} pages from {len(nz)} non-zero in {src}')
