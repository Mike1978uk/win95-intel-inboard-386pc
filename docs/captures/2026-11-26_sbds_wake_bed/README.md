# SBDS wake from the guard, bed (2026-11-26)

SBDS md5 `638fb0e5` (trace): on a wake the ring is written from 10 ms ahead of the 8237 and the first tick
runs at once; a Play while the DMA runs adds into the ring already written. Doom 95 with sound.

- Stats (`SBDS_wake.REG`): ticks 3,069, underruns 6, lead 140 ms now and max, DMA sleeps 22 / wakes 21,
  late starts 5, late bytes 5,238.
- Last run (late start only): underruns 7, lead 160 ms; most sounds came after a sleep and waited the lead.
- By ear (owner): sounded better, less choppy.
