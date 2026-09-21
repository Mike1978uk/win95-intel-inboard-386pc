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

### ✅ Track A RESULTS — run 2026-09-21, all three

**Two of the three levers were already taken.** That is the whole argument for doing the free
checks first: both would have cost a hardware run to discover.

| | Question | Answer |
|---|---|---|
| **A10/A14** | Does `T130.MPD` use string I/O? | ✅ **Yes, already.** Imports `ScsiPortRead/WritePortBufferUshort`. The 35%/33% class of win XT-IDE got is **not** on the table here |
| **A13** | Is the 3C509B drained in bulk? | ✅ **Yes, already, and at dword.** `ELNK3.VXD` (30,773 B, 1995-07-11, pulled off the machine) does `mov ecx,[ebp-0x1c]` / `shr ecx,2` / `rep insd`. Disassembled, not pattern-matched |
| **A15a** | Does the SCSI chain disconnect? | ⛔ **No evidence it ever grants it.** Zero `0xc0` and zero `or ...,0x40` in the whole driver — so it is not a computed value either. **This lever is OPEN** |

#### What the T130B disassembly gave beyond yes/no

| | |
|---|---|
| pseudo-DMA port | **`base+4`**, one fixed address, both directions |
| chunk | **0x40 ushorts = 128 bytes per call**, in a loop |
| NCR5380 registers | `base+8` … `base+0xd`, byte-wide |

⭐ **A new candidate: dword on the T130B.** 32-bit port access was ruled out in this plan
because `base+2`/`base+3` are XT-IDE's Error and Feature registers. **That reasoning is
XT-IDE's, and does not carry here** — the T130B's register file starts at `base+8`, so nothing
the driver touches occupies `base+5..+7`. Word → dword is 3.819 → 3.183 us/byte, about **16.6%**
on the data phase.

⚠ **Candidate, not a finding**, for three reasons:
1. "the driver does not touch `base+5..7`" is not "the card does not decode them";
2. the pseudo-DMA port handshakes on the 5380's DRQ, and the 2026-09-20 width measurement
   warns that a handshaked bus cycle **extends** rather than amortises;
3. `T130.MPD` is Adaptec's binary — this means patching an IAT entry and a count, not a rebuild.

Supporting but not conclusive: `ELNK3.VXD` does `rep insd` to a 16-bit ISA card at a single
port address, so a shipping period driver does do exactly this shape of thing.

#### ⚠ A15a is complete, and it reframes the lever

The INF side is now checked too: **neither `T130.INF` nor `T130-XT.INF` carries a disconnect
setting.** They set `PortDriver`, `Polling`, `DevLoader`, `DontLoadIfConflict`, `NoSetupUI` and
nothing else. So two independent lines agree - the driver never asks for disconnect, and there
is no knob to make it.

That means "the lever is open" does **not** mean "flip a registry key". It means patching
Adaptec's binary, which is a much bigger job than the plan implied.

⭐ **And it may be the wrong lever.** Disconnect frees the **SCSI** bus during a seek. What
this machine is short of is **ISA** bus. The card is jumpered for no IRQ and runs `Polling=1`,
so during a transfer the driver sits *polling the card over the ISA bus* - and that is the
occupancy the cost model actually ranks. XT-IDE already solved exactly this with
`XT_POLL_BACKOFF` (spin ~32 times, then a cache-resident delay costing zero bus cycles),
recorded in this plan as *"arguably worth more than the width win"*.

Static evidence, indicative only: 28 `ScsiPortReadPortUchar` call sites, **10 of them inside a
read/test/jump-back loop**, against 12 `ScsiPortStallExecution` sites. So it is partly paced
already and partly not.

❓ **Untested reasoning, not a measurement.** The cheap way to settle it is B2's own
instrument: a PIT-timed CPU loop, run while a SCSI transfer is in flight. If the loop slows,
polling is stealing bus cycles and paced polling is the lever. **Build that harness once and it
answers B2 and this together.**

#### Dead end recorded

