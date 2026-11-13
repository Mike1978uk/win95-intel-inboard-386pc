# Test titles for the Mach8 driver, 2026-11-13

Research only; nothing downloaded or run. Ranked by new driver coverage first, then chance of running on a 486/83, 5 MB.
"nv" = not verified by any source found. Published minimums are the publisher's claim. Already ranked D3D samples: `docs/dx_sdk_ddk_bench_survey_2026_11_09.md` section 4.

## Ranked titles

| # | Title (year, publisher) | API | Published min | Free demo | Stresses | Fit for 486/83, 5 MB | Source |
|---|---|---|---|---|---|---|---|
| 1 | Donuts / Donut, DX SDK | DDraw 8 bpp, DInput, DSound | none stated | in every DX SDK (`donuts.exe`) | colour-keyed Blt, many small blits, back buffer | tiny sprites, 640x480x8 | local `references/dx61samples/ddraw/src/donuts/readme.txt` |
| 2 | Memtime, DX SDK | DDraw | none | DX SDK | Lock writes vs Blt, sysmem to vidmem, memory bandwidth | prints a report, no load | local `ddraw/src/memtime/readme.txt` |
| 3 | Stretch, Stretch2, DX SDK | DDraw | none | DX SDK | stretch Blt with clipping | trivial | local `ddraw/src/stretch/readme.txt` |
| 4 | Wormhole, DX SDK | DDraw 8 bpp | none | DX SDK | palette animation (SetEntries each frame) | CPU-light | local `ddraw/src/wormhole/readme.txt` |
| 5 | Font, FSWindow, Switcher, DX SDK | DDraw | none | DX SDK | Lock + GDI text, GDI dialog over exclusive mode, cooperative-level switch | trivial | local `ddraw/src/{font,fswindow,switcher}/readme.txt` |
| 6 | Flip2d, DX SDK | DDraw | none | DX SDK | page flip (already tested at 640x480) | regression check | local `ddraw/src/flip2d/readme.txt` |
| 7 | Mosquito, DDOverlay, DX SDK | DDraw overlays | none | DX SDK | overlay caps: must fail cleanly if HAL has none | trivial | local `ddraw/src/mosquito/readme.txt` |
| 8 | Red Alert Win95 demo (1996, Westwood/Virgin) | DDraw 8 bpp, DX3 on CD | not in readme; full DOS game 486-66 per a secondary page | `ra95demo.zip` | scrolling map, many small colour-keyed blits; INI `HardwareFills`, `VideoBackBuffer` toggle HAL paths | 640x400 default; CD must stay in drive; 30 MB disk | https://discmaster.textfiles.com/view/26120/dpcb1297.iso/rademo/INSTALL/README.WRI ; https://cncnz.com/downloads/red-alert-downloads ; https://ensigame.com/games/command-conquer-red-alert/system-requirements |
| 9 | Fury3 trial (1995, Microsoft) | Win95 API nv; WinG/DISPDIB on Win 3.1 | 486DX/66, 8 MB, local-bus SVGA 256 colours | `fury3try` on MS CD samplers | software renderer into 8 bpp surface: Lock-heavy, palette; "Full Screen, Panoramic" mode for slow machines | published minimum is a 486 | https://discmaster.textfiles.com/view/14632/Microsoft%20Home%20CD%20Sampler.iso/software/fury3try/readme.txt |
| 10 | Doom 95 (1996, id/Microsoft) | DDraw; `-emulate` disables DDraw acceleration | 486/50, 8 MB, 256 colours | nv | full-screen 8 bpp blit of a CPU-rendered frame; DDraw vs emulate path | 486 title | https://discmaster.textfiles.com/file/6198/BILA_VRANA.iso/010A/DOOM95.ZIP/DOOM.HLP/DOOM.pdf ; https://doomwiki.org/wiki/Doom95 (403 to fetch; text from search snippet) |
| 11 | Hellbender trial (1996, Microsoft) | Direct3D | retail Pentium 75, 8 MB; trial readme not readable | `heltrial.exe` v1.01 (Aug 1996) | D3D flat triangles on a HAL, sorting without Z | Pentium minimum; long shot | https://elisoftware.org/wiki/Hellbender_(PC,_CD-ROM)_Microsoft_-_1996_USA,_Canada_Release_(NFR) ; https://discmaster.textfiles.com/file/6148/WIN95_DEC_1996_3.ISO/action/HELTRIAL.EXE/RCDATA/CABINET/README.DOC (404) |
| 12 | Monster Truck Madness trial (1996, Microsoft) | D3D HAL or own 8 bpp software path (`useDirect3D=0`) | nv; retail recommended P166, 32 MB | `mtmtrial.exe` | current test: textures, D3D vs software A/B | long shot, in progress | https://en.wikipedia.org/wiki/Monster_Truck_Madness ; `docs/mtm_bed_setup_2026_11_12.md` |
| 13 | Diablo shareware (1996, Blizzard) | DDraw 8 bpp | Pentium 60, 8 MB (retail) | shareware existed; file name nv | 640x480 palette, isometric sprites, many small blits | 8 MB listed, Pentium | https://gamesystemrequirements.com/game/diablo ; https://github.com/footballhead/pre-ablo |
| 14 | Warcraft II demo (1996, Blizzard) | WinG + Win32s per demo readme | 486/33, 8 MB, SVGA, CD | demo released 1996-01-28; file name nv | WinG-style DIB blits to 8 bpp, palette | 486 listed | https://discmaster.textfiles.com/view/22821/PCA0496.ISO/demos/warc_2/readme.txt ; https://warcraft.wiki.gg/wiki/Warcraft_II_Demo |
| 15 | Shockwave Assault demo (1996) | DDraw if adapter supports it, else fallback | 486-66, 8 MB, 1 MB SVGA | demo on a shareware CD; file nv | HAL caps negotiation, DDraw-or-fallback | 486 listed; publisher not in source | https://discmaster.textfiles.com/view/6137/The%20Best%20of%20Select%20Top%2050%20Volume%201.iso/sw_demo/read_me.txt |
| 16 | WinBench 96 / 97 (Ziff-Davis) | GDI | not stated; WinBench 97 is 32-bit, Win95/NT only | benchmark, free | GDI playback: text, bitblt, many small ops | 32-bit, 486 likely; nv | https://www.vogons.org/viewtopic.php?p=355951 ; https://archive.org/details/PCMagBenchmark |
| 17 | CrystalMark Retro 2.1 (GDI test) | GDI | Win95 listed ("may crash"); CPU nv; slow without FPU | free | GDI drawing rates, no-FPU timing note | FPU fitted | https://crystalmark.info/en/?p=1356 |
| 18 | Full Tilt! Pinball (1995, Maxis) / 3D Pinball Space Cadet (Plus! 95) | GDI/DIB; API nv | 486SX, 8 MB (one listing); Plus! 486, 8 MB | Space Cadet shipped free in Plus!; Full Tilt shareware nv | small sprite redraw, 8 bpp palette | 486 listed | https://www.mobygames.com/game/4476/full-tilt-pinball/specs/ ; https://technet.microsoft.com/en-us/library/cc768075.aspx |
| 19 | Win95 stock: Solitaire, Minesweeper, Mystify, Flying Windows | GDI | Win95 baseline 386DX/20 | in the OS | card blits, text, line drawing, screensaver flips | certain to run | https://techshelps.github.io/MSDN/DNWIN95/HTML/S61F2.HTM ; https://www.techrepublic.com/pictures/photos-classic-windows-screensavers-from-windows-1-0-to-windows-98/7/ |
| 20 | Hover! (1995, Microsoft) | API nv | 486/33, 8 MB | on the Win95 CD-ROM | 3D first-person software render | 486 listed | https://tcrf.net/Hover!_(1995) ; https://en.wikipedia.org/wiki/Hover! |
| 21 | Foxbear, DX3a/DX5 SDK | DDraw, DSound | none | SDK | scrolling background, colour-keyed sprites | tiny | https://archive.org/details/directx3asdk ; `docs/dx_sdk_ddk_bench_survey_2026_11_09.md` |
| 22 | D3D SDK set: flipcube, tunnel, twist, flip3dtl, tunneldp, lights | D3D | none | SDK | triangles, Gouraud, textures | see survey | `docs/dx_sdk_ddk_bench_survey_2026_11_09.md` |
| 23 | PC Player / Gamestar D3D benchmarks (1997) | D3D | built for 1-4 MB cards | free | fog, dither, perspective, filter options | local, not unpacked | https://archive.org/details/pcplbe101 ; https://archive.org/details/gad3dbe101 |
| 24 | 3D WinBench 97, Turok demo (1997) | D3D | nv | local | heavy D3D | too heavy, parked | `references/d3d_programs/` |
| 25 | Descent II demo (1996, Interplay) | DOS, VGA/VESA | 486-33, 8 MB; Pentium, 16 MB recommended | `d2demo` | DOS box / MS-DOS mode switching against the Mach8 VGA half | DOS only | https://discmaster.textfiles.com/view/21834/Power%20Game%20Pak%20volume%202.iso/d2demo/readme.txt |
| 26 | Age of Empires demo (Oct 1997, Microsoft) | DDraw 8 bpp | Pentium 90, 16 MB Win95, 1 MB VRAM | demo exists; file nv | palette, large map, many blits | 16 MB: will not fit | https://dosdays.co.uk/topics/Games/game_aoe.php ; https://jeffpar.github.io/kbarchive/kb/174/Q174850 |
| 27 | Total Annihilation demo (1997, Cavedog) | DDraw, no 3D accel | Pentium 100, 16 MB, DX5 | demo exists; file nv | 2D sprites at scale | 16 MB: will not fit | https://discmaster.textfiles.com/view/38168/CD1.iso/README.TXT |
| 28 | Stash (TBL, The Party 1997, 64k) | Win32; API nv | nv | scene.org | intro on a Win32 loader | unknown | https://hugi.scene.org/online/hugi15/fowin32.htm |

