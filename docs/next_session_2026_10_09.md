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

**Built, not yet run in the bed** (`86box_3c509b/build_log`, 17:09, uncommitted): sub-test 14 - monochrome read
(DP_CONFIG bit 2) through an extended blit returns 16 pixels per word, from bit 15 down unless LSB_FIRST, pixel = 1
when `(P | ~RD_MASK) == FFh` (RD_MASK unrotated in ATI ops); and planar reads now set the reserved bit 0 the card
returns (all 20 card words). Run M8TS2 in the bed, decode, commit if sub-test 14 matches.

**Sub-test 15 (open):** 8514 colour rectangle 8x4 at y=206h, then CMD C3F0h BitBLT read (FRGD_MIX 67h). Guide p. 8-34:
a BLIT read ignores CMD[1] and acts as NIBBLE mode from the source trajectory, still drawing to the destination.
Expected 0000/1010/2020/3030 (card matches, bed FFFF) = each row's pixels AND F0h - not nugget-shaped (bit 5 set).
The rule that produces that is not found yet.

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
   `tools/m8seq/m8ts2_decode.py`): 144 of 150 differ, 32 idle timeouts in the bed, none on the card. Groups:
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
