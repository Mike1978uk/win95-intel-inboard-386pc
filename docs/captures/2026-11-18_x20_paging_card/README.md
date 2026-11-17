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

## Retry 2026-11-18 (card power-cycled, idle first): no hang in either run
Files: PERFLOG_idle.csv, PERFLOG_twist1/2.csv, M8CENSUS_twist1/2.txt (CSV/TXT append; read the label
X20idle / X20twist / X20twist2; X20twist-S28-234422 is the earlier hung attempt, it sampled 106 rows).
Rate = delta cPageIns/cPageOuts over delta ms. Twist ran ~176 s; table covers the first 140 s (no census/exit).

| | page-ins/s | page-outs/s | faults/s | cpgFree |
|---|---|---|---|---|
| idle 60 s | 1.6 | 1.1 | 8.5 | 0 (field is bytes) |
| twist run 1, 0-140 s | 23.4 | 9.8 | 39 | 0-0.3 MB, mostly ~0.15 |
| twist run 2, 0-140 s | 24.2 | 11.1 | 37 | 0-0.36 MB |
| run 1 / 2, first 60 s | 33 / 37 | 13 / 18 | 60 / 59 | 4 KB at 60 s |

- twist fps (census flips a->b over host-timed ~62 s): run 1 409/62.9 = 6.5; run 2 411/62.1 = 6.6.
- Full screen: not known; v9x screenshot failed both runs (Win32 1460, screenshot helper failed, ~80 s in).
- Paging is 15-20x idle while twist runs, heaviest in the first minute; free memory sits near zero.
- Hung: no; agent answered throughout, Alt+F4 closed twist, both CSV and census fetched.
