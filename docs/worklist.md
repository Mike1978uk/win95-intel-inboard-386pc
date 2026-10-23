# Work list - in attack order

**Now (10-21): on the real card, 1.045x ATIM8 with identical pixels in all 35 tests** (`ddf69f9`,
`docs/captures/2026-10-21_card/`). **Every change follows `docs/mach8_driver_method.md`**: guide, ATIM8, XFree86
first; bed for pixels only; the card (`tools\m8_card_cycle.ps1`) for speed. Rules: `docs/mach8_guide_rules.md`.

**Plan of attack (owner, 10-20): beat ATIM8 on all 35 M8BENCH tests, with less bus traffic.** ATIM8 keeps nothing
on the card and re-uploads every draw; we have ~424 KB off-screen and an engine that copies on-card. Thesis: put
it on the card once, draw it with the engine. Scoreboard: M8BENCH on the real card against `card_ati1,card_ati2`
(the bed misprices port writes; its speeds do not rank work). Loop: the method doc. Order:
1. **Bitmaps in video memory as device bitmaps** (DDK `DeviceBitmap`/`SelectBitmap`/`BitmapBits`, `RC_DEVBITS`):
   GDI routes writes through us, so no content hashing. bmp_*, mem_to_mem, draw_icon. Read the DDK first.
   Replaces the 32-bit hashing item if it holds. One off-screen allocator shared by every cache and DirectDraw.
