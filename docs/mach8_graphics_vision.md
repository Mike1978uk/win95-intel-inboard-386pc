# The Mach8 as a graphics processor, not a frame buffer - ideas, 2026-10-05 (#49)

Owner's brief: the Mach8 is one of the first real graphics accelerators and is under-used here. Think
like the demoscene, with 2026 knowledge, inside the limits of this bus and CPU. Text first
(`docs/mach8_text_and_driver_plan.md`); this is the wider picture.

## The economics on this machine

| resource | measured | source |
|---|---|---|
| 8-bit ISA bus | ~3.8 us per 16-bit I/O write, ~1.9 us/byte | `iowidth` measurements, 2026-09-20 |
| the card's own engine | blits ~16 MB/s, fills ~31 MB/s | `docs/txtbench_2026_10_04.md` |
| CPU | 486BL3 at 83.5 MHz - tens of times an XT's 8088 | owner, CHKCPU |
| card memory | 1 MB; ~424 KB spare at 800x600x8, ~724 KB at 640x480x8 | bus plan |

The card works on its own memory about ten times faster than the bus can feed it, and the CPU has
cycles to spare. **The bus is the only scarce resource.** Every design choice below follows from one
rule: *send intent, not pixels* - a few register writes that make the card do the work, and spend CPU
time freely if it saves bus bytes.

## What the card can do on its own (8514/A plus ATI's extensions)

- **Blit** card memory to card memory, any rectangle, with a raster operation (copy, XOR, AND, OR...).
- **Fill** rectangles in a colour or an 8x8 pattern.
- **Draw lines** (Bresenham in hardware) and spans.
- **Colour-expand** 1-bit data to 8-bit pixels, from the CPU or (to be confirmed) from card memory: an
  8:1 decoder for anything with two colours - text, icons, masks.
- **Plane masks:** write and read masks per bit-plane, so 8 bit-planes can act as 8 independent 1-bit
  layers.
- **Palette (DAC):** changing a colour is about 4 bytes, however many pixels use it.
- **CRT start address:** where the visible screen begins. Changing it is one write: page flip or
  hardware scroll.
- **Scissor** (clip) rectangle in hardware.

## The card as a decompressor

The card cannot run LZ4, but several of its operations *are* decoders, and the CPU can choose the best
one per region before anything crosses the bus:

| encoding the CPU picks | the card's decoder | wins when |
|---|---|---|
| 1-bit mask + two colours | colour-expand | 2-colour content: 8 pixels per byte sent |
| run of one colour | rectangle fill (~5 writes) | runs longer than ~10 pixels |
| "copy this block from over there" | blit from card memory | anything already on the card: last frame, a sprite/tile/glyph dictionary, scrolled content |
| change of colour, not of pixels | palette write | fades, colour cycling, flashing, water and fire effects |
| vectors | line and fill commands | flat-shaded shapes, UI chrome, wireframe |
| raw pixels | pixel transfer (`rep outsw`) | photographic data - the fallback |

The big one is the third row: **LZ77 where the decoder is the blitter.** Keep a dictionary in spare card
memory (glyphs, icons, sprites, tiles, the previous frame) and describe each new picture as copies
from it plus whatever residual pixels are new. Moving objects and scrolling become motion-compensated
copies done entirely on the card.

## Techniques to build on

