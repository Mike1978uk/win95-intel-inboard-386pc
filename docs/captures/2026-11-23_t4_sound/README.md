# T4: what sound costs a Mode X frame, card, 2026-11-23

Card boot 517, driver `M8CW.DRV` (`253b949`), 800x600x16 desktop, M8IDLE quiet before the first run. Each
run is `C:\M8DDB.EXE <mode> <label>` through the agent's `exec` (300 s timeout; all exited 0 in 60-90 s),
in the order x1 s1 d1 x2 s2 d2. `modexs` loops a sound through `PlaySound` (waveOut, the SB Pro driver);
`modexd` loops a DirectSound buffer and starts a 100 ms shot (emulated DirectSound: no DS driver for the
SB Pro). `python t4tab.py M8DDB.TXT` prints the table.

| Test | silent x1, x2 | PlaySound s1, s2 | DirectSound d1, d2 | PlaySound / silent | DirectSound / silent |
|---|---|---|---|---|---|
| `mx_fill` | 11.71, 10.97 | 10.65, 10.50 | 5.80, 10.54 | 0.93 | 0.72 |
| `mx_frame` | 13.50, 13.63 | 12.68, 12.83 | 10.86, 10.84 | 0.94 | 0.80 |
| `mx_box` | 14.83, 14.84 | 13.94, 14.00 | 11.54, 11.89 | 0.94 | 0.79 |
| `mx_same` | 14.97, 14.85 | 14.05, 14.02 | 10.96, 11.75 | 0.94 | 0.76 |
| `mx_lock` | 2002, 2035 | 1893, 1905 | 1598, 1563 | 0.94 | 0.78 |
| `mx_flip` | 43.73, 43.77 | 41.07, 41.48 | 34.77, 33.51 | 0.94 | 0.78 |
| `mx_flipblt` | 41.76, 41.85 | 39.63, 39.32 | 33.07, 31.46 | 0.94 | 0.77 |

- Playing sound costs ~6 % of every test; emulated DirectSound ~22 %. The ~16 % between them is
  DirectSound's software emulation (its mixer thread feeding waveOut), on every frame of a DirectSound title
  such as Doom 95. That is T5's case: a DirectSound driver for the SB Pro (#58) would leave ~6 %.
- `mx_fill` d1 (5.80) is one outlier; d2 agrees with the other tests.
- An earlier attempt ran the six from one BAT: it stalled after the first run with the desktop idle. Run
  each through `exec` with a timeout.

Across the stack: DOS titles and DirectXT drive the SB Pro themselves, so they pay the ~6 % at most; the
emulation cost is Windows DirectSound's alone.
