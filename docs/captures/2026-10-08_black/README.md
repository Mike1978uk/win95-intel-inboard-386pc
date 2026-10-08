# Black screen after M8TSX - 6AEEh bit 10 (5160, 2026-10-08)

Card: Graphics Ultra 113-11504-002, 1 MB, 8-bit slot, in the 5160. Bed (86Box) stayed visible.

## Symptom

After REGR, or M8TSX alone, the 5160's monitor showed a black picture with a valid VGA signal. TEST.COM
still passed afterwards and Esc out of it gave a picture back.

## How it was found

| step | result |
|---|---|
| DAC masks 3C6h/2EAh, palette 7, sequencer 1 | FFh, FFh, 2A/2A/2A, 00h - normal |
| TEST.COM's return-to-VGA routine (06BBh) replayed | still black |
| CLOCK_SEL 1E38h / 1631h | black / "input not supported" - bit 0 does switch the output to the Mach8 |
| `MODE CO80` (VGA BIOS mode set) | still black |
| `VGASNAP` cold vs after M8TSX | byte-identical - the VGA half is untouched |
| `M8REGS` cold vs after M8TSX | 17 differences, incl. 56EEh 0820h -> 0004h, 6AEEh 00EAh -> 009Ah |
| ScratchPad1 restored | still black |
| `M8RPL n S`, cold boot each: 999 / 59 / 29 / 13 / 0 | black / black / black / black / visible |
| 6AEEh = 00EAh, the cold value, written by hand | **picture back** |
| fixed M8TSX (saves and restores 6AEEh, 52EEh, 56EEh) | visible; its 32 result words per pass identical to the 10-07 card run |

## Cause

ATI's programmer's guide (`references/ati_mach32/guide.txt`, page 9-9), Mach8 MAX_WAITSTATES, 6AEEh:

| bits | name | |
|---|---|---|
| 3:0 | Q_WSTATES | write wait states |
| 7:4 | ROM_SPEED | POST ROM read wait states |
| 8 | LINE_OPT_ENA | 1 = horizontal line-draw optimisation off |
| 9 | IOR16_ENA | 1 = PIX_TRANS reads as two 8-bit cycles |
| 10 | PASSTHROUGH_OVERRIDE | 1 = "passthrough connection not made, VGA syncs not detected" |

"Initialized by the POST ROM. It should not be modified by normal program applications." TEST.COM's early
stages write 049Ah (bit 10 set). M8PRE.DAT carries that write; M8TSX replayed it and never undid it. The card's
ROM leaves 00EAh. With bit 10 set the DAC does not take the VGA pixels: VGA sync, black picture.

Read-back on the card is the low byte only: 049Ah written reads 009Ah.

M8PRE.DAT also carries TEST.COM's scratch-pad tests, which left the bed's ScratchPad1 (0004h) on the card.

## Changes

- `tools/m8seq/M8TSX`: reads 6AEEh, 52EEh and 56EEh at start and writes them back at exit.
- 86Box fork: a Graphics Ultra blanks the VGA picture while 6AEEh bit 10 is set; 6AEEh reads back its low byte.
- Tools: `M8RPL` (replay the first N writes of M8PRE.DAT, optionally setup ports only), `VGASNAP` (VGA registers).

## Open

- What ScratchPad1 / 4AEEh the card's ROM leaves at boot: M8TSX still writes CLOCK_SEL = 1850h at exit (the ROM's
  first write in the bed; visible on the card with `M8RPL 0`).
- Bed check of the model change: old M8TSX should now blank the bed as it blanks the card.
