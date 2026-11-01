# DirectDraw on Windows 95: research for the Mach8 driver (2026-11-01)

Desk research only; no code changed, nothing run on the card. Items marked UNVERIFIED were
not confirmed from a primary source.

## 1. How a Win95 display minidriver exposes DirectDraw

The best primary evidence we hold is working code, not Microsoft prose: `vmdisp9x` (16-bit) plus
`vmhal9x` (32-bit), both local, MIT. The Win95 DDK we hold contains no DirectDraw material
(see section 3).

**Two halves.** (from `vmdisp9x/control.c`, `vmdisp9x/dddrv.c`, `vmhal9x/README.md`)
- 16-bit minidriver: answers the `Control` escape `DCICOMMAND` (3075). Our `enable.c:412` returns 0
  for it today, which is what tells DDRAW there is no HAL.
- 32-bit HAL DLL: **yes, needed in practice.** DDRAW is 32-bit and the callbacks (`Lock`, `Blt`,
  `Flip`...) are called 32-bit. vmhal9x README: the driver is 16-bit, so "it needs to call 32 bit
  code"; the HAL "could in theory" be 16-bit, but a screen over 64 KB cannot be mapped in one
  16-bit segment. The 32-bit DLL must load in the shared arena (0x80000000-0xBFFFFFFF) with code
  and data sections flagged `IMAGE_SCN_MEM_SHARED` (vmhal9x ships a `makeshared` tool for this).
  It stays loaded until shutdown.

**Discovery handshake** (`vmdisp9x/control.c` ~l.247-330; `DCICMD.dwVersion == DD_VERSION`):
| dwCommand | Driver does |
|---|---|
| `DDCREATEDRIVEROBJECT` (10) | build `DDHALINFO`, return the HAL `hInstance` |
| `DDGET32BITDRIVERNAME` | return `{ "vmhal9x.dll", "DriverInit", context }` (`dddrv.c` l.38-48, 625-640) |
| `DDNEWCALLBACKFNS` | DDRAW hands over its `lpSetInfo` callback |
| `DDVERSIONINFO` | return `DD_RUNTIME_VERSION` |

A call with a non-DD `dwVersion` is passed to `DIB_Control` (comment: copied from the S3 driver;
DDRAW otherwise fails under emulators).

**DDHALINFO** (`vmdisp9x/dddrv.c` l.200-330; layout also at
<https://learn.microsoft.com/en-us/previous-versions/windows/embedded/ee486054(v=winembedded.80)>,
which is the Windows CE variant and lists `lpDDCallbacks`, `lpDDSurfaceCallbacks`,
`lpDDPaletteCallbacks`, `GetDriverInfo`, `ddCaps`, `ddHelCaps`, `lpdwFourCC`):
- `lpPDevice`, `hInstance`, `dwModeIndex`.
- `vmiData`: `fpPrimary`, `dwDisplayWidth/Height`, `lDisplayPitch`, `ddpfDisplay` (pixel format),
  `pvmList`/`dwNumHeaps` (off-screen heaps: `VIDMEM` with `fpStart`/`fpEnd`, `VIDMEM_ISLINEAR`).
- `ddCaps` (`DDCAPS_GDI | BLT | BLTCOLORFILL | COLORKEY | CANBLTSYSMEM ...`, `dwCKeyCaps`,
  `dwFXCaps`, `ddsCaps.dwCaps`), `ddHelCaps`.
- `dwFlags`: `DDHALINFO_ISPRIMARYDISPLAY` 1, `MODEXILLEGAL` 2, `GETDRIVERINFOSET` 4,
  `GETDRIVERINFO2` 8 (`vmhal9x/vmhal9x.c` l.40-58).

**Callbacks vmhal9x registers** (`vmhal9x.c` l.250-262): `CreateSurface`, `DestroySurface`,
`CanCreateSurface`, `Flip`, `GetFlipStatus`, `Lock`, `Unlock`, `WaitForVerticalBlank`, `Blt`,
`GetBltStatus`, `SetExclusiveMode`, `SetColorKey`, `AddAttachedSurface`. This is a working set,
not the Microsoft-mandated minimum. **Which are strictly mandatory: UNVERIFIED.** DDRAW falls back
to the HEL for any callback that returns `DDHAL_DRIVER_NOTHANDLED` or is left NULL (vmhal9x README
"HAL vs. HEL": applications often refuse to run if the HAL is absent, so a HAL is worth having
even if thin).

