# Optimisation work order — 2026-09-21

The plan in `bus_optimisation_plan.md` lists sixteen actions, A1-A16. This is the **order to
do them in**, and what changed since that list was written.

Ordering principle, unchanged and still right: **(evidence available) × (cost to get it)**, not
size of prize. A big lever with no way to measure it ranks below a small one that can be settled
this week.

---

## What changed since the list was written

Four rows are no longer what they say:

| | |
|---|---|
| **A9 / E2** — what is already shadowed | ✅ **CLOSED 2026-09-20.** Five regions measured, only `0xF000` is shadowed, and no configuration on this machine moves an option ROM off the bus. Do not re-run |
| **A5** — DMA channel inventory | ⛔ **IMPOSSIBLE AS WRITTEN.** The 8237A's mask and mode registers are **write-only**, as are the XT's 74LS670 page latches. Channel assignments cannot be read back on a 5160. Status port `0x08` read `0x81` twice — bit 7 is almost certainly an undriven DREQ3 floating. Inconclusive, and it stays that way without a logic analyser |
| **A4** — time a real DMA transfer | ⚠ **METHOD CORRECTED.** It cannot use the floppy: a track read is rotation-limited at ~4,608 bytes per 200 ms, roughly **43 us/byte**, so it measures the disk spinning and not the bus. The owner's route in is the **Sound Blaster Pro's DMA channel 1**, which transfers asynchronously |
| **A1** — request merging | 🎯 **UNBLOCKED.** It was queued behind the LS-120, and the LS-120 is finished |

And one addition from outside: [ISA-PicoMEM](https://github.com/FreddyVRetro/ISA-PicoMEM)
independently reproduces the request-merging result — *"single sector read is slower than
multiple sector read"* — which is A1's premise, arrived at on different hardware.

---

## The order

Two tracks, because the constraint is **whether the owner is at the machine**. Track A needs
nobody and no hardware; track B needs the 5160. Run track A in any gap.

### Track A — no hardware, no boot, binaries we already hold

Three `pedis.py` runs. **Do these first**: they cost minutes, they have never been run, and each
one *ranks* a piece of track B. Going to the machine without them means measuring blind.

| | Action | Command | Answers |
|---|---|---|---|
| **A10 / A14** | Does `T130.MPD` use string I/O? | `tools/pedis.py T130.MPD io` | Whether the SCSI driver already collects the 35%/33% win XT-IDE got, or whether that lever is still on the table. **One command, never run** |
| **A13** | 3C509B packet buffer, drained in bulk? | `pedis.py` the packet driver — `rep insw` vs per-byte loops | [#32](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/32). Never examined |
| **A15a** | Does the SCSI chain disconnect? | `pedis.py` `T130.MPD` for identify-message handling; the INF/registry for a disconnect setting | Half of [#31](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/31). ⚠ **Indicative only** — the value could be computed at runtime, so a negative here is not proof. The definitive test is dynamic |

### Track B — needs the 5160

**B1 first, and it is not an optimisation.**

| | Action | Cost | Why this order |
|---|---|---|---|
| **B1 = A3** | ❗ **Can the 8237 reach the 384 KB that E1 moved onto the Inboard?** Poison a conventional-memory buffer, run one DMA read into it, verify the bytes landed | 1 DOS run, needs a `.COM` — **not** a DEBUG script | **Correctness, not speed.** Sound and floppy allocate there *today*. The plan says do it before A1 and that is still right. ⚠ Confound: DOS INT 25h and BIOS INT 13h both pass through `INBRDPC.SYS`, which may bounce-buffer through low memory. **A failure is decisive; a success is not** |
| **B2 = A4 + A6** | Time DMA **and** test CPU overlap in **one** experiment: PIT-timed CPU loop with SB Pro DMA channel 1 running, then idle | 1 DOS run | The two questions share an instrument. The delta is bus cycles stolen, which is exactly what the cost model ranks — and it is *why* DMA is worth more on an Inboard than on a stock XT. [#29](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/29) |
| **B3 = A1 + A2** | **Request merging in `XTIDEMP.MPD`**, plus the transfer-loop work | driver build + 1 boot | 🎯 **The biggest measured lever left.** 1.88× on sequential, modelled not guessed; a 1-sector command is 53% overhead. A2 rides along on the same build, now informed by the 2026-09-20 width measurement: **dword is 44% cheaper per byte** than byte-wide. [#30](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/30) |
| **B4 = A16** | `MODE SENSE` page 8 on every SCSI target; `MODE SELECT` what is off | 1 DOS run, **no code** | Read cache, write cache, prefetch — settable per device, and nobody has looked. [#31](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/31) |
| **B5 = A7** | DRAM refresh tuning — reprogram PIT channel 1, memory-pattern test, throughput A/B | 1 DOS run, instantly reversible | A tax every device pays, and E1 changed the premise. [#33](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/33) |
| **B6 = A12a** | Display mode as a bus lever — benchmark 1024×768 vs 800×600 vs 640×480 | Control Panel + a benchmark, **no code** | ~2.6× fewer bytes at 640×480 and ~470 KB more off-screen VRAM. **A trade, so the owner's call** — it is the only row here that costs something visible. [#34](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/34) |

### Track C — emulator, whenever

| | Action | Why it is last |
|---|---|---|
| **A11** | Memory-mapped storage in 86Box — prove the 4.2× before anyone buys hardware | Now has a worked design to copy: PicoMEM moves disk data through a 16 KB memory aperture. But **nothing on this machine has an aperture**, so this buys a number, not a speed-up. [#35](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/35) |
| **#38** | Wire `lpt_epat.c` to upstream's EPP callbacks | Not optimisation. It unblocks running the **vendor** miniport in the bed, which matters for upstream credibility rather than for this machine's speed |

---

## What I would actually do next

**Track A, all three, in one sitting.** They are free, they have never been run, and two of them
could remove a whole track-B item before it costs a boot — if `T130.MPD` already uses string I/O
then A10's lever is closed, and if the packet driver already drains in bulk then so is #32's.

**Then B1**, because it is a correctness question about memory that sound and floppy are using
today, and it has been outstanding since 2026-09-20 with a known-wrong harness.

**Then B3**, which is the biggest measured prize on the machine.

⛔ **Not first:** B6. It is the only item that trades something the owner can see, and ranking it
by bytes-on-the-bus alone ignores that he chose 1024×768 deliberately.

---

## One warning to carry into B4

From [BlueSCSI's own guidance](https://bluescsi.com/docs/Performance): on old or slow hosts,
**asynchronous transfer is often faster and more reliable than synchronous**. An Amiga A2091
benchmarked better in async.

**The fastest negotiated mode is not the fastest real mode.** Measure before and after, and be
willing to put a cache back the way it was.
