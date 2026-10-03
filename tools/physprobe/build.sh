#!/bin/sh
# Build PHYSPROBE.COM (or the program named as the argument, e.g. ROMPROBE) with binutils:
# O(x) is expanded to a CS-relative offset (ORG 100h) first.
cd "$(dirname "$0")"
N=${1:-PHYSPROBE}
BIN=/c/msys64/mingw64/bin
python -c "import re,sys;s=open(sys.argv[1]+'.S').read();open('build/p.s','w').write(re.sub(r'\bO\(([^()]*)\)',r'(\1 - _start + 0x100)',s))" "$N" || exit 1
$BIN/as.exe -o build/p.o build/p.s || exit 1
$BIN/objcopy.exe -O binary -j .text build/p.o build/$N.COM || exit 1
ls -l build/$N.COM
