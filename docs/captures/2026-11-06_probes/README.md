# Engine fill sources and the VGA half, card, 2026-11-06

DOS boot, `C:\M8P\GO.BAT`, run once. Sources `tools/m8seq/`, decoder `m8vga_decode.py`. Background: `docs/mach8_vga_half_findings_2026_11_06.md`.

| Probe | Result | Means |
|---|---|---|
| M8GRAD 6213h | Interior takes each source pixel's colour, on both rows; span x 10-49, as the 2213h control | **A polygon fill can take stored colours per pixel.** Gradients and textures from card memory |
| M8GRAD A213h | Pattern bytes 80h + n advance along the row, repeat at 32, restart each row | Horizontal ramp of up to 32 entries with no stored asset |
| M8GRAD E213h | Source byte indexes the pattern; both rows the same, as they differ only above the low bits | Recolour a stored texture by loading the pattern |
| M8GRAD 6271h | Only the plane 7 pixels drawn, rest left | Plane 7 as a mask without polygon mode |
| M8VFLIP | Owner: 1 box light, 2 boxes blue, 3 boxes alternating, 4 boxes even grey with a slight sense of change. 69.7 Hz, 0 retrace timeouts | **The VGA half can flip.** The recorded "cannot flip at all" was wrong |
| M8W128 | BDh 90h, flipped 94h: A000 and B000 mirror as found, hold different values with bit 2 set. Bank 1 at A000 read 00h, not B000's | **BDh bit 2 opens A000-BFFFF as two separate stretches.** The B000 stretch is not bank 1 as tested; which memory it is, is not shown |
| M8LATCH | 16000 pixels: plane fill 4.30 ms, latch copy 10.41 ms, CPU VGA to VGA 41.64 ms, CPU RAM to VGA 17.96 ms. Mismatches before 15937, after 0 | Latch copy 4.0x CPU VGA to VGA, 1.7x RAM to VGA. A VGA write ~1.07 us, an 8514 port word 7.64 us |
| M8DACX | Both directions read back as written | One palette table, both port sets |
| M8BNDRY | 42EE reads 0000 after writes; no effect on the engine or A000:0; controls good | Closed on the card: no memory split on the Mach8 |

**Limits of M8GRAD:** two rows, 64 pixels, no triangle yet. The outline plane leaves bit 7 set on the left edge pixel (8Ah, CAh), so colours are 7 bits and the plane must be masked out of the destination or the edge drawn over. A source pixel maps 1:1 to a destination pixel: a shift, no scale or rotation. Engine time per pixel not timed.
