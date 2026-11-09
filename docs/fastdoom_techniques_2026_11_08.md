# FastDoom techniques for the Mach8 Direct3D driver

Source: https://github.com/viti95/FastDoom (GPLv2), clone at `references/fastdoom/`
(HEAD 0f8e734; untracked and not in `.gitignore` - add the path before any `git add`).
Paths below are under `references/fastdoom/FASTDOOM/` unless stated.
Speedups are quoted from `README.md` / `CHANGELOG.MD`; "unmeasured" = no figure given.
Written 2026-11-08 from a read of the source; nothing was run.

## What does not transfer

FastDoom renders every pixel on the CPU (Watcom C + NASM, 32-bit DOS/32A). We
drive a 2D engine; our cost is port OUTs plus the C that decides what to send.
Its pixel loops (`R_DrawColumn`, `R_DrawSpan`) have no analogue here.
Not found anywhere in the repo or README: cache-flush-on-I/O handling, an
8514/Mach/ATI path, a retrace wait in the frame loop (`3DA` appears only in
CGA code and `i_ibm.c` setup), port-write batching beyond one plane-select OUT
per column, reciprocal/division tables.

## 1. CPU-specific code and arithmetic

- **Runtime CPU dispatch.** `selectedCPU` from `I_GetCPUModel()` (r_main.c:2918-2938);
  one `switch (selectedCPU)` per detail/wall/sprite mode picks `colfunc`/`spanfunc`
  (r_main.c:1300-2900, e.g. `R_DrawColumnFastLEA` for Cyrix/AMD/UMC). Variants
  for slow SHLD, small-code-for-L1 and MMX (CHANGELOG:48-54). A separate 486
  build was always slower than the 386 one, so they were unified (CHANGELOG:455).
  Reported: column/span rewrites +10% (high, low) and +22% (potato) (CHANGELOG:288-290).
  Maps: we run one CPU, so no dispatch. Lesson only: small code beats unrolled when the cache is cold.
- **Unrolled column via computed jump.** `scalecalls[]` has 201 entry points;
  `jmp [scalecalls+4+ebp*4]` enters a straight-line `%rep` body at the right
  pixel, no loop counter (planar.asm:45-60, 106, 230-252). Unmeasured.
  Maps: `m8d3d_tri_span` / `m8d3d_join_fill_bands` inner loops. Rejected: with L1
  flushed by every OUT, a long unroll costs more in RAM fetch than it saves.
- **Fixed-point 16.16**, `FixedMul` = `imul` + `shrd` (fastmath.h:33-36);
  `FixedDiv` guards overflow with one compare then `cdq; shld; shl; idiv`
  (fastmath.h:80-86). Unmeasured (Doom is fixed-point by design).
  Maps: `m8d3d_pal_quad` float sampling to 16.16 integer sampling. Our own
  numbers: float multiply-add 2.97 us vs fixed 1.53 us.
- **Sorting arithmetic**: no reciprocal table or Newton step exists; see section 5.
- **Self-modifying code.** The screen-centre constant is patched into the loop
  once per view-size change: `R_PatchCenteryPlanar` writes into the `0x12345678`
  immediates (planar.asm:62-72, 126, 151-159; planar2.asm:74-76, 151-159).
  Unmeasured. Maps: per-frame constants (plane height, stride) could be patched
  once per frame instead of loaded per pixel. Low value: our loops are not load-bound.
- **AGI-stall avoidance** in column/span code (CHANGELOG:339). Unmeasured.
  Relevant only while the loop stays in L1, which ours does not between OUTs.

## 2. Detail modes and simplification

- **Potato detail**: quarter-width render, 80x200, own column/span routines
  (README; CHANGELOG:532). "Much faster, specially on 386 and slow 8-bit VGA ISA
  cards"; no number.
- **Flat visplanes**: no texture read, one precomputed colour. `R_GetFlatColor`
  returns a cached per-flat colour (r_data.c:886-896), used at r_plane.c:405-409
  and `R_DrawSpanFlat*` (r_draw.c:3274-3330). Colour is the histogram mode
  (CHANGELOG:31). `-flatterSpan` also drops distance lighting, "MUCH faster"
  (CHANGELOG:509). README table (486, timedemo): stock 19.56 fps; flat visplanes
  25.77 (+31.7%); flat + Saturn transparency 26.02 (+33%); FastDoom 0.2 without
  flat 22.44. Walls have the same switch (`WALL_FLAT`/`WALL_FLATTER`, r_main.c:1320).
  Maps: the one direct hit. Per-texture cached dominant colour, used instead of
  `m8d3d_pal_quad` sampling for small or distant triangles. Our joined patches are
  already one colour each on the card, so this saves the sampler, not bus time.
