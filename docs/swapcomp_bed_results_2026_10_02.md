# SWAPCOMP in the bed - 2026-10-02

Stage 3 of swap compression (`docs/magnaram_and_swap_compression_2026_10_01.md`).
First runs of `drivers/swapcomp/`. Raw files: `docs/captures/2026-10-02_swapcomp_bed/`.

## Setup

| | |
|---|---|
| bed | `vm_magnaram_off` (the MagnaRAM A/B bed), config md5 `95362f64` |
| 86Box | `86box_3c509b/build_log`, built 2026-09-27 at its HEAD `40466b22f` |
| workload | `PAGERUN.BAT` from StartUp: PERFLOG for 300 s + `PAGETIME.EXE` |
| `SWAPCMPC.VXD` | `537c5d9a` - compression + per-page checksum |
| `SWAPCOMP.VXD` | `9fde4934` - compression |
| `PAGETIME.EXE` | `b693fad2` |

PAGETIME opens PSP, WordPad, Notepad and Paint in turn, then switches back to each three
times; each step is timed until the window answers and has nothing left to paint. This
image has no WinZip, so its zip steps log -1.

A and B ran at the same time in two copies of the bed, each cloned from one cleanly shut
down image; the only difference was the `device=` line. Speed log (`AB_speed.txt`): B 20/20
samples at 100%, A 19/20 (one 95% sample early in boot).

## Correctness: passed

Checksum build (run C1): 4,316 pages written compressed, 8,464 read back, every one
matching its checksum. Errors 0, ChecksumErrors 0, Fallback 0, Contention 0. Ratio 2.25:1
(4.44 of 8 sectors saved per compressed page), against LZ4BENCH's 2.26:1 on the 5160.
Locked RAM +32 KB against SWAPCNT on the same bed (first sample, 3,727,360 -> 3,760,128).

## Timing: slower in the bed

| step (ms) | A off | B SWAPCOMP | C1 checksum | B vs A |
|---|---|---|---|---|
| open, 4 programs | 90,215 | 116,865 | 122,982 | +30% |
| switch 1 | 13,822 | 16,056 | 17,844 | +16% |
| switch 2 | 14,539 | 16,915 | 16,886 | +16% |
| switch 3 | 15,523 | 20,111 | 18,302 | +30% |
| page-ins (PERFLOG, run) | 16,855 | 19,028 | 18,540 | +13% |
| locked RAM (first sample) | 3,727,360 | 3,743,744 | 3,760,128 | +16 KB |

B: 3,955 compressed writes, 8,326 compressed reads, Errors 0, Fallback 0.

B lost ~36 s across all steps. At the 5160's measured CPU cost (4.71 ms compress, 1.17 ms
decompress) its 3,955 + 8,326 operations cost ~29 s. So the bed charges the compression in
full and refunds almost no I/O: it does not model the 5160's ~10.4 ms of bus per page, the
cost this driver exists to cut. On the 5160 the saving would be ~5.8 ms per page moved,
~71 s over these transfers, against ~29 s of CPU. **Estimate only; the 5160 decides.**

Open question: B paged 13% more than A. 16 KB of locked RAM does not obviously explain it,
and one run per arm gives no spread. Watch the page-in count on the 5160.

## Bed differences from the 5160

No NIC, T130B without IRQ, no LS-120, MO or B: drive, 16 colours (the 5160 runs 256, so
its bitmaps are twice the size and it pages more). Identical in both arms, so the A/B is
fair; the absolute numbers are not the 5160's.

Building a fuller bed from the current card image was tried and abandoned the same day:
B: as a 1.2 MB drive drops the stock BIOS to ROM BASIC (it needs the Sergey floppy BIOS),
the image raises Date/Time and display-fallback dialogs, and this 86Box build overwrites
`mouse_input_mode_initial` at start-up (clicks go to the tablet path), with or without the
LS-120 and MO. Not pursued.

## Next: the 5160

Steps in `docs/next_session_2026_10_02.md`, section 3.