**Primary surface.** vmhal9x sets `fpPrimary` to a flat linear address in VRAM and its `Lock32`
returns `DDHAL_DRIVER_NOTHANDLED` ("let the lock processed"), i.e. DDRAW itself uses
`lpGbl->fpVidMem`. So the stock path assumes a CPU-addressable linear surface. Nothing local says
what DDRAW does with `fpPrimary == 0` and a HAL that handles Lock itself (see section 4).

**Banked cards / VFLATD.** `Windows95_ddk/DOCS/DESGUIDE/DISPLAY.DOC`: VFLATD "provides support for
bank-switched frame buffers larger than 64K"; the driver registers the frame buffer with it and
supplies a bank-switch code template (sample: `DISPLAY/SAMPLES/MINI/FRAMEBUF/VGA.ASM`, VxD id
11Fh, `GetScreenSelector`); it returns a selector stored in `deBits`. DIBENGINE flags
`BANKEDVRAM` 0x4000 / `BANKEDSCAN` 0x2000 / `NON64KBANK` 0x80 mark VFLATD-simulated surfaces.
VFLATD gives a **16-bit selector** emulation by page-faulting; whether it gives a 32-bit linear
address usable as DirectDraw `fpVidMem` for a bank-switched card is UNVERIFIED (no DDK doc or
sample found; vmdisp9x has no VFLATD references).

## 2. vmdisp9x / vmhal9x / boxv9x

- <https://github.com/JHRobotics/vmdisp9x>, **MIT** (LICENSE in repo, (c) 2022 JHRobotics). Built from
  Michal Necasek's VirtualBox driver; builds with Open Watcom 1.9 `wmake`. README: "DirectDraw is
  supported, Ring-3 driver is in separated project VMHal9x"; Direct3D in development. Gave: confirms
  the 16-bit + 32-bit split and licence. Did not give: any banked/non-linear case.
- VMHal9x (shipped in vmdisp9x releases; source local in `references/jhrobotics/vmhal9x`): **MIT**.
  `ddraw.c` (surfaces, Lock/Unlock, Blt), `flip32.c`, `blt32.c`, `transblt.c` (colour-key blt),
  `fill.c`, `vmhal9x.c` (entry/`DriverInit`, callback table), `d3d*.c`/`mesa3d*.c` (3D, ignore),
  `vmsetup.c`, `vmhal9x.h`, `ddrawi_ddk.h`, `ddsurface.h`.
  README "Limitation": DirectDraw needs (1) linear frame-buffer access, (2) HW cursor, (3) movable
  visible frame-buffer position (flip), (4) beam timing; "all other action can be done by
  software"; flip on VMware is emulated by copying the surface to the visible buffer.
  This is the only source we found that states what the HAL minimally needs from hardware.
- **A 486 variant exists:** `vmdisp9x/dddrv.c` picks `vmhal486.dll` over `vmhal9x.dll` when
  `drv_is_p2()` is false. Minimum 486/16 MB per the repo page. Source for `vmhal486` not located
  in our copy (UNVERIFIED how it differs).
- **boxv9x** <https://github.com/phkelley/boxv9x> (Philip Kelley's fork of Necasek's driver; the
  URL named in vmdisp9x). I did not reach it (a guessed other owner returned 404). Per the
  vmdisp9x page, the base driver is Necasek's. **boxv9x DirectDraw support: UNVERIFIED**, and
  unlikely - vmdisp9x's own README says DirectDraw is in the separate project.
- Local snapshots: `references/jhrobotics/{vmdisp9x,vmhal9x,softgpu,patcher9x,fixlink}`. Our
  `thunks.asm` credits Necasek (MIT, `LICENSE-MIT.txt`); vmhal9x/vmdisp9x are MIT too, so code can
  be reused with notice. `references/velocity9x` is GPL-3.0 (`docs/mach8_driver_design.md:68`): do
  not copy from it.

## 3. Local material

Win95 DDK: `C:\Users\lycet\OneDrive\Desktop\XT_project\Windows95_ddk` (not in repo; docs call it
`XT_project/Windows95_ddk`). Findings:
- **No DirectDraw headers or docs.** No `ddraw.h`, `ddrawi.h`, `ddhal.h`; `DOCS/DDPR.HLP` has no
  hit for DirectDraw in a text grep (compiled help, so weak evidence). DirectDraw ships with
  the separate DirectX SDK/DDK, which we do not hold. vmhal9x carries its own `ddrawi_ddk.h`,
  `ddsurface.h`, and vmdisp9x's `dddrv.c` includes `ddrawi.h` (from `ddk/` in that repo).
