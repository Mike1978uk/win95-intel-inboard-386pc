# Playable 3D on the 5160 - plan and tracker (owner, 2026-11-17)

Goal (owner): **playable demos and games on this hardware, under Windows 95 and DOS, showing DirectDraw and
Direct3D on the Mach8 - and a 3D engine of our own that runs period engines' work on the card.** The Windows
work so far is the education; DOS is where the most playable results are expected.

This doc holds the tracker. `docs/worklist.md` section **P** points here and stays the single plan; the
X items it already has are mapped below, not duplicated.

## Where we stand (card, 2026-11-17)

| Program | Rate on the card | Path |
|---|---|---|
| twist (D3D SDK) | ~5.7 fps | our HAL, joined patches, 16 bpp |
| flip2d (DDraw SDK) | 2.8 fps | 8 bpp Lock + Unlock, describe at Unlock (X17) |
| M8SPAN primitives | span 20.4 us, polygon fill 0.2 ms + 35 ns/px, line 21 us/vertex | engine, measured 11-02 |

So the card can fill a flat 50x50 triangle in ~0.3 ms. A 300-triangle flat scene is ~100 ms of engine work
before the CPU: about 10 fps. That is the ceiling to beat, and why flat and Gouraud-banded polygons are the
showcase, not textured frames.

## What "playable" means here

A title counts as playable when, **on the card, measured**: it sustains **>= 12 fps** in its normal mode
(>= 8 for strategy and flight), input reaches the screen in under 150 ms, and it runs 10 minutes without a hang.
Every "playable" claim names the fps, the build hash and the capture folder.

## Why DOS is expected to win - and what it does not change

- **Same bus.** ISA byte I/O is still 5.7 us/B and an OUT 7.64 us. DOS does not make the card faster.
- **What DOS removes:** GDI and its redraws, the Lock/hash/describe-after-the-fact cost (the program tells us
  what it draws), paging on 5 MB (~2,900 page-ins per flip2d run), VMM ring switches, D3DIM (~80 ms a frame
  in tunnel, Z29), and the V9x agent. The whole 486 and all the RAM are the game's.
- **What carries over from Windows:** describe, don't scan; joined same-colour patches; palette from the
  program's hues; deferred clear; 1-bit masks through colour expansion; feeder overlap; CRT_OFFSET flips;
  FIFO slot counting; I/O flushes L1 once per burst. All of it is in `drivers/mach8_display/src`.

## Architecture: one core, many front ends, two back ends

```
 front ends (what programs call)          core (platform-free C)            back ends (to the card)
 ------------------------------          ----------------------            ----------------------
 Direct3D HAL (exists, m8d3d.c)  --+                                   +-- Win95: M8DD32.DLL + driver
 DirectDraw HAL (exists, m8dd*)  --+--> transform/clip, depth sort, ---+
 own M8 3D API (new, DOS)        --+    joined patches, palette,       +-- DOS: 32-bit lib (Open Watcom,
 Glide 2.x subset GLIDE2X.OVL    --+    describe-to-commands,          |   DOS/4GW), direct port I/O,
 VBE/AF driver VBEAF.DRV (2D)    --+    command stream                 |   raster-timed flips
 BRender .BDD / CGL (if ABI)     --+                                   +-- (bed: same code, pixels only)
```

**Compatibility layer (owner, 11-17):** "our software should give us a compatibility layer to run dos 3d titles
written for the other drivers". Each period API reaches its card through one replaceable file (`VBEAF.DRV`,
`GLIDE2X.OVL`, BRender `.BDD`); ours of each, over the one core, lets unchanged games run on the Mach8. Every API
reduces to scan spans and trapezoids, which P1.7 proved on the card. Titles wanting textures and Z look as on our
Direct3D HAL (flat, banded); they run, but not as on the card they were written for.

