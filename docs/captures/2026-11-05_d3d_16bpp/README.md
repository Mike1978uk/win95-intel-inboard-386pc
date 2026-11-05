# Direct3D at 16 bpp on stock DirectX, card, 2026-11-05

`M8D3D2` (commit fd86aa6, M8DD32.DLL md5 FA494D65 read back off the card), boot 210, LEAN, M8IDLE settled, no patched DLL in the
program's folder. Full-screen 640x480x16 flip chain, 16 flat triangles spread over the screen, Blt clear + draw + Flip, 3 s a test.

| Size | HAL fps (run 1, 2) | RGB emulation fps (run 1, 2) | lit, HAL / RGB |
|---|---|---|---|
| 8 px | 18.4, 8.2 | 1.2, 1.3 | 720 / 448 |
| 32 px | 12.9, 17.3 | 1.6, 1.6 | 8,976 / 7,936 |
| 100 px | 13.1, 11.2 | 0.6, 0.6 | 80,150 / 77,382 |

The HAL's runs disagree by up to 2x at 8 px, so these are a rough first figure, not a baseline. `lit` is the RAM copy, not the card; our count
includes edges RGB leaves out. Not comparable with the 8 bpp bench: that scene is a 256x128 target. Screen not seen by anyone in these runs.
Where 55-120 ms a frame goes is not measured. Suspects: one box around all triangles (filled each frame, and fingerprints dropped over it).

## dxdiag, card, `M8D3D2`, owner's phone video (2026-11-05)

dxdiag's two Direct3D tests both passed, stock DirectX. Timed from the video's first and last frames:

| Test | Start | End | Length |
|---|---|---|---|
| Software | 0:33.41 | 3:37.25 | 183.84 s |
| Hardware (our HAL) | 4:07.86 | 4:28.64 | 20.78 s |

8.8x, if both tests draw the same number of frames (assumed, not checked). Driver counters prove the hardware test reached the HAL: contexts 10 against 9 from
M8D3DF's runs, and about 1,280 triangles in about 220 RenderPrimitive calls beyond M8D3DF's (estimated from rounded fps), 0 refused. Owner: faster, colours flicker.
Baseline for this build. Next: M8D3DS before and after a run gives exact flips, so flips / the video's seconds = fps.

**`M8D3DF_fast.TXT`, `M8D3D3`, boot 211, M8IDLE settled:** per-triangle boxes and no 16-bit side on a black frame took hal_tiny from 8-18 to 28-46 fps. Frame ~22-35 ms; Flip ~10, 16-bit part ~1; the rest is outside Flip, growing ~1 ms a triangle (hal_many). Run-to-run spread still large on the small tests.

**`M8D3DF_nouser.TXT`, `M8D3D4`, boot 212:** hal_static 132-152 fps (a frame identical to the card's draws nothing). Moving tests unchanged, so the USER call was not the cost. Outside Flip ~7 ms static, ~15 ms moving: unexplained.

**`M8D3DF_split.TXT`, `M8D3D5`, boot 213:** frame by stage. The black clear costs 5-35 ms, growing with what was drawn, and ~1.3 ms when nothing was: unexplained, biggest target. DirectX calls ~0.3 ms a triangle plus ~2 ms.

**`M8D3DF_hi.TXT`, `M8D3D7` (065a3ac), boot 216, M8IDLE settled, one run each:** normal priority hal_tiny 31 fps, clear 11.4 ms. High priority hal_tiny 65.2, hal_small 66.2, hal_colour 70.8, hal_static 196, hal_large 36.2, hal_many 19.2 fps, clear 1.2-1.6 ms. The clear's cost is other threads: proven on the card.

**`M8D3DF_quiet.TXT`, `M8QUIET.TXT`, `M8D3D7`, boot 218, one run each:** the bench started detached, no agent exec. The agent is the thief. Fps agent idle / stopped / also no Systray or Internat / `high` with agent idle: hal_tiny 52.9 / 69.9 / 68.3 / 64.8, hal_small 51.4 / 66.1 / 58.6 / 63.7, hal_large 35.3 / 39.9 / 39.9 / 35.0, hal_static 95.1 / 209.1 / 208.7 / 167.1, hal_colour 21.7 / 75.6 / 75.7 / 58.9, hal_many 15.0 / 22.5 / 22.5 / 17.0. Clear with the agent stopped 0.8-1.4 ms. Stopping the agent frees about 200 KB. A2 hung the card on the desktop before M8QUIET wrote a log line, after the agent had been restarted twice in that boot: cause unproven, restart suspected. Rule until known: stop the agent at most once a boot, then reboot.
