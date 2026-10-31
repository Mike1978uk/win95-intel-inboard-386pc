# Mach8 static sweep: features the driver does not use yet (2026-10-31)

Desk work only. Nothing was built or run on the card or in a bed. Sources: `references/`, the ROM and
EEPROM dumps, the repo's own measurements. Items already tried, ruled out or proposed elsewhere
(`docs/mach8_driver_method.md:97`, `docs/mach8_levers.md`, `docs/mach8_unused_silicon_audit.md`,
`docs/atim8_port_census.md`, `docs/xfree86_and_all_tests_sweep.md`) are listed once, as such, not re-argued.

Result in one line: the register set is covered. Two of the five candidates below are bus-cost
questions the repo's own numbers leave open, not missing registers.

## Top 5 candidates

| # | Candidate | Cheapest card probe | Helps |
|---|---|---|---|
| 1 | **Wait-state knobs applied to PIX_TRANS streams**: MAX_WAITSTATES 6AEE bits 3:0 (reset Ch, POST sets Ah) and FIFO_OPT 36EE bits 0-1. Win95 ring 3 measured PIX_TRANS at 3.00 us/B against 2.17 us/B for an undecoded port (`docs/iowidth_measured_2026_09_20.md:42-46`): 0.83 us/B (~28 %) that only the card can add. Never swept for writes; B3 (`docs/worklist.md:141`) was scoped to a FIFO-full burst, and M8BYTE5 tried 36EE only for reads (`docs/captures/2026-10-08_m8byte/README.md:35-39`). | `tools/m8probe/M8UPRATE.C` with 6AEE low nibble = Ah (current), 6, 2, 0 and 36EE = 0, 1, 2, 3 between runs; same 5000 words, pixels compared. A short nibble can drop data, so check the image. Stop if INVALID_IO (42E8 bit 2) sets. | dib_8bpp, bmp_changing, mono_bmp, stretch_dib, text_varied (first sight), every host upload. A 100x100 upload is 10 KB: 0.83 us/B is 8.3 ms, near the 7.3 ms dib_8bpp gap. Upper bound; the whole premium may be ISA IOCHRDY and not removable. |
| 2 | **Single `out` against string I/O**: the DOS probe gives 7.64 us per word for `rep outsw` to FRGD_COLOR and to 0278 (`docs/mach8_card_coverage.md:120-135`, `docs/iowidth_measured_2026_09_20.md:30`), Win95 gives 4.34 us (2.17 us/B). If a looped `out` costs the DOS figure and a string costs the Win95 one, same-port runs should be strings: LINEDRAW points straight from the `POINT` array (x, y are adjacent words), BEE8 scissor/PIX_CNTL tables, PATT_DATA rows. The driver now sends everything through a (port, value) queue loop (`m8hw.c:336-373`) and `m8_line_solid` queues each coordinate (`m8hw.c:1358-1359`). | Extend `M8UPRATE.C`: 5000 words to A6E8 and FEEE by (a) `rep outsw`, (b) a loop of `out dx,ax` with the port reloaded each time, (c) the queue loop. One build, Win95, QueryPerformanceCounter. | lines, ellipse outline (`s18` ~3.5 ms), rubber_band, polygon outline, lines_dash. Size unknown until (a) against (c) is read; the gain is the difference per word. |
| 3 | **EXT_SHORT_STROKE C6EE with LINEDRAW_OPT = 0708h.** ATI's Win 3.1 MACH.DRV v2.3 writes A2EE = 0708h, then one `rep outsw` of vector words to C6EE (`references/mach_w23_disasm/seg07.asm:250-260`, tables at cs:190h/1AAh). M8SSV3 left A2EE unwritten and drew only 45-degree diagonals; it listed A2EE as "untested" (`docs/mach8_levers.md:218`). 0708h = BOUNDS_RESET (8), CLIP_MODE 3 (10:9), and bit 3, which the guide does not describe (`guide.txt:10373-10395`; `docs/atim8_port_census.md` row A2EE). | M8SSV3 again with A2EE = 0708h and 0008h before the C6EE writes; decode the 8 directions. Register exists on the card (it drew); the guide lists C6EE for Mach8 and Mach32 (`guide.txt:11578-11582`). | lines, lines_dash, ellipse outline. Two vectors of up to 15 px per word. GDI hands over points that are not unit steps (`docs/mach8_driver_method.md`, merge row), so a conversion to runs costs CPU; may not pay. |
| 4 | **VRAM-to-VRAM copy engine time: 8514 blit (C053) against extended blit (6011/6211), and by X alignment.** Never timed on the card. `docs/atim8_port_census.md` guesses the two are equal; the room-wait idea for cursor_move is blocked on "engine time of a 32x32 copy not measured" (`docs/xfree86_and_all_tests_sweep.md`, cursor_move). | New M8TIME arm: 256x256 and 32x32 copies x50, both command forms, X start 0..3 mod 4, read the PIT. Engine-only: no host data. | blit_large, scroll (both at ~1.0, engine-bound), cursor_move. If equal, closes the question; if not, a free change. |
| 5 | **Hardware polygon: CMD 5 boundary + fill blit (DP_CONFIG poly bit / PIX_CNTL poly fill).** Card-proven (M8PF, TS1; `docs/mach8_card_coverage.md:51,226`) and unused; ATIM8 sends polygons to DIBENG, so ATI's pixels are GDI's rule. Already named in `docs/xfree86_and_all_tests_sweep.md` (polygon, item 2) and `mach8_unused_silicon_audit.md` (polygon row). | Bed first: draw the bench's 6-point polygon by hardware and compare the hash with ATI's. No card needed until the pixels match. | polygon, ellipse fill, floodfill. Largest upside (~30-40 writes a polygon against ~200); highest cost; GDI's edge rule must match pixel for pixel. |

