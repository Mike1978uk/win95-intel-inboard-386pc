# Direct3D benchmarks and games for the Mach8 HAL

Research only; nothing here was run on the card or a bed. Target: 486-class at ~83.5 MHz, FPU, no MMX,
5 MB RAM, Mach8 1 MB, our HAL: RGB, flat/Gouraud triangles, 8/16 bpp targets. No Z, textures, alpha, fog.
A published minimum is the publisher's claim, not a test. 5 MB is below every Windows figure found.
"Not verified" = no source found. Sources are mostly archive mirrors and aggregators, not original boxes.

## 1. Benchmarks

| Benchmark | Published need | Now | After textures + Z |
|---|---|---|---|
| Final Reality (Remedy, 1997) | DirectX 6 reported by one user; Pentium/MMX class per a retro commenter; RAM not verified | will not start (probable) | not verified |
| 3DMark99 / 99 Max | Pentium 166, 32 MB, DX 6.1, "DX6 compatible 3D accelerator"; no software rasterizer mentioned | will not start | will not start (166 MHz, 32 MB) |
| 3DMark2000 | Pentium 166 with MMX, 64 MB, DX 7, DX7 3D accelerator | will not start | will not start |
| 3D WinBench 97/98/99 | Not verified; reviewers ran it on a 300 MHz Pentium II | not verified, assume no | not verified |
| DirectX SDK samples (Tunnel, Twist, Flipcube) | Immediate mode, RAMP/RGB selectable, no stated textures or Z | runs (likely) | runs |
| SDK samples with textures (Oct1, Triangle, Globe, Tex1-5, Trans) | textures, Trans uses transparent textures | partial (untextured) | runs |
| dxdiag Direct3D test | Not verified for texture/Z use | not verified | not verified |
| Quake / GLQuake timedemo | Quake: Pentium 75 (or 486DX4-100), 8 MB DOS / 16 MB Win95. GLQuake: OpenGL, Voodoo/MiniGL | software Quake: RAM short; GLQuake: no, we have no OpenGL | software Quake unchanged; GLQuake: no |

Evidence and gaps:
- Final Reality: no vendor requirements found. Search result only; the DX6 figure is a forum claim. https://www.vogons.org/viewtopic.php?p=393825 and https://community.hwbot.org/topic/240961-running-old-3d-benchmarks-with-modern-hardware-and-os (not opened; search snippets).
- 3DMark99 Max readme (read in full): https://discmaster.textfiles.com/file/21267/HACKER16.ISO/Utils/3dmark99max.exe/data1.cab/Program_Executable_Files/help/Readme.htm . Z-buffer and textures are not stated as requirements; the readme is silent on both, not a statement that they are optional.
- 3DMark2000: figures from a forum post quoting the documentation (secondary), matched by https://en.gamegpu.com/proizvoditelnost-3d/predistoriya-gpu/3dmark-2000 and forum https://forums.anandtech.com/threads/win2k-direct3d-f-ked-up-3dmark-2000-crashes.381086/post-419865
- 3D WinBench 98: no requirements found. It is a game-replay style test per https://www.tomshardware.com/uk/reviews/3d-winbench-98,57.html ; that implies textured scenes (my inference, not a source claim).
- SDK samples: the DirectX 2 SDK Direct3D samples readme (read in full) lists Tunnel, Twist, Flipcube as immediate mode with no textures or Z mentioned; Vertcol RGB only; Triangle texture. https://discmaster.textfiles.com/view/29526/WWLCD1.ISO/pc/directx2/sdk/samples/d3dsamp.txt . That is the DX2 readme; DX7 sample behaviour (and Boids, not in it) is not verified. A sample that creates a Z buffer without checking caps would still fail; the readme does not say.
- Quake: dosdays lists 486DX4-100/8 MB https://www.dosdays.co.uk/topics/early_3d_games.php ; gamesystemrequirements lists Pentium 75, 8 MB DOS / 16 MB Win95 https://gamesystemrequirements.com/game/quake . These conflict. GLQuake needs Voodoo/MiniGL: https://www.falconfly.us/info/readme-minigl-100.txt . Timedemo needs the full game's PAK1.PAK: https://www.computerbase.de/forum/threads/quake-teil-1-benchmark.2001515/
- Not found: any other period Direct3D benchmark with a stated requirement (e.g. D3D RM tests, Expendable-style demos): not verified.

## 2. Games

(a) Real DX7 titles (all beyond this machine)
- Unreal Tournament (1999): Pentium, 32 MB, 8 MB VRAM, DX7 (dosdays, link above). Beyond: 6x RAM, 8x VRAM. Needs Z and textures.
- Quake III Arena (1999): Pentium II 233 or 266 (page contradicts itself), 64 MB, 3D accelerator, DX7 (dosdays). Beyond; needs everything.
- Other DX7 titles: not searched, not verified.

(b) DX2-6 and software-renderer candidates, ranked by chance of running. Ranked by lowest published minimum, then software path. The ranking is my judgement from the minimums, not from any test.

