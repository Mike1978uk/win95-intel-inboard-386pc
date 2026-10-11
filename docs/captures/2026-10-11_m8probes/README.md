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
