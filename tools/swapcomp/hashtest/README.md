# LZ4BENCH with smaller hash tables (#57)

Built, untested on the 5160. `LZ4B10.EXE`, `LZ4B11.EXE`, `LZ4B12.EXE` are `../LZ4BENCH.C` built with
a 10-, 11- and 12-bit hash table (2, 4 and 8 KB). 12 is what `SWAPCOMP.VXD` uses today. The table
shares the 486BL3's 16 KB L1 with the 4 KB page being compressed and its output, so a smaller table
may compress faster at a small cost in ratio. Each run prints both.

Run on the card, in the folder that holds `SWAPPAGE.BIN` from the 2026-10-02 run:

```
LZ4B12 80
LZ4B11 80
LZ4B10 80
```

They write `LZ4B12.TXT`, `LZ4B11.TXT`, `LZ4B10.TXT`. `LZ4B12` should repeat
`docs/captures/lz4bench_5160_2026-10-02.txt` (ratio 2.26:1, compress 4,712 us, decompress 1,169 us
a page).

Deciding: cost per page-in = decompress + bus for the compressed sectors + 0.35 x (compress + its
bus), at ~1.96 ms a sector (`XTIDETR.ASM`, 3.82 us a byte). A smaller table wins if its compress
saving outweighs the extra sectors its lower ratio sends. If one does, build `SWAPCOMP.VXD` with
`-DHASH_LOG=<n>` added to its ML line (`drivers/swapcomp/build.ps1`) and repeat the 2026-10-02 A/B.

Build (Ubuntu `gcc-mingw-w64-i686`), from `tools/swapcomp`:

```
i686-w64-mingw32-gcc -march=i386 -O2 -ffreestanding -fno-builtin -fno-stack-protector \
  -fno-asynchronous-unwind-tables -fno-strict-aliasing -fno-tree-loop-distribute-patterns \
  -mno-sse -Wall -DHASH_LOG=10 '-DOUT_NAME="LZ4B10.TXT"' -x c LZ4BENCH.C -nostdlib \
  -o hashtest/LZ4B10.EXE \
  -Wl,--entry=_start,--subsystem,console:4.0,--major-os-version,4,--minor-os-version,0,--file-alignment,512,--section-alignment,4096,--disable-dynamicbase,--disable-nxcompat,--disable-reloc-section \
  -lkernel32 -luser32 -lgcc
```

`-march=i386` matters: MinGW's default i686 code uses CMOV, which the 486BL3 does not have.
