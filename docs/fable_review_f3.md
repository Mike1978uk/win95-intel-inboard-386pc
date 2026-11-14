# Fable review F3: Mach8 exit paths and register ownership

2026-11-14. Pack: `docs/fable_pack_f3.md`. Nothing built, run or committed.

Read: the pack's section 3 lines, plus the lines they point to (`m8hw.c:74-183`, `478-517`,
`1404-1502`; `m8dd32.c:312-322`, `381-403`, `520-543`, `588-608`; `m8d3d.c:837-960`, `3453-3456`;
`screen.c:587-644`; `m8.h:47-76`; `M8FEED.ASM:85`, `100-221`).

The review worktree was at `2c0f5a0`, which has no pack. The pack and `m8d3d.c` were read from
`7e56c1e`; `m8d3d.c` line numbers are that commit's. The other files are the same in both.

## Summary

| # | Risk | Where | One line |
|---|---|---|---|
| 1 | Hang (card), exit with the feed on | `m8hw.c:337-360`, `enable.c:322,346`, `m8.h:70` | The mode set writes over the feed's FIFO without a wait |
| 2 | Hang-like, any Direct3D or Lock path | `m8dd32.c:312-322` | The DLL's idle wait never resets a wedged engine |
| 3 | Wrong pixels, then finding 1's stale state | `m8d3d.c:1105-1113`, `1494-1498`; `screen.c:592` | A cut feed leaves SRC_CMP_FN on and a command running |
| 4 | Fault at teardown (not confirmed) | `m8d3d.c:1083-1094`, `1156-1161` | Teardown entries use the feed handle and queue without the owner check |
| 5 | Wrong pixels after a DOS box | `m8d3d.c:1091-1092` | A DOS-box round trip is counted cut, not lost |

`M8CURVE`: no path found in the lines read. See the last section.

The F2 premise (WRT_MASK, DP_CONFIG, ALU_FG_FN, SRC_Y_DIR survive a fill inside one frame) holds in
these lines. The 16-bit driver cannot write between `m8d3d_engine_begin` and the hand-over (busy is
1), and after the hand-over a cut drops the rest of the frame. Patch 2 below resets only at an idle
wait, at frame start or end, never between two fills.

---

## 1. Mode set over the feed's FIFO

**Where.** `enable.c:322` and `:346` (`Enable`), `:532` (`ReEnable`), `m8.h:70`, `m8hw.c:328-366`.

**Path.** Alt+F4 in a Direct3D program with the feed on. The last Flip handed up to 4,000 writes
(`feed_tail`, `m8d3d.c:1037`) to `M8FEED.VXD`. DirectDraw restores the desktop mode: `ReEnable` ->
`M8_FEED_TAKE` -> `Enable` -> `m8_hw_set_mode`.

**Evidence.**
- `M8_FEED_TAKE` only changes the byte: `{ if( m8_hot.busy == 2 ) m8_hot.busy = 0; }`. It stops the
  next tick. It does not wait for what the last tick sent. A tick fills the FIFO to 15 entries
  (`M8FEED.ASM:282-311`), and the engine may be in a fill of several ms.
- `enable.c` has no `m8_hw_wait_idle` between line 322 and line 346.
- `m8_hw_set_mode` starts with `m8_sh_clear()` and port writes. Line 360 sends the 13 entries of
  `m8_engine_init` through `m8_write_regs`, which has no FIFO check (`m8hw.c:95-106`).
- The driver's own rules: "A write with all 16 entries full overruns the FIFO and locks it
  (INVALID_IO, guide 8-14)" (`m8hw.c:131-133`); "Routines that write without counting start with
  `m8_wait_fifo_empty`" (`:111-113`); a DP_CONFIG rewrite "halts a command still running (guide
  9-41)" (`:217-219`). The mode set breaks all three when the feed was running.
- A locked FIFO drops GE_PITCH, GE_OFFSET, the scissors and DP_CONFIG. `m8_hw_fill` at line 365 then
  runs on the feed's last state, in the new mode. This fits the capture: the larger mode is up, the
  old 640x480 area is black, one white bar, then nothing.
- The bed runs each command at once, so its FIFO is empty at `ReEnable`. This fits "not reproduced
  in the bed".
- `m8_switch_in` (`enable.c:302`) has the same gap after a DOS program used the 8514 side.

