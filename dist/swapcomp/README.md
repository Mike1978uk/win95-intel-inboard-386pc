# SWAPCOMP - compressed swap file for Windows 95

A VxD that LZ4-compresses pages on their way to `WIN386.SWP` and decompresses them on the way
back. On the Intel Inboard 386/PC in an IBM 5160 every byte of disk I/O crosses an 8-bit bus at
about 5.7 us, so a 4 KB page costs ~10 ms to move; compressing it first is cheaper than moving it.

| file | md5 | what |
|---|---|---|
| `SWAPCOMP.VXD` | `9fde4934ed339d33492294ff80824d70` | the driver - use this one |
| `SWAPCMPC.VXD` | `537c5d9a030d9bacd5ce604da758e161` | the same driver plus a checksum on every page, for checking a new machine |

Source and build: [`drivers/swapcomp/`](../../drivers/swapcomp/).

## Install

1. Copy `SWAPCOMP.VXD` to `C:\`.
2. In `C:\WINDOWS\SYSTEM.INI`, under `[386Enh]`, add `device=C:\SWAPCOMP.VXD`.
3. Restart.

**Remove:** delete the line (or put `;` in front of it) and restart. Nothing else is changed: the
swap file is rebuilt every boot, so no compressed page outlives the session.

To see it working, add the `SWAPCOMP` counters in System Monitor: `CompressedWrites` and
`CompressedReads` climb as Windows pages. `Errors` and `Fallback` should stay at 0.

## Measured

On the real 5160 (486BL3, 5 MB, XT-CF boot disk with `XTIDEMP.MPD`), four boots in the order
off, on, off, on, with the same workload first after boot - four programs opened and switched
between, then a 4 MB WinZip zip and unzip:

| | off | on |
|---|---|---|
| whole workload | 297.6 / 294.9 s | 282.7 / 283.0 s |
| change | | **-4.5%**; switching back to a program up to -18% |
| compression ratio, real swap traffic | | 2.64 : 1 |
| locked RAM | | +22 KB |
| Errors / Fallback | | 0 / 0 |

Numbers and raw logs: [`docs/swapcomp_bed_results_2026_10_02.md`](../../docs/swapcomp_bed_results_2026_10_02.md).

## What was not tested

- **Page integrity on real hardware.** `SWAPCMPC.VXD` was run in 86Box only: 8,464 pages read
  back, every checksum matched. On the 5160 only `SWAPCOMP.VXD` ran (no checksum). On a new
  machine, run `SWAPCMPC.VXD` once first and check `ChecksumErrors` stays at 0.
- **Any other machine.** It should work on any Windows 95 whose paging goes through 32-bit file
  access. If it does not - MS-DOS compatibility paging, or a different pager - the driver notices,
  rewrites the page uncompressed and stops compressing for the session (`Fallback` = 1).
- **Speed anywhere else.** On a machine with a fast disk bus the compression costs more than it
  saves: in 86Box, which does not charge the 5160's bus cost, it made the same workload slower.
- **Swap files over 32 MB.** Pages beyond 32 MB are stored uncompressed. It still works; less of
  the file gains.
