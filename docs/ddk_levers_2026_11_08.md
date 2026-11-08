# DDK/SDK levers for the Direct3D frame, 2026-11-08

Question: what cuts the ~100 ms Microsoft's D3DIM spends per tunnel frame, how to load textures
once, and what removes page waits. Read-only survey; nothing built or measured.

## What the repo holds, and what it does not

| Source | Holds |
|---|---|
| `references/dx7sdk/dx7sdk-700.1/include/` | Public headers: `d3dcaps.h`, `d3d.h`, `d3dtypes.h`, `ddraw.h`. Flag definitions with short comments. |
| `references/jhrobotics/vmhal9x/d3dhal_ddk.h` (1961 lines) | DDK-layout `d3dhal.h` incl. DX6/7: CALLBACKS3, DP2 tokens, EXTENDEDCAPS6/7. MIT, third party. |
| `references/jhrobotics/vmhal9x/d3d.c`, `d3d_caps.h`, `mesa3d_draw6.c` | A working DX6/7 HAL (Mesa-backed): caps set, GetDriverInfo routing, DP2 command walker. |
| `references/dx61samples/d3dim/` | Application samples only. |

Not in the repo: `d3dhal.h`/`d3dnthal.h`/`d3dhalex.h` from a Microsoft DDK, any DDK driver sample
(perm3, s3, mga, rage, refrast), any DDK `.doc`/`.htm` text. `docs/directdraw_research_2026_11_01.md`
line 9 says the same of the Win95 DDK; `C:\98DDK` is not present on this host. Nothing local says what
D3DIM does per vertex, so every effect below is an estimate. The PC2X sample is not under
`references/`.

## 1. The runtime's ~100 ms

| Lever | What it does | Source | State | Effect |
|---|---|---|---|---|
| `D3DDEVCAPS_DRAWPRIMITIVES2` (0x2000) plus `GUID_D3DCallbacks3` answered in GetDriverInfo with `DrawPrimitives2` | DX6/7 runtime sends one command stream (tokens + vertex block) per DrawPrimitive instead of unpacking into DX5 DrawOnePrimitive/DrawPrimitives calls with a copy per call. Vertices can stay in the app's memory (`D3DHALDP2_USERMEMVERTICES`). | `d3dcaps.h:473`; `d3dhal_ddk.h:754-860`; vmhal9x `d3d.c:567-585`, `d3d_caps.h:265` | NEW. m8d3d.c:3746-3752 does not set it; driver info answers CALLBACKS2 and extcaps only (m8d3d.c:3716-3730) | Estimate: 10-30 ms on the sample if the DX5 path is what copies; unknown until a DX7 trace of which path runs. Costs a token walker (~15 commands, vmhal9x `mesa3d_draw6.c:457-1232` is the list). |
| Execute-buffer flag `D3DHALDP2_EXECUTEBUFFER` | Tokens arrive from an execute buffer; driver can return `D3DERR_COMMAND_UNPARSED` to make the runtime parse it. | `d3dhal_ddk.h:821,838` | NEW (only relevant after DP2) | Small. |
| Guard band (`dvGuardBand*` in EXTENDEDCAPS6) | A device with a guard band tells the runtime not to clip triangles that fit inside it; clipping happens only past the band. | `d3dhal_ddk.h:103-106`; `d3dcaps.h` (D3DDEVICEDESC `dvGuardBand*`) | HAVE: +-32768 (m8d3d.c:633-638); we clip ourselves in `m8d3d_draw_clipped` (3351). | None left. Whether D3DIM still clips against the screen is unmeasured; a count of D3DIM-clipped vs total triangles would show it. |
| `dwFVFCaps` low bits (1..8 = FVF-aware) in EXTENDEDCAPS6; `D3DFVFCAPS_DONOTSTRIPELEMENTS` | 0 means TLVERTEX only: the runtime must expand every vertex to 32-byte D3DTLVERTEX. Non-zero lets DP2 pass FVF vertices (smaller: no specular, fewer texcoord sets). | `d3dhal_ddk.h:108`; `d3dcaps.h:328-329` | NEW (ours is 0, m8d3d.c:635). Needs DP2. | Estimate: copy/expand saving only; a few ms. |
| `D3DDEVCAPS_HWTRANSFORMANDLIGHT` (0x10000) with `DRAWPRIMITIVES2EX` | Runtime passes untransformed vertices, matrices and lights through DP2 (`SETTRANSFORM`, `SETLIGHT`, `SETMATERIAL` tokens); we would transform. | `d3dcaps.h:478`; `d3dhal_ddk.h:~913-920`; EXTENDEDCAPS7 `dwVertexProcessingCaps` | NEW, not recommended | Moves the same float work onto our 387 (2.97 us a multiply-add, `memory/fpu_fitted_on_interposer`). Saves the runtime's per-call overhead only. Estimate: zero to negative. |
| `D3DDEVCAPS_SORTINCREASINGZ` / `SORTEXACT` | Runtime sorts polygons before submitting. We already sort at Flip. | `d3dcaps.h:457-459` | Do not set. | Would add runtime time. |
| `dwNumVertices` / `dwNumClipVertices` | Sizes the runtime's TL and clip buffers in GLOBALDRIVERDATA. | `d3dhal_ddk.h:68-70` | Unchecked in m8d3d.c | Unknown. |
| `D3DDEVCAPS_CANRENDERAFTERFLIP` (0x800) | Runtime does not wait for flip completion before issuing the next frame's draws. | `d3dcaps.h:469`; vmhal9x `d3d_caps.h:270` ("must be set on 2k driver") | NEW; not in m8d3d.c:3750 | Applies to the runtime's wait only, not our page wait. Likely small; cheap one-bit test. |
| DX7 removes 8 bpp as a hardware render depth | Already known: patched D3DIM at 8 bpp. | m8d3d.c:30-31 | HAVE (workaround) | n/a |

