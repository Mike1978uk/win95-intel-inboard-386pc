# Stunts through m8dos: patch plan (desk research, 2026-11-18)

Nothing was built or run. Source read: a `--depth 1` clone of restunts (single commit `5c38f25`, "fix git command") and `tools/dos3d/m8dos.h`, `m8dos.c` (grep only). Per-frame primitive counts below are limits from the source, not measurements.

## 1. Build

| Item | Finding |
|---|---|
| Assembler / linker | TASMX (16-bit) or TASM32; TLINK (default) or WLINK (experimental). `src/restunts/makefile` |
| C compiler | Borland `bcc /c /u- /mm /v /DRESTUNTS_DOS`, medium model, 16-bit real mode. `src/restunts/c/makefile`, `dos/makefile` (`-mm`) |
| Host | Tools bundled in `tools/bin` (TASM, TLINK, bcc, make, DOSBox, wlink, td). 16-bit tools run in DOSBox; works on 64-bit Windows. Drive letter `S:` is hardcoded; `tools\mount_stunts_to_s.bat` |
| Output | `restunts.exe` (patched asm + C). `restunts-original` target = unpatched asm, behaves as original |
| In C | `src/restunts/c/`: fileio, math, memmgr, restunts (main/loop), shape3d (5067 lines), shape2d, keyboard, heapsort, frame (`update_frame`, 1204 lines), state, stateply, statecrs, statecar. 16.6k lines |
| Still asm | 41 `seg0NN.asm` files in `src/restunts/asm/`. Rasterisers, lines, spheres, wheels, sprite blits, fonts, dashboard, `skybox_op`, `get_a_poly_info` are asm. seg012 is 18980 lines |
| Port method | register in `anders.idc`, C stub, `extrn` in `asm/custom.inc` (readme) |
| 32-bit | None. `fake16bit`, `execombiner`, `drvcombiner`, `sdl/` (Win32 `cl` build) exist; no protected-mode build |

## 2. Renderer

Frame flow (all 16-bit, 320x200, 8 bpp, mode 13h; coordinates are `int` pairs):

| Step | Routine | File |
|---|---|---|
| Main loop, window select | `setup_mcgawnd2()` makes `sprite1` = RAM window; `sprite_copy_wnd_to_1()` when `video_flag5_is0==0` | `c/restunts.c` ~L950-1100 |
| Frame | `update_frame(arg_0, &rect_windshield)` | `c/frame.c` |
| Transform + cull + sort | `transformed_shape_op()` per shape (tile terrain, objects, cars, wheels). `transformed_shape_add_for_sort`. Output = `polyinfo` records, sorted linked list `poly_linked_list_40ED6[0x190]` (limit 400 polys, 0x28A0 bytes polyinfo) | `c/shape3d.c` L142, L2574, L2668 |
| Sky | `skybox_op()` (asm, `seg003.asm`), polygons via `skybox_op_helper` | `c/shape3d.c` L2784 |
| Draw in sorted order | `get_a_poly_info()` (asm `seg006.asm` L2062; C version under `#if 0`). Per record `polyinfo[4]` = type: 0 poly, 1 line, 2 sphere, 3 wheel, 5 skip, else pixel. Poly pattern byte: 0 solid, 1 patterned, else "unk" | `c/shape3d.c` L2679 |
| Rasterise poly | `preRender_default`, `_alt`, `skybox_op_helper`, `preRender_wheel_helper4` -> `preRender_default_impl(color, nverts, int xy[], flag)` (C, L2824) | `c/shape3d.c` |
| Edge walk | `draw_line_related`/`_alt` + `generate_poly_edges` + `preRender_default_impl_helper` fill two per-row arrays `[480]` x-left/x-right | `c/shape3d.c` L2959-3920 |
| Span write | `spritefunc` = `draw_filled_lines(x1arr, x2arr, y, n, color)`: `rep stosb/stosw` into `sprite1.sprite_bitmapptr`; `draw_patterned_lines`; `draw_unknown_lines` | asm `seg012.asm` L13372, L14084, L10546 |
| Lines | `preRender_line(x0,y0,x1,y1,col)` (also degenerate polys) | asm `seg012.asm` L2891 |
| Sphere / wheel / pixel | `preRender_sphere` L10164, `preRender_wheel` (`seg022`), `putpixel_single_maybe` L17884 | asm |
| Explosion | `shape_op_explosion(scale, shape, cx, cy)` | asm `seg012.asm` L13556 |
| Windscreen crack / sinking | `init_crak`, `do_sinking` | asm |
| Text | `intro_draw_text`, `draw_ingame_text`, `font_draw_text` (bitmap fonts `FONTDEF.FNT`, `FONTLED.FNT`) | asm `seg012.asm` L11082 |
| Dashboard / roof | `setup_car_shapes(1)` draws roof+dashboard (redrawn only while `byte_454A4` > 0, i.e. 2 frames after a change); `setup_car_shapes(2)` every frame: `shape2d_render_bmp_as_mask`, `sprite_putimage_and/or`, `preRender_line` x2 (gauge needles); `dasmshapeptr` mask | asm `seg005.asm` L2009-2959; `c/restunts.c` L1040-1075 |
| Present | `setup_mcgawnd1()`: `sprite_set_1_from_argptr(&sprite2)` (screen) + `sprite_putimage(mcgawndsprite->bitmap)` = 64000-byte copy to A000. `byte_44346 ^= 1` page toggle | `c/shape2d.c` L208 |

