# DOS 3D title ladder, 2026-11-18

Desk research, nothing run. Extends `docs/x16_dos3d_api_sourcing_2026_11_17.md`, `docs/p5_api_reading_2026_11_17.md`, `docs/flight_titles_2026_11_18.md`, `docs/playable_3d_plan.md` P4/P5.

Rules for reading the tables:
- **Read** = a page fetched this session. Only one was: bttr-software Voodoo article (https://www.bttr-software.de/articles/voodoo.php). Its Glide rows are the only verified Glide facts.
- **nv** = not verified; figures are from the author's memory of box/readme claims. Wikipedia URLs are given as the page to check; they were **not fetched** (the Glide list page 404'd).
- Frame rates on a 486/83 over 8-bit ISA VGA: **no measurement found for any title**. Nothing to quote; the bed benches them.
- Machine limits used: 640 KB conventional, ~4.2 MB extended via DPMI only (no XMS), DOS/4GW untested (DOS/32A substitute, nv per title), SB Pro 220/5/1, VGA half is 8-bit ISA (frame writes ~ slow; a 64 KB frame is the cost to watch).

## Tier A - should run now, no new work

Software-rendered, own renderer. Ordered by likelihood.

| # | Title | Year | Renderer | RAM min | CPU min | Extender | Our layer would have to... | Expected here | Source |
|---|---|---|---|---|---|---|---|---|---|
| A1 | Wolfenstein 3D | 1992 | software VGA 320x200 (mode X-ish) | 640 KB (nv, some builds want EMS/XMS for sound/music, nv) | 286 (nv) | none (real mode) | nothing; P4.2 raycaster source (GPL) is the port path | Runs; frame writes dominate, playable likely | https://en.wikipedia.org/wiki/Wolfenstein_3D (not fetched) |
| A2 | Stunts (4D Sports Driving) | 1990 | software VGA, flat polygons | 512-640 KB | 286/8086 era, 386 helps (nv) | none | nothing; P4.3 restunts hook later | Runs; flat polygons, ideal later target | https://en.wikipedia.org/wiki/Stunts_(video_game) (not fetched) |
| A3 | Elite Plus | 1991 | software VGA, flat/wire | 640 KB (nv) | 286 (nv) | none | nothing | Runs; closest to P4.1 style | https://en.wikipedia.org/wiki/Elite_Plus (not fetched; title name nv) |
| A4 | Frontier: Elite II | 1993 | software VGA, flat-shaded | 640 KB (nv; some sources list 1 MB, nv) | 386 (nv) | own/none (nv) | nothing | Likely runs; slow game on slow CPUs, fine at 83 MHz | https://en.wikipedia.org/wiki/Frontier:_Elite_II (not fetched) |
| A5 | Ultima Underworld: The Stygian Abyss | 1992 | software texture-mapped, VGA 320x200 (small window) | 2 MB (nv) | 386/25 (nv) | own protected-mode (nv); whether it uses XMS: nv - risk, we have none | nothing | Plausible; check memory manager need first. Texture-mapped, cannot go to our layer | https://en.wikipedia.org/wiki/Ultima_Underworld:_The_Stygian_Abyss (not fetched) |
| A6 | X-Wing | 1993 | software, flat polygons, VGA | 640 KB + (nv) | 386 (nv) | none (nv) | nothing; flat polygons = best-suited later hook, needs exe patch | Likely runs | https://en.wikipedia.org/wiki/X-Wing_(video_game) (not fetched) |
| A7 | TIE Fighter | 1994 | software, flat/Gouraud polygons, VGA | 4 MB (nv) | 486/33 (nv) | own/none (nv) | nothing | Plausible; RAM at the edge, 486/83 fine | https://en.wikipedia.org/wiki/Star_Wars:_TIE_Fighter (not fetched) |
| A8 | F-15 Strike Eagle III | 1992 | software, flat polygons | 640 KB-2 MB (nv) | 386 (nv) | none (nv) | nothing | Unverified requirements; likely | https://en.wikipedia.org/wiki/F-15_Strike_Eagle_III (not fetched) |
| A9 | Flight Unlimited (DOS) | 1995 | software, textured terrain | 8 MB recommended (flight doc) | 486DX2/66 rec. | own (nv) | nothing | RAM: minimum not published; 8 MB recommended may not run. Test | `docs/flight_titles_2026_11_18.md` row 1 |
| A10 | Doom / Doom II | 1993/94 | software VGA | 4 MB | 386DX/33 (nv) | DOS/4GW (untested), DOS/32A swap nv | nothing | RAM 4 MB vs 4.2 MB found: tight. Try with `-` options and low detail | https://en.wikipedia.org/wiki/Doom_(1993_video_game) (not fetched) |
| A11 | Duke Nukem 3D | 1996 | software (Build) | 4 MB min, 8 rec (nv) | 486/66 (nv) | DOS/4GW (nv) | nothing | RAM tight; texture-mapped | https://en.wikipedia.org/wiki/Duke_Nukem_3D (not fetched) |
| A12 | Rise of the Triad | 1995 | software | 4 MB (nv) | 486 (nv) | DOS/4GW (nv) | nothing | Tight; heavy | https://en.wikipedia.org/wiki/Rise_of_the_Triad (not fetched) |
| A13 | Descent (shareware) | 1995 | software, textured | 4 MB (nv); 8 rec | 486/66 | DOS/4GW | nothing | Already a named test title in earlier docs (`docs/test_titles_2026_11_13.md`); not repeated | - |
| A14 | Heretic / Hexen | 1994/95 | software VGA | 4 MB min, 8 rec (nv) | 486 (nv) | DOS/4GW (nv) | nothing | Tight; same as Doom | https://en.wikipedia.org/wiki/Heretic_(video_game) (not fetched) |

