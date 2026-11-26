# Next session: Doom 640x480 is paging (written 2026-11-26)

Started from `next_session_2026_11_27.md`. Owner drives Doom 95 by hand on the bed; agent-driven timedemo
is dropped (it is the only route that faults Doom).

## Done 11-26 (pm)

- M8DD32 writes run stats to `HKLM\Software\M8DD` at every surface destroy (Frames, Ms, Fps100, Locks,
  Unlocks, Blts, GapMaxMs, Slow100Ms, PageIns, EngineWrites, VgaMode). Results and table:
  `captures/2026-11-26_m8dd_runstats_bed/README.md`.
- SBDS clean exit proven (all channels released, Close arrives). "Sound after fault" = agent route only; parked.
- Bed `vm_5160_dbg` carries M8DD32 `53e4b109` and the SBDS **trace** build `638fb0e5`; old M8DD32 as
  `M8DD32.B26`. Put the release SBDS back before any timing run.

## Findings

1. Doom's frame = Lock/Unlock, not Flip (~5 pairs per Flip). 320x200 ~12.5/s, 640x480 ~1.5/s.
2. 640x480 makes 4.5-17.6 page-ins per lock (320x200 ~2). Each is an XT-CF read: likely the stutter.
3. 640x480 sends pixels (EngineWrites 0): the M8FEED budget work does not touch it yet.

## Owner's time (11-26)

Doom runs are intensive for the owner to drive. At most one Doom run a round, every counter batched into
it; check the capture two ways before asking for another (one of three runs on 11-26 was lost to a wrong
grep: regedit writes `software\`, lowercase). Item 2 below should first try an unattended stand-in: an
M8DDB mode or a small DirectDraw test at 640x480 with a 300 KB system-memory back buffer.

## Next, in order

1. Counter fix (no run): time Unlocks as well as Flips; leave gaps > 1 s out of the elapsed time.
2. Where the 640x480 page-ins come from. Cheapest first: System Properties Performance tab (free memory)
   with Doom at 640x480; then whose pages (300 KB back buffer in RAM, Doom's heap, our DLL).
3. Then the 640x480 send path (pixels vs engine), and old items: M8FEED adaptive budget, skip unchanged
   frames (#54), M8FEED avoiding SBDS's tick.
4. Card run last, by the owner.

## Also (unchanged)

X41 tiled wallpaper; tidy the card desktop; FM music stalls, dxdiag Music crash, per-sound play cap parked.
