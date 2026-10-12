# M8VGA and ZWS2, 2026-10-12

`M8VGA_5160.BIN` / `ZWS2_5160.OUT`: the 5160, DOS (F8 command prompt), card EEPROM.
`M8VGA_bed_60hz.BIN`: `vm_5160_now`, fork Qt build `4a79b0b4f`, 60 Hz candidate EEPROM.

- ZWS2: AB bits 0/1/5 change nothing for VGA memory reads either (the two AB=0 arms span the others).
  Reads cost 3.87 / 3.40 / 3.0-3.2 us per byte (byte/word/dword) against writes 2.18 / 1.96 / 1.86 and
  ROM reads 2.70: never read-modify-write VGA memory; keep a shadow in RAM.
- Modes 03h and 13h: every standard VGA register = card. ATI indexes 80h-9Fh and C0h-FFh read FFh on the
  card (not implemented; the model returns 00). Differ: A0 A1 A4 A5 AB AC AD B7 (known), BA 01/00 (new).
- Mode 63h: 164 differences, GC reading all implemented bits set and many CRTC FFh on the card; A3 B1 B8
  (C2) B9 BE differ. Open: rerun the bed on the card's EEPROM before reading anything into it.