## 2. Textures

| Lever | What it does | Source | State | Effect |
|---|---|---|---|---|
| `TextureCreate(lpDDS)` -> handle; `TextureSwap`; `TextureGetSurf`; `TextureDestroy` | The only legacy callbacks that see the texture surface. Create is the one-time point: surface is the app's, in system memory (`D3DDEVCAPS_TEXTURESYSTEMMEMORY`). | `d3dhal_ddk.h:306-335,440-443,485-489` | HAVE (m8d3d.c:3131,3173,3189,3808-3811) | Tracing at create time needs the surface locked and read once; apps that fill the texture after creating it (the tunnel sample loads from a file after Create? unverified) would need the read deferred to first SetTexture (RenderState TEXTUREHANDLE). |
| Palettized formats in `lpTextureFormats` | Offering `DDPF_PALETTEINDEXED8` makes the runtime keep 8-bit texels and palette; DX6/7 delivers palette changes as DP2 `SETPALETTE`/`UPDATEPALETTE`. | `d3dhal_ddk.h:71-72,~915`; `d3dcaps.h:208` (ALPHAPALETTE) | NEW. m8d3d.c:3786-3799 offers two 16-bit formats only. | Estimate: a 4x smaller texture to read at trace time and to colour-map. The tunnel sample's own format choice is not checked. Needs the card's 8-bit path to stay indexed. |
| `D3DPTEXTURECAPS_POW2` etc. | Texture shape limits; extcaps min/max 1..256 already. | `d3dcaps.h:171-234` | HAVE | n/a |
| `DDSCAPS2_TEXTUREMANAGE` + `DDCAPS2_CANMANAGETEXTURE` | Driver-managed textures; runtime otherwise manages and calls Create/Destroy as it evicts. | `ddraw.h:~3140` | NEW | Not wanted: runtime management keeps our Create hook stable. |
| DP2 `TEXBLT`, `SETTEXLOD`, `SETPRIORITY` | Texture upload/LOD notifications (DX7). | `d3dhal_ddk.h:~920-935` | NEW (needs DP2) | Gives a second "texture changed" hook. |

