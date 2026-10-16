# Work list - in attack order

**Next (owner, 10-16): levers first, then the driver.** Section L before D8: the more we know about the card, the
better the driver - 10-16's census already found silicon ATI's drivers never used. The XT-CF measured-wait driver
(E, 10-10) stays queued behind it.

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
- [x] **A12 Submitted 10-16: 86Box PR #8351** (TC1995: Win 3.1 8514.DRV File Manager glitch from #8289).
      Command 4 width/height swapped; card probe M8NIB2 = fixed build byte for byte. Fork `mach8-nibble-axes`
      `3b23c04b2`. Details: `docs/mach8_cmd4_axes_2026_10_16.md`. Owner tells TC1995. Merge -> README row.
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
      monitor, or clean? Can both CRTCs run identical timing?
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
      `docs/rom_and_eeprom_dumps.md`; ROM mode tables bit by bit (B0/B1/B4/B5/BA/BB/BD/BF); ROM API arguments (72DDh).
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
- [ ] **D4 More than 256 colours (owner, 10-11).** Mach8 is 8 bpp only, so: (1) dithered fills from PATTERN_L/H /
      PATT_DATA - same bus cost as a plain fill; probe with A2; (2) palette splits per band on vblank / LINE_SYNC,
      ~1 colour change per line (estimate: ~23 us per entry vs ~32 us line); (3) error diffusion for still images on
      the 486; (4) ~~8-bit DAC~~ - the RAMDAC is an INMOS IMSG176 (6-bit), no 8-bit mode (photo, 10-11).
- [ ] **D5 DirectDraw (later).** A surface Lock needs a linear frame buffer; the Mach8 has none, so it rests on the
      28800 A000 window. M8LIN (10-11): one 64 KB bank, no 128 KB window, AC bit 0 no effect - 320x200x8 fits
      unbanked, 320x240 needs a bank switch. Then a HAL from
      vmhal9x (`references/jhrobotics/`, MIT) with flip on CRT_OFFSET.

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

## Parked (owner, 10-15)

- [ ] IE4 install broke the disk driver on the webcam image (no snapshot). Reproduce in a VM with one.
- [ ] Creative WebCam II on LPT: image `OneDrive\Desktop\win95_webcam.img` (LS-120 removed, port visible).
- [ ] Iomega Jaz 2 GB on the T130B worked first time as a disk: add to `docs/5160_tuned_config.md`.
