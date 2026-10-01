#!/bin/sh
# Build LZ4BENCH.EXE, a Win32 console program for Windows 95, the same way as
# tools/perflog: gcc -m32 without a C runtime, ld as i386 PE, dlltool imports.
# -fno-tree-loop-distribute-patterns: without a runtime there is no memset
# for gcc to turn a clearing loop into.
set -e
cd "$(dirname "$0")"
export PATH=/c/msys64/mingw64/bin:$PATH
B=build
mkdir -p $B

cat > $B/kernel32.def <<'DEF'
LIBRARY KERNEL32.dll
EXPORTS
ExitProcess@4
GetStdHandle@4
WriteFile@20
ReadFile@20
CreateFileA@28
CloseHandle@4
GetCommandLineA@0
QueryPerformanceCounter@4
QueryPerformanceFrequency@4
GetCurrentThread@0
SetThreadPriority@8
MulDiv@12
DEF
cat > $B/user32.def <<'DEF'
LIBRARY USER32.dll
EXPORTS
wsprintfA
DEF
for l in kernel32 user32; do
    dlltool -m i386 --as-flags=--32 -k -d $B/$l.def -l $B/lib$l.a
done

gcc -m32 -march=i386 -O2 -ffreestanding -fno-builtin -fno-stack-protector \
    -fno-asynchronous-unwind-tables -fno-strict-aliasing \
    -fno-tree-loop-distribute-patterns -mno-sse -Wall -x c -c LZ4BENCH.C -o $B/lz4bench.o
ld -m i386pe --subsystem console:4.0 --major-os-version 4 -e _start \
    $B/lz4bench.o $B/libkernel32.a $B/libuser32.a -o $B/LZ4BENCH.EXE
strip $B/LZ4BENCH.EXE
ls -l $B/LZ4BENCH.EXE
