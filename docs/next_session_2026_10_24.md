# Next session - from 2026-10-23

Supersedes `docs/next_session_2026_10_23.md`. Card log: `docs/captures/2026-10-23_card/m8_card_bench.txt`.

## Start here

Gate unchanged (owner, 10-23): equal or better than ATIM8 in every test before DirectDraw, more colours, 3D.
Method: `docs/mach8_driver_method.md` §4b (profile, shorten) and §4c (divide by the bytes first).

0. **Boot stall, second time.** 10-23 15:00, the restart into M8F6 never reached the agent (ping
   answered, port refused). The first was 11:41 into M8S1. Two in ~9 driver restarts. F6 only
   touches StretchBlt/StretchDIBits code, so the stall is likely not the new code - not proven. Ask the
   owner what the screen showed; it comes before more driver work.
   Owner, 10-23: both times the blue wallpaper was drawn and there was no taskbar or Start menu -
   Explorer did not finish starting (the agent starts after it, hence the refused port). Owner's
   restart into the same M8F6 then came up clean. Next: what Explorer's start draws through us
   (BOOTLOG/trace build), and whether the 5160's known Explorer hang (MEMORY) happens with ATIM8.
1. Late state: 1.539x (card_f19r11), pixels = ATI in all 35, all committed and pushed. Below ATI:
   mem_to_mem 0.62, text_varied 0.80, polygon 0.83, dib_8bpp 0.86, draw_icon 0.87, floodfill 0.93,
   bmp_changing 0.94, ellipse 0.95, cursor_move 0.96, dstinvert 0.97, fill_small 0.97 (noise +-8%).
2. Use the counters: `build.ps1 -Prof` now prints `c<n>` per op and s62 (span walk). Every call is
   bus work + set-up; the set-up is what short paths remove (method 4b).
