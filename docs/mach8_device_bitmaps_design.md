# Device bitmaps in video memory (worklist D1 item 1)

Goal: a bitmap drawn to the screen more than once crosses the bus once. After that each draw is an
engine copy inside video memory: ~50 us of set-up writes, no pixel bytes on the bus, no L1 refill
for the pixels (`docs/captures/2026-11-12_flushprobe/README.md`).

## What ATI did (Windows 3.x MACH v2.3, `references/mach_w23_disasm/`)

- `REALIZEOBJECT` `seg01:55c4-5758`, bitmap create: always a host copy (GlobalAlloc 2002h, handle at
  PBITMAP `+24h`), plus a video-memory slot (`call a20e`); row padded to 8 px (`+2Ah`), linear
  video address at `+20h`, size `+2Eh`. Type word 4000h marks ATI's bitmap.
- `SELECTBITMAP` `seg01:b52c`: on select, the two copies are synchronised. Upload `b623-b781`: wait
  idle, GE_OFFSET (6EEE/72EE) = slot address >> shift, GE_PITCH (76EE) = row / 8, host-to-screen
  blit at (0,0) of the bitmap's size, one `rep outsw`, then GE_OFFSET/GE_PITCH back to the screen's.
- `DEVICEBITMAP` itself is a stub (`seg01:6574`, returns 0).
- Switches (ATI README lines 291-301): `DeviceBitmap` (all of it), `DeviceBitmapDraw` (engine draws
  into realised bitmaps). Known conflicts: programs that write a bitmap's memory directly.
- ATI's Win95 driver (ATIM8) dropped this: bitmaps are DIBENG objects in host RAM
  (`docs/atim8_disassembly.md:104`).

## Unused silicon this uses

GE_OFFSET / GE_PITCH. Our driver writes 0 / 128 once (`m8hw.c:77-79`) and never again. A moved
base lets each cached object sit packed at its own pitch in the ~424 KB off-screen, instead of in
screen-shaped rectangles. Same register pair serves glyphs (D2) and textures (D5).

## Where this ended (11-12)

Most of it is already in the driver: the content-keyed bitmap cache (`screen.c:1413-1600`) stores
on the second sight, draws by engine copy, and is told of every write (`m8_bc_forget`). At
800x600x8 the bitmap tests already run 2.5-33x ATI (`card_gx82`). A 64-row lookup memo in place of
the single row averaged 0.999 over two card runs (drag_frame -7 %): not kept. GE_OFFSET/GE_PITCH
packing is kept for the glyph cache and textures.

## Our design (Windows 95, DIBENG underneath), as first written

1. `RC_DEVBITS`: GDI realises bitmaps through us. The bitmap stays a DIBENG object in host RAM, so
   DIBENG keeps drawing into it, and every program that reads the bits sees them. RAM is the master.
2. Every drawing entry point whose destination is one of our bitmaps marks it dirty, then forwards to
   DIBENG. No hashing: GDI routes all writes to a device bitmap through the driver.
3. Bitmap to screen (BitBlt, StretchBlt whole factors, icons): clean video copy -> engine copy
   (screen-to-screen across GE_OFFSET is not needed: the copy uses the screen base, source at its
   slot's x/y); dirty or none -> upload once in one burst, mark clean, then the engine copy.
4. Allocator: one off-screen heap shared with the string cache, mono cache, DirectDraw surfaces and
   later glyphs; evict least recently drawn. Rows below the visible screen at 800x600x8.
5. Screen to bitmap: read back into RAM (as now); the video copy becomes clean only if the whole
   bitmap was read.
6. `[2d] devbits=0` in `C:\M8D3D.INI` (or SYSTEM.INI `[mach8]`) turns it all off: GDI's bitmaps go
   back to DIBENG unchanged.

## Questions to settle before code

- ~~DDK contract~~ settled (`references/dxddk/ddk9x/win95ddk/DOCS/DISPLAY.DOC`, "Device Bitmaps"):
  DIBENG already sets `RC_DEVBITS` and allocates every bitmap in system memory; GDI calls the
  driver's RealizeObject (OBJ_PBITMAP), SelectBitmap and BitmapBits, which we forward. These must
  never fail (no video memory -> system memory). Mono bitmaps are always created by GDI in RAM.
  So: forward to DIBENG as now, and keep our video copy beside it, keyed by the DIBENGINE header.
- 16 bpp desktop: the card is 8 bpp; device bitmaps apply to the 8 bpp desktop first.
- Whether `+32h` in ATI's PBITMAP is a use count that gates upload (SELECTBITMAP `b57b`: <= 1Eh).

## Measure

M8BENCH at 800x600x8 against `card_ati1,card_ati2`: bmp_icon, bmp_card, bmp_changing, bmp_large,
mem_to_mem, draw_icon, scroll. Pixels must stay equal in all 35 tests.
