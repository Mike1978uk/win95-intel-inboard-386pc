# DAYNAPRB.EXE - DaynaPORT over Windows 95's ASPI layer (#50)

Built, untested on the 5160. Proves from ring 3 what a protected-mode NDIS driver for the
BlueSCSI DaynaPORT will do from ring 0, and measures the cost of reading at fixed intervals.
Design: `cloud/bluescsi/DESIGN.md` §3 (branch `claude/bluescsi-5160-firmware-o12wij`); test:
`cloud/bluescsi/REPORT.md` test 4.

```
DAYNAPRB [ha target lun] [-arp a.b.c.d] [-s seconds]
```

Steps: INQUIRY (or scan LUN 0 for vendor `Dayna`), MAC (09h), enable (0Eh), optional ARP
probe out and reply in, then READ (08h, multi-frame) at 0/14/28/55/110/220 ms intervals for
`-s` seconds each (default 10), then disable. Output to the console and `C:\DAYNAPRB.TXT`.
Timings use `QueryPerformanceCounter` (1,193,180 Hz on the 5160).

Imports only KERNEL32 and `USER32!wvsprintfA`, all present in Windows 95; no C runtime.

Build (MinGW, Ubuntu `gcc-mingw-w64-i686`):

```
i686-w64-mingw32-gcc -Os -Wall -Wextra -ffreestanding -fno-builtin \
  -fno-tree-loop-distribute-patterns -fno-stack-protector -fno-asynchronous-unwind-tables \
  -nostdlib -o DAYNAPRB.EXE daynaprb.c \
  -Wl,--entry=_entry,--subsystem,console:4.0,--major-os-version,4,--minor-os-version,0,--major-image-version,1,--file-alignment,512,--section-alignment,4096,--disable-dynamicbase,--disable-nxcompat,--disable-reloc-section \
  -lkernel32 -luser32 -lgcc
```
