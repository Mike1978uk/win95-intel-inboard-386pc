# Next session: T5 DirectSound driver, after the first build (written 2026-11-23, late)

Started from `next_session_2026_11_24.md`. Design: `sbds_design_2026_11_23.md`. Sources:
`sbpro_dsound_sources_2026_11_23.md`. Split results: `captures/2026-11-23_t5_split/`.

## State

- **Card:** boot 518, `C:\WINDOWS\SYSTEM\SBDS.VXD` = the **first** build (6,612 B, no trace).
  `SYSTEM.INI` has `device=SBDS.VXD` after `[386Enh]` (owner approved over the agent). Backup of the
  file before the edit: `C:\WINDOWS\SYSTEM.SBD`. `SYSTEM.BSR` is an **older** backup (SWAPCOMP still on),
  left untouched. Revert from DOS: `copy C:\WINDOWS\SYSTEM.SBD C:\WINDOWS\SYSTEM.INI`. Nothing played
  through the driver on the card yet. `C:\M8DDB.EXE` is the build with the caps line; old one `C:\M8DDB.T4`.
- **Bed** `vm_5160_dbg` may still be running: boot 293, newest `SBDS.VXD` (9,688 B, trace on), `SYSTEM.INI`
  backup `C:\WINDOWS\SYSTEM.BSR` (bed only). **Gotcha:** its exe (`86box_auto\build_gdb`) starts with the
  CPU paused for GDB (`gdbstub.c:1973`); connect to 127.0.0.1:12345 and disconnect to release it.
  The V9x agent answers a minute or two after the desktop appears.
- Uncommitted: `drivers/sbds/`, `tools/vxddis.py`, `docs/sbds_design_2026_11_23.md`,
  `tools/m8ddbench/M8DDB.C` (caps line), this file.

## Results tonight

| | Result |
|---|---|
| Driver loads, registers | State 0xF, `RegResult` 0, buffer 16 KB at phys 98000h (below 640 KB, aligned). DMA audit clean |
| DirectSound uses it | M8DDB: `EMULDRIVER` gone; trace shows Open, CreateSoundBuffer (primary), SetFormat, Play |
| Stereo 22 kHz plays | Bed, owner heard the tone throughout. Position rate ~45 B/ms = 22050 x 2, wraps cleanly (boot 293 trace) |
| Mono 22 kHz plays | Bed, M8DDB `modexdf`, owner heard it |
| dxdiag sound 1 (mono) | **Failed** with our driver (boot 291): DSOUND stopped the primary ~3,668 bytes after Play, 4 GetPosition calls |
| dxdiag, "No acceleration" | Emulation itself: sound 1 ok, 2 distorted, 3 ok, 4 silent. Same path as the start-up sound stutter (owner: bed and card) |
| Boot 292 oddity | DSOUND never opened our driver (Opens 0, M8DDB too) though registered; next boot it did. Not explained |
| Caps oddity | DSOUND reports `DSCAPS` flags 0 to programs; we report PRIMARYMONO/STEREO/8BIT |

## Next

1. **dxdiag mono failure** (owner: "that's the route software would take"). Trace now has ms times and is
   written on every call except GetPosition, and cleared at boot. Bed: dxdiag, slider Full, Test
   DirectSound, then `regedit /e C:\SBDS.REG HKEY_LOCAL_MACHINE\Software\SBDS` and
   `python drivers/sbds/sbds_trace.py SBDS.REG`. First suspect: write cursor = play cursor. DSOUND's own
   app-compat list pads cursors for real games (`DSAPPHACKID_PADCURSORS`, `MediaResources\DirectSound`).
   Try write = play + a DMA-safe lead.
2. Caps flags 0: find why DSOUND drops ours (`GetCaps` layout, or DSOUND builds DSCAPS itself).
3. Card: install the trace build, reboot, `M8DDB modexd/modexdf/modexdi` twice each against
   `captures/2026-11-23_t5_split/`. Target: `modexd` near PlaySound's 0.94, `modexdi` near silent.
4. Stereo channel order (Creative's one-byte lead-in) not done; check by ear with a left-only sound.
5. Start-up sound stutter: emulation/wave path, reproduces in the bed. Worklist item, separate from T5.

Commit, cheapest first: Haiku commits the files above (code commit, docs commit). Committing
`drivers/sbds/build/SBDS.VXD` follows what other drivers do (`git ls-files drivers/nmiwatch/build`).
