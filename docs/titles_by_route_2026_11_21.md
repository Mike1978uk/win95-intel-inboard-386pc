# Titles by display route, 2026-11-21

Owner, 11-21: "maybe we need to split titles and mode". The Graphics Ultra has two halves, and each title
belongs on the one its requested mode suits. Sources: `docs/test_titles_2026_11_13.md`,
`docs/dos3d_title_ladder_2026_11_18.md`, `docs/d3d_benchmarks_and_games.md`, the VGA probes
(`docs/captures/2026-11-06_probes/README.md`). "check" = the requested mode is not recorded; read it off
the title before planning its run.

## The routes

| Route | Half | Modes | What is fast | What is not there | Bus per byte |
|---|---|---|---|---|---|
| **A. 8514 engine** | Mach8 | 640x480, 800x600, 1024x768 at 8 bpp; our 16 bpp desktop; 320x240 lores | engine fills, on-card copies, polygon fill, caches, our Direct3D | a CPU-visible frame buffer: pixels go through the port | 3.82 us (word) |
| **B. VGA half, low res** (X1) | 28800 | 320x200 (13h), Mode X 320x240 / 320x400, flipped | CPU writes, page flips (69.7 Hz), latch copies 4 px / 2 cycles | any engine, any 3D help | ~1.07 us |
| **C. VGA half, 640x480x8** (D6) | 28800 | 640x480 256 colours, one page (300 of 512 KB), 64 KB window | CPU writes | flips; a flat frame buffer (needs a VFLATD-style helper) | ~1.07 us |
| **D. DOS, VGA direct** | 28800 | whatever the game sets | as B | - | ~1.07 us |
| **E. DOS, 8514 through our code** | Mach8 | m8dos (DirectXT), `CUSTOM.GRA` | as A | - | as A |

Rule of thumb: a program that **describes** its frame (polygons, fills, blits of stored images) goes to A or E.
A program that **draws every pixel itself on the CPU** goes to B, C or D. The switch between halves costs
over a second of blank, so it happens at mode set and exit, never mid-game.

## Windows titles

| Title | Asks for | Route | Blocker today |
|---|---|---|---|
| Doom 95 | 320x200x8, CPU frame | **B** | no real 320x200 mode (quarter image in the corner); ~4 fps on A, ~14 fps ceiling on B; 8 MB title on 5 MB |
| Mode X DirectDraw ports (1995-96) | 320x200 / 320x240 | **B** | check each title's mode |
| Red Alert demo | 640x400 default, 8 bpp, small keyed blits | A (C if Lock-heavy) | 640x400: DirectDraw refused it elsewhere (velocity9x note); INI may give 640x480 - check |
| Fury3 trial | 8 bpp software render, Lock-heavy | C or B | mode check; "Panoramic" low mode exists |
| Warcraft II demo | WinG DIB blits, 640x480x8 | A | WinG path untested |
| Diablo shareware | 640x480x8, many small blits | A | Pentium, 8 MB |
| Shockwave Assault demo | separate 8 and 16 bpp exes | A | mode check |
| Hover! (Win95 CD) | software 3D | B or C | mode check |
| Full Tilt / Space Cadet, Win95 stock games | GDI | A | none expected |
| DX SDK DirectDraw samples (Donuts, Flip2d, Wormhole, Foxbear ...) | 640x480x8 | A | fswindow, donuts open (X14) |
| DX SDK Direct3D samples (tunnel, twist, flipcube) | 640x480 / lores 320x240 | A | runs; tunnel 4.3 % in D3DIM (D14 a) |
| Hellbender trial | Direct3D | A | Pentium 75, 8 MB |
| Monster Truck Madness | Direct3D 640x480x16, Locks the whole back buffer | A only (600 KB, 16 bpp: not B or C) | the full Lock (X17), parked (X13) |
| **Our racer (X40), Windows DirectDraw and Direct3D** | 640x480x8 | **A** | being built |
| AVI video (#60) | windowed or full screen | A windowed; **B** full screen | player, codec decode cost |

## DOS titles

| Title | Route | Blocker today |
|---|---|---|
| FastDoom (#47) | D | freezes with SB sound on |
| Descent II demo, Wolfenstein 3D, Stunts (baselines) | D | none known; RAM for Descent II |
| Magic Carpet, Tomb Raider demos | D | RAM (8 MB+); DOS/4GW swap untested |
| Flight Simulator 3.0 (`CUSTOM.GRA`, X30) | E | card run owed |
| Indianapolis 500 (X30 route) | E | after FS3 |
| **Our racer (X40), DOS**; M8SHOW (X36) | E | being built |
| Carmageddon (BRender `.BDD`, X24) | E | parked: Pentium 75, 16 MB |
