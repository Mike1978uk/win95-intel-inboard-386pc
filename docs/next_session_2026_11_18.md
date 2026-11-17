# Next session, after 2026-11-17

Earlier: `docs/next_session_2026_11_17.md`. Newest wins.

## State

- **Card (5160):** boot 409, desktop **640x480 at 256 colours** (owner set it for the mode-change test; Win95 asked
  for a restart). Put back to 800x600 16 bpp when DirectDraw testing ends. Driver `m8nof.drv`. `C:\M8CENSUS.EXE`
  is the build with the `8bpp` line (`7DFBB0DF`).
- **Bed `vm_5160_dbg`:** running on the normal 86Box (`86box_auto\build`), current driver installed as `M8REAL`
  (DLL md5 `B1D80597`), `C:\M8D3D.INI` has `trace=1`. gdb build in `86box_auto\build_gdb` (MINGW64, automation on,
  `-mwindows`); the first attempt is kept as `build_gdb_ucrt_bad`.
- **#10:** PICCHK sent to Simon as a GitHub link (`3355e1c`); 5160 control clean. Waiting on his `C:\PICCHK.TXT`.

## Found 2026-11-17

| item | result |
|---|---|
| RaiseException entry | `BFF9A013`, not `BFF9A00E` (doc fixed, `ad5c5c2`); gdb `hbreak` proven by `tools/raisetst` |
| Blue screen trigger | flip2d at 8 bpp from a 16 bpp desktop, then PERFLOG started: PERFLOG GPF in KRNL386 `0001:49DB` (**AllocSelectorArray**), then `0E at BFF9A07C`. Desktop pre-set to 640x480x8 (no mode change): flip2d -> PERFLOG **3/3 clean**. Theory: a mode-change path in our 16-bit driver damages the LDT free list; the next program to allocate selectors faults. The agent is the victim (it starts the next program). |
| Bed page flips | 86Box's Mach8 model stores `CRT_OFFSET` (2EEE) but applies it only at a mode recalc: the bed never flips 8 bpp pages (odd frames only). Card flips correctly (counter +1 each frame). The 11-15 "flip2d 2.5x on the bed" claim is void. |
| flip2d on the card | <1 fps. 30 s run: 27 flips, 55 Locks, **28 read the whole page back** (307 KB each; bed: 0 of 1,269), ~98 KB sent per Unlock (cube ~36 KB), transfers 30.4 s of 30 s, ~2,900 page-ins per run. `docs/captures/2026-11-17_flip2d_card/` |
| Shape reuse (owner's idea) | `tools/flipshapes/flipshapes.py`: one axis, 7.18 deg/frame, exact frame repeat every 304 frames, 84 % reuse over 20,000 frames but 0 % inside a cycle; ~3 MB of masks a cycle. 1-bit mask sending pays (8x); caching pays for sprites/text/HUD, not this cube. |

## Later on 2026-11-17 (owner out, authorised)

| item | result |
|---|---|
| Agent redraws | M8EPOCH: agent 0.12's tray icon flashes every 250 ms while busy (53 redraws in 20 s, each one read back by our driver); agent off: 1. |
| Agent fixed | Michael Dale's V9x-Remote-Agent 0.17.0 plus `[agent] tray_flash=0` (icon still; refresh only on an address change), branch `inboard-tray-flash` in `RiderProjects\V9x-Remote-Agent` (outside this repo). Bed and 5160 updated (build `inboard-trayflash`); 5160 idle probe now 1 copy in 20 s. The old 0.12 host `v9xctl.ps1` still works. Owner to tell Michael (draft owed). |
| Read-back cause | Not the agent, not the feed, not GDI: the card ran `32bfaac`, before `760f004` (fills keep the Lock shadow). |
| Card driver | Now `m8head.drv` + DLL `B1D80597` = `3aaea3a` (boot 413). Way back: `M8NOF.DRV` still in SYSTEM, old DLL `C:\V9XREMOTE\REG\M8DD32.NOF`. Desktop still 640x480x8. `dist/mach8` stays `32bfaac` until a fuller card run (bench). |
| flip2d on the card | `32bfaac`: 27 flips in 30 s, 28 of 55 Locks read back, ~98 KB per Unlock. `3aaea3a`: **50 flips**, 1 of 102 read back, ~52 KB per Unlock, transfers 13 s of 30. Next: X17 item 1 (deferred clear). |
| Bed | `vm_5160_dbg` now at 640x480x8 (flip2d there: 52 of 53 Locks no read). X22 model fix: analysis in the worklist, not edited (upstream code). |

**End of 11-17:** card driver `m8dfr.drv` (DLL `A254C4DF`) = `1167bdb`, the deferred clear (X17 item 1): flip2d
2.2 fps, ~74 KB a frame, no read-backs (`docs/captures/2026-11-17_dfr_card/`). Way back: `m8head.drv` (`3aaea3a`)
and `m8nof.drv` (`32bfaac`, DLL `C:\V9XREMOTE\REG\M8DD32.NOF`) still in SYSTEM. Bed `vm_5160_dbg` on the same build.
**Later still:** masks built (off by default, `[ddraw] masks=1`), flush timers in census slots 96-103 (`m8d3d_stat` now 112), Lock skips hashing a held page. Card `m8nohsh.drv` (boot 418): flip2d **2.8 fps**. Black dark face = flip2d's own shading (shade clamped at 0), not ours. **Next: one-pass mask build in asm, row hash/extent scan in asm, then the feeder** - measurements in `docs/captures/2026-11-17_mask_card/`.

## Tasks, in order (Opus; commits by Haiku; raise any long run with the owner first)

1. **Why half of flip2d's Locks read back on the card.** `m8dd_epoch()` hashes the driver's GDI counters
   (escape 4D41h); a counter moving each frame (suspect `cur_hide`: the card's real cursor) or a failed escape
   (falls back to GetTickCount) forces the read. Probe `tools/m8epoch/M8EPOCH.EXE 30` alongside flip2d on the card
   names it (owner present). Fix: count only what changes pixels where the shadow claims to match.
2. **Unlock sends ~98 KB, not the cube.** After 1, check the span narrowing on the card; then X17 (deferred clear,
   1-bit masks, recolour in place).
3. **Paging:** hash only the locked rows; shrink the 8 bpp shadow to the two pages.
4. **Mode-change selector damage:** read every Disable/ReEnable/`m8_screen_done` path for a selector used after
   free or freed twice; a 16-bit probe that allocates/frees selectors after a mode change makes it deterministic.
   Until fixed, run DirectDraw tests with the desktop pre-set to the program's mode.
5. **Owner's mode plan:** per-program mode table (launcher) or a driver pop-up asking for the mode; depth change
   needs a restart on Win95 retail.
6. **86Box model:** apply `CRT_OFFSET` writes at the next frame (and check the `ge_offset -= crt_offset` line);
   upstream-able. Owner wants it fixed.

## Not done today

- Fable leftovers (F2 4, F3 4-5, XT-IDE 1 and 7) - untouched.
- BRender `.BDD` (X16) - research doc only (`docs/x16_dos3d_api_sourcing_2026_11_17.md`).
- Paging during a Direct3D demo (RAM-compression question) - not measured.
