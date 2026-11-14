# Fable review F2: Direct3D frame build

2026-11-14. Input: `docs/fable_pack_f2.md` and the source lines in its section 3. Nothing built,
run or committed. Line numbers are master's (`m8d3d.c` unchanged since `b6936f4`).

Every gain below is an estimate from the pack's cost figures, not a measurement. The pack gives no
triangle, colour, pass or run count for a twist frame, so each estimate states the count it assumes.
The one size hint used: the comment at `m8d3d.c:2566-2569` says the frame's edge array was "the size
of the L1", which puts a frame near 1,000 edges (~330 triangles).

Under the feed with hold on (`m8d3d.c:3332`), the build sends nothing: `m8d3d_q_flush` and
`m8d3d_q_pump` return at once. So the only I/O inside phases 36-41 is the phase clock itself
(finding 2), and the rest of the build's cost is cache lines (findings 1, 4) and the writes it
queues for later (finding 3).

| Rank | Finding | Expected gain | Where |
|---|---|---|---|
| 1 | Edge cancel one colour at a time | ~3 ms (2-4) of phase 37 | `m8d3d.c:2981-3077` |
| 2 | Phase marks read the clock inside the build | ~1.5 ms (1-3) | `m8d3d.c:720`, `:3338` |
| 3 | Four repeated register writes per colour | ~0.03 ms a colour; ~0.9 ms at 32 colours | `m8d3d.c:2779-2791` |
| 4 | Bounds re-checked on every further run | 0 at one run; ~1 ms at 6 runs | `m8d3d.c:2702-2706`, `:3389` |

No fifth finding. Section "Looked at, not proposed" says why, including asm.

---

## 1. Edge cancel one colour at a time

**Where:** `m8d3d.c:2981-3077` (all of phase 37 before the mark at `:3094`).

**Evidence.** Phase 37 is the largest part, 6.5 ms. Its work per edge, in 16-byte lines that are
not in the cache when touched (1.3 us each, pack section 2):

| Step | Lines | Why uncached |
|---|---|---|
| `m8d3d_je` written, `:2988-3003` | 0.6 | 10 bytes an edge, array near L1 size |
| `m8d3d_jhash[h]` probed, `:3016` | 1 | 16 KB table, random slot |
| first copy compared, `:3023` (half the edges) | 0.65 | random earlier edge |
| slot emptied, `:3031` | 0.5-1 | same random slot, a pass later |
| colour count, `:3035-3039` | 0.6 | whole edge array read again |
| compaction, `:3046-3051` | 0.6 | and again |
| cycle moves, `:3052-3062` (outer edges) | ~0.8 | two random edges a swap |

About 5 lines an edge, ~6.5 us; at ~1,000 edges that is the measured 6.5 ms. The table is 16 KB
for at most 3,072 edges, so no probe ever lands on a cached line.

An edge only cancels against an edge of its own colour (`:3011` hashes the colour, `m8d3d_edge_cmp`
compares it). The triangles are put in colour order anyway (`:3063-3077`), after the edges. Doing
that first and making the edges colour by colour means: the hash holds one colour at a time and is
sized to it (a 20-triangle colour uses 128 slots, 256 bytes); a cancelled edge is never stored; the
kept edges come out already in colour order, so the count, the compaction over the whole array and
the cycle moves all go. No new arrays: `m8d3d_jhash` keeps its 8192 entries for the worst case but
only the low part is touched. Lines an edge fall to about 1 (the triangle's corners, read once in
colour order, and the kept edge).

**Pixels.** The same set of outer edges per colour, each with the same bit 8: the first copy is the
lowest-numbered triangle in both, since the count sort into `m8d3d_jtri` is stable. Their order
inside a colour changes. Nothing reads that order for pixels: scratch outlines are XOR (`:3119`),
seam rows are sorted (`:3173`), `t`/`b`/`xl`/`xr` are min/max, and the on-screen outlines are plain
lines in one colour (comment `:3218-3220`; off by default, `shift=1` at `:3476`). Colour order and
the 8-a-pass grouping are unchanged.

**Patch.**

