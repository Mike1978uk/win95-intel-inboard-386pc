# DOS driver interfaces for the Mach8 engine, 2026-11-18

Desk research; nothing built or run. Extends `docs/p5_api_reading_2026_11_17.md` and `docs/x16_dos3d_api_sourcing_2026_11_17.md`. "nv" = not verified. Sources read in full unless marked snippet. vogons.org returns 403, so vogons rows are snippets.

## 1. VBE/AF driver file (VBEAF.DRV)

### Format and loading

| Fact | Source |
|---|---|
| File is one flat 32-bit blob. Allegro reads the whole file with `pack_fread` into `malloc`ed memory and uses it in place: **no relocation pass, no fixups.** The code must be position independent. | https://raw.githubusercontent.com/liballeg/allegro5/4.4/src/misc/vbeaf.c (`load_vbeaf_driver`) |
| Header = `AF_DRIVER`, 876 bytes: Signature[12] "VBEAF.DRV"; Version (>= 0x200 or rejected); OemVendorName[80]; AvailableModes ptr (offset 180); TotalMemory; Attributes; BankSize/BankedBasePtr; LinearSize/LinearBasePtr; IOPortsTable (212); IOMemoryBase/Len[4]; Checksum (260); IOMemMaps[4] (288); BankedMem (304); LinearMem (308); SetBank32; **Int86, CallRealMode, InitDriver (404-412)**; af10Funcs[40]; PlugAndPlayInit (576); OemExt; SupplementalExt; then ~72 function pointers from GetVideoModeInfo (588) to EndVideoFrame (872). | https://fuchsia.googlesource.com/third_party/yasm/+/refs/tags/v1.0.1/tests/nasm/vbeaf.asm (offsets computed from declared sizes) |
| Pointers in the table are offsets within the loaded image; the **loader** writes the runtime fields: `IOMemMaps[]`, `BankedMem`, `LinearMem`, `Int86`, `CallRealMode`. Order: signature/version check, `PlugAndPlayInit`, loader fills pointers, `InitDriver`. | vbeaf.c (`vbeaf_lowlevel_init`, `initialise_vbeaf_driver`) |
| Search order (Allegro): config key `vbeaf_driver`; the exe's directory; **`c:\vbeaf.drv`**; `%VBEAF_PATH%`. | vbeaf.c (`vbeaf_locate_driver`) |
| Protected mode, flat, 32-bit, in the application's address space (DJGPP near pointers). The driver gets port access through `IOPortsTable` and memory through `IOMemMaps`; Allegro's `vbeafex.c` also hands FreeBE/AF drivers a libc and a PM/Lite-style table via `OemExt`. | https://raw.githubusercontent.com/liballeg/allegro5/4.4/src/misc/vbeafex.c |
| Mode info = `AF_MODE_INFO`, 128 bytes; `Attributes` bits as used by Allegro: 2 virtual scroll, 8 linear buffer, 16 2D accel, 64 HW cursor, 2048 triple buffer; banked is the *else* case. | vbeaf.c (`vbeaf_init`, `find_vbeaf_mode`) |

### Mandatory vs optional

| | Functions |
|---|---|
| Loader calls unconditionally | `PlugAndPlayInit`, `InitDriver`, `GetVideoModeInfo`, `SetVideoMode`, `RestoreTextMode`, `SetDisplayStart`, `SetActiveBuffer`, `SetVisibleBuffer`, `SetPaletteData`, `SetMix`, `Set8x8MonoPattern`, `Set8x8ColorPattern`, `Use8x8ColorPattern` (no NULL check in Allegro, so stubs are needed). |
| NULL-checked (optional) | `EnableDirectAccess`, `WaitTillIdle`, `DisableDirectAccess`, `SetClipRect`, `SetCursor`, `DrawScan`, `DrawPattScan`, `DrawColorPattScan`, `DrawLine`, `DrawRect`, `DrawPattRect`, `DrawColorPattRect`, `DrawTrap`, `PutMonoImage`, `BitBlt`, `BitBltSys`, `SrcTransBlt`, `SrcTransBltSys`, `OemExt`. |
| Allegro flag set from each | `DrawScan` -> `GFX_HW_HLINE`; `DrawLine` -> `GFX_HW_LINE`; `DrawRect` -> `GFX_HW_FILL` (+ `clear_to_color`); **`DrawTrap` -> `GFX_HW_TRIANGLE`**; `PutMonoImage` -> `GFX_HW_GLYPH`; `BitBlt` -> `GFX_HW_VRAM_BLIT`; `BitBltSys` -> `GFX_HW_MEM_BLIT`; `SrcTransBlt(Sys)` -> masked blit. |
| Not used by Allegro at all | `DrawScanList`, `DrawTri`, `DrawQuad`, `StretchBlt` families (never referenced in the quoted flag table). Cheap to add, no caller found. |

### Key question: accelerator-only, no LFB and no banked buffer?

**No, not safely.** Evidence:

