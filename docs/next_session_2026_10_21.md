# Next session - from 2026-10-20

Supersedes `docs/next_session_2026_10_19.md`.

## Done 10-20

- **DIBENG polygon fault fixed** (`097fc8a`). GDI fills polygons with `Output` style 5,
  `OS_POLYSCANLINE`, and a NULL clip. Our `Output` gave it a full-screen strip, shifted its records as
  points, and read `wCount` as points; it counts records (each: n, top, bottom, n x values, n). The
  DIB Engine does not clip scan rows to the device, so it wrote past the strip's selector limit
  (fault EDX = 268 x 800, EDI = EDX + 421: screen pixel 421,268). Found by a tagged copy of the last
  `Output` call in driver data, read from the faulted guest with `find_memory` (86box-mcp). M8BENCH
  now runs all 35; tests 31-35 hash equal to ATI's. The same bounding for `OS_SCANLINES` is in but
  no bench test sends that style.
- **Bench bed on the MCP:** `vm_5160_mcp` (copy of `vm_5160_now`, gitignored), build `86box_auto`
  branch `auto-irq9` = master `5468d5952` + `ef082884b` (IRQ 9 delivered as IRQ 2 on the XT; without
  it the 3C509B never interrupts). Image holds the plain `097fc8a` build. Run `mcp6` (diag build, same
  logic): `C:\M8BENCH.TXT` in the image; polygon 11.3/s, floodfill 1.9/s, ellipse 14.5/s.
- **V9x Remote Agent** (Michael Dale) installed in `vm_5160_mcp`: 3C509B + TCP/IP added by the owner,
  SLiRP forward 9869, host firewall blocks 9869 inbound. `v9x` MCP connected: ping, info, shell work.
  Boot shows a Microsoft Networking password prompt (Client for Microsoft Networks); Enter clears it.
  Removing that client would make boots unattended.
- Issue #50 posted: BlueSCSI as a SCSI bridge (storage, DaynaPort, USB), parked.

- **Shutdown BSOD fixed** (fatal exception 0D at `xxxx:30B7`, twice, on reboot under ours). GDI
  still calls `BitBlt` on the screen after `Disable` has set `m8_screen` NULL; `BitBlt` read
  `m8_screen->deFlags`. Every screen entry now returns when `m8_screen` is NULL, as
  `SaveScreenBitmap` already did. Two clean reboots under ours after it.
- **Baseline on `86box_auto`** (`docs/captures/2026-10-20_auto/m8bench_auto.txt`, runs `auto_ours1`,
  `auto_ati2`; `auto_ati1` is mislabelled - it ran ours, a repeat run). Ours vs ours repeat: within
  +-10% per test, so smaller gaps are noise. ATI vs ours: engine paths 0.96-1.17x; text 5-11x;
  bitmaps 10-45x; fill_hatch 52x; lines_dash 21x (pixels differ); drag_frame 22x; polygon 9x.
  Switching driver through the agent: `REGEDIT /S C:\ATIM8.REG`, then `v9x_reboot_with_proof`.

## Plan item 1, 10-20 (`0dd281e`, `bf3baf1`)

Bitmaps: GDI creates/deletes colour bitmaps via RealizeObject style 5 / -5 (no DDK header names it), so
proxies are tracked by lifetime, no pixel hash; a device bitmap blitted to the screen is uploaded whole once.
Cache in two areas sized per mode (right strip; full-width rows below the screen, shared with SaveScreenBitmap).
800x600 vs ATI: bmp_large 16x, scr_to_mem 3.7x, bmp_card 2.5x ours. 1024x768 (`ours1024`/`ati1024`): pixels
34/35 as at 800, scr_to_mem 3.8x, bmp_card 2.4x ours; bmp_large ATI 11x (200 rows > the 128 rows below the
screen: hold big bitmaps as bands), drag_frame ATI 1.8x. Owner: Solitaire drag on par with ATI. Mode switch:
`C:\RES1024.REG` / `C:\RES800.REG` (both beds), then reboot. A shutdown BSOD was seen once during the
switching, address not captured; not seen again in 3 clean reboots - watch for it.

## Plan item 2 so far, 10-20 (`eb87aa9`)

Text keyed by what was asked (font, chars, spacing, xform, clip/opaque relative to the origin); a hit replays
the DIB Engine's opaque fills and the cached string bitmap. All hashes now 16-bit only (Watcom turns 32-bit
shifts into loops and multiplies into calls even with -4). PIT-timed per call: key 2.0 -> 0.4 ms, miss
4.8 -> 1.6 ms, hit 0.13 ms. 1024x768 (`h16`): text 1.6-2.8x, pixels = ATI; ATI still 1.7-7.5x on text.
text_sys ~7 ms per string while our hit costs 0.5 ms: the rest is outside ExtTextOut (GDI or other entries
per TextOut) - find it next. Fonts seen: two raster fonts, header fields do not match the DDK 3.0 layout
(bits offset 0, face offset 42) - realized copies; probe before any per-glyph cache.

## Plan item 2, continued 10-20/21

- Text refused by the engine path fell back to strips at 10-20 ms each; all 57 per bench were reason 3
  (bitmap starts left of or above the screen). Now drawn from the string cache, which blits only the
  visible part (`8aaa0fd`). 1024x768: text_sys 139 -> 651/s, others +14-23%; ATI
  still 1.4-2.8x on text, pixels equal. Per call: hit 0.53 ms, miss ~1.6 ms; GDI never asks to measure.