❌ `references/3c509b_qemu` cannot answer the **buffer size** half of A13. It models its own
32-entry FIFO and a `txbuffer[2048+4]` — that is the emulator's implementation, not the card's
physical SRAM. `3C509B_PORT_SPEC.md` defers to it and so inherits the gap.

#### Consequence for the order

- **A10/A14 and A13 are closed.** [#32](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/32)
  can be closed or narrowed to the buffer-size question alone.
- **A15/A16 rise.** The SCSI chain is now the only device-side lever in Track A still open, and
  the static evidence points the same way twice.

---

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

### ⭐ Host side: what is actually left, checked 2026-09-21

The owner's standing point, and the plan already carries it as a rule: **do not rank a lever out
because it is small.** *"4% that costs nothing is 4%, and it compounds with every other change
rather than competing with it."* These were checked against the source, not re-asserted.

#### A2 is mostly a phantom — fold it into A1

The shipped read path is **already one `rep insw` per sector**:

```
shr  ecx, 1        ; 256 words
rep  insw          ; the whole 512-byte sector
```

There is no per-byte inner loop in the shipped build — `xrd_pio8_loop` is the
`XT_FAST_XFER`-off fallback. So the *"96% bus / 4% our loop"* figure was measured **with**
`rep insw` already in, which makes 4% the **residual** cost of `rep` plus per-sector setup, not
a loop waiting to be unrolled.

➡ **What is left of A2 is the per-SECTOR setup, and the way to remove that is to move more
sectors per command — which is A1.** Run them as one job, not two.

#### The plan's `rep movsd` row is aimed at the wrong file

It says *"`XTIDEMP.ASM` has **zero** string ops and 9 byte-move lines"*. Wrong on both counts
now, and the correction matters more than the count:

| site | what it is | on the data path? |
|---|---|---|
| line 196 `rep stosb` | zeroes `HW_INITIALIZATION_DATA` | ❌ **runs once**, at init |
| line 853 `rep movsb` | `xsi_copyout` — INQUIRY-sized control data into the SRB buffer | ❌ per-request, but ~36 bytes, not bulk |

Both are byte-wide and **neither is on the bulk path**, which goes straight from `rep insw` into
the caller's buffer. Widening them is worth approximately nothing. **Do not spend a build on
this file for that reason.**

#### ⭐ The Windows side is the real untouched area, and the way in is on disk

The plan says the DDK's debug builds and symbols are *"still unopened ... the cheapest way into
this layer and nobody has looked."* **They are present**, at `C:/Users/lycet/OneDrive/Desktop/XT_project/Windows95_ddk/DEBUG`:

```
IOS.VXD + IOS.SYM        SCSIPORT.PDR + SCSIPORT.SYM
DISKTSD.VXD              CONFIGMG.VXD + .SYM
RUNWDEB.BAT   DEBUG.TXT   950/ 951/ 952/ 953/
```

⭐ **This is how A1 gets its premise settled without a single boot.** A 1-sector command is 53%
overhead — but our miniport does not choose the request size, **`DISKTSD` and `IOS` do**; ours
only advertises `MaximumTransferLength`. So *"why are the requests small?"* is a question about
those two binaries, and we hold debug builds **with symbols** for both. Raising
`MaximumTransferLength` without knowing what sits above it is guessing.

⚠ Check which Windows build `950/951/952/953` correspond to before trusting a symbol file
against the machine's own `IOS.VXD` — this install is **OSR1**.

#### Also already named, and still true

- **`HSFLOP.PDR` polls hard during a seek**, and it is a driver **we already patch**
  (`maxPhys`). Same paced-polling fix, same measured basis: a poll is 5.55 us of bus that moves
  nothing; a cached delay loop is 0.22 us and occupies no bus at all.
- **`T130.MPD` polling** — 10 of 28 `ScsiPortReadPortUchar` sites sit in tight
  read/test/jump-back loops. ✅ **Owner approved patching this binary, 2026-09-21.**

---

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
