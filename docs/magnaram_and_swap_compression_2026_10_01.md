# MagnaRAM 97 and swap compression - 2026-10-01

**Result: MagnaRAM 97 makes this machine slower and is not going on the 5160. Compressing the
swap file alone, with an LZ4-class compressor, is worth a design note.**

## MagnaRAM A/B in the bed

Two copies of the same image (`vm_vpicd_irq2`, 26 September), run side by side on the
27 September build (`86box_3c509b/build_log`, `40466b22f`), 5 MB, 486BL3 at 83.5 MHz:

- `vm_magnaram` - Quarterdeck MagnaRAM 97 installed with defaults: compression, TurboLoad,
  CacheBack ("Use available Cache memory") and the tray icon. VIDSTEAL was not installed.
- `vm_magnaram_off` - the image before the install.

Workload: `RAMBASE` (`tools/perflog/`), started from the StartUp group. Pass 2 is the
comparison; pass 1 was a first boot after setup, and Paint Shop Pro could not load on either
side (`CTL3D.DLL` missing from the bed image). Before pass 2 both images got `CTL3D.DLL` and the
5160's `[vcache]` cap (512-1024 KB), which the bed image lacked.

| Pass 2 | without | with MagnaRAM |
|---|---|---|
| desktop to workload start (guest clock) | 1:23 | 3:02 |
| page-ins by workload start | 1,700 | 4,770 |
| locked | 3.55-3.63 MB | 4.05-4.11 MB |
| committed after boot | 8.6 MB | 13.2 MB |
| copies after boot | 5.8 5.5 5.5 s | 12.0 14.4 14.8 s |
| whole workload, start to finish | 4.4 min | 8.9 min |
| CPU | idle between steps | 100% throughout |

- **The clock understates MagnaRAM's cost.** Its guest clock stood still for minutes while
  the machine was busy, so its timings are lower bounds. On the 5160 that much interrupt
  latency would also starve the serial mouse, the NIC and the sound card.
- **It compressed 2.9 MB at 1.86:1 during a window in which the machine paged in ~49 MB.**
  The pool (256 KB) is too small to matter, and the 0.5 MB it locks comes out of the ~3 MB
  that can be paged at all.
- The disk is RAM-speed in the bed, which flatters paging. More page-ins would cost more on
  the 5160, not less, so the verdict carries over without a 5160 run.

Raw: `docs/captures/perflog_bed_2026-10-01_magnaram_pass{1,2}_{with,without}.csv`,
`rambase_bed_2026-10-01_magnaram_*`.

## Swap compression - the arithmetic

The 8-bit bus is the cost. Under Windows the XT-CF moves 385 KiB/s sequential (2026-09-21), so a
4 KB page is ~10.4 ms. That is per byte, not per command: merging requests measured no gain
(#30), so cutting bytes is the only lever left on this path. The CPU has 300-480 cycles per bus
byte to spare.

The 5160's `WIN386.SWP` (32 MB, read from the card on the host, `tools/swapcomp/`): 72% of
pages are all-zero (never used); the other 2,289 pages compress as follows.

| | ratio | est. per page-in |
|---|---|---|
| WKdm (word-based, the macOS family) | 1.51:1 | ~7 ms |
| simple LZ, no entropy coding (LZ4-class) | 2.12:1 (400-page sample) | ~5.5-6.5 ms |
| deflate level 1 (zstd-class) | 2.92:1 | ~8-13 ms |

Ratios are measured. CPU costs are estimates from typical cycles per byte, not measured on
the Blue Lightning. WKdm loses because 57% of words miss its dictionary: Windows 95 here pages
16-bit code, text and bitmaps, not clustered 32-bit pointers.

**Why swap only, not DriveSpace:** the swap file is discarded every boot, so a bug costs a
session, never a file - which matters while #18 is open. Nothing else in the storage stack
changes. The cost: page-ins from program files (EXEs, DLLs) are not helped.

## Stage 1 - SWAPCNT, 2026-10-02

The hook point is `PageFile_Read_Or_Write`: synchronous, `EBX` = one `PageSwapBufferDesc`. The
DDK ships the swap device's source (`BASE\SAMPLES\DYNAPAGE\PAGEFILE.ASM`): it opens the file
with `R0_NO_CACHE`, so swap I/O already bypasses VCACHE. `drivers/swapcomp/` hooks the service,
counts, and passes through; counters appear in PERFLOG as `SWAPCNT\*`.

`vm_magnaram_off` bed, RAMBASE with teardown, `docs/captures/perflog_bed_2026-10-02_swapcnt.csv`:

| | |
|---|---|
| page-ins, all | 12,953 |
| swap pages read | **8,050 (62%)** |
| page-outs / swap pages written | 2,820 / 2,820 (hook sees every write) |
| pages per call | 1.00, reads and writes |
| all-zero pages written | 42 (1.5%) |

- **62% of page-ins are reachable.** The other 38% are EXE and DLL pages.
- **Each written page is read ~2.9 times**, so decompression speed matters most: LZ4-class.
- Estimate for the 5160 baseline (~3,100 page-ins for the four programs): ~1,900 swap reads,
  ~20 s of bus, of which 2.1:1 might save 8-10 s. Unmeasured.

## How far compression goes

Same 400-page sample, LZ4 block format (`tools/swapcomp/lz4est.py`):

| | ratio | bus per 4 KB page |
|---|---|---|
| none | 1:1 | 10.4 ms |
| LZ4 fast | 2.17:1 | 4.8 ms |
| LZ4-HC (searches harder when compressing) | 2.28:1 | 4.6 ms |
| deflate level 1 | 2.92:1 | 3.6 ms, but decompression ~5-10 ms |

LZ4-HC costs more only when compressing, which happens once per page against ~2.9 reads, and
decompresses as fast as LZ4. Use it, but expect little: nearly all the gain is in the first
2:1.

## Was the request-merging null (#30) a fair test?

Fair as an A/B: one byte differs between the arms, same session, 7x RAM so the cache cannot
hide the disk. Narrow in two ways:

- **One workload:** a sequential read of one file to NUL. Writes, many small files and paging
  were not tested.
- **The mechanism was never confirmed.** Nothing showed that larger requests reached the
  driver with `-PhysBreaks 17`; the result doc itself lists "the cap was never binding" as a
  candidate. The null says "no gain on this workload", not "merging cannot help".

For paging specifically it does not matter: SWAPCNT shows one page per call, synchronous, so
there is nothing to merge. Async completion would overlap only the card's per-sector think
time, since with PIO and no IRQ the CPU moves every byte itself.

A fairer re-test, if wanted: count XT-IDE commands per arm in the bed (86Box IDE log), then
repeat the A/B on a mixed workload (RAMBASE, a many-small-files copy, a write).

Next: stage 2, LZ4-class decompression timed on the real CPU; stage 3, the compressing hook.

## Swap file size

Keep 32 MB, minimum = maximum. Windows needs RAM + swap to cover the commit charge: 16.3 MB
peak in RAMBASE, so swap must exceed ~11.3 MB plus headroom. Size costs no bus time; only
used pages move. The "about 8 MB" suggested on 2026-09-30 was sized from pages written, not
from commit, and would have failed the four-program test.
