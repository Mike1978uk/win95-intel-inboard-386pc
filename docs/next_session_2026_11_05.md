# Next session, 2026-11-05

Order of work: `docs/mach8_frame_rate_plan.md`, last section. **The cloud session's tests are in #59**: carry
this line into new handoffs until every box there is ticked.

## State

- master has the cloud session's handoff commits only. Its code is on `claude/xtcf-tight-taskfile`,
  `claude/driver-optimisation-review`, `claude/dinput-probe`, `claude/video-playback` and
  `claude/bluescsi-5160-firmware-o12wij`: built, untested, unmerged.
- Working tree, uncommitted: the Direct3D stub (`m8d3d.c`, `dd16.c`, `m8dd32.c`, `build.ps1`). Commit once D1's gate passes.
- Bed `vm_5160_mcp` holds the stub DLL and the fixed `C:\M8D3DT.EXE` (palette on the target).

## Direct3D, bed log `C:\M8D3DT.TXT`

M8D3DT draws off-screen and logs: nothing on screen is expected.

| Mode | `target` | `createdevice` |
|---|---|---|
| 1024x768x8 | `8876017C` DDERR_OUTOFVIDEOMEMORY | - |
| 800x600x8 | OK | `8876023C` DDERR_NOPALETTEATTACHED |
| 640x480x8 | OK | `8876023C` DDERR_NOPALETTEATTACHED |

DX7 lists `Direct3D HAL` model 2 (RGB), depth `0800` (DDBD_8), shade `08` (COLORFLATRGB): P6 answered yes.

With a palette: `createdevice 88760082` = DDERR_INVALIDOBJECT, before ContextCreate.

