# SBDS adaptive lead on the bed (2026-11-25)

Trace build md5 `2afbd764`, bed `vm_5160_dbg` (387). Owner ran DSTEST and Doom 95; export md5 `e7d1042d`
(new; the feeder run's was `3413e0a7`).

- Stats at the last engine stop: ticks 2,124, **underruns 7** (feeder run without adaptation: 25),
  voices refused 0, DMA sleeps 14 / wakes 13, most voices at once 2, **lead now 180 ms, max 180 ms**.
- The lead climbed to 180 ms and had not come back down by the end: the calm rule (250 clean awake ticks)
  was not met, or underruns kept arriving. A 180 ms lead means a new sound can start up to ~180 ms late.
- Next: stage 5 (mix in the display's gaps) should keep ticks on time so the lead stays low. Also worth
  doing: on Play, add the new sound into the ring bytes already written ahead of the 8237 (clipped add),
  so its start does not wait for the lead.
