# LS-120 write boundary — measured 2026-09-14

**A write burst to this drive is intact for the first 5120 bytes and silently garbage
beyond it. Split into separate operations of 5120 bytes or less it works indefinitely,
with no cumulative degradation.**

Measured on the real 5160 over COMrade, at the Win95 DOS prompt, against the NOS disk
formatted by the vendor DOS driver. Every source file was streamed to `C:\T\` with an
end-to-end CRC-verified transfer, copied to `D:` with plain DOS `COPY`, and read back and
compared host-side.

⚠ **This measures the VENDOR driver's write path**, not ours. `SD120PPD.SYS` was loaded
throughout (technique 110 — the control was not removed). The boundary is a property of
the transport we share, but it has **not** been shown that our own driver hits the same
wall.

## The runs

| # | what | bytes | result |
|---|---|---|---|
| 1 | `S4K.BIN` | 4096 | **clean** |
| 2 | `S5120.BIN` | 5120 | **clean — exactly at the boundary** |
| 3 | `AP4.BIN`, one open built from two source files | 4096 | **clean** |
| 4 | `S8K.BIN` | 8192 | good to 5119, garbage from 5120 |
| 5 | `S8K.BIN` deleted and rewritten | 8192 | **identical CRC** to run 4 |
| 6 | `S16K.BIN` | 16384 | good to 5119, same corruption |
| 7 | `S16B.BIN` — same content, different clusters | 16384 | **identical sha256** to run 6 |
| 8 | `R16K.BIN` — 16 KB of random bytes | 16384 | good to 5119 |
| 9 | `A1`–`A4`, four separate files | 4 x 4096 | **all four byte-perfect** |

Reference CRC32s (source = destination when clean): `S4K` `ff420df3`, `S5120` `d236779d`,
`AP4` `eb775275`, `A1`-`A4` `ff420df3`.

## What each run rules out

- **Not a timeout or a race.** Runs 4 and 5 produced byte-identical corruption from two
  independent writes. Deterministic.
- **Not medium position.** Run 7 wrote the same content to different clusters and got the
  identical sha256, corruption included.
- **Not content.** Run 8 used random bytes and broke at the same 5120.
- **Not "only the first write after an open succeeds".** Run 3 is one file open fed from
  two sources — at least two write calls — and it is clean. So the limit is a cumulative
  byte count per burst, not a one-shot.
- **Not cumulative across operations.** Run 9 put the same 16 KB down as four separate
  4 KB files and every one is byte-perfect. The drive and the medium are fine.

## It is not a truncation, and the earlier reading needs amending

`docs/next_session_2026_09_14.md` records the fault as silent **truncation** — bytes past
the stop point never written, reading back as zeros. That was correct for the run it
described: `D:\FC.EXE` on a freshly formatted disk stops at 6144 and is zero-filled after,
and it still reads that way today.

What runs 4-8 show is different. Past 5120 the write does **not** stop; it lays down
garbage. The byte histogram of the corrupt tail is dominated by `0Fh` and `F0h`, then
`B0h`, `F4h`, `B4h` — one nibble carrying data and the other clear, which is the shape of
parallel-port status content rather than of our payload.

So: **on a zero-filled disk the two are indistinguishable**, and the earlier disk was
zero-filled. One mechanism, two appearances — the same lesson the 09-13 session recorded
about the *previous* disk, one layer down.

## The stop point is not a constant

| observation | stop |
|---|---|
| `FC.EXE`, earlier session, fresh disk | 6144 |
| every run today | 5120 |

Both are multiples of 1024; they are not the same number. **Do not hardcode 5120.** This
is the evidence for `DESIGN.md` I6 — calibrate at init and take the largest size that
survives on whatever machine the driver loaded on.

## Consequences for our driver

1. **`LS_MAX_XFER = 4096` is under today's boundary**, with two sectors of margin, and is
   measured-safe rather than merely plausible. It was chosen as "8 sectors"; it is now
   backed by a number.
2. **Calibrate, do not pin.** The boundary moved between sessions, so a constant that is
   right here is not right anywhere. `DESIGN.md` I6 and change 6.
3. **Verification is not optional.** Every failing run above reported `1 file(s) copied`.
   DOS was told the write succeeded in all nine cases. `DESIGN.md` section 7.
4. **Our per-SRB connect/disconnect is the shape that works.** `LS_ReadyPoll` connects and
   disconnects around each poll, so each SRB is a fresh bridge session — structurally the
   same as run 9, which is the configuration that stayed clean. That was tidiness before;
   it is now evidence-backed.

## Open

- **Does our driver hit the same 5120 wall?** Not tested. The vendor driver owned the port
  for all nine runs.
- **Why 5120?** Ten sectors. Unexplained. A FIFO depth, a driver buffer, or a bridge
  limit — nothing here distinguishes them.
- **`/di` and `/w0` are still untried** on the vendor line (`next_session_2026_09_14.md`).
  The current `CONFIG.SYS` has neither, so the vendor is on IRQ 7 with default write
  timing. Both remain worth one boot each, now that there is a reproducer that fails in
  under two seconds.
- The disk currently holds the test set (`S4K`, `S5120`, `S8K`, `S16K`, `S16B`, `R16K`,
  `W8K`, `A1`-`A4`, `AP4`) alongside the original `FC.EXE` reproducer, which is untouched.
