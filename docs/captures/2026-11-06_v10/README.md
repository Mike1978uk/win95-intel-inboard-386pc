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

**Not settled:** V10 agent-free. Whether the stutter on `hal_tiny` is the first test after the mode set paging in (it has the largest clear time, 13.5-15.5 ms, which V16 attributed to other threads), or frames alternating between one and two refreshes at 72 Hz.

**Bed:** `vm_5160_mcp`, driver loaded and ran with 0 bad frames, lit counts as the card. Too slow to exercise the page wait, so the card is the test. The bed's image backup is `vm_5160_mcp/card.img.pre_v10`.
