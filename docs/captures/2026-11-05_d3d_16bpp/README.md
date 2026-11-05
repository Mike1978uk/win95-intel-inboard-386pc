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
