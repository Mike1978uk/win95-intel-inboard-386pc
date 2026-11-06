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

**Second card hang, boot 227, owner rebooted:** with-agent disk list `AGDISK`, started by the agent 13:33, on the desktop in `PAGETIME` with Word, Paint and PSP open. Mouse dead, ping dead by 13:50. The same list with the agent off finished on `M8D3D8` that morning. V10's page code was not active, but the build was `M8D3D9`.

**`M8D3DA` (V6, V19, V5 on V10), boot 230, agent running, second pair of `v10a`, `v10b` in the log:** md5 `M8DISP.DRV` 7056185233f2ef7b5ded7e05c3559001, `M8DD32.DLL` 7c51fe70119bd54e4a8f66eb7373bbd8. Owner: small triangles a little stuttery; mid, large and many smoother.

| fps | hal_tiny | hal_small | hal_large | hal_static | hal_colour | hal_many |
|---|---|---|---|---|---|---|
| `M8D3D9` | 23.6-24.3 | 44.1-46.4 | 19.0-23.2 | 171-185 | 28.5-36.4 | 15.3-16.6 |
| `M8D3DA` | 30.8-32.2 | 28.5-33.9 | 24.2-29.5 | 182-186 | 28.0-51.5 | 17.6-18.8 |

Spread between runs is as large as most differences. `hal_tiny` runs first after the mode set: its clear is 11-13 ms and worst frame 350-377 ms, against 1-4 ms clears later, and the spin just before it loses 26-44 %. So its stutter may belong to its position, not its size. Test: run it again later in the same session.

**V10 agent-free, `M8D3DA`, owner:** no stutter, "runs beautifully". 50-60 / 34.5-35.1 / 35.8-36.0 / 207-208 / 64.6-65.1 / 21.2-21.7 fps (tiny, small, large, static, colour, many).

**Over-wait, fixed in `M8D3DD`:** `hal_small` took ~15 ms of work and sat at 28 ms with a 13-14 ms page wait. The wait always ran to the next blank, even when the one the flip needed had passed while the program ran. The latch is now acknowledged at the write and read at the next Flip. Risk: a write just before the card's pick-up, whose blank flag then follows, would count as taken, showing a frame's erase. M8PAGE places the pick-up between 0.9 of a frame and just after the flag. Watch for black flashes.

**RAM, `M8D3DD`, card, agent idle:** PERFLOG's locked count 3,020 KB (boot 231, `GMEM_FIXED`) to 2,128 KB (boot 232, moveable with `GlobalFix`): 892 KB more for paging. Bed: 3,140 to 2,036 KB. Removing `GlobalSmartPageLock` alone gave 108 KB in the bed, so the fixed allocation was what held it. Effect on paging and on desktop repaint not yet measured. Built `M8DISP.DRV` md5 4263513518f4ed7af787850622650cae, `M8DD32.DLL` md5 69ff48e5f3946a4bcda63e5408ddab1c.

**Not settled:** `M8D3DD` agent-free.

**Bed:** `vm_5160_mcp`, driver loaded and ran with 0 bad frames, lit counts as the card. Too slow to exercise the page wait, so the card is the test. The bed's image backup is `vm_5160_mcp/card.img.pre_v10`.
