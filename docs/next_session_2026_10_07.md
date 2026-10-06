# Next session - 2026-10-07 (Mach8 conformance probe)

## Gate (owner, revised 2026-10-06 evening)

The bed must be good before any driver goes near the 5160, but TEST.COM 100% is no longer the gate:

1. ATI ROM POST (`Testing........Ok`) and TEST.COM Register, FIFO, RAMDAC, Video RAM pass - they do.
2. Every engine operation shape a production driver uses matches the 5160 byte for byte.
3. TS1 (ops 0-133 of 172 match) and TS2 are tracked, not gating; fix an op when the driver depends on it.

Why: TS1 op 134 onward uses 92EEh and EAEEh. No ATI Windows driver writes either (ULTRA*.DRV,
VDDULTRA.386, MACHW3.DRV searched; WIN31ACC.EXE's hits were compressed LHA bytes). Only TEST.COM does.

## What a production driver uses

Bed `vm_mach8_w311` (Win 3.11 + ATI `MACHW3.DRV`, 1024x768x256, from `5160_Video_latest.img`), build
`86box_3c509b/build_log` with `MACH8_COUNT=1`. Owner used Windows for ~10 minutes (Program Manager, File
Manager, text, Paint Shop Pro, window moves); nothing drew wrong. Tally:
`docs/captures/2026-10-06_w311_usage/machw3_session_tally.txt`.

- 20 operation shapes: blit (type 2), LINEDRAW (type 3), SCAN_TO_X (type 5) x 13 DP_CONFIG values -
  fills `2211 2231 2071 2051 0291`, screen blits `6211`, patterns `A211` (PATT_LENGTH 0-7), mono
  expansion `3251`, host colour pixels `5210 5211 4010 4210 4215`.
- Mixes: foreground 00 01 02 04 05 07 0B 0C, background 01 02 03 05 07 0C. LINEDRAW_OPT `070C` only. No DEST_CMP_FN, no 92EEh, no EAEEh.
- 16.5 MB through E2E8 (host pixels): ATI's driver re-sends bitmaps. Ours should cache them on the card.

## Windows 95 (ATIM8.DRV) - same exercise, 2026-10-07

Bed `vm_5160_now`: the owner's card image of 2026-10-06 (`win95_0610.img`), configured as the 5160 (NIC 320/IRQ 9,
T130B irq=60, XT-CF BIOS d8000, no B:) with the Mach8 EEPROM `roms/video/mach8/eeprom_flexview2x_56hz_800x600.nvr`.
Every earlier bed had a blank EEPROM, so ATIM8 failed above 640x480 ("problem with your display adapter").
With it: 800x600x256, accelerator on, owner saw no faults; Paint Shop Pro's brush tool, broken in the
old beds, works. Tally:
`docs/captures/2026-10-07_w95_usage/atim8_800x600_session_tally.txt`.

- 17 shapes; the union with Win 3.11 is 21 (Win95 adds LINEDRAW with `2231`; 3.11 alone uses `0291 2051`,
  scan-to-X `4010 A211`). Mixes add `0E`. EXT_GE_CONFIG `000E` (800x600 pitch) as well as `000A`.
- Win95 draws far more lines (FEEE 184 KB vs 8 KB) and pushes 8.9 MB of host pixels through E2E8.
- The conformance probe covers the union: 21 shapes, mixes 00-05, 07, 0B, 0C, 0E, PATT_LENGTH 0-7.

## Next, in order

1. Write the conformance probe (M8SEQ style): each shape in the union x its mixes, on a known VRAM pattern,
   read back. Run in the bed, then on the 5160 over COMrade; diff.
2. Fix model differences row by row, scoped to the Graphics Ultra, evidence in `ati_test_com_notes.md`.
3. Then driver work (`docs/mach8_driver_design.md`).

## Bed notes

- No sound card in `vm_mach8_w311`: with SB Pro 2, Win 3.11 stalls at the wallpaper (known since July,
  `INBOARD_86BOX_PORT_PLAN.md` ~line 2470). Answer Yes to "permanent swap file is corrupt" (bed copy).
- Never pass `-Seconds` to `bed_launch.ps1` for an owner session: it force-kills the VM.
- 86Box rewrites `86box.cfg` on exit; edit the config only while the VM is closed.
- Open, parked: DP_CONFIG bit 11 with source 6 (CA11h) - not in the driver's set above.
