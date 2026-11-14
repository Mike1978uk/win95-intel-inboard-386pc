; LOOPCAL.EXE - time the XT-IDE miniport's register-only pacing loop on this CPU.
;
; XTIDETR.ASM converts every wait and timeout from loop passes at a fixed
; 0.22 us a pass. This runs the same instructions (dec ecx / jnz) for
; 10,000,000 passes, three times, against QueryPerformanceCounter (the PIT,
; 1.193 MHz on Windows 95), and prints ns per pass. If it is not ~220 ns,
; the driver's waits are off by the same factor.
;
; Assemble: fasm LOOPCAL.ASM LOOPCAL.EXE

format PE console 4.0
entry start
include 'win32a.inc'

PASSES = 10000000

section '.text' code readable executable
start:
        invoke  GetStdHandle, STD_OUTPUT_HANDLE
        mov     [hout], eax
        invoke  QueryPerformanceFrequency, freq
        mov     ebx, 3
again:
        invoke  QueryPerformanceCounter, t0
        mov     ecx, PASSES
        align   16
pace:   dec     ecx
        jnz     pace
        invoke  QueryPerformanceCounter, t1
        mov     eax, dword [t1]
        sub     eax, dword [t0]
        ; ns per pass = ticks * 1e9 / (freq * PASSES) = ticks * 100 / freq
        mov     ecx, 100
        mul     ecx
        div     dword [freq]
        mov     [nsx], eax
        mov     eax, dword [t1]
        sub     eax, dword [t0]
        cinvoke wsprintf, buf, fmt, eax, [nsx]
        invoke  WriteFile, [hout], buf, eax, written, 0
        dec     ebx
        jnz     again
        invoke  ExitProcess, 0

section '.data' data readable writeable
fmt     db      'LOOPCAL %lu ticks for 10,000,000 passes = %lu ns a pass (driver assumes 220)', 13, 10, 0
hout    dd      0
written dd      0
nsx     dd      0
freq    dq      0
t0      dq      0
t1      dq      0
buf     rb      128

section '.idata' import data readable
library kernel32, 'KERNEL32.DLL', user32, 'USER32.DLL'
import  kernel32, GetStdHandle, 'GetStdHandle', QueryPerformanceFrequency, 'QueryPerformanceFrequency', \
        QueryPerformanceCounter, 'QueryPerformanceCounter', WriteFile, 'WriteFile', ExitProcess, 'ExitProcess'
import  user32, wsprintf, 'wsprintfA'
