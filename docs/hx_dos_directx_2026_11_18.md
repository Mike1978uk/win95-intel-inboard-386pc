# HX DOS Extender: Win95 DirectX games under plain DOS (desk research, 2026-11-18)

Nothing run, nothing installed. All fetches 2026-11-18. "Did not contain" is per source.

## Verdict

| Question | Answer |
|---|---|
| Memory blocker (HIMEM 0 KB XMS) | Not a blocker on paper. HDPMI has an INT 15h/raw mode used when no XMS host is found: E801h, else AH=88h. Needs >= 72 KB extended. `HDPMI=2` adds conventional DOS memory to the page pool. **But** our 1986 BIOS has no INT 15h AH=88h (`tools/dosx/memprobe.c`, `docs/dos_driver_groundwork_2026_11_18.md` s1), and how DOS/32A and PMODE/W found 4.2 MB is undetermined. Run `memprobe` logic against HDPMI32 first; if E801h/88h both fail, try `HDPMI32 -a` with `HDPMI=2` (640K only: too small for DDRAW games). |
| DDRAW | Yes: IDirectDraw/2/4/7, Surface 1-4/7, Palette, Clipper. Needs a **VESA LFB**. No hardware acceleration. v4/v7 methods mostly "not implemented". |
| DSOUND | Yes: IDirectSound, IDirectSoundBuffer, Notify. SB via SB16.DLL. Some methods missing. |
| D3D | **None found.** No D3D/D3DIM dir in SrcEmu. HX lists OpenGL only (via SourceForge blurb). Direct3D immediate mode would be ours to write. |
| Licence | Conflicting: "freeware" (HXsrc.txt) vs "Public Domain" (SourceForge). Source is public. Modify/redistribute not clearly stated; ask Japheth or read `Doc/HXRT.TXT` locally. |
| First experiment | Bed with a VBE 2.0 LFB card, DOS + HDPMI32 + HX Win32 DLLs, run a DirectX SDK DDraw sample (8 bpp). See s6. |

## 1. Components

| Part | Source dir (GitHub `Src/`, `SrcEmu/`) | Notes |
|---|---|---|
| DPMI host HDPMI32/16 | `Src/HDPMI` (ASM: PAGEMGR, I31MEM, I15 in INTXXAPI...) | 386+. Modes: INT15/raw, XMS, VCPI. v86 without VCPI: terminates silently. |
| Loader | `Src` DPMILDR / HXLdr32 / Stubs | Loads PE; DPMILD32 maps Win32 names to D* DLLs. |
| VESA32 | `Src/Vesa32` | VESA wrapper DLL. |
| Win32 emu | DKRNL32, DUSER32, DGDI32, DADVAPI, WINMM, SHELL32, OLE32, COMCTL32, COMDLG32, WS2_32, MSACM32... | `SrcEmu/` |
| DDRAW | `SrcEmu/DDDRAW`: DDDRAW.ASM, SURFACE.ASM, PALETTE.ASM, CLIPPER.ASM, DDOBJECT.ASM, DDENUM.ASM, FACTORY.ASM | DDDRAW.TXT v2.8 (2009). |
| DSOUND | `SrcEmu/DSOUND`: DSBUFFER.ASM, DSOBJ.ASM, DSCAP.ASM, DSENUM.ASM | DSOUND.TXT v1.5. |
| DINPUT | `SrcEmu/DINPUT` | |

DDDRAW specifics (DDDRAW.TXT history):

| Item | Detail |
|---|---|
| Frame buffer | "requires VESA linear frame buffer (LFB)". EnumDisplayModes lists LFB modes only. Banked modes not mentioned. |
| Mode set | ChangeDisplaySettingsA path; VESA save/restore state; mode restored on release. |
| Surfaces | Blt (incl. COLORFILL), BltFast, Flip (multi back buffer, triple), GetDC/ReleaseDC, GetSurfaceDesc returns lpSurface. OFFSCREENPLAIN in system RAM; only DDSCAPS_VIDEOMEMORY goes to video. |
| Palette | Via GDI palette functions. |
| Ignored | Colour keys (Get/Set return OK, ignored). Gamma: dummy. |
| Not documented | Lock/Unlock, SRCCOLORKEY blits, overlays, Blt stretch/ROP support. Read DDRAW SURFACE.ASM. |

