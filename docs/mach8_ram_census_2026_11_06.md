# Mach8 display driver: system RAM census (2026-11-06)

Static read of `drivers/mach8_display/src` at HEAD (84af659). Nothing built or run.
Plan row: `docs/mach8_frame_rate_plan.md` Stage 1, R1-R4.

**Sources of the numbers.** Array sizes are computed from the source (16-bit `int` = 2,
`-mc` pointer = 4, M8DD32 `int` = 4, `RECT` = 16). Segment sizes are from
`src/build/m8disp.map` and the NE header of `src/build/M8DISP.DRV`, built 11-03 10:21 -
**stale**: `screen.c`, `enable.c`, `m8hw.c` changed 11-03 14:32, `fb16.c`, `dd16.c`,
`m8dd32.c`, `m8d3d.c` on 11-05. The hand sum of the DGROUP arrays (52,994 B + 796 B of
constants) is within 600 B of the map's 54,412, so the map is a fair guide. Rebuild before
quoting a code or DGROUP size.

**Windows 95 semantics.** Nothing in the repo documents what Windows 95 does with
`GMEM_FIXED` from a 16-bit driver. What the source shows: the flag is passed with
`GMEM_SHARE`, the block is `GlobalLock`ed, and only `m8_fb16_mem` and the code segment get
`GlobalSmartPageLock`. `docs/mach8_frame_rate_plan.md` R3 calls the frame buffer
"page-locked" - that is the call in `fb16.c:980`, not an observed lock count. Whether a
fixed block without the call is pageable, and whether its untouched pages are committed:
**unverified**. The "locked" column below says what the code requests, not what Windows does.

## 1. Segments (DRV)

| Segment | Size | Flags (NE header) | Notes |
|---|---|---|---|
| `_TEXT` | 64,858 | fixed, preload, shareable, not discardable | `build.ps1:91`. Page-locked by `GlobalSmartPageLock( cs )`, `enable.c:156`: the cursor runs at interrupt time. Near the 64 KB limit. |
| `M8DD_TEXT` (`dd16.c`) | not in the stale map; `dd16.obj` is 4,998 B, so ~3 KB of code (estimate) | fixed, preload, shareable (`build.ps1:92`) | Not covered by the SmartPageLock call, which locks the CS of `DriverInit` (`_TEXT`). Only run at mode set and DirectDraw set-up. |
| DGROUP | 54,412 (CONST 28 + CONST2 680 + `_DATA` 88 + `_BSS` 53,616) | fixed, preload, shareable (`build.ps1:90`, `oneautodata`) | One copy, every static below. Same size at 8 and 16 bpp: nothing is mode-conditional. No stack or local heap in the link. |
| Resources | `m8disp.res` 1,878 | - | Fonts, colour table, config. |

DRV file 69,040 B. Fixed code and data of the display driver: ~122,800 B (64,858 + ~3,500 +
54,412).

## 2. DGROUP statics over 256 B (all fixed, all load-time)

Size is the same at 800x600x16 and 800x600x8 unless a mode is named. "Use" is where the
code reads it; "16 bpp: dead" means the 16 bpp path passes the call to the DIB Engine
(`fb16.c:7-10`) and, from reading, never reaches the array - **not traced**.