```diff
@@ m8d3d_join, m8d3d.c:2981
-    /* Every edge with its ends in one order, sorted, then pairs within a colour dropped. */
-    for( i = 0; i < n; ++i ) {
-        const short *p = v + 6 * i;
-
-        for( j = 0; j < 3; ++j ) {
-            const short *a = p + 2 * j;
-            const short *b = p + 2 * ((j + 1) % 3);
-            M8D3D_JEDGE  *e = &m8d3d_je[ne++];
-            int         swap = a[1] > b[1] || (a[1] == b[1] && a[0] > b[0]);
-
-            int         dy = b[1] - a[1];
-            long        cr = (long)(b[0] - a[0]) * (p[2 * ((j + 2) % 3) + 1] - a[1]) -
-                             (long)dy * (p[2 * ((j + 2) % 3)] - a[0]);
-
-            /* Bit 8: ... (comment unchanged) */
-            e->col = (short)((index[i] & 0xFF) | ((dy > 0 ? cr > 0 : dy < 0 && cr < 0) ? 0x100 : 0));
-            e->y0 = swap ? b[1] : a[1];
-            e->x0 = swap ? b[0] : a[0];
-            e->y1 = swap ? a[1] : b[1];
-            e->x1 = swap ? a[0] : b[0];
-        }
-    }
-    {
-        int nu = 0;
-
-        for( i = 0; i < ne; ++i ) {
-            ... hash over the whole frame, :3009-3029 ...
-        }
-        for( i = 0; i < nu; ++i ) {
-            m8d3d_jhash[m8d3d_jused[i]] = 0;
-        }
-    }
-    memset( m8d3d_jfrom, 0, sizeof( m8d3d_jfrom ) );
-    ... count by colour, compaction, cycle moves, :3035-3062 ...
     memset( m8d3d_jfrom, 0, sizeof( m8d3d_jfrom ) );
     for( i = 0; i < n; ++i ) {
         ++m8d3d_jfrom[(index[i] & 0xFF) + 1];
     }
     ... :3067-3077 unchanged: m8d3d_jtri filled, m8d3d_jfrom shifted back ...
     m8d3d_jfrom[0] = 0;
+    /* One colour at a time: an edge cancels only within its colour, so the hash holds one
+     * colour's edges and stays in the cache, a cancelled edge is never stored, and the kept
+     * edges land in colour order. Edges met an even number of times are inner. */
+    for( k = 0; k < 256; ++k ) {
+        int         t0 = m8d3d_jfrom[k];
+        int         t1 = m8d3d_jfrom[k + 1];
+        unsigned    mask = 63;
+        int         nu = 0;
+
+        if( t0 == t1 ) {
+            continue;
+        }
+        /* At least twice the colour's edges, so every probe ends at an empty slot. */
+        while( mask + 1 < (unsigned)(t1 - t0) * 6 && mask < M8D3D_JHASH - 1 ) {
+            mask = mask * 2 + 1;
+        }
+        ne = nb;
+        for( i = t0; i < t1; ++i ) {
+            const short *p = v + 6 * m8d3d_jtri[i];
+
+            for( j = 0; j < 3; ++j ) {
+                const short *a = p + 2 * j;
+                const short *b = p + 2 * ((j + 1) % 3);
+                M8D3D_JEDGE *e = &m8d3d_je[ne];
+                int         swap = a[1] > b[1] || (a[1] == b[1] && a[0] > b[0]);
+                int         dy = b[1] - a[1];
+                long        cr = (long)(b[0] - a[0]) * (p[2 * ((j + 2) % 3) + 1] - a[1]) -
+                                 (long)dy * (p[2 * ((j + 2) % 3)] - a[0]);
+                unsigned    h;
+
+                /* Bit 8: ... (comment unchanged) */
+                e->col = (short)(k | ((dy > 0 ? cr > 0 : dy < 0 && cr < 0) ? 0x100 : 0));
+                e->y0 = swap ? b[1] : a[1];
+                e->x0 = swap ? b[0] : a[0];
+                e->y1 = swap ? a[1] : b[1];
+                e->x1 = swap ? a[0] : b[0];
+                h = ((unsigned)e->y0 * 977u + (unsigned)e->x0 * 131u +
+                     (unsigned)e->y1 * 7919u + (unsigned)e->x1 * 59u) & mask;
+                for( ;; h = (h + 1) & mask ) {
+                    int f = m8d3d_jhash[h];
+
+                    if( f == 0 ) {
+                        m8d3d_jhash[h] = (WORD)(ne + 1);
+                        m8d3d_jused[nu++] = (WORD)h;
+                        m8d3d_jodd[ne++] = 1;
+                        break;
+                    }
+                    if( m8d3d_edge_cmp( &m8d3d_je[f - 1], e ) == 0 ) {
+                        m8d3d_jodd[f - 1] ^= 1;
+                        break;
+                    }
+                }
+            }
+        }
+        for( i = 0; i < nu; ++i ) {
+            m8d3d_jhash[m8d3d_jused[i]] = 0;
+        }
+        for( i = nb; i < ne; ++i ) {
+            if( m8d3d_jodd[i] ) {
+                m8d3d_je[nb++] = m8d3d_je[i];
+            }
+        }
+    }
     if( !c->jclean || m8d3d_dclean ) {
```

