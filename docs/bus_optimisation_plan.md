# Closing the XT-to-AT gap: a bus-occupancy plan

Started 2026-09-10. This is the plan file for the performance work; the commit history
stays short and points here.

## The one fact everything follows from

The Inboard's 386 and its RAM are **on the same card**, joined by a local bus. RAM access
never touches ISA. Only **I/O ports and motherboard memory** cross.

Measured on the real 5160:

| | |
|---|---|
| Fixed cost of a bus crossing | **~3.90 us** per port access |
| Marginal cost inside one | **~1.87 us** per byte |
| Cost of a cache-resident instruction | effectively zero bus |

Two consequences, and they drive every item below:

1. **Access *count* dominates, not byte count.** A word access pays the sync once for two
   bytes. This is where the 35% XT-IDE win came from.
2. **A polled status register costs the same ~3.90 us as a byte of real data.** A driver
   that idles noisily steals bus from every other card while transferring nothing.

So the metric is **bus cycles occupied to do the job**, not any one driver's throughput.
Those rank differently. Ranking by throughput is what made me dismiss sound and floppy;
that was wrong.

## Two questions for every driver we touch

1. **Does it poll?** Pace it: spin flat out briefly, then a register-only, cache-resident
   delay. Cheap, safe, and applies where there is no data path to widen at all.
2. **Does it move data a byte at a time?** Widen it, subject to the register map allowing
   it. Check the map first - on stride 1, `base+1` is the Error register and a word access
   corrupts.

## Ranked by occupancy

| # | Target | Poll lever | Width lever | Notes |
|---|---|---|---|---|
| 1 | **Trantor T130B SCSI** | yes - `Polling=1`, no IRQ | yes - PIO data loop | Vendor binary, no source. Patch or rewrite; `XTIDEMP.MPD` is a proven template |
| 2 | **Mach8 video** | - | yes | Most bytes on the bus of anything here. Framebuffer is on ISA, so every pixel crosses |
| 3 | **`HSFLOP.PDR` floppy** | yes - polls hard during a seek | little data | We already patch this binary, so it is the most tractable |
| 4 | **Sound (SB Pro)** | yes, if it polls the DSP | **no** | Data path is DMA. No per-byte loop to widen. The real hazard is 20-bit DMA reach (technique 62), a correctness issue |
| 5 | **3C509B network** | ? | only if not already string I/O | 16-bit card. Well-written packet drivers already use `rep insw`; check before spending time |
| 6 | **Keyboard** | few accesses per event | - | Latency-bound. Near zero to win |

"Ours" means **hardware the owner runs**, not source we wrote. `KEYBOARD.DRV` and
`VKD.VXD` are already modified vendor binaries; abandonware is fair game.

## Open experiments

### E1. Where does conventional memory actually live?  **ATTEMPTED 2026-09-11, RE-RUN NEEDED**

If the Inboard backfills the low 640 KB from card RAM, memory access is local and there is
nothing to win. If the 5160's own DRAM still serves it, **every access to a low buffer
crosses the bus** - an invisible tax on every driver, made worse because the 20-bit DMA
reach forces DMA buffers low.

Test: time a tight read loop against conventional memory vs two references that are
definitely across the bus - video RAM (`B800`) and system ROM (`F000`). Both are below 1 MB,
so no protected mode is needed. Script: `docs/captures/2026-09-11_ls120/RAMTIME.SCR`.

**First attempt's numbers are not trustworthy** - `FFFF` and `12FB` BIOS ticks are impossible
for a run that took seconds. What survives is directional: both conventional regions completed
in under one tick while both bus references took many, which points at conventional memory
being local to the Inboard and would close E1 as "no action". Re-run with PIT channel 0 latch
reads instead of the BIOS tick before believing it.

If they differ, the action is *not* "stop using motherboard RAM" - DOS and Windows need
conventional memory and the decoding is fixed in hardware. It is **move everything that
does not have to be low into Inboard RAM**: disk caches, driver buffers, working sets.
Only DMA buffers are genuinely pinned. An `INBRDPC.SYS` angle is plausible *after* this
measurement, not before.

### E2. What is already shadowed?

Every BIOS or option-ROM fetch from real ROM crosses the bus. Shadowing into card RAM
removes it. Some of this went upstream; confirm rather than assume.

### E3. Request merging - the remaining measured XT-IDE lever

**4.17 ms of fixed overhead per command.** Merging adjacent requests moves the same data
in fewer commands. Needs async completion -> queue depth -> merge.

## Ruled out, with reasons - do not re-derive

| Idea | Why not |
|---|---|
| Compressing the payload across the bus | For dumb devices (XT-CF, Mach8) there is no decoder on the far end and no shared language. The card wants exactly the bytes its registers expect. The only shorthand is fewer, wider transactions |
| `READ MULTIPLE` | Drive reports word 47 = 1: unsupported |
| 32-bit port access | `base+2`/`base+3` are Error/Feature. Stride 2 still decodes A1 |
| Wait-state tuning | Already 0 wait states, cache on. Measured. Port `0x670` is write-only |
| SCSI as a faster path than XT-IDE | Identical 6880 ticks |
| Taskfile widening | 11.38 vs 11.54 us - noise |
| DriveSpace compression | Maths favour it (~480 spare CPU cycles per byte of bus) but its real-mode INT 13h hooker is exactly what #17/#19/#21 fought to remove. Parked, not disproved |

**The one real form of "say more per crossing"** is not payload compression but raising the
level of the request, and it only works for devices that run a command language - SCSI and
ATAPI. That is E3.

## Done

- **XT-IDE word transfers** ("Mike's Hi-Speed mode"): `rep insw`/`outsw` gated on stride 2.
  **35% read / 33% write** on the data phase. Hi-Speed throughput with no hardware latch.
  Shipped and published.
- **XT-IDE paced polling** (`XT_POLL_BACKOFF`): spin ~32 times, then a cache-resident delay
  costing zero bus cycles. Arguably worth more than the width win, and invisible in a
  benchmark of that driver alone.

## Housekeeping: the skill file is getting long

`.claude/skills/inboard-hw-debug/SKILL.md` now carries 111 numbered techniques plus the Win95
boot inventory. It loads on demand, not every session, so length costs less than it looks - but
it is past the point where a reader can find things.

Proposed split, **not yet done**, and to be done without losing detail:

| Stays in `SKILL.md` | Moves out |
|---|---|
| The routing table and core principle | The full Win95 boot fix inventory -> `docs/win95_boot_fix_inventory.md` |
| Techniques that apply to *any* new investigation | Device-specific technique bodies -> the relevant `drivers/*/`*`_SPEC.md`, with a one-line pointer left behind |
| Retractions and corrections - the most valuable lines in the file | Superseded elimination logs -> `docs/archive/` |

Rule for the split: **a technique that resolved or ruled out a real bug keeps its evidence.**
Compress prose, never the measurement, the address, or the retraction.
