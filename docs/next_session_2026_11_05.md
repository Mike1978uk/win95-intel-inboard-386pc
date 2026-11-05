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
