# Mach8 / 28800 probes on the 5160, 2026-10-11

Sources in `tools/m8seq/`. Run from a DOS boot on the Graphics Ultra.

- **M8XMEM**: neither half reaches the other's memory. VGA A000:0-15 unchanged after an engine fill
  (40-4F); the engine reads its row back (A5, control); a CPU write to A000 does not reach the engine
  (still A5). Moving data between the halves is a CPU copy over the bus.
- **M8LIN**: B6h bit 2 enables banking in 256-colour modes (bank 1 kept separate only with it set).
  With GR06 map 00, B000 mirrors A000 in every case - no 128 KB linear window. ACh bit 0 (saved 03h)
  changes nothing visible on this bus. A 320x200x8 screen fits one 64 KB bank.
- **M8FLIP**: the owner saw black throughout - no switch to the 8514 side reached the monitor. Likely
  cause: the probe wrote CLOCK_SEL 0, which XFree86 uses to reset pass-through. The 8514 vsync ran
  (frame 6441/6400 polls of 02E8 against 5031/5032 of 3DAh - port costs differ, so not a rate
  comparison); switch-to-vsync delays spread 82-6748 polls (not phase locked); 120 fast switches ran.
  v2 needs CLOCK_SEL bit 0 set and the 25.175 MHz clock index from the EEPROM's mode table (A4).
- **M8A2**: 4,618 bytes, raw; compare against a bed run.
- BEh bit 2 is "internal EGA DIP switches" (ATI.TXT), not a 128 KB window; the 128 KB map is VGA GR06.

## Bed comparison, 2026-10-11 (`vm_xtide_mpd`, build_log at `5897f6a48`, DOS boot)

- M8STAT: LINE_SYNC now toggles in the model (4086 of 8192 reads); the card's rate is not yet matched.
- M8XMEM: bed = card.
- M8A2: lines with ERR_TERM -1 and the compare under WRT_MASK 0Fh match. Hit test: same INSIDE_SCISSOR
  behaviour; SUBSYS_STAT bits 6 (monitor ID) and 0 (vblank) differ (card A0/AA/A8, bed E1/EB/E9).
  **Model gaps:** a fixed-pattern fill of 4 rows draws only row 0; reads outside the scissors return the
  pixels instead of FFh; a blit source past x=1023 reads zeros where the card continues into the next row.
- M8LIN: the model opens a real 128 KB window (B000 = bank 1); the card mirrors A000. 28800 half, not PR.
- M8BLEND, M8FLIP2: bed timings say nothing about the card (the model's engine finishes at once).

## M8BLEND on the 5160, 2026-10-11 (`M8BLEND_5160.BIN`)

64x64 at 8 bpp, BIOS-tick timing: engine copy (mix 07h) 0.23 ms, engine average (17h) 0.30 ms, engine
saturating add (1Bh) 0.30 ms; the CPU doing the average (read back through PIX_TRANS, blend, write) 35.7 ms.
The engine blends about 120 times faster than the CPU path, for 30% over a plain copy. No ATI driver uses
these mixes (Windows GDI has only the Boolean ones).

## M8SHOW, run on the 5160 around 2026-10-12, recorded 2026-10-16 from the owner's recollection

The owner saw both sets of coloured stripes ("the one with the two sets of coloured stripes"): **the 8514 side reaches the
monitor through ADVFUNC_CNTL alone** (shadow sets 2 then 1, no CLOCK_SEL or CRTC write). M8FLIP v1's black screen was
its own CLOCK_SEL 0 write. No file (watch-only probe). Next: M8FLIP3 (ADVFUNC-only switching, `tools/m8seq/M8FLIP3.ASM`).
