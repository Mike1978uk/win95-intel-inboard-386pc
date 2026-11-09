# James Howard (jhhoward) techniques for the Mach8 Direct3D driver

Source: https://github.com/jhhoward, clones under `references/jhhoward/` (ignored):
WolfensteinCGA 98afb74, MicroWeb fdab490, CatacombsCGA 4883a53, Arduboy3D 929db9f,
Raycaster b8c7425, doomtd3 09d6142. Paths below are under `references/jhhoward/`.
Written 2026-11-09 from a read of the source; nothing was run. Format follows
`docs/fastdoom_techniques_2026_11_08.md`. Items already in
`docs/mach8_frame_rate_plan.md` (Y1-Y13, G3, Z15) are cross-referenced, not repeated.

## What does not transfer

His renderers write pixels with the CPU to a framebuffer. Our card is commanded over
ports and has no data aperture, so compiled scalers, `rep movsw` blits and CGA
bank interleave have no analogue.
Not found in any repo: a Mach/8514/ATI path, I/O-write batching, an L1-flush or
cache note, a vsync wait in the frame loop, his own fps numbers.
Fps figures exist only as anecdotes from others: "too slow to enjoy" on a 4.77 MHz
8088 and "five frames per second" on an 8 MHz V40 (https://www.hckrnws.com/stories/34475721,
as quoted by a search summary; page not fetched).
Not read (WebFetch refused, 402/403): his X thread
https://x.com/Jameshhoward/status/1616182105087557641, the Doomworld topic
https://www.doomworld.com/forum/topic/133584, the Arduboy thread
https://community.arduboy.com/t/catacombs-of-the-damned-formerly-another-fps-style-3d-demo/6565.
No write-up by him on scalers or CGA tricks turned up in search; the reasons given
below are inferred from his code and labelled so.

## 1. Cast fewer rays than pixels (WolfensteinCGA)

- `WolfensteinCGA/WOLFSRC/WL_DR_A.ASM:547-552`: in the CGA build the ray loop steps
  `pixx` by 4 ("For CGA step 4 pixels at a time"); the VGA build steps by 1. One ray,
  one wall height, one scaler call per 4-pixel byte: 80 rays across a 320 view.
  `WL_DRAW.C:462-494` `ScalePost` draws a byte-wide strip. No fps figure given.
- `WL_SCALE.C:70` "one scaler every two pixels"; `:122-150` `stepbytwo`: beyond a
  height only every third scaler is built and taller walls reuse a neighbour.
- Maps: the stage that decides how many primitives exist before the edge pass. Fix the
  cell count per frame, not the pixel count; coarser grid when the last frame overran.
  Estimate: edge pass and fills scale with cell count, so 4x coarser = up to 4x fewer
  sends; the look is the owner's eye. Related to Y8 and Z15.

## 2. Merge equal neighbours at the source (WolfensteinCGA)

- `WL_DRAW.C:536`, `:611`, `:680`, `:750`, `:831`, `:897` (six wall-hit routines): same
  wall and texture column as the last ray gives `postwidth++` and
  `wallheight[pixx] = wallheight[pixx-1]`, no new draw; `ScalePost` runs only when wall,
  side or column changes. This is id's logic, kept by him.
- Maps: our joined patches merge after sampling (11-07 note: 72 % shared edges,
  18.6k writes toward ~2k). His merge happens before any geometry exists. Do it in
  the sampler: cell n with the same colour index and row span as cell n-1 extends the
  pending rectangle instead of emitting edges.
  Estimate: removes edge-pass work for same-colour runs, maybe half of the 13 ms;
  CPU only, no bus or RAM change.

## 3. Precomputed code per height (WolfensteinCGA, CatacombsCGA)

- `WL_SCALE.C:184-300` `BuildCompScale`: for each wall height the 64 texture rows are
  compiled once into `mov al,[si+n]` / `mov es:[di+off],al` straight-line code
  (opcode bytes at `:249-281`). `:63-160` `SetupScaling` builds `scaledirectory[]` at
  start-up and locks it (work buffer 20000 bytes, `:120`). `WL_DRAW.C:405-420`
  indexes it by height and makes one far call per column. A `RETF` is patched in to
  stop early (`WL_SCALE.C:444-462`) and removed after.
- Catacombs does it offline: `CatacombsCGA/Source/COTD/Generated/WallScaler.inc.h`
  (one `case` per height, unrolled stores, `u`/`l` colour arguments) and
  `Generated/DrawRoutines.inc.h` (a sprite as fall-through `case y:` stores).
