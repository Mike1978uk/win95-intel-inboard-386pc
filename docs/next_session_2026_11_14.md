# Next session, 2026-11-14

Earlier: `docs/next_session_2026_11_13.md` (MTM chain, HA-HJ results, design notes). Newest wins.

## START HERE

Pushed through `7d7f912`. **Uncommitted in the tree: HL** (`m8dd32.c`, `m8d3d.c`, `m8.h`,
`tools/m8d3dtest/M8MON.C`), built in `%TEMP%\m8d3hl` (DLL `E16D97F7A0263FB6258E8BEF7D47B985`,
DRV `ACE7AC740D73ABB85466BFD179DF094B`), **not yet run**. HL = the 8 bpp Lock fix:
- Lock skips the read-back when the shadow still matches the card (nothing drew since: GDI counters via
  `M8_ESC_STATS`, the HAL's engine writes `m8d3d_stat[25]`, `m8dd_card_gen` bumped by Blt/CreateSurface).
- Unlock sends only rows whose hash changed (`m8dd_held_sum`).
- Counter `m8d3d_stat[94]` = Locks with no read-back; M8MON's dd line prints it as `noread`.

Bed `vm_5160_mtm`: driver **HJ** installed (boot 24x). A background job was switching it to **8 bpp**
(`M8BPP8.REG` + restart) when the session ended: check `HKLM\Config\0001\Display\Settings` BitsPerPixel.

## Order agreed with the owner (11-13)

1. **Test HL in the bed at 8 bpp** (install_h.ps1 `-Dir %TEMP%\m8d3hl -Name M8D3HL -Target 127.0.0.1`).
   Program: DX SDK **Memtime** (`references/dx61samples/ddraw/bin/memtime.exe`, Lock vs Blt) with
   `C:\M8MON.EXE 300 memtime` started first. Expect `noread` > 0 and unlock KB < lock KB. Watch for
   cursor ghosts (the interrupt-time cursor is not in the epoch). Then commit (Haiku).
2. **Feeder fix** (11-12 job 3): stop the feed only when a call actually writes the card.
3. **Card trip** (one visit): twist for the feeder; MTM at 640x480 for one fps number is optional.
4. **Fable review**: update `docs/fable_driver_review_brief.md` to cover the Mach8 driver, `swapcomp`,
   `xtide_mpd`, and a "driver contract" audit (every DDRAW/D3DIM check on what the driver reports, with
   the error it returns; technique 153 is the first entry). Owner approves the brief; own worktree.
5. **X14 test titles** (worklist): DX SDK DirectDraw samples first, then Shockwave Assault, Doom 95,
   Red Alert, Fury3, Warcraft II, Hellbender. Files: `docs/test_titles_2026_11_13.md`.

## Monster Truck Madness: parked (owner, 11-13)

Runs full screen on our Direct3D HAL in the bed at 640x480 (`lores=0 lockscan=0`): textures on the card,
Gouraud, `palette=1`. Too slow to play: it renders in software and Locks the whole back buffer **before**
its triangles each frame, so every frame takes the slow path (whole frame over the bus); the 486 and the
bus are the limits. Owner: "it was a stretch given 24 MB and Pentium that it even loaded at all."
Open, not pursued: Lock-scan black screen (`lockscan=1`); a stall before the first Flip seen once with
`lores=1 lockscan=1` (lores alone reached the race, slowly); the start-view site (patch at `0x4978DA` has
no effect; bed exe patched, original `MONSTERX.ORG`); the owner's Graphic Options (Dashboard, Horizon,
Smoke off; Texture Low; Scenery Sparse) were not saved to `MTMHW.INI` (the game went black on exit).

## Rules learned today

- No agent (`v9xctl`) commands into the bed while a full-screen game runs: REDIR32 faulted.
- A bed game run costs the owner minutes: one run per question, bisect with INI switches first.
- Bed frame rates mean nothing; M8MON counts and the census do.

## Agents and roles (unchanged)

Opus: driver, diagnosis. Haiku: every commit and push (exact paths, PowerShell push). Sonnet: research
and downloads (today: `docs/test_titles_2026_11_13.md`, 7 titles in `Downloads\test_titles\`). Fable:
the review above, after the owner's OK.
