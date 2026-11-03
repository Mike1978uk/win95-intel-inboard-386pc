# Next session, 2026-11-04

The plan and every number are in `docs/mach8_frame_rate_plan.md` (worklist X9). This file is what to do next.

## State

- master `b770481`, pushed. Card on **`M8FLIP4.DRV`** (= master), boot 201, **800x600x16**; `Shell Icon BPP`=16
  (high-colour icons); `Display\0000\INFO` chip and DAC names set (`bed/M8INFO.REG`).
- Bed `vm_5160_mcp`: master's driver and DLL, **left at 16 bpp**: its `AUTOEXEC.BAT` imports `C:\M8DISP.REG`
  and `C:\M8FB16.REG` at every boot (`REM TEMP flip test`). The original is in the session scratchpad only;
  to undo, delete those three lines (`tools/fatls.py --get`, edit, `tools/fatcp.py`).
- M8DDB: `fs16` now ends with `mx_flip` and `mx_flipblt` and shows an n/7 title card before each test. It runs
  unattended through the agent with `-ShowWindow` (`v9xctl exec -Application C:\M8DDB.EXE -Arguments "fs16 x"`).
- Driving the bed by MCP: ctrl+Esc does not open Start; `mouse_to` (30, 468) and a click do. The bed's
  keyboard is UK: `\` is scancode `0x56`.

## Done 2026-11-03 (card-measured)

- `75d0220` string I/O in M8DD32: dd_lock +50 %, dd_sys +24-39 %. M8CUBE fixed point by default (`f` = float).
- `3ed37d3` vblank wait reads the latch every ~90 us (bus free for SB and XT-CF, #47); palette by `rep outsb`.
  `v` fps at 90 us not measured (63.3 at 0.25 ms vs 69.2 before): needs the card at 8 bpp.
- `55412b2` **16 bpp Flip with described back buffers** (plan F3). dxdiag's frame: 6.8 fps by lock, 208-220
  by Flip, card = RAM. Owner on dxdiag test 3 on the card: works, faster, stutters now and then.
- `b770481` chip and DAC names for dxdiag.

## Open from today

1. **Why the register shadows went stale** at the start of a flip (a black fill drawn white). Fixed by
   `m8_hw_forget` (idle wait + shadow clear) at each batch; the cause is not found. Look for a writer of
   FRGD_COLOR / FRGD_MIX / DP_CONFIG that skips the shadows between two ExtEscape calls.
2. **dxdiag test 3 stutter** on the card: a periodic wait. Candidates: the per-flip idle wait, GetDC/
   ExtEscape cost, or DDFLIP_WAIT. Measure before changing.
3. 16 bpp flip-chain edge: GDI drawing while the back buffer is shown goes to the frame buffer, which is not
   then on screen (a message box during a full-screen program).

## To do, in order (agreed with the owner)

1. **P6 / D1**: stub Direct3D HAL in M8DD32 at 8 bpp; gate: the device creates in the bed. Agree the shape
   with the owner first (`docs/mach8_3d_research.md`, "Direct3D HAL").
2. **B1** 8 bpp full-screen flip: two pages in VRAM, Flip = CRT_OFFSET; zero system RAM. Measure `v` here.
3. **F5** adaptive palette (owner): exact colours learned from the calls; one-colour blocks one engine fill.
4. Profile the ~0.8 ms per face (`-Prof -os`), then asm.
5. G11 present in the blank; G12 background in card memory.
6. Direct3D proper (D2-D4) on the parts above.
7. Owner's levers after Direct3D: **F6 active region** (with page Dirty bits, F4), sprite cache in card
   memory, move detection.
8. Cheaper-agent jobs: cheap probes (P9, P11, P13, 8-INFO, split screen), #54, #48, a RAM audit on the card.

## Owner's direction (11-03)

Describe frames, don't scan them; static content costs nothing; limits simplify (memory
`project-describe-dont-scan`). 3D is benched by fps, higher is better. Build the owner's levers, not just
note them. Check every fix on the real card: the bed passed a build that dropped fills on the card.

## Later on 2026-11-03

- Card `M8FLIP4` (two agent runs): M8DDB fs16 clean, `mx_flip` 208/220 fps. Owner: all M8DDB tests
  passed by eye; dxdiag test 3 showed the static background again (front buffer, see wip README).
- Fix in the working tree, on the card as `M8FLIP5` (boot 202): both new flip buffers described
  black. Owner's dxdiag check pending.
- Code segment full: dd16.c moved to `M8DD_TEXT` (2,615 bytes free in `_TEXT`). Not yet on the card.
- Direct3D stub written (`drivers/mach8_display/wip/2026-11-03/`); next: re-apply on the split,
  build, bed run of a DX5 test that lists devices and creates the HAL device (P6).
- Owner's further levers recorded in the plan (F5 adaptive palette, F6 active region).

## End of 2026-11-03 (master = card `M8FLIP7`)

- `M8FLIP5` (black fill sent from inside CreateSurface): dxdiag test 3 white background; M8DDB exited to a
  white screen with an hourglass, hard reboot. Fill at creation dropped (`M8FLIP6`): M8DDB clean, exit clean.
- dd16.c in its own segment `M8DD_TEXT`: on the card since `M8FLIP6` (boot 204), GDI pixels unchanged.
  `_TEXT` 2,615 bytes free. The Direct3D stub can now be re-applied (`wip/2026-11-03/`).
- dxdiag test 3 still showed static with `M8FLIP6`: the frame buffer keeps the old desktop at the old
  pitch. `M8FLIP7` clears the frame buffer to black at a 16 bpp size change (`m8_fb16_clear`).
  **Owner's dxdiag check of `M8FLIP7` pending: if static remains, instrument dxdiag's calls (a log
  file written by M8DD32) rather than guess.**
