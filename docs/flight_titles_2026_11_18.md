# Flight and space combat titles, 2026-11-18

Research only; nothing downloaded or run. "nv" = not verified by any source found. Minimums are the publisher's or readme's claim.
Not repeated from `docs/test_titles_2026_11_13.md` and `docs/d3d_benchmarks_and_games.md`: Hellbender, Fury3, Descent, Descent II, Shockwave Assault, Monster Truck Madness.

Result: no 486-class Direct3D flight title with a demo was found. Every Win95 title with a 486/8 MB figure either is DOS, uses DirectDraw with its own renderer, or has no demo located. Titles with Direct3D need a Pentium and 16 MB or more.

## Ranked

| # | Title (year, publisher) | API | Published min | Demo and where | Low-detail / software | Fit |
|---|---|---|---|---|---|---|
| 1 | Flight Unlimited, Windows 95 build (DOS 1995, Win95 1996; Looking Glass / Virgin) | DirectDraw 1.0 per readme ("does not support 1024x768"); no Direct3D mention; own software texture-mapped renderer (inference) | Recommended 486DX2-66, 8 MB, 25 MB disk, DOS/Win95; minimum not listed; readme says 15 MB disk. https://www.gamepressure.com/games/flight-unlimited/zd3bc5 ; readme https://discmaster.textfiles.com/view/15645/PCM_9701.iso/igre/fu95/readme.txt | `f95demo.zip`, 18,433 kB, "Windows" playable demo (Win95 build not confirmed by the page): https://www.dosgamesarchive.com/download/flight-unlimited . Full Win95 files on a CD image: the discmaster readme path above (`igre/fu95`). A separate `flight.zip` is the DOS demo | Terrain detail levels in the readme ("lowest Terrain detail level"); software is the only renderer | Best fit: 486 class, 8 MB, DirectDraw 8 bpp full-frame writes, no D3D needed |
| 2 | Microsoft Flight Simulator 98 (1997, Microsoft) | DirectDraw/software; Direct3D optional for hardware (KB lists a Direct3D accelerator as optional); setup offers DirectX 5 | 486DX/66 (Pentium recommended), 8 MB (16 recommended) Win95, Super VGA 256 colours: https://jeffpar.github.io/kbarchive/kb/173/Q173985 | none found (nv); retail CD image only (a Lutris installer points to one on archive.org) | Software path implied by D3D being optional; detail settings nv | Only Win95 flight sim with a Direct3D option and a 486 minimum; no demo, so needs the full CD |
| 3 | Star Fighter, playable demo v1.04 (1996, Krisalis / 3DO) | none: DOS program run in MS-DOS or a Win95 DOS box; no DirectX mentioned | 486/33 (Pentium 90 suggested), 6 MB (8 MB required for Win95), VESA local bus or PCI SVGA 256 colours: https://discmaster.textfiles.com/view/27161/101%20OnLy%20ThE%20BeSt%20GaMeS%206.iso/bonus/sfdemo/readme.txt | folder `sfdemo` on that CD image; exe name nv; copyright line 1996 | F9 toggles 320x200 / 640x480; F7/F8 detail; F5/F6 view distance (fog) | Tests DOS-box and VESA against the Mach8 VGA half, not the HAL; fast CPU-bound renderer |
| 4 | Air Warrior III demo (1997, Interactive Magic / Kesmai) | DirectDraw-class 2D mode plus Direct3D hardware mode; readme names neither API explicitly | 2D: Pentium 90, 16 MB, "DirectX 5.0 compatible video card". 3D: Pentium 133, 24 MB, Direct3D accelerator with 2 MB: https://discmaster.textfiles.com/view/27952/Games%20Gallery%2098%20-%20Disc%202.iso/Content/AW3/AW3Demo.EXE/%25MAINDIR%25/readme.txt | `AW3Demo.EXE` on that CD image; also see https://combatace.com/forums/topic/100945-air-warrior-iii-demo-interactive-magic-1997 (not opened) | 2D rendering path without 3D hardware; three in-flight view-size modes | Both a 2D path and a D3D path, but 16 MB / Pentium 90: RAM does not fit |
| 5 | X-Wing vs TIE Fighter demo (1997, LucasArts) | DirectX 3.0a; Direct3D/DirectDraw not named in readme | Pentium 90, 16 MB, PCI graphics card: https://discmaster.textfiles.com/view/23270/CHIP%20Turkiye%20Ocak%201998.iso/OYUN/XWINGTIE/WEBDEMO/README.TXT | `xvtdemo.exe`, about 6.1 MB: https://games.softpedia.com/get/Games-Demo/X-Wing-vs-TIE-Fighter-Demo.shtml ; also the CHIP CD above | Alt-D cycles graphics detail; no 3D mode listed | Space combat, but 16 MB and Pentium; parked |
| 6 | Independence War demo (1997, Ocean / Particle Systems) | Win95 with DirectX 5; API nv | CPU/RAM not in the 1997 readme. The 1999 Deluxe Edition demo lists Pentium 166, 32 MB, DirectX 6, "256 colour mode required for Software mode": https://discmaster.textfiles.com/file/24039/1999-09_Disc_4.15.bin/IWARDEMO/iwdedemo.exe/_SETUP.1/Req.htm | 1997 demo runs from the CD, no install: https://discmaster.textfiles.com/view/23537/Computer%20Games%2095.iso/demos/iwar/readme.txt (dated 1997-09-16) | A software mode exists in the 1999 edition; 1997 readme silent | Too heavy; parked |
| 7 | TigerShark demo v1.0b (1997, n-Space / GT Interactive) | DirectDraw, DirectSound, DirectInput; MMX 16-bit graphics; Glide for Voodoo | CPU/RAM/disk nv in readme: https://discmaster.textfiles.com/view/21665/PC%20Games%20EXE%20(July-August%201997).iso/readme.txt | on that CD image | nv | MMX-oriented; not a fit |
| 8 | Falcon 4.0 demo (1998, MicroProse) | DirectX 5 | Win95, DirectX 5.0, 32 MB per the readme: https://discmaster.textfiles.com/file/1007/Cdrtv4n16.iso/Jogos/Falcon40/DEMO.Z/README.DOC | on that CD image | nv | 32 MB: no |
| 9 | JetFighter III demo v1.31 (1997, Mission Studios / Take 2) | nv | nv; readme describes install from Win95 Explorer or DOS; demo scenery trimmed to under 1/45 of retail: https://archived.software/QmcBAV7dFi1YtUgXgAYuiwMuv1zc9HBewyRNUaJ8auMAhL/5duGyJJj6GyS4FXc2awp6Tqfpwrcr7/raw/README.TXT (certificate expired on fetch; text from search snippet) | on a CD image, name nv | nv | Unknown on every axis |

