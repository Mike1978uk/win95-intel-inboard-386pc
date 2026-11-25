# SBDS channel probe on the bed (2026-11-25)

Stage 3 probe (`docs/sbds_stage3_design_2026_11_25.md`): SBDS offers 8 channels and accepts 8-bit
secondaries, keeps each cursor by the clock, mixes nothing. Trace build md5 `bc0514af`, bed `vm_5160_dbg`
(387). Owner ran `DSTEST once`, `DSTEST loop`, then Doom 95 briefly. Decode: `trace.txt`.

- **Risk 1 cleared:** every channel's memory is in the shared arena (`8252B08C`-`8253DAE0`), so a
  timer in any context can read it.
- DSTEST: its 22 kHz mono buffer (CTRLVOLUME) went to channel 0; both runs ended and released, no hang.
- Doom 95: primary 11,025 Hz 8-bit stereo again. It **creates 8 buffers at start** (11,025 Hz mono,
  CTRLVOLUME+PAN+FREQUENCY, 5,661-18,096 B), filling all 8 channels; DSOUND then asked `GetCaps` 80 times
  and, finding none free, left every later sound to its own mixer. So channels are held per sound, not
  per voice: offer many (descriptors cost 64 B), mix only those playing.
- Channel 0 is 5,661 B, the size of Doom's pistol sample (`DSPISTOL` less its 8-byte header): played
  11 times, one-shot, each with its own pitch (`SetFrequency` 10,295-10,535 Hz) and volume 87E8h (0.53).
  The owner heard shooting; if that was the pistol, something else played it - to check.
- Volume and pan arrive as 16.16 amp factors; every call in this run had left = right (no pan seen).
  Doom muted all 8 (0/0) and restored them (FFFFh) once at start.
- After the bed window closed, 86Box stayed running with no window until killed: the automation
  connection may hold it open.
