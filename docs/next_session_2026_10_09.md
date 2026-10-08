# Next session - Mach8: ATI TEST.COM passes in the bed; regress, then the PR

Supersedes `docs/next_session_2026_10_08.md` for order (still the reference for 10-07 detail).

## Where it stands (2026-10-08, 23:35)

**ATI TEST.COM 1.46V1 passes every stage in the bed** - Register, FIFO, RAMDAC, Video RAM, Test Sequence 1,
Test Sequence 2, then "Hit a key to begin test patterns", the same screen as the real card
(`docs/captures/2026-10-08_black/TEST_COM_bed_b0639742e.txt` vs `docs/captures/2026-10-05_m8glyph/5160_TEST_COM.txt`).
Fork `inboard-ext-256k-diag` at `7fece103e` on `mike` (the last commit is a DIAGNOSTIC hook to drop before the PR).

Next, in order:
1. Run REGR in the bed (AUTOEXEC is back to `CALL REGR`) and diff all 39 probe outputs against the last run: the
   shared-DAC and shadow-lock fixes have not been through REGR yet (gate: regress all probes after each fix).
2. TEST.COM's test patterns (after the key) - the card shows 640x480, 800x600, 1024x768, 1280x1024; check the bed.
3. Regenerate `tools/m8seq/M8PRE.DAT` from a fresh `MACH8_WLOG=1` bed log: it still carries the old 6AEEh 049Ah.
4. Open for the PR: X mod 1024 only in LINEDRAW, raster and 8514/A line paths (rect/blit still linear);
   LINEDRAW read with LAST_PEL_OFF offers a 9th pixel (M8TS2Q checkpoint 1); DP_CONFIG BG/MONO sources unmeasured;
   SHADOW_SET split and 4AE8h GE_OFFSET reset (Mach8); TEST.COM speed vs the 5160 (owner: runs straight through fast).
5. Re-add the CRT shadow sets (reverted 10-09, fork `65c603e28`): the model copied the primary set into set 2 on
   the ROM's second setup pass (46EEh=0 written with the pointer at 2 and the lock already clear), so Windows/386
   2.11's 8514 driver got 640x480 at 1024x768 (`w2_ours_shadowsets_640.png` vs `w2_ours_noshadow_1024.png`).
   Copying only on a lock 1->0 transition (`shadowset_transition_rule.patch`) was NOT enough - still 640x480.
   Card data M8SHCL never rewrote an already-clear lock. Re-add with the Windows 2 boot (`vm_6695_w2_ours`) as a
   required test; without them M8REGS reads 1024x768 CRT values at boot where the card reads 640x480.
6. Paintbrush (86Box#6695), first run 10-09: ZSoft PC Paintbrush for Windows 1.05 on Windows/386 2.11 (8514.DRV 37,328 bytes,
   colour). Pencil, airbrush, lines draw; the paint-roller fill does NOTHING inside a closed outline - no leak, no hang
   seen - on ours (`ebb7bc429`, `pbrush_fill_ours_ebb7bc429.png`) AND on upstream master (09-25 build, owner's report, not
   captured). So long-standing, not from our changes. Next: `MACH8_WLOG=1` on one fill to see which engine ops and
   read-backs the driver's flood fill uses, then fix and report on 6695 (owner posts).
   Measured 10-09 (WLOG + RLOG on one fill, `vm_6695_w2_ours`): the fill is two 40F3h rect copies, then 300 x CMD 3318h
   (8514 line READ, degree 0, 16-bit, LSB first, MAJ 599) = Paintbrush reads the whole 600x300 canvas back (values
   sensible: white FFh, outline E0h), then writes NOTHING back - the rest is the software cursor (4 x C0B3h per move:
   save, AND mask 40h/300h, XOR mask 80h/300h, restore; cursor shape via 41B1h, WRT_MASK 80h). So the software flood
   in a memory bitmap finds nothing to do, or the driver's read conversion misleads it. Next: static - the Windows 2
   8514.DRV (bound into WIN200.BIN) read / ScanLR / BitBlt-to-memory code; compare with what 3318h returns.
   Diagnostics: MACH8_WLOG, MACH8_RLOG (fork DIAGNOSTIC commit).
   Static, 10-09 01:00 - driver now in `references/win2_8514/` (8514.DRV 37,328 bytes from W2 disk 7, = the 6695
   driver; `python tools/nedis.py references/win2_8514/8514.DRV exports|all`): SCANLR 4:0408 reads one row with CMD
   3318h (x..3FFh right / 0..x left), `rep insw` low byte first, then `repe/repne scasb` for the colour byte
   [bp+8]; PIXEL 3:0008 is the same read for one pixel; 1:3083 copies screen rows into a memory bitmap byte for
   byte. The fill = canvas copy (1:3083, 300 rows) + ONE left SCANLR at the click; that row's data is right
   (x 118-179 FF, 180-184 E0 outline, 185-280 FF) yet Paintbrush never scans right or draws. Next: a CPU trigger
   at SCANLR's scasb (4:0596) to log AL (the colour) and the AX it returns - if AL is not FFh the colour
   realisation (ColorInfo 1:28C6 / RealizeObject 1:2740) is the suspect.