## Tier B - through our 8514A / VBE/AF / BRender layer, and fit RAM

No title was verified to load one of these as a replaceable file other than BRender's rename. Honest list:

| # | Title | Year | Renderer | RAM min | CPU min | Extender | Layer needs | Expected | Source |
|---|---|---|---|---|---|---|---|---|---|
| B1 | Carmageddon (DOS) | 1997 | BRender, loads `.BDD`; a renamed `mcga.bdd` ran | 16 MB (nv) | Pentium 90 (nv) | DOS/4GW (nv) | BDD with output facility, flat-only; textured game | RAM does not fit: **listed because it is the only swap shown**; use BRender SDK/MIT demos instead | `docs/x16_dos3d_api_sourcing_2026_11_17.md` s2; https://www.vogons.org/viewtopic.php?p=310432 (snippet) |
| B2 | BRender sample/SDK programs (MIT tree, DOS build) | 1990s | BRender | nv | nv | nv | DOS build of `foone/BRender-v1.1.2` + custom BDD | Best first BDD test: small RAM, flat/Gouraud 8-bit | https://github.com/foone/BRender-v1.1.2 |
| B3 | Allegro 4 programs (`ex3d`, `exzbuf`, Speed Hack, own games) | 1998-2005 | VBE/AF if `vbeaf.drv` present; flat `triangle()` via `GFX_HW_TRIANGLE` | 4 MB (nv) | 386+ | DJGPP (go32 / CWSDPMI, DPMI works here) | Mach8 `vbeaf.drv` per p5 doc | Fits RAM; only realistic VBE/AF user; no commercial title verified | https://www.shawnhargreaves.com/freebe/ ; https://www.allegro.cc/manual/4/api/graphics-modes/gfx_capabilities |
| B4 | Games ported on our own core (P4.1 flight demo, P4.2 Wolf3D, P4.3 Stunts via restunts) | ours | native 640x480x8 | < 4 MB | 486 | Open Watcom DOS/4GW (ours) | the P3 core | Best chance of playable | `docs/playable_3d_plan.md` P4 |
| B5 | Any 8514/A AI program (ATI `DEMOAI`, Halo, `IBM8514.BGI` apps) | 1987+ | 8514/A AI via HDILOAD | small | any | none | nothing; pixels reference only | Test bed, not 3D | https://www.os2museum.com/wp/the-8514a-graphics-accelerator/ (cited in p5 doc) |
| B6 | Microsoft Flight Simulator 4 | 1989 | claimed 8514/A support, single forum user | nv | nv | none | nv | Claim conflicts (another thread failed under emulation); unverified | https://www.vogons.org/viewtopic.php?p=118742 (snippet, from x16 doc) |
| B7 | Creative CGL titles (Hi-Octane, Actua Soccer VLB) | 1995-96 | CGL linked in exe / `GIGTB170.CGL` | nv | 486 (nv) | nv | CGL ABI unknown, exe patch | Not fit; ABI unread; Hi-Octane demo exe disassembly first | `docs/p5_api_reading_2026_11_17.md` s4 |

## Tier C - Glide (Voodoo 1) DOS titles

