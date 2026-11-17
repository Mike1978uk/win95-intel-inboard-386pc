# X20 paging during twist, card, 2026-11-18 - not measured, card hung

Real 5160, desktop 800x600x16, driver as in the 11-18 handoff. Runner: PERFLOG X20twist 200 started
detached, then `C:\TWIST\RUN.BAT` (M8TWIST), census at +80 s and +140 s.

- Run 1 started 22:50:18; the agent stopped answering during it (ping and TCP fail). No CSV, no census.
- Owner saw twist running slower than the 6.73 fps run, with disk activity through most of it.
- At the hang (`hang_photo.jpg`): the "D3D Example" window, not full screen, menu bar drawn, client area
  black, hourglass in the middle. The owner restarted the card.
- Conditions differing from the 6.73 fps run: PERFLOG sampling in the background; an earlier twist
  (stopped script) had been closed with Alt+F4 minutes before.
- Not known: whether twist or PERFLOG hung first, and whether paging caused it.
- Next: 60 s PERFLOG alone as the baseline, then one twist run at a time.
