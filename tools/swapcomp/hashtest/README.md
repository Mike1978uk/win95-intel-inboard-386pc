# LZ4BENCH variants: hash cost and table size (#57, #54)

Built, untested on the 5160. Each is `../LZ4BENCH.C` with build options; each prints its options
on its first line and writes its own `<name>.TXT`. Every variant round-trips 64 pages byte-exact in a
host harness (code and text pages from this repo, not swap; ratio 1.30-1.34:1 there).

| file | hash | table | cleared per page | what it answers |
|---|---|---|---|---|
| `LZ4B12` | GCC's shift-and-add multiply | 8 KB | yes | repeats the 2026-10-02 run (sanity check) |
| `LZ4B12I` | **`IMUL`, as `SWAPCOMP.VXD`** | 8 KB | yes | the driver's real compress cost |
| `LZ4B12F` | fold (2 shifts, 2 XORs) | 8 KB | yes | hash cost alone |
| `LZ4B12N` | GCC multiply | 8 KB | no | clearing cost alone |
| `LZ4B12IN` | `IMUL` | 8 KB | no | clearing cost on the driver's hash |
| `LZ4B12FN` | fold | 8 KB | no | both changes |
| `LZ4B11`, `LZ4B10` | GCC multiply | 4, 2 KB | yes | table size against the 16 KB L1 |
| `LZ4B10FN` | fold | 2 KB | no | all three |

Why: `SWAPCOMP.VXD` hashes every position with `imul eax, eax, 9E3779B1h` (13-42 clocks on the 486,
the cost #54 found in the Mach8 driver) and clears its 8 KB table with `rep stosd` for every page.
The 2026-10-02 LZ4BENCH run did not measure the first: GCC built its multiply from 15 shifts and
adds, so `LZ4B12` and the driver differ. `LZ4B12I` is the faithful baseline.

The table need not be cleared: a candidate is followed only if it lies behind the current position,
and its four bytes are compared before it is used, so a stale entry costs nothing but ratio.

## Run on the card

In the folder holding `SWAPPAGE.BIN` from the 2026-10-02 run, nothing else running:

```
LZ4B12 80
LZ4B12I 80
LZ4B12F 80
LZ4B12N 80
LZ4B12IN 80
LZ4B12FN 80
LZ4B11 80
LZ4B10 80
LZ4B10FN 80
```

`LZ4B12` must repeat `docs/captures/lz4bench_5160_2026-10-02.txt` (2.26:1, ~4,712 us compress,
~1,169 us decompress a page) or the run does not count. Each must say `self-test passed`.

## Deciding

Compare against `LZ4B12I`, the driver as it is. Cost per page-in = decompress + 1.96 ms x
compressed sectors + 0.35 x (compress + its sectors' bus time). A variant wins if its compress
saving beats any sectors its lower ratio adds. Then port the winning options to
`drivers/swapcomp/src/SWAPCOMP.ASM` (hash in `LZ_Hash`, the clear before `lzc_loop`, and the
`cand <= pos` guard in the candidate check), checksum build in the bed, then the 2026-10-02 PAGERUN
A/B. `HASH_LOG` is already a build option there.

## Build

From `tools/swapcomp` (Ubuntu `gcc-mingw-w64-i686`), e.g. `LZ4B12FN`:

```
i686-w64-mingw32-gcc -march=i386 -O2 -ffreestanding -fno-builtin -fno-stack-protector \
  -fno-asynchronous-unwind-tables -fno-strict-aliasing -fno-tree-loop-distribute-patterns \
  -mno-sse -Wall -DHASH_LOG=12 -DHASH_FOLD -DNO_CLEAR '-DOUT_NAME="LZ4B12FN.TXT"' -x c LZ4BENCH.C \
  -nostdlib -o hashtest/LZ4B12FN.EXE \
  -Wl,--entry=_start,--subsystem,console:4.0,--major-os-version,4,--minor-os-version,0,--file-alignment,512,--section-alignment,4096,--disable-dynamicbase,--disable-nxcompat,--disable-reloc-section \
  -lkernel32 -luser32 -lgcc
```

`-march=i386` matters: MinGW's default i686 code uses CMOV, which the 486BL3 does not have.