| file:line | symbol | holds | bytes | use |
|---|---|---|---|---|
| screen.c:1101 | `m8_bc` | bitmap cache entries, 160 x 18 | 2,880 | 8 bpp only |
| screen.c:1104 | `m8_bcs` | cache shelves, 2 x 64 x 10 | 1,280 | 8 bpp only |
| screen.c:1108 | `m8_bc_seen` | first-sight hashes, 64 x 4 | 256 | 8 bpp only |
| screen.c:1636 | `m8_px` | bitmap proxies, 16 x 34 | 544 | 8 bpp only |
| screen.c:3016 | `m8_out_pts` | Output points, 1,024 x 4 | 4,096 | 8 bpp only |
| screen.c:3781 | `m8_sc` | string cache entries, 384 x 18 | 6,912 | 8 bpp only |
| screen.c:3782 | `m8_sc_seen` | 64 x 4 | 256 | 8 bpp only |
| screen.c:3963 | `m8_tk` | string key table, 256 x 54 | 13,824 | 8 bpp only |
| screen.c:3964 | `m8_tk_seen` | 256 x 4 | 1,024 | 8 bpp only |
| screen.c:4221-4223 | `m8_gc`, `m8_gc_bits` | glyph-table cache, 4 x 18 + 2,049 | 2,121 | 8 bpp only |
| screen.c:4680-4684 | `m8_d2d_tbl`, `_idx`, `_info` | DIB-to-device table, 256 + 256 + 1,064 | 1,576 | 8 bpp only |
| screen.c:4992 | `m8_ident` | identity 0-255 | 256 | 8 bpp only |
| screen.c:90-93 | `m8_cur_and/xor/save/draw` | pointer masks and save, 128 + 128 + 1,088 + 1,088 | 2,432 | both (the 16 bpp pointer is screen.c's) |
| screen.c:494 | `m8_cur_img` | keyed pointer image | 1,024 | both |
| screen.c:73 | `m8_stats` | counters, 20 x 4 | 80 | both |
| m8hw.c:33 | `m8_xrow` | band translated for one `rep outsw` | 4,096 | both |
| m8hw.c:400 | `m8_q` | register write queue, 64 x 2 | 128 | both |
| m8.h:66 | `m8_hot` | hot fields | 64 | both |
| fb16.c:41 | `m8_fb16_pal` | 16 bpp palette | 1,024 | 16 bpp only (copied to the card at 16 bpp) |
| fb16.c:113 | `m8_fb16_cube` | cube entry to index | 225 | 16 bpp only |
| fb16.c:124-125 | `m8_fb16_y_lv`, `_y_ix` | grey ramp | 66 | 16 bpp only |
| fb16.c:129-132 | `m8_fb16_r/g/b/y` | dither parts per threshold, 512 + 1,024 + 512 + 1,024 | 3,072 | 16 bpp only |
| fb16.c:133 | `m8_fb16_band` | row buffers for check/send | 4,096 | 16 bpp only |
| fb16.c:268 | `m8_fb16_src` | one row of 5-6-5, 800 x 2 | 1,600 | 16 bpp only |
| fb16.c:88,137,74 | `dirty`, `stat`, `rd` | rectangles, counters, row-diff block | 62 | 16 bpp only |

Subtotals (hand sum): 8 bpp-only 35,025 B; 16 bpp-only 10,145 B; both 7,824 B.
So 35,025 B of fixed data are idle at 16 bpp, and 10,145 B idle at 8 bpp.

Small statics (under 256 B) and constants make up the rest to 54,412.

## 3. Allocations

All `GlobalAlloc` in the driver are listed; there is no `GlobalFix`, `GlobalPageLock`,
`VirtualAlloc`, `HeapAlloc` or `LocalAlloc`. `GlobalLock` on a `GMEM_FIXED` block returns
its selector (unverified whether it does more).

| file:line | symbol | holds | bytes 800x600x16 | bytes 800x600x8 | flags and lock | when | freed | use |
|---|---|---|---|---|---|---|---|---|
| fb16.c:975 | `m8_fb16_mem` | the frame buffer GDI draws in and the card is sent from | 960,000 (800 x 2 x 600 always, also at 640x480) | 0 (never called) | `GMEM_FIXED \| GMEM_SHARE \| GMEM_ZEROINIT`; `GlobalSmartPageLock` (:980); `GlobalLock` | `Enable` style 0 at 16 bpp (`enable.c:354`), once; `m8_fb16_clear` writes all of it at each size change (`enable.c:539`) | **never** (no `GlobalFree` on it; survives Disable) | 16 bpp, desktop and DirectDraw primary |
| fb16.c:986 | `m8_fb16_hmem` | row fingerprints, 600 rows x 25 segments x 4 | 60,000 (the same at 640x480; 38,400 would suffice there) | 0 | `GMEM_FIXED \| GMEM_ZEROINIT`; no page lock; `GlobalLock` (:989). Allocated, and used only if < 64 KB | same call as the frame buffer | **never**. If the lock were skipped the handle would leak; not reachable at 60,000 B | 16 bpp. `m8_fb16_hash == NULL` makes every send whole |
| fb16.c:1101 | `m8_fb16_bmem` | DirectDraw's back buffer | 960,000 (current `m8_scr_x * 2 * m8_scr_y`) | 0 | `GMEM_FIXED \| GMEM_SHARE`, no `ZEROINIT`, no page lock; `GlobalLock` | batch op 3, from `m8dd_create16` (`m8dd32.c:1490`): CreateSurface of a flip chain of two | batch op 4, from `m8dd_destroy16` (`m8dd32.c:1533`), when DDRAW calls DestroySurface for the back surface. Not freed otherwise; whether DDRAW reaches that call when a program exits without releasing: **unverified** | 16 bpp DirectDraw (Direct3D rides on the flip chain) |
| screen.c:201 (twice) | `m8_dst_mem`, `m8_src_mem` | the two 8 bpp strips | 0 (`m8_screen_init` is not reached at 16 bpp, `enable.c:353-367`) | 2 x 65,520 = 131,040 | `GMEM_FIXED \| GMEM_SHARE \| GMEM_ZEROINIT`; no page lock; `GlobalLock` + `AllocSelector` | `Enable` style 0 at 8 bpp, via `m8_screen_init` (`screen.c:5385`); alloc skipped if the selector exists | `Disable` -> `m8_screen_done` (`screen.c:5441`) | 8 bpp only |
| dd16.c:97 | `m8dd` (`M8DD_SHARE`) | HAL description block shared with M8DD32 | ~830 (108 head + DDHALINFO ~480 + callback tables 128 + 3 modes 108; **estimate**, headers read not compiled) | same | `GMEM_FIXED \| GMEM_SHARE \| GMEM_ZEROINIT`; no page lock | first `Control( DCICOMMAND )`, `m8dd_command` (`dd16.c:381`): first DDRAW query, any depth | **never** ("stays for the session", `dd16.c:88`) | DirectDraw, both depths |
| dd16.c:132 | `m8dd_shadow` | 8 bpp Lock shadow: screen plus the heap rows, at the screen pitch | 0 (not allocated at 16 bpp, `dd16.c:402-409`) | 800 x (632 + 232) = 691,200 (640x480: 640 x 864 = 552,960) | `GMEM_FIXED \| GMEM_SHARE \| GMEM_ZEROINIT`; no page lock | `DDCREATEDRIVEROBJECT` at 8 bpp, i.e. the first DirectDraw object | **never**; a larger one replaces nothing (the smaller block is also kept, `dd16.c:111`) | 8 bpp DirectDraw only |

`m8_dd_heap` at 800x600: string cache top = 992 - 128 = 864; rows = 864 - 600 - 32 = 232,
top = 632. Card memory (off-screen rows, Direct3D scratch, bitmap and string caches) is not
system RAM and is not counted.

## 4. M8DD32.DLL

Built `-bd -bt=nt`, base 0xB0C00000, every section marked shared (`build.ps1:138,148`): it
lives in the shared arena. Loaded when DDRAW first creates a driver object
(`DDGET32BITDRIVERNAME`). Whether Windows 95 unloads it when DDRAW releases the driver, and
whether the BSS is committed before it is touched: **unverified**.

Stale DLL (11-03, before `m8d3d.c` was linked in): code 27,756 B, `.idata` 1,344, data
10,721 virtual, image 57,344 B. The current image is larger; the data below is computed from
HEAD.

| file:line | symbol | holds | bytes | when / freed | use |
|---|---|---|---|---|---|
| m8dd32.c:581 | `m8dd_tri_v[2][3072]` (short) | each buffer's recorded triangles, 512 x 6 | 12,288 | DLL load; freed with the DLL | Direct3D |
| m8dd32.c:582-583 | `m8dd_tri_i`, `m8dd_tri_c` (WORD, 2 x 512 each) | card palette index and 5-6-5 colour | 2,048 + 2,048 | same | Direct3D |
| m8dd32.c:584 | `m8dd_card_box[512]` (RECT) | boxes of the triangles on the card | 8,192 | same | Direct3D |
| m8dd32.c:586-587 | `m8dd_card_v`, `m8dd_card_i` | the card's triangles | 6,144 + 1,024 | same | Direct3D |
| m8dd32.c:577,579 | `m8dd_desc[2]`, `m8dd_card` | frame descriptions, 3 x 160 | 480 | same | DirectDraw 16 bpp |
| m8dd32.c:248 | `m8dd_held[8]` | Unlock write-back rectangles, 8 x 20 | 160 | same | DirectDraw |
| m8dd32.c:1751 | `m8dd_placed[32]` | off-screen placements, 32 x 20 | 640 | same | 8 bpp DirectDraw |
| m8dd32.c:618 | `m8dd_bl` | batch list, 145 WORD | 290 | same | 16 bpp DirectDraw |
| m8dd32.c:1576 | `row` (function static) | host-data row, 513 WORD | 1,026 | same | 8 bpp DirectDraw |
| m8d3d.c:251-253,262 | `m8d3d_global`, `_callbacks`, `_ctx[4]`, `_stat[20]` | D3D HAL tables, 4 contexts, counters | ~600 (192 + 140 + 192 + 80; estimate) | same | Direct3D |

Data total ~34,900 B. The triangle arrays are 31,744 B of it. Stack use (`cx`/`cy` locals,
`m8dd32.c:1774`, 520 B) is not counted.

Fingerprint tables: the only one is `m8_fb16_hmem` (row 2 of section 3). `hash.asm` and
`M8_FB16_ROWDIFF` read it by selector, 4 bytes per segment (`fb16.c:429`).

## 5. Totals by scenario

Base (always, display driver): `_TEXT` 64,858 + `M8DD_TEXT` ~3,500 (estimate) + DGROUP
54,412 = **122,770**. All of it requested fixed. Excludes M8DISP.DRV resources, the DIB
Engine, GDI, and DDRAW's own allocations.

| Scenario | Components | Bytes |
|---|---|---|
| (a) 8 bpp desktop | base + strips 131,040 | **253,810** |
| (b) 16 bpp desktop, no DirectX | base + frame buffer 960,000 + fingerprints 60,000 | **1,142,770** |
| (c) 16 bpp, after a Flip program exited | (b) + `M8DD_SHARE` ~830. Back buffer already freed by `m8dd_destroy16` | **~1,143,600** |
| (c') the same, if DestroySurface never reached the back buffer | (c) + back buffer 960,000 (614,400 if the program ran at 640x480) | ~2,103,600 (~1,758,000) |
| (d) during Direct3D at 640x480x16 | (c) + back buffer 614,400 | **~1,758,000** |
| (d') during Direct3D at 800x600x16 | (c) + back buffer 960,000 | ~2,103,600 |
| extra: 8 bpp after a DirectDraw program | (a) + shadow 691,200 + `M8DD_SHARE` ~830 | ~945,800 |

Not in these totals: M8DD32.DLL image if resident (code ~28 KB+, data ~35 KB, ~65-75 KB
estimate for the current build; loaded from DirectDraw first use in (c), (c'), (d)), and
anything DDRAW or Direct3D allocate themselves. In (d) at 640x480 the frame buffer is
960,000 B but only 614,400 B of it is addressed, 345,600 B idle; `m8_fb16_clear` writes all
of it at each mode change, so the idle part is touched too.

## 6. Candidates

Savings are the request sizes above; whether a smaller request becomes less paged memory
depends on the unverified Windows behaviour. Test by `paged` in NOAG.TXT and PERFLOG, as
the plan says.

| # | Candidate | Saves | What would have to change |
|---|---|---|---|
| 1 | **Back buffer `m8_fb16_bmem` never written** (described frames) | 614,400 at 640x480; 960,000 at 800x600, DirectDraw only | DDRAW takes `fpVidMem` at CreateSurface (`m8dd32.c:1500`) and the program may Lock the back surface and write pixels, so some RAM must exist. Needs address space without committed pages and commit on demand: `VirtualAlloc( MEM_RESERVE )` and commit from M8DD32, or a smaller real block with a fault path. Neither is in the source; Windows 95 behaviour in the shared arena: unverified. Everything that reads or writes RAM for a buffer (`m8dd_tris_to_ram`, `m8dd_ready_all`, `m8_fb16_send_rect` after op 2) has to ask first. Zero saving for a program that Locks and draws. |
| 2 | **Frame buffer only at 16 bpp** | 0 | Already so: `m8_fb16_alloc` runs only for `m8_fb16`. The 8 bpp desktop pays the strips instead (131,040). |
| 2b | Idle statics by depth (extends 2) | 35,025 at 16 bpp; 10,145 at 8 bpp | Move the 8 bpp caches (`m8_tk` alone is 14,848) and the fb16 tables into blocks allocated by `Enable` for their depth, reached by far pointer. The 8 bpp hot paths read these arrays through near DS offsets; each access gains a pointer load. Needs a trace that none is read at 16 bpp. |
| 3 | **Size to the current mode** | frame buffer 345,600 and fingerprints 21,600 at a 640x480 desktop; 0 at the current 800x600 desktop. Back buffer is already sized to the mode (`fb16.c:1102`) | Allocate `m8_scr_x * 2 * m8_scr_y` and `nseg = m8_scr_x / 32`. A live 640 <-> 800 change (`ReEnable`) then reallocates; `fb16.c:973` records that DirectDraw holds the linear address, and `m8dd_mode_changed` re-supplies it with `bReset` set. Whether DDRAW honours a new `fpPrimary` there: unverified. `m8_fb16_clear` and the selector in `m8_fb16_dev` change with it. |
| 4 | **Free on the last DirectDraw exit** | at 16 bpp ~830 (`M8DD_SHARE`) and the DLL if the driver could unload it; back buffer already freed in `m8dd_destroy16`. At 8 bpp 691,200 (shadow) | Count DirectDraw objects (`DestroyDriver`, `m8dd->destroy_driver`). The 16-bit side keeps both blocks because the 32-bit HAL holds their addresses (`dd16.c:88,111`); freeing needs M8DD32 to drop every cached pointer first. The frame buffer cannot be freed: it is the desktop. Also close the gap in (c'): free the back buffer from `Disable`/`ReEnable` too. |
| 5 | **Fingerprint table** | 60,000 if removed; 30,000 with WORD hashes or 64-pixel segments; 15,000 with both; 38,400 sized to 640x480 | Removal: delete the `GlobalAlloc` at `fb16.c:986`; the `NULL` path already sends whole, at 3.8 us per pixel of bus (`fb16.c:55`) - the cost is bus time, not correctness. WORD hashes: change `hash.asm`, the `* 4` stride at `fb16.c:429`, and the colour hash; a 16-bit hash lets more changed pieces compare equal. Larger segments: `M8_FB16_SEG_W` 32 -> 64 sends twice the pixels per change. |
| 6 | Direct3D arrays (plan R4) | 31,744 now; 23,808 at 128 triangles; or `m8dd_card_box` (8,192) derived from `m8dd_card_v` | Lower `M8DD_TRIS_MAX` (`m8dd32.c:565`) - check the limit a test program reaches; or allocate the arrays at the first `ContextCreate` and free at the last `ContextDestroy`. In the DLL's data only, so it counts if BSS is committed: unverified. |
| 7 | Strips at 8 bpp | up to 65,520 | One strip instead of two, or smaller strips. Not examined: `m8_dst` and `m8_src` are used together in copies. |
| 8 | `M8DD_TEXT` | ~3,000 | Fixed and preload by `build.ps1:92`; only run at set-up. Make it moveable or discardable. Unverified what Windows 95 does with a fixed code segment that is not page-locked, and whether the set-up runs when memory is tight. |

Largest first, at 16 bpp: 1 (614,400-960,000, DirectDraw only), 5 (30,000-60,000, always),
2b (35,025, always). Only 1 and 5 change what (d) and (b) hold by more than a few percent;
the frame buffer (960,000) is needed whenever the desktop is 16 bpp.
