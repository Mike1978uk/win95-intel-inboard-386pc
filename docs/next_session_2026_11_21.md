# Next session, from 2026-11-20/21

## Done this session
- M8SHOW: fixed point (36.8 fps), shadows, spotlight and glass by the engine's mixes (`a52e4b7` .. `5e158ad`).
- EIP sampler (`tools/eipsamp`, `46a2374`): tunnel's main thread is 4.3 % D3DIM, ~30 % blocked on a lock,
  ~38 % 16-bit code, 8 % DDRAW. Replacing D3DIM is not the lever.
- X37 desktop background on the card at 16 bpp (`b58bedd`, card FB16CHK 0 differ) and the palette-exact
  wallpaper `DXTWPAL.BMP` (`ec3048f`). The card runs `M8BG.DRV`; `C:\V9XREMOTE\REG\M8FINE.REG` goes back.
- Docs: X37-X40, OSR1 update research (no Microsoft fix targets our hangs), racer design
  (`docs/x40_racer_design.md`), video issue #60, titles by route (`docs/titles_by_route_2026_11_21.md`),
  DirectDraw Mode X research (`docs/directdraw_modex_vga_half_2026_11_21.md`).
- Doom 95 on the card: sound fine; quarter image in the corner with wrong colours (no real 320x200 mode);
  unplayably slow; the machine crawled after exit (8 MB title on 5 MB).

## Next, in order
1. **X1: 320x200x8 as the driver's own mode on the VGA half.** Built and proven on the card the same day;
   see "afternoon" below. The plan as written: Mode table row, ValidateMode, ReEnable:
   output to the VGA half, INT 10h 13h, primary at A0000 pitch 320; back to the 8514 and repaint on exit.
   Bed with M8DDB modex, then Doom 95 on the card: `-timedemo demo1`, then smaller views. Ceiling ~14 fps.
   The output switch is proven (`m8_hw_passthrough`, used on every switch to DOS; M8FLIP4 on the card, >1 s
   blank). New and unproven: setting mode 13h from inside the driver (INT 10h from Windows) while Windows keeps
   the screen, and whether the VDD/minivdd saves or restores VGA state under us - read ATI's minivdd source
   (`references/dxddk/ddk9x/win98ddk/src/SRC_DISPLAY_VDD_ATI_ATI.ASM`) first. The bed does not model the mux
   (11-02), so the card decides.
2. **X40 racer, step 1:** core + DOS front end on m8dos, no sound. Card fps and writes a frame.
3. **D14 a, two runs:** tunnel under EIPSAMP with the agent off; teach the sampler to name 16-bit code.
4. **X39** if Windows hangs before the taskbar again: boot DOSNET, read BOOTLOG first; no agent traffic until the
   taskbar is up.
5. **X32** DOS agent exit key and missing-file refusal.
6. Title mode checks for the "check" rows in `docs/titles_by_route_2026_11_21.md`.

## 2026-11-21 afternoon: X1 on the card
- Proven: DirectDraw `SetDisplayMode(320,200,8)` switches the card to the VGA half (mode 13h at A0000); test patterns and the moving box are in the right colours on the card's monitor; the desktop returns. Owner saw it.
- Card, M8DDB modex: full frame via Lock 13.4-13.7 fps, flip 13.4 fps, fill 15.1/s; hashes equal the bed's. Findings: `docs/directdraw_modex_vga_half_2026_11_21.md`.
- The card runs the X1 build: `M8VGB.DRV` once the bench batch finishes (before it `M8VGA.DRV`; baseline `M8BG.DRV`). The diagnostic escape 0x4D43 was removed after bring-up.
- Next: Doom 95 on the card (`C:\DOOM95\DOOM95.EXE`, shareware `DOOM1.WAD`) with `-timedemo demo1`, then the delta flip.
