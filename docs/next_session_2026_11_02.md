# Next session - from 2026-11-01 (day)

Supersedes `docs/next_session_2026_11_01.md`; its open items 4-6 and the boot stall still stand.
Card log: `docs/captures/2026-11-01_card/m8_card_bench.txt`.

## State: 33 of 35 at or above ATI (median timing)

master `78d944d`. Card: `M8DDM1.DRV` selected (= master).

**Correction, same day:** the morning's "all 35 at or above" was one set of runs under the old total
timing. A control run of identical code (`card_ctl`) read fill_small 0.967 and ellipse 0.988: single
tests dropped 8-15% at random (likely the NIC reading broadcasts over the 8-bit bus). M8BENCH now
also reports the median of ten 100 ms slices (`med`, `967cbef`); `m8_bench_compare.py` uses it.

With it, ATI re-benched (`card_atim2-4`) against master's levers (`card_abcm2-4`, `a517d00`):
**33 of 35 at or above**, pixels = ATI in all 35. Below: **lines_dash 0.914** (real; the total
timing hid it in ATI's 25% spread) and **floodfill 0.983** (4 ops/s: too slow for slices, total used).
Each test's lowest run is within ~2% of its mean under the median; judge on runs 2-4 of 4.

## Done today (all card-benched, pixels = ATI in all 35)

| commit | change | effect |
|---|---|---|
| `0e66ded` | Pixel and BitBlt short paths in aligned assembly (`pixel.asm`, `blt.asm`); MAJ/MIN written only when the size changes (`M8_MM_FORGET`) | setpixel 1.05-1.09; 2 of 6 writes off each setpixel / repeated fill |
| `d26ed72` | RealizeObject skips M8PXREALIZE except bitmap deletion and BS_PATTERN | fill_small 0.95 -> 1.01-1.02; lines_dash ~1.02 |
| `58c8237` | Short paths poll one EXT_FIFO_STATUS bit for room instead of draining empty (ATIM8's way) | dstinvert 0.97 -> 1.03; fill_large 0.985 -> 1.00 |
| `e4b136e` | `M8MAJMN` probe: fills, blits, LINEDRAW keep MAJ/MIN; SCAN_TO_X changes both | basis for the shadow |
| `f95c834` | Card cycle: card's C:\ root was full (512 entries, one .REG per driver); .REG files now in C:\V9XREMOTE\REG | uploads failed with Win32 5 / 82 |

## Disproved / closed (one line each)

- Pixel in assembly alone: no gain (2139/2039); GDI is ~385 of ~485 us a setpixel. The port writes were the lever.
- ~~FIFO room via C: fill_small worse~~ - two noisy runs; the C wait (`m8_fifo_need`) helps the queue users (mem_to_mem +11-14%).
- dstinvert / fill_large are engine-bound: CPU-side trims cannot pass the engine's rate; only overlap could, and now does.

## Retrospective (`a517d00`)

The three levers across the driver: `m8_fifo_need(n)` for every caller that knows its write count, MAJ/MIN
written only on a size change at every fill/blit/host-transfer site (`M8_OUT_SIZE`; M8MAJMN: host write and
read keep them, the extended mono blit does not), getpixel in `pixel.asm`. Against the control (old total
timing, 3 runs each): mem_to_mem +11%, bmp_icon +8%, fill_small +6.5%, lines +5%; lines_hv -9.5% and
stretchblt -5% not yet told apart from noise - recheck both with median timing.
Other drivers: `docs/cross_driver_levers_2026_11_01.md` (worklist item; storage is bus-bound, gains small).

## DirectDraw

Plan `docs/mach8_directdraw_plan.md`; research `docs/directdraw_research_2026_11_01.md`. Milestone 1 (hand-over,
M8DD32.DLL, M8DDTEST) built in `78d944d`, **not run: DirectX is not installed on the card**. Owner to choose
the DirectX version; the bed could take it first.
GDI regression with the milestone-1 driver (`card_ddm1`, median): pixels = ATI in all 35, geometric mean
1.87x; tests move +-3-7% against `card_abcm` both ways; lines_dash 0.86-0.91 stays the one real gap.

## Next

1. DirectX on the bed and/or card (owner), then M8DDTEST: hardware caps without DDCAPS_NOHARDWARE.
2. Milestone 2: primary Lock/Unlock through a RAM shadow; then Blt, then the CRT_OFFSET flip probe (owner at the screen).
3. lines_dash 0.914 and floodfill 0.983 against ATI (median timing).
4. M8BENCH warm-up pass so run 1 counts (ATI re-benched with the same build).