Bounds: `ne` never passes `3 * n`, and `m8d3d_join_ok` caps `n` at `M8D3D_JOIN_EDGES / 3`. The
comment at `:2566-2569` and the one on `m8d3d_jused` (`:2572`) describe the old passes and need
rewriting with the patch. `ne` initialised to 0 at `:2970` stays; `nb` is the result as before.

**Card probe.** GZ twist census, driver before and after, two runs each with the `idle:` line:
`m8d3d_stat[37]` per flip (6.5 ms now) and flips per second. Pixels: the 35-test compare, plus one
twist frame read back from the card before and after.

**Not checked:** how many edges a twist frame has. If far under 1,000 the gain shrinks in step.

---

## 2. Phase marks read the clock inside the build

**Where:** `m8d3d.c:720-721` (`M8D3D_PH`), start read at `:3338`; marks at `:3094`, `:3189`,
`:3204`, `:3299`, `:3384`, `:3402`, `:3431`, `:3444`, `:3447`.

**Evidence.** `M8D3D_PH` is unconditional and calls `QueryPerformanceCounter` each time. The pack
(section 2, from `docs/next_session_2026_11_08.md:122`) records that QPC empties the cache. With
hold on, these are the only flushes inside the build. Marks a frame: 6 fixed, plus 1 for each
`m8d3d_join` call and 3 for each pass of 8 colours inside it. A 32-colour frame in one run is 19
clock reads; each costs the call plus a refill of whatever the next phase touches again, 84 us at
1 KB live and 321 us at 4 KB (pack section 2). 19 x 84 us = 1.6 ms. The 22.6 ms split was taken
with these marks in, so it carries their cost. Marks 42 and 43 (`:3444`, `:3447`) are back to back
with one assignment between them.

**Patch.**

```diff
@@ m8d3d.c:717
-/* Diagnostic: clock ticks since the last mark, into phase k. */
+/* Diagnostic: clock ticks since the last mark, into phase k. Off unless asked for: each mark is
+ * a clock read, and a clock read empties the cache in the middle of the build. */
 static LARGE_INTEGER    m8d3d_ph;
 static DWORD            m8d3d_ph_send;
-#define M8D3D_PH( k ) do { LARGE_INTEGER t_; QueryPerformanceCounter( &t_ ); m8d3d_stat[k] += t_.LowPart - m8d3d_ph.LowPart; m8d3d_ph = t_; \
-                           m8d3d_stat[(k) + 10] += m8d3d_stat[44] - m8d3d_ph_send; m8d3d_ph_send = m8d3d_stat[44]; } while( 0 )
+static int              m8d3d_dphase;       /* [diag] phase=1: Flip's phases timed */
+#define M8D3D_PH( k ) do { if( m8d3d_dphase ) { LARGE_INTEGER t_; QueryPerformanceCounter( &t_ ); m8d3d_stat[k] += t_.LowPart - m8d3d_ph.LowPart; m8d3d_ph = t_; \
+                           m8d3d_stat[(k) + 10] += m8d3d_stat[44] - m8d3d_ph_send; m8d3d_ph_send = m8d3d_stat[44]; } } while( 0 )
@@ m8d3d_replay, m8d3d.c:3338
-    QueryPerformanceCounter( &m8d3d_ph );
+    if( m8d3d_dphase ) {
+        QueryPerformanceCounter( &m8d3d_ph );
+    }
@@ m8d3d.c:3476 (beside the other [diag] keys)
+    m8d3d_dphase = (int)GetPrivateProfileInt( "diag", "phase", 0, "C:\\M8D3D.INI" );
```

