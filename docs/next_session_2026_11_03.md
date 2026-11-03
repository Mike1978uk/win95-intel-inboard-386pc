# Next session, 2026-11-03

The plan and every number are in `docs/mach8_frame_rate_plan.md` (worklist X9). This file is what to do next.

## State

- master `7f63442`, pushed. Card on **`M8STREAM.DRV`** (boot ~193), 800x600x8: the streamed-outline test build,
  ~6 % slow on the cube. Replace it first (step 1). `M8SOLID.DRV` (master's fill fix) is also on the card.
- Card tools: `C:\M8P0.EXE`, `C:\M8CUBE.EXE` (options `v` vblank, `x` fixed point, `n` no draw, `p` profile),
  `C:\M8FPU.EXE`, `C:\8-INFO.EXE` (not yet run).
- Scoreboard: `tools\m8_score.ps1 -Label <name>` (M8CUBE, M8DDB, M8P0, twice each; `-Gdi` adds M8BENCH).
  Baseline `solid` in the plan. Agent-run cube numbers are lower than console runs: never mix.
- Build: `drivers\mach8_display\src\build.ps1 [-Prof] [-ExtraCflags @('-os')] [-Eliminate]`.
  Plain `_TEXT` 64,857 of 65,536 bytes; `-Prof` links only with `-os`.

## Done tonight (card-measured)

- Phase 0: OUT 4.5 us, IN 4.5 us under Win95; RAM write 9.3 MB/s, read 17.4 MB/s; 16 bpp unlock = 35 ms read + 29 ms hash.
- Vertical sync: refresh 72 Hz; DISP_STATUS (02E8) costs ~100 us a read under Windows; the V_BLANK latch (42E8 bit 0)
  is the sync source. `M8CUBE ... v` presents at vblank (69 fps, works).
- Owner's stills: no tearing; the faults were polygon-fill leaks. Fixed (XOR outline + outline over the fill,
  REPLACE for the fill). Owner: solid, clean. Costs ~12 % fps.
- Accidental XOR translucency recorded as G14 (one write per face: XOR, 50 % blend, saturating light).
- Cube frame: ~1.3 ms a face; M8CUBE's float maths 4.2 ms a frame. Fixed point (`x`): 1.65 ms, +22 % fps.
- String I/O: `rep outsw` 4.6 us a word vs 5.7 for a C loop. Pays on long runs only; streamed outlines were slower (reverted).
- XTOUT was already clear since 09-28 (`a0f7d44`); the sweep's claim was stale. An FPU is fitted (owner).

## To do, in order

1. **Driver build `m8dd32` string I/O** (audit finding 1): `rep outsw`/`rep insw` per row in `m8dd32.c:297, :312,
   :952, :961`. Make fixed point M8CUBE's default (keep float as an option). Card cycle
   (`tools\m8_card_cycle.ps1 -Drv ... -Name M8DDSTR -Label card_ddstr`), then `m8_score.ps1 -Label ddstr`;
   expect dd_lock/dd_sys up ~20 % of their bus share, GDI unchanged, pixels = ATI in 35.
2. **Pace `m8_hw_wait_vblank`** (looks ~1 ms apart, not back to back) so SB DMA and the XT-CF keep the bus (#47).
3. **Palette by `rep outsb`** from a packed buffer (`m8_hw_set_palette`): for colour cycling and plane animation.
4. **Where the remaining ~0.8 ms a face goes** in the driver: the `-Prof -os` build (`M8CUBE 10 3 p`, slots 45-50),
   with the profiler's own stamp cost (~84 us) subtracted or read once per frame. Then asm on what it shows.
5. **G11 present inside the blank** (copy each object's old and new boxes only) and **G12 background from VRAM**.
6. **P6 in the bed**: does D3D (DX 7.0a) accept an 8-bit HAL? Then Direct3D in its own 32-bit DLL (owner: lean
   16-bit driver; D3D outside the 64 KB segment).
7. Cheap probes still open: P11 card wait states (6AEE/36EE), P13 shared low-byte latch, P9 double scan (bed first),
   `C:\8-INFO.EXE` in DOS for the IRQ jumper, `M8P0 s` (mid-frame offset split, owner watches).
8. Cross-driver (#57): SWAPCOMP LZ4 IMUL hash (#54); `custom_vkd` `VKD_Wait_Empty` 65,536 reads of 64h at boot
   (#48, confirm by BOOTLOG first). Both unverified at source beyond the audit's quote.

## Owner's direction (11-02/11-03)

Demoscene and 8/16-bit game-scene methods; the card's own silicon and ATI-unused features; the bus is the cost.
Lean everywhere: driver size is RAM on a ~5 MB machine; asm across the board where measured. Faster fps is the
default for 3D; `v` is an option. Each trick that works is checked across all our drivers. Use cheaper agents for
sweeps and audits; verify their claims at source before acting (a stale XTOUT claim was repeated tonight).
