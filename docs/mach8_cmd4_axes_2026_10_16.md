# Mach8 command 4 axes - TC1995's File Manager report (2026-10-16)

## The report

TC1995, 86Box Discord 2026-10-16: Windows 3.1's own 8514.DRV glitches in File Manager on the Graphics
Ultra; before #8289 it was fine.

## Cause

8514.DRV fills text backgrounds (the opaque rectangles of ExtTextOut, e.g. File Manager's selection bar)
with command 4, FILL_RECT_VNIB: routine at file offset `C10F` writes MAJ_AXIS_PCNT = width - 1,
MIN_AXIS_PCNT = height - 1, CUR_X/CUR_Y, then CMD `80B3h` (Y+, X+, draw, planar, write), with PIX_CNTL
`A000h` and FRGD_MIX `07h` (BKGD_COLOR, replace) set at `C08E`. No host data.

#8289's Graphics Ultra command 4 model took MIN as the width and MAJ as the height. M8NIB, the card probe
it was built from, drew six cases, all square (MAJ = MIN), so the axes were never measured - the model
guessed. Every non-square fill came out transposed: a wide thin bar plus a tall narrow block.

Before #8289, command 4 fell through to the ordinary rectangle path (MAJ = width), which is why it worked.

ATI's 1992 ULTRA.DRV is the same code base as 8514.DRV and issues the same `80B3h` fill, so it was
affected too. MACHW3.DRV and MACH.DRV do not use command 4.

## Sources

- Richter and Smith p. 234 (IBM 8514/A) and ATI's Mach32 guide p. 8-32 (FILL_RECT_VNIB) agree on the
  nibble behaviour: 4-pixel-wide vertical strips, alternating direction, CUR_X/Y not updated, LAST_PIXEL
  and DRAW ignored. Neither says which register is the width. 8514.DRV is the only static evidence.
- So the plain 8514/A and the 8514/ULTRA add-on should use the same model; today they still run command 4
  as a horizontal rectangle (only the host-data order differs for 8514.DRV's fill, which has none).

## Fix and tests

Fork branch `mach8-nibble-axes` (`86box_cursorfix/`), `3b23c04b2`: width from MAJ_AXIS_PCNT, rows from
MIN_AXIS_PCNT. Three lines, inside `ATI_GRAPHICS_ULTRA && cmd == 4`.

| Test | Result |
|---|---|
| `vm_mach8_8514drv` (Win 3.11, 8514.DRV + VDD8514.386, shell = winfile), #8339 build | glitch reproduced: selection transposed (owner's screenshot) |
| Same bed, fixed build | clean selection (owner's screenshot) |
| M8NIB2 on the real card (`docs/captures/2026-10-16_m8nib2/5160_M8NIB2.BIN`) | MAJ+1 wide, MIN+1 tall; unaligned X starts at CUR_X; host data down, up, down per nibble column |
| M8NIB2 in the AT bed, upstream master (`upm_nib2`) | differs from the card in 210 bytes |
| M8NIB2 in the AT bed, fixed build (`nib_nib2`) | **= card, byte for byte** |
| Graphics Ultra A6, all 49 probes: upstream master (`upm_gu`) vs fixed (`nib_gu`), same EEPROM | 48/49 identical; M8STAT differs between runs of one build (timing) |
| G11: plain VGA (`vm_fenix`), fixed build | pass (INBRDPC loads, DOS prompt; owner) |
| Not run | Mach32 and 8514/A A6 (changed lines are behind `ATI_GRAPHICS_ULTRA`); a second G11 card; full TEST.COM with the owner watching (its sequences are in A6) |

Harness notes: `fix_gu50` is not a valid baseline for upstream builds - it came from the fork tree, and the
AT bed's EEPROM was rewritten by the 10-15 TEST.COM run after it (M8REGS then differs in the ports).
`vm_6695_w2/nvr/mach8.nvr` is now the post-TEST.COM EEPROM again (copy kept as `.after_testcom_1015`).
Launching a bed within seconds of the previous run's exit can fail the startup-hook check (image still locked).

## For the driver (D)

What a shipping 8514-class Windows driver actually asks of the engine - CMD values found by scanning for
`mov ax,imm16` next to port 9AE8h (a pattern scan; `001E`, `00FF`, `A000` are probably not CMD writes):

- **8514.DRV / ULTRA.DRV** (near-identical sets): RECT fills `40F3`/`40B3` (colour); RECT with host data
  `41B1`, `43B3`, `53B1`, `5171` (image and mono transfers, 8- and 16-bit, both byte orders); radial LINE
  with host data `2318`/`3318` (short-stroke style); BLIT `C0B3` (screen to screen, 9 sites); command 4
  `80B3` (text backgrounds).
- **MACHW3.DRV**: two 8514 CMDs only (`40B3`, `C2B5`); everything else goes through ATI's extended
  registers.

Lessons:
- A probe whose cases are all symmetric cannot measure an asymmetry. Every geometry probe needs MAJ != MIN
  and an unaligned X.
- The cheapest way to find what matters is the shipping driver's own port writes: one scan named the
  command and the registers in minutes, before any boot.
- Our own driver will be judged by the same commands. Keep the bed (`vm_mach8_8514drv`) as a reference
  for what correct Win 3.x output looks like on this model.