- The loader never checks `LinearMem`, `BankedMem` or `Attributes` for NULL. A mode without bit 8 is treated as banked; the screen bitmap is built with `_make_bitmap(..., vaddr, ...)` and `vaddr` (from `BankedMem`/`LinearMem`) is **not NULL-checked**. Any Allegro call the driver does not accelerate (`putpixel`, `textout` when no `PutMonoImage`, `blit` from memory when no `BitBltSys`, `getpixel`, `masked_blit`, `stretch_blit`) writes through that pointer.
- A mode without bit 16 is rejected unless the driver identifies as FreeBE ("Hardware acceleration not available").
- The spec itself was not read, so "illegal" is unproven; FreeBE/AF's page lists only drivers that have banked, linear or both ("Dumb framebuffer, banked only" is the lowest tier) and does not address the no-aperture case: https://www.shawnhargreaves.com/freebe/ .

So a driver can *report* neither; what happens then depends on Allegro mapping a pointer that does not exist. Workable variant (design, not tested anywhere): report a **banked or linear window that points at driver-owned system RAM** (a shadow 640x480 plane). Calls we accelerate go to the engine; calls we do not land in the shadow and are lost unless the driver flushes the shadow itself (e.g. on `WaitTillIdle`, `SetVisibleBuffer`, or a timer hook; Allegro calls those). Games that use *only* accelerated calls are unaffected; the rest need the flush.

### Template

| Candidate | Fit | Source |
|---|---|---|
| **FreeBE/AF `stub` driver** | Smallest; shows `InitDriver`/table fill with no hardware. First read. | https://www.shawnhargreaves.com/freebe/ (`freebs12.zip`, not downloaded) |
| FreeBE/AF **ATI mach64** | Only ATI driver, accelerated, banked + linear. Mach64 is memory-mapped, not 8514-style, so the draw functions are not reusable; the table fill, mode list, `WaitTillIdle` are. | same |
| FreeBE/AF **S3** | Accelerated, banked only; S3 86C911/805 engine is 8514/A-register-compatible (same `9AE8h`/`BEE8h`-class ports), so its draw routines are the closest to Mach8. nv: not read. | same |

Did not contain: any 8514/A or Mach8 driver; the VBE/AF spec (SciTech NDA, price); any list of games.

## 2. Games that load VBEAF.DRV

No source found that names a title. What is established:

| Claim | Status |
|---|---|
| Allegro (DOS, 3.x/4.x) loads it; `vbeaf_driver` config key; FreeBE/AF page says Allegro and SciTech MGL are the only major users. | verified: https://www.shawnhargreaves.com/freebe/ , https://liballeg.org/stabledocs/en/alleg036.html |
| SciTech sold drivers inside Display Doctor; abandoned VBE/AF for Nucleus. | verified (same pages) |
| Quake / id engines | nv; no evidence found; Quake DOS uses VESA LFB. Do not assume. |
| UniVBE bundled in many games | not the same thing; UniVBE is VBE core, not `vbeaf.drv`. |
| Any shipped commercial Allegro DOS title that uses 2D accel at 640x480x8 | **not found.** Titles need testing by dropping `c:\vbeaf.drv` and running `afinfo`. Allegro's own test programs (`exaccel`, `ex3d`) are the certain users: https://dyn.manpages.debian.org/trixie/allegro4-doc/exaccel.3alleg4 (listing only; not read). |
| Allegro autodetect tries VBE/AF before VESA | memory of the docs, nv. |

Did not contain: any list of games, any CPU minimums. Allegro DOS support ended 2007, so the title pool is mostly shareware and homebrew.

## 3. BRender .BDD

| Fact | Source |
|---|---|
| Load: `BrDevAdd(&dev, "name", args)` -> `BrImageReference` -> `BrImageLookupName(img, "BrDrv1Begin")` -> `dev = BrDrv1Begin(char *args)`; returns `br_device *` or NULL. `BrDevAddStatic` skips the file and takes the function pointer (a game that links its driver never loads a .BDD). | https://raw.githubusercontent.com/foone/BRender-1997/master/fw/devlist.c |
| Name resolution: no suffix tries `.BDD`, then host loader, then `.DLL`; export lookup by name with `_` variants. | https://raw.githubusercontent.com/foone/BRender-1997/master/fw/image.c |
| Image *format* is hidden behind `ImageLoad`/`HostImageLoad`; DOS `.BDD` binary format not found. | same; `fw/loader.c` not read |
| `BrDrv1Begin` creates a device, then facilities. Soft driver: one device + one `Default-Renderer` facility; the primitive library is a *separate* image. | https://raw.githubusercontent.com/foone/BRender-1997/master/softrend/driver.c |
| 3dfx back end is the template for a hardware driver: `DEVICE.C`, `OUTFCTY.C` (output facility, display modes, `pixelmapNew`), `Devpixmp.c` (device pixelmap), `PLIB.H/Plib.c` (primitive library), `RENDFUNC.C`, `Pstate.c`, `Match.c`. | https://github.com/foone/BRender-1997/tree/master/3dfx |
| **No CPU-visible pixelmap needed**: device pixelmaps carry `BR_PMF_NO_ACCESS`, `pm_pixels` starts 0 and is mapped only inside `directLock`/`copyBits`. `flush`/`synchronise` may be no-ops; there is no flip method, `doubleBuffer` does the swap. `copyFrom` and `rectangleCopy` may return `BRE_DEV_UNSUPPORTED`. | https://raw.githubusercontent.com/foone/BRender-1997/master/3dfx/Devpixmp.c |
| Triangle entry: `void TriangleRender_Flat(brp_block *, brp_vertex *a, *b, *c)`; reads `comp_f[C_SX,C_SY,C_SZ,C_R,C_G,C_B]`; flat uses one constant colour from vertex a, Gouraud copies per-vertex RGB. | https://raw.githubusercontent.com/foone/BRender-1997/master/3dfx/RENDFUNC.C |

