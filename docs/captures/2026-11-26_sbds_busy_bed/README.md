# SBDS stage 5 with H_BUSY set in VGA mode, bed (2026-11-26)

SBDS md5 `d3c8c811` (trace); M8DD32 `52a0643b` (H_BUSY around VGA-mode Blt/Flip/Unlock). Doom 95 with
sound; Doom was started a second time and cancelled before play.

- Stats (`SBDS_busy.REG`): ticks 2,407, busy seen 297 (12 %), deferred 269, mixed while busy 28,
  **underruns 8, lead 180 ms** (previous run, no busy flag: 4 and 140 ms; earlier runs 5-7).
- The handshake now works end to end, but on the bed deferring costs margin: more underruns, longer lead.
- By ear (owner): FM music a little more stuttery. One listen; FM is not SBDS's path.
- Decision (owner): VGA-mode path keeps H_BUSY clear (M8DD32 back to the committed code plus a comment
  saying why); the handshake stays for the engine paths, to be tested with a title drawing through the
  engine. SBDS keeps the hot-set and busy-seen counters.