Other modes: `video_set_mode_13h` (int 10h AX=0013h, `seg012` L11192), `video_set_mode4` (CGA, B800), `video_set_mode7` (B800/3B4). Palette: `video_set_palette` = int 10h AX=1012h (0-63 DAC, same as `m8_palette`).

Clip: `sprite1` window (left, width 0x140, top, height) is read at the top of `preRender_default_impl`; polygons are clipped to it by the edge walk (`draw_line_related` vs `_alt`). The 3D window is `rect_windshield` (top = roof height, bottom = `dashbmp_y_copy`; 200 when no dashboard).

Primitive kinds per frame: filled convex poly (solid, the bulk), patterned poly, line, sphere, wheel (ellipse), single pixel, explosion bitmap, sky/ground polys, bitmap blits (dashboard mask+image, windscreen crack, text glyphs), 2 gauge lines. Counts per frame: not in any source; max 400 polys. Measure with a counter in the hook (step 1 below).

## 3. Cockpit, menus: bytes per frame

| Part | Bytes (derived) | Per frame |
|---|---|---|
| Present copy to A000 | 64000 (320x200) | every frame, CPU RAM to VGA, not a card path |
| Dashboard + roof bitmaps | up to 64000 minus 3D window; not measured | static: redrawn only after a change (2 frames), under `byte_454A4` |
| Gauge needles | 2 lines | every frame |
| Text (timer, `draw_ingame_text`) | glyph blits, size not measured | every frame (timer), HUD text when shown |
| Windscreen crack / sinking | not measured | crash only |
| Menus, dialogs | full 320x200 shape2d images (`.PVS`, `.RES` shapes) | on change only |

Consequence for the card: pushing the 64000-byte window through the data port at the measured 5.695 us/byte (memory `iowidth`) = 365 ms. Any 2D that changes per frame must be drawn by engine commands (blit from card memory, rect, line), not uploaded.

## 4. Patch plan

Which display shows what: the 8514 side and the VGA side share the monitor (m8_open/m8_close switch). So "3D on the card, menus on VGA" means switch on entering and leaving a race, not per frame. In the race everything visible (dashboard, timer, HUD) must be on the card side.

Hooks (all after transform/sort, which stay as is):

| # | Hook | What it does |
|---|---|---|
| H1 | Entry to race (`init_game_state`/`setup_car_shapes(0)` path, `c/restunts.c` L780-830) and exit (`free_player_cars`) | `m8_open` / `m8_close`; set `m8_palette` from the game palette |
| H2 | Start and end of `update_frame` | `m8_frame_begin`, `m8_frame_end(1)`; skip the A000 copy (`setup_mcgawnd1`) in race |
| H3 | `preRender_default_impl` (C, one place for 4 callers) | scale vertices x*2, y*12/5, `m8_poly`; `m8_clip` from `sprite1` window. Drop edge walk and `spritefunc` |
| H4 | `preRender_line`, `preRender_sphere`, `preRender_wheel`, `putpixel_single_maybe` (asm; new C stub + `extrn`) | `m8_line`; sphere/wheel as 12-16 gon `m8_poly`; pixel as 2x2 `m8_rect` |
| H5 | `draw_patterned_lines` | solid fill first (pattern not in `m8dos.h`); check later whether a Mach8 pattern register is worth it |
| H6 | `shape_op_explosion`, crack, text, dashboard | not in `m8dos.h`: need card-memory offscreen upload once, then engine blit with AND/OR mix (the game already uses `sprite_putimage_and`/`_or`). New `m8dos` API: `m8_upload`, `m8_blit`. Defer; milestone 1 does without |

Milestone 1 skips H6: camera mode without dashboard (`cameramode != 0`, or `dashb_toggle` off) gives sky, ground, road, cars, no bitmaps except the timer. Timer and text: hide in milestone 1.

Aspect: 320x200 is a 4:3 display, so x*2 and y*2.4 fill 640x480. Shared edges keep the same scaled vertices, so no gaps are expected between neighbours; not verified.

16-bit game, 32-bit library: options.

