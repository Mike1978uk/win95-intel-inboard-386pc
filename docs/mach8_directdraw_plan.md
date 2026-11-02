# DirectDraw on the Mach8 - plan

Started 2026-11-01. Research: `docs/directdraw_research_2026_11_01.md`. Worklist item D5. Mach8 half
first, VGA half second (owner, 10-16).

## Shape

Windows 95 DirectDraw calls a 32-bit HAL; the 16-bit driver only hands it over:

- **16-bit (M8DISP.DRV)**: `Control(DCICOMMAND)` with `DD_VERSION` - `DDNEWCALLBACKFNS` (keep DDRAW's
  `lpSetInfo`), `DDGET32BITDRIVERNAME` (`"M8DD32.DLL"`, `"DriverInit"`, context = linear address of a
  shared block), `DDCREATEDRIVEROBJECT` (fill `DDHALINFO`, call `lpSetInfo`), `DDVERSIONINFO`. Today
  `enable.c` answers 0, so DDRAW runs the HEL alone.
- **32-bit (M8DD32.DLL)**: `DriverInit(context)` writes its callback addresses into the shared block; the
  16-bit side copies them into the callback tables with their `DDHAL_*CB32_*` valid bits (as
  vmdisp9x `dddrv.c` does). Loaded in the shared arena: image base above 0x80000000 and every section
  shared (vmhal9x links at 0xB00B0000 and sets the flags with `fixlink -shared`, MIT).
- Headers: vmdisp9x `ddk/ddrawi.h` (Microsoft DDK text) for the 16-bit side, vmhal9x
  `ddrawi_ddk.h` for the 32-bit side. Local only (`references/jhrobotics`), copied in when used.

## The Mach8 half has no CPU-visible memory

Every surface lives in card memory reachable only through the engine and PIX_TRANS. So:

- **Primary Lock = a shadow.** Lock reads the rectangle into a system-RAM buffer (host read, as
  `m8_hw_read_rect`), returns that buffer in `lpSurfData`; Unlock writes it back (host write). Slow
  per byte (8-bit bus), correct; apps that blit from video memory stay fast.
- **Off-screen surfaces in card memory**: 1 MB less the 800x600 screen and the driver's caches. The
  off-screen allocator is the one the bitmap cache uses; DirectDraw gets a fixed share of it.
- **Blt** (fill, VRAM-VRAM copy, colour key via COLOR_CMP) on the engine, the same short paths GDI uses.
- **Flip**: CRT_OFFSET 2AEE/2EEE (bytes/4), not yet card-probed; an ADVFUNC_CNTL write resets it on
  Mach8 (guide 7515-7523). Two 800x600 pages fit in 1 MB only with the caches given up; 640x480 fits
  three. Probe needs the owner at the screen.

## Sharing the engine with GDI

GDI's entries run under the Win16 lock and keep register shadows in `m8_hot`. The HAL must never
overlap them:

- Every HAL entry that touches the engine takes the Win16 lock (`GetpWin16Lock`, `_EnterSysLevel` /
  `_LeaveSysLevel`, KERNEL32), sets `m8_hot.busy` (the interrupt-time cursor then defers), and on the
  way out clears the shadows (`sh_ok`, `dp_ok`, `room_ok`, `rd_ok`, `mm_w/h`) so GDI's next call
  sets the engine up again. A pending cursor move stays pending for GDI's next `M8_LEAVE`.
- `m8_hot`'s linear address goes to the HAL in the shared block (16-bit side: selector base + offset).

## Milestones

1. **Handshake.** M8DD32.DLL with `DriverInit` and `DestroyDriver` only, caps 0, `fpPrimary` = the
   shadow buffer's linear address. Pass: DDRAW loads the DLL (a marker it writes), `dxdiag` or a test
   app sees a HAL, GDI unaffected (M8BENCH pixels = ATI in all 35).
2. **Primary Lock/Unlock shadow** and a test app that locks, draws, unlocks; pixels checked by
   reading the screen back.
3. **Blt**: colour fill and VRAM copy, off-screen surfaces, colour key.
4. **Flip** after the card probe; WaitForVerticalBlank from the raster counter.
5. VGA half: VFLATD banked surface (research item: whether VFLATD yields a usable `fpVidMem`).

Test bed first for the handshake (no card risk), card for everything timed.

## State (2026-11-01)

Milestone 1 is built, not run: `dd16.c` (hand-over), `m8dd32.c` (M8DD32.DLL, based 0xB0C00000, every
section shared, exports `DriverInit`), `enable.c` routes `DCICOMMAND`, `tools/m8ddtest/M8DDTEST.C`
prints the caps (pass: hardware caps without DDCAPS_NOHARDWARE). DirectX is not installed on the
card.

