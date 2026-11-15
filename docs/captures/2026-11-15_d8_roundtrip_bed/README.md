# 8 bpp full-screen round trip, Mach8 bed, 2026-11-15

Bed `vm_5160_mcp`, 800x600x16 desktop. Each pass: fresh boot, M8IDLE, M8BENCH with PERFLOG
logging alongside (150 s), WORMHOLE run 20 s and closed with Alt+F4, then the same bench again.
`run.log`, `M8BENCH.txt` (the four runs), `PERFLOG*.CSV` (read with
`tools/perflog/perflog_table.py <csv> 10 <label>`).

| pass | driver | bench | page-ins during the bench | locked MB (end) | commit MB (end) |
|---|---|---|---|---|---|
| `rt0` fresh | `32bfaac` (`246710BF`) | 95 s | 1,974 | 2.09 | 11.75 |
| `rt1` after wormhole | same | 200 s | 9,869 | 2.75 | 14.39 |
| `rt2` fresh | shadow freed (`F617E2FD`) | - | 1,893 | 1.99 | 11.62 |
| `rt3` after wormhole | same | 97 s | 2,164 | 2.21 | 13.25 |

Pixels equal in all 36 tests in every pass.

Cause: going to 8 bpp for a full-screen program, `m8dd_mode_changed` allocates the 8 bpp Lock
shadow, 640 x (512 + 480) bytes, `GMEM_FIXED` from a DLL, so page-locked; back at 16 bpp nothing
used or freed it. Fix: `m8dd_shadow_free` after DDRAW is reset to the 16 bpp primary.

After the fix, `rt3` against `rt2`: geometric mean 0.90 (yesterday's unfixed round trip: 0.63),
mem_to_mem 2.6 -> 2.3 ms (unfixed: 342 ms). Left: commit +1.6 MB and locked +0.2 MB against a
fresh boot; DDHELP.EXE and DDRAW.DLL stay loaded after a program's first DirectDraw use, which
any driver would show. Not yet checked.
