# DirectX SDK/DDK bench candidates and sample-driver FIFO lessons, 2026-11-09

Research only. Nothing here was run on the card or a bed. Triangle counts come from source constants
unless marked estimate. Builds on `docs/ddk_levers_2026_11_08.md` and `docs/d3d_benchmarks_and_games.md`.
Card runtime: DirectX 7.0a (`docs/mach8_directdraw_plan.md:66`); 8 bpp needs the patched D3DIM
(`docs/next_session_2026_11_05.md:28`). Our HAL claims `ZBUFFERLESSHSR` and a 16-bit Z depth
(`m8d3d.c:3801,3820`), so a sample that asks for Z gets a Z surface (640x480x2 = 600 KB of 1 MB).

## 1. Sources and what was downloaded

All under `references/dxddk/` (ignored, added to `.gitignore`). Names inside the DDK trees are
flattened by the installers: DX5 DDK `<hex offset>_<name>`, DX7 DDK `F<id>_<name>`, Win98 DDK
`SRC_DISPLAY_<dir>_<name>`. Directory structure inside DX7 DDK was not recovered.

| Path | Size | Source |
|---|---|---|
| `dx5ddk/dx5ddk.exe` + `src/` (320 files) | 7.5 MB + 21 MB | https://archive.org/details/dx5ddk (DirectX 5 DDK Release 3, Sep 1997). The `_SETUP.1` InstallShield-3 archive has no extractor here; unpacked with a 60-line PKWARE-explode script (all 320 entries, sizes match). Gave the S3 ViRGE sample 3D driver. |
| `dx7ddk/dx7ddk.exe`, `setup.exe`, `src/` (249 files) | 5.2 + 5.7 MB + 11 MB | https://archive.org/details/dx7ddk (DirectX 7 DDK, Feb 2000). Gave the 3Dlabs Permedia2/GLINT sample (Win9x display driver + D3D DP2), `d3dref` refrast source, `d3d.doc`, `3dimplementationdetails.doc`. |
| `ddk9x/win98ddk/` (364 files) | ~10 MB | https://archive.org/details/DDK-9x-ME (`98DDK.RAR` -> `DX_DDK.CAB`). Gave DX6-era S3 ViRGE (adds DrawOnePrimitive/CALLBACKS2), generic `d3dmini.c`, `mini\xga`, `vdd\ati`, `vdd\s3v`. RAR itself deleted after extraction. |
| `ddk9x/win95ddk/` | 1 MB | `95DDK.RAR` in the same item (`WIN95DDK.iso`). Only `DISPLAY\SAMPLES\MINI\FRAMEBUF` and `DOCS\DISPLAY.DOC` kept. |
| `bench_gamestar/`, `bench_pcplayer/` | 1.7, 1.2 MB | https://archive.org/details/gad3dbe101 and https://archive.org/details/pcplbe101 (RARs; inner `DATA.Z` not unpacked: different InstallShield directory layout). |

Read in scratch only, not kept: DirectX 3.0a SDK https://archive.org/details/directx3asdk (`DIRECTX3A.EXE`,
55 MB; gave `SDK/BIN`, sample sources, `DOCS/README/D3D.TXT`, `D3DOVER.DOC`); the local
`references/d3d_programs/idx5sdk.exe` = Internet DirectX 5 SDK (Aug 1997, `DX5SDK.EXE`);
https://archive.org/download/MicrosoftDirectX7SDK/DXF.zip/ (DX7 SDK file listing only).
Local `references/dx61samples` is the DX6.1 SDK; its readmes and `.cpp` files were read for part 1.

