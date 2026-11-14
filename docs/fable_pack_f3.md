# Fable pack F3: Mach8 exit paths and register ownership

Assembled 2026-11-14 from repo files. No diagnosis, no proposals.

## 1. Question

F3 (`docs/fable_driver_review_brief.md:24`; Part 1 section A, items 2 and 4, lines 112-116), narrowed
by the owner to two items.

(a) Exit paths. Every path out of a mode switch, Flip, Lock/Unlock, DirectDraw/Direct3D surface
teardown, and driver disable/reset that can leave the Mach8 engine, the feeder (`M8FEED.VXD`) or the
16-bit/32-bit hand-over (`hot[H_BUSY]`) in a state the next caller does not expect: a half-sent
command, FIFO not drained, hold still set, palette or mask registers not restored.

(b) Register ownership. Registers one layer writes and another assumes unchanged (WRT_MASK, RD_MASK,
DP_CONFIG, ALU_FG_FN, FRGD_COLOR, clip registers, SRC_Y_DIR, mono pattern) between the 16-bit display
driver, `m8dd32.c`, `m8d3d.c` and `m8feed`.

Out of scope: speed, cache weight, RAM (F2), stale caches (A1), FIFO accounting (A3).

## 2. Measured facts

Source: all files at master `a5b3e4a`. `m8d3d.c` is modified in the working tree (uncommitted); the
ranges below are the working tree's lines. The card runs `b6936f4`'s build (see F2 pack).

Hangs and unexplained states, each with no cause established:
- `M8CURVE` hang, boot 255, ~18 min up after two `M8D3DF` high runs and `M8IDLE`: agent stopped, ping
  went on, port then refused, later ping stopped. Second hang boot 256, fresh boot, stopped inside the
  first arm; black screen with one white disc. Boot 257 finished. "Two hangs in three runs; cause not
  known, may be intermittent." `docs/captures/2026-11-07_curves/README.md:51-55`.
- Exit hang after Alt+F4 with M8FEED on: GE hung once in 2 runs, black 640x480 area and a white bar;
  GD hung 3 of 3 tunnel runs. "The feed's guard (busy=2, census `cut`) never fired ... the cause in
  the feed at exit is still open. The feed is off on the card." `docs/next_session_2026_11_08.md:39`.
- Exit hang after Alt+F4 "seen again on boot 306" (#55): `docs/next_session_2026_11_08.md:136`;
  `docs/issue_audit_2026_11_14.md:74-75`. #55 itself (16 bpp restart, Sound Blaster looping its DMA
  buffer, also at the DirectX installer): `docs/next_session_2026_11_05.md:58-65`, "none is tied to
  [our driver] by evidence", not reproduced in the bed.
- `M8D3D9` boots 226 and 227 (egg timer black screen after `M8D3DF`; `AGDISK` hang with the agent on):
  `docs/next_session_2026_11_07.md:42`. `M8D3GK` hung once in twist, boot 345, large black window:
  `docs/next_session_2026_11_08.md:30`.
- `docs/worklist.md:360` (X10): "The card's hang needs a cause before a showcase."

Register-survival reliance the code already makes:
- The F2 review, finding 3 (`docs/fable_review_f2.md:233-250`): "The code already relies on all four
  [WRT_MASK, DP_CONFIG, ALU_FG_FN, SRC_Y_DIR] surviving a fill: they are set once for every band fill
  of a colour. Only `RD_MASK` and `FRGD_COLOR` change."
- `m8hw.c:185-192` (comment): DP_CONFIG, FRGD_MIX, BKGD_MIX and PIX_CNTL program one set of sources
  and ALU functions from two views; "a stale one hangs the engine (card)". The 16-bit driver shadows
  twelve registers (`m8_sh_port`, `m8hw.c:195-199`); the 32-bit side keeps none for these.
- `m8dd_forget` (`m8dd32.c:325`) clears the 16-bit shadows through the shared `m8_hot` block after
  the HAL used the engine. `m8d3d.c` keeps its own `m8d3d_mono_on` and `m8d3d_stamp_mode` flags.
