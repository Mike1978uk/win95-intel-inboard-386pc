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

## Afternoon, 2026-11-01

- **lines_dash above ATI** (`f31e070`): a styled line inside the clip takes the short path and the engine
  keeps its pattern/ALU/data path (`M8_DP_DASH` in `m8_dp_mix`). Card `card_dash1-4`: 1.02-1.07, pixels = ATI in all 35.
- **floodfill**: `m8_read_scan` set-up without drain or idle wait (uncommitted in `m8hw.c`); card run `card_flood1-4`
  started 13:15 (`tools\m8_card_cycle.ps1 -FetchOnly -Label card_flood` if the session ended first). Commit only if
  pixels = ATI and floodfill >= 1.0.
- **DirectDraw milestone 1 passes in the bed** (DirectX 7.0a): see `docs/mach8_directdraw_plan.md`. Using the
  primary fails (dxdiag step 11 DDERR_EXCEPTION, M8DDTEST hangs in CreateSurface/Lock) and leaves GDI fills
  missing until a restart. Milestone 2 next.
- **Focus rectangle bug, ours, older than today**: DrawFocusRect loses its top/bottom strips at some positions
  (bed: Shut Down dialog, bottom radio). New M8BENCH test 36 `focus_rect` (41 DrawFocusRect over all 8x8 phases):
  bed `bed_focus_ours` 5e7991a5 vs `bed_focus_ati` DIFFER. Suspect the one-row pattern path
  (`m8_hw_pattern_span` h == 1, SCAN_TO_X) or its phase. ATI needs a card run with the 36-test M8BENCH
  (new exe built, not yet on the card) before card comparisons include it.
- Bed `vm_5160_mcp` now has ATIM8 selected (boot 87); `C:\V9XREMOTE\M8DISPB.REG` selects ours.
  Bisect drivers left in its SYSTEM folder: M8PREMM, M8B0E66, M8BA517.

## Session 2026-11-01 (late)

- **Focus rectangle: not our bug.** M8BENCH test 36 on the card (new exe uploaded): `card_fr_ours1`
  (m8flood = master) and `card_fr_ati1` both hash `14ad90a6`, the same as `bed_focus_ati`; only
  `bed_focus_ours` (`5e7991a5`) differs. 86Box's SCAN_TO_X (`vid_ati_mach8.c`, cmd_type 5, `mono_src == 1`)
  never applies PATT_INDEX, so our one-row pattern spans are out of phase at odd x in the bed. Owner to
  decide on an 86Box fix. Bed pixels for focus_rect cannot gate our driver until then.
- **focus_rect speed: 353 vs ATI 512 ops/s (0.69)**, the one test below ATI; the other 35 equal pixels,
  geometric mean 1.81.
- Profile (`card_fr_prof1`, parts in `card_fr_prof2_1`): 4 BitBlts an op at ~570 us each; the pattern fill
  ~280 us of it, the brush lookup ~45 us (part timers cost ~85-100 us each). The one-pixel-wide sides
  cost 5 + 3 writes a row.
- **Fix** (`m8hw.c`): a one-pixel-wide pattern column is a vertical LINEDRAW with that column as an 8-bit
  line pattern (9 writes, any height); `m8_hw_pattern_begin` keeps the engine's set-up when `m8_dp_mix`
  holds it (`M8_DP_PATT`); pattern spans count FIFO room instead of draining. Card `card_focus1-4` +
  `card_focusb1-2`: focus_rect 0.92-0.95 (497-513 med vs ATI 538), pixels = ATI in all 36. lines_dash read
  483-535 in three runs, 631 in both re-runs: card noise. Card left on `M8FOCUS`.
- **focus_rect above ATI**: `m8_blt_short` takes two-colour pattern strips one pixel tall or wide (brush
  cache, cursor check, then the engine). Card `card_strip1-4`: 1.05-1.09 (566-586 vs ATI 538), pixels = ATI
  in all 36, geometric mean 1.79-1.83. Card left on `M8STRIP` (= master).
