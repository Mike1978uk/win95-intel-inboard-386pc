# PowerVR Series 1 SGL: reading, 2026-11-19

Source: https://github.com/powervr-graphics/PowerVR-Series1 (MIT, "reference, no guarantee it compiles").
Local clone, outside this repo: `C:\Users\lycet\RiderProjects\references_ext\PowerVR-Series1`.
Paths below are inside that clone. Read only; nothing built or run. Docs are PDFs and zipped
Word files under `Documentation/sgl/Docs/files/`; the `.htm` pages are only link stubs (11-17 note
was right that they hold no body text). Feeds `docs/playable_3d_plan.md` P5.6.

## Premise to correct

SGL does not do hidden-surface removal without a Z buffer. The chip does **per-pixel depth
sorting on chip, tile by tile**; the app just never owns a Z buffer.
- `D3Dguide.zip` > `D3DGUIDE.DOC` ("Using the Z buffer"): HSR "is always active and Z sorting is done on a
  per-pixel basis"; the HAL declares a dummy Z surface with no memory; Z cannot be turned off.
- Same doc, "Overview": "on-chip hidden surface removal using a tiling technique ... requires the whole 3D
  scene to be rendered in a single pass" (hence BeginScene/EndScene, `DDCAPS2_NO2DDURING3DSCENE`).
