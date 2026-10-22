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
| EEEE | DEST_CMP_FN | tools, ATIDESK - no write to it in the Win95 ATIM8 (port census, `docs/atim8_port_census.md`) nor any Windows 3.x driver | destination colour key, nibble tests 40h-78h (measured 10-10) |
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
| (superseded) | | the census missed the ROM's AX-style accesses - see "The ROM's own 28800 use" below |

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
A2: ATIDESK only. A7: VVESA only (HiColor DAC probe). AE: VVESA and ATI.VXD (bank high bits).

**Already known before 10-16** (`mach8_card_coverage.md`, 28800 section: the M8VGA card read of every index, and vgadoc
`ATI.TXT`): A1 bits 3-4 monitor detect; A4/A5 ROM pages; **A8/A9 vertical line counter** (why A9 varies); **AB zero-wait
bits - measured 10-10, no gain** (the Inboard/XT bus cycle is the limit); **AC bit 0 linear addressing**; B7 bus/DRAM/EEPROM
data bits. A 10-12 census (`tools/m8_port_census2.py`, 306 files) found no Mach8 port outside the guide. Left genuinely
unknown: A0 bits 0-3, AD bit 4, AF, BC, and most B0/B1/B4/B5/BA/BB/BD/BF bits (map from the ROM's mode tables).

Levers this points at, for L4: **A8/A9** as a cheap raster position for palette splits and tear-free flips (D4, C2);
B2/BE split banking. (AC reads 03h on the card - bit 0 is already set, so M8LIN's "no linear window" result stands.)

### Microsoft's Win95 drivers: ATIM32.DRV and ATIM8.DRV (direct `mov dx,port` scan)

| | ATIM8 (Mach8) | ATIM32 (Mach32) |
|---|---|---|
| Short strokes (9EE8 / C6EE) | 0 / 0 | 0 / 0 |
| Source compare (92EE/EAEE), DEST_CMP_FN (EEEE) | 0 | 0 |
| FIFO status polls (9AEE / 9AE8 sites) | **314 / 63** | 197 / 22 |
| PATT_DATA / PATT_INDEX sites | 50 / 55 | 43 / 58 |
| DP_CONFIG values | 0007, 2231, 2211, 6211, 5211, 3251, A211, 5210, 0047, 4210, 4215, 4011, 4010, 2071 | 2211, A211, 2231, 0007, 6211, 3251, 5211, ... |

- **No Windows driver uses short strokes** - the census's C6EE hit for ATIM32 was a byte coincidence.
- **FIFO pacing is a bus-traffic lever:** each status poll is an ISA port read (~3.8 us). The Mach8 FIFO is measured 16 deep
  and drops writes when full, so our driver counts its writes and polls once per 16 entries at most.
- DP_CONFIG's working set (fill 2211, blit 6211, mono expansion 3251, colour pattern A211, ...) is the candidate list for
  D8; mapping each to its source/mono/pattern fields is part of L5.

### EEPROM: the undecoded words are monitor-timing records (ROM 7340h-744Bh, static reading)

The ROM scans **four 11-word records at EEPROM words 0Dh, 18h, 23h, 2Eh** (CL from 0Dh, step 0Bh, stop at 39h; read routine
3A4Eh, word number in BL). Per record: word 0 flags (bit 0 skips it; bit 6 picks one of two layouts - bytes into one
parameter block, or words into another); words 1-2 = the mode codes the record serves (each byte compared with the requested
mode); words 3-9 = timing. The card (`eeprom_card_5160_2026_10_07.nvr`):

| Record | Word 0 | Mode keys | Timing words 3-9 |
|---|---|---|---|
| 0Dh | 0000 | 1211, 0000 | 6509 559A E80B 0000 0000 3E40 0000 |
| 18h | 4040 | 5463, 6A00 | 7E4B 6896 3203 0000 0000 1F00 0000 |
| 23h | 0040 | 6567, 5500 | 4C24 420B 0208 0000 0000 FD60 0000 |
| 2Eh | 0140 | 6800, 0000 | C79F A00A 08F8 07FF 0861 0A33 162D (the set decoded earlier: H_TOTAL C7 ... CLOCK_SEL 2D, FIFO 16) |

**Which chip each record serves (ROM branches 7341h / 73BEh, static reading 10-16):** word 0's high byte bit 0 = 1 ->
**Mach8** record: 73BEh stores words 3-9 as H_TOTAL/H_DISP, H_SYNC_STRT/H_SYNC_WID, V_TOTAL, V_DISP, V_SYNC_STRT,
V_SYNC_WID/DISP_CNTL, CLOCK_SEL/FIFO - only record 2Eh (0140h). Bit 0 = 0 -> **28800** record, keyed by VGA BIOS mode
numbers; 7341h reads its timing only when word 0 bit 6 is set: 0Dh = modes 12h/11h, no custom timing; 18h = 54h/63h/6Ah,
the 800x600 modes (matches the card's 800x600 FlexView setting); 23h = 65h/67h/55h, the 1024x768 modes.
Previously "words 10h-15h and 23h-2Fh not yet decoded".

**Lever:** monitor timings are data the ROM reads at mode set, four slots. A custom record (refresh, blanking, a
non-standard resolution) changes a mode with no code - as long as the ROM sets the mode. Our driver can also read these
records and program the CRT itself.

## L4 card results, 2026-10-16 (`docs/captures/2026-10-16_l4/`; bed runs in `docs/captures/2026-10-10_a6/{dpm_bed,lev_bed}`)

| Probe | Card | Bed | Meaning |
|---|---|---|---|
| **M8DPM** DP_CONFIG 3251h host mono expansion | 10 cases | **= card, byte for byte** | bits run on across rows (a 12-wide row takes 12 bits), MSB first within a byte, low byte first. The text path (M8TIME C) can be developed in the bed |
| **M8SPLIT** split read/write banks | BEh bit 3 set: one-pass A000->A000 copy lands, 0/2048 mismatches; clear: 2048/2048 miss | same | **BEh bit 3 is the switch** (VVESA's reading confirmed). **No time gain**: 27,114 vs 27,044 ticks for 4 KB (~11 us per word) - every word still crosses the bus once each way, and the RAM pass costs nothing on the 486. Saves code and bank switches only |
| **M8RAST** line counter | A9h = counter, A8h = its high part (00/01); steps every ~2 lines; samples 0000h-BF01h. **A9h alone 11.5 us per poll**, pair 26.8 us, 3DAh 6.0 us | same layout | **a raster clock** at ~2-line resolution for about a third of a line per poll: palette changes at a chosen line (D4), tear-free flips (C2). 3DAh only gives in/out of retrace |

Card BEh read 20h, bed 30h (bit 4, a clock-select bit): likely the bed's EEPROM, not checked.
Per-read costs match the 2026-09-20 I/O measurement (~5.7 us per byte-wide port access).

## L4 second sitting, 2026-10-16 (`docs/captures/2026-10-16_l4/`)

- **M8SSV2 - EXT_SHORT_STROKE (C6EEh): the card does not follow the guide.** Every case drew one 45-degree diagonal of
  ~5-9 pixels in one of four quadrants; the second vector never appeared; a "move" drew. The first byte's top bits look
  like a quadrant (bit 7 down, bit 5 right), not the guide's eight directions. The bed (guide semantics) differs in every
  case (20/19/4/14/14/23 bytes). May depend on state the probe did not set (CMD). Unused by every driver; a model gap.
  Follow-up: one vector per write (null second byte), each direction alone, with a known CMD.
- **M8SHOW (owner watched): both sets of stripes** - the 8514 side reaches the monitor by ADVFUNC_CNTL alone.
- **M8FLIP3: black throughout** (owner). Difference from M8SHOW: M8FLIP3 sets VGA mode 12h through INT 10h first.
  Hypothesis: the BIOS mode set turns the 8514 display off (DISP_CNTL, 22E8h - the ROM's engine bring-up writes it). The
  8514 vsync still runs (6,397-7,006 polls per frame vs 5,019-5,020 for VGA). 120 fast switches ran. Next: M8FLIP4 with
  three arms - text mode as M8SHOW; after mode 12h; after mode 12h plus DISP_CNTL display-on.
- **M8BITS: no visible effect, no hang.** A0h bits 0-3, ADh bit 4 (reads 10h on the card), BCh all bits hold; **AFh
  always reads 00h** (read-only or absent). Memory-read timing ~7 us/word, +-15% with no pattern by bit. The line-counter
  arm failed on every record including the baseline: it wrote the index once and re-read 1CFh - **the 28800 latches the
  value at the index write**, so each counter poll must rewrite the index (M8RAST did; its 11.5 us includes that).

## L4 third sitting, 2026-10-16 (`docs/captures/2026-10-16_l4/5160_M8SSV3.BIN`; M8FLIP4 watched)

- **M8SSV3 - EXT_SHORT_STROKE decoded on the card (as set up: DP_CONFIG 2211h, LINEDRAW_OPT not written):** either byte
  draws on its own; a null vector draws nothing; a move (bit 4 clear) draws nothing. **Only 45-degree diagonals:** bit 7
  = down, bit 5 = right, **bit 6 has no effect** - four directions, no horizontal or vertical. Length 4 -> 5 pixels,
  length 15 -> 9. The guide's "IBM-compatible SSV" (eight directions) does not hold; the model follows the guide.
  Possibly LINEDRAW_OPT (A2EEh) changes it - untested. **Not a lever as found**; the 8514 SHORT_STROKE (9EE8h) is
  card-measured correct (M8ROW3) and is the one to use.
- **M8FLIP4 (owner watched):** A text mode -> white, but **not for the full 3 s**; B after mode 12h -> white; C mode 12h +
  DISP_CNTL 23h -> white; **D DISP_CNTL 60h -> black** (ENA_DISPLAY reset turns the 8514 output off, as the guide says).
  So a BIOS mode set does **not** cause M8FLIP3's black. Reading: **each switch between the halves makes the monitor
  re-sync**, taking over a second on the owner's monitor - M8FLIP3's ~1 s slow switches flipped back before anything
  showed. Consistent with the halves' different timing (VGA ~5,020 status polls per frame, 8514 ~6,400-7,000) and no
  phase lock (10-11). Owner confirmed: **black, then white, no loss of signal** - a gap of over a second at each switch.
- **C1 is answered: switching the output costs a monitor re-sync, so C2 (alternate frames between the
  halves) and C3 (raster split) are not viable as designed.** Both halves can still work together without switching the
  output - e.g. the CPU stages data in VGA memory while the engine draws.
