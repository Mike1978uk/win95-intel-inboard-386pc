# Next session - Mach8: finish the gap list, then the PR

Supersedes `docs/next_session_2026_10_09.md` for order (still the reference for 10-08/09 detail).

## Owner's rule (10-09)

No PR until every gap that matters for our driver is closed: "if they matter for us and our driver they
matter for the PR". The CPU fix (`5fce405ab`, fetch from pages with no exec pointer) is a separate PR.

## Done 2026-10-09 (all pushed except where noted)

| what | where |
|---|---|
| CRT shadow sets re-added: per-set locks, SHADOW_SET write shows set 1 until the next ADVFUNC, primary set powers up zero, DISP_CNTL in the sets, read-backs from the shown set; vertical read-backs decoded (guide SKIP_2) | fork `e749c3496` `bf68c27d6` `3b34eb220` `e0cbfc1c7`; card probes M8SHRL/M8SHR2 = bed exactly |
| Windows/386 2.11 at 1024x768; REGR = pre-shadow baseline except CRT read-backs; TEST.COM passes | `w2_ours_1024_e0cbfc1c7.png` |
| 6695: fill works with border set, no hang outside canvas, no fade glitch; filled-box border is real hardware - M8RECT card = bed byte for byte | `M8RECT2_5160.BIN`, `M8RECT2_bed_e0cbfc1c7.BIN`; skill technique 148 (pclog repeats) |
| Vertical read-back decode gated to the Graphics Ultra (Mach32 untouched) | fork `fd653e6b9` |
| **Win 3.11 text was blank** (MACHW3.DRV font upload, DP_CONFIG 2051h): 8-bit mono host data must come from D15:8 - exposed by the 10-08 8-bit bus fix. Fixed; text back | fork `dcfbf6e67`; `w311_sb_dialog_notext_fd653e6b9.png` vs `w311_text_back_dcfbf6e67.png` |
| M8PRE.DAT regenerated (6AEE 009Ah); unchanged by the repeats fix | `8170a2f` |

Fork branch `inboard-ext-256k-diag` on `mike`. Bed `vm_mach8_w311` now carries the FlexView EEPROM
(blank one kept as `nvr/mach8.nvr.blank_bak`).

## In progress: the conformance probe (items 2-4)

Every operation shape ATI's drivers use, replayed from real driver writes on card and bed, pixels compared.

- `tools/m8seq/gen_m8conf.py` - extracts one instance per shape (trigger, DP_CONFIG, ALU_FG/BG_FN, host
  data) from MACH8_WLOG logs, largest up to 2,048 px, with the driver's state in last-write order.
- `tools/m8seq/M8CONF.ASM/.COM` - per test: reset, state, pattern (x XOR 5y, whole 1 MB), state, op,
  two 64x16 reads (53B0h). Output M8CONF.BIN: "M8CF", per test 4 + 2048 bytes.
- Win 3.11 capture: `vm_mach8_w311/conf_w311_dcfbf6e67.log` (118 MB) -> 82 shapes ->
  `vm_6695/M8CONF_w311.DAT` (52 blit, 18 scan, 6 line, 6 cmd).

Next, in order:
1. Sanity run DONE (`M8CONF_w311_bed_dcfbf6e67.BIN`): all 82 tests ran, varied pixels, one test uniform;
   tests 54, 66, 67 report timeouts - look at those three (which shapes, which wait) first. vm_6695
   AUTOEXEC.BAT restored.
1b. **Card run DONE (`M8CONF_w311_5160.BIN` vs `M8CONF_w311_bed_dcfbf6e67.BIN`): 71 of 82 shapes identical.**
   Probe gap first: read shapes (tests 3-6 blit 5210, 54 cmd 43F0, 66/67 scan 4010/4210) need the probe to
   drain and record PIX_TRANS reads after the trigger (the write log has no reads; the card stalls, 7
   timeouts). Real differences to chase: test 28 blit A211 fg07/bg05 (244 bytes), tests 58/61/62 LINEDRAW
   (2211 bg 03/05; 32/8/2 bytes - end points or last pixel). Test order = gen_m8conf.py's sorted keys.
1c. **The four real differences, read from the two .BINs (no runs; tools `m8conf_cmp.py`, `m8conf_patt.py`,
   `m8line_fit.py` in `tools/m8seq/`):**
   - Test 28: every row of a colour-pattern blit starts at PATT_INDEX on the card; the model carried the
     index across rows (+2 per row: 90 pixels mod 8). Not destination-aligned: test 76 (scan, even X,
     PATT_INDEX 5) drew entry 5. Guide p. 9-59 does not say either way.
   - Tests 61/62: the card's LINEDRAW steps as 2*min - max; the model used min - max. The guide's ERR_TERM
     rule (p. 8-39) adds -1 when start X < end X; both failing lines step X down, so that term is from the
     guide only, not measured.
   - Fixed in the fork worktree `86box_3c509b` (uncommitted, gated to the Graphics Ultra), `build_log`
     rebuilt 15:20. **Next: re-run M8CONF in `vm_6695` with that build; tests 28, 61, 62 should match the
     card and the other 71 must not move.**
   - Test 58 is NOT fixed. The card drew the line in FRGD_COLOR (FF) and drew its end point; the bed drew
     00 and no end point. The test writes neither DP_CONFIG nor LINEDRAW_OPT, so both are inherited (2211 from
     test 52, 070C from test 50; tests 59-63 write 070C and draw no end point on either machine). FRGD_MIX
     (BAE8) is written last: the model lets that pick the source for blits only, never LINEDRAW. Settle with
     a card probe: engine reset, one 8514/A CMD, then LINEDRAW; and again after a DP_CONFIG write. Why the bed
     drew 00 is not explained; a logged bed run of test 58 shows it.
2. Win95 capture: `vm_5160_now` (ATIM8.DRV, 800x600) with MACH8_WLOG=1, owner session ~10 min; run
   gen_m8conf.py over both logs into one M8CONF.DAT.
3. Card run via the CF reader (COMrade cannot move files this size reliably): copy M8CONF.COM/.DAT to
   `D:\`, owner runs `M8CONF`, read `D:\M8CONF.BIN`. Bed run the same. Diff per test; fix rows.
4. Remaining list: X wrap mod 1024 in rect/blit (5), SHADOW_SET[9:8] split and 4AE8h GE_OFFSET reset
   applied to the Mach8 (guide: Mach32 only), M8REGS read-backs a driver reads (22E8 card 0005, 12EE,
   1AEE, 36EE, 3AEE, 7AEE, A2EE, D6EE), LINEDRAW LAST_PEL_OFF 9th pixel (only if the capture shows line
   reads), TEST.COM patterns/speed.
5. Pre-PR: regress the Mach32 and 8514/A models (shared code), talk to TC1995 (blit ends vs
   `c54d36cff`), clean branch without DIAGNOSTIC commits on current master, G1-G10. Owner writes the PR;
   6695 reply draft is in the 10-09 conversation (post after the PR number exists).

## Traps met this session

- COMrade file_write of >5 KB times out part-way; use the CF reader for probes. Parts must sit inside the
  repo (bridge root). Keystroke injection stalls - have the owner type commands.
- A run that timed out still left a truncated .COM on the card; it ran and locked the machine. Hash
  before running, every time.
- pclog `*** N repeats ***` must be expanded in any log-derived replay (technique 148).