| Option | Cost / risk |
|---|---|
| A. 32-bit DOS/32A server called by interrupt | Game uses almost all of the 576 KB conventional memory (`mmgr_get_res_ofs_diff_scaled`, `0xFA00` window check); the extender plus a DPMI host must stay resident. Each call crosses real/protected mode; ~400 polys a frame. DOS/32A is not built as a TSR. Highest risk; not recommended |
| B. Real-mode port of the primitives | `m8dos.c` uses only I/O ports (`out dx,ax`), `int 10h` (restore text mode), `signal`, `long` fixed point, and flat memory for the command queue (874 lines; only `m8dos.h` read in full, `m8dos.c` grepped). The card has no memory access, so none of it needs 32-bit addressing. 386 instructions (`66h` operand prefix) are available on the Inboard 386/PC and the 486. Restunts already builds `-mm` 16-bit C with `bcc`. **Recommended** |
| C. Port restunts C parts to 32-bit | Only 13 of ~55 modules are C; the rest is segmented asm with far calls. No |

Recommendation: B. New module `m8hook.c` built with `bcc -mm` beside the restunts C files, holding a 16-bit copy of the m8dos queue (polygon fill, rect, line, clip, frame, palette), with the 4 hot `out` sequences in inline/TASM 386 code. Hooks H1-H4 are source edits in `c/shape3d.c`, `c/frame.c`, `c/restunts.c`, so a binary patch of the exe is not needed. Open Watcom 16-bit OMF into TLINK is unverified; Borland is the safe compiler.

Open risks: (a) `bcc` long-multiply helpers cost CPU; keep the edge scaling in 386 asm if it shows; (b) no measurement of polys a frame; (c) pattern and per-frame 2D need new card-side API; (d) whether the Mach8 8514 side keeps the VGA state across the race (m8_open restores on close, tested only from DOS/32A programs).

First concrete step: build restunts unmodified in DOSBox, then add a counter in `preRender_default_impl` and log polys, lines, spheres, wheels, pixels and vertices a frame for one lap (read-only; also gives max vertex count). That sizes H3-H5 before any card code is written.

## 5. Legal

| Item | Finding |
|---|---|
| Original files required | `GAME.EXE` is rebuilt from a patched disassembly; the data files (`*.RES`, `*.PVS`, `*.P3S`, `*.PRE`, `*.VCE`, `*.DRV`, `*.FNT`, `*.TRK`, `*.RPL`) come from a Stunts 1.1 install. The `4d-stunts/restunts` repo itself ships a `stunts/` folder with `game.exe`, `game_drv.exe` and all data files. Whether the owner holds a licensed copy is unknown. Using the repo's `stunts/` directory avoids needing the owner's disks but copies copyrighted game data |
| Restunts licence | None found. No `LICENSE` file in the tree (`ls -a`), no licence text in readme, C, asm or tools (grep for licen/copyright/abandon). The GitHub licence API returns 404. The wiki states CC BY-NC-SA 4.0 for wiki content only |
| Consequence | The source is "all rights reserved" by default. A private build for the owner's own test is low risk; do not redistribute a patched exe or the `stunts/` data, and ask the authors before publishing patches. Bundled Borland tools in `tools/bin` have their own licence |

## Sources

| Source | Gave | Did NOT contain |
|---|---|---|
| https://github.com/4d-stunts/restunts (cloned; `readme.md`, `src/restunts/makefile`, `c/makefile`, `dos/makefile`, `c/shape3d.c`, `c/frame.c`, `c/restunts.c`, `c/shape2d.c`, `asm/seg003/005/006/012.asm`, `tools/readme.txt`) | Toolchain, layout, all routine names and arguments above | A licence; per-frame counts; build run (not attempted); any 32-bit build; documentation of the asm rasterisers beyond IDA comments |
| https://github.com/xor2003/restunts (WebFetch) | Same tool chain (TASMX/TLINK, Borland C++ 5.2, make) | Licence; differences from 4d-stunts; any 32-bit/protected-mode work |
| https://wiki.stunts.hu/wiki/Restunts (WebFetch) | Goals, status ("translation to C minimal as of Nov 2023"), wiki CC BY-NC-SA, original video-driver files merged into the exe | Source licence, toolchain, rendering, protected mode |
| https://wiki.stunts.hu/wiki/Rendering_engine (WebFetch) | Two layers: `update_frame` (`frame.c`), `transformed_shape_op` (`shape3d.c`) | Polygon drawing, VGA, sprites, buffers, primitive types |
| `tools/dos3d/m8dos.h`, `m8dos.c` (local) | API; `m8dos.c` has only port I/O, int 10h, signal | `m8dos.c` was grepped, not read in full: write costs per primitive not re-checked |
| Search results (forum.stunts.hu topic 3355, msg 80558, wiki R5K engine) | Not opened | Not read; listed only as leads |
