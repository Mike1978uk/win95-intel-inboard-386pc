# 3D showcase candidates, 2026-11-14

Research only; nothing downloaded or run. "nv" = not verified by any source found.
Already-ranked D3D/DDraw rows: `docs/test_titles_2026_11_13.md`, `docs/dx_sdk_ddk_bench_survey_2026_11_09.md` section 4. Not repeated here.

## A. Existing 8514/A support

| Finding | Source |
|---|---|
| IBM published only the Adapter Interface (AI) as programming interface; Halo (Media Cybernetics) was written to the AI. Mach/AGX clones are AI-compatible. | https://www.os2museum.com/wp/the-8514a-graphics-accelerator/ |
| `IBM8514.BGI` shipped with Borland's compiler (GKS-PC report lists it among BGI drivers). Games using it: none found. | https://pubs.usgs.gov/of/1993/0241/report.pdf |
| Forum claim: Microsoft Flight Simulator 4 uses ATI 8514/A. The same thread also says no game used 8514/A. nv. | https://www.vogons.org/viewtopic.php?p=74295 |
| 1991 developer quote on making one of the first 8514/A games; commenter: few waited for an installed base. Title not named. | https://www.vogons.org/viewtopic.php?p=118695 |
| MetaWINDOW, Genus GX 8514 drivers, DOS game list for 8514/A: nothing found. | searches 2026-11-14, no URL |

Result: no known native 8514/A 3D game. Next lead is the XFree86 / ATI driver docs already in `docs/mach8_driver_method.md`, not game lists.

## B. Engines and ports with source

| Candidate | Licence / status | How pixels reach screen | Titles | RAM/CPU | Source |
|---|---|---|---|---|---|
| BRender 1.1.2 (Sep 1995, DOS tree: `DOSIO`, `ZB`, `FW`, makefiles for Borland/MS/Watcom) | MIT, (c) Argonaut | Not read. Driver split unverified (nv). | Privateer 2 (source of the game not included) | nv | https://github.com/foone/BRender-v1.1.2 |
| BRender ~Aug 1997 (folders `softrend`, `pixelmap`, `pentprim`, `ddraw`, `d3drend`, `ati`, `s3`, `3dfx`) | MIT; "doesn't necessarily build" | Separate software renderer and hardware back ends; the `ati` folder is unexplained (nv) | game name redacted | nv | https://github.com/foone/BRender-1997 |
| BRender 1.3.2, CrocDE fork | MIT | nv | 3D Movie Maker, Croc, Carmageddon 1/2, I-War | nv | https://gamefromscratch.com/argonauts-brender-blazing-render-engine-open-sourced/ ; https://github.com/dethrace-labs/BRender-CrocDE |
| Reality Lab (RenderMorphics) | no source found; became Direct3D | n/a | n/a | n/a | https://en.wikipedia.org/wiki/Reality_Lab |
| Restunts (Stunts reverse engineering) | Original source does not exist; C translation minimal as of Nov 2023; builds DOS exe from patched disassembly | Not read | Stunts: 512K, 8 MHz, CGA/Hercules/EGA/VGA (MCGA 256 colours) | 512K-1 MB; fits | https://github.com/4d-stunts/restunts ; https://wiki.stunts.hu/wiki/Restunts ; https://dosdays.co.uk/topics/Games/game_4ddriving.php |
| fe2-intro (Frontier Elite II intro player, C + SDL) | Licence not stated; models read from the owner's Frontier exe; flat shading nv | Renderer written in C, close to the original; one rasteriser likely | Frontier: Elite II intro only | nv | https://github.com/watsonmw/fe2-intro |
| GLFrontier | OpenGL port; source release nv | OpenGL | Frontier: Elite II | nv | https://frontierastro.co.uk/Hires/glffe.html |
| Descent 1.5 source (Jan 1998) | Parallax licence, non-commercial; D1x adds GPL-with-limits | Not read; textured renderer | Descent 1 | 4 MB, 386/33 per DOS Days; 5 MB tight | https://www.gamespot.com/news/descent-source-code-released-2462520 ; https://www.dosdays.co.uk/topics/Games/game_descent.php ; https://github.com/btb/d1x |
| WinQuake source | GPL; `r_drawflat` is a branch in `d_edge`, not a separate renderer | Span drawing in `d_*` files; `D_DrawSurfaces` path | Quake | 8 MB / Pentium: nv | https://android.googlesource.com/platform/external/quake/+/0300c65810aa961c3bfe6cf23ed9550660bf04ff/quake/src/WinQuake/d_edge.cpp |

Notes: Descent and Quake are textured, so they stress the wrong path. BRender is the only multi-title engine with MIT source; its flat/Gouraud primitive code is in the software renderer folders (location of the span routine nv).

## C. Windows 95 DirectX titles, flat/Gouraud

No new title found this pass that is untextured and period-correct beyond rows in `docs/test_titles_2026_11_13.md` (Hellbender trial, Monster Truck Madness, etc.). Searches gave history only:
- Direct3D first shipped in DirectX 3.0; the DX1 Game SDK had none: https://archive.org/details/gamesdk
- Fury3 is the Win95 launch title: https://stoked.blog/?p=3287 (already row 9).
- Anecdote: a few hundred low-poly flat polygons run on a 486, not a benchmark: https://www.vogons.org/viewtopic.php?p=1369353 (nv).

