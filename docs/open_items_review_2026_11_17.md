# Open items review, 2026-11-17

Desk read, nothing run. Sources: `docs/playable_3d_plan.md` (plan), `docs/worklist.md` (WL), `docs/next_session_2026_11_18.md` (H18),
earlier handoffs `next_session_2026_10_*`, `_11_*`, `docs/issue_audit_2026_11_14.md` (audit), `docs/mach8_levers.md` (levers),
`docs/mach8_3d_research.md` (3DR), `docs/mach8_frame_rate_plan.md` (FRP, Z/G rows), `docs/test_titles_2026_11_13.md` (TT),
`docs/showcase_3d_candidates_2026_11_14.md` (SC), `docs/p5_api_reading_2026_11_17.md` (P5R), `docs/contributor_input_ledger.md` (ledger).
Line numbers are for the files as read on 2026-11-17. "Used" in section 3 = a grep of `drivers/mach8_display/src` found the register.
Not read in full: the older 10-0x to 10-2x handoffs (grep only) and most `docs/captures/` READMEs.

## 1. Unanswered questions

| Question | Raised where | Status | Cheapest way to answer |
|---|---|---|---|
| L1 on vs off for a 512-word `rep insw` from the XT-CF: does it change speed? | Andrew, #50, 10-23; ledger "OWED to Andrew"; `next_session_2026_10_24.md:48` | Open as a number. The mechanism is answered elsewhere (I/O invalidates L1 once per burst; `docs/fable_pack_f1_flush.md`, captures `2026-11-13_flush_sw1`); draft in audit says "still to be run" | One XTSECT-style run, CTCHIP cache off/on, twice |
| Which slot is the Inboard in on each of Simon's machines; is the 5150 an early board; where does Interrupt level 0 fail in Windows 95? | #10; `issue10_old_bios_2026_09_27.md` sections 14-16 | Part answered: slots J1-J5 and SW values 11-16 (ledger row 67); H1 ruled out by PICCHK 11-17 (section 16). Open: H2 (ports 100h-1FFh), H3 (F0000-F3FFF decode), his machine state; reply to him still "owner to send" (ledger row 68) | H2: log the ports Windows touches in 100-1FF in the bed; no help needed from Simon |
| Does the Monster Floppy at the lowest option-ROM address corrupt data in emulation? | Andrew, #18, 09-09; audit "stale claim" | Open; the repro bed was never built | `next_session_2026_10_21.md:116`: copy `vm_5160_mcp`, add `fdc = monster_fdc`, one boot |
| FastDoom through the VGA-half memory path (1.83 us/B, no flush): does it help? | Andrew, #47, 10-05; audit; WL X12 | Open; reply owed; not built | Reply draft exists (audit). Build is X12 / Z26; first a Win16 or DOS probe of 64 KB through A000 |
| With the dashboard off and chase view, does MTM stop Locking art? | Plan P2.9; settings did not save to `MTMHW.INI` (11-14) | Open; owner parked MTM, bed-only work allowed | One bed run, `[diag] runs=1` |
| Paging during a Direct3D demo, and does RAM compression help? | WL X20 (line 401); H18 "Not done today" | Open. S6a counted page-ins in our entry points on twist (~0), not total paging | PERFLOG paging column on one twist run on the card |
| Does DEST_CMP_FN work on SCAN_TO_X spans and LINEDRAW (gates the depth-band Z trick)? | 3DR section 4 q4; FRP Z8 "Not written" | Open; measured only on CMD fills | One M8SPAN arm with a compare set, card, twice |
| Is a 28800 unchained (Mode X) write still 1.86-2.18 us/B for 4 pixels? | 3DR section 4 q6; WL X1, X8 | Open | One Win16 or DOS probe, map mask 0Fh |
| Palette writes per frame: visible snow? | 3DR section 4 q7 | Open (cost measured 837-869 us / 64 entries, snow not recorded) | Owner's eye on one palette-rotation run |
| Vertical-sync count in M8SPAN arm F | 3DR section 5 "F"; "measurement loop is wrong" | Open; flip timing itself is used in `m8dd32.c:2333` | Redo with DISP_STAT bit 1 as in `tools/m8seq/M8FLIP.ASM` |
| CRT_OFFSET: latched at vsync or immediate? | 3DR section 4 q3 | Likely answered by M8PAGE / Z7 "page wait 3.9-7.6 ms" and the card flipping +1 per frame (H18); no single doc states the latch rule | Read `m8dd32.c:2333` comment against M8PAGE capture; close or one probe |
| Does the engine ever finish slower than the bus; long fill queued ahead of the FIFO? | 3DR section 4 q1-q2 | q1 answered (M8SPAN A/B: bus-bound, width independent); q2 open | One probe: 640x480 clear then 16 writes |
| Can the CPU read and write VGA memory while the Mach8 drives the output? | Owner, C1 (WL 152) | Answered in part: L4c "both halves stay usable"; M8XMEM memories do not cross. Not measured under load | Same probe as the Mode X row |
| GE-idle interrupt: does the card raise an IRQ when the engine idles? | FRP Z22 "unverified", "static sources first" | Open | Read guide and board jumpers; no card |
| M8CURVE hang (card hung twice, once on a fresh boot) | WL X10; `next_session_2026_11_08.md:229-231`; F4 in `next_session_2026_11_16.md:100-102` | Open, cause unknown. Matters for P1.11 (faces as polygons) | Fable F4 pack, committed first |
| Boot 429 startup hang, sound looping, same as 11-05 | H18 "Card hang"; #55 | Two observations, no cause; "not chased" | Compare with `captures/2026-11-05_baselines/README.md`; no run |
| Does `m8_switch_in` drop the DLL's card state after a DOS-box round trip? | WL X23(c), Fable F3 5 | Open | Read `m8dd_mode_changed`, one bed round trip |
| ~9.5 ms per XT-CF command is Windows or a tick; which? | `next_session_2026_11_16.md` "Learned" | Open | A trace before any theory |
| XT-IDE finding 1: DRV/HEAD under `-SkipDrvHd` after an INT 13h reset | WL H (line 440); `fable_review_2026_11_13_xtide.md` | Open; owner decides | DEBUG `i 30c` probe on the card |
| Hover! API; Doom 95 DDraw version; Shockwave Assault's genre | TT rows 20, 10, 15 ("nv") | Open | Sonnet: strings and readmes from files on disk (not Hover) |
| Is "first 3D on an XT" prior art? | Plan P6.5 | Open | Sonnet web read, one line per find |
| Does Z27's "M8TEX/M8SHAPE already drive the card from DOS" hold? | FRP Z27 | Doubtful: `tools/m8probe/M8TEX.C`, `M8SPAN.C` are Windows EXEs (M8SPAN ends with the driver's resync escape, 3DR section 5). DOS drivers of the card are `tools/m8seq/*.COM` | Correct Z27 in place; P3.2 starts from the `.COM` set-up |
| Plan P1.6 says "owner decides" on the 86Box `CRT_OFFSET` fix; H18 task 6 says "owner wants it fixed" | Plan line 81; H18 line 98 | Rule contradiction in the docs | Owner to say which |
| TC1995: may he merge our Graphics Ultra work into the add-on and Mach32? | 86Box Discord 10-16; ledger; WL A8 | Open: owner asked him to share the PR; none recorded | One Discord question |
| Notify Michael Dale of the tray-flash patch | H18 "Agent fixed" ("draft owed") | Open | Draft only; owner posts |

