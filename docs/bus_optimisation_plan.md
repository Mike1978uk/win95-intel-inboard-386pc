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

## The standing principle

**No gain is too small to take.** This is not a hunt for one big win; it is cumulative relief
of a shared resource. A driver that stops polling noisily gives its cycles to every other card
on the bus, and those additions compound across the whole stack. A change worth 1% in isolation
is worth taking if it costs nothing to keep.

Corollary: never rank a target out on "it doesn't move much data". That reasoning wrongly
dismissed sound and floppy - both poll hard, and polling is pure contention. Rank on **bus
cycles occupied**, and if in doubt, measure rather than reason.

## Scope: the whole stack, not just storage

Everything the owner's machine runs is in scope. "Ours" means **hardware we run**, not source
we wrote - `KEYBOARD.DRV` and `VKD.VXD` are already modified vendor binaries, and this software
is long-abandoned; enthusiast preservation is the only remaining interest in it.

### The VxD layer - a target class in its own right

The owner's point, and it is a good one: the VxD stack is the heart of Windows itself, we have
already modified parts of it, and every VxD sits on a hot path.

| Already ours | What to look at |
|---|---|
| `VKD.VXD` (custom-built from DDK source) | Its INT 09 path runs on every keystroke |
| `KEYBOARD.DRV` (patched at `0xf14`) | Same path, ring 3 |
| `HSFLOP.PDR` (patched `maxPhys`) | **Polls hard during a seek** - the most tractable pacing target we already patch |
| `INBRDPC.SYS` | Only slows floppies (measured). Do not disable it - required, not optional |

The DDK ships **debug builds and symbols** of IOS, SCSIPORT, DISKTSD and VMM, plus `WDEB386`,
and they are **still unopened**. That is the cheapest way into this layer and nobody has looked.

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

**E1 is the one that must not be dropped.** It is cheap, it is unresolved, and if it goes the
wrong way it is the largest single lever in this document - a tax paid by every driver in the
stack on every buffer access.

### E1. Where does conventional memory actually live?  **CLOSED 2026-09-11**

**Answered, and acted on.** Intel's own Inboard manual requires it for a 5160:

> "You must disable conventional memory on the system board down to 256K bytes. Use a
> ballpoint pen to set switches 3 and 4 on the system board to ON."

The machine had been running SW1-3/4 **Off/Off** - all four banks, 640 KB of planar DRAM,
the Inboard backfilling nothing. Now On/On: 256 KB planar, **384 KB served from the card**.
POST still counts 640 KB, which is the backfill confirmed.

Sources: modem7 and cimonvg in
<https://forum.vcfed.org/index.php?threads/memory-layout-for-a-intel-inboard-386-pc.1257324/>
and <https://forum.vcfed.org/index.php?threads/windows-3-1-w-intel-inboard-386-pc-vxd-issue.79730/page-2>.
Note the 5150 is different and its advice does not transfer - *"The 5150 motherboard's RAM
sockets are permanently enabled"*; only the 5160 routes those switch signals to bank enables.

**Measured read speed: no change at all.** `RMTMB4.OUT` / `RMTMAF.OUT`, 32 KB linear sweep,
PIT channel 0, captures in `docs/captures/2026-09-11_ls120/`:

| region | before | after | delta |
|---|---|---|---|
| seg `1000` (64K - planar either way) | 5286 | 5284 | -2 |
| seg `8000` (512K - planar, then Inboard) | **5290** | **5290** | **0** |
| `B800` video | 16650 | 17740 | +1090 |
| `F000` ROM | 10552 | 10552 | 0 |

`F000` identical across two separate boots puts the noise floor at +/-2 ticks, so zero is a
real null. Both conventional regions read 135 ns/byte throughout.

**Why that does not make the change worthless, and why we stopped measuring.** A linear
sweep is the one pattern a line-fill cache hides - one miss pulls a line, the next reads
are hits, and the bus cost is amortised away. The test measures CPU-visible latency; this
track is about **bus occupancy**, which it cannot see. A strided retest was written
(`gen_ramstride.py`, `RMTMST.SCR` staged on the card) and **deliberately not run**: the
owner's call, and it is right, because no outcome changes the decision.

The gain is structural, not a latency win: **conventional memory that crosses the bus fell
from 640 KB to 256 KB.** Every access that used to land between 256 KB and 640 KB now stays
on the card. That is a 60% reduction in the conventional-memory footprint on the shared bus,
and by this document's own standing principle it is worth taking whether or not a read loop
can see it.