**Milestone 1 passes in the bed** (`vm_5160_mcp`, DirectX 7.0a installed by the owner, driver
`0aa4a87`): M8DDTEST prints `DirectDrawCreate 0`, `hw caps 00000400` (DDCAPS_GDI) `caps2 00080000
pal 00000014 scaps 00000300 vidmem 0  HAL`. Bed image before DirectX: `vm_5160_mcp/card.img.pre_dx7`.

Using the primary fails, as expected without Lock: dxdiag's DirectDraw test stops at step 11 (colour
fill to the primary) with DDERR_EXCEPTION, and M8DDTEST hangs creating or locking the primary. After
those runs GDI fills on the screen go missing (Start menu highlight: text background only) until a
restart. Bisected in the bed: `d26ed72`, `0e66ded`, `a517d00` draw it right; `0aa4a87` draws it right
after a fresh boot too, so it is the failed DirectDraw use, not a GDI path. Likely a primary lock left
held, so the DIB Engine treats the screen as busy. Milestone 2 must:

- allocate the Lock shadow when first needed, sized to the mode, and check it (today one 768 KB fixed
  `GlobalAlloc` at the first DCICOMMAND, result unchecked: a failure gives `fpPrimary` 0);
- have M8DDTEST flush each line, so a hang shows its step.

**Milestone 2 passes in the bed** (2026-11-01, `vm_5160_mcp`): `M8DD32.DLL` handles Lock/Unlock of the
primary (identified by `fpVidMem` = the shadow's linear address; anything else goes back to DDRAW). Lock
reads the rectangle from the card into the shadow (x and width widened to even), Unlock writes it back,
then GDI's engine shadows in `m8_hot` are forgotten. M8DDTEST round trip: GDI black read back through a
lock (0 of 2048 wrong); white written through a full lock and through a rect lock at odd x read back by
GetPixel, neighbours at x 200 and 233 untouched. M8BENCH straight after: pixels = before in all 36, so the
lost-fills symptom is gone. Relies on DDRAW holding the Win16 lock from Lock to Unlock (not with
DDLOCK_NOSYSLOCK). The software cursor inside a locked rectangle is read and written back with it.
dxdiag's DirectDraw test (owner, bed, 15:23-15:25, `vm_5160_mcp/screenshots/Monitor_1_20261101-1523*` to
`-152532`): step 11 now passes - concentric alternating black and white rectangles, which is what dxdiag asks
about ("Did you see alternating black and white rectangles around the screen?"), cleared at the end. Very slow:
each frame is the HEL drawing through a full-surface Lock, i.e. the whole screen read back and written through
PIX_TRANS. Milestone 3 (HAL Blt: colour fill on the engine, clip list honoured) is the speed fix. Card not yet run.

Milestone 3 (`9d7896a`, `056ce69`), bed, owner's eye, 17:45-17:50: test 1's rectangles appear at once (fills
on the engine); test 2's square moves at a usable speed (system-memory source sent as host data, `DDCAPS_CANBLTSYSMEM`
+ `dwSVBCaps`). Test 3 asks for 640x480x16 and fails: the IMSG176 DAC has no 16-bit mode. The pointer is hidden
while dxdiag holds exclusive mode and returns after. M8DDTEST passes; M8BENCH pixels same in all 36.

Off-screen surfaces (`d279668`), bed, 11-01 evening. The shadow covers the card rows from the screen to
the end of the heap rows (`m8_dd_heap`: up to 256 rows above the string cache, 32 left for
SaveScreenBitmap; 96 at 1024x768) at the screen's pitch. M8DD32's CreateSurface places 8bpp off-screen
surfaces there itself (no DDRAW heap) and sets fpVidMem/lPitch; the first one sends escape 0x4D39 so
GDI's bitmap cache and menu save leave the rows (memory bitmaps written back first); the last
DestroySurface gives them back. M8DDTEST: surface caps `10004040` (video memory), at card row 800; Lock
round trip 0 of 2048 wrong; fill + card-to-card copy to the screen correct. M8BENCH pixels same in all 36.

Dead ends, one line each (bed, M8DDTEST "NO HAL" = DDRAW refused the HAL at creation):
- `DDSCAPS_OFFSCREENPLAIN` in `ddCaps.ddsCaps`: NO HAL, with or without a heap. Leaving it out, DDRAW
  still asks CreateSurface for `DDSCAPS_VIDEOMEMORY` surfaces.
- A rectangular or linear heap in RAM: NO HAL, but every such build also set the bit above, so a heap
  alone is untested.
- DDRAW keeps a refused HAL for the session: an INI switch read at DDCREATEDRIVEROBJECT needs a reboot
  per setting.
- GDI32 imported by M8DD32.DLL: fine (the DLL loads; an earlier guess blamed it).

## 16 bpp

