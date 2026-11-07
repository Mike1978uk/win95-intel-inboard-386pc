# Silicon probes, 2026-11-07

Card, boot 258, `M8D3DY` (pageable), 640x480x16 DirectDraw exclusive, agent on. Run: t1 of `tools/m8probe/M8TEX.C`. Log: `M8TEX.TXT` in this folder. Waits lost: 0. No hang.

Times are us per draw, fastest of 3 reps x 20 passes.

| Arm | 32 px | 100 px | 100 px vs flat |
|---|---|---|---|
| flat: scratch, edges, polygon fill | 409 | 1,011 | 1.00x |
| tcopy: texture copied to scratch, edges, 6213h fill | 463 | 1,577 | 1.56x |
| tatlas: edges XORed into plane 7 of the texture, 6213h fill, edges XORed out | 529 | 1,064 | 1.05x |
| avg, add, xor mix on the polygon fill | 409-410 | 1,011-1,012 | 1.00x |

100 x 100 box:

| Arm | us |
|---|---|
| replace | 190 |
| replace with WRT_MASK 0Fh | 347 |
| add 1Bh | 347 |
| add with WRT_MASK 0Fh | 347 |

Findings:

- tatlas leaves plane 7 clear (0 pixels set on the checked row): a texture in card memory takes its own outline and gives it back.
- 6213h pixels follow the atlas 1:1 on the checked row. The left edge pixel keeps bit 7 (9A for 1A, B4 for 34): 7-bit colours, as M8GRAD found; the driver must mask or draw over it.
- Mixes work on the polygon fill at no extra time. After 20 passes: add saturated at FF, xor cancelled back to background, avg converged on the ink colour.
- Field arithmetic: add 1 with WRT_MASK 0Fh, 20 passes, on 41h gave 4Fh on the card: it saturates inside the field. Bed gave 45h: the bed is wrong here.
- A copy wider than its source does not wrap in the row: at SRC_X_END the source continues on the next source row (40-4F then the row below). The bed agrees.
- SCAN_TO_X with DP_CONFIG 6211h steps along the source, one source pixel per destination pixel. The bed agrees.
- LINEDRAW with DP_CONFIG 6211h steps along the source (card: 43 44 45 ...). The bed drew nothing there. This answers M8TEXL, for a horizontal line only so far; other slopes are the next probe.
- Bed difference: in M8TEX the polygon-mode edges drew nothing in the bed (scratch stayed 00), and the triangle arms stayed background; the card drew them. Cause not found.

## Locked against pageable

Card, `M8D3DF` at high priority, agent on, fps. Locked = `M8D3DX` runs zl, zl2. Pageable = `M8D3DY` runs ya1, ya2, boot 258, FB16CHK 0 differ. `M8D3DY` also carries the desktop changes.

| Test | Locked | Pageable |
|---|---|---|
| hal_many | 31.2, 30.8 | 40.8, 39.6 |
| hal_large | 38.0, 38.3 | 43.0, 43.4 |
| hal_grad | 35.9, 36.2 | 40.7, 42.8 |
| hal_turn100 | 30.7, 30.6 | 32.5, 30.8 |

Locking slowed each test except hal_turn100.

## Texture probe by rows and lines (M8TEXR, run r1, boot 258, M8D3DY)

| Arm | 32 px | 100 px |
|---|---|---|
| flat, polygon fill | 409 us | 1,011 |
| tatlas, 6213h from card memory | 531 | 1,067 |
| rows, source per row | 1,486 | 4,655 |
| shear, source moved per row | 1,583 | 4,967 |
| flatrows, one colour per row | 1,118 | 3,558 |
| lines, source row per 45-degree line | 1,764 | 5,577 |

LINEDRAW steps the source once per pixel on the line's major axis. Diagonal and vertical read clean. Shallow and steep agree at the points read: the read-back samples a straight line, not the engine's steps. A reversed line reads forward along its own path: a mirror. SRC_Y_DIR picks the next or previous source row at SRC_X_END. A horizontal line starts 3 source pixels late and misreads past pixel 32, so spans use SCAN_TO_X.

## HAL textures and the DX5 route

Stage one, `M8D3DZ` and `M8D3E1`: formats 565 and 1555 in system memory, handles, TEXTUREHANDLE and TEXTUREMAPBLEND. A triangle takes the texel under each corner through the gradient path: an approximation, not mapped texture. Pictures from the card: `grab_zg/`.

`M8D3DZ` run za1, high priority, agent on: hal_tex 39.1 fps, hal_tex32 62.9, against hal_grad 37.8 and hal_large 40.3. Run za2 hung at its test 1: ping answered, agent did not. The texture tests had passed in za1.

