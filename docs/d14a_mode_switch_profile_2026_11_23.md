# D14a: where tunnel's main thread goes, 16 bpp desktop (2026-11-23)

Question (owner, 11-22/23): do 32/16-bit switches and Win16-lock waits limit frame rate, and should our
32-bit layer take over from DirectX's runtime to avoid them?

Method: `tools/eipsamp` (EIPSAMP + GHEAP16 + eipsum.py) on the card, `M8TUNF.EXE` (DX tunnel), agent silent.
Samples are the program's main thread only. M8DD32.DLL (based at 0xB0C00000) is not in the Toolhelp module
list, so eipsum shows it as "flat, no module"; named here from a linker map plus a `-d1` listing of the same
source (sections compared byte for byte with the card's DLL). Captures: `captures/2026-11-23_eipsamp_tunnel/`.

## Answer

**Start-up only.** Once frames are steady, 16-bit code and lock waits are ~3-4 % of the main thread.
The frame is our 32-bit Direct3D code (~61 %) and D3DIM (~27 %).

| Phase (card build `e3544f3`, 76 s run) | 16-bit | lock wait | M8DD32 | D3DIM |
|---|---|---|---|---|
| 0-30 s load, mode set | 7-100 % | 0-16 % | 0 | 0-13 % |
| 30-61 s first frames | 1-35 % | 41-60 % | 0-3 % | 0-12 % |
| 61-76 s steady | 1 % | 4-13 % | 41-67 % | 24-33 % |

The lock wait is one instruction, KERNEL32 `bff74269`: a loop that checks the lock's owner and blocks
through VWIN32 service 0x2A001D, always on the same lock object (8152B6CC). In the first-frames phase most
waits enter from USER32 thunks: our 16 bpp sends did `GetDC` + `ExtEscape` + `ReleaseDC` each time
(`m8dd32.c` `m8dd_send`, `m8dd_copied`, `m8dd_flush`), three lock acquisitions a send.

## F1: one display DC per process (built, card boot 497)

`m8dd_dc()` keeps a `CreateDC("DISPLAY")` per process (the DLL's data is shared by every process; an entry is
cleared at process detach). Desktop bench `card_dc1` vs `card_vgc1,2`: 0.996, pixels same in all tests.

| Phase (F1, 155 s run) | 16-bit | lock wait | M8DD32 | D3DIM |
|---|---|---|---|---|
| 0-31 s | 15-39 % | 3-49 % | 0-3 % | 0-11 % |
| 31-41 s | 16 % | 22 % | 23 % | 22 % |
| 41-155 s steady | 1-2 % | 1-5 % | 44-69 % | 24-46 % |

Steady state reached at ~41 s against ~61 s, but not a like-for-like comparison: F1 ran on a fresh boot,
the baseline after 2.4 h uptime with heavy paging. Frame rate not yet measured (below).

## Steady frame, our code (F1, 9,345 samples, 41-155 s)

| Share | Functions | Note |
|---|---|---|
| 8.7 % | `m8d3d_fround` 6.6, `m8d3d_fixf` 2.1 | float to fixed. `draw_on` converts each corner twice (x, y at `-half` and at 1/8 px); `draw_clipped` turns clipped integer corners back into float bits (`fixf`) for `draw_on` to convert again |
| ~10 % | `join`, `join_fill_bands`, `join_line`, `join_ok` | patch joining |
| ~11 % | `draw_on` 6.8, `tri_span`, `draw_tri`, `draw_list` | |
| ~6 % | `pal_quad_i`, `pal_patch`, `pal_find`, `pal_rgb` | colour matching |
| ~5 % | `draw_clipped`, `clip_side_i` | |
| ~4 % | `q_send_to`, `room` | the bus and FIFO waits |
| 2.5 % | `cells_mark` | |

## Consequences for the plan

- F2 (one escape a frame) and F3 (16 bpp sends in 32-bit) mainly shorten start-up; they do not move
  the steady frame. F3 still matters for GDI-heavy titles (text over 3D, Lock-heavy 16 bpp programs);
  measure one of those before building it.
- The steady lever is CPU work in our own Direct3D path: convert each corner once, keep clipped corners
  as integers, then joining and palette matching.
- Sound (emulated DirectSound through `waveOut`) is still a 16-bit path and unmeasured: sample Doom 95 with sound.

## Frame rate and paging (F1, `TUNB.LST`, agent idle in the boot)

(Withdrawn by T3a below: this window was the start-up paging phase.) 107 flips between census readings 97 s apart: ~0.9 s a frame of wall time, while the driver's own time
(`ours`) is 75 ms a flip, as on 11-09. NOAG.TXT shows 0 KB free and ~10 MB paged throughout, and
`SLEEP 30` takes 71 s. So the wall frame is mostly not our CPU work: paging (or other threads).
A sample is where the suspended thread stands, so a thread waiting on a page fault counts at the
faulting instruction: part of "our code" above may be page-fault waits in it. Next: PERFLOG page-ins
alongside the sampler, and the same run with the agent out of the boot (11-09 conditions).

## T3a: wall time and paging, measured (afternoon)

M8CENSUS now writes `now` (the performance counter), so wall time between readings is known;
`captures/2026-11-23_eipsamp_tunnel/frames.py` prints wall, draw calls (`ours`), Flip and the rest a frame.
Runs: fresh restart, M8IDLE quiet, tunnel started from a BAT with PERFLOG (2 s), census every ~60 s, no sampler.

| Run (F1 build, `t3d`) | wall ms/frame | draw calls | Flip | rest | page-ins/s |
|---|---|---|---|---|---|
| first ~100 s (start-up) | 476 | 79 | 84 | 312 | ~50 |
| steady (two intervals) | 319, 325 | 79, 77 | 86, 89 | 154, 158 | ~4 |

- **Steady tunnel is ~320 ms a frame (3.1 fps)**, as on 11-22 (327). The "0.9 s a frame" above was the
  start-up phase plus `SLEEP` stretched by paging; it is withdrawn. Paging is heavy for the first ~100 s
  (5,600 -> 8,300 page-ins), then ~4/s: not the steady limit.
- Of a steady frame, ~165 ms is our draw calls plus Flip; ~155 ms is outside them (D3DIM, the program, other
  threads). D3DIM's samples sit in one block (`D3DIM.DLL+55000..57000`), not yet named.
- Under EIPSAMP the driver's own times triple (draw 280 ms, Flip 215 ms a frame): the sampler's
  suspend/resume every 5 ms costs that much. Sample shares stay usable; absolute times from a sampled run do not.
- `start /m` (minimised) tunnel stalls after one primitive: never start a DirectX program minimised.
  Likely the TUNA "counters froze" case too (not checked).

## T3b: corner conversion once (built, card boot 506)

`m8d3d_draw_on` read each corner's x and y twice through `m8d3d_fround` (at the drawing precision and at
1/8 px for facing). Now once, cut toward zero at 1/16 px (`m8d3d_ftrunc16`), and both roundings come from that
(`m8d3d_r16`, a macro). Rounding a value cut at a finer precision is exact: checked against `m8d3d_fround`
for every float below 2048 at all three precisions used (2,650,800,128 cases, 0 differ).