## DOS-only demos found (no HAL contact, listed so they are not searched again)

- Su-27 Flanker demo (1995-96, SSI/Mindscape): `SU27.EXE`, self-running demo; readme has no requirements, no DirectX. https://discmaster.textfiles.com/file/15747/PC%20Format%20CD-ROM%20Collection%2021%20(1995-12)(Future%20Publishing)(GB)[issue%2051].iso/su27/su27.a01/README.TXT
- Fighter Duel demo (1996, Philips Media / Jaeger): VESA, 1 MB video RAM, 7 MB XMS/EMS; runs better outside a Win95 DOS box. https://discmaster.textfiles.com/view/1896/101onlythebestgames7techexpresssoftware1996.iso/arcade/fddemo/readme.txt
- Flying Corps demo v1.00b (Empire Interactive): P90, 16 MB, DOS; Ctrl-D changes detail. https://discmaster.textfiles.com/file/852/boot-disc-1997-04.iso/FLY/README.TXT

## New facts on titles already covered

- Fury3 also exists as a trial on a PC Gamer CD (`fury3/readme.txt`, `Fury3x.exe`, 486 minimum): https://discmaster.textfiles.com/view/23646/PCGAMER16.bin/fury3/readme.txt (listed by search, not opened).
- Terminal Velocity (1995, Apogee/3D Realms, DOS, Fury3's engine): Ensigame lists Pentium 60, 8 MB; one help file gives 8 MB, 10 MB disk. DOS: https://ensigame.com/games/terminal-velocity/system-requirements

## Sources that did not help

- Microsoft Flight Simulator for Windows 95 (1996): sources disagree (Ensigame Pentium 90 / 16 MB; Respawn 8 MB / 2x CD); no demo found. https://ensigame.com/games/microsoft-flight-simulator-95/system-requirements
- Microsoft KB Q147320: describes Microsoft World of Flight (486, 8 MB), an educational title, not a sim. https://jeffpar.github.io/kbarchive/kb/147/Q147320
- mobygames.com game pages: HTTP 403 to fetch.
- Flight Unlimited readme: states no CPU/RAM and does not call the file a demo; `f95demo.zip` content and Win95-ness not confirmed.
- Slipstream 5000, Hardwar, Wing Commander Prophecy: no usable requirement or demo found.
- gamerequirementslab Slipstream 5000 page: modern figures, unreliable.
- A Russian site's Terminal Velocity figures (DirectX 7, 64 MB): refer to a later release.
- Search for a demo of FS98: only retail pages and the KB.
