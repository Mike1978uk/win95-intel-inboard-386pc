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
