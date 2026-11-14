# Mach8, Fable F3 findings 1-3, bed only, 2026-11-14

Bed `vm_5160_mcp` (86Box `86box_auto`), driver loaded as `M8D3GG.DRV` (the bed's `SYSTEM.INI` name; writing
`M8DISP.DRV` changes nothing there). Runs in `M8BENCH.TXT`:

- `bed_headA`: master `7e7095e` build, `M8D3GG.DRV` md5 `c262e8e3`, `M8DD32.DLL` `8a843530`.
- `bed_f3B`: the same plus F3 1 (idle wait before a mode set), 2 (the DLL resets a wedged engine),
  3 (compare off and idle wait when GDI cuts the feed; `m8d3d_mono_off` order). `47e954a9`, `c9a5a9ed`.
- `bed_head` is not a valid run: the 16-bit driver was the bed's older `M8D3GG.DRV`.

Pixels: same in all 36 tests. Speeds are the bed's and do not count. Card run pending, with the
Alt+F4 exit test that hung before.