The owner's choice (11-01): a real 640x480 16-bit mode, so 16-bit programs (dxdiag's test 3) run, shown
on the card's 256 colours. `src/fb16.c` (`66e7525`), bed only so far:

- GDI draws through the DIB Engine into a 5-6-5 frame buffer in RAM (600 KB). The screen device has no
  NOT_FRAMEBUFFER and is not `m8_hot.screen`, so every existing entry passes it to the DIB Engine.
- The access callbacks name what each call changes (right and bottom inclusive); when the outermost ends,
  those rectangles (up to four) go to the card, dithered (4x4 ordered) - a byte per pixel, as at 8 bpp.
  Output, BitBlt and Pixel narrow them: the DIB Engine names a line's clip rectangle and the box around
  a copy's source and destination.
- Palette (`b27c8b9`): a 5x9x5 cube (each channel has 0, 128, 192, 255, so the system colours are
  entries) plus a grey ramp about every 8 steps; index i and 255 - i are inverse. The DIB Engine stores a
  colour three ways (c * max / 255; c's top bits; for icons, 5-bit green doubled), and only those exact
  codes of a palette level are taken undithered. TEST24.BMP averages within 1-2.5 of the original per band.
- The pointer is screen.c's card pointer, as at 8 bpp; a send lifts it where they overlap. Drawing it in
  the frame buffer (the DIB Engine's) left ghosts on the card every way we tried to track it.
- `DibBlt` must get fPalettized 0 at 16 bpp, or icon and pointer masks break (`edbfc83`).
- FB16CHK (escape 0x4D3B) compares the card with what the frame buffer dithers to; ICONTEST, SCRGRAB
  and RAW565 read GDI's side.
- Chosen at boot: `bed/M8FB16.REG` (16) and `bed/M8FB8.REG` (8, 1024x768); ReEnable refuses a depth change.
- FB16TEST (ExtEscape 0x4D3A counters), pixels sent per call: PatBlt 64x32 2048, 400x300 120000, a
  190x140 line 28420, ten 148-pixel lines 1480 each, TextOut 1376, BitBlt 100x100 10000, SetPixel 2,
  GetPixel and copies off the screen 0.
- At 8 bpp the same build is unchanged: M8BENCH pixels same in all 36 (`bed_fb8d`).

Owner's eye, 11-01 late (bed, `M8FB16N`): desktop, menus and icons flat; gradients smooth; no pointer
ghosts. Slow only while the disk is read. Paint's title bar stayed half navy after starting Paint
from Run, until clicked: resolved 11-02, Windows' own (ATI's driver shows it too). Next: DirectDraw at 16 bpp (dd16.c refuses
it today; the primary is the frame buffer, Unlock sends the rectangle), then dxdiag test 3 with the
desktop already at 640x480x16. Not yet: a live depth change; pattern-register fills for solid colours
(the bus cost on the card); the card itself.

### 2026-11-02: DirectDraw at 16 bpp

- Primary = the fb16 frame buffer. Blt fills, copies and source-keyed blits run in RAM, then go to the
  card via `M8_ESC_FB16SEND`; screen copies moving by multiples of 4 use `M8_ESC_FB16COPY` (`a8ee0b2`).
- 8 bpp colour-keyed blits run on the engine (`7bca399`). The card needs `M8DD32.DLL` copied (via
  `M8DD32.NEW` and `WININIT.INI`) before DDRAW offers a HAL; without it, dd_sprite 0.30/s, with it 869/s.
- Card, 800x600x16 (`card16dd`, M8DD16): DirectDraw card check 0 in every test. With fingerprints
  (`7295b6e`, M8HASH) DirectDraw is 1.66x M8DD16: dd_scroll 8.3 -> 81/s, locks 1.7x.
- Next: Mode X on the VGA half (clear `DDHALINFO_MODEXILLEGAL` in `dd16.c`; M8DDB variant asks for
  320x200 with `DDSCL_ALLOWMODEX`; ddraw.dll has its own ModeX set, see
  `references/velocity9x/docs/issues/2026-08-15-doom95-low-resolution-modes.md`); card surfaces at 16 bpp.

## Open

- After dxdiag's run, M8BENCH's `bmp_*`, `draw_icon`, `scr_to_mem`, `drag_frame` ran 3-25x slower in the bed
  (`bed_svb1`, `bed_svb2`); a fresh boot (`bed_svb_fresh`) is back near `bed_after_dd`. Some state left behind
  by DDRAW (the failed mode set?) is not undone. Not yet looked at.
- 16-bit for DirectDraw: owner asks (11-01) for a pseudo-16-bit mode - see worklist item 9.

- Does DDRAW accept a primary whose memory the HAL supplies only through Lock? (Milestone 1-2 answer it.)
- DirectX 7.0a on the card (owner), after milestone 2 works in the bed.
- The 486 HAL vmdisp9x names (`vmhal486.dll`): no source found.
