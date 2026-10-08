# Next session - Mach8: TS2 first bed difference, then TS1's 17 bytes

Supersedes `docs/next_session_2026_10_08.md` for order (still the reference for 10-07 detail).

## Done 2026-10-08 (all pushed: main `master`, fork `inboard-ext-256k-diag` on remote `mike`)

| what | where |
|---|---|
| 8-bit bus measured: FIFO-side ports latch the low byte, high byte commits; high-byte PIX_TRANS read pops a word | `docs/captures/2026-10-08_m8byte/`, fork `442d373df` |
| 5160 black screen after M8TSX = 6AEEh bit 10 PASSTHROUGH_OVERRIDE (TEST.COM writes 049Ah) | `docs/captures/2026-10-08_black/`, fork `f3bc205f1`, M8TSX fixed `11e013b` |
| GP_STAT data ready dropped one read late in the model | fork `46d9ad164`; M8TSX GP_STAT now 0000h like the card |
| Ruled out for TS1 by card measurement: bus width, 36EEh host-data width (M8BYTE5), ADVFUNC/MEM_CNTL reset state (M8PLACE) | audit doc |
| ATI programmer's guide audit started (~15 of ~110 pages) | `docs/mach8_guide_audit.md` |
| TS2 decoded: 71 sub-tests from TEST.COM's pointer table 5A31h, 150 read-backs; M8TS2 replays it; card matches all 150 + final 32 words | `tools/m8seq/M8TS2.ASM`, `m8ts2_decode.py` |
| Rule: derive from ROM/EEPROM/docs/drivers first, one card probe confirms | skill technique 147 |

TEST.COM's TS2 error `14204 0602 0004` is NOT data: 1E49h is the generic compare-failed message (DX=4, SI=0602h).

## Done 2026-10-08 afternoon (fork commits local, NOT pushed)

| what | where |
|---|---|
| TS2 sub-test 1 fixed: line draw continues word by word on host data. 144 -> 141 differ | fork `2221087fd`, capture `M8TS2_bed_2221087fd.BIN` |
| TS2 sub-tests 2-11 fixed: 8514 planar read returns nuggets, bits 4..1 = columns 0..3 (Richter & Smith p. 299), RD_MASK rotated left one bit for IBM reads (ATI guide RD_MASK note 3), first group in the high byte with CMD bit 12 clear. 141 -> 116 differ | fork `50d2be32f`; fit `tools/m8seq/m8ts2_nugget_fit.py` (0 mismatches) |
| TS1 "blit source after ADVFUNC" lead closed statically: the Mach8 has ONE GE_PITCH/GE_OFFSET for source and destination (split is Mach32-only, guide pp. 9-20/9-21); TEST.COM never sets SHADOW_SET bits 9:8 nor writes GE_OFFSET. No card probe needed | this doc |
| Model gaps vs the guide, harmless for TEST.COM: SHADOW_SET[9:8] split applied to the Mach8 (`mach32` only); 4AE8h does not reset GE_OFFSET on the Mach8 | for the PR |
| TS2 sub-test 14 fixed: monochrome read (DP_CONFIG bit 2) on an extended blit returns 16 pixels per word, bit 15 first unless LSB_FIRST, pixel = 1 when `(P | ~RD_MASK) == FFh`, RD_MASK unrotated. Planar reads set reserved bit 0 like the card. Sub-tests 1-14 now identical to the card; 116 -> 112 differ | fork `2af5fbf35`, `ddf484f78`; capture `M8TS2_bed_ddf484f78.BIN` |
| TS2 sub-test 15 fixed. Static sources gave the shape only (guide pp. 8-29, 8-34: NIBBLE mode, one byte per 4-pixel group); M8BLRD measured the content: each byte = AND of the source pixels in one screen-aligned group, RD_MASK ignored, bytes run on across rows. Second bug found on the way: a DP_CONFIG write sets the 8514/A foreground source (register written last wins). 112 -> 108 differ, bed idle timeouts 32 -> 0; other 34 probes unchanged (M8REGS A9h varies every run) | fork `08adab8d1`, `20ca51adb`; `tools/m8seq/M8BLRD.ASM`; captures `M8BLRD_card.BIN` (= bed), `M8TS2_bed_20ca51adb.BIN` |
| TS2 sub-tests 16-23 fixed, statically (guide pp. 9-23, 9-48): BOUNDS_RESET gives L=T=2047, R=B=-2048; writes without it do not reset; every LINEDRAW point grows the box, a coordinate outside the device range -512..1535 counting as 2047/-2048 (fits all 8 sub-tests; -512..-1 untested). 108 -> 79 differ; other probes unchanged (boot bounds now match the card cold capture) | fork `16653c40f`; capture `M8TS2_bed_16653c40f.BIN` |

