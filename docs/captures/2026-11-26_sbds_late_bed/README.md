# SBDS late start on Play, bed (2026-11-26)

SBDS md5 `45bed8a5` (trace): a Play adds the sound into the ring already written, from 10 ms ahead of the
8237. M8DD32 `05327601` (VGA path leaves H_BUSY clear). Doom 95 with sound.

- Stats (`SBDS_late.REG`): ticks 2,266, underruns 7, lead 160 ms, DMA sleeps 21 / wakes 20,
  **late starts 8, late bytes 10,812** (about 60 ms each).
- Most Plays come after 300 ms of silence, with the DMA asleep: the wake rewrote the ring with a whole lead
  of silence first, so those sounds still started ~160 ms late. Next build: the wake writes from the guard
  and ticks at once (SBDS `11:02` build).
- By ear (owner): one moment where the pistol's firing frame held while the sound stuttered. Not explained
  by these counters; watch for it on the card.
