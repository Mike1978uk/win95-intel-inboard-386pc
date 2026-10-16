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
| EEEE | DEST_CMP_FN | Win95 ATIM8, tools, ATIDESK - no Windows 3.x driver | destination colour key, nibble tests 40h-78h (measured 10-10) |
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

## VVESA.COM, disassembled 2026-10-16 (static reading - each line is what the code implies, not yet measured)

ATI's VESA TSR (13,548 bytes; **the copy on the CF, `D:\utils\ATIVGA`, differs from `ATIMACH8/V3`** - two builds,
not yet compared). Extended port taken from a variable (`[150h] = 1CEh`), plus helpers at 1016h (OR), 102Bh
(write), 1032h (read), 103Dh (AND). Indices it touches: **A3, A7, AE, B0, B2, B6, B8, BB, BE**.

| Index | What VVESA does | Reading |
|---|---|---|
| B2 | +2 / -2 for the write bank; ror, +10h, rol for the read bank | **separate read and write banks**: write bank bits 1-4, read bank bits 5-7 + 0. A VGA-to-VGA copy across banks with no reprogramming |
| BE | bit 3 set / cleared around banking | enables the split read/write banking above |
| AE | bits 0-1 = bits 4-5 of the bank number | bank high bits: reaches all 512 KB (and beyond, on larger boards) |
| A7 | bit 1 tested on chip revision '5', then five reads of 3C6h | the hidden DAC command register probe: detects a HiColor DAC. Ours is an INMOS IMSG176, 6-bit, none |
| BB | bit 4 merged into a clock-select value | a clock-select bit |
| A3 | bit 4 set from a VVESA flag | unknown |
| B0 | bit 6 set from a VVESA flag | unknown (B0 also used by the drivers) |
| B6, B8 | mode setting | 256-colour mode (B6), as the drivers |

(The untouched list that stood here was superseded by the ROM reading below.)

## L3 (desk), 2026-10-16

### The two VVESA builds

CF (`D:\utils\ATIVGA`) = "VESA implementation **2.21**"; `ATIMACH8/V3` = **2.22**. Same size, 13 differing runs, two
changes, both fixes in 2.22:
- Setting a bank, 2.21 keeps B2h bits 0-4 (`and 1Fh`); 2.22 keeps bits 1-4 (`and 1Eh`). **B2h bit 0 is the read bank's
  top bit** (read bank = bits 5-7 + bit 0), so 2.21 could leave a stale high read-bank bit - only banks 8-15 (above
  512 KB), not reachable on our card.
- Mode set: after clearing 42EEh, 2.22 calls ROM entry 68h (al=2) **before** clearing video memory bank by bank; 2.21
  cleared first. The ROM call changes what the banks map, so the order matters.

### The card ROM has an API: four far-call entry points

ROM (`roms/video/mach8/BIOS.BIN`, dated "1992/4/8 12:16" at 50h) has a jump table at C000:**0064h, 0068h, 006Ch, 0070h**
(-> 7022h, 7026h, 702Ah, 702Ch). Not in the Mach32 guide, XFree86 or our notes.
- **64h**: dispatches on AL into an internal service (72DDh) with AX = 1201h/5502h (AL 0), 6Axxh (AL 1-2), 6801h (AL 11h),
  6802h (AL 12h); other AL -> carry set. The service writes 5AEEh and 46EEh (the shadow-set registers) - accelerator mode
  loading.
- **68h**: AL 0 = ATI's engine bring-up: DISP_CNTL (22E8h) = 60h, ADVFUNC_CNTL (4AE8h), EXT_GE_CONFIG (7AEEh) built from
  42E8h bits 4-6, and 26EEh/76EEh = A0h when B2EEh reads 9Fh. VVESA calls it with AL 2.
- **6Ch, 70h**: `stc; retf` - not implemented in this ROM.
- **Finding the ROM:** TEST.COM takes the segment from SCRATCH_PAD_0 (52EEh): C000h + (bits 0-6) x 80h, then calls 64h with
  AL 0 and its parameter in BX (services 1201h, then 5502h with BX) - the accelerator mode set.
- **Callers, confirmed by a far call through the stored offset:** TEST.COM (64h) and VVESA (64h, 68h, 6Ch) only. No
  Windows driver calls it - they program modes themselves. (A first, looser scan also listed MACH.DRV; there 64h/78h are
  the numbers 100 and 120, a font scaling ratio.)

**For D8, a candidate:** set the Mach8 mode through ROM entry 64h - the EEPROM's monitor timings, no CRTC tables of our
own. Unproven from protected mode (no Windows driver does it); the ROM code uses SS as scratch. Still to read: each AL
function's arguments and effect (72DDh, 710Dh).

### The ROM's own 28800 use (disassembly, helpers at 2536h OR / 254Bh AND / 2567h read / 2572h write, AX = value:index)

The census pattern missed these: the ROM passes index and value together in AX. Uses found (bits as hex masks):

| Index | ROM use | Reading |
|---|---|---|
| A3 | OR 20; per-mode table bits 0-2 (58Bh) | mode table field |
| A6 | OR 20 if revision 6; OR 10 / AND EF around one routine; AND FB | revision-dependent bits; bit 2 cleared before EEPROM access |
| AA | read, low nibble compared with 5 and 6 | **chip revision** (28800-5 / -6) |
| B0 | many (OR 01/02/20, AND EF/DF/DE, WR 30) | mode control |
| B1 | the most used: OR 01/02/08/28/30/40/80, AND 05/67/C7/DF/FE | mode control (to map bit by bit) |
| B3 | bit-banged at 3A4Eh with B7 bit 3 read back | **EEPROM serial interface** (B3 = clock/select/data out, B7 bit 3 = data in) |
| B4, B5 | OR/AND on bits 0, 1, 3, 7 | to map |
| B6 | OR 01/02/04/08/18/20/80 | mode control; B6 2 = 256-colour banking (M8LIN) |
| B8 | OR 02/04/08/0B, WR 40 | mode control |
| B9 | bit 1 in the 16-clock loop; bits 4-5 per mode; OR 40/80 | clock select (with Misc Output bits 2-3 and BE bit 4) |
| BA, BB, BD | read / write | to map |
| BE | bit 4 in the clock loop; bit 5 if revision >= 5; OR 02/05/08 | clock select bit; revision features; bit 3 = VVESA's split banking |
| BF | OR 80 / AND 7F on one mode path | to map |

**Touched by no ATI code found** (ROM, VVESA, drivers, TEST.COM, tools): **A1, A4, A5, A8, A9, AB, AC, AD, AF, BC**.
A2: ATIDESK only. A7: VVESA only (HiColor DAC probe). AE: VVESA and ATI.VXD (bank high bits). These ten are L4's
read-back list; the rest are mapped bit by bit from the ROM's mode tables before any poke.
