# DirectX 1-5 titles as HX DOS candidates, 2026-11-18

Desk research. Nothing run, nothing downloaded. Companion to `docs/hx_dos_directx_2026_11_18.md` and `docs/dos3d_title_ladder_2026_11_18.md`.

Reading the tables:
- **nv** = not verified. Almost every figure here is from memory of box/readme claims. No title page was fetched.
- Two WebSearches for HX + DirectDraw game reports returned **no per-title report** (no compatibility list; generic HX pages only). So the "HX report" column is "none found" for every title except the one Vogons snippet already in the HX doc.
- Machine budget: ~3.5 MB for game + HX runtime; 8 bpp 640x480 shadow surface = 300 KB, so one system-RAM full-screen surface costs ~9% of budget. Lock on the primary = upload over the slow bus.
- Ratings: good = Blt/BltFast sprites, 8 bpp, no D3D, small. maybe = needs work or size unknown. poor = Lock-everything, D3D, or RAM far over.
- Pattern key: **Blt** = off-screen surfaces blitted (engine-friendly); **Lock** = software renderer writes pixels (upload per frame); **mixed** = both.

## 1. DirectX SDK samples (Foxbear is in `test_titles/dx3a_sdk`)

Samples from DX 3-5 SDKs; behaviour from memory (nv). Source of the sample set: Microsoft DirectX SDK, not fetched.

| Sample | DX | DDraw pattern | D3D | DSound | Res/depth | Fit |
|---|---|---|---|---|---|---|
| Foxbear | 3 | Blt: scrolling background strips and colour-keyed sprites from many off-screen surfaces, page flip (nv: video-memory placement requested). Crawls under Win95 here (software emulation + paging, owner) | no | nv (sound/music via DSound/MCI, nv) | 640x480x8 | good as the engine test; surface count and size is the risk (needs ~1 MB of surfaces, nv) |
| Donuts | 1-3 | Blt: BltFast colour-keyed sprites, back buffer flip | no | yes | 640x480x8 | good |
| Duel | 1-3 | Blt sprites, flip; DirectPlay lobby | no | yes | 640x480x8 | maybe: needs DirectPlay (not in HX list, nv); can run single-player path nv |
| Stretch | 1-3 | Blt with stretch (`DDBLT_*` stretch) on primary from off-screen surface | no | no | 640x480x8 | maybe: exercises stretch-blit support, which HX DDDRAW leaves undocumented (`hx_dos_directx` s1) |
| Wormhole | 1-3 | Lock/palette animation tunnel (nv): writes pixels or rotates palette | no | no | 640x480x8 | maybe: if palette rotate only, free on our DAC; if Lock, upload cost |
| Flip2D | 5 (nv) | Blt + Flip between page-flipped surfaces; pure flip timing | no | no | 640x480x8 | good: matches the flip2d bed/card bench already done (`docs/next_session_2026_11_18.md`) |
| DDEx1-5 (ddex) | 1-3 | 1 = primary only; 2 = set mode + palette; 3 = off-screen bitmap Blt; 4 = flip; 5 = overlay/colour key (nv for 4/5) | no | no | 640x480x8 | good: smallest ladder, run first |
| Palette sample, Bounce, Scrnsave | 2-5 | Blt sprites / palette (nv) | no | no | 640x480x8 | good (nv) |
| D3D samples (Tunnel, Twist, Bounce3D etc.) | 2-5 | -- | yes | no | 640x480 | poor until D3D exists; useful later only |

## 2. Retail and shareware candidates

All DDraw titles below ran 8 bpp 640x480 (or 640x400) unless stated. "HX report" = none found for all.

