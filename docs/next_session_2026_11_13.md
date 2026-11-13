# Next session, 2026-11-13

Earlier handoff: `docs/next_session_2026_11_12.md` (owner's 11-12 list, results, agent roles). Newest wins.

## START HERE

Card: **powered off** by the owner at the end of 11-12. Last boot 378 on **HC**, 800x600x**8**.
Working tree carries **HA + HB + HC uncommitted** (`dd16.c`, `m8d3d.c`, `m8dd32.c`, +207/-6).
Do not lose them; commit once one card run proves them (see step 4).

Card files as left:
- `C:\M8D3D.INI` (backup `C:\M8D3D.BK3`): `[diag] trace=1 mode=0`, `[feed] on=1`, `[d3d] render8=1`
  (was 0 since 11-09), `[M8TWIST.EXE] lores=0 zsort=1 palette=1`, `[MONSTERX.EXE] lores=0`.
- Card driver HC: DLL md5 `31E53BA8E147F55DFB4D3095A176282B`, DRV `043CD010C697EDDA19BC76455F550EA9`.
- MTM in `C:\MTM`; launchers `MTM_SW.BAT` (useDirect3D=0) / `MTM_HW.BAT` (useDirect3D=1) on the
  desktop; patched D3DIM.DLL / D3DIM700.DLL app-local in `C:\MTM`.
- Depth REGs: `C:\V9XREMOTE\REG\M8BPP8.REG` / `M8BPP16.REG` (restart after).

## Where Monster Truck Madness stands

| Build | What it changes | Card result |
|---|---|---|
| HA | 8 bpp mode change rebuilds the HAL; 8 bpp page flipping (back buffer = row 512, Flip = CRT_OFFSET_HI) | `MTM_SW` reaches the track (slow). `MTM_HW`: "Unable to switch to full screen mode", then a hang |
| HB | Trace `m8dd_trace` -> `C:\M8TRACE.TXT` when `[diag] trace=1` (create, flip chain, first flips, destroy, D3D context) | `MTM_HW` (render8=1): same error, **no trace file** |
| HC | D3D HAL offers the desktop's depth only (8 bpp desktop -> DDBD_8) | `MTM_HW`: **same error**, **no trace file** (checked 11-12 evening, `C:\M8TRACE.TXT` absent) |

Reading: either DirectDraw never asks our driver for a surface in the D3D path (the failure is before
CreateSurface: the game's SetDisplayMode or its device check), or the trace does not write.
**The trace has never been seen writing on any run.** A negative from an unproven probe is not a
result (technique 74).

## Next, in order (cheapest first)

1. **Prove the trace writes**: run `MTM_SW.BAT` (it reaches the track, so it must create a flip
   chain). Expect `create ...` and `flip8 chain made ...` lines in `C:\M8TRACE.TXT`.
   - No file: the trace is broken (check `GetPrivateProfileInt` path, CreateFileA from the game's
     process); fix it in the bed with twist before reading anything into MTM_HW.
   - File present: the MTM_HW failure is before surface creation. Go to 2.
2. **MTM_HW at 16 bpp** (`M8BPP16.REG` + restart; D3D at 16 bpp offered since 11-09). If the game's
   Direct3D path asks for 640x480x16 regardless of the desktop, it works here and fails at 8 bpp
   because our driver cannot change depth without a reboot. That settles the cause in one boot.
3. If 16 bpp also fails: find what the game asks for. Static first: disassemble MONSTERX.EXE around
   the "Unable to switch to full screen mode" string (which call failed, which mode and depth it
   requested). Then a bed check of the mode list our HAL reports at 8 bpp.
4. Commit HA/HB/HC (Haiku, push from PowerShell; subjects <= 50 chars). HA is proven by MTM_SW
   reaching the track; HB/HC once the trace and step 2 settle. Split by build if the hunks allow.
5. Then, as agreed: textures on the card at 8 bpp, then Fable review (ask the owner first; brief
   `docs/fable_driver_review_brief.md`).

## Models and agents (owner, standing since 11-12)

| Who | Does | Rules |
|---|---|---|
| **Opus** (main session) | Driver code, diagnosis, reading measurements, card runs, deciding what a result means | Never hands diagnosis to another model. One card job at a time |
| **Haiku** agent | Every commit and push | Exact paths only, never `git add -A`; check `git diff --cached` shows only the intended files; push from **PowerShell** (Bash has no GitHub login); subject <= 50 chars; report hash + push line. 11-12's handoff commit `321a2bf` was done this way |
| **Sonnet** agent | Research, write-ups, prescribed bed chores (with a written brief) | Runs in the background; does not commit; no diagnosis (memory: 2026-08-22 lesson) |
| **Fable** | Hard reviews only, starting with the driver review (`docs/fable_driver_review_brief.md`) | **Ask the owner before starting any Fable work.** Own worktree, never master; Opus benches each of its commits on the card |

Also: the owner approves any text posted to GitHub (issues, PRs, replies); Opus drafts, never posts.
Bed first for anything that fails before the first frame; the card decides speed. COMrade only
with the mouse unplugged; DOS probes via a `.BAT`.

## Later (unchanged from 11-12)

8 bpp Lock shadow pageable (GMEM_FIXED, ~630-720 KB); depth per program without reboot (add to
worklist; also step 2's likely cause); MTM sound loop (sound off in both INIs; #55-type SB Pro
suspect); bed I/O cost model; card C:\ tidy (job 6a); FastDoom VGA-half path (#47); video plan;
DirectSound (#58); showcase grid; card back to 16 bpp for twist work.

## Beds

`vm_5160_mtm` (Windows at 8 bpp, D3DIM patched system-wide with `.STK` backups, MTM at
`C:\Monster Truck Madness Trial`; agent unreliable while the game runs) and `vm_5160_page`. Both
gitignored. Bed mouse: absolute moves fail; small moverel steps (<= 5).

## Results, 2026-11-13 (bed `vm_5160_mtm`)

- Trace works: `MTM_SW` at 8 bpp asks for two back buffers, is refused, then builds one (row 512). HA proven in the bed.
- Why `MTM_HW` fails (disassembly of MONSTERX.EXE, string 50004): full-screen setup at `0x494880` sets
  640x480 at **16 bpp whenever Direct3D is on** (8 otherwise). At an 8 bpp desktop the depth change fails.
  At 16 bpp: mode, flip chain and D3D context succeed, then D3D setup (`0x495090`) fails at the texture step
  (`0x495740`): it needs **>= 96 textures `TEXTURE|VIDEOMEMORY|ALLOCONLOAD`** (32x32 or 64x64, 16-bit); we never
  advertised `DDSCAPS_TEXTURE`, so DDRAW refused them without calling us. The game then retries at 8 bpp,
  which fails, and continues **windowed with its software renderer** (reaches the track at 16 bpp; census: no flips).
- HD (uncommitted): 8 bpp Lock/Unlock/Flip counters (`m8d3d_stat` 72-83), escape `M8_ESC_STATS` (0x4D41) for
  the 16-bit counters, `tools/m8d3dtest/M8MON.EXE` (half-second deltas to `C:\M8MON.TXT`). Idle bed desktop
  shows ~6 DIBToDevice/s with read-backs: something draws continuously; not chased.
- HE (uncommitted, built, not yet run): textures in video memory at 16 bpp on the card, 99 slots of 32x32 at
  x 640-991 rows 0-287 (stamps start below), 8 bpp at upload, 8x8 summary in RAM for the colour per
  triangle, shared 2 KB Lock buffer; HAL handles 64 -> 128; `D3DDEVCAPS_TEXTUREVIDEOMEMORY`. Bed INI
  `maxD3DTextures=96`. A `-Prof` build no longer fits `_TEXT` (1,823 bytes over).
- Loading screens: menu backdrops paint after the controls. At 8 bpp a DirectDraw Lock reads the locked area
  back and Unlock writes it all (`m8dd32.c` lock/unlock, ~2.4 s for 640x480 on the card's I/O cost). M8MON
  run not yet made; it names the path.
- Test titles after MTM: worklist X14, `docs/test_titles_2026_11_13.md`.
- **HF-HH, MTM_HW runs full screen on our Direct3D HAL (bed, boot 240).** DDRAW refused `DDSCAPS_TEXTURE` because
  `vmiData.dwTextureAlign` was 0 (technique 153; same rule as `OFFSCREENPLAIN`); HG sets 8. HH handles a texture
  `Load` (Blt from a 5-6-5 system-memory texture). Probe `M8MTMI.EXE` passes every start-up call; 99 textures
  on the card. Census, 10 frames: 3,111 triangles (all colour-keyed and perspective), 195 textures, engine writes 0,
  ~0.9 s a frame in Flip: every frame took the slow path (whole RAM frame sent). Cockpit view Locks the back buffer
  each frame; chase view is the owner's choice anyway. Next: chase view + `palette=1` + Gouraud (INI), keyed
  triangles on the engine path, card run for fps, then lores (320x240) with textures moved out of the column.

## MTM to playable: design, measured order (afternoon)

Bed now: HI (boot 241), MTMHW.INI `flatShadeFlag=0` (Gouraud), M8D3D.INI `[MONSTERX.EXE] palette=1`,
`MTM_HW.BAT` starts M8MON (15 min). View: chase near in game (V cycles views, D the dashboard).

1. **Measure (owner, bed):** MTM_HW in chase view, a few seconds racing. `C:\M8MON.TXT` 16 bpp line per half
   second: flips fast/slow, `lockback` (writable back-buffer Locks, rows), system-memory copies over the
   scene: `ovl ok`, `keyed->ram`, `plain->ram`. Any of the last four non-zero each frame = the slow path's cause.
2. **Fix by what 1 shows** (each turns "send the whole 640x480 frame" into "send what changed"):
   - `plain->ram`: unkeyed sprite copies over the scene; `m8dd_ovl_add` already takes `M8DD_OVL_NOKEY`, only
     the Blt condition requires a key. (HI removed an empty `if` at that spot, a half-finished case.)
   - `keyed->ram`: overlay cache full (8 entries, 32,768 pixels, card rows 992-1023 at 640x480).
   - `lockback`: HUD written by Lock. Keep the description at Lock; at Flip compare the locked rows with the
     described colour fills and make the cells that differ keyed overlays (key = described colour).
3. **Room:** at 640x480 the overlay cache has 32 card rows; at 320x240 (lores) 272. Lores needs the textures
   out of the column (join/stamps use x 640+ there): x 320-639, rows 512-1023 (page 1 is 320 wide in lores;
   overlay shelves then x 0-319, enough for a half-size HUD). Not rows 0-479 at x 320+: a slow Flip sends
   the full 640x480 frame to page 0 (fb16 op 2 has no half-size send).
4. Card run in chase view for fps; then the colour work (16x16 summary, D5 row copies).

## HJ results (bed, late afternoon)

HJ = lores texture slots (x 320-639, rows 512-1023, 160), page erase sized to the mode, overlay cache
16 entries with hashed `direct` entries, Lock-scan HUD overlays (`[d3d] lockscan`, now **off by default**).
- `lockscan=0 lores=0`: works, full screen on our HAL (grabs 15:22-15:23; first frame blue sky, then
  `palette=1`'s own hues). Same as HI.
- `lockscan=1 lores=0`: black screen. M8MON: `tris-after 1` - **MTM Locks the back buffer before its
  triangles**, so what it writes is under the scene (sky/backdrop?), not a HUD over it. Black-screen
  cause in the Lock-scan path not found; the slow path replays triangles, so it should not be black.
- `lockscan=1 lores=1`: one execute buffer, no Flip, game unresponsive. Lores alone not yet tried.
- Chase-view exe patch (`0x4978DA` view=0 -> 1, bed copy, original `MONSTERX.ORG`) has no effect: races
  still start in Cockpit. The start view is set elsewhere.
- REDIR32 (the agent's shell helper) faulted when a `v9xctl shell` call reached the guest during the
  game: no agent commands while MTM runs.
Next: one run `lores=1 lockscan=0`; then an **underlay** (content Locked before the triangles, drawn
first on the card, ideally as fills); then the right start-view site.
