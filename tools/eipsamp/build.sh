#!/bin/sh
# Build EIPSAMP.EXE, a Win32 console program for Windows 95, as tools/perflog: gcc -m32,
# ld as i386 PE, dlltool import libraries.
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
Sleep@4
GetTickCount@0
GetCommandLineA@0
CreateFileA@28
CloseHandle@4
CreateProcessA@40
WaitForSingleObject@8
SuspendThread@4
ResumeThread@4
GetThreadContext@8
GetCurrentThread@0
SetThreadPriority@8
CreateToolhelp32Snapshot@8
Module32First@8
Module32Next@8
DEF
cat > $B/user32.def <<'DEF'
LIBRARY USER32.dll
EXPORTS
wsprintfA
DEF
cat > $B/winmm.def <<'DEF'
LIBRARY WINMM.dll
EXPORTS
timeBeginPeriod@4
timeEndPeriod@4
DEF

for l in kernel32 user32 winmm; do
    dlltool -m i386 --as-flags=--32 -k -d $B/$l.def -l $B/lib$l.a
done

gcc -m32 -march=i386 -O2 -ffreestanding -fno-builtin -fno-stack-protector \
    -fno-asynchronous-unwind-tables -mno-sse -Wall -x c -c EIPSAMP.C -o $B/eipsamp.o
ld -m i386pe --subsystem console:4.0 --major-os-version 4 -e _start \
    $B/eipsamp.o $B/libkernel32.a $B/libuser32.a $B/libwinmm.a -o $B/EIPSAMP.EXE
strip $B/EIPSAMP.EXE
ls -l $B/EIPSAMP.EXE
