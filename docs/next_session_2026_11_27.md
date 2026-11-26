# Next session: the graphics retrospective from the sound work (written 2026-11-26)

Started from `next_session_2026_11_26.md`. Owner's aim: sound and graphics cooperating, each the best this
machine allows; demoscene method (measure, use idle silicon); bed first, then the card; then more titles.

## Done 11-26

- Stage 5 hand-over proven (`captures/2026-11-26_sbds_hand_bed/`): SBDS gets the hot address (hot sets 1).
  Doom 95 at 320x200 flips through `m8dd_vga_flip` (CPU copy to A0000), which set no `H_BUSY`.
- Setting `H_BUSY` around the VGA-mode calls doubled bed underruns (4 -> 8, lead 140 -> 180 ms,
  `_busy_bed/`). Reverted (owner); M8DD32 says why in a comment. Handshake kept for the engine paths:
  untested, needs a title drawing through the engine (640x480 Doom).
- SBDS: a Play adds the sound into the ring already written (10 ms guard ahead of the 8237); a wake writes
  from the guard and ticks at once. Bed: underruns 6, lead 140 ms; owner: "sounded better, less choppy".
- **Card** (`captures/2026-11-26_sbds_card/`): release SBDS `7a68e0bd`. Underruns 2, lead 60 ms (max 100).
  M8DDB level with 11-24, all pixels equal: the mixer costs no frame time. Backup `SBDS.T24` on the card.
- `tools/bed_launch.ps1` unpauses the `build_gdb` exe (it starts CPU-paused; this was every "black bed").
  Launch owner runs WITHOUT `-Automation`.
- Card reached with `v9xctl.ps1 -Host 192.168.68.62 -ConnectTimeoutSeconds 60`; the v9x MCP servers are
  beds only. Both agents' `allowed_client` blank (DOS agent checked); `stage_dosnet.py` blanked too.
- Card: keyboard indicator removed (one locale, `5160_tuned_config.md`).

## Unattended Doom 95 timedemo (bed, partly proven)

`DOOM95.EXE -timedemo demo1` (agent exec, detach, working dir `C:\DOOM95`) opens the launcher; New Game (the
default button: Enter) plays the demo with the argument kept. At the end Doom 95 returns to its menu and
shows **no timing result**. `C:\WINCLOSE.EXE doom` closes it cleanly (bed, 11-26). Still to do: press New
Game from the agent (key Enter), and get the number from our side - M8DD32 flip count over elapsed time
(item 2 below). The agent screenshot failed on the bed (error 1460) while Doom was up; use the owner's.

## Next, in order (owner agreed 11-26)

1. **Display end-of-run stats** (retro item 2): M8DD32 writes frames, elapsed ms, flips, page-ins and
   feed counts to the registry when DirectDraw closes, as SBDS's "Stats". Cheap; gives Doom a fps number.
2. **640x480 Doom baseline** on the card with the current driver (owner found it too slow to play; that is
   the yardstick). Unattended via the timedemo route once item 1 exists.
3. **M8FEED adaptive budget** (retro item 1, most likely win): fixed 1 ms tick / 512 writes now; take
   bigger, rarer bites when the queue is long, as the sound lead grows. Each tick empties the cache.
   Bed for pixels, card for speed.
4. Then retro items 3 (skip unchanged full-screen frames, check #54), 4 (M8FEED avoids SBDS's tick),
   5 (draw small changes into the frame already sent). The engine-path busy handshake gets its test here.

## Also

- X41 tiled wallpaper with an aperiodic look (worklist): measure Doom page-ins with/without wallpaper first.
- Tidy the card desktop: owner moved icons into a `Shortcuts` folder; clean up at the end.
- Doom 95 Spectres ("invisible" monster): id's fuzz effect, not our driver. Noted only.
- Parked as before: FM music stalls (Doom's timing), dxdiag Music crash, per-sound play cap.
