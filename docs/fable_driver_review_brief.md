# Fable review of what runs on the 5160: the brief

## Revised checklist (owner, 2026-11-13 night): supersedes the order below

Fable spent a 5-hour limit in minutes, twice, mostly reading in. From now on:

1. **One Fable job at a time, one question per job.** The owner starts each one. A job cut off by
   the limit is not resumed; it is re-scoped and started fresh.
2. **Pack first, by a cheaper agent.** Sonnet writes `docs/fable_pack_<job>.md`: the question, the
   measured facts (numbers only, with their source), and the exact files and line ranges to read,
   no more than ~1,500 source lines. Opus checks the pack. Fable reads the pack and those lines,
   nothing else unless a line it reads points somewhere.
3. **Fable reasons; it does not build, run or commit.** Output is one report: at most 5 findings,
   ranked by expected gain, each with file:line, the evidence, a proposed patch as a diff in the
   report, and the card probe that would prove it. Opus applies, builds and bed/card-tests;
   Haiku commits.
4. **Stop rule in every prompt:** "When you have 5 findings or have read the pack's lines, write the
   report and stop."

| # | Job | Status |
|---|---|---|
| F1 | **CPU flush on I/O**: what invalidates the L1 on every IN/OUT (Inboard glue, BL3 config, INBRDPC.SYS), whether it can be narrowed without breaking DMA coherency, and driver-side avoidance beyond the known (bursts built in RAM, VGA-half memory writes) | **closed 11-13** without Fable: SW1 measured, no effect; flush fixed in the module (`docs/fable_pack_f1_flush.md`) |
| F2 | **Direct3D frame build**: `m8d3d_replay` phases 36-41, ~22.6 ms CPU a twist frame of 32.7 ms ours; including asm candidates in that path | next; pack includes the flush cost model for avoidance |
| F3 | **Mach8 correctness**: Part 1 items A1-A4 (stale caches, unowned register state, FIFO accounting, exit paths) incl. `m8feed` | after F2, if the owner wants it |
| - | Part 2 swapcomp, Part 3 XT-IDE | **done 11-13**: reports on their worktree branches; Opus validates, bed then card |
| - | Parts 4-8 (SHADRAM, T130B, patched MS drivers, real mode, scripts) | **not Fable work** unless the owner asks; Opus can use the shared checklist below |

Facts settled since the first draft (put in packs, not re-derived): at 16 bpp in twist every feed
stop is at Flip and Flip has ~23 writes left to send (card, 11-13, boot 381); feed stop points are
not a cost.

## Original brief (11-13 afternoon): reference for the checklists
Scope (owner, 2026-11-13): everything of ours that runs on the 5160, drivers and VxDs first, then the
real-mode programs and the scripts that patch a user's files. Not 86Box code, not probes or benches.
Pack: a Sonnet agent assembles each part's file list and numbers into one place first, so Fable reads
code, not history.

## Order: one Fable pass per part, each in its own worktree and report

| Part | Component | On the card (read 2026-11-13) | Source |
|---|---|---|---|
| 1 | Mach8 display driver (DRV, `M8DD32.DLL`, Direct3D HAL) and `m8feed` | display driver; feeder not yet loaded | `drivers/mach8_display/`, `drivers/m8feed/` |
| 2 | swapcomp | `DEVICE=c:\swapcomp.vxd` (`SWAPZF` build) | `drivers/swapcomp/` |
| 3 | XT-IDE miniport | `IOSUBSYS\XTIDEMP.MPD` (`XTBOTH` build) | branch `claude/xtcf-tight-taskfile` `c06907a`, not master |
| 4 | Shadow RAM | `DEVICE=C:\SHADRAM.VXD` | `drivers/shadowram/` |
| 5 | Trantor T130B miniport | `IOSUBSYS\T130.MPD` | `drivers/trantor_t130b/` |
| 6 | Patched Microsoft drivers: `HSFLOP.PDR`, `SCSIPORT.PDR`, `ELNK3.VXD` (IRQ 9 to 2, no multicast), `MSSBLST.VXD`, `LPT`, `QIC117` | the pack lists which are on the card | `vxd-patches/`, `dist/post-install-fixes/`, `drivers/3c509b/` |
| 7 | Real mode: `IVT68FIX.COM`, `WAIT86.COM`, `REFR64/18.COM`, the boot menu (`CONFIG.SYS` + `AUTOEXEC.BAT` renames), `CPUSET.BAT`/`IBM486.CFG`, `SCSICACH.EXE` | all but SCSICACH run at every boot | `ivt68fix/`, `tools/bootmenu/`, `dist/` |
| 8 | Patch scripts that write a user's binaries | run on the host | `dist/post-install-fixes/scripts/` |

