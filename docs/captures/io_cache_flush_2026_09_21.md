# One I/O access costs far more than its bus time — measured 2026-09-21

**SW1 on the CPU module is ON, the cache flush on I/O is real, and it is large.**
Established by measurement over COMrade, without opening the case.

## Why this was asked

feipoa's DIP-switch table for the IBM 486BL3 module
([cpu-world](https://www.cpu-world.com/forum/viewtopic.php?t=33652&view=previous&))
gives **SW1 = cache flush trigger: ON = "DMA + I/O read/write", OFF = "DMA only"**, and
warns that an IBM-based system *"cannot even run DOOM"* with it OFF. This machine runs
Windows 95, so SW1 was almost certainly ON — but "almost certainly" is not a number, and
the switch position alone would not say what it costs.

## Method

`BUSFLUSH.COM`, 212 bytes, from `tools/gen_busload_com.py --flush`, capstone-verified before
deployment (4 `rep lodsd`, **exactly 2** port reads, 8 PIT latches, nothing stray).

Four PIT-timed passes, alternating. Each pass re-reads a **1 KB** buffer **64 times** —
64 KB total, working set far inside the BL3's 16 KB L1, so after the first pass every read
should hit cache. The only difference between pass types is **one `in al,0x21`** per outer
iteration — the 8259's interrupt mask register, a standard non-destructive read, with
interrupts already masked by `cli`.

## Result

| pass | ticks | |
|---|---|---|
| 1 — no I/O | 3092 | |
| 2 — **with I/O** | 10072 | |
| 3 — no I/O | 3090 | agrees with pass 1 to **0.06%** |
| 4 — **with I/O** | 9956 | agrees with pass 2 to **1.2%** |

| | |
|---|---|
| cached | **39.5 ns/byte** |
| with one I/O per KB | **128.1 ns/byte** |
| **penalty** | **3.24x** |
| per I/O access | **90.7 us**, of which only **5.7 us** is bus |
| attributable to refill | **83 ns/byte** over the lost working set |

## ⭐ The cross-check that makes this convincing

**128.1 ns/byte** flushed, against **135 ns/byte** measured independently for conventional
RAM in **E1** on 2026-09-11 — a different instrument, a different session, agreeing to 5%.
When the cache is flushed we are reading at Inboard-RAM speed, which is exactly what a real
flush predicts.

## What it changes

⛔ **The cost model in `bus_optimisation_plan.md` is a FLOOR, not the cost.** Every ranking
in that document prices an 8-bit I/O access at **5.695 us** of bus. That is only the bus
half: the access also destroys the cache, and the refill is paid by whatever code runs next.

⭐ **Request merging (A1) is worth more than 1.88x.** Fewer commands means fewer I/O
accesses means fewer flushes — a third saving nobody had counted.

⭐ **Paced polling is worth more than measured.** `XT_POLL_BACKOFF` was justified on bus
occupancy alone. Each poll it removes also removes a cache flush. That applies to the
unpaced drivers too — **A17** (`HSFLOP.PDR`), **A18** (`T130.MPD`), **A21** (`ELNK3.VXD`).

⚠ **And it narrows technique 109's claim**, without retracting it. *"A register-only delay
runs from L1 and costs NO bus cycle"* — the **bus** half stands, and that is the half that
matters for occupancy. The **L1** half does not survive the poll that precedes it. The
delay loop itself is three bytes so its own refill is trivial; what is destroyed is the
*driver's* working set, and that was already happening before pacing.

## Caveats, stated plainly

- **The refill cost scales with the working set**, and 1 KB here was chosen by us. A driver
  with a smaller hot set pays less. **The transferable number is the 83 ns/byte rate, not
  the 90.7 us per access.**
- This proves the **cache is invalidated on I/O**. It does not, by itself, prove SW1 is the
  mechanism — but SW1 is the documented cause and feipoa's compatibility note says an IBM
  system needs it ON.
- `rep lodsd` is not the fastest read primitive available; the absolute ns/byte figures
  belong to this access pattern.

## Harness gotchas paid for here

- ⛔ **The result area was not cleared between runs**, so a program that failed to launch
  left the *previous* run's values sitting there looking valid. Fixed by poking
  `DEADDEADDEADDEAD...` into `0040:00F0` first — the surviving `dead` bytes in slots 5-8
  then also prove only four values were written.
- ⛔ **COMrade writes files out-of-band, so COMMAND.COM does not see them.** A freshly
  written `.COM` gave *"Bad command or file name"* while `file_stat` showed it present at
  the right size. **A `DIR` forces the directory re-read**, after which it runs.
