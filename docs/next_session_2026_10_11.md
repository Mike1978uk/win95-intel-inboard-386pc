# Next session - Mach8: the whole-card coverage pass

Supersedes `docs/next_session_2026_10_10.md` for order; that file (items 1c-1i) holds the detail of 10-09.

## Owner's rules for this work

- **The whole card, both halves.** Our own driver, DirectDraw and the planned 3D will use parts of the card
  no ATI driver touches (demoscene methods, RAM on both halves, tricks nobody tried). Never rank fidelity by
  what existing drivers use; driver traffic is a test source only (skill technique 144, corrected).
- No upstream PR until every gap that matters for our driver is closed. The CPU fix (`5fce405ab`) is a
  separate PR. The 86Box#6695 reply waits for the Mach8 PR number.

## Where it stands (all on the card unless marked)

| | result | where |
|---|---|---|
| M8CONF, 82 Win 3.11 driver shapes incl. read-back data | **82/82 = card in the bed** | fork `9c9fa70c2` (pushed) |
| TEST.COM TS1/TS2, M8LOPT, M8WRAP | = card | `docs/captures/2026-10-09_m8probes/` |
| VGA half B2h page register (M8BANK) | as XFree86 says; page 8 wraps to 0 (512 KB) | `M8BANK_5160.BIN` |

Fixed 10-09 (Graphics Ultra only): colour-pattern rows restart at PATT_INDEX; LINEDRAW 8514/A setup;
scan reads (data width, byte order, end); CMD sets LINEDRAW_OPT's line bits; X mod 1024 in fills and blits.

Not measured, from the guide only: ERR_TERM -1 when X increases; CMD bit 3 copied (vs cleared); ATI blit
SOURCE wrap; host reads wrap and return FFh outside the scissors; polygon path at the compatible pitch;
BRES_COUNT direction after a CMD. The guide has held on every point tested so far - take its explicit
statements as reliable, label them guide-only, measure where a driver depends on them.

## Plan, in order

1. **Accelerator coverage pass** -> new `docs/mach8_card_coverage.md`, one row per register and feature:
   guide text (page, bits), model status (correct / partial / stored only / absent), card-measured?, driver
   use, possible use (driver, DirectDraw, 3D, demoscene - see `docs/mach8_graphics_vision.md`).
   - Sources: the Mach32 guide (`references/ati_mach32/guide.txt`; ~100 register descriptions found by
     scanning for "XXXX (R/W)" headings at lines 7000-13000) and **Ferraro ch. 19.7-19.24** (Mach8/Mach32
     engine) as a second source. Mark each feature Mach8 / Mach32 / both - the guide says which.
   - Read each model handler by hand (`86box_3c509b/src/video/vid_ati_mach8.c`, `vid_8514a.c`). An automated
     case-label cross-check was too noisy (ports written without the leading zero, other dispatch paths).
   - Start with chapter 8 (8514/A-compatible registers), then chapter 9.
2. **Appendices**: BIOS interface, EEPROM map (we have the card's dump), CRT parameters, clocks, RAMDAC.
3. **VGA half (ATI 28800-6, 512 KB)** - a VGA Wonder (Henry Worth 1993; dosdays' VGA Wonder GT has the same
   chip pair). Sources, all logged in `docs/resources_and_sources.md`:
   - **Ferraro ch. 19** (owner's text capture, OCR-grade): 28800 rev 1 vs rev 2+ banking, start and cursor
     address code; extended register tables 19.46/47 are the Mach32's VGA - check each against the 28800.
   - **XFree86 3.3.6 `vga256/drivers/ati/`** (`references/xfree86_336_ati/`, not vendored): B2h confirmed;
     clocks, CRTC, DAC, identification not yet read.
   - **Sutty and Blair** (18800 only, owner's PDF).
   - First fields to settle on the card: BEh bit 2 (128 KB window at A0000-BFFFF), B6h bit 5 (vertical
     interrupt), B6h bits 2/6 (linear addressing), B0h (memory size, 8-bit DAC). One COMrade probe each,
     like M8BANK.
4. Then, from the 10-10 list: Windows 95 capture into M8CONF (owner ~10 min, `vm_5160_now`, MACH8_WLOG=1);
   Mach32 / 8514/A regression (costed 10-09: build the branch point `48e098d45` and HEAD, AT bed
   `vm_6695_w2` with `mach32_isa` and with `8514a = 1`, 4 runs of REGR + M8CONF + M8WRAP, diff - about 2 h);
   TC1995 on blit ends (owner writes); clean branch and G1-G10.

## Traps met on 10-09

- COMrade `run_command` does not reach the prompt on the 5160 (keystrokes stall): ask the owner to type.
  `file_read`/`file_hash` sometimes time out once - retry once, then check `dos_status`.
- Windows Python and `gh` cannot see Git Bash's `/tmp`: use `C:/Users/lycet/AppData/Local/Temp`.
- A probe header must not name a decoder that does not exist yet - write both together.
- `out` is an instruction mnemonic in NASM: do not use it as a label.
- Test 58's line colour is inherited from earlier tests' state; it is not a model result on its own.