- **Off-screen caches:** glyphs (#49 first step), then icons, brushes, cursors, and "save under" for
  menus and dialogs, so repainting what a menu covered is a blit, not a resend.
- **Dirty rectangles:** send only what changed; the driver already gets this from GDI, games must do it.
- **Page flipping:** at 640x480x8 two full pages fit in 1 MB. Draw the next frame off-screen with
  card-side operations, then flip the CRT start address: no tearing, one write.
- **Hardware scrolling:** move the CRT start address and repaint only the new edge.
- **Bit-plane layers:** e.g. background in planes 0-5, a sprite layer in plane 6, a highlight in plane
  7, combined by the palette - moving a layer touches one plane, and overlap needs no redraw.
- **Palette effects:** colour cycling (Mark Ferrari's famous landscapes), fades and glows at a few bytes
  per frame.
- **CPU-side encoding:** the 486 can afford to analyse a bitmap per scanline and pick the cheapest
  encoding above - the Windows driver's bitmap path (`SetDIBitsToDevice`) is the place for it.

## Where it can land

1. **The Windows driver** (#49): glyph cache, then the icon/brush/save-under caches, then an encoding
   bitmap path. Benefits every program, and measurable with `TXTBENCH`.
2. **A small library for DOS and Windows programs** that use the card directly: sprite and tile
   dictionaries, page flip, palette effects, run and vector drawing. A demo built on it is the
   showcase - and the "bragging rights".
3. **Later, possibly:** DCI or a DirectDraw HAL in our own driver (off-screen surfaces and blits), so
   Windows games could use the card. Large; only after 1 and 2 prove the ideas.

## 3D, built around the card (owner, 2026-10-07)

Not Direct3D: perspective-correct texturing and a Z-buffer are per-pixel work, and per-pixel means the bus.
A 3D engine designed around what the card does cheaply is a different question, and the answer may be yes.
The principle is the one this file opens with: send commands, not pixels.

- **Flat-shaded polygons:** the 8514/A polygon fill (outline, then fill; types A and B, modelled in the bed
  for TS1) draws a triangle in a handful of writes whatever its size. At 15-20 writes, the bus allows on the
  order of 10,000 triangles a second.
- **Lighting by palette:** colour ramps, so shading is an index; fades are DAC writes. Patterns (`A211`)
  for dithered tones.
- **Page flipping:** two 640x480 pages at pitch 1024 fit in 1 MB; more at lower resolutions.
- **The 486 does the maths:** transforms, clipping, painter's-algorithm sort.
- **Textured walls, raycaster style:** pre-scale each texture's columns once into spare card memory; each
  screen column is then one engine blit (~320 blits, ~10 ms of bus per frame).
- **Planes and write masks** for layers (HUD, sprites) composited by the palette.
- **Two cards in one:** VGA side (512 KB) and 8514 side (1 MB) have separate memory and the engine cannot
  read the VGA side, but the CPU can fill one while the engine draws on the other.

Speculative until measured on the 5160: the cost of a polygon-fill triangle, the engine's fill rate, and
whether pre-scaled column blits hold up. Comes after the desktop driver; probes can be written any time.

## Measure first, always

Extend `TXTBENCH` before each step: a bitmap test (`SetDIBitsToDevice` of photo-like and flat-colour
images), a long-run fill test, a palette-write test. Count port writes per operation as well as time:
writes are bus occupancy, the thing we are saving. Timing on the 5160 (the bed does not model bus cost); behaviour in the bed, which runs ATI's accelerated
Win95 driver since 2026-10-07 with the EEPROM in `roms/video/mach8/`.

## Unknowns to settle before designing

- Colour-expand from card memory (mono source = video memory, read mask selecting a plane): supported on
  the Mach8 or only from the CPU? Check the Mach32 register guide below against the real card.
- Which spare card memory `ATIM8.DRV` already uses.
- A 16-bit linker for building or patching a Windows 3.x/95 display driver.

## Sources

- Daniel_K (Daniel Kawakami) - modded Creative Sound Blaster driver packs, <https://danielkawakami.blogspot.com/>
  and <https://forums.guru3d.com/threads/all-in-one-daniel_k-modded-soundcard-driver-packs-disks.327083/>.
  His work is audio, not display; what carries over is the method - take a vendor driver apart, patch
  and recombine it, and unlock what the hardware could already do. No display-driver work by him found.
- Angel Trinidad - Omega Drivers, tweaked ATI and NVIDIA graphics drivers built on the vendors' releases:
  registry tweaks, extra resolutions, overclocking, internal optimisations, an alternative installer.
  <https://en.wikipedia.org/wiki/Omega_Drivers>. The closest model for #49: improve the vendor driver
  rather than rewrite it, and look for modes and settings the stock setup hides (e.g. a 640x480 mode
  with room for two pages, for page flipping).
- Robert McClelland - PAX drivers, modified Creative sound card drivers (audio):
  <https://www.overclock.net/threads/new-pax-creative-drivers.1216641/>.
- Asder00 is a different person: an account reporting AMD/ATI and Intel driver leaks and releases
  (<http://asder00.blogspot.com/>), not a driver modder; nothing linked the two names.
- **Shared lesson of all three modders:** start from the vendor's driver, unlock what the hardware
  already does, retune internals, measure, and package it cleanly. Supports patching `ATIM8.DRV` first.
- ATI, *Programmer's Guide to the mach32 Registers* (a superset of the Mach8's 8514/A and ATI-native
  registers): <https://fenarinarsa.com/misc/atari-forum/reg-688000-15_programmers_guide_to_the_mach32_registers.pdf>
- OS/2 Museum, ATI mach8/mach32 documentation: <http://www.os2museum.com/wp/ati-mach8mach32early-mach64-documentation/>
  and Michal Necasek's 8514/A article (already used for #8).
- vgadoc ATI notes: <https://pdos.csail.mit.edu/6.828/2018/readings/hardware/vgadoc/ATI.TXT>
- Our own: `docs/mach8_real_hardware_registers_2026_09_08.md`, `docs/bus_optimisation_plan.md` (Mach8 section).