2. **Text** - done so far: string-keyed replay, 16-bit hashes, off-left strings cached (text_sys 87 -> 651/s at 1024x768, ATI 1044). Left: **glyph cache keyed by font + character**, mono expansion from video memory (XFree86, 51 us/glyph). text_*.
3. **PATT_DATA brushes + brush cache** (fill_hatch, drag_frame). [x] lines_dash (`565ec61`: ATIM8's pattern pens, card 496/s vs ATI 492, pixels = ATI). [x] fill_hatch (card 316/s vs ATI 298, pixels = ATI; `docs/captures/2026-10-22_card`).
4. **Shapes on the engine:** polygon fill (cmd 5) for polygon/ellipse, short strokes, floodfill one row read per row.
5. **Source colour compare** (92EE/EAEE): icons and sprites in one pass; DirectDraw colour keys.
6. **Bus discipline** - [x] shadow the setup registers (`7f71893`: colours, masks, scissors only - the data path cannot be cached, guide 7-1). Next: **FIFO slot counting** (XFree86 `WaitQueue(n)`: wait for the slots needed, not an empty FIFO), batched set-up (one set-up, many spans), `-oi` inline port writes, no idle polls, run-time generated stubs.
7. **DirectDraw HAL on the Mach8:** CRT_OFFSET flips timed by the A8/A9 raster counter, colour-key blits,
   surfaces off-screen; the VGA half (Mode X) after.
8. **3D:** engine spans/fills into an off-screen page, flipped (raycaster projects as the model).
9. **More colours (experimental):** palette changes per raster band (A8/A9 counter), temporal dithering by
   alternating two flipped pages. 6-bit DAC, 256 at a time; perceived colours beyond it.

**First (10-17 night): match ATIM8 where it wins, from its own code** (`docs/atim8_disassembly.md`, M8BENCH
`docs/captures/2026-10-16_atim8_tap/m8bench_1017_first.txt`): engine paths at parity; every pixel path 5-50x slower.
(1) uploads: one extended blit (DP_CONFIG 5211) + `rep outsw` per row, no status polls - ours runs C loops, hashing,
run encoding; (2) read-back: one DATA_READY poll then `rep insw` - ours polls per word; (3) hatched/mono/colour
brushes and dashed lines on the pattern registers (PATT_DATA) - ours fall back; (4) text: ~12 port ops per glyph,
FIFO slot budget, no cache - our string cache wins only once its hash is cheap (32-bit asm). Then re-run M8BENCH.
Ours alone after that: caches in VRAM (ATI has none), colour compare, polygon fill (cmd 5), short strokes - ATI
uses none. Open: polygon/ellipse fault (design doc "M8BENCH, first results").

**Next (owner, 10-17): finish the base driver, prove it on the 5160, then the new league.** L and D8 are done in
the bed. Base: (1) bitmap cache LRU + room per mode, (2) engine lines (last XFree86 gap), (3) colour-compare
transparency for both caches, (4) first timed 5160 run vs ATIM8 (the "base done" gate), (5) D8c 16-colour modes.
Then the D9 experimental block, in order of what (4) shows the bus can afford; DirectDraw (D5) on the Mach8
first. The XT-CF measured-wait driver
(E, 10-10) stays queued behind it.

The one list. Tick an item in the same commit as its result; the detail stays in the doc each item links.
Where: **desk** = sources and code only; **B** = emulator bed, unattended; **M** = owner at the 5160
(batched into one sitting where possible); **C** = card in the reader.

Method throughout (owner's demoscene brief, `docs/mach8_graphics_vision.md`, links in
`docs/resources_and_sources.md`): the bus is the only scarce resource; measure cycles on the card; use
hardware nobody else touches; never rank a feature by whether ATI's drivers use it.

Driver ethos (owner, 10-17): the whole card is the point; velocity9x/vmdisp9x are only the frame. Take the best
part of every driver (ATI, XFree86, velocity9x, the DDK samples) and fit it to this card; build custom where none
covers it. A win is less CPU, bus traffic or code, not only less time - so every fallback the engine can take
over is removed as a decision, even where the tally shows little time, because each one is bus noise for disk,
network and sound.

## 0. Decide

- [x] **PR gate (owner, 10-10: a gate for now, not forever).** The PR covers the Mach8 accelerator (`vid_ati_mach8.c`, `vid_8514a.c`);
      the 28800 VGA half is own-use work (sections B, C) and does not gate it. Revises the 10-07 gate.

## Next machine sitting (M, ~1 h, one card trip)

- [x] XTWLAT on the card: ~3 ms to DRQ, ~110 ms commit, 1.1 ms per 16 KB boundary (`docs/captures/2026-10-10_xtlat/`)
- [x] RSOAK2 warm (10-10): XMS + conventional clean at 64; `REFR64` re-applied
- [x] RSOAK2 again from a clean boot with NO COMrade loaded (done 10-11, `5160_tuned_config.md`) - the planar 64 KB check was confounded by 3 resident copies
- [x] Read and format one floppy at divisor 64 (done 10-11)
- [ ] Reboot clean first (F8, command prompt) and load COMrade ONCE - re-running `comrade` stacks resident copies
- [x] `M8STAT` run 10-10 (LINE_SYNC live). ZWS.SCR full copy (5,009 B) staged on the card 10-10, 22:31
- [x] ZWS on the card 10-10: AB wait-state bits change nothing (negative result, `docs/captures/2026-10-10_zws/`). Was: the B2 wait-state timing: `python tools/gen_zws_probe.py > ZWS.SCR`, then
      `DEBUG < ZWS.SCR > ZWS.OUT` (proven in the bed, restores AB)
- [ ] Any card probes ready from A2 below, run in the same sitting

## A. Mach8 to the PR

- [x] **A0 One emulator build (10-10).** Mach8 branch `inboard-ext-256k-diag` now carries XT-CF stride 2, the
      trace-off default and the PIT diagnostics (`3e6c49449`..`fb6142612`, pushed); `build_log` rebuilt, 0 warnings.
      `vm_xtide_mpd` has the Mach8 EEPROM. Check boot: stride 2 up, tick logged, WRTEST 129/129 identical.
- [ ] **A1 Chapter 9 coverage (desk) - AFTER the PR (owner, 10-11).** ATI extended registers into `docs/mach8_card_coverage.md`, same columns
      as chapter 8. Guide + Ferraro ch. 19 + a hand read of the model.
- [x] **A2 Close chapter 8's unmeasured rows (desk -> M). Done 10-11 by M8A2/M8PF/M8REGS/M8STAT (checked 10-12);
      left, not driver-relevant: CMD opcode 7, CMD bit 3, poly fill at compatible pitch, BRES_COUNT after CMD,
      colour compares under a partial WRT_MASK. Model gap still open (A3): blit source past x=1023.** One probe file, one sitting:
      ~~arithmetic mixes~~ (done 10-10: all 32 present, card = model); CMD DRAW=0 + INSIDE_SCISSOR hit test;
      PATTERN_L/H; DEST_CMP_FN masked by WRT_MASK; LINE_SYNC (DISP_STATUS bit 2); the guide-only six
      (ERR_TERM -1, CMD bit 3, blit SOURCE wrap, reads outside scissors = FFh, poly at compatible pitch,
      BRES_COUNT after CMD). Add A1's unmeasured rows.
- [ ] **A3 Model fixes from A1/A2 (desk + B).** Found 10-10: (1) ~~DEST_CMP_FN ignored on 8514/A commands~~ fixed `c12ab587c`; (2) ~~SCAN_TO_X fill flag~~ fixed `a52d2f2ce`; (4) LINE_SYNC (02E8 bit 2) absent - card toggles it (M8STAT); (5) **A6: HEAD exits on a Mach32 at M8FG6** - the branch point runs every probe; repeated (`head_mach32_r2`). Last write before the exit: engine reset, scissors, then an 8514/A fill CMD 40F3h at y=025Ah, MAJ_AXIS 1Fh. **Bisected 10-10: first bad = `5a5079fbc` "DIAGNOSTIC: trace TEST.COM's RAM Addressing stage"** - a diagnostic hook, not a model change (`bisect_log.txt`). **Fixed 10-10: reverted on the working branch (`3667abf59`)**; it read `dev->vram`, absent on a Mach32. `tools/a6_finish.sh` (left running) writes `base_8514a` and `head_mach32_fixed` under docs/captures/2026-10-10_a6/: **HEAD 8514/A run crawled** (owner stopped it): M8CONF logs an `[M8P]` line per 1x1 fill on the plain 8514/A path, minutes became hours - find and gate that trace, then rerun. Also next session, check head_mach32_fixed has DONE.TXT and cmp every BIN against base_mach32, and head_8514a against base_8514a; (3) ~~CONFIG_STATUS_1 high byte~~ fixed `a7b84dbe4`. Found 10-11 by M8A2 (`docs/captures/2026-10-11_m8probes/`): (6) fixed-pattern fill draws one row of four; (7) reads outside the scissors must return FFh; (8) blit source past x=1023 continues into the next row; (9) SUBSYS_STAT monitor-ID bit. (4) LINE_SYNC done `5897f6a48` (rate open).
      Previously listed: Regress M8CONF, TEST.COM, REGR after each. Includes deciding
      how far to model the FIFO (EXT_FIFO_STATUS occupancy, INVALID_IO on overrun) - our driver relies on it.
- [ ] **A4 Appendices (desk) - AFTER the PR (owner, 10-11).** EEPROM map (we have the dump), BIOS interface, CRT parameters, clocks, RAMDAC,
      checked against the model.
- [x] **A5 Windows 95 capture into M8CONF (taken 10-12, `docs/captures/2026-10-12_a5_win95/`; feeding M8CONF still to do) (owner ~10 min, `vm_5160_now`, MACH8_WLOG=1).**
- [x] **A6 Regression on other cards (B).** Rerun 10-11 after gating `27e57fcfe`/`89942be5e`: Mach32 M8CONF 82/82 = branch point (`head_mach32_r3`). 8514/A (`head_8514a_r3`): 33/35, only M8CMP8 and M8MIX differ (the documented fixes). A6 complete. First run 10-11 (`docs/captures/2026-10-10_a6/`, HEAD `3667abf59`+IDE diag, `MACH8_COUNT=1`): Mach32 runs every probe (crash fixed). vs branch point, 8514/A 33/35 same, Mach32 32/35: M8CMP8 = the colour-compare fix (HEAD = the card byte for byte); M8MIX mix 1Fh saturates per guide 8-24; **M8CONF on the Mach32, tests 3-6 one fewer read word and test 54 byte order - Graphics Ultra measurements now applied to the Mach32 with no Mach32 evidence: gate them to the Graphics Ultra before the PR, or show the Mach32 guide agrees.** 8514/A M8CONF skipped (>30 min, owner 10-11). Was: Mach32 and 8514/A, branch point `48e098d45` vs HEAD,
      `vm_6695_w2`, REGR + M8CONF + M8WRAP, diff.
- [x] **A7 Merged 10-11: 86Box PR #8289** (OBattler, merge `3a0efe82b`) (`942e2de86`, 6 files); replies posted on #6695 and #4898. Follow-up: INMOS IMSG176 DAC model. Was: 10-11: fork `mach8-pr` = `6e9978efd`, one commit on upstream `1741108a7`, +1708/-410 in 4 files; diagnostics stripped (`mach8-pr-prep` `91afd001f`), comments made upstream-neutral; G1 pass; G7 applies and builds (no warnings in our files); G9 Mach32 35/35 and Graphics Ultra M8A2/M8XMEM/M8LIN identical on that build (`prup_mach32`). Left: G5 comment read, G2-G4, G6, G8, G10 (owner), PR text. Clean branch, G1-G10 (`repo-hygiene` section 10), owner's G10 by hand, PR text drafted for
      the owner. ~~Then the separate CPU-fix PR (`5fce405ab`)~~ - not needed, upstream `acc82a87f` (Hinoserm) fixed it; 86Box#6695 reply posted 10-11.

- [x] **A7b Merged 10-12: 86Box PR #8294** (OBattler), default EEPROM at 60 Hz.
- [x] **A11 Merged 10-15: 86Box PR #8339** (OBattler, merge `0ea018811`) (TC1995's Win 3.x cursor regression from #8289). Cause: ATI's
      ULTRA.DRV (1992) uploads cursor masks with DP_CONFIG then FRGD_MIX; #8289 moved the mono source to
      PIX_CNTL too. Fix keeps only the foreground switch. MACHW3 never affected. Tests and gates:
      `docs/mach8_ultra_drv_cursor_2026_10_15.md`. Owner told TC1995.
- [x] **A12 Merged 10-16: 86Box PR #8351** (OBattler, merge `c43f3e2d0`) (TC1995: Win 3.1 8514.DRV File Manager glitch from #8289).
      Command 4 width/height swapped; card probe M8NIB2 = fixed build byte for byte. Fork `mach8-nibble-axes`
      `3b23c04b2`. Details: `docs/mach8_cmd4_axes_2026_10_16.md`. Owner told TC1995. README row added.
- [x] **A13 Merged 10-18: 86Box PR #8394** (merge `0f7cf1381`) (Mamoru/OBattler, Discord 10-18): Win 3.0 Solitaire hangs on the Mach8 since #8289.** One-pixel
      line read: GE_STAT read-data-available cleared before the host took the word. Fork `mach8-dataready`
      `6ca622ca1`. Details and PR draft: `docs/mach8_data_available_2026_10_18.md`. 8514.DRV's caption-button
      row is the driver, not the model (same doc).
- [ ] **A8 (Thursday, owner) Siblings of #8289 (TC1995, 86Box Discord 10-12).** The Mach8 fixes should
      apply to the 8514/ULTRA add-on (same chip; `ATI_8514A_ULTRA`) and likely the Mach32. #8289 added 118
      lines gated on `ATI_GRAPHICS_ULTRA`. List each gated fix (fix, measured on our card, siblings it
      should cover); widen engine fixes to the add-on AND the Mach32 at 8 bpp - TC1995 12:04: "think of the
      mach32 as a 8bpp+ mach8 in the acceleration"; only its >8 bpp paths need separate thought; regress all four cards
      (repo-hygiene G11). Also boot the add-on on PR #8294's build.
- [ ] **A10 Host-side harness for the 8514 family (skiretic, 86Box Discord 10-12).** Upstream
      `tests/video/rage128/` (PR #8292): `reg-harness/` calls device handlers with no VM; `replay-harness/`
      replays a recorded command stream against the device sources in seconds. Build the Mach8 equivalent
      (reuse `host_stubs.c`), feed it MACH8_WLOG captures and the card probes (M8CONF, M8A2, M8VGA), and
      regress all four 8514-family cards per build - makes A8 cheap.
      Read 10-12: reuse the method and host_stubs.c (pclog, config, RAM, an io_sethandler that records the
      device handlers); our MACH8_WLOG logs are already a capture tap. Not useful: the Rage 128 vectors, JIT,
      GPU, bus-master/PCI stubs. Main cost: his VGA core is an empty shell; ours needs more of svga_t.
      **Step 1 ready (10-16):** branch `mach8-tap` in `86box_bisect/` (pushed to the fork, `mike/mach8-tap`) = upstream master + #8351 + a
      tap (`e8a50f66a`, M8W writes and M8R word reads, `MACH8_WLOG=1`); built and smoke-tested (M8DPM: 1,220 writes, 5,539
      reads, output still = card; `docs/captures/2026-10-10_a6/tap_smoke/`).
      **Order (10-16, for D8):** (1) the tap exists - `MACH8_WLOG=1` (fork diagnostic commits `1c58617df`, `778f9f180`,
      on `mike/mach8-pr-prep` and `mike/inboard-ext-256k-diag`): cherry-pick onto the D8 test build, run the driver in a bed
      with a marker write between operations (e.g. SCRATCH_PAD_1, 56EEh), count writes / reads / PIX_TRANS bytes per
      operation - D8's bus-cost numbers with no replay at all. (2) Replay later: capture -> host program linking
      `vid_8514a.c` + `vid_ati_mach8.c` with skiretic's `host_stubs.c`, VRAM dump and diff per driver or emulator change.
      (3) Baselines: the same tap over 8514.DRV (`vm_mach8_8514drv`) and ATIM8 (`vm_5160_now` via `ATIM8.REG`).
- [x] **A9 M8VGA second bed pass** (done 10-12: mode 63h 164 -> 31 differences; left = VGA register read-back must be masked to implemented bits, model fix for Thursday) on the card's own EEPROM, to read mode 63h's 164 differences
      (`docs/captures/2026-10-12_m8vga/`). Model fixes found 10-12: ATI indexes 80h-9Fh and C0h-FFh read
      FFh on the card (model 00); BA 01 vs 00.

## B. Bus knobs on both chips (own use; performance)

- [ ] **B1 28800 knobs from static sources (desk).** 10-16: census + VVESA done (L1, L2); rest is L3. Start from the 10 extended registers where card and bed differ (`mach8_card_coverage.md`). VVESA.COM disassembly, XFree86 `aticlock.c` and CRTC code,
      Ferraro ch. 19, Sutty and Blair. Table: register, bits, what it trades.
- [ ] **B2 One probe per 28800 field (M).** First: **AB bits 0/5 (zero-wait-state video writes), off on our card** - M8BUS write timing with them set, then a stability soak. ~~BEh bit 2 (128 KB window)~~ (it is the EGA DIP-switch select; the 128 KB map is VGA GR06; M8LIN 10-11: no 128 KB linear window, B000 mirrors A000; B6h bit 2 = 256-colour banking), B6h bits 2/5/6, B0h - as M8BANK did B2h.
- [ ] **B3 Mach8 knobs under load (M).** FIFO_OPT W_STATE_ENA and MAX_WAITSTATES during a FIFO-full burst
      (idle cost already = an empty port).
- [ ] **B4 Inboard write-through cache over A0000 (M).** Coherent for a packed-256 bank if flushed on bank switch.
- [ ] **B5 Combine the winners**, M8CONF + TEST.COM after each change (SNP 8A hung the 5160 once).

## C. Both halves at once - the novel part

The halves have separate memory and separate CRTCs; only one drives the monitor (ADVFUNC_CNTL bit 0).
The CPU writes the VGA side at 2.0 us/byte, the cheapest path on the card; the engine draws the 8514 side at
~31 Mpixel/s and cannot touch VGA memory. Ideas, in order of what they depend on:

- [ ] **C1 Settle the facts (desk -> M).** 10-11 (`docs/captures/2026-10-11_m8probes/`): memories do not cross (M8XMEM); halves not phase locked; M8FLIP v1 never reached the monitor (CLOCK_SEL 0 = pass-through) - v2 needs the 640x480 clock index from the EEPROM (A4). Is the DAC shared? What does an output switch cost - a resync on the
      monitor, or clean? Can both CRTCs run identical timing? Can the CPU read and write VGA memory while the Mach8
      drives the output (owner, 10-16)? If so, the 512 KB is a parking place that saves system RAM - not a cache for
      the 8514 desktop (the engine cannot read it; data parked there crosses the bus twice).
- [ ] ~~**C2 Two renderers, alternate frames.**~~ **Not viable as designed (10-16, L4c):** every switch between the halves
      blanks the monitor for over a second (owner: black, then white, no signal loss); the halves run different timing and are
      not phase locked. Matched timing untested, but the sync phase would still jump. Was: CPU builds frame N+1 in VGA memory while the engine draws frame N on
      the 8514 side; flip by the output switch. Two "GPUs" on one card.
- [ ] ~~**C3 Split screen by raster switch.**~~ **Not viable (10-16):** same re-sync gap at each switch. Was: Switch output mid-frame on LINE_SYNC / VGA status, the CRTC method of
      reenigne's 8088 MPH: VGA text or HUD on top, engine graphics below. Needs C1's timing answer.
- [ ] **C4 Pick the winner for the driver and the demo**, measured with TXTBENCH-style counts.

- [ ] **B6 (parked idea) A custom card ROM.** EEPROM is 128 B of settings, no code, full. VGA BIOS image
      (`roms/video/mach8/BIOS.BIN`, 32 KB) has 249 B free; the older image is a 64 KB chip with the 32 KB twice, and the
      28800 has ROM page registers (A4/A5, card 54/76) - so a larger chip's spare half may be reachable. Needs an EPROM
      programmer; gains are boot/DOS-only, which a RAM-resident extension gets with no risk. Confirm the paging first.

## L. Card levers first (owner, 10-16) - before D8

Find every switch on both chips, including silicon ATI's drivers never used; each kept where a measurement shows a gain.
Inventory and results: `docs/mach8_levers.md` (one place; this list only orders the work).

- [x] **L1 Port census of ATI's software (desk, 10-16).** 84 binaries, `tools/ati_port_census.py`. Source colour compare
      (92EE/EAEE) and short strokes: TEST.COM only. 17 28800 indices no ATI binary touches.
- [x] **L2 VVESA.COM disassembly (desk, 10-16).** 28800 split read/write banks (B2 + BE bit 3), AE bank high bits,
      HiColor DAC probe (A7), clock bit (BB). Static reading; L4 measures it.
- [ ] **L3 Desk.** Done 10-16: VVESA 2.21 vs 2.22 (B2 bit 0 = read-bank top bit; ROM call before clearing memory); the
      **ROM's API** (far-call entry points 64h/68h, used by TEST.COM and VVESA, no driver); the ROM's 28800 use (EEPROM
      bit-banged via B3/B7, clock select B9/BE + Misc Output, revision in AA); ATIM8/ATIM32 (no short strokes anywhere;
      314 FIFO-status poll sites in ATIM8 - pace by the measured 16-deep FIFO instead). Left: EEPROM byte map vs
      `docs/rom_and_eeprom_dumps.md`; ROM mode tables bit by bit (B0/B1/B4/B5/BA/BB/BD/BF); ROM API arguments (72DDh). EEPROM records
      decoded 10-16: one Mach8 record (2Eh), three 28800 records keyed by VGA mode (`mach8_levers.md`).
- [ ] **L4 Card sitting (M).** Done 10-16 (`docs/mach8_levers.md`): (a) line counter A9h = raster clock, 11.5 us/poll,
      ~2-line steps; (b) split banking works (BEh bit 3) but **no time gain** - bus-bound; (c) DP_CONFIG mono expansion
      **= bed byte for byte**. (d) EXT_SHORT_STROKE: **card does not follow the guide** (one diagonal per write) -
      follow-up M8SSV3; (e) unknown bits: A0 0-3, AD 4, BC hold, AF reads 0, no visible effect, no hang.
- [x] **L4c C1 answered 10-16:** an output switch costs a >1 s blank (M8FLIP4, owner watched; ENA_DISPLAY reset = black).
      Both halves stay usable without switching the output (CPU stages data in VGA memory while the engine draws). Was: What the output switch between the halves costs (clean, or a
      monitor resync) and whether both CRTCs can run identical timing - it gates C2 (two renderers, frame N+1 built while
      N shows). 10-16: M8SHOW shows both stripe sets (ADVFUNC alone works); M8FLIP3 black after an INT 10h
      mode 12h - next M8FLIP4 (text / mode 12h / mode 12h + DISP_CNTL on).
- [x] **L5 Folded 10-16:** "Lever decisions for D8" in `docs/mach8_driver_design.md` - per-job choice, evidence, what is
      ruled out. Was: Fold results into `docs/mach8_driver_design.md` - which levers the driver uses, and for what.



**Toolchain spike (10-12):** `drivers/mach8_display/spike/` - DDK FRAMEBUF with MASM 6.11 + Open Watcom; resources and
links cleanly to an NE driver (one source rule: declare imports outside segments). Sources: xga-win9x (permission given), velocity9x,
skiretic's 86Box test tools, the XFree86 3.3.6 Mach8 server (`references/xfree86_336_mach8/`: accelerated Mach8
under Linux, off-screen font cache in bit-plane pools - the D2 glyph-cache template).

**Build order (owner, 2026-10-12):** Windows 95 first. Step 1: our own drawing driver (replaces ATIM8.DRV, uses
the whole engine) on top of ATI.VXD - it costs nothing while Windows owns the card (`docs/ati_vxd_traps.md`).
Step 2: our own VxD, to save the registers ATI.VXD does not know (DEST_CMP_FN, BOUNDS) across DOS boxes.


- [x] **D0 Passed 10-16: our FRAMEBUF build loads and draws in Windows 95** (`vm_5160_now`, 640x480x256 on the 28800 half;
      BOOTLOG `LoadSuccess = owfb.drv`, VFLATD init success). Switch by registry, not SYSTEM.INI: `drivers/mach8_display/bed/OWFB.REG`
      (`ATIM8.REG` reverts; from DOS `REGEDIT C:\ATIM8.REG`). SYSTEM.INI stays `display.drv=pnpdrvr.drv`. Was: Our FRAMEBUF build (`OWFB.DRV`) and Microsoft's own
      FRAMEBUF.DRV both fail on `vm_5160_now` ("problem with your display settings", 1024x768 and 640x480),
      so the control failed too: the sample cannot drive this card, and our build is not yet shown to load.
      Next: the same swap on a bed where Windows itself installs FRAMEBUF.DRV, or start the Mach8 skeleton.
      10-16 cause (static read): FRAMEBUF takes its screen selector from VFLATD.VXD (`GetScreenSelector`); the
      card install has none (not in SYSTEM.INI, not in VMM32.VXD), so PhysicalEnable fails for any FRAMEBUF build.
      Detection passes (BIOS signature, product code '3'). The sample drives the 28800 half only: 512 KB, so
      640x480/800x600 x 256; its 1024x768 needs 1 MB. Next: VFLATD.VXD (retail, golden install) + `device=` line, boot.
- [ ] **D1 TXTBENCH extensions** - bitmap, long fill, palette tests; count port writes too.
- [ ] **D2 Glyph cache in the driver (#49)**, then icon / brush / save-under caches, then the encoding bitmap path.
- [ ] **D3 3D probes (M):** cost of a polygon-fill triangle, pre-scaled column blits (RealDOOM shape).
      Vectors (owner, 10-16 - the Vectrex model): 8514/A short strokes (9EE8, card-proven M8ROW3) are a hardware
      vector list - one byte = direction (8) + length (0-15), two per word; full lines ~5-6 writes. Estimate ~800-900
      lines per 33 ms frame; the 486 transforms, the engine draws. Erase by fill, XOR redraw, or 640x480 page flip
      (CRT_OFFSET, timed by the A9h raster counter). Re-probe EXT_SHORT_STROKE (C6EEh) with CMD set per the guide -
      a denser encoding if it works. Read M8POLY's result first (`tools/m8seq/M8POLY.ASM`).
- [ ] **D4 More than 256 colours (owner, 10-11).** Mach8 is 8 bpp only, so: (1) dithered fills from PATTERN_L/H /
      PATT_DATA - same bus cost as a plain fill; probe with A2; (2) palette splits per band on vblank / LINE_SYNC,
      ~1 colour change per line (estimate: ~23 us per entry vs ~32 us line); (3) error diffusion for still images on
      the 486; (4) ~~8-bit DAC~~ - the RAMDAC is an INMOS IMSG176 (6-bit), no 8-bit mode (photo, 10-11).
- [ ] **D5 DirectDraw - on the Mach8 first, the VGA half second (owner, 10-17: challenge "VGA half only").** Only
      Lock needs CPU access to pixels; Blt, colour-keyed Blt, fills and Flip are what the Mach8 engine does. Two back
      ends, measured against each other on the 5160:
      (a) **Mach8 HAL**, ahead of the VGA half on Blt, colour-keyed Blt (colour compare 92EE/EAEE = DDCKEY_SRCBLT/
      DESTBLT, used by no ATI driver) and Flip (CRT_OFFSET 2AEE/2EEE at VBLANK, SUBSYS_STAT). The back page is drawn
      through GE_OFFSET/GE_PITCH (coverage doc rows 194-207). With CRT_PITCH 100 (800 px): 800x600 double-buffers (2 x
      480,000 B in 1 MB); 640x480 at pitch 640 triple-buffers; 1024x768 is one page + 256 KB for sprites, tear-free by
      blits timed to VBLANK. Lock: DDHAL lock may hand back a system-RAM shadow of only the locked rectangle, read back
      before and uploaded as runs on Unlock - fine for occasional locks, poor for games that write every pixel.
      Desk first: Win95 DDK DDHAL lock/flip/blt callbacks; whether our GDI driver can leave pitch 1024 while DirectDraw
      is exclusive. One card probe: CRT_PITCH 100 + a CRT_OFFSET flip, no tearing at VBLANK.
      (b) **VGA half** (Andrew, #49 10-06: memory writes beat port I/O and may not flush L1 - unmeasured on the
      Inboard): for Lock-heavy games. M8LIN (10-11): one 64 KB bank, no 128 KB window - 320x200x8 fits unbanked,
      320x240 needs a bank switch; switching halves blanks > 1 s (L4c), so only on entering exclusive mode.
      HAL frame from vmhal9x (`references/jhrobotics/`, MIT) for both.
      **Both ship as options** (owner, 10-17): a registry/Display Properties choice, default the Mach8 HAL; expected
      ranking (unmeasured): Mach8 for Blt/Flip games, VGA half for Lock-heavy ones.
      Memory decides flipping: Mach8 1 MB, VGA half 512 KB - the VGA half cannot double-buffer even 640x480x256
      (2 x 307,200 B); it flips at 640x400 and below (2 x 256,000 B fits).

- [ ] **D6 VGA-half framebuffer driver on VFLATD (owner, 10-16).** The 28800's 512 KB through A000 is the
      cheapest CPU path on the card; VFLATD banks it into a flat surface. A fallback driver (640x480/800x600 x 256)
      and the base for D5. The Mach8's own 1 MB has no CPU window, so VFLATD cannot serve the accelerated driver.
- [ ] ~~**D7 Our own "VFLATD" for the Mach8: a RAM shadow buffer.**~~ Superseded 10-16 (owner: least RAM): a counted
      read-back of only the bounding rectangle instead (`mach8_driver_design.md`). Was: DIB-engine punts draw into system RAM; the
      driver uploads dirty rectangles by host-data blit (~1.9 us/B). Costs 300 KB (640x480) to 768 KB (1024x768).
      Gate: measure how often the driver punts (D1) first. A page-fault mapper over PIX_TRANS is ruled out (a port
      transfer per access).
- [ ] **D8 First Mach8 build (after L).** velocity9x framework (builds here, 10-16), `NOT_FRAMEBUFFER`, Mach8 800x600;
      fill, copy and text background on the engine; the counted read-back fallback for the rest. Needs A10 (replay
      harness) for per-operation port and PIX_TRANS counts. Design: `docs/mach8_driver_design.md`.
      **D8a passes in the bed (10-16):** `drivers/mach8_display/src/`, desktop/Explorer/Solitaire correct at 800x600;
      engine fill + copy, read-back fallback for the rest. Bus tally and D8b order in the design doc. Not yet on the 5160.
      **D8b done in the bed (10-16):** text, cursor, pattern ROPs (via the DIB Engine's own 8x8 brush rendering), source
      clip, SaveScreenBitmap (guarded; an intermittent overwrite of its rectangle is under watch), DOS switching.
      Next: bitmap uploads as runs, then the first timed run on the 5160 against ATIM8.
- **Working method for D8/D9 (owner, 10-17, token budget):** one `tools/m8_cycle.ps1 -Message "..."` per iteration
  (builds both variants with all checks, puts the trace build in the bed image, commits, pushes; one line out);
  two or three independent changes per bed run with the D8 regression list; read the tally summary and marker
  counts first, raw log lines only on a fault; screenshots only of reported faults, zoomed crops.
  Claude prompts the owner to switch to a cheaper model (`/model`) for mechanical steps (regression run + tally,
  docs, a fix already worked out); never for diagnosis.
- [ ] **D9 New league (owner, 10-16: "leave them all standing" - ATI's drivers and XFree86 both).** Already ours and in
      neither: uploads as runs, double-draw coverage, per-call bus accounting. Parity table against XFree86: design doc
      "Against the XFree86 Mach8 driver" - only engine lines are missing. Order: (1) glyph cache + colour-compare
      transparency (92EE/EAEE, used by no driver) - text faster than ATIM8 (121 us/glyph) and XFree86's cache (51 us);
      **cache done 10-16 as strings in bit-planes (83% hits); the 51 us target is unproven until (4)**; transparency open;
      (2) engine lines (Bresenham, short strokes); (3) bitmap/icon cache in off-screen memory, drawn transparently in one
      copy - **cache done 10-17 (95% hits), LRU eviction + more room next; transparent draw open**; (4) timed 5160 run
      vs ATIM8 to prove the lead; then the experimental block - raster-timed drawing (A9h
      line counter), >256 colours (D4), DirectDraw on the VGA half (D5/D6), vectors and 3D (D3).
      **Slow-path migration (owner, 10-17: "once tested, migrate them"):** done 10-17 in the first D9 build -
      solid polylines (LINEDRAW), Pixel sets (1x1 fill), screen-to-memory saves kept on the card (proxies). Left:
      styled pens (PATT_DATA as the style mask; GDI's style phase must match), polyline points off the 0-1023 grid
      (card coordinate range unprobed), Pixel gets, ScanLR, StretchBlt, the DibToDevice edges. Each after its test.
- [ ] **D8c Parity with ATIM8's modes (owner, 10-16: "equal and better than ATI", for the purists).** 256 colours at
      640x480, 800x600, 1024x768 (built 10-16, `9bd77c2`, in test). 16 colours at 800x600 and 1024x768 as ATIM8 offers
      them (640x480x16 is Windows' vga.drv on the VGA half, unchanged): Mach8 4-bit pixel mode, DIB Engine at 4 bpp,
      host transfers 4 pixels per word; fills, copies and mono expansion carry over. Bonus: half the bytes per upload.
      **D8 regression list (owner, 10-16), run on every driver build, screenshots of any difference vs ATIM8:**
      desktop icons + labels; Start menu; Explorer (folder icons, selection focus rectangle); Notepad "save changes?"
      (default-button frame); shutdown dialog (radio buttons, grey-out); window drag outline; Solitaire castle deck
      (bats inside the card, corner flash on click); Paintbrush flood fill (86Box#6695, the Mach8 bug class);
      Win 3.1-style File Manager selection (TC1995, #8351 class); cursor at all four edges, hourglass, I-beam;
      a clean shutdown (no blue screen). Then `tools/m8_fallback_tally.py` on the log.

## S. Sound on the same bus (owner, 2026-10-10: 8088 MPH / Area 5150 / FastDoom spirit, Daniel_K's licence to fix what the vendor didn't)

The SB Pro (220h, IRQ 5, DMA 1) moves every sample byte over the same 8-bit bus by DMA, and each buffer-end IRQ costs
like a timer tick (~170 us measured for #45). Ideas, cheapest first - measure before building, as everywhere else:

- [ ] **S1 Measure** what audio costs the bus now: DMA bytes/s per format, IRQs/s per buffer size, and the CPU loop
      (TIMERRES) with a sound playing vs silent.
- [ ] **S2 Fewer IRQs:** large auto-init DMA buffers (one IRQ per half-buffer), sized from S1. Buffer must stay below
      1 MB (8237 reach; `DMABufferIn1MB`).
- [ ] **S3 Fewer bytes - the card as decoder:** the SB's DSP decodes Creative ADPCM (4-, 2.6- and 2-bit) in hardware.
      Same principle as the Mach8 work - send intent, not samples: 4-bit ADPCM halves the DMA traffic of 8-bit PCM.
      Check the SB Pro's DSP version supports auto-init ADPCM, and the quality.
- [ ] **S4 Mix on the 486** into one stream (FastDoom's approach) rather than several; the CPU has cycles, the bus has none.
- [ ] **S5 The demoscene end:** cycle-exact PC-speaker / DAC tricks are a later, separate study.

## E. Timer, refresh, XT-CF (in flight)

- [x] **T1 (M)** (done 10-10/11: +6% idle CPU, 36 MB copy 0.7% faster, `5160_tuned_config.md`) TIMERRES CPU loop at MinTimeSlice default vs 54 on the 5160 - the bed does not model bus cost
- [x] **T1b (B)** MinTimeSlice=30 gives PIT 32768 (27.5 ms): VTD's rounding rule holds
- [ ] **T2 (B)** DOS box and serial at the chosen value; then the owner puts it on the card
- [ ] **T3 (B)** Cost per tick: bus cycles per tick interrupt (`docs/captures/2026-10-10_timer/`)
- [x] **R1 (B)** Windows does not touch PIT channel 1: divisor 64 survives into Windows (bed)
- [x] R4 `REFR64` in the card's AUTOEXEC.BAT (10-10, backup `AUTOEXEC.B10`)
- [x] **X3 (desk + B + M)** Measured-wait driver: `a67fefc4` passes bed + 5160 (10-11, `xtcf_measured_wait.md`); not yet in `dist/`; next: copy time and CPU returned
- [x] ATI.VXD port-trapping lead - recovered 10-12: `docs/ati_vxd_traps.md` (handlers on 01CE/02E8/4AE8/4AEE; 65 ports trapped as a group)

## Done 10-10

- [x] Colour compare fixed and = card; command FIFO measured (16 deep, drops); refresh gain + cold soak
- [x] Tick source found (MinTimeSlice); XT-CF read waits; XTWLAT written and proven safe in the bed;
      write-path regression passes

## H. Andrew's #50 follow-ups (10-23)

- [ ] **Mach8: queue writes per call.** Each port access flushes L1, so decide every write first and send from one short loop. Fill done (`55d5624`, 1.288x ATI); copy, spans, lines next. #50
- [ ] **Upstream timing and correctness fixes to 86Box** once the driver work settles. #50
- [ ] **WINDISK ideas for xtide_mpd** (retry + soft reset, media-change generation counter, real sense): list in #52.
- [ ] **BlueSCSI hot-swap:** fixed removable LUN reporting media change via sense (no medium, then UNIT ATTENTION). Read WINUSB for how it presents mass storage and HID to Win9x. #50
- [ ] **CH376 file-level FAT access** as an angle on #51: the chip does the FAT, only file data crosses the bus. Sources: dfmcwhir repos, `docs/resources_and_sources.md`.

## Parked (owner, 10-15)

- [ ] IE4 install broke the disk driver on the webcam image (no snapshot). Reproduce in a VM with one.
- [ ] Creative WebCam II on LPT: image `OneDrive\Desktop\win95_webcam.img` (LS-120 removed, port visible).
- [ ] Iomega Jaz 2 GB on the T130B worked first time as a disk: add to `docs/5160_tuned_config.md`.
