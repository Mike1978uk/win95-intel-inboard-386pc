# Work list - in attack order

The one list. Tick an item in the same commit as its result; the detail stays in the doc each item links.
Where: **desk** = sources and code only; **B** = emulator bed, unattended; **M** = owner at the 5160
(batched into one sitting where possible); **C** = card in the reader.

Method throughout (owner's demoscene brief, `docs/mach8_graphics_vision.md`, links in
`docs/resources_and_sources.md`): the bus is the only scarce resource; measure cycles on the card; use
hardware nobody else touches; never rank a feature by whether ATI's drivers use it.

## 0. Decide

- [x] **PR gate (owner, 10-10: a gate for now, not forever).** The PR covers the Mach8 accelerator (`vid_ati_mach8.c`, `vid_8514a.c`);
      the 28800 VGA half is own-use work (sections B, C) and does not gate it. Revises the 10-07 gate.

## Next machine sitting (M, ~1 h, one card trip)

- [x] XTWLAT on the card: ~3 ms to DRQ, ~110 ms commit, 1.1 ms per 16 KB boundary (`docs/captures/2026-10-10_xtlat/`)
- [ ] RSOAK2 warm, then `REFR64` again (RSOAK2 ends at 18)
- [ ] Read and format one floppy at divisor 64
- [ ] Any card probes ready from A2 below, run in the same sitting

## A. Mach8 to the PR

- [x] **A0 One emulator build (10-10).** Mach8 branch `inboard-ext-256k-diag` now carries XT-CF stride 2, the
      trace-off default and the PIT diagnostics (`3e6c49449`..`fb6142612`, pushed); `build_log` rebuilt, 0 warnings.
      `vm_xtide_mpd` has the Mach8 EEPROM. Check boot: stride 2 up, tick logged, WRTEST 129/129 identical.
- [ ] **A1 Chapter 9 coverage (desk).** ATI extended registers into `docs/mach8_card_coverage.md`, same columns
      as chapter 8. Guide + Ferraro ch. 19 + a hand read of the model.
- [ ] **A2 Close chapter 8's unmeasured rows (desk -> M).** One probe file, one sitting:
      arithmetic mixes 10h-1Fh (does the Mach8 have them?); CMD DRAW=0 + INSIDE_SCISSOR hit test;
      PATTERN_L/H; DEST_CMP_FN masked by WRT_MASK; LINE_SYNC (DISP_STATUS bit 2); the guide-only six
      (ERR_TERM -1, CMD bit 3, blit SOURCE wrap, reads outside scissors = FFh, poly at compatible pitch,
      BRES_COUNT after CMD). Add A1's unmeasured rows.
- [ ] **A3 Model fixes from A1/A2 (desk + B).** Regress M8CONF, TEST.COM, REGR after each. Includes deciding
      how far to model the FIFO (EXT_FIFO_STATUS occupancy, INVALID_IO on overrun) - our driver relies on it.
- [ ] **A4 Appendices (desk).** EEPROM map (we have the dump), BIOS interface, CRT parameters, clocks, RAMDAC,
      checked against the model.
- [ ] **A5 Windows 95 capture into M8CONF (owner ~10 min, `vm_5160_now`, MACH8_WLOG=1).**
- [ ] **A6 Regression on other cards (B, ~2 h).** Mach32 and 8514/A, branch point `48e098d45` vs HEAD,
      `vm_6695_w2`, REGR + M8CONF + M8WRAP, diff.
- [ ] **A7 Submit.** Clean branch, G1-G10 (`repo-hygiene` section 10), owner's G10 by hand, PR text drafted for
      the owner. Then the separate CPU-fix PR (`5fce405ab`) and the 86Box#6695 reply.

## B. Bus knobs on both chips (own use; performance)

- [ ] **B1 28800 knobs from static sources (desk).** VVESA.COM disassembly, XFree86 `aticlock.c` and CRTC code,
      Ferraro ch. 19, Sutty and Blair. Table: register, bits, what it trades.
- [ ] **B2 One probe per 28800 field (M).** BEh bit 2 (128 KB window), B6h bits 2/5/6, B0h - as M8BANK did B2h.
- [ ] **B3 Mach8 knobs under load (M).** FIFO_OPT W_STATE_ENA and MAX_WAITSTATES during a FIFO-full burst
      (idle cost already = an empty port).
- [ ] **B4 Inboard write-through cache over A0000 (M).** Coherent for a packed-256 bank if flushed on bank switch.
- [ ] **B5 Combine the winners**, M8CONF + TEST.COM after each change (SNP 8A hung the 5160 once).

## C. Both halves at once - the novel part

The halves have separate memory and separate CRTCs; only one drives the monitor (ADVFUNC_CNTL bit 0).
The CPU writes the VGA side at 2.0 us/byte, the cheapest path on the card; the engine draws the 8514 side at
~31 Mpixel/s and cannot touch VGA memory. Ideas, in order of what they depend on:

- [ ] **C1 Settle the facts (desk -> M).** Is the DAC shared? What does an output switch cost - a resync on the
      monitor, or clean? Can both CRTCs run identical timing?
- [ ] **C2 Two renderers, alternate frames.** CPU builds frame N+1 in VGA memory while the engine draws frame N on
      the 8514 side; flip by the output switch. Two "GPUs" on one card.
- [ ] **C3 Split screen by raster switch.** Switch output mid-frame on LINE_SYNC / VGA status, the CRTC method of
      reenigne's 8088 MPH: VGA text or HUD on top, engine graphics below. Needs C1's timing answer.
- [ ] **C4 Pick the winner for the driver and the demo**, measured with TXTBENCH-style counts.

## D. Our driver and the demo (after A)

- [ ] **D1 TXTBENCH extensions** - bitmap, long fill, palette tests; count port writes too.
- [ ] **D2 Glyph cache in the driver (#49)**, then icon / brush / save-under caches, then the encoding bitmap path.
- [ ] **D3 3D probes (M):** cost of a polygon-fill triangle, pre-scaled column blits (RealDOOM shape).

## E. Timer, refresh, XT-CF (in flight)

- [ ] **T1 (M)** TIMERRES CPU loop at MinTimeSlice default vs 54 on the 5160 - the bed does not model bus cost
- [ ] **T1b (B)** MinTimeSlice=30 gives PIT 32768? (tests VTD's rounding rule)
- [ ] **T2 (B)** DOS box and serial at the chosen value; then the owner puts it on the card
- [ ] **T3 (B)** Cost per tick: bus cycles per tick interrupt (`docs/captures/2026-10-10_timer/`)
- [ ] **R1 (B)** Does Windows reprogram PIT channel 1? (extend the PIT log)
- [x] R4 `REFR64` in the card's AUTOEXEC.BAT (10-10, backup `AUTOEXEC.B10`)
- [ ] **X3 (desk + B + M)** Measured-wait driver, after XTWLAT's numbers
- [ ] ATI.VXD port-trapping lead - lost 10-10; recover or re-derive

## Done 10-10

- [x] Colour compare fixed and = card; command FIFO measured (16 deep, drops); refresh gain + cold soak
- [x] Tick source found (MinTimeSlice); XT-CF read waits; XTWLAT written and proven safe in the bed;
      write-path regression passes