- **DCI only:** `INC16/DCIDDI.H` (`DCICOMMAND` 3075, `DCI_VERSION` 0x100,
  `DCICREATEPRIMARYSURFACE` 1, `DCICREATEOFFSCREENSURFACE` 2, `DCICREATEOVERLAYSURFACE` 3,
  `DCIENUMSURFACE` 4, `DCIESCAPE` 5; `DCI_PRIMARY/OFFSCREEN/OVERLAY`, `DCI_CHROMAKEY`,
  `DCI_WRITEONLY`, `DCI_ASYNC`, `DCI_CAN_STRETCH*`), `INC16/DCIMAN.H`, `LIB/DCIMAN.LIB`. Gave:
  the DCI escape numbering that DirectDraw reuses. Did not give: DirectDraw HAL structures.
- `DOCS/DESGUIDE/DISPLAY.DOC` (binary Word file; read with a Python filter): `NOT_FRAMEBUFFER`
  0x20 "Example: 8514/a or ATIM8"; `BANKEDVRAM`, `OFFSCREEN` 0x0008 "use with VRAM", VFLATD text above.
- Samples: `DISPLAY/SAMPLES/MINI/{FRAMEBUF,MINI,XGA}`, `VDD/{ATI,CIRRUS,S3,TSENG}`. None implement DCI
  or DD (grep for DCI in samples hit only a Cirrus register comment).
- Our `src/ddk/` holds only `dibeng.h`, `dibeng.lbc`, `gdidefs.h`, `valmode.h`.
- `references/xga-win9x` (holzachr, XGA 2 MB DDK mini sample): DIB-engine accelerated minidriver,
  no DirectDraw. `references/velocity9x`: Win9x driver (GPL-3.0), design reference only.
- Our current state: `enable.c:412` `Control` returns 0 for `DCICOMMAND` and for
  `QUERYESCSUPPORT(DCICOMMAND)`, so DDRAW sees no HAL and uses a pure HEL path.

Repo docs already holding the plan:
- `docs/worklist.md` D5 (l.230-246): Mach8 HAL first, VGA half second, both selectable; Lock may
  hand back a system-RAM shadow of the locked rectangle; "Desk first: Win95 DDK DDHAL lock/flip/blt
  callbacks" - this doc is that desk pass, with the DDK gap above.
- `docs/mach8_driver_design.md` l.49-52, 98-100, 155-199, 562-575: the off-screen allocator reuse,
  VGA-half VFLATD surface, CRT_OFFSET double-buffering at 640x480.
- `docs/mach8_levers.md:20`: colour compare 92EE/EAEE for colour keys.
- `docs/mach8_unused_silicon_audit.md:18`: GE/CRT offset and pitch registers; "page flip not
  card-probed"; **ADVFUNC/MEM_CNTL write resets pitch/offset** (`mach8_card_coverage.md:22,42`);
  VGA half cannot flip at all.
- `docs/mach8_card_coverage.md:67-69`: mixes and COLOR_CMP verified on the card.
- `docs/mach8_graphics_vision.md:89`: DCI/DirectDraw listed as "later, possibly".

## 4. DirectDraw with a surface the CPU cannot address

No source found that states a "sanctioned" non-lockable primary. What the evidence supports:
- DDRAW calls the HAL `Lock` for every `IDirectDrawSurface::Lock`. Microsoft's DDK text for
  `DdLock` (NT-era, same model): the driver writes the surface pointer into
  `lpSurfData`, and returns `DDHAL_DRIVER_HANDLED`; if a blit/flip is in progress it sets `ddRVal =
  DDERR_WASSTILLDRAWING`. Sources:
  <https://learn.microsoft.com/en-us/previous-versions/windows/hardware/drivers/ff568216(v=vs.85)>,
  <https://learn.microsoft.com/en-us/windows/win32/api/ddrawint/ns-ddrawint-dd_lockdata>. These
  are the NT DDI, not Win95; Win95 `DDHAL_LOCKDATA` has the same shape (`lpSurfData`, `ddRVal`,
  `bHasRect`/`rArea`, `dwFlags`) per vmhal9x's use. They do not say `lpSurfData` may point at a
  shadow buffer, but nothing forbids it. A HAL-returned shadow copy is the design in `worklist.md` D5.
- `vmhal9x` `Lock32`/`Unlock32` (`ddraw.c` l.470-): calls `FBHDA_access_begin/end` around the
  default lock and returns NOTHANDLED, i.e. it is a "read-back before CPU access" hook - the same
  idea as copying the rectangle to RAM, but applied to a flat surface vmhal9x can already see.
  Gives the hook point; does not do a rectangle shadow.