- **Far-object cull**: skip non-shootable sprites beyond 40,000,000 units
  (r_things.c:1137, option `-far`, d_main.c:1644). Enemies are kept. Unmeasured.
  Maps: drop triangles whose screen bbox is 1-2 px before `m8d3d_edges_sort`.
- No distance LOD beyond that; `zlight` is per-distance lighting only.

## 3. Video-memory write path

- **RAM backbuffer + dirty-band copy** by `updatestate` flags
  (`I_FULLSCRN/FULLVIEW/STATBAR/MESSAGES`, i_vga13h.c:56-150), `CopyDWords` (rep movsd).
- **Differential copy for slow buses** (`-slowbus`, d_main.c:1635, m_menu.c:1324,
  i_vga13h.c:30-50): a shadow `vrambuffer` holds the last frame; each byte is
  compared and VRAM written only if changed (vga13h.asm:23-60). The 486 variant
  walks the dword with `bswap`; 386 variant avoids it. Added 0.9.8, "useful for
  8-bit ISA cards on mode 13h" (CHANGELOG:211). Speedup unmeasured. The only
  bus-aware idea in the repo.
  Maps: we already join same-colour patches (18.6k writes toward ~2k). Extend
  across frames: keep last frame's patch list per tile band and send only changed
  rectangles. A tunnel moves most pixels, so expect less than Doom gets. RAM reads
  are cheap, OUTs are 7.6 us each.
- **Fewer bytes per frame**: potato = 80 columns (4x fewer); EGA 80x200 with write
  mode 2 stores 8 pixels per byte write, "good on 8-bit ISA EGA" (CHANGELOG:321).
  Maps: nothing, our card has no data aperture; analogue is a lower-res mode.
- **Planar**: one `out SC_INDEX` per column, not per pixel (planar.asm:~187-192).
  Same idea as our band joins; nothing new.
- No retrace wait, no tearing control found.

## 4. Port I/O and cache

Nothing found beyond "code size matters" (CHANGELOG:51 small code for L1, :339
AGI stalls). Maps: our L1-flush finding says keep the working set small between
OUTs and prefer compact loops. FastDoom adds no method for that.

## 5. Sorting and bucketing

- **Visplanes**: no sort; linear match over at most 128 (`MAXVISPLANES`,
  `R_FindPlane`, r_plane.c:52, 274-296).
- **Vissprites**: merge sort on pointers (`msort`/`R_SortVisSprites`,
  r_things.c:1449-1516), from Boom, replaces `qsort`. BSP walk made non-recursive
  (CHANGELOG:508, ideas from GBADoom). Unmeasured.
  Maps: `m8d3d_edges_sort` (shell sort) to a counting sort keyed by y (about 600
  buckets, O(n), no compares). FastDoom shows only "replace the generic sort";
  the bucket form is ours. Insertion sort if edges arrive near-sorted frame to frame.

## 6. Blue Lightning, upgrade modules, cache flush, CPU selection

Checked: source, README, CHANGELOG, `gh search issues` on viti95/FastDoom
(wiki not fetched).
- **Not found**: IBM 486BL/BL3/Blue Lightning, 386 upgrade-module cache settings,
  DIP switches "flush on DMA + I/O vs DMA only", any `wbinvd`/`invd`/cache-control
  code, any comment on I/O flushing the cache. Greps for those terms hit only file
  caches and CHANGELOG:51. Issue searches "Blue Lightning" and "486BL" return nothing.
- **Nearest**: https://github.com/viti95/FastDoom/issues/272 (hang on a PC-98 486SLC2
  upgrade board at DOS/32A init; no cause, no cache discussion) and
  https://github.com/viti95/FastDoom/issues/180 (L1-sized "rolled" loops for
  Pentium/K5, done in 1.2.0).
- **CPU selection** (`cpu_t`, doomdef.h:~409-428; flags `-386sx -386dx -i486 -cy386
  -cy486 -cy5x86 -cy6x86 -cy6x86mx -umc486`, d_main.c:1612-1620): auto-detect only
  separates 386/486/586/686 (r_main.c:2918-2938); anything else is user-forced.
  Effect is only which asm variant is bound: Cyrix 5x86/6x86/6x86MX, AMD K5/K6,
  RISE MP6 and UMC Green 486 get `R_DrawColumnFastLEA` (r_main.c:1307-1316),
  avoiding slow shifts/rotates (CHANGELOG:56). A 486BL would be `INTEL_486`.
  No Cyrix cache setup. Maps: none; our OUT-flush cost is ours to manage.

