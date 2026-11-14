# Fable pack F2: Direct3D frame build

Assembled 2026-11-14 from repo files. No diagnosis, no proposals.

## 1. Question

F2 (`docs/fable_driver_review_brief.md:23`, item 10a at lines 130-134): cut the CPU cost of building
each Direct3D frame in the Mach8 driver. `m8d3d_replay` phases 36-41 take ~22.6 ms CPU of 32.7 ms
per twist frame (ours). Include asm candidates in that path, and burst shaping given the CPU cache
flush. The flush is settled hardware behaviour and not in scope to fix, only to avoid.

## 2. Measured facts

Source: `m8d3d.c` and `m8dd32.c` are unchanged since `b6936f4`, the driver the card runs; line
numbers are master's.

Machine: IBM 5160, 8-bit ISA, Intel Inboard 386/PC, IBM 486BL3 at 2x (CHKCPU 83.5 MHz), 16-bit
Mach8 card. Config: `docs/5160_tuned_config.md`.

Frame breakdown, GZ twist census, card boot 369, 286 flips (`docs/fable_driver_review_brief.md:130-134`;
no raw capture file found):
- Ours 33.9 ms a frame (brief). The task text says 32.7 ms; `docs/next_session_2026_11_12.md:18-22`
  says "21 -> 34 ms". Treat as ~33 ms.
- CPU in phases 36-41: ~22.6 ms, card idle meanwhile. Split: join check 3.1, seam prep 6.5,
  joined path 4.1, band fills 5.0, clears 0.9, single triangles 3.0 (ms).
- Own synchronous sending: 8.0 ms. ~3,640 writes a frame.
- Phase counters: `m8d3d.c:715-722` (`m8d3d_stat[36..43]`, send part in `[46..53]`). Marks:
  36 `m8d3d.c:3384`, 37 `:3094`, 38 `:3189`, 39 `:3204`, 40 `:3299` and `:3402`, 41 `:3431`.
  Mapping of labels to phase numbers is inferred from the marker positions, not stated in a doc.
- twist 16 bpp: every feed stop is at Flip; Flip has ~23 writes left to send; not a cost
  (`docs/fable_driver_review_brief.md:28-30`). Feed: ~2,660 of ~2,685 writes a frame sent in the
  background (`docs/next_session_2026_11_12.md:18-22`).

Flush and bus costs (`docs/captures/2026-11-12_flushprobe/README.md`; summary in
`docs/fable_pack_f1_flush.md` section 2):
- IN/OUT invalidates the whole L1 once per burst; refill 78 ns per byte of working set, linear to
  8 KB (256 B / 1 KB / 4 KB / 8 KB: 25 / 84 / 321 / 638 us).
- Refill does not grow with burst length: 4 KB re-read 470 us after 1 OUT and after 1000 OUTs
  (README:78-80). Each extra IN in a burst adds 5.6 us.
- ISA memory write: posted, no flush. Dword memory write 1.83 us/B, word 1.94, byte 2.15
  (README:71). ISA memory read 3.48-3.72 us/B (README:72).
- Port output: byte 5.695, word 3.819, dword 3.183 us/B (`docs/iowidth_measured_2026_09_20.md:30`).
  Word OUT to a Mach8 register 7.64 us (`docs/bed_io_cost_model_2026_11_12.md:37`). PIX_TRANS is
  word OUT, 3.82 us/B. Dword OUTs to the Mach8 are barred.
- A 16-byte line touched after a flush ~1.3 us (`docs/atim8_vs_ours_lagging_tests.md:8`).
- A status read costs what a write does (comment at `m8dd32.c:3390`).
- QPC and any status read empty the cache, so diagnostics cost time
  (`docs/next_session_2026_11_08.md:122`).
- A 2 KB table scanned on every call lost up to 33 % (`docs/next_session_2026_11_12.md:88`).
- Engine, `docs/captures/2026-11-07_shape/README.md:24-25`: blit-type or mask fill ~69 ns/px of its
  box, plain fill ~12.6 ns/px; each command ~40-60 us whatever is sent; an asm queue was no faster
  than C (same text at `docs/mach8_frame_rate_plan.md:544-545`).
- Source comment inside `m8d3d_replay` (erase merge): plain fill ~15 ns/px; a box is 5 writes of ~7.6 us.
- Joined same-colour patches cut writes ~18.6k -> ~2k (`docs/silicon_for_d3d_2026_11_09.md:7`).
- Fixed RAM: DGROUP and `_TEXT` near 64 KB (`docs/fable_driver_review_brief.md`, item 8).

