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

107 flips between census readings 97 s apart: ~0.9 s a frame of wall time, while the driver's own time
(`ours`) is 75 ms a flip, as on 11-09. NOAG.TXT shows 0 KB free and ~10 MB paged throughout, and
`SLEEP 30` takes 71 s. So the wall frame is mostly not our CPU work: paging (or other threads).
A sample is where the suspended thread stands, so a thread waiting on a page fault counts at the
faulting instruction: part of "our code" above may be page-fault waits in it. Next: PERFLOG page-ins
alongside the sampler, and the same run with the agent out of the boot (11-09 conditions).

## Open

- Baseline census (`TUNA.LST`, card `e3544f3` after 2.4 h up): 180 flips at +77 s, no change by +125 s;
  the tunnel stopped flipping or the counters stopped. Unexplained.
- The 11-22 figure "tunnel 327 ms/frame" is not reproduced by this method; how it was taken is not recorded.
- Which thread holds the lock during the first-frames phase (sampler records the main thread only).