- Caps that matter (names from the DirectDraw docs; use in a HAL not shown by any local source):
  `DDCAPS_NOHARDWARE`, `DDCAPS_CANBLTSYSMEM`, `DDCAPS_BLTSTRETCH`, `ddsCaps` with
  `DDSCAPS_PRIMARYSURFACE`/`FLIP`/`OFFSCREENPLAIN`/`VIDEOMEMORY`/`SYSTEMMEMORY`. `vmdisp9x` sets
  `DDCAPS_GDI` (HW shared with GDI), `BLT`, `BLTCOLORFILL`, `COLORKEY`, `CANBLTSYSMEM`.
  **No DDCAPS bit says "surface not lockable"** was found. UNVERIFIED either way.
- **UNVERIFIED and critical:** whether DDRAW 32-bit code (`DDRAW.DLL`, `DDHEL.DLL`) tolerates
  `fpPrimary == NULL`/an `fpVidMem` that is not a real linear address, and whether `CreateSurface`
  may supply a driver-chosen `fpVidMem`. The generic DDRAW surface code gives the app the address
  from `Lock`'s `lpSurfData`, so a HAL that handles Lock and returns `DDHAL_DRIVER_HANDLED` should
  work for lock-based access; `fpPrimary`'s role elsewhere (HEL blits to the primary, mode set) is
  unknown. Cheapest test: a minimal HAL with `fpPrimary` = a system buffer and a Lock that
  uploads on Unlock, then run `dxdiag` and a one-surface test app.
- HEL: DDRAW's software emulation draws through `fpVidMem`/Lock, so with a shadow Lock the HEL
  works against the shadow and our `Unlock` must push dirty rectangles to the card.
  README for vmhal9x: "HEL is able to completely and relatively fast emulate all HAL behaviour";
  apps often refuse to run without HAL, so we need a HAL even if most callbacks decline.

Practical consequence: the Mach8 half is a **DCI/DDraw with shadow Lock** target; VGA half
(64 KB banked, via VFLATD) is the only half where `fpVidMem` could be a real address, and even
there bank switching limits frame size (800x600x8 = 480 KB > 64 KB bank).

## 5. Display start / flip registers (references/ati_mach32/guide.txt)

- `CRT_OFFSET_LO` 2AEE (bits 15:0) and `CRT_OFFSET_HI` 2EEE (bits 3:0): "20-bit linear address
  offset... which points to the start of the CRT display buffer. Specified in bytes/4" (l.9418,
  9582-9601). `CRT_PITCH` 26EE: logical width. Index lines 13071-13073. Sec. 9 intro: "the physical
  start of screen memory".
- Caveat l.7515-7523 (ADVFUNC_CNTL note 3): "Only mach8 will reset CRT_PITCH, GE_PITCH,
  CRT_OFFSET, and GE_OFFSET registers when ADFUNC_CNTL is written, not mach32." So on Mach8 a
  write to ADVFUNC_CNTL clears the flip. This is a Mach32 manual; Mach8 applicability of the
  registers themselves is per `docs/mach8_unused_silicon_audit.md:18` "documented, model ok; not
  card-probed". The guide marks the 2AEE/2EEE entry for both "Mach8" and "Mach 32" columns, but
  column text is OCR-damaged: UNVERIFIED on a real Mach8 card.
- Off-screen: l.6895-7013: non-conforming blit (`DEST_Y_END` AEEE etc.) "for efficient off-screen
  memory management"; polygon fill uses off-screen work area. No back-page-draw spec beyond
  `GE_OFFSET` (6EEE/72EE) and `GE_PITCH` (76EE).
- Colour compare: `DEST_CMP_FN` EEEE (l.11409-11437; Mach32). Mach8 source compare 92EE/EAEE is
  not in this guide (`mach8_levers.md:20`); `COLOR_CMP` B2E8 verified on the card.
- No "DISP_START" or flip entry in the guide; the term to use is CRT_OFFSET. VBLANK sync is via
  the raster counter (`mach8_driver_design.md:~155`, A8/A9) - not rechecked here.

## Open questions for the next step

1. Does DDRAW 32-bit code accept a primary with a shadow `fpVidMem` and Lock `HANDLED`? (Test with a minimal HAL.)
2. Which callbacks may be left NULL? Test with `dxdiag` and a one-flip app.
3. One card probe: `CRT_PITCH` + `CRT_OFFSET` flip at VBLANK (worklist D5).
4. Obtain the DirectX DDK / `ddrawi.h` of the Win95-era (DirectX 3-5): none held locally; vmhal9x's
   `ddrawi_ddk.h` is a stand-in.
5. Is a 486 HAL (`vmhal486`) buildable from sources? Not found in our copy.