- **Watch lines_dash**: runs read 474-535 in 5 of 11 runs since `card_focus` (others 617-632; before, 621-626
  in every run). Neither change is on its timed path (the dash short path needs `M8_DP_DASH` in
  `m8_dp_mix`); lines_hv dipped once too. Control `card_ctl_flood1-4` (M8FLOOD, before `eed1c32`):
  lines_dash 619-630 in all 4, lines_hv one dip (673). So lines_hv is noise; **lines_dash is real**: 12 of
  12 old-driver runs steady, 5 of 11 new-driver runs low, varying run to run within one boot. Untested
  guess: code layout (cache-set aliasing with each M8BENCH process's data), as no changed line runs in
  its timed loop. Next: a profile build's `drv_us/op` for lines_dash in a low run and a high run.
- ATI's only 36-test card run is `card_fr_ati1` (one run): re-run ATI 3 more times for a fair base.

## Evening, 2026-11-01: where to start next

- **GDI vs ATI (card, 36 tests):** at or above ATI in all but lines_dash; pixels = ATI in all 36.
  focus_rect 1.05-1.09 (`eed1c32`, `f6b63db`). lines_dash dips to 474-535 in about half the runs of
  today's builds, never with `M8FLOOD` (control): cause open (layout guess untested; the profile build
  did not separate it). ATI's 36-test base is one run (`card_fr_ati1`): re-run it 3 times.
- **DirectDraw milestone 2 done in the bed** (`d7be89b`): primary Lock/Unlock through the shadow.
  dxdiag: step 11 passes (concentric rectangles), the windowed blit test draws correctly (white square,
  old position erased) but at a few pixels a second: every frame is the HEL locking the whole screen.
- **Next, in order:** (1) HAL Blt: DDBLT_COLORFILL and screen-to-screen copy on the engine, clip list
  honoured, anything else NOTHANDLED; (2) Lock: no read-back for DDLOCK_WRITEONLY; (3) off-screen
  surfaces in card memory (heap from the bitmap cache's allocator); (4) system-memory -> primary as a
  host write only. Then a DirectDraw fps test on the card (fill / blit / lock), not the bed.
- **The bus limit, for expectations:** a full 800x600 frame through PIX_TRANS is ~1.8 s each way on the
  5160 (3.8 us/B). Apps that redraw the whole screen in RAM each frame cannot be fast here; apps that
  keep surfaces on the card and blit can.
- Card on `M8PROF3` (profile build of master): replace before any timed run. Bed on `M8DDL2` (master +
  HAL); restart it before the next DirectDraw build (the DLL stays loaded).

## Night, 2026-11-01: START HERE (supersedes the sections above where they differ)

**State.** master `ea519fe`, pushed. A 640x480 16-bit desktop works in the bed: GDI draws 5-6-5 in RAM,
the driver dithers what changes onto the card's 256 colours (`src/fb16.c`; details and dead ends in
`docs/mach8_directdraw_plan.md`, "16 bpp"). Owner's eye: desktop, menus, icons flat; gradients smooth
(TEST24.BMP within 1-2.5 of the original); no pointer ghosts (the card pointer, as at 8 bpp); slow only
while the disk is read. Since then: DirectDraw off-screen surfaces (`d279668`) and system-memory Blt
(`056ce69`) at 8 bpp; the card has none of tonight's work.

**Machines.** Bed `vm_5160_mcp` on `M8FB16N` (= master, 16 bpp; registry keeps it). Back to 8 bpp:
`drivers/mach8_display/bed/M8FB8.REG` (LF line ends; regedit takes them). Guest tools in C:\: FB16CHK,
SCRGRAB [keys], RAW565 l t r b, ICONTEST, FB16TEST, TEST24.BMP. Card still on `M8PROF3`.

**Next, in order:**
1. **Paint's title bar** (owner's repro: Start > Run, then Start > Accessories > Paint, click Run while
   Paint loads): inactive grey with a navy box round the caption buttons until Paint is clicked.
   FB16CHK says card = frame buffer, so GDI's own picture has it. Same steps at 8 bpp (`M8FB8.REG`,
   reboot): there too = Windows, not ours; only at 16 bpp = one of our GDI entries at 16 bpp.
2. **DirectDraw at 16 bpp.** dd16.c refuses it when `m8_fb16`. HAL: primary = the frame buffer
   (fpPrimary = its linear address, 5-6-5 pixel format, pitch 1280), Lock returns it directly, Unlock
   sends the rectangle through `m8_fb16_send`; Blt in RAM (or NOTHANDLED to the HEL) then send. Off-screen
   surfaces in system memory. Then dxdiag with the desktop already at 640x480x16, test 3.