- Maps: output is a function of (size class, colour pair), so build it once. For us
  the table holds a command list, not code: a prebuilt sequence of register words per
  size class, sent by a short loop. Y9 at command level.
  Estimate: replaces per-triangle command building with a copy; bus unchanged, RAM up
  by table size. Measure the phase split before building.

## 4. Draw into RAM, copy once; alternate fields (WolfensteinCGA, CatacombsCGA)

- `WL_DRAW.C:1752-1795` `CGABlit`: all drawing goes to `cgabackbuffer` (0x8000 bytes,
  `ID_VL.C:1600`), then one `rep movsb` per row copies the view into both CGA banks
  (0xB800, 0xBA00). Slow video memory is touched once per byte per frame.
- `CatacombsCGA/Source/COTD/Main.cpp:482-505`: interlace. Each frame blits one bank
  only (`even = !even`, 0xb800 then 0xba00); the other keeps the old picture. Half the
  bus bytes per frame, key I; key N blits a narrower window. No figure.
- Maps: bus side. A CPU-side list sent as one burst per frame is Z15's "send first
  colours' fills while computing the rest". Interlace is Y5; his shows it plays.
  Estimate: Y5 halves pixel time; combing on motion.

## 5. Bounded front-to-back draw list (CatacombsCGA, Arduboy3D)

- `CatacombsCGA/Source/COTD/Draw.cpp:294-330` `DrawCells`: cells walked front to back
  (loop direction from `camera.rotCos`), in a 12x12 window offset 5 cells ahead
  (`MAP_BUFFER_*`). `:380-432` `CreateQueuedDrawable`: insertion into a list capped at
  12 (`Defines.h:91`); when full the farthest is dropped or the new one refused
  (`return nullptr`). `wBuffer[x]` (`Draw.cpp:27`, `:194`, `:809`) holds the nearest
  depth drawn per column, so a far sprite column is skipped by one byte compare.
- Maps: before the edge pass: a fixed cap on primitives per frame, nearest kept, plus
  a per-row depth byte to skip primitives wholly behind nearer ones. Direct3D titles
  do not promise draw order, so order would have to come from Z, which we may not see.
  Estimate: bounds the worst frame (cost O(cap), not O(scene)); visible loss in dense
  scenes; correctness risk. Main gain is pacing (item 8).

## 6. Resolution chosen by the program (WolfensteinCGA, MicroWeb)

- `WL_MAIN.C:1376-1410` `SetViewSize`: recomputes `viewwidth` (`:1393`), `centerx`,
  `screenofs` and rebuilds the scalers when the player shrinks the view. Fewer rays and
  pixels in proportion. No automatic adaptation: a key press.
- MicroWeb: 15 video modes behind one `DrawSurface` interface
  (`MicroWeb/src/Draw/Surface.h`, `src/VidModes.cpp`), picked at start from detected
  hardware.
- Maps: Y8 and V11. His point is that the program chooses; the automatic step is ours:
  drop the cell grid one step after a frame over budget, raise it after N fast frames.
  Estimate: holds a fixed frame time; speed gain equals the step ratio.

## 7. Dither so few colours read as more (WolfensteinCGA)

- `WL_SCALE.C:75-99`: `dithershift` 2 (4-colour CGA), 3 (mono), 4 (composite, Tandy);
  `nodither` switch `:96`. `:253-300`: the compiled scaler writes even rows, then odd
  rows, with the pattern rotated by `CL` (header comment `:174`), so one texture byte
  gives two colours.
- Maps: Y12 / S7. His form is ordered and costs a rotation in a register. Alternate two
  palette indices by row in the fill, not per pixel.
  Estimate: no extra bus if the pattern is already a mask on the card; otherwise a mask
  load per patch (an OUT is ~7.6 us). Unmeasured.

## 8. Fixed tick, frame skip, load bar (CatacombsCGA, Arduboy3D)

- `Main.cpp:395-460`: `tickAccum` collects elapsed ms; logic ticks in fixed steps of
  `1000/TARGET_FRAMERATE` (30, `Defines.h:50`), at most `maxFrameSkip = 5` per draw,
  then `tickAccum = 0`. Draw only if a tick ran. FPS counted per second (`:509-510`).
  `Arduboy3D/Source/Arduboy3D/Arduboy3D.ino:178-215` is the same loop, and with
  `DEV_MODE` draws a CPU-load bar into the frame (`arduboy.cpuLoad()`, `:205`).
- Maps: the game owns the loop. What is ours: a bar of the phase split (edge, sample,
  send, flip) drawn by the driver, readable by eye or photo without a log. Fits the
  visual numbered bench. Estimate: no speed gain; a measurement aid costing a fill or two.

## 9. Slow display: copy, queue, time-slice (MicroWeb)