- `m8d3d_mono_off` comment (`m8d3d.c:1500`): an engine reset does not clear SRC_CMP_FN.

Hand-over protocol, as the code states it:
- `H_BUSY` (hot byte 12): 1 = a 16-bit screen operation or the DLL has the engine; 2 = the feeder's.
  `M8FEED.VXD` stops its tick (state 3) at any value but 2 (`M8FEED.ASM:268-273`) and at `H_HW_OFF`
  set (state 2, `:263-266`). `m8.h:68-70`: "A mode change must take the engine back before it writes
  a register." `M8_FEED_TAKE` is called at the entry of `Enable`, `Disable`, `ReEnable`, not in
  `m8_switch_out/in`.
- `m8d3d_feed_finish` classifies the stop as lost (state 2 or `hot[11]` = `H_HW_OFF` set), cut
  (state 3 or `hot[12]` = `H_BUSY` != 2) or completed (`m8d3d.c:1091-1092`).

Not sourced:
- Any capture for the `M8CURVE` hang beyond the README; its log file was to be read from the card.
- A boot-306 capture: only the one-line mention.
- Which of the hangs ran with M8FEED loaded (only GE/GD are stated; `[feed] on=0` was the card's
  setting at 11-08, the 11-14 card state is not stated here).
- A list of which of the five 32-bit/16-bit paths set `H_BUSY` back to 0 on every return.

## 3. Reading list

All under `drivers/mach8_display/src/` except the last. Entry and exit and save/restore code only.

| File | Lines | Count | Content |
|---|---|---|---|
| `enable.c` | 278-312 | 35 | `m8_switch_out`, `m8_switch_in` (DOS box takes and returns the screen) |
| `enable.c` | 422-440 | 19 | `Disable` |
| `enable.c` | 526-562 | 37 | `ReEnable` (mode change, failure path) |
| `m8hw.c` | 204-236 | 33 | `m8_sh_clear`, `m8_hw_forget`, `m8_engine_reset` |
| `m8hw.c` | 268-276 | 9 | `m8_plain` |
| `m8hw.c` | 277-291 | 15 | `m8_hw_wait_idle` |
| `m8hw.c` | 328-375 | 48 | `m8_hw_set_mode`, `m8_hw_passthrough` |
| `m8dd32.c` | 325-336 | 12 | `m8dd_forget` |
| `m8dd32.c` | 340-381 | 42 | `m8dd_transfer` (Lock/Unlock 8 bpp transfers), register set-up and write loop |
| `m8dd32.c` | 619-633 | 15 | `m8dd_plain` |
| `m8dd32.c` | 2231-2240 | 10 | `m8dd_pages_leave` |
| `m8dd32.c` | 2357-2391 | 35 | `m8dd_flip16_fast` |
| `m8dd32.c` | 2512-2519 | 8 | `m8dd_flip` (feed stop at entry) |
| `m8dd32.c` | 2598-2619 | 22 | `m8dd_destroy16` |
| `m8dd32.c` | 3441-3453 | 13 | `m8d3d_engine_begin` |
| `m8dd32.c` | 3477-3484 | 8 | `m8d3d_engine_end` |
| `m8dd32.c` | 3566-3629 | 64 | `m8dd_destroy_surface`, `m8dd_destroy_driver` |
| `m8dd32.c` | 3673-3693 | 21 | `m8d3d_feed_lost`, `m8d3d_feed_hand` |
| `m8d3d.c` | 982-1011 | 30 | `m8d3d_owner_check` (process change mid-feed) |
| `m8d3d.c` | 1049-1163 | 115 | `m8d3d_feed_go`, `_finish`, `_stop`, `_close` |
| `m8d3d.c` | 1492-1512 | 21 | `m8d3d_mono_off` |
| `m8d3d.c` | 3318-3334 | 17 | `m8d3d_replay` entry: owner check, feed stop, engine begin, flags |
| `m8d3d.c` | 3436-3453 | 18 | `m8d3d_replay` exit: compare restore, hold, feed_held |
| `m8d3d.c` | 3591-3604 | 14 | `m8d3d_context_destroy` |
| `m8d3d.c` | 3721-3744 | 24 | `m8d3d_lores_set` (320x240 mode set) |
| `m8d3d.c` | 3898-3911 | 14 | `m8d3d_pal_restore` |
| `../../m8feed/src/M8FEED.ASM` | 224-244 | 21 | `M8Feed_Stop` |
| `../../m8feed/src/M8FEED.ASM` | 258-283 | 26 | `M8Feed_Run` entry: state, `H_HW_OFF`, `H_BUSY` gates |
| `../../m8feed/src/M8FEED.ASM` | 323-357 | 35 | `M8Feed_Run` empty and re-arm paths |
| **Total** | | **781** | |

