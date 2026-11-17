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
