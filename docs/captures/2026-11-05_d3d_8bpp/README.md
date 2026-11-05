# Direct3D at 8 bpp, bed, 2026-11-05

`vm_5160_mcp`, 800x600x8, DirectX 7.0a. `M8D3DT.TXT` holds the runs up to the proof (run labels `bed1`-`bed10`).
`bed6`-`bed8`: stock logic with tagged error codes, `8876A02C`. `bed9`: depth test NOPed. `bed10`: only the `and` NOPed.
Counters are shared between runs, so read differences. Reading: `docs/next_session_2026_11_05.md`.

**Card, `M8D3DT_card1.TXT`:** boot 209, `M8D3D1` at 800x600x8, LEAN, M8DD32.DLL md5 99DDA869 read back off the card, stock
system DirectX, patched D3DIM beside the test. `M8IDLE 10 10 300`: quiet 10 at 52 s. Solid fill, 12,321 pixels, one triangle
drawn, none refused: the bed showed only its 448-pixel outline.

**Bench, `M8D3DB_card.TXT`:** M8IDLE settled, two runs, 16 flat triangles a frame, clear + draw + copy to the screen, 800x600x8.

| Triangle size | HAL fps (run 1, 2) | RGB emulation fps (run 1, 2) | HAL over RGB |
|---|---|---|---|
| 8 px | 61.3, 62.2 | 6.7, 8.7 | ~8x |
| 32 px | 71.8, 69.5 | 5.6, 5.6 | ~12.6x |
| 100 px | 46.9, 42.6 | 1.3, 1.3 | ~34x |

Unexplained: 8 px is slower than 32 px. ~0.9 ms a triangle is well above the ~0.2 ms of its writes: every batch waits for idle and clears the scratch.
