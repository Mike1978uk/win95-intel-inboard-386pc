# Doom 95 with sound under EIPSAMP, card, 2026-11-23 (T2)

Card boot 507, driver `M8CW.DRV` (T3b build), SWAPCOMP out, 800x600x16 desktop. `C:\DOOM95\T2D.BAT`:
PERFLOG `T2doom` 200 s, then `EIPSAMP 180 5 C:\DOOM95\DOOM95.EXE`; ENTER sent by the agent at +40 s (launcher).
Files: `EIPSAMP.TXT`, `GHEAP16.TXT`, `PERFLOG.CSV` (label `T2doom`).

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
- Not paging: page-ins ~2/s over those 45 s. Even a 180 MB fill would be done in that time, and no sample
  falls back into Doom, so it is one call whose stores are each that slow: every store trapping is the
  reading that fits, not proven. PERFLOG got no row for 29 s in that window (other threads starved).
- After the run: CPU 100 % (M8IDLE); screenshot fails (full screen). Doom's quit keys were sent at ~15:05;
  the agent's port then refused connections for 5+ minutes: card hung, owner asked to restart.
- Likely the 11-22 "sound loop, then hang" (with SWAPCOMP out this time), and possibly #55.

Next: where the DSOUND buffer lives (its address against the VxD-trapped ranges) and what traps a store
to it; a run with `-nosound` as the control.
