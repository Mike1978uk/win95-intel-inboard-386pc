# SBDS mixes: first run on the bed (2026-11-25)

Stage 3 (`docs/sbds_stage3_design_2026_11_25.md`), trace build md5 `8641b7d4` (release 12,868 B), bed
`vm_5160_dbg` (387). SBDS offers 64 channels; a 20 ms global time-out adds the playing channels (table
volume/pan, 16.16 pitch step) to DSOUND's primary and writes the ring 60 ms ahead of the 8237.

- Owner: `DSTEST once` and `loop` audible (the probe build was silent); Doom 95 effects audible, pistol
  included. No hang. Not yet judged by ear: clicks, pitch accuracy, stereo channel order.
- Doom 95 put **all 43 of its sounds** in our channels (43 `Ch.Created`, all ok); no `Duplicate` calls.
  Doom replays one buffer per sound rather than copying it: the pistol is channel 0 throughout, each
  play ending on its own (Stop 552-912 ms after Play, sample 513 ms at 11,025 Hz, plus tick and poll).
- Pan is used: amp pairs 8659h/834Ah, 84D0h/81CAh, FFFFh/FD11h alongside the centred FFFFh/FFFFh and
  87E8h. So stereo output is needed whenever Doom plays a positioned sound.
- The trace ring wrapped (673 calls, last 512 kept): the start of the run is lost.
- Not measured: ticks, underruns, voices refused (`dwTicks`, `dwUnderruns`, `dwVoiceFull` are kept but not
  yet exported), whether DSOUND's own mixer is now idle.