Top 5 picks 1-5 are already on disk and need no download.

## Already on disk

- DX6.1 DDraw samples, prebuilt: `references/dx61samples/ddraw/bin/` (donut, donuts, multinut, stretch/2/3, wormhole, mosquito, memtime, flip2d, font, fswindow, switcher, ddovrly, ddenum, ddex1-5). Sources in `ddraw/src/`.
- DX6.1 D3D samples: `references/dx61samples/d3dim/bin/` (tunneldp, tunneleb, lights, fog, boids, pplane, ...); `dxmisc/bin/dxver.exe`.
- Internet DX5 SDK installer: `references/d3d_programs/idx5sdk.exe` (holds tunnel, twist, flip2d, flip3dtl, foxbear sources).
- MTM trial: `references/d3d_programs/mtmtrial.exe`, `C:\Users\lycet\Downloads\mtmtrial.exe`, `Downloads\mtmtrial.zip`, extracted `Downloads\mtmtrial\`.
- `references/d3d_programs/3DWB97.EXE`, `TUROK.EXE` (not in earlier docs' run lists).
- `references/dxddk/bench_pcplayer/*.rar`, `bench_gamestar/*.rar` (inner `DATA.Z` not unpacked).
- DX5/DX7 DDK and Win98 DDK trees: `references/dxddk/`; DX7 SDK git clone: `references/dx7sdk/`; Win95 DDK iso: `Downloads\Microsoft Windows 95 DDK\`.
- `Downloads\Winstone_96.rar` (WinBench 96 inside per the Vogons post; contents not listed here).
- `Downloads\CrystalMarkRetro2_1_0.zip` (CMRGDI32.exe is the GDI test).
- `Downloads\FastDoom_1.2.0\` (DOS Doom; WADs not seen; docs cover it: `docs/fastdoom_techniques_2026_11_08.md`).
- Own probes: `tools/m8bench`, `m8cube`, `m8ddtest`, `m8ddbench`, `m8d3dtest`, `m8probe`, `fb16test`; `Downloads\m8utl\TEST.COM`.
- `Downloads\directx_Jun2010_redist` is DX9-era: not for this machine.
- Not found anywhere: Donuts/Foxbear installers outside the SDK trees, Doom 95, Red Alert demo, Fury3, Hellbender trial, Diablo, Warcraft II demos.
- Already covered in earlier docs, not repeated: Descent, Descent II, Duke3D, Quake, Tomb Raider, Jedi Knight, Carmageddon (`docs/d3d_benchmarks_and_games.md`); Rockem, Pouet intros (`docs/dx_sdk_ddk_bench_survey_2026_11_09.md`).

## Sources that did not help

- https://discmaster.textfiles.com/file/20377/Microsoft%209%20Pak%20CD%20Sampler.iso/help/sysreq5.hlp/sysreq5.pdf : compressed PDF, text unreadable (it lists MS game requirements).
- Hellbender trial readme (discmaster PDF and CABINET README.DOC): PDF unreadable, CABINET link 404.
- MTM requirements searches: only recommended P166/32 MB and MTM2 pages; no 1996 minimum.
- Diablo demo, AoE demo, TA demo searches: retail figures only; no demo file names or demo readmes.
- Red Alert demo readme: no CPU/RAM figures.
- Descent II demo readme: no DirectDraw or DirectX mention; runs in a DOS box.
- Fury3 readme: no DirectX or DirectDraw notes for Win95.
- WinBench search: no minimum CPU; archive links not opened.
- Rocket Jockey, Close Combat demo: no usable requirement found.
- Win95 demoscene search: no confirmed DirectDraw 486-capable production; pouet API gives no graphics API.
- Doom95 doomwiki page: HTTP 403.

## Downloaded 2026-11-13

Folder root `C:\Users\lycet\Downloads\test_titles\`; each file from archive.org `https://archive.org/download/<item>/<file>`. Sizes match the item metadata. Nothing run or installed; only readmes extracted to `x\` for the stated requirements. 177 MB total.

| Title | Folder | File | Bytes | MD5 | Item | Main exe / notes |
|---|---|---|---|---|---|---|
| Red Alert Win95 demo (row 8) | `redalert_demo` | `ra95demo.zip` | 27076646 | b44ab9ec1bc634ea755587d1988e3722 | CommandConquerRedAlert_1020 | `INSTALL\RA95.EXE`, `SETUP.EXE`; DX3 `dxsetup` inside; readme gives no CPU/RAM |
| Fury3 trial (9) | `fury3` | `FURY3X.ZIP` | 4827942 | a28e83a9681eac5d1ae7753d2dde96cb | Fury3_1020 | `FURY3X.EXE`, `SETUPX.EXE`; readme: 486DX/66, 8 MB; Win32s/WinG for Win 3.1 |
| Doom 95 shareware (10) | `doom95` | `doom95.zip` | 4487398 | 6bdf418348490bb39e1cb2bb0a7137c1 | DOOM_95 | `DOOM95.EXE`, `SETUP.EXE`, DirectX 1 in `DIRECTX\`; item says episode 1 only; `DOOM.HLP` states 486/50, 8 MB |
| Hellbender trial v1.01 (11) | `hellbender_trial` | `HellTrial.7z` | 12255294 | 97321bad1622e35e1c9c8a2a6e3bf1ef | HellTrial | `HELLBNDX.EXE`, `DXFULL.EXE` (DirectX 2); README.DOC: min Pentium 75, 8 MB, 256 colours, DirectX 2.0; recommended P90, 16+ MB |
| Warcraft II demo v1.08 (14) | `warcraft2_demo` | `war2sw108.zip` | 37372714 | 8a17f8c4d342377d4c108ee2bc33ac78 | WarcraftIiTidesOfDarknessDemo | `setup.exe`, `install.exe`, `war2ed95.exe`; readme: 486/33, 8 MB, Windows 95, WinG + Win32s, SVGA 640x480 |
| Shockwave Assault demo (15) | `shockwave_assault` | `SW_DEMO.rar` | 38905475 | 6bc31ae0433006aff8f0b6724b777f1a | SWDEMO_20140725 | `SW8_200/240/480.EXE`, `SW16_240/480.EXE` (8 and 16 bpp variants), `SETUP.EXE`; readme: 486-66, 8 MB, 1 MB SVGA (2 MB for 16 bpp); DirectDraw section |
| DirectX 3a SDK (21) | `dx3a_sdk` | `DIRECTX3A.EXE` | 55532209 | 50e268dad40bb1ac8600f29703fe49a0 | directx3asdk | RAR SFX, listed with 7-Zip only; holds `FOXBEAR\FOXBEAR.EXE`, `SDK\SAMPLES\{DONUTS,MEMTIME,FLIP2D,TUNNEL,TWIST,...}` |

Shockwave Assault is the only title found with a 16 bpp DirectDraw mode choice (2 MB card stated), useful against our emulated 16 bpp desktop.

No direct URL, or skipped:
- Diablo (13): archive.org `diablo-pre-release-demo` is a 75 MB ISO (over 60 MB cap) and a 1996 pre-release, not the shareware; no shareware item found.
- WinBench 96/97 (16): archive.org holds WinBench 97 only as a 615 MB ISO (`zdbop-97-c_202504`) and a TIFF scan; WinBench 96 is in local `Downloads\Winstone_96.rar`; `WINBEN25_ZIP` is Win 3.1 WinBench 2.5.
- Space Cadet / Full Tilt (18): only `3dPinball_201906` (`3d_pinball.msi`, a Windows Installer package, not a Win95 file); skipped.
- Hover! (20), Descent II demo (25): archive.org title searches found no standalone item (only CD-ROM images or soundtrack).
- Stash (28): archive.org search empty; scene.org search page redirected, no file URL reached.
- Row 23 PC Player/Gamestar: already local (`references/dxddk/`).
