# Mach8 driver: the method for every change

What to work on is `docs/worklist.md`; this is how each item gets done. Learned on 2026-10-20/21,
when a change that passed the bed hung the real card, and the guide already held every rule it broke.

## 1. Read before writing (static, cheap)

For the path being changed, in this order:

1. **The guide** - `references/ati_mach32/guide.txt` (Mach32 Register Reference, REG688000-15). Every
   register the change writes or relies on: its page, its aliases, what else writes it. Add each rule
   found to `docs/mach8_guide_rules.md` (rule, page, what the driver must do). Check `Mach 32` markings:
   the Mach8 lacks some registers (EXT_GE_STATUS reads 0).
2. **ATIM8.DRV** - `docs/atim8_disassembly.md`, listings in `references/atim8_disasm/` (local only). How
   ATI does the same operation on this card: register order, waits, recovery. It is the floor to beat
   and proof that a sequence works on the hardware.
3. **XFree86 3.3.6 Mach8 server** - `references/xfree86_336_mach8/` (Kevin E. Martin, MIT-style). The
   other working driver: `mach8fs.c` spans, `mach8frect.c` fills and patterns, `mach8line.c`,
   `mach8dsln.c` dashes, `mach8plypt.c` points, `mach8gtimg.c` reads, `mach8im*` image writes and
   stipples, `mach8fcach.c` glyph cache, `regmach8.h` `WaitQueue(n)`.

Write down what was taken, with the file and line or address, in the commit or the doc.

## 2. Write the change

- Follow the rules in `docs/mach8_guide_rules.md`; a cached register must be one the guide says nothing
  else writes.
- Comments state the constraint and cite the page (`guide 9-41`, `ATIM8 seg08:03a5`), never the history.
  Re-read every comment you wrote before committing (CLAUDE.md).
- Build: `drivers\mach8_display\src\build.ps1 -Out $env:TEMP\m8plain`.

## 3. Bed: pixels only

The bed (`vm_5160_mcp`, 86Box `auto-irq9`) checks that pixels did not change. Its speeds do not count:
it charges a port write ~1 us (card 7.64 us), memory 14 cycles a read, and finishes engine work
instantly. It also does not model all of the guide's rules: a bed pass is necessary, not sufficient.

