# Fable review of the Mach8 driver: the brief

When: after the feeder lands (job 4 in `docs/next_session_2026_11_12.md`). The owner approves the start.
Pack: a Sonnet agent assembles the file list and numbers below into one place first, so Fable reads
code, not history.

## How Fable works

- Work in its own git worktree (Agent `isolation: "worktree"`), never on master.
- **Find, then fix**: one commit per finding, each self-contained, subject naming the finding.
  Each commit message states the expected gain and which M8BENCH tests or census fields should move.
- Findings it cannot fix safely (needs a card probe, a register rule unclear in the guide): write
  them up instead, with the probe that would settle it.
- It does not measure. The card decides: Opus benches each commit (`tools\m8_card_cycle.ps1`,
  twice, against `card_gx82`) and keeps or drops it. A commit that loses in any test is reworked or
  dropped, not merged on balance.
- Output: the worktree branch, plus `docs/fable_review_<date>.md`: findings ranked by expected
  gain, each with file:line, evidence, fix commit or proposed probe.

## What it reads (in order)

1. `CLAUDE.md`, `docs/mach8_driver_method.md`, `docs/mach8_guide_rules.md`.
2. Cost model, measured on the card:
   - `docs/captures/2026-11-12_flushprobe/README.md`: every IN/OUT invalidates the whole L1, once
     per burst; refill ~78 ns per byte touched afterwards; ISA memory writes 1.83 us/B, no flush.
   - Port word write ~7.6 us; a 16-byte line touched after a flush ~1.3 us; engine fill ~69 ns/px;
     triangle command 40-60 us (`docs/next_session_2026_11_08.md`, memory notes).
   - Dword OUTs to the Mach8 are dead (upper word hits the DAC mask).
3. Source: `drivers/mach8_display/src/` (16-bit: screen.c, m8hw.c, enable.c, *.asm; 32-bit:
   m8dd32.c, m8d3d.c), `m8.h`, `build.ps1`.
4. Exemplars: ATIM8 listings (`references/atim8_disasm/`, notes `docs/atim8_disassembly.md`),
   ATI Win 3.x v2.3 (`references/mach_w23_disasm/`), XFree86 Mach8 (`references/xfree86_336_mach8/`).
5. Scoreboards: latest `card_gx82` (800x600x8, 1.79x ATI geometric mean, pixels = ATI in all 35)
   and the twist / tunnel census figures.

## What it checks for

### A. Correctness (first; a fast wrong driver is worse than a slow right one)
1. **Caches that can go stale**: every path that writes a bitmap, glyph, brush or surface's RAM must
   invalidate what the card holds of it (bitmap cache `m8_bc_*`, memo, proxies `m8_px_*`, string
   cache, overlays, D3D page boxes). List each writer and whether it invalidates.
2. **Register state assumed but not owned**: any shadowed register another path (GDI, DirectDraw,
   DOS box, VGA half) can change; the guide's rules on which registers survive what.
3. **FIFO accounting**: every burst preceded by enough free-slot waits; no overrun on any path.
4. **Exit and mode-change paths**: feeder, DirectDraw, DOS box switch; nothing writes the card
   after it is no longer ours.
5. Comments that describe code that no longer exists or another device (CLAUDE.md canary rule).

### B. Weight: CPU, cache lines, RAM
6. **Work after each port burst**: data and code touched between bursts, in lines; tables scanned
   on every call (the 11-12 lesson: a 2 KB scan per call cost up to 33 %). Hot paths should touch
   a few lines.
7. **Hot paths in C that should be assembly**, and assembly that is longer than ATIM8's for the
   same job. Measure in bytes of code touched per operation, not instructions.
8. **Fixed RAM**: DGROUP and code-segment size (the plain driver's `_TEXT` sits near 64 KB),
   static tables that could be smaller or built on demand, the DLL's resident size. Low RAM is a
   goal in its own right.
9. Compiler output for the hottest ten functions (`wdis`): spills, far calls, 32-bit maths in
   16-bit code.
10a. **Named target: building a Direct3D frame.** GZ twist census (card boot 369, 286 flips):
   our time 33.9 ms a frame, of which ~22.6 ms is CPU in `m8d3d_replay`'s phases 36-41 (join check
   3.1, seam prep 6.5, joined path 4.1, band fills 5.0, clears 0.9, single triangles 3.0) while the
   card is idle, and 8.0 ms our own synchronous sending. ~3,640 writes a frame. Halving the build
   is worth more than any FIFO change.

### C. Bus
10. **Writes per operation** against ATIM8's for the same operation; any register written when it
    already holds the value; status reads that could be fewer.
11. **Burst shape**: CPU work interleaved with port writes where it could be done first and sent
    together.
12. Pixel data that crosses the bus when it is already on the card.

### D. The card's own silicon (the owner's directive: more on the card, less on bus and CPU)
13. Engine features unused or under-used: GE_OFFSET/GE_PITCH (written once, `m8hw.c:77-79`),
    polygon fill, colour compare, pattern registers, scissors for clipping, SCAN_TO_X, the
    raster counter, plane masks. For each: where it would replace CPU or bus work, and what a probe
    must confirm first. Cross-check `docs/silicon_for_d3d_2026_11_09.md` and `docs/mach8_levers.md`.
14. Off-screen memory use: one allocator for every cache, packing, eviction.

### E. Against ATI
15. The tests below or near ATI (mem_to_mem, text_varied, fill_small, dstinvert in `card_gx82`):
    why, from both listings.

## What it must not do

- Edit master, push, or post anything.
- Propose disabling INBRDPC.SYS, raising the CPU clock, or dword OUTs to the Mach8.
- Trust bed speeds: the bed misprices port writes (`docs/bed_io_cost_model_2026_11_12.md`).