## Ranked top 5 for our case

1. **Per-texture cached flat colour for small/far triangles** (r_plane.c:405,
   r_data.c:886). Only technique with a published gain (+14.8 to +31.7%). Cheapest to try.
2. **Differential frame** (i_vga13h.c:30, vga13h.asm:23): send only changed patches.
   Targets OUT count, our scarcest resource; win depends on frame coherence, measure first.
3. **16.16 integer sampling in `m8d3d_pal_quad`** (fastmath.h:33-36): 2x on the
   multiply-add by our own numbers.
4. **Bucket sort for `m8d3d_edges_sort`** (idea from r_things.c:1455): removes compare-heavy
   shell sort running from cold RAM.
5. **Small-triangle cull / potato-style detail drop** (r_things.c:1137, Potato
   mode): skip triangles under ~2 px, or lower resolution when over a frame budget.

Rejected: computed-jump unrolling, self-modifying constants, per-CPU dispatch,
MMX paths (one CPU, cold L1 favours small code).

## CPU, video and sound options for the 5160 (issue #47)

Read 2026-11-09 from the source; nothing run. Paths under `references/fastdoom/FASTDOOM/`.
Issue #47 (https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/47): Andrew's 84 MHz
BL video; owner: Cyrix-tuned setting "likely optimal", sound "hammers the system".
viti95/FastDoom searches ("freeze sound blaster", "XT", "8-bit", "slow bus") find no matching
issue; #88 "Broken 8-bit sound on 0.8.14" is closed, not read.

### CPU selection
- Switch or `selectedCPU` value in FDOOM.CFG (m_misc.c:308, default -1 = auto), d_main.c:1612-1631:
  `-386sx -386dx -i486 -cy386 -cy486 -cy5x86 -cy6x86 -cy6x86mx -umc486 -k5 -k6 -pentium
  -pentiumP54CS -pentiumMMX -pentiumII -mp6 -winchip` (enum doomdef.h:409-428).
- Auto: `I_GetCPUModel()` is DPMI call 0400h, CPU field x100+86 (i_ibm.c:1366-1373): 386, 486,
  586, 686. Maps to `INTEL_386DX`/`INTEL_486`/P5/PII; anything else INTEL_486 (r_main.c:2918-2938).
  No CPUID needed. CPUID (EFLAGS.ID test, fpummx.asm:52-64) only sets `hasFPU`/`hasMMX`, printed
  on the banner (d_main.c:1517-1521). Not verified: what DOS/32A reports for a BL3. The banner
  prints the number: read it first. Expect 486 -> `INTEL_486`.
- What it changes, Mode Y (our `fdoom.exe`), r_main.c:1300-1800:
  - Walls and sprites, high detail: `R_DrawColumnFastLEA` only for UMC486, Cyrix 5x86/6x86/6x86MX,
    K5, K6, MP6 (1307-1316, 1331-1341). Everything else, `-cy486` included: `R_DrawColumn`.
  - Spans (high/low/potato): `R_DrawSpan386SX` / `Low386SX` / `Potato386SX` for 386SX, 386DX,
    `-cy386`, `-cy486` (1409-1418, 1596-1605, 1783-1792). Else `R_DrawSpan`.
  - The 1.2.0 slow-SHLD, small-code-for-L1 and MMX routines (CHANGELOG:48-54) are chosen only in
    backbuffered and VBE2 branches (r_main.c:1892-2080, 2443-2606). Mode Y never uses them.
- `INTEL_486` and `-cy486` differ only in the span routine. `-cy5x86` is the only way to get LEA
  columns on this CPU. Whether LEA beats the default on a BL3 is unmeasured; CHANGELOG:56 says it
  avoids slow shifts/rotates, which is a Cyrix 5x86/AMD trait.
- Cyrix/TI note: README.md:61 live demo is a TI 486DLC at 40 MHz on a Cirrus ISA card, 0.8.
  No source or doc note about IBM, TI or 486SLC/DLC handling. Issue #272 (PC-98 486SLC2) is a hang.
  `-cy486`'s 386SX span is the nearest to a DLC-class path. Owner's "shares traits with Cyrix"
  is not in FastDoom; test it.
- `-i486` also picks `I_FinishUpdateDifferential486` (bswap) in 13h `-slowbus` (i_vga13h.c:36-50).
  Unverified: BSWAP on a BL3; if DOS/32A says 386 the non-bswap one is used.

