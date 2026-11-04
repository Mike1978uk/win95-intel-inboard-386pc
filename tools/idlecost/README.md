# IDLECOST.EXE - what background I/O costs a running program (#57)

Built, untested on the 5160. Every port access flushes the L1, and the refill is paid by whatever
runs next (`docs/captures/io_cache_flush_2026_09_21.md`: 90.7 us an access, 5.7 us of it bus).
TIMERRES's CPU loop has a tiny working set, so it barely sees that refill; a real program does.
IDLECOST walks 1, 4, 8 and 12 KB working sets (one read per 16-byte line) and reports passes per
second for each, at normal priority so every background source runs as usual.

```
IDLECOST <label> [seconds per size, default 5]
```

Appends to `C:\IDLECOST.TXT`. Run it once per state, changing one background source at a time,
three times each, nothing else running (close the agent if it can be left idle):

| label | state |
|---|---|
| `base` | as the card is normally used |
| `nolan` | network cable out (3C509B traffic gone) |
| `noext` | external SCSI chain powered off |
| `tick` | `MinTimeSlice` removed from `SYSTEM.INI` (the default 13.7 ms tick); a known +6%, here as the yardstick |

A source whose removal raises the 12 KB figure much more than the 1 KB figure is paid in refills.

Build (Ubuntu `gcc-mingw-w64-i686`):

```
i686-w64-mingw32-gcc -Os -Wall -Wextra -march=i386 -mno-sse -ffreestanding -fno-builtin \
  -fno-tree-loop-distribute-patterns -fno-stack-protector -fno-asynchronous-unwind-tables \
  -nostdlib -o IDLECOST.EXE idlecost.c \
  -Wl,--entry=_entry,--subsystem,console:4.0,--major-os-version,4,--minor-os-version,0,--major-image-version,1,--file-alignment,512,--section-alignment,4096,--disable-dynamicbase,--disable-nxcompat,--disable-reloc-section \
  -lkernel32 -luser32 -lgcc
```

`-march=i386` matters: MinGW's default i686 code uses CMOV, which the 486BL3 does not have.
Imports only KERNEL32 and `USER32!wvsprintfA`.
