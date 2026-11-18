# V9x 0.19.0 tray redraws on the 5160, 640x480x8 (2026-11-18 night)

`C:\M8EPOCH.EXE 20` run through the agent (agent busy for the whole sample), fresh `C:\M8EPOCH.TXT` each arm.
M8EPOCH counts the Mach8 driver's GDI-to-screen work (escape 4D41h).

| Arm | AGENT.INI | dibtodev moved | copy_hw | readback |
|---|---|---|---|---|
| A | `tray_flash=0` | 3 samples, +9 | +23 | +6 |
| B | `tray_flash=1` (agent restarted by reboot, boot 454) | 49 samples, +147 | +345 | +98 |

For reference, agent 0.12 on 11-17: 53 redraws in 20 s. The card is left at `tray_flash=0` (takes effect at the
next agent start). Explorer-restart re-add: not tested. The 11-18 800x600x16 files in `../2026-11-18_tray019_card/`
all begin with the same first block because M8EPOCH appends; that run did not separate the arms.