## Table

"Mach8?" is yes only where a card measurement, the card ROM or TEST.COM shows it. Guide page numbers are
the guide's own; `guide.txt:N` is the line in `references/ati_mach32/guide.txt`.

| Feature / register | Source | Mach8? | Our driver | ATIM8 / XFree86 | Test | Estimate |
|---|---|---|---|---|---|---|
| PIX_TRANS wait-state premium; 6AEE Q_WSTATES, 36EE bits 0-1 | `guide.txt:9720-9790`; `iowidth_measured_2026_09_20.md:42-46`; `m8hw.c:22-26` | yes (6AEE, 36EE exist; ROM writes both, `BIOS.BIN` 0x7EDA/0x7EEA, 0x7949) | no | ATIM8 writes 36EE from its INI and sets 6AEE bit 8 (`atim8_port_census.md` rows 6AEE, 36EE); XFree86 no | host uploads | candidate 1 |
| Same-port register runs by string I/O | as candidate 2 | yes (any port) | only PIX_TRANS and mono | MACH.DRV seg07:020a does it to C6EE; ATIM8 to PIX_TRANS | polyline tests | candidate 2 |
| EXT_SHORT_STROKE C6EE + A2EE 0708h | MACH.DRV seg07:01f2-020a; `guide.txt:11578`; ROM bytes at 0x3DC1, 0x47CE, 0x5715 (unclear: may be data) | yes (drew on the card, M8SSV3) | no | MACH.DRV v2.3 only; not ATIM8 | lines family | candidate 3 |
| Copy engine time, 8514 vs extended | `m8.h:164` (C053), `m8.h:126` (6011); ATIM8 6211 (`atim8_port_census.md`) | yes | 8514 form | ATIM8 extended | blit_large, scroll, cursor_move | candidate 4 |
| CMD 5 + poly fill | `guide.txt:8620`, `8679`, `10950-11010`, 7-10 at `6926ff` | yes (M8PF, TS1) | no | ATIM8 via DIBENG; XFree86 no | polygon, ellipse, floodfill | candidate 5 |
| Rect fill leaves MAJ/MIN unchanged; CUR_X/CUR_Y become x+w, y+h | `guide.txt:8571` (fill HOR side effect), `8676-8677` (blit: MAJ/MIN unchanged); XFree86 `mach8fc.c:149-157` | yes | writes both every fill (`m8hw.c:436-438`) | XFree86 shadows them | fill_small, dstinvert, blit_small, setpixel | already proposed (`xfree86_and_all_tests_sweep.md`), -2 writes ~15 us; one probe, same sweep |
| GE_IDLE_INT in SUBSYS_STAT bit 3 as a one-read idle test | `guide.txt:8013`, `8038`; XFree86 `regmach8.h:139` GPIDLE | yes | no | no | all idle waits | no gain: the bit is a latched transition flag needing an ACK write (7.6 us) to re-arm, against 2 reads now. Not a lever |
| PIX_CNTL fixed pattern (MIXSEL_PATT 40h), PATTERN_L/H (BEE8 idx 8/9) | `reg8514.h:73-74,235`; `mach8_card_coverage.md:70` | yes (ok/card, M8A2) | no | no (defined in XFree86, not used) | fill_hatch (ahead) | none: 4-pixel mono tile, hatch is solved by PATT_DATA rows |
| Source colour compare 92EE/EAEE | ROM port list `BIOS.BIN` 0x78B8, 0x78BE; TEST.COM; `m8.h:101-102` | yes (Mach8-only, in the ROM's register list) | defined, not used | neither | draw_icon, cursor | covered: `mach8_unused_silicon_audit.md` row 1 (GDI sends AND/XOR masks, not a key) |
| DEST_CMP_FN EEEE | `BIOS.BIN` 0x3B2F, 0x5670 (unclear); `mach8_card_coverage.md:224` | yes (card = bed) | no | no | floodfill | covered, unconfirmed use |
| SHORT_STROKE 9EE8 | `BIOS.BIN` 0x836, 0x1A44 (unclear); M8ROW3 | yes | no | neither | setpixel, lines_hv | covered: ~3 writes against 5-6, setpixel now at ATI |
| LINEDRAW preclip, CLIP_MODE (A2EE 10:9) | `guide.txt:10373`; Richter and Smith p. 318 (`richter_smith_graphics_programming_8514a.txt:16078-16086`) says "Mach 8"; the guide says mach32 | unclear: R&S names the Mach 8, the guide the mach32, and CLIP_OVERRUN lives in EXT_GE_STATUS 62EE, which reads 0 on the Mach8 | no | ATIM8 writes 070C; no effect on pixels (`mach8_driver_method.md:97` table) | lines | low: CPU clips for free |
| LINE_OPT_ENA 6AEE bit 8 | `guide.txt:9760-9770`; ATIM8 `seg02:1eb2-1ebc` | yes | no | ATIM8 sets it | lines | ERR_TERM exactness, not speed |
| W_STATE_ENA only for "all-extended" operations | `guide.txt:9726-9735`, `9855-9860` | yes | no | no | register bursts | folded into candidate 1; A14 form already measured, writes past 16 still dropped (`mach8_card_coverage.md:145-148`) |
| HOST_8_ENA 36EE bit 1 | `guide.txt:9866-9870`; M8BYTE5 | yes | no | no | uploads | reads unchanged; writes untried: candidate 1 |
| Byte writes to word registers | `captures/2026-10-08_m8byte/README.md:5-13` | yes | no | no | none | dead: one low-byte latch shared by all ports, high byte commits; a byte pair costs 2 x 5.7 us against 7.6 |
| Dword OUT to PIX_TRANS or adjacent registers | `mach8_driver_method.md` (M8DWORD row) | yes | no | no | none | dead: upper word lands in the DAC mask (E2EA decodes as 02EA) |
| Block write / 64-bit fill | `guide.txt:7093-7135`; MISC_OPTIONS bits 10-11 `9850-9890` | no: 68800-6 only, 64-bit also needs 2 MB; card has 1 MB | no | no | fill_large | none |
| 8x8 mono tile (D2EE bit 7) | `guide.txt:7061-7090` | no (M8PATT, 10-22: rows identical) | no | ATIM8 never sets it | fill_hatch | none |
| Hardware cursor 0AEE-1EEE, 3AEE/3EEE | `mach8_card_coverage.md:249`; ATIM8 writes them on a Mach32 branch (`atim8_port_census.md`) | no | no | no | cursor_move | none |
| Gray-scale font assist | `guide.txt:7054`; R&S line 16097 (Crystal Fonts, software) | no (mach32 section) | no | no | text | none |
| Split destination/source GE_OFFSET and pitch (SHADOW_SET 9:8), DEST_CMP mask F2EE | `guide.txt:7023-7035` | no (68800-6 onward) | no | no | none | none |
| Memory aperture through the VGA window | `mach8_driver_method.md` aperture row | no (M8APERT) | no | MACH.DRV mentions it | none | dead |
| GE interrupt for idle | `mach8_card_coverage.md:42` | no IRQ line on `mach8_vga_isa` | no | no | none | dead |
| A8/A9 raster counter, DISP_STATUS LINE_SYNC | `mach8_levers.md:190` (M8RAST) | yes | no | no | none | dead for GDI: output switch costs a monitor re-sync (M8FLIP4) |
| ROM API entries C000:0064/0068 | `mach8_levers.md:93-107` | yes | no | VVESA only | none | dead for speed |
| JU1 8/16-bit jumper: 16-bit slot faster | `web_captures/vogons_t28392_p2_mach8_graphics_ultra.txt:78,115` | 5160 slot is 8-bit | n/a | n/a | all | n/a |

## ROM and EEPROM

- Files: `roms/video/mach8/BIOS.BIN` (32 KB, md5 9270063457...c1d8c, "1992/4/8 12:16" at 0x50), `11301113140_ROM.BIN`
  and `_4k.BIN` (8 KB), `BIOS.BIN.old_11301115150` (64 KB). Searched `BIOS.BIN` only, for every Mach8 port
  as a little-endian word.
- The engine code is one block, **0x77A0-0x7A60**. It is the POST memory test and engine reset, not a GDI
  routine: SCAN_TO_X spans (CAEE at 0x77C5, 0x77D5, 0x77E5, 0x77F5 with CUR_X/CUR_Y/FRGD_COLOR), then a
  doubling BLIT (MULTIFUNC 0x7803, MAJ_AXIS 0x780A, CUR_Y 0x7813, CUR_X 0x7819, CMD C0F1 at 0x7827 and
  0x7848) that clears memory by copy-doubling. ATIM8's hatch replication and our doubling copies are the same idea.
- **0x78AE-0x78CC** is a port list: CEEE, 82EE, 86EE, 8AEE, 8EEE, 92EE, A6EE, BAEE, EAEE, D2EE, D6EE, DAEE,
  DEEE, E2EE, E6EE. It names 92EE and EAEE, which is the strongest evidence that the source colour compare
  exists on the Mach8. 8AEE and 86EE are read from the bytes; not otherwise confirmed.
- Isolated hits of unclear meaning (probably tables or fonts, not code): LINEDRAW at 0x1AC, 0x2029, 0x3B1D,
  0x3D6D, 0x4332, 0x473E, 0x51D0, 0x5671; SHORT_STROKE 0x836, 0x1A44; EXT_SHORT_STROKE 0x3DC1, 0x47CE, 0x5715;
  DEST_CMP_FN 0x3B2F, 0x5670; PIX_TRANS 0xFE, 0x152, 0x2266, 0x7369. No PIX_TRANS, CMD or DP_CONFIG loop that
  looks like a drawing routine. DP_CONFIG appears once (0x78AE, the list above).
- FIFO_OPT 36EE at 0x7949, 0x7EDE, 0x7EEE and MAX_WAITSTATES 6AEE at 0x7EDA, 0x7EEA: the POST writes the two
  knobs of candidate 1. Values not decoded.
- EEPROM (`eeprom_card_5160_2026_10_07.nvr`, 64 words): monitor-timing records at words 0Dh, 18h, 23h, 2Eh;
  only record 2Eh is the Mach8's (`docs/mach8_levers.md:159-182`). Words 00h-0Ch and 0Fh-10h are not decoded
  in any repo doc (flags, monitor id, memory mode); nothing there is a drawing option. The blank-EEPROM rule
  (`MEMORY.md`, ATIM8 fails above 640x480) is the only performance-relevant field known.
- `tools/m8eedump/m8eedump.asm` reads it through B3h/B7h; read-only.

## Sources read and what they did NOT contain

- `references/ati_mach32/guide.txt` (18,358 lines): full register set, Mach32 marks are OCR-damaged ("Macha",
  "mach32" in headers); no timing figures for any register; no ISA wait-state numbers.
- `references/ati_mach32/ports.txt`: 71-line port index, no semantics.
- `references/8514a_docs/richter_smith_graphics_programming_8514a.txt`: lists the Mach 8 enhancements
  (16062-16135) and says register-level beats the AI by 2x (9219); no Mach8 timing, no driver technique.
- `references/8514a_docs/ibm_8514a_techref_198704.txt`, `os2museum_8514a.txt`: IBM 8514/A base; skimmed for the
  planar mode (R&S 11374-11385: nibble mode up to 4x, already set in `M8_CMD_FILL`); nothing ATI-specific.
- `references/8514a_docs/ardent_*.txt/.html`: web pages (driver download list, 2.6-10 KB). No register content.
- `references/8514a_docs/ardent/ATI_8514_Ultra_Manual_compact.pdf`, `_v2_1.pdf`: **image scans, no text layer**
  (`pdftotext` returns 0 words); unread. The `.txt` beside each is a 35 and 83 byte stub.
- `ATI_8514_Ultra_Disk1.zip`: INSTALL.EXE, TEST.COM (42,462 B, already in `ati_test_com`), 8-INFO.EXE, README.TXT
  (installation notes only: ROM address, TEST.COM false failure on fast machines). 8-INFO.EXE (19 KB) unread.
- `ATI_8514_Ultra_Disk2.zip`: LHARC, application directories. `ATI_8514_Ultra_Win31.zip`: Win 3.1 drivers
  (W31-72B, ULTRA*.DR_, ULTRABIG, ULTRAGS = Crystal Fonts) and a README with install steps and a PowerPoint GP
  fault note; compressed (SZDD), not disassembled; no register information. The 8514 Ultra is the predecessor
  board, not our card.
- `references/ati_test_com/TEST.asm`, `tables.txt`: factory test; its table player confirms the register set
  and 92EE/EAEE use. Already mined by the port census.
- `references/vgadoc/ATI.TXT` (2,707 lines): 28800 VGA registers; mined 10-16 (`mach8_levers.md`), nothing on
  the 8514 engine.
- `references/xfree86_336_mach8/` and `ibm8514/`: all engine writes are the registers already in `m8.h`;
  `MIXSEL_PATT`/`PATTERN_L/H` are defined and unused. No short stroke, compare or extended blit anywhere.
- `references/xfree86_336_ati/`: VGA Wonder (28800) mode and bank code; not the engine.
- `references/mach_w23_disasm/` (MACH.DRV v2.3): the C6EE use above (seg07:01db-020a) and an 8514 fill setup at
  seg01:c6b3-c6f5 (DP_CONFIG 2211, ALU 7). Otherwise ATIM8's pattern.
- `references/atim8_disasm/`: not re-read; `docs/atim8_port_census.md` stands.
- `references/web_captures/vogons_t28392_p2_mach8_graphics_ultra.txt`: VGA-half speed and ISA clock tolerance
  (lines 74-82, 113-115, 285-296); no engine data. `realdoom_*`, `vogons_t101489_*`: unrelated.
- Other `references/` directories (3c509b, cfu1, ds1315, inboard386, jhrobotics, velocity9x, xga-win9x,
  win2_8514, pbrush105): not Mach8; `win2_8514/8514.DRV` is IBM's, not read.
- Vendor README/WRI files for the Graphics Ultra (`ATIDESK/MACH8.WRI` is cited in
  `docs/mach8_driver_method.md:117` but is not in this tree; it lives with the owner's `XT_project/ATI`
  copy). None found under `vxd-patches/ati/` or `roms/`.

## One inconsistency worth settling first

The cost model the driver is built on says one port access is 7.6 us and flushes the L1 (`docs/captures/
io_cache_flush_2026_09_21.md`, `iowidth_measured_2026_09_20.md:30`). The same repo records a Win95 string write
at 4.34 us a word to an undecoded port and 6.0 us to PIX_TRANS (`M8UPRATE.C`, `iowidth...:42`), and the ATIM8
upload at 4.3-4.7 us a word (`docs/atim8_vs_ours_lagging_tests.md:9-10`). Candidates 1 and 2 are one program
run: they say how much of the 7.6 us is bus, how much is the card, and how much is the instruction stream.
