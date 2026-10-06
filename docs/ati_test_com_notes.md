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

Diagnostic commits in the same tree (`[M8T]`, `[M8A]`, `[M8X]`, `[M8Y]` in the 86Box log) are not for
upstream.