Existing asm in the path: `m8d3d_q_send` (inline `#pragma aux`, `m8d3d.c:838-847`, one OUT word per
queued DWORD), `m8d3d_outb/outw/inb` (`m8d3d.c:3695-3700`). Standalone asm exists elsewhere
(`drivers/mach8_display/src/spans.asm`, `hash.asm`, `blt.asm`) but none is called from phases 36-41.

Not sourced: a raw capture file for the 22.6 ms split (only the brief carries it); the ms per
function inside phases 37-39.

## 3. Reading list

All in `drivers/mach8_display/src/`. What each range is and where it sits in the phases.

| File | Lines | Count | Content |
|---|---|---|---|
| `m8d3d.c` | 715-722 | 8 | stat array, phase macro `M8D3D_PH` (a QPC read per mark) |
| `m8d3d.c` | 813-935 | 123 | write queue: `M8D3D_Q`, `m8d3d_q_send` asm, send_to/all, flush, pump, need |
| `m8d3d.c` | 1165-1192 | 28 | `m8d3d_q_rect`, `m8d3d_q_clear` |
| `m8d3d.c` | 1407-1417 | 11 | `m8d3d_scratch_clear` (phase 40/42 clears) |
| `m8d3d.c` | 1488-1512 | 25 | `m8d3d_mono_off` |
| `m8d3d.c` | 1867-1950 | 84 | `m8d3d_tri_cached`, single-triangle path (phase 41) |
| `m8d3d.c` | 2055-2277 | 223 | edges, `m8d3d_tri_bands` (phase 41, shaded singles) |
| `m8d3d.c` | 2541-3303 | 763 | join: types, hash, sort, `join_ok`, fills, `m8d3d_join` (phases 36-40) |
| `m8d3d.c` | 3305-3452 | 148 | `m8d3d_replay` |
| `m8dd32.c` | 3394-3452 | 59 | `m8d3d_room`, `m8d3d_room_take`, `m8d3d_engine_begin` |
| **Total** | | **1472** | |

Cut to stay under 1,500, and why: `m8d3d_plane_make` (1951-2054) and the stamp path
(`m8d3d_tri_stamped`, stamp helpers, 2278-2426) are called only from the single-triangle phase 41
(3.0 ms, 13 % of the build); `tri_bands` and `edges` stay because they are the first call for
shaded singles. Phase 40's first mark is inside `m8d3d_join` (3299), so join is read whole. Feeder
(`m8d3d_feed_*`, ~1048) is out: feed stops are settled. Follow a pointer out of these lines only if
a line read points there.

Layout notes for the reader: the join arrays are file-scope statics (`m8d3d.c:2562-2575`, edges 3072
entries, hash 8192 WORDs). Queue is `m8d3d_q[]` in RAM, 8192 DWORDs max, sent in chunks of 8
(`M8D3D_Q_CHUNK`); `m8d3d_q_pump` is called from `m8d3d_q_need` during the build.

## 4. Constraints

- The flush is not in scope to fix; only to avoid.
- Never propose dword OUTs to the Mach8, raising the CPU clock, or disabling INBRDPC.SYS.
- Real hardware outranks the 86Box bed: the bed prices I/O at ~1 us per OUT against 7.64 us on the
  card (`docs/bed_io_cost_model_2026_11_12.md`), so a bed number does not prove a CPU-vs-port trade.
- Low fixed RAM is a goal in its own right; do not trade RAM for a gain under ~1 %.
- Output pixels must stay identical (card pixels = ATI in all 35 tests).

## 5. Prompt for the reviewer

```
Read docs/fable_pack_f2.md and only the source lines it lists in section 3. Nothing else unless a
line you read points there.

Question: cut the CPU cost of building each Direct3D frame in the Mach8 driver. m8d3d_replay
phases 36-41 take ~22.6 ms CPU of ~33 ms per twist frame; the card is idle meanwhile. Consider
asm candidates in that path, and burst shaping: every I/O instruction invalidates the 486's L1
once per burst (refill ~78 ns per byte of working set), a port word write costs ~7.6 us, an ISA
memory write 1.83 us/B with no flush, PIX_TRANS 3.82 us/B. The flush is settled and not yours to
fix, only to avoid. Measure work in cache lines touched and port writes per frame, not in
instructions.

Output contract: At most 5 findings ranked by expected gain, each with file:line, evidence, a
proposed patch as a diff, and the card probe that would prove it. Do not build, run or commit.
When you have 5 findings or have read the pack's lines, write the report to docs/fable_review_f2.md
and stop.
```