Not found: DirectX 1 Direct3D (https://archive.org/details/gamesdk, Game SDK 1995, has DirectDraw/Sound/Input/Play
only; Direct3D starts at DX2). Final DX2 SDK on archive.org (only https://archive.org/details/ms-dx20-d3d-beta2, an ISO,
not opened; the final DX2 `sdk/bin` list is at https://discmaster.textfiles.com/browse/29526/WWLCD1.ISO/pc/directx2/sdk/bin).
DDK sample drivers for MGA, Rage, Permedia (original), perm3: none in these items. A Win95-DDK 8514 or
Mach sample: none (`win95ddk` has only the framebuffer mini; `win98ddk` has `mini\xga`, `vdd\ati` VDD only).
`win98se_ddk.exe` (https://archive.org/details/win98se_ddk.exe) holds no display or 3D files.

## 2. Bench candidates by SDK

Shared facts. Every D3DApp-based sample (DX2/3/5) shows fps and triangles/s: DX3a
`SDK/SAMPLES/MISC/D3DMAIN.CPP` plus `stats.cpp`, function `WriteFrameRateBuffer(float fps, long tps)`.
Every DX6.1/DX7 d3dframe sample prints `"%7.02f fps (%dx%dx%d)"` per second (`dx61samples/d3dim/src/<s>/winmain.cpp:606-629`).
A sample whose framework asks for Z fails with `D3DFWERR_NOZBUFFER` when the device has none
(`tunneldp/winmain.cpp:398,707`); ours offers one, at a VRAM cost.

### DirectX 2 (1996) and 3/3a (1996)

DX2: https://discmaster.textfiles.com/view/29526/WWLCD1.ISO/pc/directx2/sdk/samples/d3dsamp.txt (sample readme).
DX3a: https://archive.org/details/directx3asdk. Prebuilt in `SDK/BIN`: `FLIPCUBE.EXE`, `TUNNEL.EXE`, `TWIST.EXE`,
`D3DTEST.EXE`, `GLOBE.EXE`, `VIEWER.EXE`, `FLIP2D.EXE`. Source only, no exe: `OCT1`, `OCT2`, `SPHERE`, `TRIANGLE`.
API: execute buffers (immediate mode); Globe/Viewer/Tex*/Hier*/Faces are Retained Mode on the same runtime.

| Sample | Scene | Needs | Notes |
|---|---|---|---|
| flipcube | cube, 6 faces = 12 tri (readme: "separate material per side") | flat default, Z off (`FLIPCUBE.C:132-133`), no texture | Window or full screen; mouse spins. Smallest scene in any SDK. |
| tunnel | 8 sides x 20 segments x 2 = 320 tri (`TUNNEL.C:76-84`) | Z off (`:449`), checker + `tex2` textures, perspective | Same scene as our bench at one third the triangles (DX6.1 uses 24 sides = 960, `tunneldp/tunnel.cpp:129-135`). |
| twist | flattened sphere 9 rings x 15 sections, ~250-270 tri (estimate from `TWIST.C:171`) | untextured per readme | All motion is matrix multiplies in the execute buffer, so the runtime transforms. |
| oct1 / oct2 | octahedron, 8 tri | oct1 textured background, oct2 none | Source only. |
| sphere | 18 x 20 sphere, ~700 tri (estimate, `SPHERE.C:110`) | one material | Source only. |
| d3dtest | polygon-throughput test, fill-rate test (four tunnels, front-to-back or back-to-front), texture-create timing | textures, tests the HAL only | `D3DOVER.DOC` calls it the tool to compare drivers; strings in the exe say "polygon throughput test". Needs a texture format list; not run. |
| Rockem3D (DX2/3/5 SDK) | Retained-Mode game, robots | RM, textures, sound | TAB shows fps and an H/S flag (hardware or software device). Not low-count; shows a real RM title. |

### DirectX 5 (Aug 1997 Internet SDK, local `idx5sdk.exe`; DDK Oct 1997 above)

No archive.org item for the final DX5 SDK was found; https://archive.org/details/dx5_beta2 is Beta 2 (265 MB ISO, not opened).
`cdrom\sdk\bin` ships: `tunnel.exe`, `twist.exe`, `flip2d.exe` (DirectDraw cube, GDI/software), `flip3dtl.exe`,
`pplane.exe`, `boids.exe`, `globe.exe`, `viewer.exe`. Sources: `flip3d` (readme: DrawPrimitive, "uses D3DTLVERTEX
data only", full-screen only, textures via GDI colour conversion), `boids`, `pplane`, `d3dex1/2` (spinning triangle,
execute buffer; source only), `tri3`. The DX5 `flipcube` is DirectDraw only (`flip2d`); `flip3dtl` is the 3D one.
Gap: no `tunnel`/`twist` source in this SDK; triangle count of those two exes is unverified, assume DX3a's.

### DirectX 6 / 6.1 (1998) — https://archive.org/details/directx6sdk (218 MB) and https://archive.org/details/dx-6-sdk

Local: `references/dx61samples/d3dim/bin`. DrawPrimitive/DrawIndexedPrimitive on IDirect3DDevice3 with
D3DVERTEX (runtime transforms and lights). DX6 runtime sends DrawPrimitives2 when the HAL has it.
Z column is `g_bAppUseZBuffer` in each `.cpp`.

| Sample | Triangles | Z | Texture / other | Path |
|---|---|---|---|---|
| tunneldp | 960 (`tunnel.cpp:129-135`) | off | 1 texture, fog option | DrawIndexedPrimitive |
| tunneleb | 960, identical scene | off | same | execute buffer |
| lights | 3 walls x 242 + sphere meshes | off | banana texture, D3D lights | DrawIndexedPrimitive |
| mtexture | few quads (room) | off | needs 2 texture stages; ours offers 1 | DrawPrimitive |
| compress, filter, lightmap, mipmap, shadowvol | small | off | texture; alpha in compress, lightmap, shadowvol | DrawPrimitive |
| fireworks, flare | 100 and 12 sprite quads | off | alpha blend | DrawPrimitive |
| bend, boids, pplane, fog, vbuffer, wbuffer, billboard | 100s to 1900 (fog grid 31x31x2 = 1922) | on | texture; billboard alpha; wbuffer w-buffer; vbuffer IDirect3DVertexBuffer | DrawPrimitive |
| globe, tex1/3/4/5, trans, faces, hier1/2, quat, fly (d3drm) | small meshes | RM manages | Retained Mode | RM |

`bellhop` is a DirectPlay lobby sample (`dplay/bin`), not Direct3D. No `flipcube`, `twist` or `oct` in DX6.1.

### DirectX 7 / 7.0a (Sep 1999 / Feb 2000)

SDK: https://archive.org/details/MicrosoftDirectX7SDK (`DXF.zip`, 125 MB, listed) and
https://archive.org/details/cdrom-directx-7-sdk (CD image, 380 MB, not opened). `samples/multimedia/d3dim/bin`: bend,
billboard, boids, bumpearth, bumpwaves, clipmirror, compress, dolphin, envcube, filter, fireworks, flare, lightmap, lights,
mfcfog, mfctex, mipmap, mtexture, shadowvol(2), spheremap, stencildepth, stencilmirror, vbuffer, videotex, wbuffer, xfile;
`d3dx/bin`: envmap, gravity, particle, sprite, tentacle, trivfs, trivwin; `vbsamples/d3dim/bin/vb_tunnel.exe` (VB tunnel,
the only tunnel). No `tunneldp/eb`, `pplane`, `fog` exe. Most DX7 samples need Z, stencil, env or bump maps: low suitability.
The DX7 SDK is also what the card has installed, so the DX6.1 exes above run on its runtime.

### Third party

- PC Player Direct3D Benchmark v1.10 (1997, German magazine): https://archive.org/details/pcplbe101. Description: "working
  with old 1, 2, 4 MB graphics cards like S3, Trident". README (Word 6, partly unreadable here): options for fog, dithering,
  perspective correction, bilinear filter, transparent textures (trees), software star field, sound; results appended to
  `pcpd3dbn.txt`; "relatively small polygons". Gamestar Direct3D Benchmark v1.00: https://archive.org/details/gad3dbe101,
  same description, README is empty. Both ship `SETUP.EXE` + `DATA.Z` (contains `pcpd3dbn.exe`). Textured; Z need unknown.
  Licence not stated; treat as period freeware, not for redistribution in the repo.
- Not found: a freely redistributable low-polygon 1996-98 title documented to run at 60 fps on DX5-7 software or HAL with
  a 486-class CPU. The "60 fps is possible" case has no published sample; the lowest-count scenes (flipcube, oct) are the
  nearest and the only evidence is ours once measured. Games stay as in `docs/d3d_benchmarks_and_games.md` section 2.

## 3. DDK sample drivers: how they feed the hardware

Cites are `<tree>/src/<file>:<line>`; `dx5` = `dx5ddk`, `dx7` = `dx7ddk`, `w98` = `ddk9x/win98ddk`.

### 3.1 S3 ViRGE (dx5 `s3data.h` = `002a121b_s3data.h`, w98 `SRC_DISPLAY_MINI_S3V_*`)

- Two modes chosen at init. FIFO mode: `WaitFifo(x)` spins `while(((MmioRegRead(0x8504)&0x1F00)>>8) < x)` (free slots),
  `s3data.h:13-17`. Called before each register group with that group's size (2, 9, 14 slots): `d3ddrv.h:147,155,246,330,379,396,414`;
  `d3dline.c:92,114,134,149,198`. Never waits for empty during drawing. Wait-for-empty `WaitFifoEmpty()` (`s3data.h:19-23`) and
  `WaitForEngineIdle()` (`:25`) only on first DMA enable, `d3dpoint.c:403-412`.
- DMA mode (4 KB ring, not on ViRGE or GX2/M5 or DX rev A; `d3dcb2.c:60-110`): driver writes register images into a ring at
  `g_DmaIndex & 0xFFF` (`s3data.h:79-84`), each block led by a header word (`_WRITE_HEADER`, `:88-104`). Space test uses a
  cached read pointer and re-reads the hardware register `0x8598` only when the cached value says full (`:93-100`); write
  pointer register `0x8594` is written once per triangle by `_START_DMA` (`:119-122`, invoked `d3ddrv.h:435`). The CPU builds
  triangle N+1 while the chip parses N. DMA enable/ring regs set at `d3dcb2.c:168-174`; a VxD hands back the buffer (`:154-160`).
- State cached in the context: `SetupPrimitive` rewrites setup registers only when `ctxt->bChanged` (`d3dpoint.c:57-67`); a
  each render-state change sets it (`dx5 0025446e_d3drendr.c:41,64,70`).
- Float to fixed with no control-word switches: add a magic double and read the low word, `d3ddrv.c:270`; x87 control word set once per
  call (round-truncate, double precision) `w98 SRC_DISPLAY_MINI_S3V_D3DCB2.C:193-201`.
- DX6-era DrawOnePrimitive: `SetupPrimitive` once per call, then one function pointer `pRenderTriangle[dwRCode]` picked from state
  code, looped over the vertex list (`w98 SRC_DISPLAY_MINI_S3V_D3DCB2.C:75-92,128-175`). Registered through
  `GUID_D3DCallbacks2` with SETRENDERTARGET, DRAWONEPRIMITIVE, DRAWONEINDEXEDPRIMITIVE, DRAWPRIMITIVES
  (`w98 SRC_DISPLAY_MINI_S3V_S3_DD32.C:5544-5562`).
- Texture handles (DX5): `myTextureCreate` mallocs a driver struct, stores the surface pointer, returns the struct address as the
  handle; no table, no pixel copy (`dx5 00254f40_d3dstate.c:20-54`). Textures stay in video memory (`D3DDEVCAPS_TEXTUREVIDEOMEMORY`).
- Caps: devCaps EXECUTESYSTEMMEMORY, TEXTUREVIDEOMEMORY, TLVERTEXSYSTEMMEMORY, FLOATTLVERTEX, DRAWPRIMTLVERTEX, SORTINCREASINGZ;
  render depths `DDBD_16|DDBD_24` (no 8 bpp), Z 16 (`dx5 0024edbc_d3dctxt.c:284-300`); `bClipping FALSE`, no transform, no lighting
  (`:296-298`). Callback table: ContextCreate/Destroy, RenderState, RenderPrimitive, four Texture*, GetState; the rest NULL (`:117-155`).

### 3.2 3Dlabs Permedia2/GLINT, DX7 (dx7 `F3###` files)

- Input FIFO: `WAIT_FOR_FREE_ENTRIES(n)` spins on `InFIFOSpace < n` (`F3286_p2hw.h:58-59`). One wait of 120 entries per
  triangle, then 120 unchecked writes (`F3282_p2d3dtri.c:44`; 40 for points and lines `F3281_p2d3dpt.c:56`, `F3280_p2d3dln.c:49`).
  The header comment says the macros can be redefined for DMA (`F3286_p2hw.h:52-55`).
- Wait for idle only at context switch between GDI, DDraw and D3D: `P2DDorD3DContextInit` returns at once when the context is
  unchanged (`F3276_p2cntxt.c:67-72`), else `vSyncWithPermedia` plus `bDrawEngineBusy` (`:75-76`). DP2 entry saves the context
  once per primitive command, not per triangle (`F3656_d3ddp2.c:769-822`, `F3256_d3dcntxt.h:392-397`).
- Poll back-off: while DMA remains, spin `count<32 ? 1 : count<<1` iterations between reads of `DMACount`
  (`F3285_p2hw.c:28-45`), so the bus is not hammered.
- Flip is queued, not awaited: `HWLoadNewFrameBaseAddress` writes `SuspendUntilFrameBlank` into the FIFO (`F3286_p2hw.h:136-140`,
  `F3269_ddsurfcb.c:210`) and returns DD_OK. Flip and Lock return `DDERR_WASSTILLDRAWING` rather than block when the previous flip or
  the engine is pending (`F3269_ddsurfcb.c:99-109,260-271`). Lock waits only if the surface is the flip target (`:255-263`); then
  `SyncHardware` (`:278`).
- Minimal DrawPrimitives2: one loop over `D3DHAL_DP2COMMAND` from `lpDDCommands` fpVidMem, vertices from `lpVertices` when
  `D3DHALDP2_USERMEMVERTICES` (`F3656_d3ddp2.c:107-170`); render opcodes first validate FVF once (`:206-233`) and rebuild dirty state
  (`:236-252`); handled: RENDERSTATE, TEXTURESTAGESTATE, VIEWPORTINFO, WINFO, point/line/triangle families, TEXBLT, STATESET,
  SETPALETTE, UPDATEPALETTE, SETRENDERTARGET, CLEAR, ZRANGE and the SET* transform tokens skipped (`:261-1700`). Unknown token:
  call the runtime's `ParseUnknownCommand`, else `D3DERR_COMMAND_UNPARSED` with `dwErrorOffset` (`:1692-1712`, `:1760-1768`).
  `d3d.doc` 4.3.2 lists SETRENDERTARGET and CLEAR as "always required", SETPALETTE/UPDATEPALETTE as no-ops without palette textures.
- Texture handles (DX7): `CreateSurfaceEx` copies surface data into a driver table indexed by `lpSurfMore->dwSurfaceHandle`
  (`F3260_d3dresrc.c:8-27,57,110-139`); DP2 tokens carry only handles. Registered through `GUID_Miscellaneous2Callbacks`
  (CREATESURFACEEX, GETDRIVERSTATE, DESTROYDDLOCAL) (`F3268_ddinit.c:522-541`).
- Caps: `D3DDEVCAPS_DRAWPRIMITIVES2`, `DRAWPRIMITIVES2EX`, FLOATTLVERTEX, SORTINCREASINGZ, SORTEXACT, TLVERTEXSYSTEMMEMORY,
  EXECUTESYSTEMMEMORY, TEXTUREVIDEOMEMORY, DRAWPRIMTLVERTEX (`F3265_d3dutil.c:96-106,382`); `GUID_D3DCallbacks3` returns
  DrawPrimitives2 + ValidateTextureStageState only (`F3268_ddinit.c:346-362`); extcaps `dwFVFCaps = 1`, one blend stage, one
  simultaneous texture, textures 1..2048 (`:365-410`); `dwNumVertices = 0` "we don't parse execute buffers"
  (`F3253_d3d.c:56`). Not set: `CANRENDERAFTERFLIP`. The D3D HAL is disabled at 8 bpp (`F3253_d3d.c:46-52`), as in ViRGE.
- Vertex/command buffer cycling: `d3d.doc` 4.7.1 says a driver may allocate N vertex and command buffers, Direct3D fills one
  while the hardware renders from the others, "very useful with DMA", swap on `D3DHALDP2_SWAPVERTEXBUFFER/SWAPCOMMANDBUFFER`.
  Same section: copy the parsing loop from the Permedia2 sample.

### 3.3 XGA mini (w98 `SRC_DISPLAY_MINI_XGA_*`), the closest 2D sample to an 8514-class engine

`MakeHardwareNotBusy` spins on the coprocessor busy bit (`XGA.INC:78-82`) and is called at the start of each blit setup, before
programming registers (`XGABLT.ASM:524,623,703,715,762,776,824`), not after starting one. A software BUSY flag on the PDevice
blocks the DIB engine's cursor draw during an accelerated blit (`XGABLT.ASM:205-209,296-301`). Absent: any command queue.
`vdd\ati\ATI.ASM` (4646 lines) is a virtual-display VDD for Mach32/64 mode switching, no drawing.

### 3.4 Not found

- Any sample that batches a whole frame and kicks once: ViRGE kicks per triangle, Permedia2 per FIFO fill.
- A sample with a software depth sort at flip, or a driver for an engine with no triangle hardware.
- Win95-era 8514/Mach sample, `d3dref` driver-level FIFO use (refrast is a software rasteriser, `F3790_refrast.hpp`).

## Owner's list, checked

The list came from a Google AI answer; each line checked against files, not against the answer.

- "Fox" (DX5/6 sample): exists only as The Fox & The Bear, `foxbear`, in the DX3a SDK (`SDK/SAMPLES/FOXBEAR`, README "The Fox & The
  Bear Sample") and the DX5 SDK (`cdrom/sdk/samples/foxbear`). Sources are `ddraw.c`, `bmp.c`, `fbsound.c`; no Direct3D reference
  (grep). It is not in the DX6.1 samples. A 2D DirectDraw/DirectSound side-scroller: useful for DirectDraw only, not a 3D bench.
- Rockem (DX3/5, "untextured low-poly"): ships in DX3a (`ROCKEM/`, `SDK/SAMPLES/ROCKEM`) and DX5. Retained Mode (`RM.CPP` holds
  `LPDIRECT3DRMFRAME` scene/camera/arena). Its readme says it "is not optimised for raw speed". The folder holds 23 `.PPM` bitmaps
  (e.g. `DEMECHBK.PPM`) plus 5 `.X` meshes, so the "untextured" claim is not supported. Triangle count: not confirmed. Kept as a
  Retained-Mode game test, not ranked.
- Tunnel / TunnelDP (DX6.1): already ours; `tunneldp` and `tunneleb` are 960 triangles, Z off (part 2).
- Hellbender demo (1996, DX2): no demo found on archive.org (title search returns only music/video items; `012230` is a 9 MB
  video). Only evidence is the existing `d3d_benchmarks_and_games.md` entry (trial readme at discmaster,
  `.../1996-10_Disc_2.9.iso/hellbend/readme.doc/readme.pdf`, text not extractable here). HAL vs software, RAM: not confirmed.
- Monster Truck Madness trial (Aug 1996): local `references/d3d_programs/mtmtrial.exe` (12 MB, 169 files). Contains `MONSTERX.EXE`
  with strings `useDirect3D`, `required Direct3D features:`, `maxD3DTextures`, `useMMXFlag`, and the INF installs the DX2-era
  `d3dim.dll`, `d3dhalf.dll`, `d3drampf.dll`, `d3drg8f/16f/24f/32f.dll`, `d3drm.dll`. So it selects a Direct3D device by
  features and can use a HAL. Whether a non-Direct3D software path exists: not confirmed (the exe also carries
  `Software\Microsoft\Hellbender\Services`, a shared engine key). RAM need: not stated in `MONSTER.TXT` (licence only) or the INF;
  the 32 MB figure in the earlier doc is the retail recommendation. Archive.org holds only retail images
  (https://archive.org/details/monster-truck-madness_202201, 232 MB). With a 1 MB card and ~5 MB RAM it is a long shot; not run.
- Pouet Direct3D intros 1997-99: pouet's API (https://api.pouet.net/v1/prod/?id=N) and prod pages give platform and date, not
  graphics API, polygon style or RAM. Nothing below is confirmed as D3D with flat/Gouraud polygons at 640x480x8/16 in ~5 MB.
  Windows-only 1998 candidates whose scene.org packages could be read for an NFO: https://www.pouet.net/prod.php?which=3141
  (Agroo, Obscure, 1998-01), `?which=875` (Startrail to Glory, Abyss, 1998-04), `?which=5493` (Planet Cornball, Komplex, 1998-04),
  `?which=502` (photon, Purple, 64k, 1998-04, Windows+DOS), `?which=531` (minimal, Radium Software, listed 1998, upload 2000).
  API path: not confirmed for all five. Pouet's date filter is unreliable (several 1998 rows show month 00).
- fr-08 (https://www.pouet.net/prod.php?which=1221, 2000-12-15, outside the 1997-99 range, 64k): farbrausch member ryg writes in the
  comments that it uses "plain d3d for polygon drawing". DirectX version not stated by the authors (a commenter guesses DX8).
  A critic says it is slow on a GeForce DDR with 256 MB; one commenter says it runs on a Voodoo card. It generates textures and
  meshes at start-up from a 64 KB executable; memory use is not stated. A realtime textured, lit, many-object scene aimed at 2000
  hardware: too heavy for 5 MB and 1 MB VRAM (judgement, not measured).

Effect on ranking: none beat an existing entry. Foxbear is 2D; Rockem is textured Retained Mode; the retail demos and intros are
unconfirmed or unrunnable on this machine.

## 4. Top 8 bench candidates

1. flipcube.exe, DX3a `SDK/BIN` (also DX2): 12 triangles, flat, Z off, no texture, fps/tps on screen. The floor: if this is not 60 fps, nothing else will be.
2. tunnel.exe, DX3a or DX5 `bin`: 320 triangles, Z off, textured; the same scene as the current bench at one third the load (DX6.1 = 960).
3. twist.exe, DX3a or DX5 `bin`: ~250-270 triangles (estimate), no texture, Z off by default; transform work is in the runtime.
4. flip3dtl.exe, DX5 `bin`: DrawPrimitive with D3DTLVERTEX only (runtime does no transform), textured cube, full screen only.
5. tunneldp.exe + tunneleb.exe, DX6.1 `d3dim/bin`: identical 960-triangle scene on DrawIndexedPrimitive and execute buffers; Z off; isolates API-path cost.
6. lights.exe, DX6.1/DX7: ~750 lit textured triangles, Z off, runtime lighting; tests Gouraud.
7. d3dtest.exe, DX2/DX3a `bin`: Microsoft's own polygon-throughput and fill-rate tests; needs texture formats, run on the bed first.
8. PC Player D3D Benchmark 1.10 / Gamestar 1.00, archive.org: 1997 period benchmarks for 1-4 MB cards, fps to `pcpd3dbn.txt`; textured, fog, option set unverified.

Below the line: boids, pplane, bend, fog, vbuffer (Z on, 1 MB VRAM cost), fireworks, flare, billboard (alpha), mtexture (2 stages).

## 5. FIFO-feeding lessons, cheapest first

1. Poll with back-off, not flat out (Permedia2 `F3285_p2hw.c:28-45`): delay about as long as the remaining work between status reads; cuts ISA status-read cycles (5.7 us per byte I/O) while the engine draws, no RAM cost.
2. Wait for N free slots before each register group, never for empty (ViRGE `s3data.h:13-17`, `d3ddrv.h:147-414`): the CPU overlaps setup with drawing; costs one status read per group, saves the idle FIFO seen now.
3. Choose the per-triangle routine once per DrawPrimitive from a state code and rewrite setup registers only when state changed (`d3dpoint.c:57-67`; w98 `D3DCB2.C:75-92`): fewer OUTs on the bus and fewer branches per triangle on the CPU.
4. Set the x87 control word once per call and convert float to int with the magic-double add (`w98 D3DCB2.C:193-201`, `d3ddrv.c:270`): removes per-vertex FLDCW/FIST sequences on the 387 (2.97 us a multiply-add).
5. Queue the flip in the command stream and return `DDERR_WASSTILLDRAWING` from Lock/Flip only when pending (Permedia2 `F3269_ddsurfcb.c:99-109,210,255-263`): the CPU starts frame N+1 during N's page wait; no extra RAM.
6. Ring of command buffers with a cached read pointer re-read only when full, write pointer published once per triangle (ViRGE `s3data.h:88-122`); we have no DMA, so the nearest form is a 4 KB RAM ring of register writes drained by a VxD-timed loop: costs 4 KB, moves bus time off the draw path only if the drain runs in idle CPU time (unproven on ISA).
