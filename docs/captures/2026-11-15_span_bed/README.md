# 8 bpp Unlock sends only the changed span after a fill: Mach8 bed, 2026-11-15

Bed `vm_5160_mcp`, boot 240, `M8DD32.DLL` `ADC6300A`. flip2d (DX 6.1 SDK), pictures at 5-90 s
from `tools\bed_shot.ps1`.

flip2d clears its back buffer with a fill, Locks it and draws the cube. A Lock inside the last
fill starts from one colour, so Unlock now sends each changed run only between the first and last
byte of another colour, instead of whole rows.

Bed: frame 13 at 45 s, frame 31 at 60 s (~1.2 fps), cube drawn correctly. No speed change on the
bed is expected: it prices a port write at ~1 us against the card's 7.6 us, so fewer bus bytes
show only on the 5160. Card run next.