## 3. Reference driver structure

| Source | Shows | Use |
|---|---|---|
| vmhal9x `d3d.c:128-131` (DP2 stub), `567-585` (CALLBACKS3 via GUID, only when DDI >= 6) | Minimal DP2 registration. | Copy the shape; gate on the runtime's DDI level so DX5 still gets CALLBACKS2. |
| vmhal9x `mesa3d_draw6.c:457-1232` | The token switch: POINTS, LINE*, TRIANGLELIST/STRIP/FAN, INDEXED*, `*_IMM`, RENDERSTATE, TEXTURESTAGESTATE, VIEWPORTINFO, WINFO, SETPALETTE, UPDATEPALETTE, ZRANGE, SET* transforms. | Which tokens a DX7 runtime sends even to a TL-vertex device; a driver may reject any it does not handle by returning `dwErrorOffset`. |
| vmhal9x `d3d_caps.h:227-271,283` | Caps for a DX5-only vs DX7 device (`CAPS_DX7` = HWVERTEXBUFFER, CANRENDERAFTERFLIP, DRAWPRIMITIVES2EX). | Template for the bits above. |

Not found: a DDK software rasteriser, any statement of which path the runtime takes, any vertex-buffer
callback beyond `D3DHALDP2_SWAPVERTEXBUFFER`/`VIDMEMVERTEXBUF` flags (`d3dhal_ddk.h:824-834`). Those flags
let the driver keep the runtime's vertex buffer and hand back a new one; with DP2 they would let us
read vertices without a copy but are untested.

## 4. Flip, vsync, page waits

| Lever | What it does | Source | State | Effect |
|---|---|---|---|---|
| `DDCAPS2_FLIPNOVSYNC` / `DDFLIP_NOVSYNC` | Driver says it flips without waiting for retrace. | `ddraw.h:3133` | NEW. Our Flip never waits for vertical blank (the "flip" is the draw: m8dd32.c:1633-1744); flag is advisory. | None on the page wait. |
| `DDCAPS2_FLIPINTERVAL` | Honours `DDFLIP_INTERVALn`. | `ddraw.h:3130` | NEW | None wanted. |
| `GetFlipStatus` (`DDHAL_SURFCB32_GETFLIPSTATUS`) | Lets the runtime poll instead of blocking in Flip. | `ddraw.h:1439` (interface) | NEW. Not in dd16.c:214-230 | Lets an app run its next frame while the card draws only if our Flip returned early; ours draws in Flip. Estimate: only useful after a rework that queues the frame. |
| `WaitForVerticalBlank` | HAVE (dd16.c:209). | | HAVE | n/a |
| `D3DDEVCAPS_CANRENDERAFTERFLIP` | See section 1. | | NEW | Small. |

The page wait (`m8dd_page_wait`, m8dd32.c:1562; `m8d3d_stat[19]`) is the engine finishing, not a retrace.
No flag in these headers removes it; the lever is overlap (queue the next frame while the engine draws),
which is our design, not the DDK's.

## Ranked by likely frame-time effect (all estimates)

1. DP2 + CALLBACKS3 + `D3DDEVCAPS_DRAWPRIMITIVES2`: up to ~10-30 ms of the 100 ms, if the DX5 path copies.
2. Palettized 8-bit texture format: reduces trace-at-create work and texel reads; effect on frame, not runtime.
3. Trace texture once at `TextureCreate` / first SetTexture (already hooked): moves per-frame sampling out of the frame.
4. FVF-aware extcaps (needs 1): a few ms.
5. `CANRENDERAFTERFLIP`: one bit, test it.

Measure first: a DX7 trace of whether DrawPrimitive reaches DrawOnePrimitive (CALLBACKS2) or DP2 on this
runtime, and ticks inside `m8d3d_render_primitive` ([24]) against the ~100 ms.
