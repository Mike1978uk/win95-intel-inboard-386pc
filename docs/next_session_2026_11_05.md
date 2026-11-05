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

## Next

1. Bed: `C:\M8D3DT.EXE bed2`, read the log. Gate: `createdevice 00000000`.
2. D2: triangles to the polygon fill.