| # | Title | Published minimum | Software mode / API | Needs from us |
|---|---|---|---|---|
| 1 | Descent II (1996) | 486DX2-50, 8 MB DOS; 16 MB Win95 | Own renderer, DOS version suggested under 16 MB | nothing |
| 2 | Duke Nukem 3D (1996) | 486DX2, 8 MB | Software only | nothing |
| 3 | Descent (1995) | 386DX, 4 MB | Software only | nothing |
| 4 | Doom / Doom II | 386, 4 MB | Software only | nothing |
| 5 | Screamer, Screamer 2, Destruction Derby, Need for Speed | 486DX2-66 or DX4, 8 MB | Software, DOS | nothing |
| 6 | Quake (1996) | 486DX4-100 or Pentium 75, 8 MB | Software renderer; DOS needs 8 MB | nothing, RAM short |
| 7 | Tomb Raider (1996) | Pentium 60, 8 MB, DOS 6 or Win95, SVGA | Software; DirectX not listed | nothing |
| 8 | Diablo (1996) | Pentium 60, 8 MB (16 MB multiplayer), DX3 compatible | DirectDraw 2D, one source | DirectDraw only |
| 9 | Warcraft II | Pentium 60 or 90 (sources disagree), 16 MB | 256-colour SVGA | DirectDraw only |
| 10 | Red Alert | Pentium, 8 MB (16 recommended) per a secondary quote | DirectDraw | DirectDraw only |
| 11 | Age of Empires (1997) | Pentium 90, 16 MB Win95, 1 MB VRAM, DX5 | 2D | DirectDraw only |
| 12 | Hellbender (1996) | Pentium 75, 8 MB (database); trial readme recommends P90, 16 MB, DX2 | Direct3D advertised; software renderer: not verified | triangles; textures unknown |
| 13 | Monster Truck Madness (1996) | not verified; recommended P166, 32 MB; DX2+ | not verified | not verified |
| 14 | Jedi Knight (1997) | Pentium 90, 16 MB, DX5.2 | Own software renderer per a forum post; not in the requirement pages | software path only |
| 15 | Total Annihilation (1997) | Pentium 100, 16 MB Win95 | 2D | DirectDraw only |
| 16 | Carmageddon (1997) | Pentium 75, 8 MB, 1 MB VRAM | not verified | not verified |
| 17 | Quake II / Need for Speed II SE | Pentium, 16 MB, DX5 | Software mode: not verified | not verified |

Sources: dosdays table https://www.dosdays.co.uk/topics/early_3d_games.php (Descent, Descent II, Duke3D, Doom, Screamer, DD, NFS, Quake, Carmageddon, NFS II SE, Quake II, JK);
Descent II manual via https://manualmachine.com/gamespc/descentii/1119338-user-manual ;
Tomb Raider https://gamesystemrequirements.com/game/tomb-raider-1996 ;
Diablo https://elisoftware.org/wiki/Diablo_(PC,_CD-ROM)_Blizzard_Entertainment_-_1997_UK_Release ;
Warcraft II https://respawn.media/games/warcraft-ii-tides-of-darkness/system-requirements and https://blizzplanet.substack.com/p/warcraft_ii_tides_of_darkness___system_requirements ;
Red Alert https://ensigame.com/games/command-conquer-red-alert/system-requirements (secondary);
Age of Empires https://infania.net/misc/kbarchive/kb/174/Q174850/index.html (Microsoft KB);
Hellbender https://elisoftware.org/wiki/Hellbender_(PC,_CD-ROM)_Microsoft_-_1996_USA,_Canada_Release_(NFR) and the trial readme https://discmaster.textfiles.com/file/24014/1996-10_Disc_2.9.iso/hellbend/readme.doc/readme.pdf ;
Monster Truck Madness https://discmaster.textfiles.com/file/11390/Millennium%20Gold%202000%20-%20Disc%202.iso/Monster/README.TXT ;
Jedi Knight https://store.steampowered.com/app/32380 (Steam page) and software-renderer remark https://www.vogons.org/viewtopic.php?p=385781 ;
Total Annihilation https://www.zx.net.nz/mirror/www.atari.com/us/support/faq/totala_pc.html .

Notes:
- DOS titles (1-6) bypass the Win95 HAL entirely. They test the 5 MB / CPU limit, not the driver. Only titles 8-12, 14, 15 touch DirectDraw or Direct3D under Windows.
- No source gave a memory-limit workaround that lets a title run under 8 MB. Descent II's manual points to the DOS version below 16 MB.
- Not found: a Direct3D title with a published 486 minimum. All Windows Direct3D titles found list Pentium.

## 3. Recommendation

Benchmarks, in order:
1. DirectX SDK Tunnel, Twist, Flipcube (Direct3D immediate mode, if a sample set is installed). Proves triangles, flat and Gouraud, and flipping through a real D3D application. Check each creates no Z buffer or texture.
2. Textureless SDK samples next (Egg, Faces, Hier, Quat, Fly). Proves retained-mode lighting with RGB. Viewer needs the RGB device.
3. Quake `timedemo demo1` in software mode, with the RAM limit stated. Proves CPU and 8 bpp memory throughput; uses no D3D. May not start at 5 MB.
3DMark and Final Reality: skip until a textured and Z-buffered HAL exists, and even then the 32 to 64 MB figures stop them.

Games, in order:
1. Descent II (Windows 95 version). 486DX2-50 listed, own renderer, nearest to the CPU class; 16 MB Win95 vs 5 MB tests the memory limit.
2. Diablo. DirectDraw 8 bpp; proves page-flip and surface paths against a real game, 8 MB listed.
3. Duke Nukem 3D. 486DX2, 8 MB; a pure CPU/VGA baseline, no HAL.
4. Hellbender. The only Direct3D title found with a Pentium 75 listing; tests whether flat triangles are enough. Software path unknown.
5. Warcraft II or Age of Empires. DirectDraw, 16 MB; shows the 5 MB limit on a 2D title.
Nothing here is expected to prove textures or Z; those need a HAL change first.