## 2. Outstanding items not in the playable plan

| Item | Source | Suggested P-section or close |
|---|---|---|
| Replies owed: #50 and #47 (Andrew), #18 status, #10 (Simon, rows 65-68), #55 and #59 updates, #49 stale text | audit "Contributors not told"; ledger lines 21-30, 65-68, 341; `next_session_2026_11_07.md` ("not posted") | P7: a standing line "replies owed", Opus drafts, owner posts, thanks included |
| README stale: open-issues table lists closed #23, #28, #29, #31, #35, #45 and omits #50-#59; line 95 1024x768 claim; #49 row; `XTIDEMP.MPD` name | audit "Elsewhere in the repo" | P6 (repo-hygiene G-gates), before P6.5 |
| `dist/mach8` is `32bfaac`; card runs `a2f862c`/`92add43`; `dist/mach8/README.md` "Known problems" unchecked | Plan P6.1; H18 "Next" 5 | P6.1 (already there); owner verifies (G10) |
| `dist/xtide_mpd` (10-11, `a67fefc4`) vs card `XTBOTH.MPD` `DAEF6830`; `5160_tuned_config.md` names 11-14 files | audit; `next_session_2026_11_15.md` | P6, new row "dist/ for storage drivers" |
| Branch cleanup: `claude/xtcf-tight-taskfile` merged, delete; five unmerged branches without PRs | audit | P7 |
| Worklist drift: line 317 says "S1-S8", plan has S1-S9; X17 (line 386-398) says "items 1-4 not built" in its last line while items 1-2 are done; two items named X4 (lines 344, 420); X10 sits after X26; header "Now (10-21)" | `worklist.md` | Fix in place (feedback-amend-wrong-docs); P7 |
| Fable leftovers: F2 4 (`m8d3d_join` count), F3 4-5 (teardown owner check, `m8_switch_in`), F4 (M8CURVE), XT-IDE 1 and 7 (X4 wedged drive) | WL X23, H, E-X4; `next_session_2026_11_16.md:100-102` | P1-S new row S10 "Fable leftovers"; X4 close as a storage item, separate from P |
| Owed card runs from 11-15: F3 build with feed on, Alt+F4 three times; F2 3 `m8d3d_stat[25]`; `[diag] phase=1` vs 0 | `next_session_2026_11_15.md:107` ("Mach8 card run") | Check whether done 11-16/17; else add to the P1.13 card trip |
| X14 steps 2-7 not mapped: Shockwave Assault demo, Foxbear (DX3a), WinBench 96 and CrystalMark GDI benches, Memtime numbers, fswindow body | WL X14 (361-385); TT | P2: add rows P2.10 Shockwave, P2.11 Foxbear; GDI benches to P6 (comparison numbers) |
| P1.4 gate: fswindow dialog body not drawn, did not close on Alt+F4 | `captures/2026-11-15_x14_ddraw_bed/README.md` | P1.13 spread list; no P row |
| X10 D3D playable approximations D1-D12 (blends, bilinear look, perspective, mips, palette ramps) | WL X10; FRP "Direct3D: playable approximations" | P2.7 (our demo) draws from it; close the rest as "behind P2" |
| X2-X5 16 bpp surfaces and GDI caches in spare rows | WL 342-346 | S9b (room at 16 bpp, design) |
| X11 burst-shaped sends; Z17/Z25/Z31 feeder VxD, queued flip | WL X11; FRP Z17, Z25, Z31 | P1.3 (feeder); Z31 as P1.3b |
| X12 / D5(b) / D6 / X1 / X8 / Z26 VGA half, Mode X, VGA-half framebuffer | WL 230-256, 356; Z26 | New P section "VGA half" or park: it is the only 1.83 us/B path (Doom-class DOS and Windows) and nobody owns it in the plan |
| X13 MTM: lockscan black screen, stall before first Flip, start-view patch | WL X13 | P2.8/P2.9 (parked by owner) |
| X26: own DirectX stability A/B; DirectSound for the SB Pro (#58) | WL X26 | DirectSound: Donuts and Foxbear are silent without it; add to P2 as a dependency |
| Z6 per-program 320x240 choice; Z7 third page; Z8 depth bands; Z9; Z10-Z14 detail layers, Penrose pattern, scanline mode; Z24 x87 control word | FRP Z6-Z14, Z24 | Z6 into P1.5; Z8, Z7 section 3 below; Z10-Z14 are twist/tunnel look, park with MTM |
| Worklist S1-S5 sound on the same bus (SB DMA, ADPCM, mix on the 486, OPL cost Z28) | WL 297-310 | P4 (DOS demos need sound budget); S1 measure first |
| D1 TXTBENCH, D2 glyph cache (#49), D3 vector probes, D4 >256 colours, D8c 16-colour modes, D9 remainder (styled pens, polyline range, Pixel gets, ScanLR, StretchBlt, DibToDevice edges, LRU, transparent draw) | WL 218-295 | D3 into P4.1; D4 into section 3; D8c, D9 remainder: close as "GDI parity, not the playable goal" or park |
| A1, A4 chapter 9 and appendices "after the PR": the PR merged 10-11 (#8289) | WL 77, 89, 109 | Close A1/A4 as unblocked-but-low; A8 siblings and A10 harness: P3 only if the DOS core needs a host-side replay |
| A3 model gaps: LINE_SYNC absent; bed fills rectangles, does not model POLY_MODE through LINEDRAW (3DR section 5) | WL 86; 3DR section 5 "Bed" | **Check before P1.11**: if still true, bed pixels cannot judge polygon faces; add as P1.11 precondition |
| X22 86Box CRT_OFFSET at next frame | WL 403; H18 task 6; plan P1.6 | P1.6 (exists); see contradiction in section 1 |
| B1-B5 28800 knobs; B6 custom ROM; C1-C4 | WL 136-168 | C2/C3 dead (L4c); B2 AB bits negative; B4 write-through cache is the same question as X12; close the rest |
| E: T2 DOS box and serial timer; T3 cost per tick | WL 414-415 | Close or park (storage/timer track, not P) |
| H: Andrew's #50 ideas (queue writes per call; upstream timing fixes; WINDISK ideas #52; BlueSCSI; CH376) | WL 432-442 | "Queue writes per call" is S1/S8; rest park under storage |
| `docs/cross_driver_levers_2026_11_01.md` (levers for other drivers) | WL line 441 | Park until P1-S has no todo |
| Parked: IE4 broke disk driver; WebCam II on LPT; Jaz 2 GB note | WL 443-446 | Unchanged; Jaz note is a one-line doc edit |

## 3. Unused silicon and card features

Value is for the playable goal (>= 12 fps, 640x480x8 or 320x240 lores, 8-bit ISA).

| Feature | Used today / not used | Value | Next step |
|---|---|---|---|
| Polygon fill (CMD 5 / LINEDRAW `POLY_MODE` + poly blit) | Used by D3D patches: `m8.h:122-123`, `m8hw.c:1297`, `m8d3d.c`. Not used for DDraw faces | **High**: 0.2 ms + 35 ns/px, any size; the core of flat 3D (3DR section 5) | P1.11 (check bed model first, section 2); DOS core P3.2 |
| SCAN_TO_X spans | Used (`m8hw.c`, `m8d3d.c`); spans at Unlock P1.7 | **High**: 20.4 us a span; base of VBE/AF `DrawScanList` | Done on Windows; first primitive of the DOS core |
| Source colour compare 92EE/EAEE | Used (`m8hw.c:1915`, `m8dd32.c:244`): DDraw keyed blits, 869 copies/s | **High**: sprites, HUD, scaled-sprite games (G6) | Static-art cache S9a, DOS P3.6 |
| Off-screen VRAM caches (strings 83 %, bitmaps 95 %, textures; shape cache not built) | Partly (WL D9; X17.4 and S9 unbuilt) | **High**: the MTM lesson; cockpit/HUD for nothing per frame | S9a design, then P4.5 |
| CRT_OFFSET page flips | Used (`m8dd32.c`, `m8.h:174-175`) | **High** (done) | Bed cannot show it until X22 |
| DP_CONFIG mono expansion (1-bit masks, text) | Used (masks, X17.2) | **High**: 8x fewer bytes than pixels | P1.1 asm, default on |
| Palette ramp lighting and rotation | Palette from hues used; ramp lighting not | **High** for P4.1: shade by index, 64 entries ~0.84 ms; no pixels sent | Ramp layout for the flat demo; snow check (section 1) |
| Pre-scaled column blits (raycaster, RealDOOM shape) | Not used; D3 and G3 probes unrun | **High** for P4.2: ~20 us a column; 3DR approach 3 | One probe before P4.2 |
| Raster counter A8/A9 (11.5 us a poll) | Not used (no reference in `src`) | **Med**: raster-timed flips and palette bands in the DOS core (plan P3.3 says "raster-timed"); Windows flips already work off vblank | Read `tools/m8seq/M8FLIP.ASM` for DISP_STAT bit 1; use in P3.3 |
| Destination compare EEEE / depth bands | Not used (no reference in `src`); unmeasured on spans | **Med**: occlusion without sorting, 16-32 visible shades only (3DR section 1) | One M8SPAN arm (section 1) |
| Short strokes 9EE8 | Not used; card-proven M8ROW3 | **Med**: 3.8 us a vector against 21 us a LINEDRAW vertex, for starfield, radar, wireframe HUD | Probe in the vector demo (P4.1) |
| LINEDRAW polylines | Used (`m8hw.c`, `m8d3d.c`) | **Med**: ~1,500 segments per 30 fps frame; wireframe only | Elite-style wireframe ships in P4.1 |
| PATT_DATA 8x8 patterns | Used (fill_hatch; `m8d3d.c:4042` wall patterns) | **Med**: Z12 aperiodic shading at plain-fill cost | Only if P4.1 needs grain |
| Arithmetic mixes, masked writes, 6213h fill from VRAM (Z1-Z3) | Measured on card 11-07 (`captures/2026-11-07_silicon`); WRT_MASK used; Z1 "into the HAL" next | **Med**: translucency, lighting by add, textured patches 1.05x flat | One HAL row after P2.7 |
| Scissors, BOUNDS | Scissors used; BOUNDS read in `m8hw.c`/`m8d3d.c` (1-2 sites) | **Low-med**: BOUNDS is a free erase rectangle (G11) | Fold into S3 |
| Third page at pitch 640 (Z7) | Not written | **Med**: removes the 3.9-7.6 ms page wait | After P1.3 |
| GE-idle IRQ (Z22) | Unverified | **Med if real**: feeder without polling (Z31) | Static read first |
| VGA half / Mode X | Not used (X1 not done) | **Med**: 1.83 us/B, no flush, but 64 KB a frame = ~117 ms (8 fps cap); 5160 only wins with partial updates or 320x200 polygons (3DR row 4, unmeasured) | Probe (section 1), then decide a P section |
| Palette per raster band, temporal dither (D4, G5) | Not used | **Low** for playable; **med** for showcase looks | After P3.3 |
| EXT_SHORT_STROKE C6EE | Card does 45 degree only (M8SSV3) | Low | Close |
| Split read/write banks (BEh bit 3) | Works, no time gain | Low | Close |
| ROM entry 64h/68h, EEPROM timing records | Not used | **Low-med**: the DOS core needs a way to set the 8514 mode; the ROM call is unproven from protected mode | Read the `.COM` set-up in `tools/m8seq`; try 64h/68h in P3.2 |
| 4 bpp mode, BG_COLOR_SRC, mirrored copy (Z9) | Not used | Low | Park |
| FIFO_OPT W_STATE / MAX_WAITSTATES (B3), AB zero-wait | B3 unrun; AB measured, no gain | Low | Close AB; B3 with S8a |
| A0 0-3, AD 4, AF, BC | No visible effect (M8BITS) | Low | Close |

## 4. Showcase candidates

Plan bar: >= 12 fps (>= 8 for strategy and flight), input under 150 ms, 10 minutes without a hang (plan line 24). Card today: twist 6.73 fps, flip2d 4.33 fps.
Windows titles are unlikely to meet the bar; the table says so. "On disk" = `C:\Users\lycet\Downloads\test_titles\` or `references/`.

| # | Candidate | API / bpp | On disk | Last status | Chance of playable (honest) |
|---|---|---|---|---|---|
| 1 | **Own DOS flat-shaded flight/space demo** (P4.1) | engine core, 8 bpp | not built | plan only; ceiling ~10 fps at 300 flat triangles (plan line 18), more with banded discs and ramp lighting | **Medium-high**: built to fit the card; needs P3.1-P3.3 |
| 2 | **Own DOS raycaster on Wolf3D source** (P4.2) | columns as fills | source not read | estimate 35 fps at 640 columns (3DR row 3); unmeasured | **Medium-high**; read source first |
| 3 | **Own D3D flat demo on the HAL** (P2.7: cube field, flat + Gouraud bands) | D3D 16 bpp | not built | the only D3D title where the scene size is ours; D3DIM geometry costs ~118 ms a frame in twist | **Medium**: a demo at <= ~100 triangles; not playable above that |
| 4 | **Donuts** (DX SDK) | DDraw 8 bpp, keyed blits | yes (`dx61samples/ddraw/bin`) | silent at 30 s on 11-15 bed; "now reaches its title" 11-17 (P1.13) | **Medium**: a real arcade game, small sprites, 869 keyed copies/s; needs 8 bpp desktop (X19) and DirectSound for sound (#58, not built) |
| 5 | **Foxbear** (DX3a SDK) | DDraw, scroll + keyed sprites | in `dx3a_sdk\DIRECTX3A.EXE` | never run; not in the plan's P2 table | **Medium**: scrolling suits CRT_OFFSET; 2D only |
| 6 | **Stunts**, original binary or restunts | DOS MCGA, flat polygons | not on disk | 512 KB game, runs unchanged; unmeasured | **Medium**: unmodified, a 320x200 frame through A000 is ~117 ms bus (8 fps cap); polygon hook (P4.3) removes it; counts as flight/strategy bar |
| 7 | **FastDoom** (potato 80x200, #47) | DOS VGA half | `Downloads\FastDoom_1.2.0` (WADs not seen) | X16 calls it a bus test, not a showcase; potato = 16 KB a frame | **Medium-high as a game**, not as a showcase: the likeliest playable thing here, not our engine |
| 8 | **Red Alert Win95 demo** | DDraw 8 bpp, keyed blits | yes | never run | **Medium-low**: strategy bar is 8 fps; mostly small changes suit describe-at-Unlock; needs CD, 30 MB, 5 MB paging |
| 9 | **Warcraft II demo** | WinG (GDI DIB) | yes | never run | **Medium-low**: GDI path (bitmap cache) is our best; not DDraw or D3D |
| 10 | **Shockwave Assault demo** | DDraw; 8 bpp exes at 200/240/480 lines and 16 bpp | yes | never run; not in P2 table | **Low-medium**: the only title with a 16 bpp DDraw choice and 200/240-line 8 bpp exes (matches the 320x240 target); genre not read |
| 11 | **Fury3 trial** | 8 bpp Lock-heavy, "panoramic" mode for slow machines | yes | never run | **Low**: min 486DX/66, 8 MB; tests X17 |
| 12 | **Doom 95 shareware** | DDraw 8 bpp, CPU-rendered 320x200 | yes | never run | **Low**: every pixel changes, 64,000 px x 3.84 us = 246 ms (4 fps); only the VGA half path reaches ~8 fps |
| 13 | **flipcube / wormhole / tunnel / twist** (SDK) | D3D, DDraw | yes | twist 6.73 fps; wormhole works (11-15 bed, 11-17 card); flipcube (12 flat triangles) not recorded | **Demo only**; flipcube should be the fastest D3D sample (survey section 4 item 1) |
| 14 | **Hellbender trial** | D3D | yes | never run | **Low**: P75 minimum |
| 15 | **MTM trial** | D3D, 640x480x16 | yes | parked by owner; Locked art 124 runs a row | **Very low** on Windows; HUD/static art on the bed only |
| 16 | **Hover!** (Win95 CD) | nv | no | API unknown (P2.5) | **Unknown**; first check is an API read |
| 17 | VBE/AF Allegro games on our `VBEAF.DRV` | DOS 2D, 8 bpp | none | no Mach8 driver exists (P5R section 1) | **Low-medium**, after P3; needs a DOS extender host |
| 18 | BRender `.BDD` (Carmageddon, 3D Movie Maker), Glide 2 `GLIDE2X.OVL`, CGL | DOS | none | swap shown only for BRender and Glide (P5R); titles textured, Pentium-class | **Low**: they run, they do not look as intended |
| 19 | Descent II demo, Age of Empires, Total Annihilation | | partial | AoE and TA need 16 MB | **None** on this RAM |

Own-demo genres that need no new silicon (3DR section 2 rows 7-10, unmeasured estimates): pseudo-3D road (refresh-bound at 320x200), scaled sprites
(29 keyed copies a frame at 30 fps), isometric dirty regions, hardware scroll and parallax. These are the most likely first playable games.
8514/A Adapter Interface programs (Halo, `IBM8514.BGI`, ATI `DEMOAI`) are pixel references, not games (SC section A, P5R section 5).

## 5. DOS driver

### Already defined

| Source | What it says |
|---|---|
| Plan "Architecture" (lines 44-58) | One platform-free core (transform, clip, depth sort, joined patches, palette, describe-to-commands); front ends: D3D HAL, DDraw HAL, own M8 3D API, Glide subset, VBE/AF, BRender `.BDD`; two back ends: Win95 DLL and a DOS 32-bit library (Open Watcom, DOS/4GW, direct port I/O, raster-timed flips) |
| Z29 (FRP 649) | Glide subset `GLIDE2X.OVL` drawing `grDrawTriangle` as flat/patch fills; first a DOS-native tunnel/flipcube on the same engine code to measure what leaving Windows saves (D3DIM ~80 ms a frame); COMrade needs the mouse unplugged, owner-attended |
| X15 (WL 376) | Own DOS renderer built around the card: engine fills, mask strips, keyed copies, textures resident in VRAM rows, 320x240, only changes over the bus |
| X26 (WL 407) | "DOS-level 3D driver (Glide-like, `594650a`) after FastDoom (X12, #47)" |
| Z27 (FRP 647) | DOS library of engine calls; FastDoom with flat floors as fills and walls from pre-scaled columns, ~2,500 writes, ~20 ms a frame |
| P3.1-P3.6 | Factor the core out of `m8d3d.c`; DOS card layer from `m8hw` primitives; DOS bench (flat and banded triangles/s, 640x480x8, page-flipped, raster-timed); port tunnel/flipcube; M8 3D API; shape and glyph cache |
| P4 / P5R | P4.5 static art from the start (S9); every API reduces to a trapezoid or scan list, the first primitive; order BRender, VBE/AF, Glide, with stop rules |
| 3DR sections 1, 5 | Cost model: span 20.4 us, polygon 0.2 ms + 35 ns/px, vertex 21 us, pan 15 us, palette 64 entries 0.84 ms |
| S1-S9 (plan) | S1e no stores in loops, S3c frame clear, S4d spans/masks/pixels priced per run, S9c cached HUD and cockpit |

Gaps the docs do not fill: no document says who sets the 8514 mode in DOS (probes `tools/m8probe/*.EXE` ran under the Windows driver; the DOS probes are `tools/m8seq/*.COM`); no DOS4GW found under `C:\Users\lycet\tools\openwatcom` (the `lib386\dos` start-up objects are there); Z27's claim about DOS probes is doubtful (section 1).

### First three build steps

| Step | What | Needs |
|---|---|---|
| 1. P3.2 core of the card layer | `m8hw`-style primitives (FIFO slot count, fill, span, polygon CMD 5, colour expand, copy, CRT_OFFSET flip) as a 32-bit DOS library; reproduce the M8SPAN numbers (span 20.4 us, polygon 0.2 ms + 35 ns/px) from DOS | `wcc386 -bt=dos` (Open Watcom is on disk; `build.ps1` uses `-bt=nt`); a DOS extender, source it; the 640x480x8 mode set from DOS (start from `tools/m8seq/M8PF.ASM` or `M8FLIP*.ASM`, or ROM entry 64h/68h, unproven); the bed for pixels, the card for time, owner at the 5160 (COMrade, mouse unplugged) |
| 2. P3.1 factor the core out of `m8d3d.c` | Move transform, join, sort, palette, describe-to-commands behind a back-end interface, no behaviour change | Twist pixels identical on the bed and fps within noise on the card (6.73 fps); bed polygon fidelity checked first (section 2, A3 gap); S1a store-free loops applied as it moves |
| 3. P3.3 bench, then P3.4 | DOS bench of flat and banded triangles/s, page-flipped, raster-timed (A9h); then the tunnel/flipcube scene on the core for the Windows-vs-DOS number | Step 1 and 2; raster-timed flip needs DISP_STAT bit 1 or A9h read (11.5 us a poll); capture folder in `docs/captures/`; result decides whether P4.1 is scoped at ~300 triangles |

After that: P3.5 (M8 3D API) and P4.1 on top, with P3.6 shape cache for the HUD (S9c), and P5.1 `VBEAF.DRV` as the first compatibility file (its table is the engine's own vocabulary).