Minimum device: device + output facility (modes, `pixelmapNew` returning a no-access device pixelmap, `doubleBuffer`) + renderer facility + primitive library with `brp_block`s for the cases to be accelerated (flat/Gouraud triangle; lines/points optional). Everything the library does not claim falls back via `Match.c` to the software path, which draws into a **system-memory** pixelmap, so it does not need a CPU-visible screen but must then be copied (`copyTo`/`rectangleCopyTo`).

Shipped titles loading a `.BDD` from disk: **none confirmed.**
- Vogons list shows `3DFX_DOS`, `RAGE_DOS` and Windows driver images; poster says only the 3dfx driver shipped and no one has tested BDD swapping (snippet: https://www.vogons.org/viewtopic.php?p=310432 ).
- Carmageddon DOS 3dfx patches use Glide `.OVL`, not a `.BDD` (snippet: https://git.new.deadbsd.org/falso/dethrace/src/branch/3dfx_printscreen ).
- Carmageddon II is listed as a BRender title (https://www.moddb.com/engines/brender/games , search snippet; page 403).
- CPU minimums: nv for all.

Did not contain: DOS `.BDD` file format, a title that loads one, build rules for a DOS driver (the 1997 tree has no `dos/` dir and says it "doesn't necessarily build": https://github.com/foone/BRender-1997 ). License MIT.

## 4. GLIDE2X.OVL

Described only as "a DOS (kinda) DLL"; it works with DOS/4GW titles and needs DPMI (snippet: https://www.vogons.org/viewtopic.php?p=577356 ; search results for https://www.vogons.org/viewtopic.php?p=115687 ). DOSBox-X and dgVoodoo1 ship replacement OVLs that forward calls to a host Glide library, so a swap point exists for dynamically-linked titles (https://github.com/Wengier/dosbox-x-wiki/wiki/Guide:Setting-up-3dfx-Voodoo-in-DOSBox%E2%80%90X ). The file format, export table and extender coupling are not documented anywhere found; one would have to disassemble a stock OVL. Replacing it is feasible in principle (call forwarding is proven by others), but it needs reverse engineering first, ~60 `gr*` entry points, and targets Pentium-era titles that assume Z-buffer and Gouraud. Statically linked games are out. Not first choice.

## 5. Recommendation

**Implement VBE/AF first**, as `c:\vbeaf.drv`, with a driver-owned shadow plane for the aperture.

Reasons: the only interface here whose binary layout, loader, flag mapping and file path are fully documented (sections 1); the loader is open source and needs nothing reverse-engineered; the test needs no title (`afinfo`, Allegro `exaccel`). BRender has the better driver contract and the cleaner "no CPU-visible pixelmap" precedent, but no title that loads a `.BDD` is confirmed and the DOS image format is unknown, so a first swap cannot be tested. Revisit BRender after reading `fw/loader.c` and a real `.BDD`.

Honest limit: no commercial title using Mach8-class 2D accel via VBE/AF was found. First success is Allegro's own accel examples; real-title yield is **unknown**.

### Minimum function list

| Group | Functions |
|---|---|
| Loader-required stubs | `PlugAndPlayInit`, `InitDriver`, `GetVideoModeInfo`, `SetVideoMode` (640x480x8, mode info bit 16 set, bit 8 set with `LinearMem` = shadow), `RestoreTextMode`, `SetPaletteData`, `SetDisplayStart`, `SetActiveBuffer`, `SetVisibleBuffer` (CRT_OFFSET flip), `SetMix`, `Set8x8MonoPattern`, `Set8x8ColorPattern`, `Use8x8ColorPattern` |
| Sync | `WaitTillIdle`, `EnableDirectAccess`, `DisableDirectAccess` (flush shadow here) |
| Drawing (engine) | `SetClipRect`, `DrawRect`, `DrawScan` (also used for `polygon()`), `DrawTrap` (triangle()), `DrawLine`, `BitBltSys` (host pixel transfer), `BitBlt` (VRAM copy), `PutMonoImage` (mono expansion) |
| Skip first | pattern, stipple, masked/stretch blits, `SetCursor`, `DrawScanList`, `DrawTri`, `DrawQuad` |

Target titles: Allegro `exaccel`/`ex3d` test programs (certain), then any Allegro 4 DOS game at 640x480x8 found by trial (none named; unverified).

## Sources not retrievable

vogons.org (403), osdev.org VBE/AF page (403), grok.dragonflybsd.org (Anubis block), moddb (403). The VBE/AF spec (VBE-AF07.pdf) was not located.