### Video
- Executables (README.txt:85-125, build.sh:49-176): `FDOOM` Mode Y (draws in VRAM, one SC_INDEX
  OUT per column, planar.asm:~187-192), `FDOOMH` Mode Y 320x100, `FDOOMX` 320x240, `FDOOM13H`
  mode 13h with RAM backbuffer and dirty-band copy (i_vga13h.c:56-150).
- Detail: `-high` 320x200, `-low` 160x200, `-potato` 80x200 (d_main.c:1368-1370); `-size 3..12`
  view size (d_main.c:1587). Potato: "specially on slow 8-bit VGA ISA cards" (README.md:36).
- Flat: `-flatSpan`, `-flatterSpan`, `-flatWall`, `-flatterWall`, `-flatSprite`, `-flatsky`,
  `-flatInv` (d_main.c:1597-1608). Less CPU, not fewer VRAM bytes; README.md table: +31.7% on a 486.
- `-slowbus` (diff copy against shadow frame, 13h only, d_main.c:1635) is the ISA-specific path.
  `-vsync` / `-novsync` waits on retrace (d_main.c:1636). `-nomelt` skips the melt wipe.
- Mode Y vs 13h on 8-bit ISA: same 64,000 bytes per full frame cross the bus either way; 13h adds
  a RAM copy, Mode Y writes during render. `FDOOMH` halves the bytes. Which wins is unmeasured.
- Not in this clone: any Mach8/8514 path; Mach8 VGA side only.

### Sound
- No command-line device switch. Device set in FDSETUP or FDOOM.CFG: `snd_sfxdevice`,
  `snd_musicdevice` (m_misc.c:316-318); enum i_sound.h:25-52: 1 PC speaker, 2 Adlib, 3 SB,
  13 SB Direct, 14 PC PWM, 11 PC 1-bit. `snd_rate` 0-8 = 7000...44100 Hz, default 2 = 11025
  (m_misc.c:321, dmx.c:715-745). `snd_channels` default 3 (m_misc.c:315).
- Switches: `-nosound`, `-nosfx`, `-nomusic` (i_sound.c:104-115), `-mono` (panning only; DMA stays
  stereo, FX_Init gets 2, dmx.c:~755; s_sound.c:645), `-forceSound` skips init failure checks.
  SB address, IRQ, DMA from BLASTER (dmx.c:687-688, ns_fxm.c:229-240); no switch overrides them.
- `-xt` only swaps the keyboard ISR for one that acks via port 61h (i_ibm.c:1012-1044,
  CHANGELOG:72). No interaction with sound code found.
- SB path: 8-bit auto-init DMA, 256-sample buffers x16 (ns_muldf.h:38-41), so ~43 IRQs/s at
  11025 Hz plus the DSP writes. Music/FX tasks run from a PIT task at 140 Hz (i_sound.h:22,
  ns_task.c:130-132).
- OPL music write = 2 OUTs + 33 INs (ns_sbmus.c:214-228). At 5.7 us per byte I/O and an L1 flush
  per access, one register write ~200 us; a chord of writes per 140 Hz tick is a plausible
  "hammer". `-nomusic` removes it. Unmeasured.
- Freeze, candidates only, none tested: (a) ISR time at IRQ 5 stacking on the OPL write bursts;
  (b) PIT channel 0 reprogrammed to the task rate (ns_task.c:130-132, mode 36h) with the BIOS
  tick chained; (c) SB DMA 1 vs the Inboard's flush rules.
  Each is separable by the rows below: freeze with SB sfx only, music only, or neither.

### Test matrix (`-timedemo demo1`, realtics; ~22 min each, docs/next_session_2026_09_28.md:5-8)
Baseline is the existing no-sound run. Idle proof and two runs per row, as before.
1. `fdoom -xt -nosound -timedemo demo1` (auto = INTEL_486, high)
2. same, `-cy486` (386SX spans)
3. same, `-cy5x86` (LEA columns + default spans)
4. same, `-386dx` (is the 486 path worth anything)
5. best of 1-4, `-low`
6. best of 1-4, `-potato`
7. best of 1-4, `-flatSpan -flatWall` high (CPU-bound ceiling)
8. best, high, `-nomusic`, SFX on SB at 11025 Hz (the freeze suspect, sfx only)
9. best, high, `-nosfx`, music on SB OPL
10. best, high, `-nomusic`, SFX on PC speaker (`snd_sfxdevice 1`)
11. best, high, SB sfx + OPL music, `snd_rate 0` (7000 Hz)
12. best, high, SB sfx + OPL music, default rate
Rows 8-12 stop at a freeze: note the row, hard reset, move on.