**Cause, proven in the bed: DirectX 7 strips 8 bpp from a hardware HAL.** D3DIM.DLL `56663de9`
`and byte ptr [eax+0xa1],0xf7` clears `DDBD_8` from the global data's render depths, guarded by
`cmp [esi+0x1310],0` (read as the hardware description's dwFlags, an inference). D3DIM700.DLL `566636e1` has the
same instruction. `56669257` then fails the 8-bit target against the depths. Found by giving each of the 110 error
sites a unique code (technique 152): `8876A02C`, the exit at `566692ff`.

| D3DIM copy on the bed | createdevice | counters after the draw |
|---|---|---|
| stock | INVALIDOBJECT | ContextCreate never reached |
| depth test NOPed | OK | not read |
| only the `and` NOPed | OK | ContextCreate reached; RenderPrimitive 1 primitive; RenderState and SceneCapture called |

So our tables and callbacks work. No driver change gets an 8 bpp HAL past the stock runtime. This also explains why
no shipping 8-bit HAL was found. Measured on 7.0a only; earlier runtimes unchecked. Log: `docs/captures/2026-11-05_d3d_8bpp/`.

Routes, owner to choose: (a) offer the HAL at 16 bpp, drawing with the engine as described frames (F3); (b) patch
D3DIM for 8 bpp, own programs only; (c) `hwCaps.dwFlags = 0` to dodge the guard, untested and likely to confuse
programs that test it. Recommended: (a) to ship, (b) in the bed as scaffolding for D2.

⚠ Bed `vm_5160_mcp` still has the error-tagged `DDRAW.DLL` (in use, so the put failed): restore
`C:\WINDOWS\SYSTEM\DDRAW.DLL`, md5 `EEDE14A8FB89A725C903E33A6A0F9F28`, at the next stop. D3DIM and D3DIM700 are stock again.
Keep a copy of stock DX files first: the originals above live only in the session scratchpad, and the bed image.

Also learned: `LoadLibrary("M8DD32.DLL")` from a test program hangs it. Read `m8d3d_stat` by address, from the DLL's exports.
At zero texture formats a path in D3DIM returns `887602D0` (`56665e24`): expect it once textures matter.

dxdiag: velocity9x reports its Direct3D test failing at "step 44 (EnumTextureFormats)" with HRESULT 0, on a driver
with no texture format matching the display; its cause is an unproven lead
(`references/velocity9x/docs/issues/2026-09-02-dxdiag-fails-at-enumtextureformats.md`). Expect to publish a format.

## Sound loop lock (#55): third report

Card, 2026-11-05: locked with the Windows startup sound looping; shutdown and reboot cleared it, next boot normal.
Earlier: 16 bpp restart looping the shutdown sound (#55); first DirectX installer run with a constant tone
(`next_session_2026_11_02.md:172`). The loop is the Sound Blaster repeating its auto-init DMA buffer, so it marks
that the CPU stopped servicing it while a sound played, not that sound is the fault. All three began after our
driver reached the card; none is tied to it by evidence. Owner: new behaviour. Not reproduced in the bed.
Missing: how often, and the driver and mode at each lock. Test when it recurs: same boot with the sound scheme
off; if locks continue silently, sound was only the witness.

## Next

1. Owner picks the route above; then D2: triangles to the polygon fill.
2. Disk: repeat XTSECT and IDLECOST after `M8IDLE 10 10 600`. LZ4 hash runs are in `docs/captures/2026-11-05_lz4hash/`;
   LZ4B12 did not repeat 10-02 (compress ~5,060 against 4,712 us), so re-baseline before porting: LZ4B12FN looks ~3 % cheaper.

## Card state and the first triangle (2026-11-05)

Card: `M8D3D1` (commit 8878634) at 800x600x8, boot 209. Its first Direct3D triangle is solid: `docs/captures/2026-11-05_d3d_8bpp/M8D3DT_card1.TXT`.
Going back to `M8FLIP8` at 800x600x16: `C:\V9XREMOTE\REG\M8BACK.REG`, with the old DLL at `C:\V9XREMOTE\REG\M8DD32.BCK`, renamed in by WININIT.INI.
Bed `vm_5160_mcp`: same source, stock D3DIM in SYSTEM, patched copy in `C:\M8D3D\`, CONFIG.SYS identical to the card's. `vm_5160_now` and `vm_5160_ati` still default to FULL.
Next: a triangle bench (triangles a second by size, against RGB emulation), then the 16 bpp route.

## Direct3D at 16 bpp (later 2026-11-05)

Works on the card on stock DirectX (fd86aa6): `docs/captures/2026-11-05_d3d_16bpp/`. Card now `M8D3D2` at 800x600x16; way back to `M8FLIP8`
unchanged. Bed `vm_5160_mcp` at 640x480x16 with the build before the dirty-rectangle fix; AUTOEXEC's temporary imports removed (backup C:\AUTOEXEC.D3D).
Fixed on the way: `m8dd_dirty` wrote the colour over pixels just drawn once past 8 rectangles (also DirectDraw, since F3).

Next: the owner watches M8D3DF for flicker; profile a frame; a box per triangle in place of one around all; try dxdiag.
Open: flip chain targets only, pointer not lifted during replay, nearest colour with no dither, drawn on the visible screen.

## Latest (2026-11-05, evening)

**Every technique is listed for ruling in or out: `docs/mach8_frame_rate_plan.md`, "Direct3D levers".** Goal 60 fps or better on M8D3DF's small tests.
Card: `M8D3D5` (a46433e) at 800x600x16, boot 213. dxdiag's two Direct3D tests pass, stock DirectX; hardware 20.78 s against software 183.84 s on `M8D3D2`. Owner's rerun on `M8D3D5` pending.
Owner's reading of the "flicker", from `photos/flicker1-8.PNG`: a face's two triangles in opposing block colours. Crisp undithered look preferred; gradients required (C1-C3).
Next: the build in the working tree (`%TEMP%\m8next`, not on the card) times inside Blt: find why the black clear costs 5-35 ms.

Card hung 2026-11-05 about 15:45 during the second M8D3DF run on `M8D3D6` (boot 214): first run finished, agent stopped answering, ping still answered, not back in 9 minutes. Owner to report the screen and any sound, and restart. `C:\M8D3DF.TXT` holds run 1.

## START HERE next session (end of 2026-11-05)

**Running when this was written:** `M8D3D7` (065a3ac + uncommitted M8D3DF `high` option) installing on the card, then `M8D3DF norm 3` and `M8D3DF hi 3 high`. Results append to `C:\M8D3DF.TXT` on the card (host 192.168.68.62, `v9xctl -Host`; `mcp__v9x` is the BED). Fetch them, keep in `docs/captures/2026-11-05_d3d_16bpp/`, commit `tools/m8d3dtest/M8D3DF.C`.

**The question it answers:** `Blt` in our driver 0.22 ms, the same call from the program 4.8-33 ms, all clears the described path. Theory: Windows runs other threads after a long Flip. If the `high` run's clear collapses, it is scheduling; if not, look in DDRAW's Blt path. Unproven.

**Owner to check on M8D3D7:** static gone at full-screen start of dxdiag and DirectDraw (fix in `m8_fb16_send_all`); rectangle blocks gone (`m8d3d_edges`, unproven hypothesis). Use `photos/shading3_blocks.PNG` as the before.

**Open hang:** card hung once, black screen with the egg timer, no restart by itself, during the second M8D3DF run on `M8D3D6` (run 1 clean; agent gone, ping alive). Rebooted by the owner, boot 215. One report, cause unknown.

**Then:** the "Direct3D levers" table in `docs/mach8_frame_rate_plan.md`, every row ruled in or out by a card number. Best figures so far (`M8D3D6`, one run): hal_tiny 41.9, hal_small 43.6, hal_static 84.6-152, hal_large 15.6, hal_many 11 fps. Goal 60+. Next after the clear: V19 status reads, V18 high-byte writes (probe), V5, V6, V15, C2 adaptive palette, C3 gradient bands (required).

**Way back:** `C:\V9XREMOTE\REG\M8BACK.REG` = M8FLIP8, High Color, 800x600; old DLL `C:\V9XREMOTE\REG\M8DD32.BCK`, rename in by WININIT.INI. Card scripts were in the session scratchpad: model them on `tools/m8_card_cycle.ps1`; DLL by `M8DD32.NEW` and WININIT.INI. Git: local master ahead of origin, nothing pushed; #55/#59 updates not posted.