Not shown: that an overrun leaves a state `m8_fifo_need`'s reset (0.4 s) does not recover. The
probe below settles that.

**Patch.** Two port reads per mode set when idle. No write added. Nothing per frame.

```diff
--- a/drivers/mach8_display/src/m8hw.c
+++ b/drivers/mach8_display/src/m8hw.c
@@ int m8_hw_set_mode( int mode )
     m = &m8_modes[mode];
+    /* Another writer's commands may still be queued (the feed's, a DOS
+     * program's). The set-up below is not counted against the FIFO, and an
+     * overrun locks it (guide 8-14). */
+    m8_hw_wait_idle();
     m8_sh_clear();
```

**Probe (card).** In `m8_hw_set_mode`, store `inpw( M8_EXT_FIFO_STATUS )` and `inpw( M8_GP_STAT )`
at entry in two `m8_stats` slots. Feed on, GD tunnel, Alt+F4, three runs. A non-zero FIFO word or
GE_BUSY at the `ReEnable` after Alt+F4 shows the path. The bed will read zero.

---

## 2. The DLL's idle wait never resets

**Where.** `m8dd32.c:312-322` (`m8dd_idle`); callers `:361`, `:381`, `:622`, `:3450`, `:3481`.

**Path.** The engine wedges once (finding 1, a locked FIFO, a transfer left unfinished). From then
every DLL entry polls and gives up. Nothing recovers it until the 16-bit driver happens to wait.

**Evidence.**
- `m8dd_idle` returns 0 after `M8_WAIT_LIMIT` polls. The 16-bit twin resets:
  `m8_hw_wait_idle` -> `m8_engine_reset` (`m8hw.c:288`).
- Each failed wait is 60,000 polls of two port reads: about 0.9 s on the card at 7.64 us, with the
  Win16 lock held.
- `m8d3d_engine_begin` fails (`:3450`), so `m8d3d_replay` returns 0 and the frame is dropped
  (`m8d3d.c:3325`). A full-screen program makes no GDI call, so no 16-bit reset comes. Each frame
  costs 0.9 s and draws nothing: a black screen, user mode starved, ring 0 alive. That matches "agent
  stopped, ping went on" and the black windows of `M8D3D9` and `M8D3GK`.
- `m8d3d_engine_end` ignores the result (`:3481`): on failure it forgets the shadows and clears
  busy with the registers not restored.
- `m8dd_transfer`'s read leaves HOST_READ pending when DATA_READY never comes (`:384-396`). The
  16-bit twin resets there (`m8hw.c:1428-1430`).
- The lost frame's queue is dropped on a failed room wait (`m8d3d.c:864-866`, `1132-1134`), with
  the compare-off write in it (finding 3).

**Patch.** No write on the normal path. On a wedge: 2 + 14 writes, once.

```diff
--- a/drivers/mach8_display/src/m8dd32.c
+++ b/drivers/mach8_display/src/m8dd32.c
@@
+static void m8dd_forget( BYTE *hot );
+
+/* m8hw.c's m8_engine_init, then the compare, which a reset does not clear. */
+static const WORD m8dd_engine_init[][2] = {
+    { 0x76EE, 0x0080 }, { 0x6EEE, 0x0000 }, { 0x72EE, 0x0000 }, { 0xB2E8, 0x0000 },
+    { M8_WRT_MASK, 0x00FF }, { M8_RD_MASK, 0x00FF }, { 0xB6EE, 0x0002 },
+    { M8_EXT_SCISSOR_L, M8_EXT_SCISSOR_MIN }, { M8_EXT_SCISSOR_T, M8_EXT_SCISSOR_MIN },
+    { M8_EXT_SCISSOR_R, M8_EXT_SCISSOR_MAX }, { M8_EXT_SCISSOR_B, M8_EXT_SCISSOR_MAX },
+    { M8_DP_CONFIG, 0x2211 }, { M8_MULTIFUNC_CNTL, M8_MF_PIX_CNTL }, { M8_SRC_CMP_FN, 0 },
+};
+
 static int m8dd_idle( void )
 {
     unsigned n;
+    int      i;
 
     for( n = 0; n < M8_WAIT_LIMIT; ++n ) {
         if( inpw( M8_EXT_FIFO_STATUS ) == 0 && !(inpw( M8_GP_STAT ) & M8_GP_BUSY) ) {
             return( 1 );
         }
     }
+    /* Wedged: a transfer left unfinished, or a locked FIFO, which ignores every write until the
+     * engine is reset (guide 8-14). The caller drops its work; the next wait succeeds. */
+    outpw( M8_SUBSYS_CNTL, M8_SUBSYS_RESET );
+    outpw( M8_SUBSYS_CNTL, M8_SUBSYS_RUN );
+    for( i = 0; i < sizeof( m8dd_engine_init ) / sizeof( m8dd_engine_init[0] ); ++i ) {
+        outpw( m8dd_engine_init[i][0], m8dd_engine_init[i][1] );
+    }
+    m8dd_forget( (BYTE *)m8dd->hot );
     return( 0 );
 }
```