URLs:
- https://github.com/Baron-von-Riedesel/HX (landing page: build tools, dirs `Src`, `SrcEmu`, `Include`; did not contain DDRAW, licence, memory)
- https://raw.githubusercontent.com/Baron-von-Riedesel/HX/master/HXsrc.txt (layout, "freeware, Copyright 1996-2026 Japheth", JWasm 2.15; no D3D; WSOCK32 binary excluded for copyright)
- https://raw.githubusercontent.com/Baron-von-Riedesel/HX/master/SrcEmu/DDDRAW/DDDRAW.TXT (interfaces, history; did not contain Lock, resolutions, working apps)
- https://raw.githubusercontent.com/Baron-von-Riedesel/HX/master/SrcEmu/DSOUND/DSOUND.TXT (interfaces; did not contain mixing, limits, SB detail, requirements)
- https://github.com/Baron-von-Riedesel/HX/tree/master/SrcEmu , `.../SrcEmu/DDDRAW`, `.../SrcEmu/DSOUND` (file lists)

## 2. Source and licence

| Item | Finding | Source |
|---|---|---|
| Source | Public, all of HX incl. DDDRAW/DSOUND | GitHub above |
| Toolchain | JWasm 2.15 (MASM-syntax assembly), JWlink, JWlib, NMake or Open Watcom WMake, WRC; HXDEV package | HXsrc.txt |
| Language of DDRAW | ASM (all `.ASM`) | tree listing |
| Licence | "freeware" (HXsrc.txt); "Public Domain" (https://sourceforge.net/projects/hx-dos/); DDDRAW.TXT/DSOUND.TXT defer to HXRT.TXT / HXGUI.TXT | not read |
| Open | Whether a modified DDRAW may be redistributed. HXRT.TXT: fetch of `Doc/HXRT.TXT` and `Doc/HXGUI.TXT` returned 404 at the guessed paths. Get it from the release archive https://sourceforge.net/projects/hx-dos/files/ (hxrt217.7z / HXGUI). | |

A rewritten DDRAW back end in C is possible: DDDRAW only has to export the same COM vtables to a PE loaded by HX.

## 3. Games / demos and failures

Little evidence. Nothing found that is a verified list.

| Report | Source | Did not contain |
|---|---|---|
| DirectX 2 / DirectDraw 2 good enough for Neko Project II (Win32 emulator) | VOGONS search hit https://www.vogons.org/viewtopic.php?p=82965 (via search summary; thread not fetched) | settings, VESA card |
| "limited DirectDraw support"; aimed at simple GUI apps | https://sourceforge.net/projects/hx-dos/files/ (search snippet) | list |
| Quake II "runs like crap" under HX + GUI addon (VMware) | https://www.vogons.org/viewtopic.php?t=19298 (search snippet only; direct fetch 403) | renderer used; D3D not available so presumably software/OpenGL |
| MechWarrior 2 Titanium, X-Wing 95 tried with Win98 DLLs copied | same thread | outcome |
| Early assessment: emulation "will most likely not allow to run third-party DirectDraw apps" | https://www.vogons.org/viewtopic.php?p=61451 (snippet) | later versions improved (v2.x) |

Failure causes inferable from DDDRAW.TXT, not from reports: ignored colour keys, v4/v7 stubs, no D3D, no acceleration, LFB needed. Real Win95 games usually use Lock on the primary, DDSCAPS_COMPLEX flips, colour-keyed Blt and often IDirectDraw2+, so expect partial compatibility. Not verified on any game.

## 4. Memory and CPU

| Question | Answer | Source |
|---|---|---|
| Minimum extended | 72 KB for host init | HDPMI.TXT s2: https://raw.githubusercontent.com/Baron-von-Riedesel/HX/master/Src/HDPMI/HDPMI.TXT |
| Needs XMS/HIMEM? | No. Real mode with no XMS host = INT15 mode (E801h, else 88h). XMS only needed if the A20 mechanism is unknown to raw mode. | same |
| No extended at all | `HDPMI=2` adds DOS memory to the pool | same |
| VCPI | Only if v86 (EMM386); XMS preferred when present | same |
| `HDPMI=64` | skip XMS v3 calls (WSWAP) | same |
| CPU | "80386 or better". No PCI/APIC requirement stated. No 486 caveat stated. | same |
| DR-DOS XMS-only "insufficient memory" | fixed by HDPMI=64 | https://bttr-software.de/forum/forum_entry.php?id=6318 |
| Not found | Minimum RAM for a Win32 app under HX (DDRAW+DSOUND+game). 4.2 MB is likely tight for a Win95 game; PE images load into extended memory. | |

Our risk: the 5160 BIOS has no INT 15h AH=88h. HDPMI INT15 mode may see 0 KB. How DOS/32A got 4.2 MB is unexplained (groundwork doc s1). Test: HDPMI32 then a small PE console app printing GlobalMemoryStatus. HDPMI is "-a" (separate address contexts) recommended; "-r" TSR.

## 5. Feasibility: m8dos behind DDRAW

What HX's design assumes: the primary surface is the LFB itself (VESA, CPU-visible); Lock returns a pointer into it, GetSurfaceDesc returns lpSurface; Flip is a VESA page/start-address change or copy. Offscreen plain surfaces already live in system RAM.

| Piece | Plan | Cost / risk |
|---|---|---|
| Primary and back buffers | System-RAM shadow (8 bpp, 640x480 = 300 KB). Lock = pointer to shadow. | RAM; 4.2 MB budget |
| Flip / UnLock-then-present | Upload shadow to the 8514 side. No aperture: only I/O-port pixel transfer (card OUT ~7.6 us/B-class, see memory: iowidth). 300 KB per frame is unusable as a per-frame path. | Main blocker for Lock-based games |
| Blt / fills / BltFast between surfaces | If dest is the primary: engine rects/copies via m8dos; src in RAM needs pixel upload (same cost) | Fills/flat primitives fine; sprites not |
| Palette | m8dos DAC writes | easy |
| Games that draw by Lock (most 2D) | Cost above; same wall as the Windows-side DDraw work (`m8dd32`) | Not a DOS-only problem |
| Direct3D | Write D3D immediate-mode COM (IDirect3D/2/3, Device, Viewport, Execute buffers) as a new PE DLL that reuses the m8d3d core: flat-shaded polygons, 8 bpp, depth sort at Flip (header of `drivers/mach8_display/src/m8d3d.c`). In Windows that code relies on the DDraw runtime walking execute buffers and calling RenderState/RenderPrimitive; under HX there is no runtime, so the walk and the state machine must be added. | Largest piece; no HX code to start from |
| VGA half for shadow display | The VGA half does have a CPU-visible buffer; HX's own DDDRAW over VESA could show frames via the VGA half with no 8514 involvement, but then the Mach8 engine is not used. | gives a baseline |

VESA on our card: HX DDRAW needs a VESA 2.0 BIOS with LFB. Whether the Mach8 card's VGA BIOS has VBE is unknown here (repo notes mention a `VVESA` TSR from the Windows side; `docs/dos_driver_groundwork_2026_11_18.md` row C000:0064h). A VBE 2.0 LFB TSR (e.g. UniVBE) could supply it; not checked, licence unchecked. Our own planned VBE/AF driver (`docs/dos_driver_interfaces_2026_11_18.md`) is a different interface (AF, not VBE).

## 6. Cheapest first experiment (bed)

| Step | Detail |
|---|---|
| 1 | AT-class 486 bed (86Box), 16 MB RAM, DOS 6.22, HIMEM present is fine for this first test; a 2nd run without HIMEM, no EMM386, tests the INT15 path. |
| 2 | Video card with real VBE 2.0 LFB in 86Box (needs a card whose BIOS has VBE 2.0 LFB; on 86Box several S3/Cirrus/Trident ROMs; verify with a VBE info call first). If none, add UniVBE to the guest. |
| 3 | HXRT release archive (https://sourceforge.net/projects/hx-dos/files/): HDPMI32.EXE, DPMILD32.EXE, DKRNL32/DUSER32/DGDI32/DDDRAW/DSOUND DLLs. `HDPMI32 -a`, then `DPMILD32 flip2d.exe`-style run of a DirectX SDK DDraw sample (8 bpp, 640x480 flip). |
| 4 | Pass: sample draws and flips. Then repeat on the INT15-only memory path (no HIMEM) to settle the blocker. |
| 5 | Record which DDraw calls fail (DDERR codes) to size the replacement back end. |

What the bed needs: VBE 2.0 LFB, HX archive, an SDK DDraw sample. 86Box does not model the Mach8 DOS path, so the memory test (4.2 MB, no AH=88h) needs the 5160 itself: put HDPMI32 and a tiny PE console test on the card.

## Not found

- Authoritative HX licence text (404 on guessed Doc paths).
- Any compatibility list; any D3D in HX; Lock/Unlock support statement.
- HDPMI behaviour on a BIOS without INT 15h AH=88h.

## 7. Step 1 on the 5160 (2026-11-19)

HX 2.23 runtime (`HXRT223.zip`, SHA-256 `20510cf6...5c9`, https://github.com/Baron-von-Riedesel/HX/releases/tag/v2.23)
on the card's DOS boot, run through the DOS agent (`captures/2026-11-19_hx_card/HX1.TXT`). `HDPMI32 -r` resident (it
exits with errorlevel 1 when it stays); HX's `DPMI.EXE`: INT 15h AH=88h 0 KB as feared, but HDPMI uses the XMS host
(4,152 KB largest block), 4,188 KB lockable, 4,284 KB physical. `DPMILD32 HXMEM.EXE` (Win32 console,
`tools/dos3d/hx/hxmem.c`): GlobalMemoryStatus 4,284 KB total, 2,836 KB free; 2 MB VirtualAlloc at 0040B000h, 512 pages
written and read back, 0 bad. `HDPMI32 -u` unloads; the agent restarted normally. Budget for a DirectX title plus our
DLLs: ~2.8 MB, no swap. Next: step 2, our DDRAW over `m8dos` in place of HX's LFB-only DDDRAW.

## 8. Step 2 design (2026-11-19, parked for the card-levers work)

HX's `DDDRAW.DLL` imports `VESA32.DLL` (21 exports) and uses `SVGAINFO.PhysBasePtr` directly as a linear address
(`SrcEmu/DDDRAW/DDOBJECT.ASM` SetVideoVars; `SURFACE.ASM` Create). Its Flip calls `SetVesaDisplayStart(offset, pitch,
flags)` (`SURFACE.ASM` ~1109) and polls `GetVesaFlipStatus`. Palette changes arrive through DGDI32 as
`SetVesaPaletteEntries(start, n, entries)`, 4 bytes an entry, 8-bit, red first (VESA32 itself swaps R/B for VBE).
So step 2 is **our own VESA32.DLL** and nothing else: one mode 101h (640x480x8, attributes SUPPORTED|GFX|COLOR|LFB),
`PhysBasePtr` = a 640 KB buffer in RAM, `GetVesaVideoMemorySize` = 640 KB (two pages, so DDDRAW's flip chain gets its
back buffer inside it), `SetVesaDisplayStart` = compare the page with a shadow of the card's back page, send changed
runs with `m8_hline`, `m8_frame_end(vsync unless DDFLIP_NOVSYNC 8)`, `GetVesaFlipStatus` = 1; state save/restore and
mouse stubs. m8dos needs a `-bt=nt` build: its two `int386` calls (mode 3 at restore, DPMI 0100h for the mailbox)
become inline `int 10h` / `int 31h` (HDPMI gives IOPL 3, so port I/O is direct). flip2d also needs HX's `DINPUT.DLL`
and `WINMM.DLL` (`HXGUI223.zip`). HX source read: https://github.com/Baron-von-Riedesel/HX `e4cb9e2`.