Verified (read) rows marked V. Everything else from memory/nv. All want Pentium-class + Z-buffered textures: flat-colour fidelity is low across the board. No CPU/RAM figure was found for any; box minimums are nv.

| # | Title | Year | Glide delivery | RAM / CPU min | Flat-colour survival | Source |
|---|---|---|---|---|---|---|
| C1 | Screamer Rally | 1997 | V: ships `glide2x.ovl` 2.42, Voodoo Graphics + Rush out of box | nv (Pentium class) | Poor: textured race scenery; painter order unknown | https://www.bttr-software.de/articles/voodoo.php |
| C2 | Dreams | 1997 | V: `glide2x.ovl` 2.42, installer requires it | nv | nv | same |
| C3 | Extreme Assault | 1997 | V: OVL 2.31 / 2.4 / Voodoo2 by patch | nv | Likely poor: terrain textures | same |
| C4 | Shadow Warrior | 1997 | V: v1.2 + official patches; Voodoo Graphics/V2, not Rush; delivery not stated | nv | Build sectors: ordering exists in engine, but the Glide path uses Z; nv | same |
| C5 | Descent II (3dfx DOS build) | 1996-97 | nv (listed with no Glide data on the bttr page) | nv | Medium: polygon room geometry, textures carry all readability; nv | same (title list only) |
| C6 | Carmageddon / Splat Pack 3dfx | 1997-98 | nv; patches use Glide `.OVL`/DLL not a BDD (snippet) | 16 MB / P90 (nv) | Poor | https://git.new.deadbsd.org/falso/dethrace/src/branch/3dfx_printscreen (snippet) |
| C7 | Tomb Raider (3dfx DOS patch) | 1996 | Voodoo Graphics patch = DOS exe `TOMB.EXE` talking to card directly; Rush patch uses `glide2x.ovl` (snippet); builds are exes, not swappable drivers | 8 MB, Pentium 90 (nv) | Poor: texture-defined, Z/near-plane clipping | https://en.wikipedia.org/wiki/Tomb_Raider_(1996_video_game) (not fetched) |
| C8 | MechWarrior 2 3dfx edition | 1996 | nv (DOS and Win95 builds both claimed; unverified which) | 16 MB, Pentium (nv) | Medium: mechs/terrain are polygon-heavy, flat shading plausible | https://en.wikipedia.org/wiki/MechWarrior_2:_31st_Century_Combat (not fetched) |
| C9 | Screamer 2 | 1996 | nv (bttr: no Glide data) | nv | nv | bttr page |
| C10 | Blood | 1997 | nv (bttr: no data) | nv | Build engine; nv | bttr page |
| C11 | Archimedean Dynasty | 1996 | nv (bttr: no data) | nv | nv | bttr page |
| C12 | Grand Theft Auto (1) | 1997 | nv (bttr: no data); 2D game, Glide path nv | nv | nv | bttr page |

Not Glide, correct the brief: Whiplash and Fatal Racing use Rendition (Speedy3D) / Creative CGL, not Glide (x16 doc s1-2). Descent 1 and MDK Voodoo patches are Windows-only (bttr "False positives"). Statically linked Glide games cannot use an OVL swap (p5 doc s2).

## Tier V - vendor 3D APIs (S3D, Rendition, ATI, PowerVR, Matrox, Creative)

Ranking key 1 is the RAM gate (~4.2 MB extended). Every title below is a 1996-97 PCI/VLB-card title; the card's own minimum is Pentium class in nearly all cases and RAM minimums are **nv** unless stated. None fits the gate by a verified figure, so rank within each API by smallest likely RAM (nv). "Binding" = how the game reaches the API. Only rows with a source-stated binding are not marked nv; the dosdays page names titles but says nothing about binding, RAM or CPU (read).

Cross-cutting result: no source found states the binding for any S3D, ATI, PowerVR or Matrox title. What is known is per API below.

