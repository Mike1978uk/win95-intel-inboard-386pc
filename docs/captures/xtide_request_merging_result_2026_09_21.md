# Request merging buys nothing on XT-IDE — measured 2026-09-21

**A/B on the real 5160, both arms, same workload, same session. The answer is a clean null.**

## Result

| arm | driver | elapsed | throughput |
|---|---|---|---|
| baseline | `XTIDEMP.MPD` `db88f64d`, `NumberOfPhysicalBreaks = 0` | **93.15 s** | **385.1 KiB/s** |
| test | `XTIDEMP.PB` `a6bfb48b`, `NumberOfPhysicalBreaks = 17` | **93.26 s** | 384.7 KiB/s |

**Difference: 0.11 s, 0.12%** — and the *baseline* is the faster of the two, so it is noise.

Workload: `COPY C:\GOODTIME\GOODTIME.MPG NUL`, **36,735,152 bytes**, from an MS-DOS Prompt
inside Windows 95 so the transfer goes through `XTIDEMP.MPD`. The file is **7x** the machine's
total RAM, so the cache cannot hide the disk. Raw output: `pbench_ab_2026_09_21.txt`.

The two builds differ by **exactly one functional byte** (`0x0D57`, `00` → `11`), verified by
diff — see `xtidemp_physbreaks_arms_2026_09_21.md`. This is as close to a single-variable
experiment as this project gets.

## What it means

⛔ **Do not adopt `-PhysBreaks 17`.** It was modelled at **1.88x** on sequential and delivers
**nothing**. The model was wrong, or the premise was.

The premise was that `NumberOfPhysicalBreaks = 0` promises SCSIPORT one physical run and so
caps every request at a page, making the advertised 64 KB `MaximumTransferLength` unreachable.
Raising it to 17 (64 KB of 4 KB pages) should then have cut the command count ~16x on a
sequential read. **It did not move the clock at all.** So either

- the cap was never binding for this path, or
- **command overhead is not the bottleneck** — the data phase is.

⭐ **The throughput figure points at the second.** 385 KiB/s is **2.1x** the ~180 KB/s ceiling
the driver source derives for *byte-wide* 8-bit PIO, which is what `XT_FAST_XFER`'s
`rep insw`/`outsw` already bought. We are running at the width-limited rate, and the transfer
is **bus-bound per byte**, not per command. Removing commands from a per-byte-bound transfer
changes nothing, which is exactly what was measured.

## ⭐ This closes XT-IDE

Every lever is now either shipped or measured shut:

| lever | |
|---|---|
| word transfers (`XT_FAST_XFER`) | ✅ shipped, 35% read / 33% write |
| paced polling (`XT_POLL_BACKOFF`) | ✅ shipped |
| "the free 4%" transfer loop | ⛔ phantom — already one `rep insw` per sector |
| **request merging** | ⛔ **measured, no effect** |
| `rep insd` (dword) | ⛔ blocked — stride 2 decodes A1, `base+2/+3` are Error/Feature |
| memory aperture | ⛔ no card on this machine has one |

**There is nothing further to do to this driver without different hardware.**

## ⭐ OPEN QUESTION the null threw up: the transfer beats the cost model by 1.51x

| | us/byte | this file |
|---|---|---|
| **measured, real transfer** | **2.536** | **93.15 s** |
| iowidth table, word | 3.819 | 140.3 s |
| iowidth table, byte | 5.695 | 209.2 s |
| iowidth table, dword | 3.183 | 116.9 s |

The shipped driver uses `rep insw` (word), so the model predicts **140 s**. It took **93 s** —
**1.51x faster than the machine's own measured width table**, and faster even than the *dword*
figure the same table says is the floor.

⛔ **Something is wrong, and it matters**, because that table is what ranks every remaining
lever in `bus_optimisation_plan.md` — including how much paced polling is worth.

Candidates, none tested:
- The iowidth probe used the **LPT port** (`0x378`, control `0x278`); the XT-CF is at `0x300`.
  The memory note claims the cost is the Inboard's sync and so *"applies to every ISA device on
  the machine"*. **This measurement is evidence against that.**
- The probe **wrote** to a port nothing answers; this **reads** a real card. 109e found reads
  slightly *slower* (5.87 vs 5.695 byte-wide), so that is the wrong direction.
- Not every byte crosses the bus — but the file is **7x RAM**, so caching cannot explain 1.5x.
- `rep` loop overhead was ~9% of the probe's figure, nowhere near 51%.

➡ **Next step**: re-run `tools/gen_iowidth_probe.py` against the **XT-CF's data port** rather
than LPT, and see whether the per-byte cost is card-specific. If it is, the width table is not a
machine constant and several rankings need revisiting.

## Scope, stated honestly

This measures **one large sequential read**. Merging helps most where requests are small and
numerous, and small-random I/O was not tested. But the sequential case was the one modelled at
1.88x, and it is the case the optimisation plan ranks by. A lever that does nothing in its own
best case does not get adopted on the hope of another.

⚠ The negative also **retires a risk**: Windows 95 boots and runs normally on the
`-PhysBreaks 17` arm, so 64 KB SRBs on the boot volume are harmless. That is worth knowing even
though the change is not being kept.
