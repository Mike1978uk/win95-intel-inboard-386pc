# ATI Graphics Ultra (Mach8): real card against the 86Box model

Measured behaviour of a real ATI Graphics Ultra and where 86Box's model differs. Written for
emulator authors and anyone documenting the card (Michal Necasek's 8514/A article, OS/2 Museum,
is the obvious reader). Every row says how it was established.

**The card:** BIOS 113-11504-002 (1992/4/8), 38800-1 Mach8 + 28800-6 VGA, 1 MB VRAM, in an
8-bit slot of an IBM 5160 with an Intel Inboard 386/PC. Read over COMrade from DOS.

## The EEPROM

- **One 64-word serial EEPROM, reached only through the VGA chip.** The ROM's read routine
  (3A4Eh) bit-bangs it through ATI extended register B3h at 1CEh/1CFh: bit 0 DI, bit 1 SK,
  bit 2 CS, bit 3 enable; DO is B7h bit 3; A6h bit 2 is held clear while it runs. It sends
  6 address bits, so 64 words. The Mach8 has no EEPROM path of its own; the ROM copies values
  into it. Dumped with `tools/m8eedump/` (read opcode only):
  `roms/video/mach8/eeprom_card_5160_2026_10_07.nvr`.
- **ScratchPad1 (56EEh) = EEPROM words 8 and 9, low byte of each** (ROM 0783h). The card reads
  0820h; it was the check that the dump is right.
- **Words 31h-37h are a CRT parameter set** in ATI `MONITOR.INF` order: H_TOTAL C7h,
  H_DISP 9Fh, CRT_PITCH A0h, H_SYNC_WID 0Ah, V_TOTAL 8F8h, V_DISP 7FFh, V_SYNC_STRT 861h,
  V_SYNC_WID 0Ah, DISP_CNTL 33h, CLOCK_SEL 2Dh, FIFO_DEPTH 16h - `MONITOR.INF`'s
  1280x1024 87 Hz interlaced entry. Words 10h-15h and 23h-2Fh are not yet decoded.
- **Word 02h bit 0 switches on a ROM routine (04C0h) that runs from video memory.** It copies
  12Bh bytes of ROM code to colour text memory (B800:1000, or B000 on mono), far-calls it, and
  from there copies 8 KB of ROM alongside and reads B7h bit 0; read from the code, not traced:
  with a 16-bit slot it goes on to compare ROM reads under settings in B9h, A3h and A0h bit 4.
  Set on this card (0919h).
- **ATI INSTALL does not write the whole EEPROM.** A Set Power Up Configuration run made a file
  that differs from the card in 32 of 64 words: the CRT tables are blank and word 02h bit 0 is
  clear. A bed built from INSTALL never runs the routine above.

## Where 86Box differed, and the state of each

| # | Behaviour | Real card | 86Box | State |
|---|---|---|---|---|
| 1 | Executing code from VGA memory | runs | interpreter fetched from a 4-byte dummy array indexed as a page (`getpccache()`); POST hangs with the card's EEPROM | fixed in the fork, `5fce405ab` (CPU core, not the Mach8) |
| 2 | CONFIG_STATUS_1, 12EEh | `xx21`: CLK_MODE, 1 MB; EEPROM_ENA and ROM_ENA clear | `00A9`: EEPROM_ENA and ROM_ENA set | corrected for the Graphics Ultra; high byte read FEh on the card, not modelled |
| 3 | CONFIG_STATUS_2, 16EEh | `0046`: HIRES_BOOT, EPROM_16_ENA, reserved bit 6 | `001A`: WRITE_PER_BIT and FLASH_ENA set | corrected |
| 4 | Extended register BDh bit 4 in an 8-bit slot | set (`90h`) | forced clear for 8-bit | open: the source comment misread 90h as bit 4 clear; meaning of the bit unknown |
| 5 | 16-bit I/O on an 8-bit slot | two byte cycles | one word access; `bus_width` only changes status bits | open: splitting into bytes made results worse, so the model's byte paths for 16-bit registers are not faithful either (V_DISP read back 96) |
| 6 | Extended A1h, A4h, A5h, ABh, ACh, ADh after POST | 1Fh, 54h, 76h, 80h, 03h, 10h | 00h | open |
| 7 | 7AEEh / 7EEEh read back after POST | 0400h / 03FFh | 0000h / 0000h | open |
| 8 | CRT shadow sets | three register sets: primary, shadow 1 (640x480), shadow 2 (1024x768); SHADOW_SET (5AEEh) picks which set the CRT writes load, SHADOW_CTL (46EEh) locks groups of them | one set; SHADOW_CTL lock bits only block writes, SHADOW_SET is logged and otherwise ignored, so the last set written wins | open: after POST the card reads B2EEh 694Fh (640 set), the bed A17Fh (the 1024 set the ROM loads last) |
| 9 | Display after M8MONO (mono host data, then 4AE8h = 2) | VGA picture returns | VGA output stays blank until a mode set (TEST.COM) | open: same ports as earlier probes that recover; next step logs the renderer chosen when 4AE8h returns to the VGA |