`m8d3d_q_send_to` and `m8d3d_q_send_all` (`:857`, `:871`, `:883`, `:899`) also read the clock
twice a call. They send anyway, so the flush is already paid; left alone.

**Card probe.** No build needed beyond the patch: twist with `phase=1` then `phase=0` in
`C:\M8D3D.INI`, flips per second each, twice, idle proven. The difference is the marks' cost. This
is the cheapest of the four and worth running first: it also says how far to trust the split.

**Not checked:** QPC's own cost on this machine, and passes per twist frame.

---

## 3. Four repeated register writes per colour

**Where:** `m8d3d.c:2779-2791` (`m8d3d_join_fill_begin`), called once per colour at `:3199`.

**Evidence.** Each colour's fill starts with 6 writes. Between one colour's start and the next
(`:3191-3203`) the only writes queued are `m8d3d_join_fill`'s coordinates (`:2803-2811`), so for
the 2nd to 8th colour of a pass `WRT_MASK` 0x00FF, `DP_CONFIG` poly fill, `ALU_FG_FN` replace and
`SRC_Y_DIR` 1 are written with the value they already hold. The code already relies on all four
surviving a fill: they are set once for every band fill of a colour. Only `RD_MASK` and
`FRGD_COLOR` change. Saving: 4 writes x (colours - passes) x 7.64 us; 32 colours in 4 passes is
112 writes, 0.86 ms of bus time a frame and 448 bytes less queue. Same register values at every
command, so the pixels cannot differ.

**Patch.**

```diff
@@ m8d3d.c:2778
-/* The registers every fill of one colour shares. */
-static int m8d3d_join_fill_begin( int plane, BYTE col )
+/* The registers every fill of one colour shares. Four of them every colour of a pass shares:
+ * only the fills' coordinates are written between colours. */
+static int m8d3d_join_fill_begin( int plane, BYTE col, int first )
 {
     if( !m8d3d_q_need( 6 ) ) {
         return( 0 );
     }
-    M8D3D_Q( M8_WRT_MASK, 0x00FF );
+    if( first ) {
+        M8D3D_Q( M8_WRT_MASK, 0x00FF );
+        M8D3D_Q( M8_DP_CONFIG, M8_DP_POLY_FILL );
+        M8D3D_Q( M8_ALU_FG_FN, M8_MIX_REPLACE );
+        M8D3D_Q( M8_SRC_Y_DIR, 1 );
+    }
     M8D3D_Q( M8_RD_MASK, plane );
-    M8D3D_Q( M8_DP_CONFIG, M8_DP_POLY_FILL );
-    M8D3D_Q( M8_ALU_FG_FN, M8_MIX_REPLACE );
     M8D3D_Q( M8_FRGD_COLOR, col );
-    M8D3D_Q( M8_SRC_Y_DIR, 1 );
     return( 1 );
 }
@@ m8d3d.c:3199
-            ok = m8d3d_join_fill_begin( 1 << j, (BYTE)cols[j] ) &&
+            ok = m8d3d_join_fill_begin( 1 << j, (BYTE)cols[j], j == 0 ) &&
```

Smaller, same kind, no diff offered: with outlines off (`shift=1`, the default) the `WRT_MASK`
at `:3209` is overwritten by `m8d3d_join_clear_part`'s at `:2759` with no command between, and
that function's `RD_MASK` and `DP_CONFIG` (`:2760-2761`) repeat `:3208` and `:3210`. Three writes
a pass, ~0.02 ms; `m8d3d_join_clear` at `:3089` needs them, so it wants a flag.

**Card probe.** `m8d3d_stat[25]` (writes sent) per flip before and after gives the write count
saved exactly; flips per second for the time. Pixels: 35-test compare.

**Not checked:** colours per twist frame. The guide was not read; the claim that the four
registers survive a fill rests on the existing code's own use of them.

---

## 4. Bounds re-checked on every further run

**Where:** `m8d3d.c:2702-2706` in `m8d3d_join_ok`; called again per run at `:3389`.

