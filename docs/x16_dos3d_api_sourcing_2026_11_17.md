# X16 DOS 3D API sourcing, 2026-11-17

Research only; nothing downloaded or run. Extends `docs/showcase_3d_candidates_2026_11_14.md` (sections A, D, shortlist).
"snippet" = search-result summary, page not read. Every vogons.org page returned HTTP 403 to fetch, so all vogons rows are snippet, not read.

## Corrections to the 2026-11-14 doc

| Earlier claim | Now |
|---|---|
| Screamer II and Rally are the last VLB CGL games, separate CGL files | Snippet: both are PCI-only (Rendition Verite), via FST/IST-format CGL files that exist only for Verite. Another listing says Screamer II has a patch for retail. Sources conflict. Not a VLB target. |
| First 5 bundled games link CGL in the exe, later games use CGL files | Snippet, same: Hi-Octane has CGL in the exe (VLB). Actua Soccer VLB build has no .CGL file (embedded); the PCI fix adds `VRITB170.CGL` to the install dir (typically `c:\asoccer`). |
| CGL 3D Blaster = one chip | VLB card is 3Dlabs GLINT; PCI card is Rendition Verite. Different hardware, same API. |

## 1. Creative CGL

| Game | CGL delivery | Source |
|---|---|---|
| Hi-Octane | In the exe (VLB). Creative PCI patch existed, never released. | https://www.vogons.org/viewtopic.php?p=1420090 (snippet) |
| Actua Soccer | VLB: in the exe. PCI: `VRITB170.CGL` copied beside it. | https://www.vogons.org/viewtopic.php?p=1420090 , https://www.vogons.org/viewtopic.php?p=270274 (snippets) |
| Screamer II, Screamer Rally | PCI only, patch needed; separate CGL for Verite. | https://www.vogons.org/viewtopic.php?p=1392009 (snippet) |
| Fatal Racing, Whiplash | VLB; `VRITE170.CGL` lets them start on PCI. | same thread (snippet) |
| Rule of thumb from forum | Game ships VRIT*.CGL = PCI-capable; only GIGT*/none = VLB only. | https://www.vogons.org/viewtopic.php?p=1212419 (snippet) |

SDK and headers:
- Creative never released the SDK. Forum posts mention a leaked "HOT1/1.2" SDK with two .LIB files (one a static-linked VLB driver) and a "CGLCDB.LIB" loader in HOT2. No URL to the leak found. Function names and vertex structures: not found.
- Post says Actua Soccer source code contains CGL usage in source files: https://www.vogons.org/viewtopic.php?p=1420090 (snippet). Source location not found.
- The RReady (Verite wrapper) developer works on a VLB wrapper and has a forced "CREATIVE 3D BLASTER DETECTED" path: https://www.vogons.org/viewtopic.php?p=1420471 (snippet; result list only, not confirmed). That wrapper's source would show the CGL entry points; unverified.

Detection: testers say CGL looks for a PCI ID / vendor ID, not only a BIOS string (Euro 96 did not activate with Verite as 2D card; Screamer 2 did). Exact BIOS string or port probe: not found. Source: https://www.vogons.org/viewtopic.php?p=1420090 (snippet).

Demos/legal copies: Actua Soccer demo is on Games Machine 33 cover disc, `actua/actua.zip` (README says about 4 MB disk): https://discmaster.textfiles.com/view/22395/XENIATGM33.iso/actua/actua.zip/README.TXT (snippet; whether the demo has CGL: not stated). Hi-Octane demo, one level (Amazon Delta Turnpike), PC Gamer UK July 1995: https://www.tcrf.net/Proto:Hi-Octane_(DOS) ; discmaster copy https://discmaster.textfiles.com/view/23999/1995-10_Disc_11.iso/hioctane/readme.txt (snippet). Full game listed free at https://dosgamesarchive.com/download/hi-octane (links not checked).

## 2. Other DOS 3D APIs

