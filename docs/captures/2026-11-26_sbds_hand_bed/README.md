# SBDS stage 5 hand-over check on the bed (2026-11-26)

SBDS md5 `d3c8c811` (trace, hot-set counters); M8DD32 `f10e59e8` (hand-over traced). Doom 95 with sound.

- `M8TRACE.TXT`: `sbds hand: hot 80114e18 taken`; `vga flip: the VGA-mode path`.
- Stats (`SBDS_hand.REG`): ticks 1,657, underruns 4, lead 140 ms, hot sets 1, refused 0, hot addr
  0x80114e18, **busy seen 0**, deferred 0.
- So the hand-over works. Doom 95 runs 320x200 and flips through `m8dd_vga_flip`, a CPU copy to A0000
  that uses no engine and so never set `H_BUSY`. Fix: M8DD32 sets `H_BUSY` around the VGA-mode Blt,
  Flip and Unlock (old value restored; 2 is M8FEED's), md5 `52a0643b`. Result in the next run.
