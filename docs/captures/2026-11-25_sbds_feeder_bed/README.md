# SBDS feeder on the bed (2026-11-25)

Stage 4. Trace build md5 `54c4d6a9`, bed `vm_5160_dbg` (387). Output packed four bytes to a dword store;
a lone 8-bit mono channel over a silent primary is mixed straight into the ring (no scratch, no stores in
the loop); several channels still meet in the scratch. Asleep, the tick runs every 100 ms.

- Owner: DSTEST both tests and Doom 95 sounded right; play slows a little when monsters are on screen,
  as it did before (the game, not the sound).
- Stats at the last engine stop: ticks 2,859, **underruns 25**, voices refused 0, DMA sleeps 21 / wakes 20,
  most voices at once 2, engine runs 3. Doom created 25 channels in this run (it loads sounds as needed).
- Underruns mean the tick came more than ~40 ms late: the 8237 played old ring bytes and the mixer jumped
  ahead. Not heard as faults by the owner. Response in the next build (md5 `2afbd764`): the lead grows
  20 ms per underrun up to 200 ms and shrinks back to 60 ms after 250 calm ticks; "lead ms now/max" added
  to the stats. Not yet run.
- The bed prices stores and I/O unlike the card (`bed_misprices_io_vs_memory`): what the feeder saves is
  measured on the card only.
