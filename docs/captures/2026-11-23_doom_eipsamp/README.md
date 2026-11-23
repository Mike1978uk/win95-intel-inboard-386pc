# Doom 95 with sound under EIPSAMP, card, 2026-11-23 (T2)

Card boot 507, driver `M8CW.DRV` (T3b build), SWAPCOMP out, 800x600x16 desktop. `C:\DOOM95\T2D.BAT`:
PERFLOG `T2doom` 200 s, then `EIPSAMP 180 5 C:\DOOM95\DOOM95.EXE`; ENTER sent by the agent at +40 s (launcher).
Files: `EIPSAMP.TXT`, `GHEAP16.TXT`, `PERFLOG.CSV` (label `T2doom`).

**These runs were windowed Doom (640x480, GDI/DIB), not the 320x200 full-screen Mode X path** used on
11-21/22: no switch was given and the launcher's saved setting was a window. `DOOM95.EXE` takes
`-320x200` (full screen) or `-640x480w` (window) etc. The profile below is the GDI path's.

## Main thread by phase

| s | 16-bit | lock wait | DSOUND | DOOM95 | KERNEL32 |
|---|---|---|---|---|---|
| 0-77 launcher, start-up | 31-79 % | 6-55 % | 0 | 0-23 % | 2-15 % |
| 77-123 game running | 21-70 % | 2-27 % | 0 | 13-31 % | 1-47 % |
| 123-138 | 55 % | 14 % | 11 % | 3 % | 13 % |
| 138-184 | 4-12 % | 0 | **88-96 %** | 0 | 0 |

- While Doom runs, 16-bit code is the largest share: DISPLAY seg 1 (`+B6AF` hottest), DIBENG seg 5, DISPLAY
  seg 2. Doom 95 draws through GDI/DIB into our 16-bit driver, not DirectDraw (DDRAW 0.1 %). Lock waits
  enter from `DOOMLNCH.DLL+2637` and USER32. This is the title where the 16/32-bit switches matter.
- From ~138 s the main thread does not leave DSOUND: 2,367 samples on three stores of DSOUND's memset
  (`DSOUND.DLL+2FE04/07/0A`, a fill of 32 bytes a pass), called from `+30518` (fill a buffer of `[ecx+8]`
  bytes at `[ecx+0xC]` with silence, 80h or 00h), reached from `+395D7` (a play/stop state method).
- Not paging: page-ins ~2/s over those 45 s. PERFLOG got no row for 29 s in that window.
- **Cause of the stall (owner's photo of the screen):** a fault dialog, title `Msgsrv32`, "An error has
  occurred in your program" with Close/Ignore (the Win16 fault dialog), over Doom, sound looping. MSGSRV32
  hosts the 16-bit multimedia callbacks (MMSYSTEM, the SB Pro wave driver), so the fault is in the 16-bit
  sound path; DMA replays the last buffer once its callback is dead. Close left an hourglass: power cycle.
  The "every store trapping" reading is withdrawn: Doom's main thread sat in the fill behind the fault.
- Likely the 11-22 "sound loop, then hang" (with SWAPCOMP out this time), and possibly #55.

## Repeats (boot 508, 509), Dr. Watson installed (`C:\WINDOWS\DRWATSON.EXE`, from `WIN95_20.CAB`)

| Run | Sampler | Result |
|---|---|---|
| `T2E` | off | 7 min of play, no fault, quit from the menu cleanly |
| `T2F` | EIPSAMP 200 s | no fault; DSOUND 55 % at 140-158 s then 1 %: the fill is a normal phase |

The fault is intermittent (1 run in 3 today) and not tied to the sampler. Dr. Watson stays in the
start-up BATs so the next fault leaves a log naming the module.

## Our 16-bit driver during game time (77-123 s, 2,569 samples, `m8disp.map` of the M8CW build)

| Share of the main thread | Code | What it does |
|---|---|---|
| ~12 % | `M8_FB16_ROWDIFF` + `M8DD_TEXT` statics (`m8_fb16_changed`; the map names them `m8_fb16_check`, the nearest public symbol) | fingerprints each 32-pixel piece of each row sent |
| ~4.6 % | `m8_hw_write_rect` +B6AF | the pixel loop to the card (bus) |
| ~3.6 % | `M8_XLAT`, `M8_FB16_CONV` | 5-6-5 to the 8-bit palette, dithered |

Windowed Doom redraws its whole view each frame, so the fingerprints find every piece changed: they cost
about three times the conversion and save no bus.

**Tried and reverted (not committed):** the VGA half's adaptive delta rule (`m8dd_vga_send`) in
`m8_fb16_send`: a rectangle found mostly sent from RAM goes whole for 16 sends, then is re-tested. Desktop
bench, card, against M8CW re-benched the same evening (`card_cwb1,2`), pixels same in every run:

| Build | Rule | Result |
|---|---|---|
| `card_ad` | one dense send | `stretch_dib` 0.17, `stretchblt` 0.22, `fill_hatch` 0.28, `bmp_card` 0.36, both runs |
| `card_ae` | two in a row | those recovered; `draw_icon` 0.90 (an icon's mask and image are two dense sends) |
| `card_af` | two in a row, >= 16,384 px | run 1 as `card_ad` again, run 2 clean: depends on what earlier tests left |

Reading: going whole forgets the fingerprints, and at 800x600 the background store serves alternating
pictures from them; once whole, every send looks dense and it stays whole. Not confirmed whether the forget
also empties the store itself. The rule is safe only where the store is off (not 800x600), and the
only title there is windowed Doom, which is played in Mode X. Dropped. `_TEXT` has 263 bytes left (0xFEF9 used): new
fb16 code goes in `M8DD_TEXT`, which cannot use 32-bit multiply.
