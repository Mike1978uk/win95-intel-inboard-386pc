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

## Next, in order

1. Read the 10-08 bed run of REGR with M8TS2 (`vm_6695/run_ts2.log`; image results): `python tools/m8seq/m8ts2_decode.py
   TEST.COM <bed M8TS2.BIN> docs/captures/2026-10-08_black/M8TS2_card.BIN`. The first differing sub-test names the
   register; its write table is in TEST.COM. Fix the model, rerun.
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
