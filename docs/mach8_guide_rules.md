# Mach8 engine rules from the guide

Source: ATI Mach32 Register Reference Guide, REG688000-15 (`references/ati_mach32/guide.txt`), checked
against the real card (5160, 2026-10-20/21, `docs/captures/2026-10-21_shadow/`) and ATIM8.DRV
(`docs/atim8_disassembly.md`). Each rule names the guide page, what our driver must do, and how the
card showed it. Read this before adding an engine path or caching any register.

The bed (86Box) models only some of these: a driver that passes the bed can still hang the card.

## The data path is one set of registers seen two ways

| Rule | Page | Driver |
|---|---|---|
| DP_CONFIG (sources, mono source) and ALU_FG/BG_FN are the extended view of what FRGD_MIX, BKGD_MIX and PIX_CNTL set. A write to either side changes what the other holds. | 7-1, 9-13 | Never cache DP_CONFIG, FRGD_MIX, BKGD_MIX, PIX_CNTL: every operation writes its whole data path, as ATIM8 does (CEEE + BAEE per blit). Cached, text hung the card and screen copies drew solid squares. |
| Writing DP_CONFIG halts and flushes the blit source FIFO; nothing else does (bar a blit source register or engine reset). | 9-41 | Write DP_CONFIG before every extended blit, or it continues from the last one's data. |

## Host data

| Rule | Page | Driver |
|---|---|---|
| PIX_TRANS is BKGD_COLOR at another address. Outside a command, E2E8 writes land in BKGD_COLOR (E6E8 in FRGD_COLOR); data past the end of a blit overwrites BKGD_COLOR. | 8-14, 9-42 | Forget the cached BKGD_COLOR after every host-data transfer. |
| While a CPU_WAIT command still takes data, only A2E8/A6E8/E2E8/E6E8 count as data; any other FIFO write cancels the wait and the rest of the rectangle draws without data. FRGD/BKGD_COLOR writes in that window are eaten as pixels. | 8-14 | Send exactly the data the command takes. A write queued after the last data word is read as a register write, because the engine interprets each entry when it consumes it (8-14; card 10-21, `card_noidle2`): no idle wait is needed at the end. Nothing (cursor included) may write mid-transfer (`m8_busy`). |
| A PIX_TRANS read with GE_STAT DATA_READY clear locks the FIFO; a locked FIFO (SUBSYS_STAT bit 2, INVALID_IO) ignores writes and PIX_TRANS until the engine is reset. | 8-14 | Never read without DATA_READY; on a timed-out wait reset the engine (SUBSYS_CNTL 900Fh, 500Fh, as ATIM8) and rewrite the set-up. The card's wedge was this. |
| Extended mono host data is 8 pixels per byte; 8514/A-compatible mono is 4 per byte. | 7-1 | Mono through the extended blit only. |

## FIFO and status

| Rule | Page | Driver |
|---|---|---|
| 16-entry command FIFO; a write with all 16 full overruns it (INVALID_IO). Extended registers get wait states when full; 8514/A registers at A14 = 0 do not. | 7-5, 8-14, 9-9 | Count free slots from an empty FIFO; bursts stop at 15. |
| GE_STAT bit 8 DATA_READY, bit 9 GE_BUSY; bits 7:0 show only 8 FIFO entries. EXT_FIFO_STATUS (9AEE) shows all 16. EXT_GE_STATUS (62EE) is Mach32 only (reads 0 on the Mach8). | 8-40, 9-16 | Idle = EXT_FIFO_STATUS 0 and GE_BUSY clear (ATIM8's test); a queued command has not set GE_BUSY yet. |

## Geometry

| Rule | Page | Driver |
|---|---|---|
| Extended blit: CUR_X and DEST_X_START inclusive, DEST_X_END and DEST_Y_END exclusive (left to right, top to bottom). Start = end is indeterminate; DEST_Y_END = CUR_Y draws nothing. The write to DEST_Y_END starts the blit. | 9-40 | Return before an empty blit; one left waiting for host data wedges the engine. |
| MAJ_AXIS_PCNT is width - 1 (0..2047). | 8-41 | A zero width wraps to 2047: guard w, h <= 0 before every 8514/A rectangle. |
| CMD: opcode 15:13, LSB_FIRST 12, 16-bit 9, CPU_WAIT 8, direction 7:5, DRAW 4, NIBBLE 1, write 0. NIBBLE is faster when no host data is involved. | 8-28 | Our five command words decode correctly. |
| PIX_CNTL (BEE8 index A): COLOR_CMP_FN 5:3, MONO_SRC 7:6 (0 always 1, 2 PIX_TRANS, 3 blit source by mono read; RD_MASK picks the plane). | 8-23, 9-? | A000 for plain draws; A0C0 for the plane blit. |

## Still to read (for planned work)

- Dashed lines: LINEDRAW_OPT 9-22, direct linedraw 9-29, clip mode 3 patterned lines 9-34, pattern registers 9-46.
- Polygon and floodfill: polygon boundary line 8-33, clip mode 2 9-29, PIX_CNTL polygon bits, BOUNDS 9-48.
- Hatch: pattern registers 9-46 (ATIM8 `seg16:3ccf`).
- Fills: block write 7-14, 64-bit fill 7-14.