The core is factored out of `m8d3d.c` and the X17 Unlock path, not rewritten. The engine layer
(`m8hw.c` primitives: fills, polygon fill cmd 5, colour expansion, copies, flips) gets a DOS build.

## Tracker

Status: `todo`, `doing`, `done <hash>`, `parked`, `dropped`. Where: desk / B bed / C card (owner present
for card runs). Each item lists the measure that closes it. Update this table in the commit that changes it.

### P1. Finish the DirectDraw lessons (Windows, now)

| ID | Item | Where | Closes when | Status |
|---|---|---|---|---|
| P1.1 | One-pass 1-bit mask build (X17.2), masks default on if it wins | desk, C | flip2d fps on card, twice | doing: C one-pass, no stores in the loop: build 0.150 -> 0.055 s, **flip2d 2.8 -> 3.37 fps** (`docs/captures/2026-11-17_regmask_card/`, bed pixels clean). Left: asm (60 cycles/px), default on |
| P1.2 | Row hash and extent scan | desk, C | flip2d fps | doing: hash without imul 0.070 -> 0.043 s (RAM floor 0.024). Left: extent scans by dword, fuse hash + scan + build into one read of each row |
| P1.7 | **Spans at Unlock**: a changed row's single-colour runs sent as engine spans, priced against masks per run (`[ddraw] spans=1`, default on). Same primitive as VBE/AF `DrawScanList` and the DOS core | desk, B, C | flip2d fps | done 11-17: ~480 spans a frame, send 0.061 -> 0.018 s, **flip2d 4.13 fps** (`2026-11-17_spans_card`, bed pixels clean) |
| P1.8 | Where the ~1,000 page-ins a run are | C | counted per entry point | done 11-17: Flip 158-207, Blt 7-13, Lock/Unlock 0; ~80 % are outside our driver (flip2d, DirectDraw, GDI code on a full 5 MB). Not ours to fix in Windows; absent under DOS (`2026-11-17_callbacks_card`) |
| P1.9 | Held clear wipes only what the page's last flush found drawn | desk, B, C | Blt time | done 11-17: Blt 37 -> 11 ms a frame, 126 of 130 clears partial, **flip2d 4.33 fps** (`2026-11-17_partclear_card`, bed clean) |
| P1.10 | Fused row pass in asm: hash, extents and spans in one read of each row, no stores in the loop | desk, C | flip2d fps | todo - next (hash 0.047 + find 0.028 s; RAM floor 0.024) |
| P1.11 | Whole faces as engine polygons (cmd 5) when a colour's spans form one convex shape: one command a face, not one a row | desk, B, C | send time | todo |
| P1.12 | Split the rest of Flip (~0.05 s untimed) | C | timers | todo |
| P1.13 | **Regression and spread** of the 11-17 changes: every DDraw SDK sample (wormhole, donuts, stretch, fswindow, memtime) and twist/tunnel on the card; then carry the lessons over: store-free loops in `m8d3d.c` join/sort and the 16 bpp paths, imul-free hashes in `hash.asm` and 16 bpp detection (#54), spans at Unlock for MTM (X13), Fury3, Doom 95; page-in counters on twist | B, C | no sample slower or wrong | todo - first next session |
| P1.3 | Feeder overlaps the send with the program's next frame | C | flip2d ~5 fps target | todo |
| P1.4 | Mode-change selector damage (X19) - **gate for any public showcase** | B (gdb), C | flip2d then PERFLOG from a 16 bpp desktop, 5/5 clean | todo |
| P1.5 | Per-program display mode launcher (X21) | desk, C | a title starts in its mode without a manual change | todo |
| P1.6 | 86Box `CRT_OFFSET` at the next frame (X22), so the bed flips pages | upstream code, owner decides | bed flip2d shows even frames | todo |

### P1-S. Spread every learning across the driver (owner, 11-17: "keep using the learnings in one path across the whole")

A win in one path is not done until each place below has been applied and card-measured, or marked "n/a" with the
reason. New learnings get a row the day they are found. Measure before and after on the card; bed for pixels.

| ID | Learning (where found) | Apply to | Status |
|---|---|---|---|
| S1 | **No stores in inner loops:** the L1 writes through to the slow Inboard bus; a spill or memory RMW per pixel cost ~45 of 106 cycles (8 bpp mask build) | S1a `m8d3d.c` patch join, depth sort, triangle setup; S1b 16 bpp paths (`fb16.c`, `fb16cv.asm`, Lock/Unlock 16); S1c 16-bit driver `hash.asm`, `blt.asm`, `spans.asm`, `text.asm`; S1d GDI caches (glyph, bitmap); S1e the DOS core from the start | todo (check each with `wdis` for spills) |
| S2 | **No imul in hashes:** add/rotate, 75 -> 47 cycles a dword | S2a `hash.asm` (16-bit driver); S2b 16 bpp change detection (#54); S2c string/glyph cache keys | todo |
| S3 | **Never rewrite what is already right:** wipe only what the last frame drew (Blt 37 -> 11 ms) | S3a Direct3D per-frame clears and Z-less sort buffers; S3b 16 bpp held clears; S3c DOS core frame clear | todo |
| S4 | **Describe, then send the cheapest shape:** spans priced against masks against pixels per run | S4a Lock-heavy titles: MTM (X13), Fury3, Doom 95; S4b 16 bpp Unlock; S4c Direct3D fallback paths that Lock the back buffer; S4d DOS core as its base primitive | todo |
| S5 | **Read each row once:** hash, extent and runs in one pass (RAM 78 ns/B is the floor) | S5a 8 bpp flush (P1.10); S5b 16 bpp change scan; S5c Lock held-row hashes | todo |
| S6 | **Count before theorising:** page-ins and ticks per entry point showed 80 % of paging outside our driver | S6a same counters on twist/tunnel (Direct3D callbacks); S6b on MTM; S6c GDI desktop idle | todo |
| S7 | **Allocation already page-locked:** `GMEM_FIXED` from a DLL is locked, so pinning added nothing; locked RAM is the 5 MB budget | S7a audit every fixed allocation (shadows, caches, D3D buffers) for size; S7b release the 8 bpp shadow when DirectDraw ends | todo |
| S8 | **One FIFO check per batch, height register written once** (spans) | S8a every engine burst in `m8hw.c` and `m8d3d.c` feed; S8b XFree86-style slot counting (worklist A.6) | todo |

### P2. Windows showcase: titles that play (DirectDraw and Direct3D)

Order is cheapest and most likely to play first. Each: bed run (pixels), card run (fps), verdict, capture.
Files: `docs/test_titles_2026_11_13.md`, `Downloads\test_titles\`.

| ID | Title | API | Why it is here | Status |
|---|---|---|---|---|
| P2.1 | Donuts (SDK) | DDraw 8 bpp, keyed blits | a real arcade game, tiny sprites; silent at 30 s in the bed | todo |
| P2.2 | Doom 95 shareware | DDraw 8 bpp | 486/50 title; `-emulate` gives the A/B | todo |
| P2.3 | Red Alert Win95 demo | DDraw 8 bpp | scrolling + keyed blits; `HardwareFills` uses our fills | todo |
| P2.4 | Warcraft II demo | WinG | 486/33 title; DIB blits path | todo |
| P2.5 | Hover! (Win95 CD) | nv | 486/33 3D first-person; the period's own showcase | todo - check API first |
| P2.6 | Fury3 trial | Lock-heavy 8 bpp | 486DX/66 minimum; tests X17 on a real game | todo |
| P2.7 | Direct3D flat demo of our own on the HAL (cube field, flat + Gouraud bands) | D3D | shows D3D on the card at a playable rate where SDK samples cannot | todo |
| P2.8 | Hellbender trial, MTM trial (X13) | D3D | long shots; MTM parked on whole-buffer Locks | parked behind P2.1-P2.7 |

Target: **three P2 rows playable** by the definition above, one of them Direct3D.

### P3. The engine core and the DOS back end

| ID | Item | Where | Closes when | Status |
|---|---|---|---|---|
| P3.1 | Factor the platform-free core out of `m8d3d.c` (no behaviour change; twist fps and pixels unchanged) | desk, B, C | twist pixels = before, fps within noise | todo |
| P3.2 | DOS card layer: `m8hw` primitives built for DOS/4GW (Open Watcom, already our compiler) | desk, B | M8SPAN numbers reproduced from DOS on the card | todo |
| P3.3 | DOS bench: flat triangles/s and Gouraud-band triangles/s, 640x480x8, page-flipped, raster-timed | C | numbers in `docs/captures/` | todo |
| P3.4 | Port the SDK tunnel/flipcube scene to DOS on the core (Z29's "what leaving Windows saves") | C | DOS fps vs Windows fps, same scene | todo |
| P3.5 | M8 3D API (own front end): begin/end frame, triangles, strips, flat/banded colour, palette, overlays/text | desk | P3.4 and P4 build on it | todo |
| P3.6 | Shape and glyph cache in spare VRAM for HUDs and sprites (X17.4) | desk, C | HUD costs ~0 bytes a frame when static | todo |

### P4. Playable on DOS - our own demos first

| ID | Item | Why | Closes when | Status |
|---|---|---|---|---|
| P4.1 | **Flat-shaded flight/space demo** (Elite-like: ships, planets as banded discs, starfield) | the style the engine fills best; 1984-era look at 640x480 | >= 12 fps, playable 10 min | todo |
| P4.2 | **Raycaster** on Wolfenstein 3D source (GPL, id), walls as engine column fills, floor/ceiling as fills | the raycaster was the model in A.8; columns at ~20 us each | >= 12 fps at 320 columns | todo - read source first |
| P4.3 | Stunts via restunts (builds DOS exe from patched disassembly): hook its polygon draw | runs unchanged at 512 KB; flat polygons | draws on the engine | todo - read restunts first |
| P4.4 | Combined showcase: one menu disk, DOS and Windows parts, each with an fps counter on screen | the thing to show people | owner happy to film it | todo |

### P5. Period APIs - for ports and for learning

Each gets an offline ABI read first (no card), and a stop rule: if the ABI is not recoverable in one session, park it.

| ID | API | File swap | Learning value | First step | Status |
|---|---|---|---|---|---|
| P5.1 | **VBE/AF** (`VBEAF.DRV`, Allegro / SciTech MGL games, 2D acceleration) | yes, a loaded driver file; FreeBE/AF is an open driver set to copy from (nv for Mach8) | high: real DOS games, 2D blits and fills = our strongest paths | read FreeBE/AF source and the VBE/AF 1.0 spec | todo |
| P5.2 | **Glide 2.x subset** `GLIDE2X.OVL` (Z29) | DOS Glide titles load it as an overlay (nv per title); 3dfx Glide source is public | high for learning the API shape; titles want a Pentium | read Glide 2 headers/source; list DOS titles that ship the OVL | todo |
| P5.3 | **BRender `.BDD`** (X24) | shown: a renamed `mcga.bdd` ran | MIT source; flat/Gouraud renderer in `softrend` | read the device export table (1.1.2 DOS tree, 1997 tree) | todo |
| P5.4 | **8514/A Adapter Interface** (HDILOAD) | the card's own API | programs exist (Halo, BGI driver); not 3D | find ATI's AI driver and a test program | todo |
| P5.5 | **Creative CGL** (3D Blaster VLB, X16) | linked into the exe on VLB games; separate `.CGL` files only on the PCI Verite builds | the only 486/VLB-era 3D API: what a 1995 card was handed per triangle, how a game split setup between CPU and card | disassemble the Hi-Octane demo exe's CGL entry points; list the Actua Soccer demo files for `*.CGL`; look for the RReady VLB wrapper source | todo - reading |
| P5.6 | **S3D, ATI CIF, Speedy3D, PowerVR SGL, Matrox MSI** | per title, not shown | each solved our problem on a different budget: S3D and CIF triangle setup, Speedy3D's span-based DOS API (dosbox-rendition reimplements it), SGL's scene-at-once sorting without Z - closest to our depth sort | find the public SDK or reimplementation for each, read the triangle and span interfaces | todo - reading |

Read 11-17 (Sonnet, `docs/p5_api_reading_2026_11_17.md`): ranked BRender (MIT, `BrDrv1Begin`, trapezoid cut in
`ZB/TRIGEN.C`), VBE/AF (`DrawScanList`/`DrawTrap`/`PutMonoImage` = our engine's set; FreeBE/AF has no Mach8
driver), Glide 2 (OVL swap shown by Glidos/dgVoodoo/DOSBox-X; OVL format unread). Every API reduces to a trapezoid
or scan list: that is the first primitive to build (P3.2). CGL ABI still not found; ATI CIF header is in
BRender-1997 `ati/` (read, do not copy). Also: heavy titles are run anyway, as MTM was - each failure names a gap.

Rule (owner, 11-17): no DOS 3D engine or API is off the table. Heavier hardware is a reason not to *port* it,
never a reason not to *read* it; each reading row ends with one line in the plan on what it taught us, or what
it did not contain.

### P6. Distribution and installation

| ID | Item | Status |
|---|---|---|
| P6.1 | `dist/mach8` refreshed to each card-proven build, MD5s, byte-identical (X25, `repo-hygiene`) | todo - still `32bfaac` |
| P6.2 | Have Disk INF for the Windows driver; owner checks it on a clean install | todo |
| P6.3 | `M8D3D.INI` documented setting by setting; per-program mode table (P1.5) ships with it | todo |
| P6.4 | DOS kit `dist/mach8_dos3d`: library, headers, demos, VBEAF.DRV / GLIDE2X.OVL when they exist, README with requirements and measured fps | todo |
| P6.5 | Showcase write-up in `docs/` and the README: what runs, at what fps, on what. "First 3D on an XT" is the owner's claim to make: check prior art (XT-class 3D, 8514/A on an 8-bit bus) before it is printed | todo |

### P7. Keeping GitHub current (standing)

- Every verified iteration: **Haiku** commits and pushes from PowerShell, exact paths, subject <= 50 chars.
- `dist/` changes only with a card-tested build; owner verifies a release before it is tagged (G10).
- Issue and PR text: Opus drafts, owner posts. Contributors thanked.
- **Sonnet** for research rows above (P2.5 API check, P4.2/P4.3 source reading, P5 ABI reads) - written brief, no commits, no diagnosis.
- Census captures: keep only the "before" and "during" sections of a run (11-17: 22,000 lines went in).

## Session protocol

**Start:** read the newest `docs/next_session_*.md`, then this tracker. Pick at most three IDs; say which, the
cost of each (desk / bed / card trip) and which need the owner at the 5160.

**End:** update the Status column here and the worklist P line in the same commit as the results; write the
handoff (state, what ran, next three IDs); Haiku commits and pushes; say what is left running.

## Order (proposed, owner decides)

1. P1.1-P1.3 (finish what flip2d is teaching), with P1.4 alongside - it gates showing anything.
2. P2.1-P2.3 in the bed, then one card trip for all three.
3. P3.1-P3.4 - the DOS back end and the "what Windows costs" number.
4. P4.1, P4.2 - first playable DOS demos.
5. P5.1, P5.2 reads (Sonnet), then whichever ABI is cleanest.
6. P6 throughout; P6.5 when P4 has a playable demo.