| | F1 (`t3d`) | T3b (`t3i`) |
|---|---|---|
| draw calls, ms/frame (3 steady intervals) | 79, 77, 88 | 73, 73, 70 |
| conversion share of the main thread (EIPSAMP) | 6.6 % `fround` | 3.8 % (`fround` 2.1 for textures, `ftrunc16` 1.7) |
| desktop bench vs `card_dc1` | | pixels same in all tests, both runs |

- First build kept `m8d3d_r16` as a function (Watcom did not inline it): 9.8 % against 10.8 % of our code,
  inside noise. As a macro: about 6-8 ms off the draw calls a frame (~8 %). Wall frame 307-326 ms, inside
  its own run-to-run spread.
- Desktop bench `cursor_move` is bimodal across builds (~400 or ~1,200 ops/s; vgh 1,187, cw 1,122): not this change.
- Left: clipped corners still go integer -> float bits (`fixf`, 2.0 %) -> integer; `draw_on` would need the
  integer corners passed in, and the split path still reads floats.

Retrospective across the stack: `m8dos` and `m8cube32` (DOS) take integer corners already; DirectXT's
Direct3D is to run on this same `m8d3d.c`, so it inherits it. The rule for any new float path: convert a
value once at the finest precision needed, cut toward zero, and round from that.

## T3c: what D3DIM does in a steady frame

`t3h` (T3b build, EIPSAMP), last two thirds of the run: D3DIM is 27.3 % of the main thread. Its blocks,
disassembled from the card's `D3DIM.DLL` (`pedis.py`: capstone over the PE sections):

| Share of D3DIM | Of the thread | Block (RVA) | What it is |
|---|---|---|---|
| 41.7 % | 11.4 % | `56800..56E00` | vertex transform: 4x4 matrix times x,y,z in x87 (12 fmul, 12 fadd), then clip codes from the sign bits |
| 23.8 % | 6.5 % | `51300..51500` | point light: light-to-vertex vector, dot with the normal, `fsqrt`, attenuation `a0 + a1 d + a2 d^2` |
| 9.3 % | 2.5 % | `55D00..56000` | colour: float to integer (`fistp`), clamp to 0..255 |
| 25.3 % | 6.9 % | other | not named |

- This is Direct3D's own software geometry (the DX3-5 HAL has no transform/lighting hook for a driver). Every
  float operation runs on the 387 interposer next to a 486 without an FPU: that is why it costs this much.
- Nothing for M8DD32 to take over here for real titles. The levers are elsewhere: titles that send
  pre-lit, pre-transformed vertices (`D3DTLVERTEX`, as our `M8D3D*` tests do) skip this block entirely.

Retrospective across the stack: DirectXT and the DOS engine (`m8dos`, `m8cube32`) own their geometry, so
they should transform and light in fixed point on the 486's integer unit and keep the 387 out of the frame.
The same rule as T3b applies, with more force: a float operation is a bus trip to another chip.

## Open

- Baseline census (`TUNA.LST`, card `e3544f3` after 2.4 h up): 180 flips at +77 s, no change by +125 s;
  the tunnel stopped flipping or the counters stopped. Unexplained.
- The 11-22 figure "tunnel 327 ms/frame": reproduced by `t3d` (319-325 ms steady).
- Which thread holds the lock during the first-frames phase (sampler records the main thread only).