`M8D3E1` adds GetDriverInfo, GUID_D3DCallbacks2, DrawOnePrimitive, DrawOneIndexedPrimitive, DrawPrimitives and SetRenderTarget. Run e1a1, boot 261: exec 0 on every test, so the runtime uses the new route. FB16CHK 0 differ. **No fps gain on this bench:** hal_many 39.9 against 38.6-40.8. Kept because 3DMark99 uses only these calls, per velocity9x's records.

| 64 triangles, e1a1 | fps | DirectX call ms |
|---|---|---|
| hal_many | 39.9 | 9.47 |
| hal_many_c, no clip | 45.6 | 5.95 |
| hal_many_e, no clip or extents | 49.4 | 3.85 |

The runtime's clip and extents cost 5.6 ms a frame, and this route does not remove them.

## Real programs on the HAL (bed `vm_5160_mcp`, 2026-11-07 evening)

Original SDK DirectX 6.1 sample `tunneldp.exe`, unmodified. Bed timings are not card costs. Picture: `tunnel/tunneldp_hal_bed_m8d3e7.png`.

What it stopped on, in order, and the answer:

| Build | What happened | Cause | Answer |
|---|---|---|---|
| `M8D3E2` | "No Z buffer, switching to software" | The framework calls EnumZBufferFormats before anything else, even with `g_bAppUseZBuffer = FALSE` | Report one 16-bit Z format; claim ZBUFFERLESSHSR, sort by depth at Flip |
| `M8D3E4` | No error, window, 0.01 fps, RGB Emulation only | `DDCAPS_NOHARDWARE`, hw caps 02000000: DirectDraw silently discarded the whole HAL. Adding DDCAPS_BLTDEPTHFILL, DDSCAPS_ZBUFFER and dwZBufferBitDepths caused it; which of the three is not yet known | Taken out of the DirectDraw caps in `M8D3E6`; Direct3D side kept. Rule recorded by velocity9x, `references/velocity9x/docs/issues/2026-08-14-directdraw-hal-nohardware.md` |
| `M8D3E6` | HAL listed. "No 3D Device, switching to software" | DirectDraw sets CANRENDERWINDOWED itself, so the framework tries the HAL in a window, which needs a flip chain here | By hand: F2, Direct3D HAL, 640x480x16. Windowed HAL is a later nice-to-have |
| `M8D3E6` | Full screen, then exits | Census: contexts 1, clipped 59, tris 0: the clipper recursed on rounding errors until the stack ran out. My test used exact values | `M8D3E7`: cut corners snapped to the edge; pieces not checked again |
| `M8D3E7` | **Runs full screen on the HAL at 640x480x16, 0.11-0.16 fps** | Far too slow even for the bed, where hal_tex gives ~10 fps: being looked into | |

The tunnel's texture is 256 px; it uses DrawOneIndexedPrimitive and DrawPrimitives. Tools: `tools/m8d3dtest/M8ENUM.C` lists what DirectX reports, `tools/m8d3dtest/M8CENSUS.C` reads the counters in the driver.

A scratch rebuild of the sample with full-screen hardware forced, built against the SDK's headers, stops on "requires a 16-bit display mode"; not looked into, and set aside.

Card: `M8D3E2` installed, boot 262. Its bench failed: the owner found a black screen with an egg-timer cursor, and restarted it. No card figures for `M8D3E2` or later.

### Iterations, evening

| Build | Change | Result |
|---|---|---|
| `M8D3E8` | M8DD_TRIS_MAX 512 to 1024; triangles under 32 px flat | Bed tunnel 0.13-0.22 fps, about 750 triangles a frame, sizes mostly 8-32 px. Card e8a1, boot 264, 0 differ: hal_depth pixel 07e0, the nearest on top though sent first. hal_tex 41.9, hal_tex32 61.1, hal_large 41.6, hal_many 34.1: down from 39.9, our share 3.35 to 6.03 ms |
| `M8D3E9` | Clip check by float bits; census detail 1 in 8; specular added | Bed runs. Picture still dark: suspected averaged corner samples of a high-contrast texture. Card: did not come back within 4 minutes of the restart; ping answered, agent refused |

Tunnel on the card, `M8D3E8`: stopped responding at "No 3D Device, switching to software", then restarted itself. No card baseline yet. On the bed that phase only crawled.

`M8TUNF`: a copy of `tunneldp.exe` that starts full screen on hardware, 5 bytes changed, by `tools/m8d3dtest/tunnel_full_patch.py`. The sample's other gate exits if the device cannot render in a window, with a misleading message about 16-bit modes: that gate is the sample's, not DirectX's.
