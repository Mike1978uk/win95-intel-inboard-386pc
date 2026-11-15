# Overlays at 16 bpp, and 8 bpp fills kept in the Lock shadow: Mach8 bed, 2026-11-15

Bed `vm_5160_mcp`, boot 239, 800x600x16. Driver `M8D3GG.DRV` `2F04F3BE`, `M8DD32.DLL` `9E24DAED`.
DX 6.1 SDK samples through `tools\m8cardscripts\sample.ps1` (30 s), pictures from `tools\bed_shot.ps1`.

| sample | before (11-15 morning) | now |
|---|---|---|
| mosquito | "needs overlays" box | **runs**: the overlay over the desktop, black keyed out (`mosquito.png`) |
| ddovrly | "needs overlays" box | **runs**: its image in its window (`ddovrly.png`) |
| flip2d | frame 24 at 45 s, `FPS 00` | frame 81 at 90 s, `FPS 01`: ~2.5x |
| wormhole | runs | runs |
| M8BENCH `bed_ov2` against `rt2` | | pixels equal in 36 of 36, 0.985 |

How:
- **Overlay:** the Mach8 has no overlay. One 5-6-5 overlay surface in RAM; `UpdateOverlay`,
  `SetOverlayPosition` and overlay Flips describe it to fb16.c (batch op 6), which merges it into
  every row it sends under the destination (`m8_ov_merge`, dd16.c's segment: `_TEXT` is full).
  The frame buffer is never written, so moving or hiding it is a resend. Fills and card copies
  that touch it are sent instead. 5-5-5 and YUV are refused, so a program tries 5-6-5.
- **First build refused:** DDRAW drops the whole HAL when a claimed surface type has alignment 0;
  `vmiData.dwOverlayAlign` = 8.
- **flip2d:** it clears the back buffer with a fill, then Locks it. The fill now writes the
  shadow too and keeps it in step, so that Lock reads nothing back from the card.

Open: flip2d's Unlock sends whole rows where the cube changed; sending only the changed span is
next. Overlay cost per flip not yet measured; card run waits on the 8 bpp round-trip fault.