To check before it is applied: `M8_SUBSYS_CNTL`, `M8_SUBSYS_RESET` and `M8_SUBSYS_RUN` are defined in
`m8.h`; `m8dd32.c` keeps its own port defines (`:234`) and may need them added. A free
`m8d3d_stat` slot counting the resets would make the card runs readable.

**Probe (bed, state only).** From a probe program send HOST_READ for a rectangle and read no data,
then start `M8D3DF`. Today `m8d3d_stat[11]` rises once per frame, for good, and the screen stays
black. With the patch, one frame is dropped and the run goes on.

---

## 3. A cut feed leaves the compare on and a command running

**Where.** `m8d3d.c:1105-1113` (cut), `:1132-1134`, `:1494-1498` (`m8d3d_mono_off`), `:3443-3446`;
`screen.c:587-592` (`m8_enter`); `m8dd32.c:3687-3693` (`m8d3d_feed_hand`); `m8hw.c:76-90`.

**Path.** Flip hands the tail of the frame to the feed. A GDI call or a pointer move arrives:
`m8_enter` sets busy to 1 and draws at once. The feed stops at its next tick. The DLL's next entry
counts a cut and drops the rest of the queue.

**Evidence.**
- SRC_CMP_FN has no owner across the layers. `m8d3d_replay` turns it off with a queued write at the
  very end of the frame (`:3445`), so that write is in the feed's tail. The cut branch sets
  `m8d3d_qn = 0` and calls `m8d3d_engine_end`, whose `m8dd_plain` (`m8dd32.c:625-633`) does not
  write SRC_CMP_FN.
- The 16-bit driver assumes it is 0: it writes 0x0028 and 0 only around its own keyed copies
  (`m8hw.c:1927/1939`, `1991/2002`), keeps no shadow of it (`:195-199`), and `m8_engine_init` does
  not hold it. The DLL's own comments state the result: "left on, it would key the next copy"
  (`:1499`), "GDI's copies would be keyed" (`:3442`).