**Can we skip the remaining 256 KB entirely? No, and it may not be desirable.**
The bottom of memory is where the IVT, the BDA, IO.SYS/MSDOS.SYS, every `CONFIG.SYS`
real-mode driver and COMMAND.COM must live; DOS fills from the bottom up. Windows 95 has no
knob to exclude a physical low-RAM range (`EMMExclude` covers `A000-FFFF`, not low RAM).

And the inversion worth keeping: **planar DRAM is arguably the right home for DMA buffers.**
They are read by the DMA controller from the bus side, so a buffer in Inboard RAM makes every
DMA cycle cross onto the card, competing with the CPU. Kept in the planar 256 KB it does not -
and the 20-bit DMA reach (technique 62) forces those buffers low anyway. DOS puts them there
by default. So the current configuration is close to the right split by accident, and
"as good as it gets" is probably better than neutral.

**Action: switches stay On/On. E1 needs no further measurement.**

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

## E4. Memory-mapped storage beats I/O-mapped by 4.2x - @andrew-hoffman, CONFIRMED

His suggestion, on [#23](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/23),
2026-09-10:

> *"If memory accesses have fewer wait-states than IO port accesses (and it seems they do,
> because drawing to the screen isn't as horribly slow in your video as would be expected
> from throttling it to 180 kb/s max), try Disk-On-Chip storage or creating a CF card
> adapter that's memory-mapped instead of IO mapped."*

**He is right, and the margin is large.** Measured on the real 5160 the same day, 32 KB
linear sweeps timed on PIT channel 0 (`RMTMAF.OUT`, `docs/captures/2026-09-11_ls120/`):

| path | us/byte |
|---|---|
| conventional RAM, local to the Inboard | **0.135** |
| system ROM `F000`, across the bus | **0.270** |
| video RAM `B800`, across the bus | **0.454** |
| 8-bit I/O port, byte at a time | 5.770 |
| 8-bit I/O port, `rep insw` (the shipped XT-IDE path) | 1.910 |

**Memory-mapped across the bus is 4.2x faster per byte than our best I/O path, and 12.7x
faster than byte-wide I/O.** The fixed per-access synchronisation that dominates I/O
(~3.90 us, technique 109e) does not apply to a memory cycle at all.

That reframes the ceiling. Technique 109 put 8-bit PIO at ~180 KB/s and concluded no
software change beats it. True - but a memory-mapped adapter is not a software change, and
the same bus does ~2.2 MB/s to video RAM.

**His caveat is real and must be designed for:** the Inboard shadows memory, and shadowing
the window a memory-mapped card lives behind would break it. That is exactly the
`0x5E0000`/`0x5F0000` machinery in `inboard386.c` (techniques 66, 67, 72), so we already
know where to look.

**Status: idea confirmed, not actioned.** It needs hardware that does not exist here yet -
a DiskOnChip, or a memory-mapped CF adapter. Recorded so the number is not re-derived.

### Also from the same comment, and it CORRECTS one of our own notes

> *"Using the 32 bit miniport for storage is improving performance by getting rid of real
> mode thunks and allowing more of the OS to be paged out [...] not increasing the raw bus
> throughput. You only get that second benefit once **every** storage driver is 32-bit
> which is why the LS-120 work is worthwhile."*

That is the case for finishing #22 stated more precisely than we had it.

> *"DriveSpace has a 32 bit driver on Windows 95, which is higher in the storage stack than
> the miniports and transparent to them."*

**This contradicts why we parked compression.** Our note said DriveSpace was parked because
its real-mode INT 13h hooker is exactly what #17/#19/#21 fought to remove. On Windows 95
there is a 32-bit DriveSpace driver above the miniports, so that objection does not hold
for the Windows path. His own caution stands: ~150 KB of code, extra CPU, and
**significant** extra corruption risk. Reference he gave:
<http://www.faqs.org/faqs/windows/win95/faq/part11/>

> *"Still makes sense to support REP INSW/OUTSW transfers on every type of card that it
> will work on for some easy free performance improvement."*

Shipped for XT-IDE (35% read / 33% write). Not yet applied to the Trantor T130B, which
already imports `ScsiPortRead/WritePortBufferUshort` - see the ranked table above.

### E4a. The memory-mapped card exists, XUB supports it, and 86Box already models it

Following E4. The XT-CF cannot do this - it decodes I/O `0x300-0x31F` and its only memory
window is the option ROM socket at `D8000`, with no write path. But **JR-IDE/ISA** is a
period-correct memory-mapped IDE adapter for the PCjr/XT, XTIDE Universal BIOS drives it,
and **86Box models it in full** (`src/disk/hdc_xtide.c`):

