#!/bin/sh
# Build TIMERRES.EXE, a Win32 console program for Windows 95, with the
# 64-bit MSYS2 MinGW: gcc -m32 compiles, ld links as i386 PE, and dlltool
# makes the 32-bit import libraries the 64-bit install lacks.
# -x c: the .C suffix would otherwise compile as C++.
# -march=i386: the target is an IBM 486BL3; no CMOV or later.
set -e
cd "$(dirname "$0")"
export PATH=/c/msys64/mingw64/bin:$PATH
B=build
mkdir -p $B

cat > $B/kernel32.def <<'EOF'
LIBRARY KERNEL32.dll
EXPORTS
ExitProcess@4
GetStdHandle@4
WriteFile@20
ReadFile@20
Sleep@4
QueryPerformanceCounter@4
QueryPerformanceFrequency@4
GetTickCount@0
GetCommandLineA@0
CreateFileA@28
SetFilePointer@16
CloseHandle@4
MulDiv@12
EOF
cat > $B/winmm.def <<'EOF'
LIBRARY WINMM.dll
EXPORTS
timeGetTime@0
timeBeginPeriod@4
timeEndPeriod@4
EOF
cat > $B/user32.def <<'EOF'
LIBRARY USER32.dll
EXPORTS
wsprintfA
EOF

for l in kernel32 winmm user32; do
    dlltool -m i386 --as-flags=--32 -k -d $B/$l.def -l $B/lib$l.a
done

gcc -m32 -march=i386 -O2 -ffreestanding -fno-builtin -fno-stack-protector \
    -fno-asynchronous-unwind-tables -mno-sse -Wall -x c -c TIMERRES.C -o $B/timerres.o
ld -m i386pe --subsystem console:4.0 --major-os-version 4 -e _start \
    $B/timerres.o $B/libkernel32.a $B/libwinmm.a $B/libuser32.a -o $B/TIMERRES.EXE
strip $B/TIMERRES.EXE
ls -l $B/TIMERRES.EXE