| API / card | Title | Year | Binding (swappable?) | RAM / CPU min | Extender | Layer would need | Expected | Source |
|---|---|---|---|---|---|---|---|---|
| Creative CGL (3D Blaster VLB, 3Dlabs GLINT) | Hi-Octane | 1995 | **Linked in exe** (VLB): binary patch / wrapper | nv; 486 era (nv) | nv | CGL ABI unread | Best RAM odds of the vendor set (486 VLB era), still blocked on ABI | https://www.vogons.org/viewtopic.php?p=1420090 (snippet) |
| CGL | Actua Soccer (VLB) | 1995-96 | Separate `.CGL`: `C:\ASOCCER\VLB\GIGTB170.CGL` (3Dlabs), `PCI\VRITB170.CGL` (Rendition) - swappable by name, ABI unknown | nv; 486 VLB era | nv | Driver ABI | Same | https://www.vogons.org/viewtopic.php?p=270274 (snippet) |
| CGL | Fatal Racing / Whiplash (VLB builds) | 1995 | Separate: `VRITE170.CGL` lets them start on PCI (snippet) | nv | nv | ABI | Same | https://www.vogons.org/viewtopic.php?p=1392009 (snippet) |
| CGL | Screamer II, Screamer Rally | 1996-97 | PCI only, separate Verite `.CGL`; Screamer Rally also ships `glide2x.ovl` (bttr, read) | nv | nv | n/a | Not VLB target | x16 doc s1 |
| Rendition Verite (Speedy3D / RRedline) | IndyCar Racing II | 1996 | Speedy3D DOS API; V1000-only in early builds, `V2DOSFIX` patch (snippet); binding nv | nv, Pentium | nv | Speedy3D SDK unread | Out on CPU | https://sourceforge.net/projects/dosbox-rendition/ (snippet) |
| Verite | NASCAR Racing 2 / 1999, VQuake | 1996-98 | nv (changelog mention only) | nv | nv | same | Out | same |
| Verite | Whiplash | 1995-96 | Speedy3D/CGL both claimed (sources conflict) | nv | nv | same | Out | x16 doc s2 |
| S3 ViRGE (S3D Toolkit) | Terminal Velocity, Descent II, Tomb Raider (+Unfinished Business), MechWarrior II S3 Virge edition, Screamer, Fatal Racing, Whiplash 3D, Megarace 2, Havoc, Time Warriors, Battlerace, Terracide, FX Fighter Turbo, Flying Nightmares 2, Actua Soccer/Golf, Baseball, Confirmed Kill, Demon Driver, Rocket Jockey, Stratosphere, Croc, POD, Destruction Derby (Win95 only per page) | 1995-97 | **nv for all** (SDK header is DLL-style `DllExport`; DOS use claimed only in a snippet) | nv; ViRGE is PCI, Pentium | nv | S3D `TriangleSet` etc. (header read in p5 doc s6) | Out on CPU/RAM for most; older (Terminal Velocity 8 MB per `docs/flight_titles_2026_11_18.md`, Fatal Racing, Whiplash, Actua Soccer) are the least heavy | https://dosdays.co.uk/topics/s3d_games.php (read: titles only); https://github.com/kytulendu/S3DTK ; https://www.vogons.org/viewtopic.php?p=1254615 (snippet) |
| ATI 3D Rage (CIF) | None verified DOS. BRender has a `RAGE_DOS` device name; `DosInit` exists under `DOS_BUILD` in `Ati3dcif.h` | 1996-97 | BRender `.BDD`-style device name (swappable by BRender rule), not shown for any game | nv | nv | CIF header is a design model only | No title | https://github.com/foone/BRender-1997/blob/master/ati/Ati3dcif.h ; https://www.vogons.org/viewtopic.php?p=310432 (snippet) |
| PowerVR SGL | None found | 1996-97 | nv | nv | nv | SGL docs unread | No title | https://github.com/powervr-graphics/PowerVR-Series1 |
| Matrox Impression Plus (RenderWare 1.x, 1994) | Sento, Creep Clash, Spectre VR/MGA, IceHawk (3D Superpack) | 1994-95 | nv; forum only: RenderWare API, Sento 640x400 65k; driver file name, extender, RAM/CPU all nv. DOS stated for the Impression line only via a 1994 RenderWare datasheet snippet | nv | nv | nv | Unrecoverable: no driver docs, no SDK found | Possible fit by era (486, flat+Gouraud, no texture); blocked on ABI | https://www.vogons.org/viewtopic.php?p=279817 (search snippet; fetch refused) ; https://discmaster.textfiles.com/view/30698/Renderwaredemos1194.iso/rwftp/mac/demo/viewer.hqx/Viewer/RWdatasheet (snippet) |
| Matrox Mystique (MSI) | MechWarrior 2 (Mystique edition) - the only title found | 1996-97 | nv | nv; Pentium | nv | none recoverable | No | https://en.wikipedia.org/wiki/Matrox_Simple_Interface |