3. polygon: 63 us a span, 23 of it writes - the walk itself is CPU (each OUT flushes the L1, so the
   walk's code is refetched after every write): fewer cache lines in spans.asm per span.
4. mem_to_mem 0.62: the destination (bmp_card2) is held on the card, so each copy hashes its RAM
   source for a cache lookup; with neither bitmap on the card the copy already goes to DIBENG
   (m8_px_pieces returns 0). Trading the proxy for a RAM copy costs one read-back and touches the
   design drag_frame (8.7x) relies on: measure both on the card before changing the policy.
   ATIM8 (`seg01:15c4-15d9`): a destination without deFlags 8000h, or with 10h, goes to DIBENG - its
   mem_to_mem is a RAM copy every call.
5. draw_icon: both halves on the card (1 copy + 1 plane blit) yet 1.9 ms a call. Not the string
   cache's linear scan (hash buckets: no change, 10-23). Next: time m8_px_restore / m8_px_upload /
   m8_mono_cached / m8_bc_draw separately in a profile build (as s62 did for Output) before guessing.
6. text_varied: ExtTextOut 1.47 ms a call against ATIM8's ~1.16 ms of driver time. ATIM8 has no text
   cache (`docs/atim8_disassembly.md`, ExtTextOut section): it does our uncached path's engine work and
   nothing else. Ours adds, on every new string, a text-key lookup and a hash over the rendered bits for
   the string cache's seen-twice test. Lead: hash the bits only when the text key has been seen before,
   so a first-sight string costs what ATIM8's does. A glyph cache (XFree86 mach8fcach.c) comes after.
6b. Assembly, by the cache-line model (each port access flushes the L1; ~1.3 us a 16-byte line):
   spans.asm spends ~20 us of CPU a span - fit the per-span path in 1-2 aligned lines, registers for
   the record pointer / clip / CUR_Y, room counted once a record, no `call drain` on the common path,
   segment loads once a call. Then the fill / copy / pixel short paths in assembly. Count bytes run
   per span from `wdis`, predict at 1.3 us a line, confirm with the profile build.
8. OWED to Andrew (#50, replied 10-23): time a 512-word `rep insw` from the XT-CF with the L1 on and
   off, ring 0 or DOS, PIT harness (`tools/gen_iowidth_probe.py` is the model). Report the number
   either way; if it helps, gate it to this hardware. Ledger entry 2026-10-23 16:00.
7. Closed 10-23: VGA aperture, dword PIX_TRANS, polyline merge, DIB table cache, pattern grouping.

## Cloud stream (started 10-23, owner's credits)

BlueSCSI firmware for the 5160 (multi-LUN behind one ID, removable LUN with media change by sense
data, DaynaPort later), run by a remote cloud agent (Opus leading, Sonnet for mechanical work). It
writes only to branch `claude/bluescsi-5160-firmware-o12wij`, folder `cloud/bluescsi/`; report in `cloud/bluescsi/REPORT.md`
(plus SURVEY.md, DESIGN.md, and `firmware/`: the `EnableLUNs` patch, the Pico 2 W UF2, SHA256SUMS).
Owner-approved and posted 10-23: the #50 update (issuecomment-6371954024); on master, the ledger entry
(`e3ce7b0`) and this handoff's corrections. Review: `git fetch` then read the
branch; everything there is "built, untested on the 5160" until tested here.

## State

- Card: `M8D1.DRV` selected = master `14323e2` (card_d1r11: 1.519x, floodfill 1.00, pixels = ATI in
  all 35). `menudefault=LEAN`. `C:\M8BENCH.EXE` is the profile-v4 bench (prints counters `c<n>`).
- ATIM8's span loop (`seg08:0dcd`): 7 I/O a span (FIFO poll, CUR_Y, PATT_DATA_INDEX, PATT_DATA,
  CUR_X, PATT_INDEX, SCAN_TO_X), ~52 us; ours ~45 us. Polygon's gap is per call, not per span.
- ATIM8 ExtTextOut read 10-23 (`docs/atim8_disassembly.md`): no cache, ~12 writes and an idle wait a string.

## Where we are: 1.473x ATI overall (card_f5r11, was 1.345x), pixels = ATI in all 35

Afternoon 10-23: commits `61fe29a` ScanLR, `ad11835` DIBs, `d5e84c3` stretch, `3291c34` mono
(`docs/captures/2026-10-23_card/README.md`). The morning's figures follow.

Below ATI: stretchblt 0.40, stretch_dib 0.46, dib_8bpp 0.61, mono_bmp 0.66, floodfill 0.68, mem_to_mem 0.68,
draw_icon 0.80, polygon 0.82, text_varied 0.85, cursor_move 0.91, bmp_changing 0.92, fill_small 0.95,
dstinvert 0.96, ellipse 0.97. Brought level or ahead today: setpixel 1.00 (0.80), getpixel 1.12 (0.82), lines
1.19 (0.88), lines_hv 1.31 (0.84), rubber_band 1.19 (0.88), rectangle 1.14 (0.76), blit_small 1.19 (0.93),
fill_xor 1.06 (0.91).

## What 10-23 established

- **Each GDI call starts with the L1 empty** (the last call's port writes flushed it; every I/O access flushes,
  `docs/captures/io_cache_flush_2026_09_21.md`). A call's CPU cost is the cache lines it touches, ~1.3 us each.
  Compiler flags do nothing (`-ox`, `-os` neutral); short paths, one hot-state block (`m8_hot`) and assembly for
  the span loop did (Andrew, #50).
- **Profile build** (`build.ps1 -Prof`, `3973538`): our us per operation per test, per entry point, per Output
  style, short-path hits, and a re-read check for pixel reads (190 checked, 0 wrong). The timer costs ~83 us a
  call (VTD emulates the PIT) and is subtracted; never quote a profile build's ops/s.
- **ATIM8 and ours report the same caps** (poly 0008, curve 0000, line 0023): GDI scan-converts shapes for both.
- **ATIM8 reads a pixel without an idle wait** (seg07:00aa); ours now sends 3 writes against its 6.
- Commits: `55d5624` queued fill, `3973538` profiler, `56ee0f4` short paths, `d3c3fee` hot block, `2c6845c`
  pixel reads, `d6646b4` Output styles + caps, `4951a99` span loop, `f272261` assembly span walker.

## People

- Andrew (#50, 10-23): reply posted (issuecomment-6360908020); cross-references on #51, #52, #53. His leads
  (compiled binary, inline asm, size, I/O flushing the cache) drove the day's work - worth telling him the
  numbers once the shapes are level. Owner approves wording; an agent may post on approval.
