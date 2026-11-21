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
1. **X1: 320x200x8 as the driver's own mode on the VGA half.** Mode table row, ValidateMode, ReEnable:
   output to the VGA half, INT 10h 13h, primary at A0000 pitch 320; back to the 8514 and repaint on exit.
   Bed with M8DDB modex, then Doom 95 on the card: `-timedemo demo1`, then smaller views. Ceiling ~14 fps.
2. **X40 racer, step 1:** core + DOS front end on m8dos, no sound. Card fps and writes a frame.
3. **D14 a, two runs:** tunnel under EIPSAMP with the agent off; teach the sampler to name 16-bit code.
4. **X39** if Windows hangs before the taskbar again: boot DOSNET, read BOOTLOG first; no agent traffic until the
   taskbar is up.
5. **X32** DOS agent exit key and missing-file refusal.
6. Title mode checks for the "check" rows in `docs/titles_by_route_2026_11_21.md`.
