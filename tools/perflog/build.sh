#!/bin/sh
# Build PERFLOG.EXE, a Win32 console program for Windows 95, the same way as
# tools/timerres: gcc -m32, ld as i386 PE, dlltool import libraries.
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
SetFilePointer@16
CloseHandle@4
CreateProcessA@40
WaitForSingleObject@8
DeleteFileA@4
DEF
cat > $B/advapi32.def <<'DEF'
LIBRARY ADVAPI32.dll
EXPORTS
RegOpenKeyExA@20
RegEnumValueA@32
RegQueryValueExA@24
RegCloseKey@4
DEF
cat > $B/user32.def <<'DEF'
LIBRARY USER32.dll
EXPORTS
wsprintfA
EnumWindows@8
GetWindowTextA@12
IsWindowVisible@4
PostMessageA@16
WaitForInputIdle@8
SendMessageTimeoutA@28
SetForegroundWindow@4
RedrawWindow@16
GetUpdateRect@12
EnumChildWindows@12
DEF

for l in kernel32 advapi32 user32; do
    dlltool -m i386 --as-flags=--32 -k -d $B/$l.def -l $B/lib$l.a
done

gcc -m32 -march=i386 -O2 -ffreestanding -fno-builtin -fno-stack-protector \
    -fno-asynchronous-unwind-tables -mno-sse -Wall -x c -c PERFLOG.C -o $B/perflog.o
ld -m i386pe --subsystem console:4.0 --major-os-version 4 -e _start \
    $B/perflog.o $B/libkernel32.a $B/libadvapi32.a $B/libuser32.a -o $B/PERFLOG.EXE
strip $B/PERFLOG.EXE
ls -l $B/PERFLOG.EXE

gcc -m32 -march=i386 -O2 -ffreestanding -fno-builtin -fno-stack-protector \
    -fno-asynchronous-unwind-tables -mno-sse -Wall -x c -c SLEEP.C -o $B/sleep.o
ld -m i386pe --subsystem console:4.0 --major-os-version 4 -e _start \
    $B/sleep.o $B/libkernel32.a -o $B/SLEEP.EXE
strip $B/SLEEP.EXE
ls -l $B/SLEEP.EXE

gcc -m32 -march=i386 -O2 -ffreestanding -fno-builtin -fno-stack-protector     -fno-asynchronous-unwind-tables -mno-sse -Wall -x c -c WINCLOSE.C -o $B/winclose.o
ld -m i386pe --subsystem console:4.0 --major-os-version 4 -e _start     $B/winclose.o $B/libkernel32.a $B/libuser32.a -o $B/WINCLOSE.EXE
strip $B/WINCLOSE.EXE
ls -l $B/WINCLOSE.EXE

gcc -m32 -march=i386 -O2 -ffreestanding -fno-builtin -fno-stack-protector     -fno-asynchronous-unwind-tables -mno-sse -Wall -x c -c PAGETIME.C -o $B/pagetime.o
ld -m i386pe --subsystem console:4.0 --major-os-version 4 -e _start     $B/pagetime.o $B/libkernel32.a $B/libuser32.a -o $B/PAGETIME.EXE
strip $B/PAGETIME.EXE
ls -l $B/PAGETIME.EXE