| API | Delivery | Gave | Did NOT give | Source |
|---|---|---|---|---|
| BRender (Argonaut) | Plugin device drivers, `.BDD` files, e.g. `3DFX_DOS.BDD`, `mcga.bdd`. Driver list also shows `RAGE_DOS` (ATI Rage) and Windows entries naming DLLs. Poster: 3Dfx and ATI DOS drivers may be low-level, not API-based; most seem unreleased. | A replaceable-file mechanism exists. Someone renamed a driver to `mcga.bdd` in Carmageddon and ran RAGE_DOS: graphics garbled. | Any DOS title shipping a BDD that is not software/MCGA/Glide; driver ABI. | https://www.vogons.org/viewtopic.php?p=310432 ; https://www.vogons.org/viewtopic.php?p=227454 (snippets) |
| BRender, Carmageddon DOS 3Dfx | Patches use Glide `.OVL`/DLL, not a BDD. | Carmageddon 3dfx via Splat Pack patch. | Whether stock Carmageddon DOS loads `.BDD` by name. | https://git.new.deadbsd.org/falso/dethrace/src/branch/3dfx_printscreen (snippet) |
| BRender source | MIT, DOS tree public. | See 2026-11-14 doc, section B. | `.BDD` interface read from source: not done. | https://github.com/foone/BRender-v1.1.2 |
| S3D (ViRGE) | Not determined. | Title list in 2026-11-14 doc. | Delivery, SDK. ViRGE is PCI, Pentium era. | https://dosdays.co.uk/topics/s3d_games.php |
| ATI 3D Rage CIF | Windows DLL; DOS path not found. | See 2026-11-14 doc. | DOS use. BRender has a `RAGE_DOS` driver name, so DOS access existed for ATI Rage in BRender. | https://www.vogons.org/viewtopic.php?p=310432 (snippet) |
| Rendition Speedy3D | DOS API; RRedline is its Windows port. Titles: IndyCar Racing II, NASCAR Racing 2 / 1999, Whiplash, VQuake (changelog mention only). Early titles recognise only V1000; `V2DOSFIX` patches. | Title names, existence of a Speedy3D DOS SDK thread. | SDK contents, whether file or linked. | https://sourceforge.net/projects/dosbox-rendition/ ; https://www.vogons.org/viewtopic.php?p=171383 (snippets) |
| Matrox MSI | Nothing new found. | - | - | - |
| RenderWare | Nothing new found for DOS. | - | - | - |

Hardware note: every card above is a texture-mapping 3D chip. Mach8 has no triangle or texture unit, so the API implementation would be ours (host-side setup, Mach8 span fills).

## 3. DOS games with 8514/A or Mach8/32 modes (additions to section A)

Nothing new confirmed. Snippet-only items:
- "Mah Jongg -8514-" claimed to use 8514/A, single forum user, nv: https://www.vogons.org/viewtopic.php?p=74295
- Microsoft Flight Simulator 4 claim repeated, another thread failed to run it under 8514 emulation, nv: https://www.vogons.org/viewtopic.php?p=118742
- ATI VGAWONDER-family register doc (not a game list): https://pdos.csail.mit.edu/6.1810/2017/readings/hardware/vgadoc/ATI.TXT (snippet, not read)
No 3D or vector title found.

## 4. Ranking: replaceable file, 486 minimum, flat/Gouraud

None meets all three. Best fits, with the gap stated:

| # | Target | File | Fit / gap | Legal copy | First offline test |
|---|---|---|---|---|---|
| 1 | BRender DOS titles with `.BDD` (Carmageddon DOS / Splat Pack; Motor Mash rip had `3DFX_DOS.BDD`) | `*.BDD` (size nv) | Replaceable by name (renaming worked). Textured; 486 minimum nv. Gap: driver ABI. | Carmageddon: retail only (none found). Source: MIT BRender trees above. | Read `drivers`/`softrend` in BRender-1997 and the 1.1.2 DOS tree for the device-driver export table; compare with `mcga.bdd` behaviour. No card needed. |
| 2 | Actua Soccer (VLB/PCI builds, Gouraud-ish players, textured pitch) | `VRITB170.CGL` (PCI variant); VLB embedded | Separate file only for the PCI variant, Verite-targeted. 486 VLB era. | Games Machine 33 demo (above), CGL content nv. | List demo files for `*.CGL`; if present, dump exports/strings; if absent, the demo is exe-linked. |
| 3 | Hi-Octane demo | CGL inside exe | Binary patch, not a file swap. Textured. | PC Gamer UK demo / shareware discs above. | Search the exe for CGL strings, GLINT PCI ID (3Dlabs 0x3D3D) and port probes. |

Screamer II / Rally dropped: PCI-only, Verite. Honest read: a file-swap path on a 486 is not shown by any source; item 1 is the only one with a proven swap, and it needs the ABI from BRender source first.
