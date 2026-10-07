# ATI TEST.COM - what it checks, and what a Mach8 driver can take from it

`TEST.COM` from `M8UTL` (ATI Accelerator Series, Extended Test Sequence 1.46V1, 43,169 bytes, md5
`bf168ffd9f68f3a27bd8963d3dd39b63`, local copy `COMrade_Latest/XT_5160_rework_claude/ATI/ATIMACH8/M8UTL/`).
Pointed at by @andrew-hoffman on #49. The real 5160's Graphics Ultra passes every stage
(`docs/captures/2026-10-05_m8glyph/5160_TEST_COM.txt`).

**Full listing:** `python tools/comdis.py TEST.COM --out references/ati_test_com/TEST.asm` (ATI's code -
`references/ati_test_com/` is git-ignored; regenerate it rather than vendor it). `comdis.py` follows the
code from `0100h`; 24% of the file is reached as code, the rest is messages, screen art, tables and the
two test-sequence scripts. One indirect call, `0599 call si`, is a trampoline: callers load `SI` with a
routine and call it (`comdis.py` follows that pattern).

## Stage map

| stage | entry | what it does |
|---|---|---|
| Register Integrity | `2494` | writes tables of ports with `AAAA` then `5555` (`2410`, `2439`), reads each back against a fixed value; then `LINEDRAW` index 4/5 moves through `FEEE` and reads `CUR_X`/`CUR_Y` (`245E`, `246B`). On failure prints the port and the value read |
| FIFO Integrity | `23E9` | - |
| RAMDAC Integrity | `1793` | switches the display to the 8514 side (`069B`), then per pattern (`55`, `AA`) through `2EB`/`2EC`/`2ED`: set read index n-1 and read `2EC` = n; write 768 bytes, `2EB` = 0; read them back; `2EB` = 1 |
| Video RAM | `23A1` -> `7627` | per pattern and mix (`7539`): colour, fill by repeated blits (`76D2`, CMD `C0F1`), read 32 words from `PIX_TRANS` against the pattern, then `GP_STAT & 0300` and `SUBSYS_STAT & 04` must be clear (else `0615`, "Graphics Subsystem Failure"); then `75AE` |
| Test Sequence 1 / 2 | scripts at `2952`, `509C` | data, not code - an interpreter not yet located |

Error lines read `142tt mmmm cccc text`: test number, the message's address, the code passed to the
printer (`0776`). Only Register Integrity prints the failing port.

## Register read-back widths on the Graphics Ultra (measured over COMrade, 2026-10-05)

| port | register | write `FFFF`, reads | 86Box before |
|---|---|---|---|
| `82EE` | PATT_DATA_INDEX | `003F` (6 bits) | 5 bits |
| `D6EE` | PATT_INDEX | `001F` | no read (0) |
| `A2EE` | LINEDRAW_OPT | `06EE` | all 16 bits |
| `2EB`/`2EC` | DAC address | index `10` written, both read `11` | VGA state register |

## For our own driver

- **Program the engine from (port, value) tables** as ATI does (`0641`): wait for `9AEE` bit `10` clear before
  each entry, bounded; write ports below `1000h` and `22E8` (`DISP_CNTL`) as bytes. `4AE8` takes a delay
  first; `4AEE` (`CLOCK_SEL`) is written with bits 7:6 forced to `01`, a delay, then the real value.
- **Idle:** `GP_STAT` (`9AE8`) bit `0200` clear, at most 65,536 polls (`0827`). **Data ready before reading
  `PIX_TRANS`:** bit `0100` set (`080D`).
- **After a read-back,** ATI checks `GP_STAT & 0300` and `SUBSYS_STAT & 04` (invalid I/O) are clear - a
  cheap self-check worth keeping in a debug build.
- **`LINEDRAW` index 4/5 is a move,** not a draw (ATI register guide; `TEST.COM` relies on it).
- **The DAC's address register is readable** on `2EB`/`2EC`; there is no VGA-style state register.

## 86Box model changes so far (diagnostic tree `86box_3c509b`, local only)

`851a39139` `82EE` six bits; `ec3a00cc5` `D6EE` read, `A2EE` mask, Graphics Ultra DAC address read;
`0cc24cb14` + `c07be80ea` `LINEDRAW` index 5 as a move, masked to 11 bits. With these, Register Integrity
and RAMDAC Integrity pass in the bed. The Mach8 model is TC1995's code: the owner asked for this work;
upstream is his decision.

Later, each measured on the real card over COMrade first:

- `805bb2a56` FIFO test mode (`LOCAL_CNTL` bit 4) queues writes instead of carrying them out;
  `23f38fead` reading `FIFO_TEST_TAG`, not `FIFO_TEST_DATA`, takes an entry off the queue.
