# MTM dashboard off, bed vm_5160_mtm, 2026-11-18 (800x600x16, [diag] runs=1)
Dashboard off: Options > Graphics, untick Dashboard (Steering Wheel greys out), OK, in the menus before the race; MTMHW.INI not checked.
Not tried: chase view (V). The agent is unreachable during the race, so no keys could be sent.
Race reached by agent clicks (Mtm_hw icon, RACES, Circuit, RACE); b14-b16 show no dashboard, HUD text only.
Run ~4.5 min wall from the first race frame (clock 6.5 s of race); census M8CENSUS.TXT before-/after-dashoff.
Colour runs a row: 17 (206,981 runs / 12,000 rows), rows <= 4 runs 31 %. 11-17 cockpit: 124.
Lock frames: 25, all 480 rows each: the back buffer is still Locked whole every frame. Art Lock is not gone.
Flips: 26 (11-17: 12 in the same time); callback ticks: lock 39.6M, flip 38.8M, ours 63.5M (11-17: ours 17M).
So the runs are 7x fewer but Locks remain (likely HUD text / whole-frame read-modify); a frame is not triangles only.
Census counters are cumulative from boot (menus and loading included, as 11-17).
Shutdown: Alt+F4 hung the game; End Task, then Shut Down reached "safe to turn off"; process stopped.