3. **Bus cost on the card.** Solid fills at 16 bpp cross the bus as pixels (400x300 = 120000 bytes, ~0.7
   s on the 5160). Send a solid-colour rectangle as a pattern fill (PATTERN_L/H or PATT_DATA, 4 dither
   rows then a card-side copy, as ATIM8's colour brushes); also skip rows unchanged since the last send.
4. Older, still open: the post-dxdiag GDI slowdown in the bed (`docs/mach8_directdraw_plan.md`, Open);
   lines_dash dips on the card; 3 more ATI 36-test runs; put master on the card (GDI gate: >= ATI in
   all 36) before any 16 bpp work there.

## Day, 2026-11-02: START HERE (supersedes above)

**State.** master `24593e7`. Card on `M8TXTFX` (= master, plain build, md5 `bbea6965...`).

**Selection bug.** Owner, real 5160, installing DirectX 7.0a (photos
`C:\Users\lycet\OneDrive\Desktop\XT_project\photos\dissapearingtext1-3.jpg`): in Explorer the selected
name was blank and old focus boxes stayed. The packed-text path (`m8_gc_text`, from `29419f9`) ignored
`lpOpaqueRect`. GDI passes list-view labels as transparent text with an opaque rectangle and
`options == ETO_BYTE_PACKED` only (no `ETO_OPAQUE`). Fixed in `24593e7`: such calls go to DIBENG.
Reproduced in bed `vm_5160_mcp` at 1024x768x8 on master, gone with the fix, same keys and timing.
Not visible at 16 bpp.

**Bench, card.** `card_txtfx1`, `card_txtfx_b1`, `b2`: pixels = ATI in all 36; faster in 25 (24 in run 1);
geometric mean 1.78x ATI. text_varied 461-463 vs ATI 505 (master before the fix, `card_strip1-4` /
`card_ctl_flood1-4`: 469-538). Likely cause: GDI gives OPAQUE TextOut an opaque rectangle, so unique
strings now miss the packed path.

**DirectX 7.0a** is installed on the card (owner). First launch of the installer hung the 5160 with a
constant tone from the Sound Blaster (SB looping its DMA buffer after the CPU stopped); hard reset;
second attempt installed. Cause unknown; one-off unless it recurs.

**Handoff item 1 closed.** Paint's inactive title bar with the navy box round the caption buttons
happens with ATI's driver too (owner, bed, 2026-11-02): Windows 95's own behaviour, triggered by
focusing the Run dialog while Paint loads. Also seen at 8 bpp on ours.

**Machines.** Bed `vm_5160_mcp`: registry selects `atim8.drv` (owner's ATI comparison). `M8FB16N.DRV` and
`M8DISP.DRV` in it are both the `24593e7` build. Original M8FB16N kept on the host:
`C:\Users\lycet\AppData\Local\Temp\claude\C--Users-lycet-RiderProjects-86Box-Inboard\3159db7d-37a7-4dba-b55f-b312a2decdec\scratchpad\M8FB16N.orig.DRV`.
Display at 1024x768x8 (`M8FB8.REG`); `M8FB16.REG` and `M8FB8.REG` are in C:\ of the bed.

**Issue #10.** Simon's Phoenix and Ruud ROM results recorded (`f6be662`); owner has replied.

**Next, in order:**
1. **Packed-text opaque fill.** Draw the opaque rectangle in the packed path (fill, then text; recorded
   for key replay). Bench >= ATI in text_varied. In progress.
2. **DirectDraw at 16 bpp** (item 2 of the previous section, unchanged).
3. **Bus cost at 16 bpp** (item 3 of the previous section, unchanged).
4. Older open items (item 4 of the previous section).

## Afternoon, 2026-11-02: START HERE (supersedes above)

**State.** master `7295b6e`, pushed. Card on `m8hash.drv` at 800x600x16 (registry
`Config\0001\Display\Settings` BitsPerPixel 16; `MODES\16\640,480` and `800,600` present). Bed
`vm_5160_mcp` at 800x600x16; `M8DISP.DRV` and `M8FB16N.DRV` = the M8HASH build; `FB800.REG` in its C:\.
The script's keystroke restart did not happen (boot counter unchanged); owner restarted by hand.

**Landed after `521b987`.**
- `fa3d7c3` packed text draws its opaque rectangle (replaces `24593e7`'s hand-off to DIBENG).
- `cba2a55` 16 bpp at 800x600 as well as 640x480 (`M8FB16.REG` offers both).
- `2f658cb` 16 bpp one-colour rectangles: 4x4 dither block + engine copies.
- `db47d9a` 16 bpp lines send strips.
- `052e448` dither rows in assembly (`fb16cv.asm`); FB16CHK checks against the C row.
- `be088b9` M8DDB DirectDraw bench (`tools/m8ddbench`, writes `C:\M8DDB.TXT`, M8BENCH format; per-test
  "card N" check at 16 bpp).
- `7bca399` DirectDraw colour-keyed blits on the engine (8 bpp). Card script installs `M8DD32.DLL` via
  `M8DD32.NEW` and `WININIT.INI` (DDRAW keeps the DLL loaded).
- `0093e5a` DirectDraw at 16 bpp: primary = fb16 frame buffer; Blt fills, copies, source-keyed in RAM,
  then sent via `M8_ESC_FB16SEND`.
- `a8ee0b2` screen copies moving by multiples of 4 are card copies (`M8_ESC_FB16COPY` for DirectDraw).
- `7295b6e` fingerprints: per 32-pixel row piece, sends skip unchanged pieces.

**Card results (real 5160).**
- GDI 8 bpp, M8KEY: faster than ATI in 28, slower in 0, pixels = ATI in 36. text_varied 507.6 vs ATI 505.
- DirectDraw 8 bpp (`card8key` vs `card8hal2`): dd_sprite 0.30 -> 869/s, pixels same. The card had no HAL
  until `M8DD32.DLL` was copied (`card8hal` = NO HAL; `card8hal2` = HAL).
- 16 bpp 800x600 works (`card16_1`, `card16dd`, build M8DD16): DirectDraw card check 0 in every test.
- Fingerprint build M8HASH vs M8DD16: GDI geometric mean 1.54x (text 3.4-3.9x, bmp_card 5.1x,
  stretch_dib 8.4x); slower in 9 single-run tests (mem_to_mem 0.67, focus_rect 0.73), not yet re-run.
  DirectDraw 1.66x (dd_scroll 8.3 -> 81/s, locks 1.7x); card 0 throughout.

**Next, in order (owner).**
1. **DirectDraw Mode X on the VGA half (28800).** First test: clear `DDHALINFO_MODEXILLEGAL` in `dd16.c`;
   an M8DDB variant requests 320x200 with `DDSCL_ALLOWMODEX`. velocity9x: ddraw.dll has its own ModeX set
   (`references/velocity9x/docs/issues/2026-08-15-doom95-low-resolution-modes.md`). Risk: output switch
   and restoring the desktop.
2. Card surfaces at 16 bpp (RAM 5-6-5 copy + dithered card copy).
3. GDI caches in spare card rows at 16 bpp.
4. Unaligned copies of exact-colour areas as card copies.
5. Fewer row set-ups per send.

**Aspiration (owner).** Some 3D: software Direct3D at 16 bpp first, then flat-shaded triangles as engine
span fills (D3D HAL) at 8 bpp.

**Open.** DOS timing of VGA-memory writes vs 8514 PIX_TRANS writes on the 5160: offered, not run.
Re-run the 9 slower single-run GDI tests before judging them.

## Evening, 2026-11-02: START HERE (supersedes above)

**State.** master `6f76f14` plus this file. Card on `m8resync.drv` (= master `f4fda53` build) at
800x600x16, `MODES\16` keys present; `M8DD32.DLL` installs via `M8DD32.NEW` and `WININIT.INI`
(`tools/m8_card_cycle.ps1`). Bed `vm_5160_mcp` at 1024x768x8 (`M8FB8.REG` imported); `M8DISP.DRV` and
`M8FB16N.DRV` = the resync build. The agent's screenshot helper fails at 16 bpp (Win32 1460) and would
read the RAM frame buffer, not the card.

**Landed after `4b48102`.**
- `3b45b38` worklist section X.
- `d948817`, `86c1ede`, `6f76f14` `docs/mach8_3d_research.md`: 3D research, retro approaches 7-10, M8SPAN
  card results.
- `84be69f` SetPalette while disabled does nothing (DDRAW Mode X set the palette with the driver
  disabled -> null DIB device -> fatal 0E in the bed); M8DDB `modex <label>` mode with a step log.
- `f4fda53` M8SPAN, and driver resync escape `0x4D3E` (`m8_switch_out` + `m8_switch_in`) for probes that
  draw behind the driver.
- `58df2c1` M8SPAN arms D-G.

**Mode X (bed; build with `DDHALINFO_MODEXILLEGAL` cleared, master still sets it).**
- DDRAW then lists 320x200x8 and 320x240x8 (`DDSCAPS_MODEX`).
- From the 16 bpp desktop, `SetDisplayMode(320,200,8)` returns `DDERR_INVALIDMODE` 88760078.
- From 8 bpp it succeeds; primary created (caps 1020c200, pitch 320); `CreatePalette` OK; `SetPalette`
  killed the program: fatal 0E at 0137:BFF9A07C before the guard, silent death after it.
- The monitor stayed on the 8514 output (black at 1024x768): nothing hands the output to the VGA half.
- An Internat page fault in KERNEL32 at shutdown followed a failed Mode X attempt; link unproven.
- Next: `inboard-hw-debug` skill first. Find which driver entries DDRAW calls on Mode X entry and exit
  (trace build); switch to the VGA half there, restore via the resync path; fix the palette death.

**M8SPAN on the card (three runs).**
- Spans 20.4-21.9 us along a row, 25-28 us per row (width-independent).
- LINEDRAW 20.6-22.1 us/vertex.
- Polygon fill 0.2 ms (20x20), 3.0 ms (200x200). Owner photographed a correct triangle
  (`photos/directx.jpg`). The bed fills rectangles: 86Box lacks POLY_MODE via LINEDRAW (observed_86box_quirks).
- Palette 64 entries 0.84 ms; pan 15 us; vsync count wrong (loop bug, arm F).
- A probe drawing under the pointer leaves a stale square: anything drawing outside the driver must lift
  the pointer.

**Next, in order.**
1. Mode X investigation (worklist X1).
2. First 3D demo: flat-shaded rotating object via the polygon fill, through the driver (pointer lift);
   then a Direct3D HAL at 8 bpp claiming flat shading (X7).
3. Fix M8SPAN arm F (vsync).
4. dxdiag DirectDraw tests on the card at 16 bpp (owner).
5. 16 bpp levers X2-X5.

**Aspiration (owner).** The first 8-bit Direct3D hardware layer.

## Late evening, 2026-11-02

**Landed after `981ce10`.**
- `2aeb8f2` M8DDB `nopal` label: Mode X without the palette. Fill then returns `DDERR_EXCEPTION`, lock
  `DDERR_SURFACEBUSY`, restore is clean. Unproven: A0000 not mapped for the app (VDD contract).
- `3d30bed` fingerprints are FNV-1a. Rotate-XOR was linear: an even number of fully flipped dwords
  collided. Cause of the card's malformed Explorer text, partial selection and dxdiag test 2 ghosting.
- `b2ebd4f` 640 <-> 800 at 16 bpp without a restart (frame buffer sized for 800x600). Card: full-screen
  640x480x16 sets and restores, card 0 (dxdiag test 3's failure).
- `b5d46e1` M8DDB `fs16 <label>`.
- `9afb390` sends under 1024 px skip fingerprints; a row diffed in one asm call. Card, 2 runs vs FNV:
  geomean 1.04, setpixel 1.49, focus_rect 1.67, floodfill 0.79 (repeated identical fills no longer
  skipped). Two runs of one build differ by 1.04 geomean, single tests by up to 30%: bench twice.

**Card restart.** The script's Start > Shut Down > Restart keys once did nothing (boot counter unchanged,
4-minute throw). Esc, Esc, then the same keys worked. Screenshots fail at 16 bpp (Win32 1460).

**For the owner.** Card is on `m8rowd.drv` at 800x600x16. Check: dxdiag DirectDraw tests 1-3; Display
Properties 640 <-> 800 at High Color; Explorer text; WinZip icon after deleting `C:\WINDOWS\ShellIconCache`.

**Uncommitted.** `86box_auto` `vid_ati_mach8.c`: `M8STEP` pclog on ScratchPad1 writes (diagnostic only).

## Night, 2026-11-02

**Card hung** at dxdiag test 2 -> 3 (no ping), driver `m8known.drv` (fill fix), after SCRGRAB screenshots over
the agent. Not reproduced in the bed with the same build. Owner to power-cycle.

**dxdiag test 3 cost (bed, CPU sampling).** 9 of 11 samples of CS:EIP sit in `M8_FB16_ROWDIFF`'s loop
hashing the whole 640x480 screen each frame: DirectDraw emulates the flip by writing the whole primary
through one lock (16 bpp has no video memory), and Unlock sends the whole screen. The bus carries only
the box; the CPU fingerprints 600 KB a frame. Card test 2: ~20,000 pixels/s sent (FB16MON).

**Fingerprint hash.** Offline test (`tools/fbhash_test.py` pattern set: even flips, swaps, 1-px shifts,
small two-pixel colour deltas, 2.27 M changes): FNV 11 collisions (swaps); rotate-add variants ~2.5 %
on small colour deltas (near-linear mod 2^32-1: rule them out); Jenkins one-at-a-time per dword 0.
`| 1` to keep fingerprints non-zero dropped bit 0 and caused most collisions; use `h ? h : 1`.
One-at-a-time needs no IMUL (486: 13-42 cycles); the bed does not price IMUL, so bed `mx_box` shows
373 vs 350 ms (no gain there). Card measurement pending.

**Uncommitted (bed-checked, card speed unmeasured):** fill marks pieces known (`m8_fb16_known`); 16 bpp
sysmem->primary Blt copies only differing spans and sends them exact (`m8dd_diff_row`, flag 2); fingerprints
= one-at-a-time, unrolled; poly escape 0x4D3F + M8CUBE (card only: the bed lacks LINEDRAW POLY_MODE);
M8VGAW (Win16 probe, VGA memory vs port cost, X8) not yet run.

**Next on the card:** (1) M8DDB `fs16` mx_box FNV (`m8diff`) vs one-at-a-time (`m8oaat`), twice each;
(2) M8VGAW; (3) switch to 800x600x8 (`M8FB8S.REG` is in the bed), M8CUBE 1 and 3 cubes, photo.

**Levers recorded, not built:** 16 bpp HAL flip with a RAM back buffer (diff on Flip, no HEL copy, no hash);
386 page Dirty bits via a small VxD for programs that write the primary sparsely (useless for full-frame
copies: written != changed); 8 bpp full-screen with two pages in VRAM and CRT_OFFSET flip (15 us).

## Late night, 2026-11-02 (owner back; record for the next session)

**Done (pushed):** `b3a5afe` one-at-a-time fingerprints + known fills + diff-on-copy: card mx_box 213 -> 151 ms
(1.42x), GDI 1.03, pixels = before, FB16CHK clean. `98e4fdc` poly escape 0x4D3F + M8CUBE + M8VGAW (card
build carries it; demo not yet run on the card). Card is on `m8oaat.drv`, 800x600x16, boot 185.

**Hangs seen tonight (both 16 bpp):**
- dxdiag frozen with test 3's start dialog drawn, keyboard dead, clock stopped (owner photo). The call in
  flight was SCRGRAB over the agent: do not screenshot during tests until this is understood.
- Start > Shut Down > Restart hung with the hourglass and the SB Pro looping the shutdown sound (owner).

**To do, in order:**
1. Owner: dxdiag DirectDraw tests 1-3 on `m8oaat` (test 3 should be ~1.4x faster than before).
2. Cube demo: card to 800x600x8 (import `M8FB8S.REG` from the bed's C:\ to the card, restart), run
   `C:\M8CUBE.EXE 10 1` and `10 3`; owner photo. Then M8VGAW (`C:\M8VGAW.EXE`, writes C:\M8VGAW.TXT).
3. Levers (owner asked for all three):
   a. 16 bpp HAL Flip with a RAM back buffer: Flip diffs back vs front in asm (repe cmpsd), copies and
      sends changed spans; no HEL copy, no hash. Biggest win for full-frame games at 16 bpp.
   b. 8 bpp full screen: two pages in VRAM (640x480 x2 fits 1 MB at pitch 1024), Flip = CRT_OFFSET (15 us),
      sprites by engine blits; Lock of a VRAM surface via a RAM shadow + fingerprints.
   c. 386 page Dirty bits via a small VxD (DDK toolchain as custom_vkd) for sparse writers.
4. Move remaining C hot loops to asm once each lever is proven (owner: "tight, asm").

**Draft issues for other drivers (owner to approve wording; not posted):**
- Change detection by hash: a linear hash (rotate-XOR, rotate-add) misses structured changes; `| 1`
  to reserve 0 loses a bit; on a 486 an IMUL per dword dominates. Audit any driver that fingerprints.
- 16 bpp restart path hangs (hourglass, SB tone looping): check SB Pro driver + display Disable order.
- Agent-driven screenshots during DirectDraw tests may hang the card: check SCRGRAB vs Win16 lock.

**SB Pro and DirectSound:** works through DirectSound's emulation on the ordinary wave driver (CPU
mixing, higher latency, no hardware buffers); a DirectSound HAL would need our own driver (worklist S).