- `PowerSGL.pdf` 5.1.1: equal-depth faces: the one later in the list wins (painter's tie-break only).
- `PowerSGL.pdf` 17.3.5: "not necessary to sort the polygons in z order"; depth comes from `fInvW`.
So it is the closest thing to an order-independent scene, but the order independence is hardware, not an
algorithm we can lift. Only translucency is sorted in software (section 3).

## 1. Programming model

Two APIs in one library, sharing device and texture management (`PowerSGL.pdf` 1.1):
- **PowerSGL (retained)**: devices, viewports, cameras, lights, display lists with transforms, instancing,
  convex polyhedra defined by plane equations, polygon meshes, LOD, shadows, fog, collision. The game builds
  a scene graph; per frame `sgl_render(viewport_or_device, camera_or_list, swap_buffers)` (ch. 12).
- **PowerSGL Direct (immediate, screen space)**: "equivalent to Microsoft D3D and Apple RAVE" (17.1). The
  game transforms, lights and projects itself and re-submits every triangle every frame.
  Frame: `sgl_create_screen_device(dev, x, y, mode, double_buffer)` once, then
  `sgltri_startofframe(ctx)`, any number of `sgltri_triangles(ctx, n, faces[][3], verts)` /
  `sgltri_quads(...)`, then `sgltri_render(ctx)` (`SGLDirect.pdf` 4.4). `sgltri_isrendercomplete` polls.
  Example: `Documentation/sgl/QuickStart/f/triangex1.c`.
- **SGL-Lite** is not a third API. It is the name for the cut-down triangle path inside the Direct/D3D core:
  `Source/dcore.h:77`, `Source/d3dcore.h:102` ("processes both D3D and SGL-Lite triangles, also translucent"),
  `Source/d3dreg.c:2188, 2554, 3901`, `Source/win32/sgllite.def`.

Per-frame data (`SGLCONTEXT`, `PowerSGL.pdf` 17.3.4, also `Source/sgl.h`):
- Material per batch, not per vertex: `u32Flags` (`SGLTT_GOURAUD`, `_TEXTURE`, `_HIGHLIGHT`, `_GLOBALTRANS`,
  `_VERTEXTRANS`...), `nTextureName`, fog on/colour/density (0..31), `bCullBackfacing`, `bDoClipping`,
  background colour, shadow / light-volume mode, `u32GlobalTrans`.
- Vertex `SGLVERTEX` = D3DTLVERTEX layout: `fX fY fZ(dummy) fInvW`, fog RGB, `u32Colour`, `u32Specular`,
  `fU/VOverW`. X/Y up to 1024. Z-clip is mandatory, XY clip optional.
- Translucency: constant per batch (`u32GlobalTrans`) or per-pixel in a 4444 texture. Hardware keeps only the
  nearest surface per pixel per pass, so each `sgltri_triangles` call is one pass ordered by
  `fTranslucentPassDepth` (larger = nearer). In retained SGL `sgl_new_translucent()` starts a pass
  (`PowerSGL.pdf` 8.1.6, 8.2.8). Smooth shading plus translucency on one polygon is unsupported.
- Direct hands whole batches (arrays of faces plus a shared vertex array), not single polygons.

## 2. DOS driver and binding

Yes, a DOS build exists: `Source/dos32/` (about 8.5k lines C/asm).
- Target `OS=DOS32`, Watcom, DOS4GW 32-bit protected mode: `Source/dos32/pvrosapi.c:3-11`,
  `Source/pcmake/pcmake.mak:156,274`, `Source/pcmake/Makefile:308-311` (`DOS32_SRC`).
- **Binding is a static library linked into the game**: `Source/pcmake/dostgt.dat` runs `WLIB` to make
  `SGLMID*.lib` with `pmode.lib` and `uvbelib.lib` merged in. No TSR, overlay, or DLL. The game links
  `sgl_*`/`sgltri_*` directly.
- Talks to the card itself: PCI BIOS scan (`pci.c`), VDS for DMA-safe buffers (`vds.c`), IRQ hook
  (`irq.c`, `isr.asm`), config from `sglhw.ini` (`brdcfg.c:68`). A VESA 2.0 (UniVBE) mode is set on the
  *host* VGA; the PCX chip is told its framebuffer address/stride/format and renders over PCI into it
  (`vesa.c`, `univbe/README.md`; PCX1/PCX2 are add-on cards that need a host SVGA).
- Only game named anywhere: the **Tomb Raider** SGL port (root `README.md`, `dos32/univbe/README.md`).
  Neither names other titles.
- Colour depth: `sgl_device_16bit` / `sgl_device_24bit` only (`Source/sgl.h:211`). No 8 bpp device.
- Software fallback: `Source/simulat/`, `simulat2/`, `simulat3/` are hardware *simulators* (register-level
  chip models), not a game-speed renderer.

## 3. Mapping onto m8dos

Maps:
- Direct's frame bracket (`startofframe` .. `render`) is our `m8_frame_begin` / `m8_frame_end`; a whole scene
  is handed over at once, as we already queue it.
- Flat-shaded, untextured, opaque triangles and quads: `m8_tri` / `m8_poly` (convex, up to 16), back-face
  cull, `bDoClipping`. SGL itself has quads and larger polygons ("coplanar triangles can be combined",
  `perform.doc`), matching `m8_poly`.
- Fog by distance is a palette/colour pick per face at best; SGL's is per-pixel (`fogtable.h`). Not comparable.
- SGL's no-order tie rule ("later wins") is our painter's order for coplanar faces.

Does not map (no engine path, or no memory):
- Textures (8/16-bit, MIP), bilinear: no texture hardware on the Mach8, 640x480x8.
- Gouraud, specular highlights: flat fills only.
- 16/24 bpp: ours is 8 bpp palette.
- Translucency: needs a read-modify-write of the frame buffer; the Mach8 engine has no blend. 50/50 stipple or
  a palette-pair trick is the only candidate, not in SGL.
- Shadow and light volumes, per-pixel depth, per-pixel fog: chip features.

Where SGL's sorting could replace ours: it cannot replace the opaque sort (chip depth). The reusable part is
the software translucent sort: `Source/d3dtsort.c` (file header, "Takes all the translucent triangles for a
region and sorts them by building a Directed Acyclic Graph, giving a correct Z ordering and minimum
number of passes"; functions `AllIntersects`, `LineSplitsVertices`, `CreatePasses`) and `dregion.c`
`SortTransFaceLists` (line 2957) / `NoSortTransFaceLists` (3055). A DAG "A must be behind B" topological order
is a correct painter's order for any non-cyclic set, per screen region, which is what a depth sort by
centroid gets wrong. Cost: O(n^2) overlap tests per region; written for a Pentium-class CPU
(unrolled min/max in the 1997 log), likely too slow on an 83 MHz 486 for more than a few hundred triangles
unless limited to overlapping neighbours. Candidate for our sorter, not measured.

## 4. Reuse for a DX3 immediate-mode D3D DLL

Useful as a worked map of what a game sends:
- `D3Dguide.zip` > `D3DGUIDE.DOC` documents a real D3D **HAL** (`pvrhal32.dll`, secondary HAL via the
  `HKLM\...\DirectDraw\Secondary` key) with its caps: `D3DDEVCAPS_SORT*` false, `dwZCmpCaps` =
  `LESSEQUAL` only, blend caps `SRCALPHA/INVSRCALPHA`, textures 5551 and 4444, BeginScene/EndScene one per
  frame, execute-buffer ordering (opaque first, translucent last, blend state per batch). The caps table is
  a ready checklist for what to export from our DLL. Appendix 1 of that doc lists them in full.
- `Source/d3dtri.c`: `DirectD3DPolygons(TexHeap, ctx, nFaces, faces[][3], verts, bFullSort)` is the D3D
  DrawPrimitive entry mapped onto Direct, with indexed triangle lists as the unit. Shows how a
  D3DTLVERTEX array becomes batches (`bDoUVTimesInvW`, `Source/pvrd.c` `PVRDTrianglesPrimitive`).
- `Source/pvrd.c` / `pvrd.h`: PVRDirect entry points (`PVRDCreateRenderContext` ...), the layer the D3D HAL
  DLL calls. Shows how little state D3D needs once vertices are screen-space.
- Windows-only: `Source/win32/pvrddll.c`, `d3dheap.c`.
Not directly reusable: DX3 on a DOS extender is an *application-side* DLL (no HAL, no ddraw
driver), and the code here is the **HAL-side** consumer of D3D, written against DirectX 5 DDI. No `D3DHAL*`
structure or DDI callback table appears anywhere in the tree.

## Not found

- Any DOS game list other than Tomb Raider; no DOS sample programs (all `Examples/` zips are Win32 SGL, SGLDirect, D3D).
- The HAL DLL source itself (`pvrhal32.dll`, DDI glue). Only the library under it.
- How the host program talks to the PCX registers: register map is in `Source/dos32/hwdevice.c` /
  `Source/hwinterf.h` / `Source/simulat/` (not read in depth); no public chip spec.
- An 8 bpp or palette mode. No benchmark of a 486 host. `univbe` libs are removed for licence reasons, so
  `dos32/vesa.c` does not link as shipped.
- Performance notes (`perform.doc`): only the 1997 benchmark-rules text; nothing on CPU load per triangle.
- Whether `SGLMID*.lib` was ever shipped as a DOS SDK: only the source tree shows the target exists.
- `.hlp` and `.zip` samples, `3DS2SGL` and other tools were listed, not opened.
