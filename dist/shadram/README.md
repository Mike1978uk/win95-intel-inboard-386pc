# SHADRAM - the Inboard's spare shadow RAM, given to Windows 95

The Intel Inboard 386/PC reserves 128 KB of its own RAM for shadowing ROMs: 64 KB holds the copy
of the system BIOS, the other 64 KB (meant for the EGA BIOS) is never used. On a 5 MB machine that
is RAM worth having. `SHADRAM.VXD` gives Windows the unused half, plus the pages of the BIOS copy
it does not need, and keeps the BIOS running from the card's fast RAM.

| file | md5 | bytes |
|---|---|---|
| `SHADRAM.VXD` | `bd529d5007b732b99658ff1517dc5dc9` | 6,378 |

Version 4. Rebuilds byte-identical from commit `e1e7bfd`: source and build script in
[`drivers/shadowram/`](../../drivers/shadowram/).

## Install

1. Copy `SHADRAM.VXD` to `C:\`.
2. In `C:\WINDOWS\SYSTEM.INI`, under `[386Enh]`, add `device=C:\SHADRAM.VXD`.
3. Restart.

**Remove:** delete the line (or put `;` in front of it) and restart. Nothing on disk is changed.

**Needs** `INBRDPC.SYS` v1.1 (02/17/89), Intel's own driver, loaded in `CONFIG.SYS` as usual. Without
it, or if anything below does not hold, the driver adds nothing and changes nothing.

## What it does, at Windows start-up

1. Opens the card's window onto the reserved RAM at `5E0000`-`5FFFFF` (port 670h bit 0), the way
   `INBRDPC.SYS` itself does, and checks the BIOS copy is visible there.
2. Writes the never-used 64 KB before reading it. The card checks parity on every read, and that
   RAM is never written at power-on; the first write raises the card's error latch, which is cleared.
3. Keeps the BIOS-copy pages that are still needed (the top two, any page `INBRDPC.SYS` patched, any
   page of code) and releases the rest (filler, and the IBM cassette BASIC).
4. Tests every page it will release and gives the ones that pass to Windows.
5. Maps the BIOS area in every DOS box: kept pages from the card's RAM, the rest from the ROM.

## Measured

On the real 5160 (486BL3, 5 MB Inboard): **28 pages, 112 KB**, added to Windows.
Version 3 (the unused half only, 16 pages) showed no measurable speed change (0.05%), as expected
for about 1% more RAM; the gain is memory, not speed. Version 4's speed was not measured.

To see it working, add the `SHADRAM` counters in System Monitor: `PagesAdded` should read 28 and
`PagesFailed` 0. If nothing was added, `Status` says why:

| Status | meaning |
|---|---|
| 0 | pages added |
| 1 | Windows already owns these pages |
| 2 | Windows refused to map an area |
| 4 | `INBRDPC.SYS` not found, or not v1.1 |
| 5 | the BIOS area of DOS boxes is already claimed by another driver |
| 6 | the window did not open |
| 7 | a parity error during the page tests |

Notes: [`docs/shadram_2026_10_02.md`](../../docs/shadram_2026_10_02.md),
[`docs/shadram_5160_2026_10_03.md`](../../docs/shadram_5160_2026_10_03.md),
[`docs/romcache_decode_2026_10_03.md`](../../docs/romcache_decode_2026_10_03.md).
Releasing the unused BIOS-copy pages was @andrew-hoffman's suggestion on
[#35](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/35).