## D. DOS 3D APIs other than Glide

Delivery column: "file" = separate loadable file (replaceable); "exe" = linked into the game exe (binary patch); nv = not verified. All title lists are community lists, not vendor documents. CPU/RAM minimums for these titles: nv unless stated.

| API | Titles with a DOS build | How it reaches hardware | Spec/SDK public | Source |
|---|---|---|---|---|
| Creative CGL (3D Blaster VLB, 1995, Creative's own API, later also on the PCI Verite card) | Actua Soccer, Hi-Octane, Screamer II, Rally (Screamer II and Rally said to be the last); Toshinden (PCI); Fatal Racing and Whiplash said to support VLB (disputed) | Mixed. Forum: the first 5 bundled CGL games have the library inside the game exe ("exe"); later games use separate CGL files ("file"). Games probe the card BIOS, so they refuse other cards. | nv | https://www.vogons.org/viewtopic.php?p=381039 (fetch failed this pass; text from search snippet) |
| S3D (S3 ViRGE, 1996) | Actua Golf, Actua Soccer (semi-finals), Battlerace, Confirmed Kill, Croc 1.0, Demon Driver, Descent II (patch), Destination Quartzon, Fatal Racing (HP OEM), Flying Nightmares 2, FX Fighter Turbo, Havoc, MechWarrior II S3 ViRGE Edition (exclusive build), Megarace 2, POD, Rocket Jockey, Screamer (exclusive), Stratosphere, Terracide, Terminal Velocity, Tomb Raider (patch), Whiplash 3D (HP OEM) | Page does not say. Several are separate exclusive builds or patches, so likely per-title code (nv). Games often detect only the ViRGE 325; patchers exist. | S3D Toolkit is listed among SDKs; availability nv | https://dosdays.co.uk/topics/s3d_games.php ; https://www.vogonswiki.com/index.php/S3 |
| Rendition Speedy3D / RRedline | Not enumerated this pass (Verite: Quake VQuake, Indy Car 2 etc. recalled, nv) | Speedy3D is the DOS API; RRedline its Windows port. Mechanism nv. | nv | https://www.vogons.org/viewtopic.php?p=171383 ; https://www.patreon.com/Rready/about |
| ATI 3D Rage CIF (ATI3DCIF) | Toshinden, Space Rox, Terracide, MechWarrior Mercenaries, Flying Nightmares 2, Assault Rigs, MechWarrior 2 (3D Rage build); Win-side: G-Police, IRC | `ATI3DCIF.dll` ships with ATI drivers, so Windows DLL ("file"). The MW2 Rage build needs a Rage card. DOS-mode path nv. | nv | https://www.oocities.org/k_lupinsky/support.htm ; https://www.vogons.org/viewtopic.php?p=947154 |
| Matrox MSI | MechWarrior 2 Enhanced for Mystique (a collector's list). No MSI API source or title list found. | nv | nv | https://www.vogons.org/viewtopic.php?p=417125 |
| PowerVR SGL, NV1 NVLib | Named in a Vogons compilation of proprietary APIs; no titles or mechanism checked. | nv | nv | https://www.vogons.org/viewtopic.php?p=316609 |
| RenderWare 1 (Criterion, 1993) | Wikipedia infobox lists MS-DOS; no DOS game list found. | nv | nv | https://en.wikipedia.org/wiki/RenderWare |
| BRender / Reality Lab | See B. | BRender: MIT source, DOS tree public | BRender yes (MIT); Reality Lab no | B |

Reading: all of these cards were 486-era only for CGL (VLB). S3D needs ViRGE (PCI, Pentium era), so those titles' minimums are Pentium-class in practice (nv per title). Only CGL has a plausible 486 target; its delivery is split between linked and loadable.
Not checked: whether any CGL or S3D game accepts a replacement driver file; first test below.

## Ranked shortlist

Weighted: replaceable driver file and 486-era minimum highest.

1. **CGL later-generation games (Screamer II, Rally, Hi-Octane).** 486 VLB era; separate CGL files per the forum. First test: find a game's CGL file and read its entry points (disassemble); no card needed.
2. **BRender 1.1.2 (DOS tree).** MIT source; Privateer 2 era. First test: read `ZB` and `DOSIO` for the framebuffer write and judge a polygon hook.
3. **Stunts (original binary).** Flat polygons, 512K, runs unchanged. First test: run in MCGA on the card, time a frame.
4. **Hellbender trial / Monster Truck Madness (rows 11, 12 of `docs/test_titles_2026_11_13.md`).** D3D flat triangles on our HAL. First test: run per that doc.
5. **BRender 1997 tree (`ati`, `softrend`).** Unlocks Carmageddon-class titles; build state unknown. First test: read `ati/` for its target.
6. **S3D exclusive builds (MechWarrior II ViRGE Edition, Terminal Velocity).** Many titles but Pentium-class and loading method unknown. First test: look for a separate S3D file in the game folder; if none, it is a binary patch.

Dropped: fe2-intro (no licence, intro only), WinQuake `r_drawflat` (8 MB nv, textured), Descent source (textured, 4 MB tight).