```c
#define JRIDE_DATA_WINDOW_OFFSET   0x3a00   // IDE data window
#define JRIDE_DATA_WINDOW_SIZE     0x0200   // 512 bytes - a whole sector
#define JRIDE_CS0_OFFSET           0x3c00   // task file, memory-mapped
#define JRIDE_CS1_OFFSET           0x3c08
#define JRIDE_SCRATCH_OFFSET       0x3c12
```

Task file **and** data register in a memory window. A sector transfer becomes a `rep movsw`
out of memory rather than 512 I/O cycles.

**Predicted from E4's measurements**, per 512-byte sector data phase:

| path | us/byte | per sector |
|---|---|---|
| I/O `rep insw` (shipped XT-IDE) | 1.910 | 978 us |
| memory window, at video-RAM cost | 0.454 | 232 us |

**~4.2x on the data phase, and it costs nothing to test** - `hdc_xtide.c` needs only a
config change, exactly as the stride-1 bed did for #24 (technique 100's addendum: enumerate
what the emulator already models before declaring something untestable).

**What it needs from us:** a memory-mapped transport in `XTIDEMP.MPD`. That is a real
driver project, but it is the one piece of E4 that is software, and the emulator can prove
the gain before any hardware is sourced.

⚠ Two caveats to design for. Andrew's: **the Inboard must not shadow the window** the card
lives behind - the `0x5E0000`/`0x5F0000` machinery, techniques 66/67/72. And ours: the
measured 0.454 us/byte is **video RAM**, which has CRTC contention; system ROM measured
0.270. A JR-IDE window may land anywhere between, so treat 4.2x as the conservative end.

## The blind spot: we optimised the CARD and stopped at ITS limit

The owner's correction, 2026-09-11, and it reframes the whole track:

> *"we stopped at the card side the hardware limit of the xtide but left a hole at what it's
> plugged into - we should absolutely do all of these things"*

Every XT-IDE gain so far attacked the **peripheral**: word transfers, paced polling, the
register map. Then we hit the card's decode limit (A1 decoded, so no `insd`) and treated
that as the end of the road. **It is the end of one road.** The transaction crosses a bus
into a machine, and everything on the far side of the connector was left untouched.

⚠ And I compounded it by writing *"a faster inner loop buys ~4% at best"* - ranking a
target out on size, which is the exact anti-pattern recorded at the top of this document
and the one that wrongly dismissed sound and floppy. **4% that costs nothing is 4%**, and
it compounds with every other change rather than competing with it.

### Both sides of the connector

| side | lever | worth | state |
|---|---|---|---|
| **card** | word transfers (`rep insw`/`outsw`) | 35% / 33% | ✅ shipped |
| **card** | paced polling | frees bus, invisible in a single-driver benchmark | ✅ shipped |
| **card** | `rep insd` | +34% | ❌ blocked - A1 decoded, `base+2` is Error |
| **card** | a wider-decode or memory-mapped card | up to 8x | needs different hardware (E4/E4a) |
| **host** | **request merging** | **1.88x** on sequential - a 1-sector command is 53% overhead | ❌ not started (E3) |
| **host** | transfer-loop overhead | **4%**, free, compounds | ❌ not started |
| **host** | `rep movsd` for buffer copies in local RAM | small, free | ❌ `XTIDEMP.ASM` has **zero** string ops and 9 byte-move lines |
| **host** | compression (32-bit DriveSpace) | trades ~480 spare CPU cycles per bus byte | parked - risk, not architecture |
| **host** | read-ahead into Inboard RAM | moves bus work off the critical path | not costed |
| **host** | the VxD layer above the miniport | unmeasured | DDK debug builds + symbols still unopened |

### Why the host side pays unusually well HERE

The 3.90 us fixed cost per I/O access is the Inboard **synchronising a fast CPU down to a
4.77 MHz bus**. A stock 8088 never pays it - there the CPU *is* the bus. So this machine has
an unusually **high** cost per transaction and an unusually **low** cost per computation
(0.135 us/byte to local RAM, and a cached instruction costs no bus at all).

That asymmetry is the whole opportunity: **spend CPU to avoid transactions.** It
generalises to any 386-class XT accelerator; the magnitude is Inboard-specific and measured.

### Standing rule

**Do all of them.** Do not rank a lever out because it is small, and do not stop at a
peripheral's limit without asking what the transaction costs on the other side of the
connector. Rank by bus cycles occupied, take every gain that costs nothing to keep, and
when a hardware ceiling is reached, turn round.