- Tried and reverted: an exact-compare string key (no hash) - zero hits, cause not found.
- Next for text: per-glyph cache (beats ATI on varied text); needs the realized-font layout probed first.

## Items 3-4 started, 10-21

- Hatched brushes (`eb6dd90`): cell painted twice over two fillers, gaps = pixels that differ; transparent
  gaps filled with bkgd mix LEAVE. fill_hatch 12 -> 302/s (ATI 386); drag_frame 183 vs ATI 91 (2x ours).
- Scan spans (`5fde707`): OS_SCANLINES/OS_POLYSCANLINE with a solid brush = one engine fill per span.
  ellipse 13 -> 51 (ATI 68), floodfill 1.9 -> 5.8 (ATI 10.5), polygon 10 -> 15 (ATI 99: rest of the
  polygon goes another way - find which Output style). Pixels = ATI in all but lines_dash.
- Still behind (1024x768): lines_dash 29x, mono_bmp 9x, polygon 6.7x, bmp_large 11x (needs banding),
  dib/stretch 3.5-5x, text 1.4-2.8x, mem_to_mem 2x.

## Start here: shadowed setup registers (worklist item 6), design read 10-21

- Every engine routine in `m8hw.c` sets FRGD_MIX, starts the op, then restores FRGD_MIX to
  SRC_FRGD|REPLACE (fill_mix, copy_mix, write_rect, read_rect, polyline, mono: 2 writes each;
  plane_blit also restores BKGD_MIX, RD_MASK, MULTIFUNC PIX_CNTL). Each also polls
  EXT_FIFO_STATUS once first (`m8_wait_fifo_empty`). ~100 `outpw` sites, all in `m8hw.c`.
- Shadowing alone saves little while the restores flip the value back. Plan: (1) `m8_out(port, v)`
  caching FRGD_MIX, BKGD_MIX, FRGD_COLOR, BKGD_COLOR, WRT_MASK, RD_MASK, DP_CONFIG, MULTIFUNC by
  index (PIX_CNTL, scissors T/L/B/R) and EXT_SCISSOR_*; coordinates, MAJ/MIN axis and CMD always
  written. (2) Drop the end-of-op restores; each routine states the mix/mask it needs up front.
  (3) Invalidate the cache in `m8_hw_set_mode`, `m8_hw_passthrough` and `m8_screen_resume` (DOS).
  Audit `m8_hw_mono_plane`, cursor and palette code too (they were not in the first scan).
- Measure: full M8BENCH vs `auto_ati2`/`ati1024`; every engine test should rise, pixels must stay
  equal. Then FIFO slot counting in place of the per-op status read.

## Shadowed setup registers, tried 10-21 (not committed)

`m8hw.c` in the working tree: a cache of FRGD/BKGD mix and colour, WRT/RD_MASK, DP_CONFIG, PIX_CNTL and
the extended scissors; restores dropped; FIFO room counted. Pixels = baseline in all 35. Two constraints
found (card, as 86Box models them): a CMD write sets LINEDRAW_OPT's line bits, so LINEDRAW_OPT cannot be
cached (cached, it changed `lines`/`rubber_band` pixels); whichever of DP_CONFIG, FRGD_MIX and CMD was
written last picks an extended op's foreground source, so mono rewrites DP_CONFIG after either.
Result (`docs/captures/2026-10-21_shadow/`, 1024x768): control = HEAD in two boots, `ctl1-4`, geomean
1.000 between boots; shadow `shadow4-5` geomean 0.972, stretchblt 0.87, stretch_dib 0.91, rubber_band
0.90, lines_dash 0.90, although rubber_band sends ~12 port writes per op instead of ~24.
Cause: the bed prices port I/O and memory the wrong way round. `inboard386.c` charges an OUT
`12 + 11 x (busspeed/4.77 MHz - 1)` cycles = ~65 (+ `io_delay`) at 83.5/3 MHz, ~0.8-1.1 us; the real 5160
measured 7.64 us per word OUT (`docs/iowidth_measured_2026_09_20.md`). Every memory read costs 14 cycles
(0.17 us, no cache model); on the card a cache hit is one cycle. Each skipped-write check is two memory
reads, so in the bed 12 checks cost more than the 2-3 OUTs they save; on the card they save ~15 us per
fill, ~90 us per rubber-band rectangle, ~100 us per text string, for well under 1 us of CPU. The bed
cannot judge any trade of CPU work for fewer port writes; count the writes, or measure on the card.

## Next, in order

1. **Shadow the setup registers** (worklist item 6): every engine path gains.
2. **Dashed lines** (29x behind): PATT_DATA lines, short strokes.
3. **Glyph cache** for text; then polygon (rest of it), mono bitmaps, bitmap banding at 1024x768.

## Driving the bed

Two beds, same image, both on `86box_auto`: `vm_5160_mcp` (ours, agent on 9869, MCP `v9x`) and `vm_5160_ati`
(ATIM8, agent on 9870, MCP `v9x_ati`). Bench one at a time: two running VMs share the host CPU and skew
real-time results. Launch GUI programs with `v9x_exec` without `detach`: a detached launch runs hidden (a hidden
Solitaire left a second, visible one with no cards, on either driver).

`vm_start` with `vm_dir` = `vm_5160_mcp`, `window=True`; step ~120 s to the desktop. Start menu: press
`ctrl+esc`, step 3 s, then `r` (keys sent too early open Help or the Task List). After boot `release`,
then `v9x_*` tools work. A stuck run: the fault dialog's Details give the registers.
