# Next session - from 2026-10-16

Start from `docs/worklist.md`. Owner's direction (10-16): the driver uses the whole card - both chips and the
silicon ATI's drivers never used - kept wherever a measurement shows a gain. Least RAM, least bus traffic.

## Done 10-16

- **86Box PR #8351** raised (TC1995: Win 3.1 8514.DRV File Manager glitch from #8289): command 4 had width and
  height swapped. Card probe M8NIB2 = fixed build byte for byte; A6 48/49 = master. G11 siblings not run (owner's
  call, gated code). CI's Windows jobs failed in MSYS2 setup (86Box's CI, not the change). Owner told TC1995.
  `docs/mach8_cmd4_axes_2026_10_16.md`.
- **#8339 merged** (10-15); README lists #8294 and #8339; #8351 open.
- **D0 passes:** our FRAMEBUF build loads in Windows 95 (VFLATD was missing; the 28800 half holds 640x480/800x600
  only). Switch drivers with `drivers/mach8_display/bed/OWFB.REG` / `ATIM8.REG`. `vm_5160_now` is left on owfb.drv,
  LS-120 and NIC removed in Device Manager.
- **Design** (`docs/mach8_driver_design.md`, dated sections): 800x600 base; `NOT_FRAMEBUFFER` (DDK-documented, "8514/a
  or ATIM8"); velocity9x read - its Trio64 back end is an 8514/A engine by port I/O, the template for fill/copy/text.
- **velocity9x builds here** against the Win95 DDK (`C:\Users\lycet\tools\ddk95_as98`): Win16 driver links; audits
  trip on newer `wdump` output; mini-VDD needs Win98's MINIVDD.INC (DPMS - not wanted on this card, owner agrees).
- **Lever inventory** `docs/mach8_levers.md`: port census of 84 ATI binaries (`tools/ati_port_census.py`); source
  colour compare (92EE/EAEE) and short strokes used by TEST.COM only; VVESA disassembled - split read/write banks
  on the 28800 (B2 + BE bit 3), AE bank high bits; 28800 indices no ATI binary touches listed.
- **L3/L4 card work (three sittings):** ROM has a far-call API (64h-70h); EEPROM = four 11-word timing records; line
  counter A9h = raster clock (11.5 us/poll, rewrite the index each read); DP_CONFIG mono = bed byte for byte; split
  banking works, no speed gain; EXT_SHORT_STROKE draws only diagonals; **switching halves blanks >1 s - C2/C3 not
  viable**. All in `docs/mach8_levers.md`.
- Sources added: DDK ATI mini-VDD sample (`DISPLAY/SAMPLES/VDD/ATI`, has a Mach8 path); 3Dos and Portfolio repos (§12).

## Done later 10-16

- **#8351 merged** (OBattler, `c43f3e2d0`); README row; owner told TC1995.
- **ATIM8 tap capture** (`docs/captures/2026-10-16_atim8_tap/`): ATIM8 loads its own shadow sets; the 800x600
  sequence is what D8a replays.
- **D8a passes in the bed:** `drivers/mach8_display/src/M8DISP.DRV` (boxv9x/vmdisp9x frame, MIT; own screen
  core, GPL-3.0). Desktop, Explorer, Solitaire correct at 800x600. Result, four bring-up bugs and the bus
  tally: `docs/mach8_driver_design.md` "D8a result". `vm_5160_now` is left on m8disp.drv (trace build);
  `ATIM8.REG` from DOS goes back; image before the driver: `card.img.pre_m8disp`.

## Done in the evening, 10-16

- **D8b on the engine:** text (17.0 -> 2.0 ms per ExtTextOut), cursor (13.3 -> 1.0 ms per move), pattern ROPs (solid,
  two-colour and mono-bitmap brushes by mix). Disable now restores trapping and text mode (blue screen gone in one run).
- **Open glitches, ATIM8 correct in the same bed:** white square beside the animated castle deck; dashed frame on the
  default button. Evidence and next steps: `docs/mach8_driver_design.md` "Pattern ROPs and open glitches".
- **Bed:** `vm_5160_now` is on **ATIM8** (registry) with M8DISP.DRV `d2808a4` (trace build) in `C:\WINDOWS\SYSTEM`;
  `REGEDIT C:\M8DISP.REG` from DOS switches to ours.

## Next

The plan is `docs/worklist.md` - the one source of truth; this file is a snapshot. Section L (card levers) is **done** 10-16 except
L3's desk details (mode-table bits, ROM API arguments, EEPROM record layouts - not blocking). **Next: D8b** (design doc "D8a result": attribute fallback pixels per GDI entry, text and cursor on the engine, DOS switching), A10 replay later. Was: A10 (replay
harness), then D8 from the "Lever decisions for D8" table in `docs/mach8_driver_design.md`. Tools the owner OK'd:
the Windows 98 DDK (archive.org) for a faithful velocity9x build.

## Owed / to tell people (owner sends)

- Michael Dale (velocity9x): build notes **sent by the owner 10-16** (ledger). Watch for a reply. Our clone keeps one
  local, uncommitted edit in `scripts/common.ps1`.

## Left on disk

- `86box_cursorfix/` now holds branch `mach8-nibble-axes` (#8351) - keep until it merges. `86box_bisect/` now holds
  branch `mach8-tap` (A10 step 1, built, pushed to the fork) and `bisect-cursorfix-wip` - **keep it**.
- `vm_mach8_8514drv/` (Win 3.11 on 8514.DRV, shell = File Manager): reference bed for correct Win 3.x output.
- `vm_6695_w2/nvr/mach8.nvr` is the post-TEST.COM EEPROM; `.bak_1015` the earlier one (A6 baselines differ by it).