- So the GDI call that causes the cut, and every one after it, copies with the compare on, until
  the next Direct3D frame ends whole. After Alt+F4 there is no next frame. The mode set does not
  clear it either (finding 1's path, "An engine reset does not clear this").
- `m8d3d_mono_off` clears `m8d3d_stamp_mode` before `m8d3d_q_need( 2 )` (`:1495-1496`). When the
  need fails, the guard at `:3443` sees 0 and queues nothing: the one failure the guard is for.
- `m8d3d_feed_hand` forgets the 16-bit shadows while the engine is about to be busy. The first GDI
  call after it finds `m8_dp_ok` 0 and rewrites DP_CONFIG while the feed's command may still run.
  `m8hw.c:217-219` says the forget must be "from an idle engine" for this reason (guide 9-41).
  `m8_enter` has no idle wait.
- `m8d3d_q_add` drops a write without a word when the queue is full (`:1044`). `m8dd32.c:1445` and
  `:1456` queue the compare on and off through it; an off dropped there has the same result. Not
  read further.

**Patch.** Normal path: one byte compare in `m8_enter`, on the cache line it already writes; no port
access. A cut: two reads or more and one write. Mode set and reset: one more write.

```diff
--- a/drivers/mach8_display/src/m8hw.c
+++ b/drivers/mach8_display/src/m8hw.c
@@ static const M8_REG m8_engine_init[] = {
     { 0xCEEE, 0x2211, 0 },      /* DP_CONFIG */
     { M8_MULTIFUNC_CNTL, M8_MF_PIX_CNTL, 0 },
+    { M8_SRC_CMP_FN, 0x0000, 0 },   /* a reset keeps it; no routine expects it on */
 };
@@
+/* The feed had the engine: its last command may still run, and a DP_CONFIG
+ * write would halt it (guide 9-41); its compare may be on, and would key
+ * this driver's copies. */
+void m8_hw_feed_cut( void )
+{
+    m8_hw_wait_idle();
+    outpw( M8_SRC_CMP_FN, 0 );
+}
--- a/drivers/mach8_display/src/screen.c
+++ b/drivers/mach8_display/src/screen.c
@@ static int m8_enter( const M8_RECT *r )
 #endif
+    if( m8_busy == 2 ) {
+        m8_busy = 1;
+        m8_hw_feed_cut();
+    }
     m8_busy = 1;
--- a/drivers/mach8_display/src/m8d3d.c
+++ b/drivers/mach8_display/src/m8d3d.c
@@ static int m8d3d_mono_off( void )
     if( m8d3d_stamp_mode ) {
-        m8d3d_stamp_mode = 0;
         if( !m8d3d_q_need( 2 ) ) {
             return( 0 );
         }
+        m8d3d_stamp_mode = 0;
```

`m8_engine_init` becomes 14 entries, under the FIFO's 15 after a reset. `screen.c:640` and `:2698`
set busy to 1 without `m8_enter`; `:640` is followed by `m8_hw_passthrough`, which waits. `:2698`
was not read.

**Probe (bed, pixels).** Feed on, a frame with stamps (the twist's text overlays), mouse moved while
it runs until `m8d3d_stat[60]` (cut) rises. Then a screen-to-screen copy in GDI (drag a window over
a patterned background): pixels of the key colour are missing from the copy. With the patch the
copy is whole. On the card the same run also shows the halted command, if it shows at all.

---

## 4. Teardown uses the feed without the owner check

**Where.** `m8d3d.c:1083-1094` (`m8d3d_feed_finish`), `:1156-1161` (`m8d3d_feed_close`); entries
`m8dd32.c:2514`, `:3571`, `:3620`, `m8d3d.c:3596`. The check: `m8d3d.c:982-1011`, called only from
`m8d3d_replay` (`:3323`).

**Path.** The program that opened the feed ends without releasing its surfaces (killed, or it
exits in a message handler). Its handle closes: `M8FEED.VXD` unloads and frees the queue
(`M8FEED.ASM:85`, `_PageFree`). DirectDraw's cleanup for the dead process then calls
DestroySurface, ContextDestroy and DestroyDriver from another process.

**Evidence.**
- The DLL's data is shared by every process, so `m8d3d_feeding` or `m8d3d_feed_held` is still 1.
  `m8d3d_feed_h` is a handle of the dead process. `m8d3d_feed` points at freed pages.
- `m8d3d_feed_finish` calls `DeviceIoControl` on that handle (`:1090`), then reads and writes
  `m8d3d_feed[M8FEED_STATE]` (`:1091-1094`): a page fault inside a HAL call, the mode not yet
  restored.
- If the pages are still mapped, the call fails, `head` stays 0, and the loop at `:1124-1137` sends
  the whole queue again from entry 0 over whatever state the feed left.
- `m8d3d_feed_close` then calls `CloseHandle` on a value that belongs to another process (`:1158`).
- `m8d3d_owner_check` was written for this case ("The last owner may have ended mid-feed") and
  guards the queue with `IsBadWritePtr`; the teardown entries do not pass through it.
- `m8d3d_owner_check` also leaves busy at 1 when the owner died between `m8d3d_replay` and
  `m8d3d_feed_go` (`m8d3d_feed_held` set, state not 1): it clears the flag without
  `m8d3d_engine_end`.

Not shown: that any teardown call reaches the DLL from a second process on this system. A program
that exits through `ExitProcess` is cleaned up in its own process, and this path does not run. It
does not explain a hang where the cut census stayed silent unless the probe shows a foreign caller.

**Patch.** No port access added.

```diff
--- a/drivers/mach8_display/src/m8d3d.c
+++ b/drivers/mach8_display/src/m8d3d.c
@@ static void m8d3d_owner_check( void )
         m8d3d_room_forget();
         m8d3d_engine_end();
+    } else if( m8d3d_feed_held ) {
+        /* The owner ended between its frame and the hand-over: busy is still 1. */
+        m8d3d_room_forget();
+        m8d3d_engine_end();
     }
@@ void m8d3d_feed_finish( void )
     int     cut;
 
+    /* The handle and the queue are the owner's: a call from another process must not use them. */
+    m8d3d_owner_check();
     if( m8d3d_feed_held ) {
```

`m8d3d_replay` then checks twice (`:3323`, then through `m8d3d_feed_stop`); the first call can go.

**Probe (card or bed).** `[diag] trace=1`: add `GetCurrentProcessId()` and `m8d3d_owner` to the
"destroy caps" line (`m8dd32.c:3578`) and one such line in `m8d3d_context_destroy` and
`m8dd_destroy_driver`. Run GD, Alt+F4. `M8TRACE.TXT` is written through, so a hung run keeps its
last line. Two different ids on one line show the path.

---

## 5. A DOS-box round trip is counted cut, not lost

**Where.** `m8d3d.c:1091-1092`, `:1105-1120`; `enable.c:296-311`; `m8dd32.c:3673-3682`.

**Path.** A frame is handed to the feed. A full-screen DOS box takes the screen and gives it back
before the DLL is entered again.

**Evidence.**
- `lost` needs the queue's state to be 2 or `H_HW_OFF` still set when `m8d3d_feed_finish` runs. The
  state is 2 only if a tick ran while the screen was away. A feed that had drained (state 0) or
  was stopped by `m8_cur_suspend`'s busy = 1 (state 3) is not lost, and `m8_switch_in` has cleared
  `H_HW_OFF` (`enable.c:301`).
- The branch taken is `cut`. It does not do what `m8d3d_feed_lost` does: `m8dd_card.known`,
  `m8dd_pages_on` and `m8dd_page_pending` stay set.
- `m8_switch_in` set the mode again and cleared the card. `m8dd_flip16_fast` then compares the
  next frame against `m8dd_card_v` (`m8dd32.c:2372-2375`), finds "the frame is there already" for a
  still scene, and draws nothing. With pages on, the CRT offset the DLL believes in is gone.
- `ReEnable` tells the DLL (`m8dd_mode_changed`, `enable.c:551`); `m8_switch_in` does not.

**Patch.** No port access added. `m8dd_mode_changed` was not read: confirm it drops the DLL's card
description and page state, and no more, before this is applied.

```diff
--- a/drivers/mach8_display/src/enable.c
+++ b/drivers/mach8_display/src/enable.c
@@ void __cdecl m8_switch_in( void )
     m8_pdevice->deFlags &= ~BUSY;
     if( m8_fb16 ) {
         m8_fb16_send_all();
+        m8dd_mode_changed();
         return;
     }
```

**Probe (bed, pixels).** A Direct3D program on the fast path with a still scene; Alt+Enter into a
full-screen DOS box and back. The triangles do not return until the scene changes;
`m8d3d_stat[60]` (cut) rises by one where the lost path should have run.

---

## M8CURVE

No path in the lines read explains it. Both waits on the 16-bit side are bounded and reset
(`m8_fifo_need`, `m8_hw_wait_idle`). Every `m8_plain` caller read (`m8hw.c:1410-1412`,
`1449-1451`, `1495-1497`) drops `m8_dp_ok` first through `m8_wait_fifo_empty`, so the group cache is
not left stale there. The second hang was on a fresh boot inside the first arm, so no Direct3D
state is needed for it. The curve and one-pixel paths (`m8hw.c` about 1790-2060, `m8_room_for( 7 )`
to `( 15 )`) and the interrupt-time pointer (`screen.c:1503`, `1678`) are not in the pack; they are
where to read next.

One thing to check there: a reset inside `m8_room_for` returns to a caller that goes on with its
command. `m8_engine_init` restores DP_CONFIG and PIX_CNTL but not FRGD_MIX, and the caller's
`m8_put( M8_SH_FRGD_MIX, ... )` came before the reset (`m8hw.c:1413-1414`, `1452-1453`,
`1499-1500`).

## Comments

None in the lines read describes another device or a fixed bug. Two disagree with the code beside
them:
- `m8hw.c:217-219` says shadows are forgotten "from an idle engine". `m8d3d_feed_hand`
  (`m8dd32.c:3691`) forgets them as the engine is handed over busy (finding 3).
- `m8d3d.c:3442`, "Whatever failed, the compare must not outlive this", is not true of the failure
  in `m8d3d_mono_off`, nor of a cut (finding 3).
