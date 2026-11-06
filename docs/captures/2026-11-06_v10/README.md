# V10: two pages for Direct3D at 640x480, 2026-11-06

Build `M8D3D9`: `M8DISP.DRV` md5 3fe7938e347b3c7a692c23772ad106b4, `M8DD32.DLL` md5 8975bcce9958ce01da2198a45cba2edb. Card boot 226, agent running. Log: `M8D3DF.TXT`, labels `v10a`, `v10b`.

**Owner, by eye:** black overlaps gone, colours look much better, smaller shapes stutter at times, `hal_small` best. The colour code did not change, so "better" is the absence of frames part-erased to black.

**Page path ran on the card:** page wait 3.9-7.6 ms on every moving test, 0.00 on `hal_static`, 0 bad frames.

| fps | hal_tiny | hal_small | hal_large | hal_static | hal_colour | hal_many |
|---|---|---|---|---|---|---|
| `M8D3D7` `norm`, agent running | 31.1 | 30.3 | 16.3 | 98.8 | 32.0 | 7.5 |
| V10 `v10a`, agent running | 23.6 | 44.1 | 23.2 | 171.3 | 36.4 | 15.3 |
| V10 `v10b`, agent running | 24.3 | 46.4 | 19.0 | 184.6 | 28.5 | 16.6 |
| `M8D3D8` `own`, agent off | 69.7-71.4 | 70.4-73.3 | 39.6-39.8 | 207 | 75.5 | 22.5 |

The two agent-running builds differ by more than V10; compare `high` with `high`, or agent-free with agent-free, before reading a gain or loss into this.

**High priority, like with like, agent running:**

| | hal_tiny | hal_small | hal_large | hal_static | hal_colour | hal_many |
|---|---|---|---|---|---|---|
| `M8D3D7` `hi`, fps | 65.2 | 66.2 | 36.2 | 196.3 | 70.8 | 19.2 |
| V10 `v10hi1`, fps | 35.0 | 36.3 | 29.7 | 177.2 | 35.2 | 15.6 |
| V10 page wait, ms | 8.59 | 8.18 | 3.42 | 0.00 | 5.73 | 9.48 |

**The stutter:** the small tests lock near 36 fps, half the 72 Hz refresh. Without the wait a small frame takes ~20 ms, over the 13.85 ms a refresh gives, so each frame waits for the next one. A frame under 13.85 ms should give 72 fps.

**Unexplained:** excluding the wait, a small frame is ~20 ms against ~15 ms on `M8D3D7` `hi`: `clear` and `d3d`, outside the page code, each cost about 2 ms more. One run, agent running.

**Card hang, boot 226, owner rebooted, boot 227:** black screen, egg timer, after `v10hi1` finished and closed its log; `v10hi2` left no line, though its header may only have been buffered. Same symptom as `M8D3D6` on 11-05, also the second of two runs started by the agent, before V10 existed. `v10b`, started by M8NOAG, did not hang. Two reports, cause open, V10 not cleared.

**Not settled:** V10 agent-free.

**Bed:** `vm_5160_mcp`, driver loaded and ran with 0 bad frames, lit counts as the card. Too slow to exercise the page wait, so the card is the test. The bed's image backup is `vm_5160_mcp/card.img.pre_v10`.