- `ea7d3951d` the idle report in `SUBSYS_STAT` is no longer gated on the 8514 display being off.
- `76b4cf4ee` (8514/A data path keyed on `dp_compat` alone) - reverted 2026-10-06 (`047c79e0a`): a
  revert changed nothing in the bed, so it is out until something needs it.
- **`78eeb22d5` (2026-10-06): `SCAN_TO_X` on the 8514/A path ignores `DP_CONFIG`'s read/write bit.** With
  FIFO test writes queued, `DP_CONFIG` stays `0000`, bit 0 clear reads as a pixel read, and the corner
  draws of the RAM Addressing test (`75AE`, table `7691`) set busy + data-ready and drew nothing. The
  ATI ROM runs the same test (same code and corner table, at `+125h` in `ATI_MACH8.bin`), so this also
  brought back the option ROM's `RAM Addressing` POST error that TC1995's `c54d36cff` had fixed (#8).
  With the fix the ROM prints `Testing........Ok` again.

**Bed, 2026-10-06, `78eeb22d5`:** Register, FIFO, RAMDAC and Video RAM pass. Still failing:
Test Sequence 1 `14216 27DB 0016 RAM Post Examination Failure` and Test Sequence 2
`14204 0602 0004 Graphics Subsystem Failure01`. Both are scripts (`2952`, `509C`); interpreter not
yet located.

Diagnostic commits in the same tree (`[M8T]`, `[M8A]`, `[M8X]`, `[M8Y]`, `[M8R]` in the 86Box log) are not
for upstream.

## Test Sequence 1, bisected with M8SEQ (2026-10-06)

Test Sequence 1 is not a script: it is port table `29A4` (1 MB card; `3D40` on 512 KB), 1,230 writes and
106 commands, played by the same runner at `0641` as every other table. `tools/testcom_tables.py` decodes
all 38 tables (output in the git-ignored `references/ati_test_com/tables.txt`). `tools/m8seq/` replays the
table up to each command, then does TEST.COM's fold and 8x8 read, so the real card and 86Box can be diffed
per command. The real card's final checkpoint is 17 bytes off TEST.COM's expected table: the replay forces GE_PITCH/GE_OFFSET, which TEST.COM does not (M8TSY, 2026-10-07).
Reference result: `docs/captures/2026-10-06_m8seq/5160_M8SEQ.BIN`.

Model fixes found this way, each checked against the real card's file in the bed (diagnostic tree):

| commit | first difference moved | what the real card does |
|---|---|---|
| `8f520484d` | command 0 -> 3 | a PIX_TRANS word read with 16-bit data (CMD bit 9) and LSB_FIRST (bit 12) clear (`43F0`) returns the first pixel in the HIGH byte; with LSB_FIRST set (`53B0`, M8GLYPH) in the low byte |
| `5bcd8bf03` | 3 -> 8 | lines and short strokes add X to the row as 11 bits unsigned: (-1,-1) lands at X 1023 of row 0, not the row above |
| `ab2fe76ef`, `5924277c2` | 8 -> 16 | Bresenham `DESTY_AXSTP`, `DESTX_DIASTP`, `ERR_TERM` are 13-bit signed (`1FFE` = -2), and a line steps diagonally while the error is not negative. Both scoped to `ATI_GRAPHICS_ULTRA` |

**Polygon-boundary lines, measured with `tools/m8seq/M8PL2` (16 lines in open space, read back directly; real
card over COMrade, `docs/captures/2026-10-06_m8seq/5160_M8PL2.BIN`):** octant bits 7/6/5 = +Y / Y-major / +X;
a Bresenham polygon line writes the LAST pixel of each scan line it crosses (endpoint included); a radial one
writes every pixel except that horizontal ones write nothing; X is clamped to the left scissor only and the
clamp does not move the line. `3642d259d` implements this for `ATI_GRAPHICS_ULTRA`; M8PL2 and M8POLY then match
the card in all 16 cases and all 8 TEST.COM polygon lines.

Later fixes, same method (M8SEQ now also checkpoints the Mach8 triggers `DEST_Y_END` and `SCAN_TO_X`, 172
operations; file version `M8S3`, reference `5160_M8SEQ3.BIN`):

| commit | first difference moved | what the real card does |
|---|---|---|
| `3642d259d` | op 16 -> mix sweep | polygon-boundary lines as above |
| `ebd093b15` | mix 1Fh | ATI mix 1Fh = (S+D > FFh) ? FFh : (S+D)/2 (Mach32 guide); 86Box halved first and never saturated |
| `433ae0650` | op 60 -> 62 | a VRAM-source non-conforming blit (`DP_CONFIG 6211`) leaves CUR_X/CUR_Y at its end; 86Box left them, so the next host-data blit started 7 rows high |

More, measured with `M8ROW` (rows 100h-10Fh read back directly after each operation) and `M8CMP`
(every DEST_CMP_FN code 40h-78h against a 00-FFh ramp, three compare colours), both run over COMrade:

| commit | first difference moved | what the real card does |
|---|---|---|
| `88f53e2aa` | op 62 -> 64 | a FRGD_MIX write after DP_CONFIG hands the source back to the 8514/A registers: op 62 fills with FRGD_COLOR although DP_CONFIG still says VRAM blit |
| `197776365` | 64 -> 70 | DEST_CMP_FN functions 2-7: TRUE leaves the pixel unchanged (Mach32 guide says so too); 86Box had them inverted |
| `7da6fc80c` | 70 -> 74 | DEST_CMP_FN bit 6 in 8 bpp is not in the guide's table: it ignores COLOR_CMP and tests a nibble for 0 or F - bit 4 picks the nibble (0 high, 1 low), bit 3 picks the side written, bit 5 is ignored (`5160_M8CMP.BIN`) |

Then, each measured over COMrade with `M8ROW3` (a 64x16 area read back directly at chosen checkpoints) and
`M8SRC` (which register writes switch a SCAN_TO_X to the 8514/A source):

| commit | first difference moved | what the real card does |
|---|---|---|
| `35da79908` | op 74 -> 77 | PATTERN_L (MULTIFUNC index 8) is the LEFT four pixels of the fixed pattern, bit 4 leftmost (Richter and Smith agree); the model took index 9 first |
| `54859c68b` | 77 -> 81 | a CPU-fed vector line with LAST_PIXEL off ends on its last drawn pixel and moves CUR to its end; the model waited for data for the undrawn point and kept CUR |
| `6ce5dee9f`, `27d9c1cbe`, `a8cd388f4` | 81 -> 84 | SCAN_TO_X: only a FRGD_MIX write (not WRT_MASK, RD_MASK, COLOR_CMP, BKGD_MIX, FRGD_COLOR, PIX_CNTL, CUR_X, SRC_X_START) after DP_CONFIG selects the 8514/A source, whatever DP_CONFIG bit 4 says; after a draw CUR_X is one past the end pixel; a zero-width SCAN_TO_X draws nothing and leaves CUR_X. `27d9c1cbe` also fixed my own `88f53e2aa`, whose hook sat in a case shared with four other registers |
| `a0bad65cc` | 84 -> 85 | a CPU-fed RECTANGLE with 16-bit data and LSB_FIRST clear takes the first pixel from the high byte (the lines that "already matched" had byte-symmetric data, AAAAh/5555h; see op 108) |
| `c9b4c970a` | 85 -> 87 | PIX_CNTL polygon mode on a rectangle: boundary = all RD_MASK bits set; a boundary flips inside/outside and is always filled; others filled while inside; RD_MASK planes cleared everywhere. The model sent `40FD` (bit 3 set) down the vectored path and drew nothing |
| `5b8db62f4`, `f825dab75` | 87 -> 91 | LAST_PIXEL off makes every rectangle row MAJ pixels and consumes MAJ pixels of CPU data |
| `9c50163ab` | 91 -> 95 | command 3 (`63F5`, "rectangle, Y direction") runs MAJ down each column, then the next column, MIN+1 columns; CPU words carry two pixels, high byte first. The model treated it as command 2 |
| `6059f3dd5`, `87e19f112` | 95 -> 98 | command 4 (`83F5`, "Y direction using nibbles", M8NIB): 4-pixel columns aligned to X mod 4, each data byte fills one row of one column; the first column runs in the command's Y direction from CUR_Y, the next back; LAST_PIXEL ignored; 16-bit data gives two bytes per word, MSB first unless LSB_FIRST |
| `bf2d347bd` | (M8EXP) | GE_PITCH applies to the extended path as soon as it is written |
| `3929cfd3d` | 98 -> 109 | host COLOUR data with 16-bit data and LSB_FIRST clear is MSB first for blits (op 99), lines (op 108, worked out from the fold: (A5h-12h)/2 = 49h on the card) and short strokes; host MONO data for lines keeps the low byte first (ops 78-79) |
| `6fae05813` | op 105 | PIX_CNTL bit 1 = polygon fill type B: WRT_MASK qualifies the outline, both edges fill, no planes cleared. Straight from the Mach32 guide's fill-type table (section 7, "Polygon Fills"); the card agreed |
| `c69bfbc3d` | (op 110) | a linear mono pattern is PATT_LENGTH+1 bits from PATT_DATA_10 on, from PATT_INDEX, MSB first in each 16-bit word; the model repeated one byte |
| `7a72b6a27` | 109 -> 110 | SHORT_STROKE with host data: the first vector of the pair takes no host data and draws with the low byte of the stroke word (the guide says only that it "does not consume host data correctly"); the second takes PIX_TRANS MSB first, then, when it runs dry, the low byte of the next FIFO entry whatever its port (TS1 writes 5678h to CUR_X there; the card draws 78 78 78) |
| `04956aedf` | 110 -> 112 | EXT_SHORT_STROKE (C6EE, a TODO in 86Box): two IBM SSV bytes, 15:8 first, drawn as extended degree-mode lines with the DP_CONFIG path; a PATT_INDEX write sets the next pattern pixel |
| `ed8cb37b0` | 112 -> 113 | a LINEDRAW (FEEE) line reaches its end point unless LINEDRAW_OPT bit 2; the model stopped one short |
| `4ecf27e40`, `5a781453f`, `39456c0e4` | 113 -> 116 | 16-bit host COLOUR data via DP_CONFIG is MSB first unless bit 12. DP_CONFIG foreground source 6 (not in the guide) waits for host data; each byte picks colour-pattern byte ((byte & 7) << 2) \| (x & 1), ignoring PATT_INDEX/LENGTH (M8FG6-9 on the 5160). Bit 11 set (CA11h): data starts 4 pixels late in every probe, 8 in TS1 op 112 - not explained by PATT_INDEX, PATT_LENGTH, CUR_X, write speed or a prior pattern draw (M8FG7-9); model keeps (byte >> 2), which fits TS1 only. OPEN |
| `03b909952` | 116 -> 134 | a non-conforming blit that wraps rows draws neither DEST_X_END nor reads SRC_X_END; source and destination are separate streams, first row from CUR_X/SRC_X. TC1995's `c54d36cff` counted both ends; Graphics Ultra only, and TEST.COM's Video RAM test still passes in the bed |

**The harness itself (M8SEQ `M8S4`, M8ROW3 `M8RC`).** TEST.COM runs routine `1E21` before every test
sequence: its `05F0` (32EEh = 0, SUBSYS_CNTL 900Fh/400Fh) and the setup table at `17E7` (PIX_CNTL, scissors,
FRGD_MIX 27h, BKGD_MIX 07h, colours, patterns, masks, two full-screen fills). Both tools now play exactly that
from the loaded TEST.COM before every checkpoint. Before, BKGD_MIX and COLOR_CMP kept whatever the previous
program left: a bed run after M8ROW3 104-107 (which reaches TS1's BKGD_MIX write) failed op 74, and the old
reference `5160_M8SEQ3.BIN` differs from the new `5160_M8SEQ4.BIN` at 10 checkpoints for the same reason.
Reference from now on: `5160_M8SEQ4.BIN`.

**Open:**
- op 134 onward, 39 of 173 checkpoints still differ (2026-10-06 evening); TS2 not yet bisected. Op 133-140 are one-row
  blits after writes to 92EEh (8, 10h, 18h, ...) and EAEEh (55h): Mach8-only registers, read/write (they are in
  TEST.COM's and the ROM's register-integrity port list at TEST.COM 741Bh / ROM 78BEh), not in the Mach32 guide,
  ignored by 86Box. The WIN31ACC.EXE "hits" were compressed bytes (it is an LHA SFX). No ATI Windows driver
  (ULTRA*.DRV, VDDULTRA.386) touches either port; the M8UTL/V3 utilities only read 92EEh. Only TEST.COM
  writes them. XFree86 3.3.6 mach8 does not name them either.
- the polygon type A rule above fills the right edge; the Mach32 guide says type A excludes it. Op 85's data may
  not tell the two apart - check before relying on it.
- the linear mono pattern rule is only applied to SCAN_TO_X; blits and lines still repeat one byte.
- Earlier: 86Box's 8514/A path ignores DEST_CMP_FN (the card applies it to 8514/A commands too); the 8514/A
  COLOR_CMP compare writes on TRUE (guide: TRUE leaves the pixel). Unmeasured / TS1 does not depend on them yet.

Bit names: the Mach32 guide calls CMD bit 9 DATA_WIDTH (1 = 16-bit host data) and bit 12 LSB_FIRST. Older
notes and code comments here called them "BYTE_SEQ" and "the 16-bit bit" the other way round.

(Superseded below this line: the first account of the polygon lines, before M8PL2.)

**Open: command 16 onward, polygon-boundary lines (`A0xx`).** The Mach32 guide: one pixel per scan line,
clamped to the left scissor. Measured (fold coordinates, read swap undone): radial vertical lines match;
radial horizontal lines draw **nothing** on the card (86Box draws every pixel - its vectored loop never
updates `oldcy` on a horizontal step); Bresenham polygon lines draw 2-4 pixels on the card and nothing in
86Box. A rule fit ("plot the pixel being left when the row changes") explains the radial lines only.
The fold mixes pixels, so the next evidence should be a probe that draws one polygon line into a cleared
area and reads the area back directly, on the 5160 and in the bed.
