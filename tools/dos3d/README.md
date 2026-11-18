# M8CUBE32 - flat-shaded cubes on the Mach8 from DOS

First DOS-side 3D program for the card (plan items P3.2 / P4.1, `docs/playable_3d_plan.md`).
Not yet run on the card or in a bed.

## What it does

Sets 640x480 at 8 bpp on the 8514 side from a plain DOS prompt with the init block of
`tools/m8seq/M8DOS8.ASM`, loads a 256-entry palette (black, a grey ramp, seven hues of 32
shades), and turns 1-8 flat-shaded cubes. The CPU transforms, culls, shades (Lambert, one light)
and builds the whole frame as a list of port writes; the engine draws every face.

- **Pages:** page 0 at row 0, page 1 at row 512, pitch 1024. A frame is drawn on the hidden page
  and shown by one `CRT_OFFSET_HI` (2EEEh) write queued after the frame's last write, then the
  vertical-blank latch (42E8h bit 0) is acknowledged. Before the next frame is sent the program
  waits for the latch, so it never draws on the page being shown (as `m8dd_page_frame` /
  `m8dd_page_wait` in `drivers/mach8_display/src/m8dd32.c`).
- **Clear:** each frame wipes only the box the back page held two frames ago.
- **Faces:** a face of 3 or more rows whose box is at most 383 px wide is drawn with the polygon
  fill (outline in one bit-plane of the scratch column x 640-1023, fill between its pixels, outline
  over the fill in the face's colour). A shorter one, or one too wide for the scratch, is
  drawn as one span a row. The program picks whichever needs fewer port writes.
- **Passes:** one per object (at most 8 faces, one scratch plane each): every scratch outline, then
  every fill, then every on-screen outline, then one clear of the planes used. Objects are drawn
  back to front by centre depth.
- **Timing:** BIOS tick (55 ms) from the first frame to engine idle after the last. Animation
  advances by frame, not time, so every run draws the same frames.

## Run on the card

1. Copy `M8CUBE32.EXE` to the card. The DOS/32A extender is bound into the EXE (the stub), so
   `DOS32A.EXE` is not needed beside it; copying it does no harm.
2. Boot to a plain DOS prompt with `INBRDPC.SYS` loaded (no Windows, no DOS box).
3. `M8CUBE32 [frames] [objects] [nv]`
   - `frames` default 600; any key stops sooner.
   - `objects` 1-8 cubes, default 1.
   - `nv` does not wait for the flip to take effect: measures throughput, frames tear.
4. Text mode, the DAC and `CRT_OFFSET` are restored on exit. The report is printed and
   appended to `M8CUBE32.TXT`: frames, ticks, seconds, fps, port writes and status reads a
   frame, faces by fill and by spans, writes a frame by kind, flip-wait polls and timeouts.

Suggested first runs: `M8CUBE32 300 1`, `M8CUBE32 300 1 nv`, `M8CUBE32 300 8 nv`.
With vsync on, one cube should be held at the refresh rate; `nv` shows what the bus and engine allow.

## Port-write budget (from the code; the program reports the measured numbers)

| Item | Writes |
|---|---|
| Quad face, polygon fill | 1 + 5 a non-horizontal edge (scratch) + 11 (fill) + 2 + 2 x 5 (outline) = **44** with 4 edges |
| Triangle face, polygon fill | 1 + 15 + 11 + 2 + 2 x 4 = **37** |
| Face by spans | 1 + 3 a row |
| A pass (an object) | 5 + 4 + 5 set-up, 11 scratch clear = **25** |
| A frame | back-page clear 10, flip 2 (+1 status read) |

One cube (3 faces showing) is about 3 x 44 + 25 + 12 = **~170 writes, ~1.3 ms of OUTs** at
7.6 us, against ~300 for the same cube as six `m8d3d_tri` triangles (50 each) with a scratch clear.
FIFO room is read once per 8 writes at most (`m8d3d_room`), plus the waits while the engine fills.

## Where it comes from

- Register sequences: `drivers/mach8_display/src/m8d3d.c` - `m8d3d_edges` (scratch outline, XOR,
  POLY + LAST_OFF, each edge top to bottom), `m8d3d_tri` (fill registers, rows strictly between
  top and bottom, outline over the fill), `m8d3d_outline`, `m8d3d_spans` (generalised to n
  corners), `m8d3d_q_clear`, the pass structure and shared set-up of `m8d3d_join` /
  `m8d3d_join_fill_begin` / `m8d3d_join_clear_part`, the queue send `m8d3d_q_send` and the FIFO
  count `m8d3d_room` (`m8dd32.c`).
- Flip: `m8dd_page_show`, `m8dd_page_wait`, `m8dd_spin`, page layout `M8DD_PAGE1_Y` / `M8DD_PAGE1_HI`
  (`m8dd32.c`).
- Mode and palette: `tools/m8seq/M8DOS8.ASM`; palette save and restore as `m8d3d_pal_load` /
  `m8d3d_pal_restore`.
- Scene: the fixed-point build of `tools/m8cube/M8CUBE.C` (Windows), at 640x480 with up to 8 cubes on
  a ring in depth.

## Unproven

- Nothing here has run. Port I/O from a DOS/32A program on this machine (`memprobe` did none).
- Whether `CRT_OFFSET_HI` and the 42E8h acknowledge pass through the FIFO or act at once; the
  Windows driver queues them the same way, so a flip may show a frame's last fill finishing.
- Fills sharing one `DP_CONFIG` write per pass, as `m8d3d_join` does (guide rule 9-41 asks for one
  before every extended blit; the join path runs on the card).
- The refresh rate of shadow set 1, and so the vsync-capped fps.
- Exit on a CPU exception: DOS/32A ends the program without the restore; a hung engine is reset
  (900Fh/500Fh) on the timeout path only.
- Depth order is by object centre: cubes are spaced so that it holds, not checked for every frame.
