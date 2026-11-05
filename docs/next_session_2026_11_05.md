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

With a palette (`bed2`): `createdevice 88760082` = DDERR_INVALIDOBJECT. Counters (`M8D3DT bed5 b0c0ba0c`, the
address of `m8d3d_stat`: re-derive it from the DLL's exports after each build): **ContextCreate 0, ContextDestroy 1,
nothing else.** The runtime refuses before our context code, so the callback layouts are not the cause. velocity9x's
probe creates its target the same way, with OFFSCREENPLAIN in its caps; its target is 16 bpp.
`LoadLibrary("M8DD32.DLL")` from the test hangs the process: do not repeat. The DLL is not a module of the test's
process. D3DIM.DLL holds 110 copies of this error code, mostly parameter checks, so a search alone does not locate
it: trace `CreateDevice` in the bed's debugger instead, and check which of D3DIM and D3DIM700 runs.

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

1. Bed: rebuilt `M8D3DT`, read `hal ctx` in the log. Gate: `createdevice 00000000`.
2. D2: triangles to the polygon fill.