DP_CONFIG's BG_COLOR_SRC and MONO_SRC reaching 8514/A commands are not measured (M8BLRD only exercises the foreground).

## (done) TS2 sub-test 1 root cause, 10-08 morning

M8T2S1 (`tools/m8seq/M8T2S1.ASM`, captures `docs/captures/2026-10-08_black/M8T2S1_{card,bed}.BIN`): the raster
write is right in the bed (rectangle read = card's 55 55 AA AA AA AA 55 55); TS2's LINEDRAW read is wrong (bed
55 55 then zeros, card correct). Cause, `vid_ati_mach8.c` `mach_accel_start` case 3 (Direct Linedraw, ~line 1626):
on host-data calls (cpu_input, count = pixels in the word) it overwrites `count` with the whole line length and
re-initialises `mach->accel.err` every call, so the first PIX_TRANS word runs the entire line. Fix: compute the
length and err only when !cpu_input (store total in `mach->accel.width`, already set); on cpu_input draw
min(count, width + 1 - sx) pixels and keep err. Then rerun M8T2S1 and M8TS2 in the bed. Also seen there: the
model sign-extends coordinates >= 600h (`|= ~0x5ff`), and CUR_X/CUR_Y read back raw (FE01h) where the card
returns 11 bits (0601h) - the CUR group in item 1.

## Next, in order

1. TS2 bed vs card (`docs/captures/2026-10-08_black/M8TS2_bed_46d9ad164.BIN` vs `M8TS2_card.BIN`, decode with
   `tools/m8seq/m8ts2_decode.py`): 144 of 150 differed at the start of 10-08; now 79, first is sub-test 24. Groups:
   - EXT_GE_STATUS 62EEh (sub-tests 25-71): bed 4xxxh (bit 14 set, low bits counting); card clip/status flags
     (8501h, 9000h, 1400h...). Read the guide page (p. 9-68) first.
   - CUR_X/CUR_Y after lines leaving the clip area (26-34, 51-57): bed FE01h/FDFFh, card 0601h/0602h/01FEh - the end
     position differs, not only the mask.
   - Pattern index D6EEh (26-31): bed stuck at 2, card advances 3-6.
   - Bounds accumulator 72EE/76EE/7AEE/7EEEh (16-23): bed 0000h/07FFh, card reset values 07FFh/F800h and real bounds.
   - PIX_TRANS read-back (1-15, 24): zeros/garbage; sub-test 24 bytes swapped (bed 0403h, card 0304h).
   Work them from sub-test 1 up; each sub-test's write table is in TEST.COM (pointer table 5A31h). Rerun M8TS2 in the
   bed after each fix; the card file is the reference.
2. TS1 17 bytes: last lead is the blit SOURCE pitch/offset after an ADVFUNC write (M8PLACE tested only the
   destination). One card probe: marker, 4AE8h, blit, read.
3. Bed-only: TEST.COM hangs after REGR (after its CLOCK_SEL sweep). Not explained by CLOCK_SEL bits. Needs a trace.
4. Guide audit: rest of the pages; open entries in the audit doc (DP_CONFIG LSB_FIRST with DATA_WIDTH 0, 7AEEh 8-bit
   layout, 6AEEh bit 8 LINE_OPT_ENA which ATI's MACHW3.DRV toggles).
5. Card reads at x >= 1536 return FFh (M8PLACE) - model check.

## Notes

- `XT_project/ATI`: all four TEST.COM copies are identical v1.46. MACHW3.DRV sets/clears 6AEEh bit 8 around drawing.
- Bed `vm_6695` now has the fixed M8TSX and M8TS2 in REGR; REGR runs at boot.
- Fork's remote for pushes is `mike`, not `origin` (upstream 86Box).
