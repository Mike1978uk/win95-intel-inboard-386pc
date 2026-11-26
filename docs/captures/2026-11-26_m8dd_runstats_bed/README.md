# M8DD32 run stats, Doom 95 on the bed (2026-11-26)

M8DD32 `53e4b109` (run stats + trace), SBDS `638fb0e5` (trace build). Owner drove Doom 95 by hand, quit
normally. Stats: `HKLM\Software\M8DD`, written at every surface destroy (DirectDraw never passes the
primary's create/destroy to the HAL). RUN1/RUN2 = second launch, RUN1b/RUN2b = third.

| Run | Mode | Locks/s | Flips/s | Page-ins | per lock | Engine writes |
|---|---|---|---|---|---|---|
| RUN1  | 320x200 | ~12.3 | 1.9  | 2,764 | 2.5  | 0 |
| RUN1b | 320x200 | ~12.8 | 2.4  | 2,711 | 1.8  | 0 |
| RUN2  | 640x480 | ~1.4  | 0.18 | 5,152 | 17.6 | 0 |
| RUN2b | 640x480 | ~1.7  | 0.40 | 4,615 | 4.5  | 0 |

- Flips are not Doom's frames: ~5 Lock/Unlock pairs per Flip, nearly every Flip >= 100 ms after the last,
  25-47 s gaps (menus) inside `Ms`. `Fps100` as written is wrong for Doom.
- 640x480 pages: 4.5-17.6 page-ins per lock against ~2 at 320x200; each is an XT-CF read.
- 640x480 never used the engine (EngineWrites 0): flip8 path sends pixels.
- SBDS clean exit (`RUN1.REG` first launch, trace): all channels Released, primary Stopped, `Drv.Close`.
  Underruns 4-7, lead 140-180 ms. Sound after a fault happens only on the agent-driven timedemo route.
- `M8TRACE3.TXT`: `run:` lines, registry result 0.