Ranking within the vendor tier (RAM gate first, all nv): 1 CGL Hi-Octane / Actua Soccer (486 VLB era), 2 Fatal Racing and Whiplash (earlier, lighter), 3 Terminal Velocity (8 MB: fails gate), 4 the rest, all Pentium-class. The ordering is inference; no RAM minimum was verified for any of them.

## Tier D - out of reach

| Title | Why | Source |
|---|---|---|
| Quake (DOS) | 8 MB min, Pentium and FPU-heavy, DOS/4GW-style; RAM does not fit | https://en.wikipedia.org/wiki/Quake_(video_game) (not fetched) |
| Quake II DOS, Ultimate Doom Builder era ports | RAM and CPU | nv |
| Carmageddon (software) | 16 MB, Pentium | Tier B1 |
| Terminal Velocity | 8 MB (one source), Pentium 60 | `docs/flight_titles_2026_11_18.md` |
| Comanche / Magic Carpet / System Shock | 4-8 MB with 486/66 plus rendering cost; RAM edge (nv), voxel/heavy | nv |
| Win95 Direct3D titles (FS98 D3D, X-Wing vs TIE, Falcon 4.0, Independence War, TigerShark, Air Warrior III) | Pentium, 16-32 MB, Windows-only | `docs/flight_titles_2026_11_18.md` |
| Speedy3D titles (IndyCar II, NASCAR 2, Whiplash, VQuake) | Verite PCI, Pentium | `docs/x16_dos3d_api_sourcing_2026_11_17.md` |
| S3D, ATI Rage, PowerVR, Matrox MSI titles | PCI cards, Pentium, DLL-style API | p5 doc s6 |

## Leads checked 2026-11-18 (owner's paste)

Method: WebSearch snippets plus a few fetches. **vogons.org refused every fetch (ECONNREFUSED)**, so all Vogons claims below are search-snippet level, not read. Read pages: discmaster RenderWare 1.4 DOS readmes, dosdays MGA Impression, Tomb Raider PowerVR readme, ctrl-alt-rees S3D patch page, HiOctaneTools issue 2.