- `MicroWeb/src/Render.cpp:84-160` `OnPageScroll`: on a scroll of N rows the hardware
  copies the surviving rows (`ScrollScreen`, `:143`; EGA/VGA
  `src/DOS/Surf4bpp.cpp:830-860` uses write mode 1, latch copy, no CPU per byte) and
  only the exposed N rows are redrawn. Queued items are shifted (`lowerClip -=
  scrollDelta`), not redrawn.
- `Render.cpp:240-320` `Update`: a queue of (node, upperClip, lowerClip), 512 entries
  (`Render.h:12`), drained inside `UPDATE_TIME_SLICE` = 200 ms (`App.h:33`), and
  stopped early when `HasInputPending()` (`:307`). Images go 16 lines per update
  (`:287`). `AddToQueue` (`:323`) merges overlaps by growing the clip. Mouse hidden once
  per batch (`:48`, `:79`, `:139`), not per draw.
- `src/Layout.cpp:31-100`: layout is incremental; a node is drawable as soon as its
  layout completes (`MarkNodeLayoutComplete`), so drawing starts before the page has
  finished loading.
- Maps, three parts:
  a) Copy, do not redraw (Y7, V8b): the Mach8 has a screen-to-screen blit. For a pan,
  copy the surviving area in one command and describe only the exposed strip. A tunnel
  fly-through is a zoom, not a shift, so little gain there.
  Estimate: a pan of N rows sends N/height of the fills.
  b) Producer/consumer queue: the edge pass appends to a bounded array and the sender
  drains it as fills are ready (Z15). Estimate: hides part of the ~120 ms of CPU time
  the FIFO sits empty; bus unchanged, RAM a few KB.
  c) One cursor hide per batch: check that the driver hides the V9x cursor once per
  frame, not per primitive. Not checked.

## 10. Data size (all repos)

- Offline conversion to the small form: `WolfensteinCGA/tools/cgaify.cpp` turns VGA
  assets into CGA ones on the PC; Arduboy3D `Generated/` holds 1-bit sprite data.
- `CatacombsCGA/Source/COTD/Generated/LUT.inc.h`: `int16_t` sine table;
  `FixedMath.cpp`: 16-bit xorshift RNG, no multiply.
- Maps: already Z15 and FastDoom item 3. Only new thing: keep tables 16-bit.

## 11. Measurement

- `WL_DEF.H:1295-1322`, `WL_DRAW.C:464,494,1156,1286,1385,1754,1807`:
  `BEGIN_PROFILE`/`END_PROFILE(PROF_*)` add `TimeCount` ticks per phase (3DRefresh,
  WallRefresh, ScalePost, DrawScaled, DrawWeapon, CGABlit); compiled out unless
  `WITH_PROFILER`. `CatacombsCGA/Source/COTD/Profiler.h` and `Main.cpp:521-530` print
  eight named sections per frame. `WOLF3DC.EXE timedemo` (README) replays a recorded
  demo as fast as possible and reports average fps, so builds see identical frames.
- Maps: we have phase timers. Our tunnel sample is deterministic, which is the same
  property; nothing to add.

## Checked, nothing found

- `doomtd3` (stripped Doom timedemo 3): README lists platform layers only; no renderer
  technique beyond FastDoom's.
- `Raycaster` (833-line DOS raycaster, `raycast.cpp`): plain Watcom C; nothing beyond
  Wolfenstein CGA.
- MicroWeb: no vsync, no flip; no per-technique speedup figure.
- WolfensteinCGA git history is one commit; no commit messages explain the choices.
- No source states a measured gain for any single technique.
- No ISA bus-width, I/O-wait or cache-flush handling in any repo.

## Candidates for the driver, cheapest first

1. Merge same-colour neighbours in the sampler (item 2): CPU, edge pass down; no bus or RAM change.
2. Phase-split bar drawn by the driver (item 8): measurement only; a fill or two of bus.
3. Automatic cell-grid step from last frame time (items 1, 6): fewer edges and fills, CPU and bus down, no RAM.
4. Bounded queue drained by the sender while the edge pass runs (item 9b): hides FIFO-starved CPU time; bus same, few KB RAM.
5. Screen-to-screen copy of the unchanged area on pans (item 9a): bus down in proportion; needs a blit probe.
6. Interlaced fields (item 4, = Y5): half the fills per frame; combing; owner's eye.
7. Cap primitives per frame, nearest kept (item 5): bounds the worst frame; correctness risk with unordered Direct3D.
8. Prebuilt command sequences by size class and colour pair (item 3, = Y9 at command level): CPU down, bus same, RAM up.
