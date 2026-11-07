# Agent-free run on `M8D3DX`, 2026-11-07

`NOAGPAGE`, owner at the machine, agent off, `M8D3DF` at normal priority. CF read on the host afterwards.

| fps | `M8D3DD` agent-free, 11-06 | `M8D3DX` |
|---|---|---|
| hal_tiny, first / last | 63-67 / 71.5-71.8 | 62.8 / 67.4 |
| hal_small | 66-67 | 65.5 |
| hal_large | 36.0 | 47.5 |
| hal_static | 206-208 | 189.7 |
| hal_colour | 71.6-71.7 | 68.7 |
| hal_many | 23.5-23.7 | 41.4 |
| hal_turn32, hal_turn100 | - | 69.6, 35.3 |
| hal_grad | - | 44.6 |
| hal_many_c, no clip | - | 47.4 |

Small tests are a little under the older build's, not at the 72 cap: not explained. `hal_many_e` 24.6 had a 902 ms stall, one run.

| 300 s of PERFLOG, deltas | `noag` M8D3D8, locked | `r892` pageable | `xa` pageable |
|---|---|---|---|
| Page-outs | 2,815 | 4,889 | 5,817 |
| Discards | 7,666 | 5,851 | 7,083 |
| Compressed writes | 2,592 | 4,692 | 5,588 |
| Locked bytes at start | 2,920,448 | 1,896,448 | 1,892,352 |
| PAGETIME open, switch 1 / 2 / 3, s | 64.6, 12.7 / 12.5 / 11.8 | 62.7, 15.4 / 12.7 / 12.2 | 66.1, 14.9 / 12.5 / 12.0 |

Pageable buffers: page-outs about double, in two runs now; no time gained. Builds differ, so a locked build of today's code is the test.

**Ghosts:** owner saw them again with several programs open. `FB16CHK` after PAGETIME and at the end: 0 pixels differ, 0 sends overlapped. It ran after the programs had closed, so it cannot speak for the ghosts while they were up. No overlap ever counted.

**Hang:** dxdiag's Direct3D button, sound looping, **agent off**. After a reboot dxdiag's tests ran: owner, colours better, no blockiness, good frame rate. So the agent is not the sole cause.

**Gradients:** owner: "quite big gradient colours and not many per triangle", multi-colour rather than fine. Up to 8 bands and a 5 x 9 x 5 palette.

`WINCLOSE WAIT PERFLOG` logged exit -1, not a path in its source, in this run and `r892`. Owner closes the last window by hand. Cause not found.
