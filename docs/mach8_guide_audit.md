# ATI programmer's guide vs the 86Box Mach8 model

Source: `references/ati_mach32/guide.txt` (ATI REG688000-15, 1993, OCR text). Each register page names the
chips it applies to (8514/A, Mach 8, Mach 32). Model: `86box_3c509b`, `src/video/vid_ati_mach8.c`.
Card values: the 5160's Graphics Ultra (113-11504-002, 8-bit slot), cold boot, M8REGS
(`docs/captures/2026-10-08_black/M8REGS_cold.BIN`).

Each gap is tagged by who it affects: **driver**, **TEST.COM**, or **cosmetic**. A gap is measured on the card
before the model changes.

## Done

| register | guide (Mach8) | model before | status |
|---|---|---|---|
| 6AEEh MAX_WAITSTATES (p. 9-9) | bit 10 PASSTHROUGH_OVERRIDE: 1 = VGA picture not passed; bit 9 IOR16_ENA; bit 8 LINE_OPT_ENA; 7:4 ROM_SPEED; 3:0 write waits. "Initialized by the POST ROM, should not be modified by applications" | wait states only | bit 10 modelled, fork `f3bc205f1`; found as the 5160 black screen (`docs/captures/2026-10-08_black/`) |

## Found, not yet changed

| register | guide (Mach8) | model | who | next |
|---|---|---|---|---|
| 7AEEh EXT_GE_CONFIG (pp. 9-17, 9-18) | two Mach8 layouts. 8-bit slot with ALIAS_ENA = 0: bit 0 EE_DATA_OUT, 1 EE_CLK, 2 EE_CS, 3 ALIAS_ENA, 4 "Z1280" (OCR; likely a 1280 mode), 7 EE_SELECT. 16-bit: 2:0 MONITOR_ALIAS, 3 ALIAS_ENA, 15:12 the EEPROM lines | stored only; all layout logic is Mach32 | driver (EEPROM access), TEST.COM | not the TS1 gap: TEST.COM does not write it before TS1 and the card ROM wrote 0Ah. Settle bit 4's name from a clean scan |
| DP_CONFIG (p. 9-?) LSB_FIRST | "ignored in mach8 mode when DATA_WIDTH = 0, not ignored in mach32 mode" | to check | driver | read the model's DP_CONFIG byte order path |
| 4AEEh CLOCK_SEL (p. 9-3) | 0 PASS_THROUGH (0 = VGA drives the output, 1 = 8514; confirmed on the card 10-08), 5:2 clock select, 6 divide-by-2, 7 refresh forcing, 11:8 VFIFO_DEPTH ("DRAM only"), 12 composite sync for shadow sets 1/2. "Writing it forces the CRT controller to ATI mode" (4AE8h forces 8514 mode) | uses bits 0, 2-6 only | cosmetic | the bed's TEST.COM hang after REGR (upper byte 18h vs 1Eh) is NOT explained by these bits - the model ignores them. Needs a trace of where the bed stops |

## Checked, consistent

| register | note |
|---|---|
| 36EEh FIFO_OPT (p. 9-8, write-only) | bit 1 HOST_8_ENA: 0 = 16-bit host data I/O, 1 = 8-bit; bit 0 W_STATE_ENA | model discards bits 0-3; reads back memory-size bits where the card reads 0000h | **measured 10-08 (M8BYTE5): 36EEh = 0, 1, 2, 3 give identical PIX_TRANS reads on the card in an 8-bit slot** - not the TS1 gap. Writes through PIX_TRANS not tested; the read-back value remains a small gap |
| 4AE8h ADVFUNC_CNTL (p. 8-6), 76EEh GE_PITCH (p. 9-20) | "Only mach8 will reset CRT_PITCH, GE_PITCH, CRT_OFFSET and GE_OFFSET when ADVFUNC_CNTL is written"; GE_PITCH also reset by MEM_CNTL | model resets the internal pitch only | **measured 10-08 (M8PLACE): on the card, markers drawn after a 4AE8h (2 or 7) or MEM_CNTL write land exactly where explicit pitch 80h / offset 0 puts them** - the reset state is not the TS1 gap. The register-level reset is still unmodelled (stale values on a later byte write) |
| 16EEh CONFIG_STATUS_2 (p. 9-65) | bit 0 SHARE_CLOCK, bit 1 HIRES_BOOT, bit 2 EPROM_16_ENA - modelled from the card on 10-07 (`f9b466ac2`) |
| 9AE8h GE_STAT (p. 8-40) | bit 8 DATA_READY "data is ready to be read by host". When it drops is not stated; measured on the card (M8BYTE 9) and fixed in the model 10-08 |

## TEST.COM structure, for the TS1/TS2 work

- TEST.COM's pre-TS1 reset (1E21h) never writes pitch or offsets; TS1 runs on what the earlier stages leave.
- Setup table 17E7h (30 writes) and TS1 29A4h (1,230 writes): no writes to pitch, offsets, 4AE8h, MEM_CNTL,
  EXT_GE_CONFIG, 36EEh or 6AEEh. Table player 0641h: port word with low byte 00 ends a table, FFFFh = wait idle.
- 3D40h (1,227 writes) is TS1's other branch (BX bit 15): 65E0h, 3D40h, 2155h, compare 3D20h.
- TS2 is small sub-tables, not one table. First (5105h, 15 writes): DP_CONFIG 4211h, LINEDRAW_OPT 0004h,
  SCAN_TO_X with host data 5555/AAAA/AAAA/5555; DP_CONFIG 4210h, LINEDRAW (FEEEh) (0,200h)-(8,200h).
  5074h is a read-and-compare walker over (port, expected, mask) triples.
- Card: reads at x >= 1536 return FFh even with the scissor at 2047 (M8PLACE).

## Not yet read

Next, for TS2: A2EEh LINEDRAW_OPT (p. 9-22), FEEEh LINEDRAW, CEEEh DP_CONFIG (Mach8 page), CAEEh SCAN_TO_X.

Chapter 8 (8514/A-compatible registers) and the rest of chapter 9, page by page.
