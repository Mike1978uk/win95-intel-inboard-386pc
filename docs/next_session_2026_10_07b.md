# Next session - build the Mach8 driver (start here)

Supersedes the order in `docs/next_session_2026_10_07.md`, which stays the reference for the op lists,
bed notes and later items (VESA, DirectDraw, upstream PR). Design: `docs/mach8_driver_design.md`.
Ideas beyond the desktop: `docs/mach8_graphics_vision.md`.

## Where it stands

- Gate (owner, 2026-10-06): the driver waits only for the operations ATI's own drivers use to match the
  5160. Union of the Win 3.11 (`MACHW3.DRV`) and Win95 (`ATIM8.DRV`) tallies: 21 shapes,
  `docs/captures/2026-10-0[67]_*_usage/`.
- Bed `vm_5160_now`: the owner's card image of 2026-10-06, configured as the 5160, Mach8 EEPROM
  `roms/video/mach8/eeprom_flexview2x_56hz_800x600.nvr`. ATI's Win95 driver runs there at 800x600x256 with
  the accelerator on; Paint Shop Pro works. Default bed from now on; a known-good copy of its disk, config
  and EEPROM is in `vm_5160_now/default/` - restore from there instead of rebuilding a bed. No SmartWatch in
  86Box: "SmartWatch not found" at boot is expected, set date/time by hand.
- Build: `86box_3c509b/build_log` (`ee29d00c8`), launch with `$env:MACH8_COUNT='1'` for a usage tally.
- Sources and terms: velocity9x (Michael Dale, GPL-3.0, permission given), XFree86 Mach8 (MIT-style),
  xga-win9x (Christian Holzapfel, blessing given; DDK-sample-derived parts stay out of a GPL driver),
  ATI drivers and ROM (study only).
- Everything is pushed.

## Today, in order

1. **velocity9x: can it drive a card with no CPU-visible framebuffer?** Cloned (`45d439e2`, 0.11.0) from
   <https://github.com/michaeldale/velocity9x> to `references/velocity9x/` (not vendored). Read how it
   creates the DIB Engine PDevice (`CreateDIBPDevice`, `lpBits`, the `deFlags`) and whether its drawing
   assumes a linear surface. ATIM8 uses `VRAM | NOT_FRAMEBUFFER` with `lpBits = 0`
   (`docs/mach8_text_and_driver_plan.md`, ATIM8 findings). Also: does it build with Open Watcom here, and
   does its sample driver run in a Win95 bed? Answer both before building on it.
2. **Conformance probe, core shapes only:** fill `2211`, screen blit `6211`, host pixels `5211`/`4010`,
   mono expansion `3251`, and pixel read-back. M8SEQ style: known VRAM pattern, run each, read the area
   back. Bed first, then the 5160 over COMrade; diff with `tools/m8seq/m8seq_diff.py`. Several are already
   partly proven (M8EXP, M8TIME).
3. **Driver skeleton** to a plain desktop at 800x600x256 in `vm_5160_now`: mode set through the ATI ROM
   (the call ATIM8 makes works in the bed), the core shapes, nothing clever. Keep stock `ATIM8.DRV` beside
   it; the SAFE boot entry recovers a broken display.
4. Then the remaining shapes (lines - Win95 draws many - patterns, the other mixes), each enabled once it
   matches the card. Then the bus savers: glyph cache, bitmaps cached on the card.

## Rules that apply

- Ask before launching any VM; name the bed, build and duration. Owner drives the 5160 and COMrade.
- Never `-Seconds` on `bed_launch.ps1` for an owner session. Edit `86box.cfg`/`nvr/` only with 86Box closed.
- Audit every comment in a new file before committing (CLAUDE.md, "comments are a canary").

## Owner's open questions

- **Could a 3D capability serve existing games - a "DOOM mode"?** Not as a drop-in for DOOM's own renderer:
  DOOM draws every pixel in software, and DirectDraw's `Lock` hands it memory, so its frame still crosses
  the bus. Routes worth thinking about: (a) a source port of DOOM (the code is GPL) whose renderer sends
  engine commands - walls as pre-scaled column blits from card memory, flats as span fills, sprites
  cached - instead of pixels; (b) games that use DirectDraw `Blt` with sprites in video memory gain
  through a HAL. (a) is where "translation" happens: in the renderer, not in a driver. After the desktop
  driver; record measurements in the vision doc first.
- Upstream PR of the Mach8 model: owner's decision (his rule: fix only code he contributed). TC1995 first.
