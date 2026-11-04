# XTSECT.EXE - per-command cost of the disk driver (#57)

Built, untested on the 5160. Times absolute sector reads through Windows 95's INT 25h service
(VWIN32), which reaches the disk through IOS and the miniport, so two driver builds can be
compared by their cost per command. **Read only: it never writes a sector.**

```
XTSECT <label> [drive] [count] [start] [stride]
```

Defaults: drive C, 500 reads per pass, from logical sector 20000, 37 sectors apart (at least 16).
Each of three passes reads its own range one sector at a time, then eight at a time. Appends to
`C:\XTSECT.TXT`. A line marked `(cached?)` came back faster than 512 bytes can cross the bus
(~2 ms a sector) and does not measure the driver.

Used by `docs/xtcf_taskfile_switches_2026_11_04.md`.

Build (Ubuntu `gcc-mingw-w64-i686`):

```
i686-w64-mingw32-gcc -Os -Wall -Wextra -march=i386 -mno-sse -ffreestanding -fno-builtin \
  -fno-tree-loop-distribute-patterns -fno-stack-protector -fno-asynchronous-unwind-tables \
  -nostdlib -o XTSECT.EXE xtsect.c \
  -Wl,--entry=_entry,--subsystem,console:4.0,--major-os-version,4,--minor-os-version,0,--major-image-version,1,--file-alignment,512,--section-alignment,4096,--disable-dynamicbase,--disable-nxcompat,--disable-reloc-section \
  -lkernel32 -luser32 -lgcc
```

`-march=i386` matters: MinGW's default i686 code uses CMOV, which the 486BL3 does not have.
