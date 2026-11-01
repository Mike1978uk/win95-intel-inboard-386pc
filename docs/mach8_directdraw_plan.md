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

## Open

- Does DDRAW accept a primary whose memory the HAL supplies only through Lock? (Milestone 1-2 answer it.)
- DirectX 7.0a on the card (owner), after milestone 2 works in the bed.
- The 486 HAL vmdisp9x names (`vmhal486.dll`): no source found.
