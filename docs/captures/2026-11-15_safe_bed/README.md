# The fatal exception reproduced on the bed without a depth change, 2026-11-15

Bed `vm_5160_mcp`, boot 247: driver `3aaea3a` with 8 bpp refused on a 16 bpp desktop (built, not
committed). wormhole: "SetDisplayMode FAILED", closed (`1-wormhole.png`); flip2d at 16 bpp;
mosquito: **fatal exception 0E at 0137:BFF9A07C** (`3-mosquito.png`), the card's address. Summary and
what the address is: `docs/directdraw_raise_bsod_2026_11_15.md`.