| # | Lead | Verdict | Detail | Source |
|---|---|---|---|---|
| 1 | Matrox Impression (Plus), RenderWare: Sento, Creep Clash, Spectre VR/MGA, IceHawk | Titles real (bundle), DOS and driver binding **unverified** | Snippets: Impression Plus June 1994; Sento + Creep Clash use RenderWare API, Sento 640x400 65k; Spectre MGA = Spectre VR port to RenderWare; IceHawk and Sento in 3D Superpack. Games do not run on Millennium; a Sento hardware-check removal still crashed, so a software fallback in the same exe is **not** indicated. A Vogons post says the card has proprietary API "SXCI" and a Matrox dev kit existed. No driver file name, RAM or CPU found | https://www.vogons.org/viewtopic.php?p=279817 ; ?p=279818 ; ?p=279847 (all snippets) |
| 1a | RenderWare v1.x DOS SDK | Exists, but no accelerator driver docs | v1.4 for MS-DOS (Feb 1995) readme: VESA BIOS required, `dosrw.ini` sets resolution, libs `RW<D>...LIB` (static link). Does NOT mention Matrox/ATI drivers, extender, RAM, CPU. A 1994 datasheet snippet says Matrox and ATI support "announced" for DOS and Windows. Early 1.x SDKs listed as wanted on Vogons (not archived) | https://discmaster.textfiles.com/view/2963/Learn_3D_Graphics_Programming_on_the_PC_Ferraro.iso/rwdos/readme.txt ; .../rwdos/read1.txt ; https://discmaster.textfiles.com/view/30698/Renderwaredemos1194.iso/rwftp/mac/demo/viewer.hqx/Viewer/RWdatasheet |
| 1b | Impression card itself | Hardware real | Gouraud 3D at 1280x1024x8 (ISA: 800x600); DOS standards listed only Hercules/CGA/EGA/VGA; only Win 3.1 drivers listed; no DOS API/driver, no RenderWare/Sento mention. Utilities ILOAD, IDUMP | https://dosdays.co.uk/topics/Manufacturers/matrox/mga_impression.php |
| 2 | Creative 3D Blaster VLB (GLINT) CGL: Magic Carpet, Flight Unlimited, Rebel Moon, Toshinden | **Not confirmed**: none of Magic Carpet, Flight Unlimited, Toshinden tied to CGL by any source | Rebel Moon: bundled with 3D Blaster; MobyGames snippet says Verite 1000 only (so PCI build). Confirmed CGL VLB titles remain Hi-Octane, Actua Soccer, Fatal Racing, Whiplash. Snippet: the earliest five VLB-bundle CGL games carry the library inside the exe (agrees with earlier reading); `HOTCGL` env var selects the VLB file when both `GIGTB170.CGL` and `VRITB170.CGL` exist (snippet). Magic Carpet only appears as Hi-Octane's engine ancestry (Sean Cooper quote), not CGL. "No one understands CGL API today" (Moburma) | https://www.vogons.org/viewtopic.php?p=381039 (snippet) ; https://github.com/movAX13h/HiOctaneTools/issues/2 (read) |
| 3 | Matrox Millennium NASCAR Racing accelerated (1995) | **Windows**, not DOS | Snippet: accelerated build has no DOS support, runs only under Windows; also bundled with Diamond Edge 3D (NV1). Needs the compilation release. Vogons suggests a DOS-window workaround, unverified. API unstated | https://en.wikipedia.org/wiki/NASCAR_Racing_(video_game) (snippet) ; https://racesimcentral.net/?p=22983 (snippet) |
| 4 | NV1 / Diamond Edge DOS titles incl. Toshinden | **Toshinden NV1 not confirmed**; NV1 list is Saturn-port/Windows-era (Daytona USA, Panzer Dragoon, Virtua Fighter Remix, NASCAR) | One snippet: DOS compatibility "poor"; no DOS binding found | https://dosdays.co.uk/topics/Manufacturers/diamond/diamond_edge_3d.php (snippet) |
| 5a | Tomb Raider PowerVR | DOS, **per-chip exe, not a driver file** | `TOMBPCX1.EXE`/`TOMBPCX2.EXE`; needs PowerVR driver 4.0.0+ and VESA 2.0 (UniVBE via `UVCONFIG.EXE`); **16 MB** expected, `16` option forces less. Fails RAM gate | https://discmaster.textfiles.com/view/24018/1998-02_Disc_3.11.iso/PATCHES/tombpvr2.zip/Tombraid/ReadMe.txt (read) |
| 5b | Tomb Raider 3dfx/S3D/ATI | 3dfx: DOS exe (static-link old build works in pure DOS; Rush needs `glide2x.ovl`); S3D: DOS `Tomb.exe` replaces game exe; ATI CIF: **Windows** exe (ATI3DCIF, `ESCAPE` DLLs) | No RAM/CPU stated in S3D page | https://www.vogons.org/viewtopic.php?p=227081 (snippet) ; https://www.github.com/ata4/glrage (snippet) ; https://ctrl-alt-rees.com/2020-12-06-s3-virge-officially-supported-games-and-s3d-patch-downloads.html (read) |
| 5c | Descent II S3D | DOS `D2VIRGEP.EXE` patch, untested by author; binding = exe patch; no RAM/CPU | same ctrl-alt-rees page |
| 5d | Terminal Velocity S3D | DOS `tvreg_s3.zip`: replace `GAME.EXE`; needs `S3DRUN` wrapper on DX/GX; no RAM/CPU | same page |
| 5e | Terminal Velocity Rendition/Glide, Descent II Rendition/Glide | Not found | Searches returned no readme | - |

Result: no title found with a verified swappable driver file. Patches are replaced exes. Not found anywhere: any RAM/CPU minimum under 4.2 MB for an accelerated build.

Not contained in sources: Impression driver file name; Sento/Creep Clash/Spectre/IceHawk extender, RAM, CPU; RenderWare 1.x accelerator driver interface; Flight Unlimited, Magic Carpet, Toshinden CGL/NV1 support; Descent II/TV Glide-Rendition readmes.

## Recommended first five

1. **Wolfenstein 3D** - 640 KB, no extender; proves the VGA half at 320x200 with the least unknowns; P4.2 source is GPL.
2. **Stunts** - flat polygons, 512 KB; the cleanest flat-colour hook later (P4.3).
3. **Elite Plus or Frontier: Elite II** - flat-shaded space, the P4.1 look, no extender risk.
4. **Doom shareware** - 4 MB / DOS/4GW is the single test that answers the DPMI-at-4.2-MB question for all of A10-A14 and D-tier titles.
5. **TIE Fighter or X-Wing** - flat polygons on a 486; shows frame rate of a flat renderer over 8-bit VGA, the number we must beat.

Also first, but not a title: BRender 1.1.2 DOS samples with a stub BDD (B2), the shortest path to tier B.