Out unless the owner adds them: `NMIWATCH.VXD`, `SHADRAMD.VXD`, `SWAPCMPC.VXD` (on C:\, not loaded),
`custom_vkd` (SYSTEM.INI loads the stock `*vkd`), retired `imation_ls120_mpd`, Intel's `INBRDPC.SYS`.

The pack's first job for every part: MD5 the file on the card, the file in `dist/`, and a build of the
committed source. Where they differ, that is finding zero for the part, and the review covers the
card's version. Known 11-13: the card runs `SWAPZF` (master) and `XTBOTH` (the branch above), neither in
`dist/`. A PE's code hash changes with the build folder: compare at the original path or by bytes.

## Checks for every part (Parts 2-8; Part 1 has its own list below)

1. **Contract**: every value Windows, IOS, VMM or DirectDraw checks on what the component reports, and
   the error it returns; what happens on each failure path, including absent or dead hardware.
2. **DMA reach**: any buffer a DMA controller sees sits below the XT's 1 MB page-register reach
   (`win9x-dma-driver-audit` skill).
3. **XT port aliasing**: no access that decodes onto the 8259 (22h/23h), DMA page registers or another
   card's range at XT 10-bit decode.
4. **Interrupts and reentrancy**: work at interrupt time, IRQ 2/9 routing, what is locked, what can be
   re-entered by the next interrupt or a DOS box.
5. **Locked RAM and time on the bus**: pages locked, work per I/O, polling loops (every IN/OUT flushes
   the L1).
6. **Comments as canary** (CLAUDE.md): comments describing another device, dead code, a fixed bug.
7. **Patches** (Parts 6 and 8): each site found by signature, refused on a mismatch, `Patched: N` with
   N > 0; the reason for each changed byte written next to it.

## Part 1: the Mach8 driver

### How Fable works

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

### What it reads (in order)

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

### What it checks for

#### A. Correctness (first; a fast wrong driver is worse than a slow right one)
1. **Caches that can go stale**: every path that writes a bitmap, glyph, brush or surface's RAM must
   invalidate what the card holds of it (bitmap cache `m8_bc_*`, memo, proxies `m8_px_*`, string
   cache, overlays, D3D page boxes). List each writer and whether it invalidates.
2. **Register state assumed but not owned**: any shadowed register another path (GDI, DirectDraw,
   DOS box, VGA half) can change; the guide's rules on which registers survive what.
3. **FIFO accounting**: every burst preceded by enough free-slot waits; no overrun on any path.
4. **Exit and mode-change paths**: feeder, DirectDraw, DOS box switch; nothing writes the card
   after it is no longer ours.
5. Comments that describe code that no longer exists or another device (CLAUDE.md canary rule).

#### B. Weight: CPU, cache lines, RAM
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

#### C. Bus
10. **Writes per operation** against ATIM8's for the same operation; any register written when it
    already holds the value; status reads that could be fewer.
11. **Burst shape**: CPU work interleaved with port writes where it could be done first and sent
    together.
12. Pixel data that crosses the bus when it is already on the card.

#### D. The card's own silicon (the owner's directive: more on the card, less on bus and CPU)
13. Engine features unused or under-used: GE_OFFSET/GE_PITCH (written once, `m8hw.c:77-79`),
    polygon fill, colour compare, pattern registers, scissors for clipping, SCAN_TO_X, the
    raster counter, plane masks. For each: where it would replace CPU or bus work, and what a probe
    must confirm first. Cross-check `docs/silicon_for_d3d_2026_11_09.md` and `docs/mach8_levers.md`.
14. Off-screen memory use: one allocator for every cache, packing, eviction.

#### E. Against ATI
15. The tests below or near ATI (mem_to_mem, text_varied, fill_small, dstinvert in `card_gx82`):
    why, from both listings.

## What Fable must not do (every part)

- Edit master, push, or post anything.
- Propose disabling INBRDPC.SYS, raising the CPU clock, or dword OUTs to the Mach8.
- Trust bed speeds: the bed misprices port writes (`docs/bed_io_cost_model_2026_11_12.md`).
