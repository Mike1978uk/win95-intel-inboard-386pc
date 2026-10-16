# Graphics Ultra levers - what ATI built, what its drivers used (2026-10-16)

Owner's question: find every lever and switch on the card, both chips, including silicon ATI's drivers never used.
Register detail and measurement status live in `docs/mach8_card_coverage.md`; this file is the inventory and the
plan. Owner's reading, which the census supports: ATI shipped features in silicon and in the factory test
(TEST.COM) and moved to the Mach32 before the drivers caught up.

## Method: a port census across ATI's own software

Every ATI binary on disk (84 unique files after de-duplication: TEST.COM, the card ROM, VVESA, ~40 DOS tools,
the Windows 3.x drivers MACH*/ULTRA*/MACHW3, Microsoft's Win95 ATIM8/ATI.VXD and ATIM32, ATIDESK) scanned for
Mach8 port immediates (`xxE8`/`xxEE` after `mov dx`/`push`/`mov r16`/`cmp`) and 28800 extended indices (`A0-BF`
next to `mov dx,1CEh`). A byte-pattern scan: single hits can be data, the pattern across groups is the signal.
Script: `tools/ati_port_census.py`; full output `docs/captures/2026-10-16_ati_port_census.tsv`.

## Mach8: used by the factory test, never by a driver

| Port | Register | Used by | Value to us |
|---|---|---|---|
| 92EE / EAEE | source colour compare function / colour (Mach8 only, not in the Mach32 guide) | TEST.COM only | **transparent blits**: sprites, DirectDraw colour keys, icons without masks. Measured and modelled (`8283ed2bc`) |
| EEEE | DEST_CMP_FN | TEST.COM, Win95 ATIM8 only | destination colour key, nibble tests 40h-78h (measured 10-10) |
| 9EE8 | SHORT_STROKE | TEST.COM, tools | two vectors per write: line art, glyph strokes. 8514/A form measured (M8ROW3) |
| C6EE | EXT_SHORT_STROKE | TEST.COM, MACH (W31), ATIM32 - not ULTRA, not MACHW3 | packed mono short strokes, colour patterns; implemented, **not card-measured** |
| 22EE, 46E8, 5AEE, 7AEE, F6EE | setup / EXT_GE_CONFIG / alias of CUR_Y | TEST.COM, ROM | configuration; 7AEE is EXT_GE_CONFIG (pitch, bpp, monitor alias) |
| 7EEE, 66EE, 86EE, 92EE (read) | MISC_CNTL / VERT_OVERSCAN / - | tools only | to identify |

Already found by measurement, used by no ATI driver: PATT_DATA colour patterns beyond 8x1, the command 4 nibble fill
(only 8514.DRV/ULTRA.DRV's text background), polygon boundary + fill (command 5), DP_CONFIG mono expansion from
display memory (the XFree86 glyph cache path).

## 28800: extended registers (index at 1CEh)

| Index | Used by | Note |
|---|---|---|
| B0, B2, B3, B6, B8 | Windows drivers (MACH, MACHW3, ATIM8) | banking (B2), 256-colour mode (B6), the drivers' set |
| B2 | VVESA | VESA banking |
| A0, A6, B7, B9 | card ROM | mode set / clocks |
| A6, BB | TEST.COM | |
| AE, BE | Win95 ATI.VXD | AE, BE: save/restore around DOS boxes |
| **A1-A5, A7-AD, AF, B1, B4, B5, BA, BC, BD, BF** | **nothing found** | **unknown or unused**. AB bits 0/5 (zero-wait writes) are off on our card (B2) |

## Open, and how each gets answered

| Question | Static source first | Then on the card |
|---|---|---|
| 28800 indices nothing uses | VVESA.COM disassembly (ATI's 28800 banking/mode/clock code); ROM's mode tables (A0/A6/B7/B9); Sutty and Blair (18800), Ferraro ch. 15-21 | B2: one probe per field |
| DP_CONFIG mono expansion fidelity (text path C) | ATIM8 / MACHW3 call sequences | one probe, compared with the bed |
| EXT_SHORT_STROKE (C6EE) | MACH (W31) and ATIM32 usage | one probe |
| 7EEE / 66EE / 86EE meaning | guide, tools' disassembly | read-back probe |
| Mach8 bus knobs (FIFO_OPT, MAX_WAITSTATES) | guide 9-x | B3 under load |
| EEPROM: all 128 bytes accounted for? | `mach8_card_coverage.md` EEPROM map, INSTALL.EXE | - |

## Mach32 drivers as a source

The Mach32 keeps the Mach8's engine and adds to it; the guide marks each register 8514/A | Mach8 | Mach32.
Microsoft's Win95 `ATIM32.DRV/.DLL` (golden install) uses, beyond the Mach8 set, CHIP_ID (FAEE), FEE8 and C6EE. Reading
how a later ATI-tuned driver drives the shared registers (C6EE, DP_CONFIG, PATT_DATA, FIFO pacing) is a cheap source of
technique for ours; Mach32-only registers do not apply.