**Evidence.** When a run stops at an overlap, `m8d3d_join_ok` is called for what is left, and its
first loop reads every remaining corner again to test 0..640 / 0..480. The first call already
tested them (all of them when the frame has at most 1,024 triangles). With R runs a frame the
corners are read about R/2 times over. The corner array is 12 bytes a triangle, read at 78 ns a
byte when it is not cached: ~0.94 us a triangle a re-read. 400 triangles in 6 even runs is ~1,000
re-reads, ~0.9 ms; one run is 0. The time lands in the next call's phase 37, not in 36.

A count of triangles already known to be in range keeps the answer the same: a triangle is skipped
only if an earlier call tested it and passed.

**Patch.**

```diff
@@ m8d3d.c:2689
-static int m8d3d_join_ok( M8D3D_CONTEXT *c, const short *v, int n, int least )
+/* `known`: how many of the first triangles an earlier call found on screen. */
+static int m8d3d_join_ok( M8D3D_CONTEXT *c, const short *v, int n, int least, int known )
@@ m8d3d.c:2702
-    for( i = 0; i < n * 6; i += 2 ) {
+    for( i = known * 6; i < n * 6; i += 2 ) {
@@ m8d3d_replay, m8d3d.c:3313
     int             first;
+    int             seen;       /* triangles from the frame's start that m8d3d_join_ok tested */
@@ m8d3d.c:3383
-    first = ok && n != 0 ? m8d3d_join_ok( c, v, n, M8D3D_JOIN_MIN ) : 0;
+    first = ok && n != 0 ? m8d3d_join_ok( c, v, n, M8D3D_JOIN_MIN, 0 ) : 0;
+    seen = n < M8D3D_JOIN_EDGES / 3 ? n : M8D3D_JOIN_EDGES / 3;
@@ m8d3d.c:3389
-            int more = m8d3d_join_ok( c, v + 6 * first, n - first, 1 );
+            int more = m8d3d_join_ok( c, v + 6 * first, n - first, 1, seen - first );
+
+            seen = n - first < M8D3D_JOIN_EDGES / 3 ? n : first + M8D3D_JOIN_EDGES / 3;
```

`seen` is only read when `first != 0`, which means the first call passed its bounds loop; `seen -
first` is never negative because a run never ends past the window that was tested.

**Card probe.** First a count, no timing: add one to a spare `m8d3d_stat` slot per `m8d3d_join`
call and read calls per flip on twist. One call a flip closes this finding with no patch. More than
about three makes the patch worth the build; then `m8d3d_stat[37]` per flip before and after.

**Not checked:** runs per twist frame. This is the least certain of the four.

---

## Looked at, not proposed

- **Asm.** No candidate in phases 36-41. `m8d3d_q_send` is already asm and is bus-bound (7.64 us a
  write against a handful of instructions). The rest of the build is bound by lines fetched, not
  instructions; findings 1 and 4 remove fetches and asm would not. This agrees with the pack's
  record that an asm queue was no faster than C.
- **Sending during the build (burst shaping).** With hold on there are no bursts in the build to
  shape. Sending between passes is the `nohold=1` path, already tried and recorded as slower at
  `m8d3d.c:830-831`. The feeder is outside the pack.
- **Chaining the scratch outlines** (5 writes an edge at `:2744-2748`, the largest write count in
  the joined path) the way the on-screen ones are chained (`:3285-3286`). The scratch lines are XOR
  with the last row left off and always drawn top to bottom; a chain draws some edges upward and
  drops a different pixel. Pixel risk, and the guide was not in the reading list. Worth a guide
  read by whoever takes this further: it is ~3 writes an outer edge.
- **Band fills, phase 39 (5.0 ms).** `m8d3d_join_fill_bands` rescans a colour's triangles once per
  64-row band and `m8d3d_tri_span` divides up to six times per triangle per band. That is
  instruction cost on lines already cached, and the runs it produces decide the fill boxes; no
  change found that is both safe and measured in lines.
- **Cell marks, phase 36.** `m8d3d_cells_mark` recomputes three edge functions per cell row with
  multiplies where adds would do. Instruction cost only; any change to which cells are marked moves
  where runs stop.
- **On-screen outline point hash** (`:3223-3294`): about five random lines an edge in the 16 KB
  table, but off by default (`shift=1`).
- **Single triangles, phase 41 (3.0 ms):** `m8d3d_tri_cached` searches `c->mask[]` and `c->seen[]`
  linearly per triangle. Their sizes are outside the lines read, so no line count could be made.