- Write the build into `vm_5160_mcp/card.img` (`tools/fatcp.py`, VM stopped), boot, run
  `C:\M8BENCH.EXE <label>`, fetch `C:\M8BENCH.TXT`, compare with the last good run:
  `python tools/m8_bench_compare.py <log> <good> <label>` - pixels must be `same` unless the change
  means them to move (towards ATI's hash).

## 4. Card: the measurement

`tools\m8_card_cycle.ps1 -Drv $env:TEMP\m8plain\M8DISP.DRV -Name M8XXXX -Label card_xxxx` installs the
driver beside the running one, selects it, reboots (unattended: Logged + CONFIG `FULL`), waits for
the start-up programs to finish (`M8IDLE.EXE`: CPU at most 10 % for 10 s; a bench during them is slow in every test: `card_hatch2`, 10-22), benches once
and compares with ATI's card runs. ATI's were after a `LEAN` boot, ours after `FULL`: not like for like.

- Pass: pixels equal ATI's in every test (or the known exceptions shrink), no engine resets.
- A wedge: the bench call times out. Say "it's hung, can you restart" to the owner; then
  `-FetchOnly` reads the log (its last `started` line names the test). The agent still answers in a
  wedged session: `C:\M8STAT.EXE` reads the engine status (GP_STAT 0200 + SUBSYS_STAT bit 2 = a
  locked FIFO).
- Unexplained slowness or wrong pixels: a diagnostic build that writes its engine resets to
  SCRATCH_PAD_1; M8BENCH prints it per test as `pad D<routine><wait><count>` (see
  `docs/captures/2026-10-21_card/README.md`).

## 4b. Where a call's time goes: profile, then shorten

Every port access flushes the L1 (`docs/captures/io_cache_flush_2026_09_21.md`), and each GDI call
starts after the last call's port writes. So a call's CPU cost is mostly the cache lines it touches -
code, data, stack - at about 1.3 us a 16-byte line. Compiler flags do not change that (`-ox`, `-os`
neutral, 10-22/10-23); touching less does. ATIM8 is compact assembly for this reason.

- Measure with the profile build: `build.ps1 -Prof`. M8BENCH then writes a `prof` line per test: our
  driver's us per operation (`drv_us/op`), calls per operation, each entry point's calls/us (`e<n>`,
  `M8_PF_N` order in `m8.h`), and `hits` (Pixel's short path). The timer costs ~83 us a call (VTD
  emulates the PIT) and is subtracted; never quote a profile build's ops/s.
- Then a short path for the common case: decide from a few cached values, touch little, send the
  writes together, fall through to the full path otherwise. Count that the short path is taken: a
  short path that never runs measures as "no gain" (setpixel, 10-23: `hits 0`, the test changed colour
  each call).
- First result (10-23): setpixel 217 -> 97 us in our driver, ATI's whole draw ~95 us.
- The counters (profile v4, `c<n>` per operation, `M8_STAT_*` in m8.h) say how much bus work a call
  does; time it does not account for is set-up. Polygon (10-23): 197 spans an op at 63 us, of which
  23 us is port writes; style 62 (the span walk alone) put the rest in Output's general path.
- A short path that is never taken measures as no change: check `hits` first. Two misses on 10-23:
  `COLORSOLID` is not set on every solid brush (use `m8_brush_colours`), and a scanline clip holds
  the cursor unless cut to the spans' box first.

## 4c. Is ATI moving less data? Divide by the bytes first

Before reading ATI's code for a data-heavy test, divide ATI's ms/op by the bytes the operation must
move and compare with the card's floor: a 100x100 8bpp upload is 30.0 ms, 3.00 us a byte
(`tools/m8probe/M8UPRATE.C`). ATI at the floor and us above it = our CPU (dib_8bpp, mono_bmp: a RAM
copy or repack before the send). ATI below the floor = it sends less, find how. Then send fewer
bytes than ATI: stretch_dib sends the source once and the engine enlarges it (10 KB against 40 KB).

## 5. Keep it

- Card log to `docs/captures/<date>_card/`, a row per run in its README.
- Commit when the card passes (`m8_cycle.ps1` style: short subject, what and why), push, tick the
  worklist item in the same commit.
- A rule learned goes into `docs/mach8_guide_rules.md`; a method lesson into this file.

## Levers known to beat ATI (from step 1)

| Lever | Source | Status |
|---|---|---|
| Wait for free slots (`WaitQueue(n)` on EXT_FIFO_STATUS), not for an empty FIFO, so the CPU sets up while the engine draws | XFree86 `regmach8.h:286` | tried 10-21 (branch `queue-wip`): correct, 0.97x - most routines still end in an idle wait, so nothing overlaps. Every fixed burst must be subtracted from the count, or the FIFO overruns (first try lost writes in 11 tests; the bed reports its FIFO the other way round and cannot catch this). Redone 10-21 with the trailing waits removed: correct (FIFO entries are interpreted when consumed, guide 8-14), 0.98x - on this bus the engine finishes before the CPU has set up the next operation. Dead as a lever; the cost is port writes and CPU |
| Batch: set up once, then many primitives (5 writes per span) | XFree86 `mach8fs.c:162` | to do |
| Inline port writes (`-oi`): no call per `outpw` | Watcom; ATIM8 writes `out` inline | to do |
| Engine advances PATT_INDEX itself on patterned lines: 2 writes per segment, not 3 | guide 9-33 | to test on the card |
| 8x8 mono pattern (PATT_LENGTH bit 7): any hatch in one blit | guide 9-26/9-60 | **not on the Mach8**: 68800 (Mach32) only, guide 9-26 |
| PATT_LENGTH bits 8, 13, 14 (guide: reserved): ATIM8 writes 4107h/6107h for one-blit diagonals | ATIM8 `seg16:3dd6`, `3e45` | **no effect on our card** (`tools/m8probe/M8PATT.C`, 10-22): rows identical. ATIM8 takes that path only when `[58]`=0 (`seg02:2033`); on ours it uses the per-row routine `3fe9`. Linear mono pattern: index 0 = bit 7 of the low byte, PATT_INDEX reloads each row |
| Hatch: ATIM8's per-row pattern (3 writes a row), then doubling engine copies of the first 8 rows | ATIM8 `seg16:3fe9` + ours | card 316/s vs ATI 298 (was 119), pixels = ATI in all 35 (10-22) |
| LINEDRAW_OPT 000Ch / 070Ch (ATIM8 polylines; bit 3 not in the guide) | port census, ATIM8 `seg08:01e8` | **no effect** (`tools/m8probe/M8OPTS.C`, 10-22): same pixels as 0004h, same ~58 us a line. Runs that took 25x longer were other drawing competing, not the bit |
| Pixel set as ATIM8 does it: DP_CONFIG 4215h, SCAN_TO_X, colour in PIX_TRANS's high byte | ATIM8 `seg07:0066` | **no gain** (M8OPTS): 51 us a pixel either way. A set costs ~590 us a call; the engine's share is 51 us, the rest is per-call path |
| SCAN_TO_X (CAEE): CUR_X + one write draws a one-row span; CUR_Y and pattern byte only when they change | guide 9-61, ATIM8 `seg08:0dcd` | card: pattern and two spans a row correct (`M8PATT s`); used for one-row spans, solid 2-3 writes (ATI 5). **86Box gap**: its SCAN_TO_X starts a mono pattern at bit 7 and ignores PATT_INDEX, so polygon differs in the bed only (10-22) |
| Abort a read with SUBSYS_CNTL 900Fh/500Fh and keep the set-up: masks, scissors, FRGD_COLOR, pitch, DP_CONFIG and ALU_FG_FN survive | ATIM8 `seg09:0194`, `M8ABORT.C` | card: ScanLR 5 writes a stopped scan (ATIM8 7), floodfill 0.68 -> 0.93 (10-23) |
| DIB colours from DIBENG itself: convert one 256x1 DIB of indices 0-255 with the caller's table, then look each pixel up | ours | card: pixels = ATI, dib_8bpp 0.61 -> 0.82, no RAM copy of the DIB (10-23) |
| Enlarge on the card: source once at its own size, engine copies spread columns then rows | ours | card: stretch_dib 1.15, stretchblt 1.01 against 0.45/0.40 (10-23). Whole factors, box unclipped only |
| Mono source unshifted: start the expansion up to 7 px left (X may be -512, guide 8-22), scissors cut it | ours | card: mono_bmp 0.62 -> 0.82 (10-23) |
| Mono bitmaps kept whole in the string cache's bit-planes, drawn by plane blit with both mixes | ours | card: mono_bmp 0.82 -> 2.96 (10-23) |
| Merge collinear polyline points (horizontal, vertical, 45 degrees) | ours | **no gain** (10-23): GDI's ellipse outline points are not unit-step runs; the test made each call slower (3551 -> 4080 us) |
| Memory window into the Mach8's memory through the VGA (ATI Win 3.1 docs: "aperture" in the VGA area) | `ATIDESK/MACH8.WRI:140`, MACH.DRV v2.3 `seg03:02c6` | **not on this card** (10-23): ATI's "Virtual Aperture" serves 4bpp modes on 512 KB boards (SUBSYS_STAT bit 7); `M8APERT.C` finds only the VGA's text page in A0000-BFFFFh |
| Dword OUTs to PIX_TRANS | bus plan A20 | **no** (`M8DWORD.C`, 10-23): the upper word lands in the DAC mask (E2EAh = 02EAh) |