| Title | Year | DX | DDraw pattern | D3D | DSound | Min RAM / CPU (box, nv) | Fit | Note |
|---|---|---|---|---|---|---|---|---|
| Dink Smallwood (RTsoft) | 1997 | 3-5 (nv) | Blt: 8 bpp sprite engine, BltFast colour key | no | nv | nv (small shareware, likely <=16 MB / 486-Pentium) | good | Small and sprite-based; best retail-class candidate by design; verify DX version in the installer |
| Worms (Team17, Win95 build) | 1995 | nv | Blt/Lock mixed (nv) | no | nv | 8 MB (nv) / 486 (nv) | maybe | Landscape bitmap kept in system surface, redraw by Blt (nv) |
| Pandemonium! (Crystal Dynamics) | 1996 | nv | Blt (nv) | no | nv | 8 MB / 486DX2-66 (nv) | maybe | Win95 DirectX port of the PSX game (nv) |
| Close Combat | 1996 | nv | Blt/Lock mixed (nv) | no | nv | 8 MB / 486DX2-66 (nv) | maybe | Map is scrolled; Atomic Games engine details nv |
| Return of Arcade (Microsoft/Namco) | 1995 | 1 (nv) | Blt sprites (nv); uses WinG or DDraw: nv | no | nv | 4-8 MB / 486 (nv) | maybe | Era-correct small DX1; installer needs verification |
| Microsoft Arcade | 1993 | none | WinG/GDI (Win 3.1) | no | no | -- | poor | Not DirectX; listed because it was asked |
| 3D Pinball Space Cadet | 1995 | none | WinG/DIB (nv), not DDraw | no | no | -- | poor | No DDraw back end to attach to (nv) |
| Full Tilt! Pinball | 1995 | none (nv) | WinG (nv) | no | no | -- | poor (nv) | |
| Hellbender (Microsoft) | 1996 | 2 (nv) | Lock software renderer plus optional D3D (nv) | optional (nv) | yes | 8 MB / Pentium (nv) | poor | Already trialled (`test_titles/hellbender_trial`) |
| Fury3 (Microsoft) | 1995-96 | 1-2 (nv) | Lock-style software renderer (ex-WinG) (nv) | no | nv | 8 MB / 486DX2-66 (nv) | poor | Already trialled (`test_titles/fury3`) |
| Doom95 | 1996 | 2 (nv) | Lock/Blt of one 8 bpp frame buffer: upload per frame | no | nv | 8 MB (nv) / 486DX2-66 (nv) | poor for speed, good as a harness | Owned; the DOS original is the better Doom here (title ladder A10) |
| Warcraft II BNE | 1996-97 | nv | Blt/Lock mixed, 8 bpp (nv) | no | nv | 8 MB (nv) / 486DX2-66 (nv) | maybe | Demo owned (`test_titles/warcraft2_demo`) |
| Command & Conquer Win95 (Gold) | 1995-96 | 2 (nv) | Lock into system buffer then Blt; 640x400 and 640x480 (nv) | no | nv | 8 MB (nv) / 486DX2-66 (nv) | maybe-poor | Large assets; full-screen redraw by upload |
| Red Alert Win95 | 1996 | 2 (nv) | as C&C | no | nv | 8 MB (nv) / 486DX4-100 or Pentium (nv) | poor | Demo owned (`test_titles/redalert_demo`) |
| Diablo | 1996 | 2 (nv) | Lock: 8 bpp 640x480 frame in system RAM, copied to primary each frame | no | yes | 16 MB (nv) / Pentium 60 (nv) | poor | RAM over budget and Lock-heavy |
| StarCraft | 1998 | 5 (nv) | Lock/Blt, 8 bpp 640x480 | no | yes | 16 MB (nv) / Pentium 90 (nv) | poor | |
| Age of Empires | 1997 | 5 (nv) | Lock/Blt mixed, 8 bpp | no | yes | 16 MB (nv) / Pentium 90 (nv) | poor | RAM |
| Total Annihilation | 1997 | nv | Lock, 8 bpp | no | yes | 16 MB (nv) / Pentium 90 (nv) | poor | |
| Dark Reign | 1997 | nv | Lock/Blt, 8 bpp (nv) | no | yes | 16 MB (nv) | poor | |
| Jazz Jackrabbit 2 | 1998 | nv | Lock/Blt, 8 bpp (nv) | no | yes | 16 MB (nv) / Pentium (nv) | poor | |
| MechWarrior 2 Win95 | 1996 | nv | software 3D via DDraw (nv); D3D build exists (nv) | variant | nv | 8-16 MB / Pentium (nv) | poor | Listed in HX VOGONS thread: tried, outcome not given (https://www.vogons.org/viewtopic.php?t=19298 snippet) |
| Monster Truck Madness | 1996 | 2 | D3D | yes | yes | nv | poor | Owned, too heavy |
| Flight Simulator 98 | 1997 | 5 | D3D/DDraw | yes | yes | nv | poor | Owned, too heavy |
| Shockwave Assault | 1996 | nv | nv | nv | nv | nv | nv | Owned (`test_titles/shockwave_assault`); not assessed here |
| Hexen95 | -- | -- | -- | -- | -- | -- | -- | No Win95 DirectX Hexen found (nv); DOS Hexen is ladder A14. Name dropped |

## 3. Other names to check (all nv)

| Title | Why listed |
|---|---|
| Descent II (Win95 DX build, 1996) | 8 bpp Lock software renderer; DOS original runs anyway |
| SimCopter / SimCity 2000 Win95 Network Ed. | Maxis DDraw 8 bpp (nv); too large (nv) |
| Earthworm Jim (Win95) | 8 bpp sprite port (nv) |
| Pitfall/PSX-port class titles (Pandemonium!, Crash-type) | Tile/sprite Blt (nv) |

No title in 2.-3. had an HX test report found.

## 4. Order to try (by fit, then cost)

1. DDEx1-4 from the SDK: proves HX load, DDraw call set, and our surface back end with the least code.
2. Flip2D: already benched on the card; compare to a native m8dos flip.
3. Donuts: sprites with colour keys; tests the ignored-colour-key gap in HX DDDRAW.
4. Foxbear: scrolling, many surfaces; RAM and off-screen budget (600 KB spare) test.
5. Dink Smallwood: the only retail-class 8 bpp sprite engine on the list.
6. Stretch / Wormhole: only to size stretch-blit and Lock gaps.

## 5. Not found

- Any HX per-title compatibility list (two searches this session returned only generic pages: https://www.uvlist.net/groups/info/hxdos , https://bttr-software.de/forum/forum_entry.php?id=11263).
- A confirmed DX version or RAM minimum for any retail title above.
- DirectPlay support in HX (Duel).
- Whether HXRT's DDDRAW honours colour-key Blt: DDDRAW.TXT says colour keys ignored (`hx_dos_directx` s1).