Decoding sources: ATI's register reference (Mach8 pages of the Mach32 programmer's guide,
`references/ati_mach32/guide.txt`, CONFIG_STATUS_1 at 9-64, CONFIG_STATUS_2 at 9-66). Neither the
ROM nor TEST.COM tests the bits corrected in rows 2-3 (they test bits 1, 2, 4 and 5-6 of 12EEh,
which already matched), so rows 2-3 are accuracy fixes, not the TEST.COM TS1/TS2 cause.

## How the ROM loads the shadow sets at POST

From an 86Box write log of the ROM (the order is the ROM's; the values came from the bed's EEPROM):

    5AEE=0001 46EE=0000  02E8=63 06E8=4F 0AE8=52 0EE8=2C 12E8=418 16E8=3BF 1AE8=3D6 1EE8=22 22E8=23
                         4AEE=1850 46EE=003F 5AEE=0000 46EE=0000
    5AEE=0002 46EE=0000  02E8=9D 06E8=7F 0AE8=81 0EE8=16 12E8=662 16E8=5FF 1AE8=600 1EE8=09 22E8=33
                         4AEE=181C 46EE=003F 5AEE=0000 46EE=0000

Set 1 gets 640x480 timings, set 2 1024x768, each followed by SHADOW_CTL = 3Fh, then the pointer
goes back to the primary set and the locks are released. Which set B2EEh (R_H_TOTAL&DISP) reads,
and which set drives the display in each 4AE8h mode, is still to be measured on the card.

Measured on the card (`tools/m8seq/M8SHAD.COM`, `M8SHAD_5160.BIN`): reading B2EEh/B6EEh/BAEEh/C2EEh/
C6EEh/CAEEh after setting 4AE8h (2 or 6, display on the VGA), SHADOW_CTL and SHADOW_SET:

| SHADOW_CTL | SET 0 | SET 1 | SET 2 |
|---|---|---|---|
| 00h | A | A | B |
| 3Fh | B | B | A |

A = 694Fh 53h 05h, V 414h/3BFh/3D0h (the card's own 640x480 timing); B = 634Fh 52h 2Ch,
V 418h/3BFh/3D4h (`MONITOR.INF` 640x480 60 Hz). 4AE8h bit 2 changes nothing while the VGA drives
the display. No 1024 values appear: with an 800x600 monitor in its EEPROM the card holds two 640
sets. The rule (the lock appears to invert the selection) needs one more probe that writes a marker
value into each set before the model can copy it. 86Box would read the same values in every cell.

## Register dumps

`tools/m8seq/M8REGS.COM` reads ATI extended registers A0h-BFh and the Mach8 status and
read-back ports (no data ports). Captures in `docs/captures/2026-10-07_m8txt/`: `M8REGS_5160.BIN`
(card, cold boot to DOS) and `M8REGS_bed*.BIN`. Mode-dependent registers (B2EEh, the CRT
read-backs) differ with whatever mode was last set, so compare like with like.