7. Then the PR (owner writes the text), then the driver.

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

## Done 2026-10-08 afternoon (all pushed: main `master`, fork on `mike`)

| what | where |
|---|---|
| TS2 sub-test 1 fixed: line draw continues word by word on host data. 144 -> 141 differ | fork `2221087fd`, capture `M8TS2_bed_2221087fd.BIN` |
| TS2 sub-tests 2-11 fixed: 8514 planar read returns nuggets, bits 4..1 = columns 0..3 (Richter & Smith p. 299), RD_MASK rotated left one bit for IBM reads (ATI guide RD_MASK note 3), first group in the high byte with CMD bit 12 clear. 141 -> 116 differ | fork `50d2be32f`; fit `tools/m8seq/m8ts2_nugget_fit.py` (0 mismatches) |
| TS1 "blit source after ADVFUNC" lead closed statically: the Mach8 has ONE GE_PITCH/GE_OFFSET for source and destination (split is Mach32-only, guide pp. 9-20/9-21); TEST.COM never sets SHADOW_SET bits 9:8 nor writes GE_OFFSET. No card probe needed | this doc |
| Model gaps vs the guide, harmless for TEST.COM: SHADOW_SET[9:8] split applied to the Mach8 (`mach32` only); 4AE8h does not reset GE_OFFSET on the Mach8 | for the PR |
| TS2 sub-test 14 fixed: monochrome read (DP_CONFIG bit 2) on an extended blit returns 16 pixels per word, bit 15 first unless LSB_FIRST, pixel = 1 when `(P | ~RD_MASK) == FFh`, RD_MASK unrotated. Planar reads set reserved bit 0 like the card. Sub-tests 1-14 now identical to the card; 116 -> 112 differ | fork `2af5fbf35`, `ddf484f78`; capture `M8TS2_bed_ddf484f78.BIN` |
| TS2 sub-test 15 fixed. Static sources gave the shape only (guide pp. 8-29, 8-34: NIBBLE mode, one byte per 4-pixel group); M8BLRD measured the content: each byte = AND of the source pixels in one screen-aligned group, RD_MASK ignored, bytes run on across rows. Second bug found on the way: a DP_CONFIG write sets the 8514/A foreground source (register written last wins). 112 -> 108 differ, bed idle timeouts 32 -> 0; other 34 probes unchanged (M8REGS A9h varies every run) | fork `08adab8d1`, `20ca51adb`; `tools/m8seq/M8BLRD.ASM`; captures `M8BLRD_card.BIN` (= bed), `M8TS2_bed_20ca51adb.BIN` |
| TS2 sub-tests 16-23 fixed, statically (guide pp. 9-23, 9-48): BOUNDS_RESET gives L=T=2047, R=B=-2048; writes without it do not reset; every LINEDRAW point grows the box, a coordinate outside the device range -512..1535 counting as 2047/-2048 (fits all 8 sub-tests; -512..-1 untested). 108 -> 79 differ; other probes unchanged (boot bounds now match the card cold capture) | fork `16653c40f`; capture `M8TS2_bed_16653c40f.BIN` |
| TS2 sub-test 24 fixed: a 16-bit colour read takes pixels along the trajectory (first row from CUR_X, later rows from DEST_X_START), odd-width rows run on into the next, first pixel in D15:8 unless LSB_FIRST (guide DP_CONFIG), last word padded with 00. 79 -> 69 differ; other 36 probes unchanged | fork `a93a112f5`; capture `M8TS2_bed_a93a112f5.BIN` |
| TS2 sub-tests 25-71 (LINEDRAW pre-clip, EXT_GE_STATUS) fixed except PATT_INDEX: guide table p. 7-10 and pp. 9-29..9-34, 9-68; the rest fitted to TS2 (CLIP_FLAGS are outcodes, 1 = outside, opposite to the guide wording; POINTS_OUTSIDE cleared by a start point; CLIP_INSIDE rules; CLIP_MODE 0 drops lines leaving device space) - `tools/m8seq/m8ts2_clip_fit.py` fits all 70 compares, each fitted rule needed. CUR reads 11 bits. 69 -> 6 differ (PATT_INDEX 26-31: the read returned the written value). TS2 final 32 words still differ in 2 words (FFFF/FF00, FFFE/FFFD) | fork `6eb406741`; `tools/m8seq/m8ts2_table.py`; capture `M8TS2_bed_6eb406741.BIN` |
| **TS2 passes against TEST.COM in the bed: 0 of 150 differ.** PATT_INDEX read returns the drawing index (the engine already advanced it per step). Left vs the card: EXT_GE_STATUS bit 14 EE_DATA_IN (bed 1, card 0; TEST.COM masks it) and 2 of the final 32 words (FFFF/FF00, FFFE/FFFD). TS1 checkpoints unchanged | fork `f011fcc31`; capture `M8TS2_bed_f011fcc31.BIN` |
| **TS2 now identical to the card** (all 150 read-backs and the final 8x8 block). EE_DATA_IN reads 0 while the EEPROM is deselected (fork `23ccd0dfd`). Final block: M8TS2Q (TS2 checkpoint bisect, card capture `M8TS2Q_card.BIN`) put it at sub-test 22, a point at x=1365; in 8514/A-compatible pitch the card plots X mod 1024 (guide pp. 8-21/22) - modelled for LINEDRAW only (fork `261ab1548`), other draw paths still linear: open for the PR. M8TS2Q bed: one idle timeout at checkpoint 1 (LINEDRAW read with LAST_PEL_OFF offers a 9th pixel; card does not) - open, not hit by TEST.COM. **TS1 still 17 bytes off the card in M8TSX** (all 6 passes; counts, GP_STAT, timeouts now match) | captures `M8TS2_bed_261ab1548.BIN`, `M8TS2Q_bed_261ab1548.BIN`, `M8TSX_bed_261ab1548.BIN` |
| TS1: the card runs TS1 at the 8514/A-compatible pitch (TEST.COM never writes GE_PITCH; M8TSY forcing it gave the 17 bytes) where X is plotted mod 1024. TS1 ops 153/155 (raster, x 1363-1368) now wrap: M8TSX 17 -> 10 bytes off, all in word 3 (pixels 6-7) of rows 0-3, 6, 7. M8TS1/M8SEQ5 (they write GE_PITCH, linear) still equal the card. **Next: a TS1 bisect at the compatible pitch** (M8SEQ5 with M8TSX's setup, no GE_PITCH write) on card and bed to name the op; other draw paths (8514 rect/blit in vid_8514a.c, ATI blits) still address linearly. Bed TEST.COM after REGR: black screen after the CLOCK_SEL sweep and 4AE8h=2 - item 3 below, still untraced | fork `d037f08ca`; capture `M8TSX_bed_d037f08ca.BIN` |
| **TS1 now identical to the card** (M8TSX all 6 passes = TEST.COM's expected table). M8SEQ6 (M8SEQ5 without the GE_PITCH/OFFSET writes) on the card named the rest without a bed run: card M8SEQ5 vs M8SEQ6 diverge at checkpoints 4-6 (8514/A degree lines from (0,0) at 135/180/225 deg into x -1/-2) and 153/155 (the raster ops). The 8514/A line path masked X to 11 bits; a Graphics Ultra flag (`x_wrap`, set with `compat_pitch`) makes it 10. TS2, M8SEQ5, M8TS1 unchanged; other probes unchanged (M8ROW4/5 differ only in unwritten pad bytes). **Both TS1 and TS2 replays now equal the card. Next: TEST.COM itself in the bed - from a boot that skips REGR (after REGR it black-screens, item 3)** | fork `3a940860c`; `tools/m8seq/M8SEQ6.ASM`; captures `M8SEQ6_card.BIN`, `M8SEQ6_bed_3a940860c.BIN`, `M8TSX_bed_3a940860c.BIN` |
| TEST.COM itself in the bed (boot without REGR - bed AUTOEXEC.BAT now ends `REM CALL REGR`; put `CALL REGR` back for regression runs). Black screen was NOT REGR. Cause 1 (fixed): TEST.COM 797Eh writes A4h to the VGA DAC read index 3C7h and reads 2ECh, expecting A5h (one shared DAC); the model had two DAC address registers, so TEST.COM set 6AEEh bit 10 (049Ah) and the VGA went black. Now 009Ah. **M8PRE.DAT (from a bed log) still carries 049Ah - regenerate it.** Cause 2 (open): TEST.COM now runs TS1 (passes) and all of TS2 to its final fold, then waits for a key (heartbeat: INT 16h loop at 067C:71D9) - on the 5160 it runs straight through, so it is showing an error, on a black screen. No 3C6h writes in TEST.COM; VGA render uses `svga->dac_mask`. Next: a hook that dumps the VGA state and B8000 text at the wait (or logs TEST.COM's compare values) | fork `11412f112` |
| **TEST.COM passes in the bed.** Black screen cause 2: TEST.COM locks SHADOW_CNTL (003Fh); the Graphics Ultra branch of `mach_set_resolution` then skipped `svga_recalctimings`, so ADVFUNC=2 cleared `on` but the output stayed on `ibm8514_poll`, which draws nothing when off. Found with the MACH8_VGADUMP hook (VGA text page intact, VGA poll stopped). With it fixed TEST.COM shows its whole screen and every stage passes; the key-wait was its normal "Hit a key to begin test patterns". AUTOEXEC restored to `CALL REGR` | fork `b0639742e` (fix), `7fece103e` (DIAGNOSTIC hook); capture `TEST_COM_bed_b0639742e.txt` |
| 10-09 00:10: Windows/386 2.11 (8514) showed 640x480 with the 1024x768 desktop cut off - caused by the CRT shadow-set model (10-07b). Reverted; narrowed tonight's recalc to the 8514-off case. On that build: Windows 2 at 1024x768 again, REGR done, TEST.COM passes straight after REGR, M8TSX = card, TS2 = card | fork `65c603e28`, `ebb7bc429` |

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
   `tools/m8seq/m8ts2_decode.py`): 144 of 150 differed at the start of 10-08; now 0, and identical to the card including the final block. Groups:
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