Cut to stay under ~800, and why. Cut: `m8dd_flip16` slow path (`m8dd32.c:2437-2491`, 55 lines: it ends
with `m8d3d_feed_go` and `feed_finish`, so its exit state is the one `flip16_fast` leaves); 8 bpp
`m8dd_lock`/`m8dd_unlock` bodies (497-617: they enter through `m8d3d_feed_stop(0/1)` and leave through
`m8dd_transfer`, which is listed); `m8dd_lock16`/`unlock16` and `m8dd_send` (RAM and an escape to the
16-bit side, no register writes of their own); `m8d3d_pal_load` and `m8d3d_feed_open`; `m8d3d_clear`
(`m8dd32.c:3459-3475`, writes the four registers then `m8dd_rect`); `m8hw.c` FIFO accounting (A3);
`M8Feed_IoCtl` (`:137-221`, start and stop plumbing); the 16-bit `screen.c` entry points that set
`m8_busy` (about 20 sites between 592 and 5337). Follow a pointer into any of these only if a line
read points there.

Layout notes for the reader: the feeder and the DLL share `m8_hot`, a block in the 16-bit driver's
data (`m8.h:55-60`), reached from `m8dd32.c` as `m8dd->hot` and from the VxD as `dwHot`. The DLL's
data is shared by every process (`m8d3d.c:1000-1003`).

## 4. Constraints

- Never propose disabling `INBRDPC.SYS`, dword OUTs to the Mach8, or a clock change.
- Real hardware outranks the 86Box bed: the bed prices an OUT at ~1 us against 7.64 us on the card,
  and has not reproduced #55 or the exit hang; a bed probe shows pixels and state, not timing.
- Output pixels must stay identical (card pixels = ATI in all 35 tests).
- Every IN/OUT flushes the 486 L1; a proposed extra register write costs ~7.64 us on the card. State
  the writes added per path.
- The F2 review proposed dropping four register writes on the premise that they survive a fill
  (`docs/fable_review_f2.md:233`). A finding here that a path breaks that premise bears on that patch.
- Comments are a canary (CLAUDE.md): report a comment that describes another device or a fixed bug.
- Not Fable's to change: `INBRDPC.SYS`, 86Box code, the bed model.

## 5. Prompt for the reviewer

```
Read docs/fable_pack_f3.md and only the source lines it lists in section 3. Nothing else unless a
line you read points there.

Question: Mach8 driver correctness, two items. (a) Exit paths: every path out of a mode switch,
Flip, Lock/Unlock, DirectDraw/Direct3D surface teardown, and driver disable/reset that can leave the
engine, the feeder (m8feed) or the 16-bit/32-bit hand-over (hot busy byte) in a state the next caller
does not expect: a half-sent command, FIFO not drained, hold still set, palette or mask registers not
restored. (b) Register ownership: registers one layer writes and another layer assumes unchanged
(WRT_MASK, RD_MASK, DP_CONFIG, ALU_FG_FN, FRGD_COLOR, clip registers, SRC_Y_DIR, mono pattern)
between the 16-bit display driver, m8dd32.c, m8d3d.c and m8feed. The card has hung in M8CURVE and at
exit after Alt+F4 with the feeder on, with no cause found; look for a path that explains either.

Output contract: At most 5 findings ranked by risk (hang or wrong pixels first), each with file:line,
the path that breaks, evidence, a proposed patch as a diff, and the bed or card probe that would show
it. Do not build, run or commit. When you have 5 findings or have read the pack's lines, write the
report to docs/fable_review_f3.md and stop.
```
