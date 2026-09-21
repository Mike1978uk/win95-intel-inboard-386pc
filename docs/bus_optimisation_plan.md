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
the Inboard backfilling nothing. Now On/On: **bank 0 only**.

⛔ **CORRECTED 2026-09-21: that is 64 KB planar and 576 KB from the card, not 256/384.** This line read *"256 KB planar, 384 KB served from the card"* for ten days. The owner read the chips in bank 0: **HYB4164** - Siemens 64K x 1, nine to a bank with parity - so this is the **64-256 KB** 5160 planar and bank 0 holds **64 KB**. The reduction E1 achieved is therefore **640 KB -> 64 KB, a 90% cut** in conventional memory crossing the bus, not the 60% recorded below. ⭐ It also puts the machine **well under** Intel's 256 KB ceiling with room to spare, where 256 KB would have been exactly at it.
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


## A23. The CPU upgrade module — the owner's lead, 2026-09-21

➡ **Tracked as [#40](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/40).**

> *"we matched the cpu registers that were set by revto486 but we haven't explored if there is
> more to be done or better outcomes ... i believe that module has switches also."*

**Both halves are unexplored, and one is already written down as unfinished.**

### What is set today, and verified by read-back

| register | value | |
|---|---|---|
| `1000h:0` | `92` | CE, ASNP, CPC enabled · **SNP disabled** |
| `1000h:1` | `9C` | CNPX, **XTOUT**, CRLD, IKEN enabled |
| `1000h:2` | `00` | ❓ purpose never recorded |
| `1001h:0/1` | `FF` / `03` | LMCR - low 640 KB cacheable |
| `1001h:2/3` | `00` / `00` | ❓ LMROR - **no region marked read-only** |
| `1001h:4` | `F0` | CMLR - **our 2026-09-12 fix**, 1-16 MB cacheable |
| `1001h:5/6` | `00` / `00` | ECMLR - above 16 MB, moot on a 5 MB machine |
| `1002h:3` | `03` | 2:1 clock, doubling confirmed live |

### The concrete one

⭐ **`XTOUT` (`1000h:1` bit 4) is set; feipoa recommends `0`.** Recorded on 2026-09-12 under
"Not done" as *"separate, minor, untested here"* and never picked up. One bit, one boot,
instantly reversible.

### Method constraints — already paid for, do not re-derive

- ✅ **`22h`/`23h` do NOT hit the 8259.** Suspected (technique 75's aliasing) and **disproven**:
  every value written read back correctly. The BL3 claims those I/O cycles internally rather than
  driving them onto the bus - if it did not, an index write to `22h` would fire ICW1 and kill
  interrupts on every boot. **Do not re-raise this.**
- ⛔ **`CTCHIP34`'s screen print is NOT a read-back.** Proven by running the same batch under
  86Box, where `cpu_read()` returns `0xFF` and the writes are dropped entirely - and CTCHIP
  still prints `92 / CE: Internal Cache: enabled`. **Verify with a separate, write-free
  `CPUSHOW.BAT` invocation, always.**
- ⚠ **Do not raise CMLR beyond installed RAM.** The Inboard aliases its BIOS shadow at
  `0x5E0000`/`0x5F0000` ≈ 5.9 MB, above the 5 MB fitted. Caching an alias window invites stale
  data.
- **Benchmark harness exists**: the CMLR change was sized with Dhrystone and a cache latency
  walk (`docs/cpu_cache_cmlr_2026_09_12.md`). Reuse it rather than inventing a measure.

### Why it is complementary, not a competitor

This plan ranks by **bus occupancy**, and CPU registers do not move bus cycles. But the whole
strategy here is *"spend CPU to avoid transactions"* - 0.2 us for a cached instruction against
5.55 us for an I/O access. **A faster CPU makes that trade better**, so this compounds with every
pacing and merging lever rather than competing with them.

### ❌ Missing: the switch documentation

The owner has a cpu-world page detailing the module's **physical switches**. It was shared in an
earlier session and **never recorded on this page** - the second such gap found on 2026-09-21,
after Trixter. **Get the URL and record it before the hardware side is touched.**

## E7. Take the planar RAM out entirely — the owner's idea, 2026-09-21

> *"if we removed the system ram and modified the bios we might be able to then utilise 100% fast
> ram on the inboard and not cross the bus at all for ram ... could the inboard backfill the whole
> 640kb? people have changed roms before on these machines historically so it is a period correct
> thing to potentially explore."*

**Well founded, and the same class as E1 — which paid.** E1 took conventional memory on the bus
from 640 KB to 256 KB. This takes the remaining 256 KB to zero. By this document's standing
principle, rank by bus cycles occupied, that is the largest structural change left to
conventional memory.

Seed: [minuszerodegrees, substituting the 5150 U33 BIOS ROM](https://www.minuszerodegrees.net/5150/motherboard/IBM%205150%20motherboard%20-%20Substituting%20the%20U33%20BIOS%20ROM.htm) — a
diagnostic ROM can run with no usable RAM because it keeps everything in registers, which is what
makes "boot without bank 0" thinkable at all. ⚠ It documents the **5150**; the 5160 case is not
established and must not be assumed.

### One correction to the premise, and it matters

The owner's phrase is *"fast ram"*. **Measured, it is not faster.** E1's own sweep:

| region | before | after | delta |
|---|---|---|---|
| seg `1000` (64K — planar either way) | 5286 | 5284 | -2 |
| seg `8000` (512K — planar, then **Inboard**) | **5290** | **5290** | **0** |

Against a +/-2 tick noise floor, moving 512 K from planar to card changed read speed by **nothing**.
Both read 135 ns/byte. The gain from E1 was **occupancy, not latency** — and E7's would be too.

⭐ **But that null is not safe, and the owner's idea is exactly what makes it worth re-testing.**
E1 says so itself: *"a linear sweep is the one pattern a line-fill cache hides."* The strided
retest (`gen_ramstride.py`, `RMTMST.SCR`) was written and **deliberately not run**, on the
reasoning that *"no outcome changes the decision."*

➡ **That reasoning has now expired.** There is a decision that depends on it: whether card RAM
is genuinely faster decides whether E7 is worth the risk of a ROM change, or whether it is only
an occupancy play. **Run `gen_ramstride.py` before anything else here.**

### ⛔ There is no cheap switch version — corrected 2026-09-21

A first draft of this section proposed *"set SW1-3/4 for 64 KB planar and see what POST counts"*.
**That setting does not exist.** Per
[minuszerodegrees, 5160 switch settings](https://www.minuszerodegrees.net/5160/misc/5160_motherboard_switch_settings.htm):

| SW1-3 | SW1-4 | result |
|---|---|---|
| **ON** | **ON** | **enable only bank 0** ← **this machine, already** |
| OFF | ON | banks 0/1 |
| ON | OFF | banks 0/1/2 |
| OFF | OFF | banks 0/1/2/3 |

The switches are **already at their minimum**, set on 2026-09-11. On the **256-640 KB** board
revision bank 0 would itself be 256 KB. ⛔ **It is not that board** - corrected 2026-09-21, see
E1 above: the chips are **HYB4164** (64K x 1), so this is the 64-256 KB planar and bank 0 is
**64 KB**.

➡ So what is already proven is that **the card backfills 256 KB-640 KB**. What is unproven is
whether it can serve **0-256 KB**, and there is no switch that asks the question. Bank 0 comes out
physically or not at all. **E7 has no cheap first step; its first step is the experiment itself.**

⚠ Confirm the board revision before anything else - "64-256 KB" and "256-640 KB" 5160 planars
both exist and their bank sizes differ, so the RAM total and what bank 0 holds depend on which one
this is.

### ⭐ E7 IS LIVE — I closed it an hour too early, and the primary source says otherwise

⛔ **RETRACTION, same session.** I closed E7 on Intel's FaxBACK compatibility catalog, which says
machine after machine that a board must be *"disabled down to 256K bytes"*. I read that as a
**floor**. Intel's own manual, in `inboard_files/DOX1.TXT`, says it is a **ceiling**:

> **S.O.S. Beeps.** *This error occurs if the InBoard 386/PC detects the computer supplying over
> 256k of memory. The InBoard 386/PC can only replace the system board memory if **256k (or
> less)** is being provided by the system board.*
>
> *It may be necessary to check with the system board manufacturer as to how to properly disable
> memory to **256k (or lower)**.*

**"Or less." "Or lower."** Twice, and the second is an instruction to the user, which is hard to
read as loose phrasing. The card also **detects** how much the board supplies - it raises S.O.S.
beeps above 256 KB - so it measures rather than assumes.

➡ **The constraint is `board <= 256 KB`, not `board == 256 KB`.** Intel's wording permits the
system board supplying less, and the card *"replaces the system board memory"* accordingly.

**The lesson, which is the reusable part:** the FaxBACK catalog is a **secondary, derived** source
- a compatibility list written per machine. `DOX1.TXT` is the **product's own manual**. When they
disagree, the primary source wins, and I should have read it first rather than closing on the one
I happened to grep. [[feedback-real-hardware-outranks-inference]] one rung further down.

### ⭐ The decode is FIXED IN A PAL — @RonnyRoy's dumps, read 2026-09-21

The fourth lead paid. [@RonnyRoy's reproduction](https://github.com/ronnyroy111/inboard386) dumped
the card's PALs — *"luckily not secured"* — and published **CUPL equation files**, not just JEDEC
blobs. Cloned to `references/inboard386_ronnyroy/` (gitignored: read it, do not vendor it).

**`logic/U71.pld` is the memory decoder** — the only one of the sixteen PALs that touches
`A16`-`A23`. It is also one of only two RonnyRoy modified, which is a hint in itself.

```
INPUTS   A16..A23, i9, W_R, DACK0, ROMCACHE, _A20EN, i13, MAENA_BA
OUTPUTS  RAS_EN_SYS, RAS_EN_1M_1, RAS_EN_1M_2, RAS_EN_4M, _32BIT_EN, _MA7C, _MA8a
```

⛔ **There is no configuration input.** No jumper line, no register bit, no switch. The bank
enables are a pure function of the address lines and a few control signals. **If the backfill
boundary were settable, this PAL would need an input telling it where the boundary is, and it has
none.**

➡ **That answers the owner's preferred route directly: there is no software disable.** It also
explains why UniPCemu models no such register — there is nothing to model. Intel's remedy for
non-compliant boards being *a PAL from Smartec* fits the same picture: on this card, memory decode
is wiring.

⚠ **What this does NOT settle, and I am not going to claim it does.** Which address ranges each
`RAS_EN_*` actually covers needs the schematic and the two unnamed inputs `i9` and `i13`, whose
names the dump did not recover. A first read of the sum-of-products is suggestive — `RAS_EN_SYS`
looks like the card's own base 1 MB bank rather than the system board — but reading PAL equations
without the netlist is exactly the kind of inference this project keeps getting wrong. **Decoding
U71 properly is a session of its own**, and the material to do it is now local.

⭐ **And it is the route to a real answer, not a guess.** The equations plus
`inboard386/inboard386.kicad_sch` are enough to derive the true backfill range **on paper**, with
no hardware, no case open and no risk — which is a far better outcome than the experiment the
owner was contemplating.

### ⭐ So everything now turns on one fact: which board revision

Per [minuszerodegrees](https://www.minuszerodegrees.net/5160/misc/5160_motherboard_switch_settings.htm)
there are **two** 5160 planars, and SW1-3/4 = ON/ON means *bank 0 only* on both:

| board revision | bank 0 holds | planar RAM today | card supplies | at Intel's ceiling? |
|---|---|---|---|---|
| **256-640 KB** | 256 KB | **256 KB** | 384 KB | ⚠ **exactly at it** - no headroom |
| **64-256 KB** | 64 KB | **64 KB** | **576 KB** | ✅ **already well under it** |

✅ **ANSWERED 2026-09-21: it is the 64-256 KB board.** The owner read bank 0's chips -
**HYB4164 P3EF 8437**, Siemens 64K x 1. So the machine has been at **64 KB planar / 576 KB from
the card** since 2026-09-11, and **only 64 KB of conventional memory still crosses the bus.**

⭐ **Most of E7's prize was already collected, and nobody noticed for ten days** - because E1's
note said 256/384 and nobody checked the chips. The outstanding gain is the last **64 KB**, not
256 KB, which changes the cost/benefit of a hardware modification sharply: E7 is now a
**bookkeeping correction plus a small remainder**, not a big lever.

### What stays true from the earlier closure

- **UniPCemu models no backfill base register** - `inboardXT_PortA0` bit 7 is ROM mapping only.
  So there is still no evidence of a *software* control, and the owner's preferred route remains
  unsupported. What changed is the **floor**, not the mechanism.
- **The bank enable is decode logic** - Intel offers a *PAL* to boards that cannot disable memory,
  never a setting.
- ✅ `gen_ramstride.py` still unrun, and still worth running either way.

### ⛔ Superseded reasoning, kept because it was wrong in an instructive way

### ⛔ E7 LOOKS CLOSED — investigated and answered 2026-09-21, offline, same day

Three independent lines all say the backfill floor is **256 KB and it is hardware**. No case was
opened, no ROM touched, no chip pulled.

**1. Intel's own documentation, repeatedly.** The FaxBACK catalog
(`OneDrive/Desktop/XT_project/ibrd/`) states the requirement machine by machine, and always as the
same number:

> *"This version of the memory expansion card **can't be disabled down to 256K bytes**, so Inboard
> 386 can't provide conventional memory in these computers without the ILIM386.SYS memory manager."*

> *"You must remove SIMMs on system board for an Inboard 386 to work. **You can't disable the
> memory down to 256K bytes.**"*

➡ **The Inboard backfills *above* 256 KB. It does not serve `0x00000-0x3FFFF`.** That is the
product's architecture, not cautious advice, and Intel repeats it for every machine in the
compatibility list.

**2. The remedy Intel offers is a PAL, not a setting.** For one non-compliant board: *"you can
request a **PAL** from Smartec that will allow this."* The bank enable is decode logic. There is no
software route, which is what the owner hoped for.

**3. UniPCemu models no such register.** The only other implementation of this card in existence
(`unipcemu/UniPCemu/hardware/inboard.c`, 257 lines) has exactly three Inboard ports, and
`inboardXT_PortA0` is referenced in **one** place:

```c
if (inboardXT_PortA0 & 0x80) //Mapping the ROM areas low (unconfirmed)?
```

Port `0xA0` bit 7 is **ROM** mapping. `0x670` is wait states plus cache. `0x674` is AT-only. **No
backfill base register exists in the model.** That retires the `0xA0` lead this plan raised an
hour earlier.

### What is left of it, honestly

⚠ **Not disproven, just very unlikely.** Documentation describes the product as sold and a model
implements what its author needed; neither is a schematic. But three independent sources agreeing,
against zero evidence for a software route, is enough to stop spending on it.

✅ **E1 already collected most of this prize anyway.** Conventional memory on the bus went 640 KB
→ 256 KB, a 60% cut. The remaining 256 KB is the part Intel's design reserves for the system
board, and it is out of reach without different hardware.

⭐ **Where it could still go:** @RonnyRoy is
[cloning the Inboard](https://github.com/ronnyroy111/inboard386). A clone is not bound by Intel's
decode. If the floor is a PAL term, a reproduction card could lower it — that is his domain, not a
patch to ours, and it is a hardware project rather than an optimisation.

✅ **And one thing survives regardless:** `gen_ramstride.py`, still unrun. Whether card RAM is
genuinely faster than planar under a cache-defeating access pattern is a fact worth having about
this machine, independent of E7.

### Owner's constraints, 2026-09-21

> *"the board with the current bios needs bank 0 populated, i can't pull the ram from the board -
> also i would rather not remove it but disable via software and that would mean a bios hack. as
> for the revision of the board i'll have to pull the lid."*

1. **Bank 0 stays populated.** The current BIOS requires it, and the owner does not want it out.
2. **Software route preferred**, not chip removal.
3. **Board revision unknown** until the lid comes off - and it decides what bank 0 actually holds.

### ⭐ The reframe those constraints force, and it may be good news

**"Remove the planar RAM" was never the goal.** The Inboard sits **in the CPU socket**, between
the CPU and the bus. If it serves an address out of its own RAM it never issues a bus cycle at
all, and the planar chips simply sit there unread. **Populated-but-unused costs nothing.**

➡ So the objective is not *"disable bank 0"*, it is *"make the card serve `0x00000-0x3FFFF`
locally"*. Whether bank 0 is fitted becomes irrelevant rather than blocking, and no chip has to
come out.

⚠ **Unverified, and our own tools cannot verify it.** `86box_upstream/src/device/inboard386.c`
does **not** model the backfill as a mechanism - conventional RAM there is just `mem_size` on the
machine — so the emulator has no opinion to consult. This has to come from the card.

### ⛔ The timing problem, which is the real obstacle

At power-on the BIOS POST uses `0x00000-0x3FFFF` for the interrupt vector table, the BIOS data
area and its stack **before any software of ours can run**. Reconfiguring the card to serve that
range *after* DOS is up would swap the IVT, the BDA and DOS itself out from under the running
machine.

➡ So it cannot be a driver doing it, and *"a BIOS hack"* is the right instinct for the wrong
reason: the change has to happen **at or before POST**, which means the card's own hardware
configuration or option ROM, not `INBRDPC.SYS`. **Establish whether the backfill base is
configurable at all before designing anything around it.**

### Leads worth pulling first — all offline

- **Intel's Inboard manual.** It documents `EGACACHE` and the shadow behaviour precisely; if the
  backfill base is configurable it will say so. Paths in [[reference-recovered-intel-files-2026-08-03]].
- **UniPCemu's `hardware/inboard.c`** - this project's model is a direct port of it, and it is the
  only other implementation of this card in existence.
- **Port `0xA0`.** Our own `inboard386.c` carries it as *"XT-only port 0xA0 shadow (memory-size/remap
  related)"* and nothing else in this repo has followed that up.
- ⛔ `hardware/pal_gal_reverse_engineering/` is **empty** - the decode has never been analysed.

### Open questions, in the order they gate the idea

1. **Does the backfill decode reach below 256 KB?** — the switch test above. ⚠ `hardware/pal_gal_reverse_engineering/` is **empty**, so no decode analysis exists in this repo to answer it on paper.
2. **Parity.** The XT uses parity RAM and an absent bank can raise NMI. Does the card supply parity for backfilled addresses? Unknown.
3. **Does the 5160 POST require bank 0 at all?** The minuszerodegrees page is 5150. Needs the 5160 case establishing before any chip comes out.
4. **DMA into 0-256 KB once it is card RAM.** Lower risk than it looks: sound already DMAs into card-served memory every time it plays, and has for weeks.
5. **Which BIOS?** This machine requires a **1986** ROM already (`ibm5160_050986` / `ibm5160_011086`); a modified ROM must stay in that family.

### Why it is worth logging even if it never ships

The backfill is **hardware, active at power-on** — POST counts 640 KB before any driver loads, so
`INBRDPC.SYS` is not what creates it. That means the question is purely one of address decode,
and the answer is a fact about this card that is worth knowing whether or not we act on it.

## THE THIRD STRATEGY — don't move the bytes at all

Everything in this document until 2026-09-12 was one of two ideas:

| | strategy | what it attacks | best result so far |
|---|---|---|---|
| 1 | **Widen** the transfer | the `~1.87 us/byte` term | `rep insw`/`outsw` - **35% read, 33% write**, shipped |
| 2 | **Fewer** transactions | the `~3.90 us` fixed sync term | request merging, **1.2-1.5x**, A1, not yet built |

Both accept that the bytes cross the socket and argue about how. The DMA question and the Mach8
question arrived within a day of each other and both point at a third:

| | strategy | what it attacks | |
|---|---|---|---|
| 3 | **Don't move them** | the transfer itself | DMA (A4-A6), on-card blits (A12), memory-mapped storage (A11), refresh (A7) |

**This is the one with the most headroom, because strategies 1 and 2 are bounded by the card's
decode ceiling and strategy 3 is not.** A byte that never crosses the socket costs nothing, and
no amount of widening beats that.

### The question to ask of every device on this bus

We have always asked *"how fast can we push bytes to it"*. The other question, never asked
systematically:

```
What can STAY there, and what can it do without being told again?
```

| device | has its own memory? | can it act unattended? | asked yet |
|---|---|---|---|
| **Mach8 / Graphics Ultra** | ✅ **512 KB or 1 MB VRAM**, ~700 KB of it likely idle | ✅ a real drawing engine - blits with a VRAM source never touch the bus | **A12** |
| **SB Pro** | ❌ no buffer | ✅ plays a whole DMA buffer without per-sample attention | already offloaded; the hazard is 20-bit reach, not speed |
| **3C509B** | ✅ on-card packet buffer | ✅ receives into it without the CPU servicing each byte | ❓ **never examined.** How big, and does the driver drain it in bulk or per-byte? |
| **Trantor T130B** | ❓ pseudo-DMA design, buffer size unknown | ❓ | ❓ **never examined** - and D1/A10 (does `T130.MPD` use string I/O?) is one `pedis.py` command |
| **The whole SCSI chain** - Nakamichi CD changer, Yamaha CD-RW, Fujitsu MO, Iomega Zip, HP DAT | ✅ **every one has a cache**; period CD drives carry 128-256 KB | ✅✅ **SCSI is built for this** - a device takes a command, **disconnects from the bus**, does the work, and reconnects when ready | ❓ **A15/A16 - never examined, and this is the most on-point mechanism in the machine** |
| **Floppy (Sergey / 765)** | ✅ sector buffer | ✅ DMA channel 2 | partly - A3/A4 use it as the measuring instrument |
| **XT-CF (Lo-tech)** | the CF card has its own sector buffer; the ISA side is a dumb latch | ❌ PIO only, no DRQ/DACK | closed - this is why A4 cannot help the boot disk |
| **LS-120 / EPAT** | ✅ drive buffer behind the bridge | ⚠ the bridge's DMA path writes `0x22`/`0x23`, which alias onto the 8259 | ❌ **do not use** - that is the #22 keyboard-killer, in the transfer path |
| **Inboard itself** | ✅ local RAM + cache | ✅ this is *why* DMA may overlap with CPU work here (A6) | **A6** |

Three of those rows say **"never examined"**, and two of the three cost nothing but a
`pedis.py` run against a binary we already hold.

## THE PER-COMPONENT AUDIT — a session of its own, one device at a time

**Owner's decision, 2026-09-12: this is not a "now" question.** It gets its own session, or
several, driving each piece of hardware in turn. This section is the method and the register, so
that session starts with a procedure rather than a blank page.

The Mach8 audit on 2026-09-12 is the **worked example** and it took under an hour, entirely
offline against binaries already on the card. It answered four questions, corrected one of my own
estimates, retired one open question, and settled half of an unrelated issue (#8) as a
side-effect. That is the shape to repeat.

### Why one device at a time, and why it pays

We optimised **one half of one device** — the XT-IDE data path — and got 35%. Every device here
has two halves:

```
the CARD half   : what the hardware can be made to do   (decode width, engine, buffer, cache)
the MACHINE half: what the driver actually asks it to do (string I/O, merging, caching, offload)
```

The XT-IDE win came from the machine half. **We have never run that pass on anything else.**

### The six questions to ask of every device

Ask all six. Record the answer even when it is "no lever" — a negative result written down once
is cheaper than re-deriving it (this document already has a "ruled out, with reasons" section for
exactly that).

| | question | how to answer it |
|---|---|---|
| **Q1** | Does the driver use **string I/O** for bulk data, or a byte loop? | count `F3 6D/6C/6F/6E` in the binary |
| **Q2** | How many **transactions** per unit of work — can they be merged? | traffic capture (technique 93), then command-setup vs data time |
| **Q3** | Does the device have **memory of its own**, and is it used? | datasheet / registry / binary; then a dynamic test |
| **Q4** | Can the device **act unattended** — DMA, disconnect, a command engine? | binary inspection for the enabling bit, then measure |
| **Q5** | Is there a **width or decode ceiling** that caps the card half? | the card's bus width vs the slot's; do not credit width gains past it |
| **Q6** | What does the driver **poll**, and how often? | every polled register costs the same `~5.55 us` as a real byte |

### Register — where each device stands

**Updated 2026-09-21.** Every driver we touch gets a row, not just the storage ones — the owner's
rule: *"every driver we touch"*, and *"sub-1% changes are worthwhile, they collectively add up"*.

⚠ **This register is the authority, not the ACTIONS table below it.** On 2026-09-21 the two
disagreed — this said the T130B's Q1 was done on 09-20, ACTIONS said "never run" — and the work
was redone. If they conflict again, believe this one and fix that one.

Q1 string I/O · Q2 transactions · Q3 own memory · Q4 unattended · Q5 width ceiling · Q6 polling.

| device / binary | ours? | Q1 width | Q6 polling | what is left |
|---|---|---|---|---|
| **XT-CF / XT-IDE** (`XTIDEMP.MPD`) | ✅ we wrote it | ✅ `rep insw` one per sector, **35%/33% shipped** | ✅ **paced, shipped** (`XT_POLL_BACKOFF`) | **A1 request merging, 1.88x.** A2 folded in — the loop is already one `rep`, what is left is per-**sector** setup, which only merging removes |
| **Trantor T130B** (`T130.MPD`) | ❌ Adaptec's — **patch approved 2026-09-21** | ✅ already `...BufferUshort`. Pseudo-DMA at **`base+4`**, 128 B/call. ⭐ **dword candidate**: registers start at `base+8`, so `base+5..7` are clear — unlike XT-IDE | ⚠ **10 of 28 `ReadPortUchar` sites in tight read/test/jump-back loops**, 12 `StallExecution`. Partly paced, partly not | **Pace the tight loops** (the XT-IDE fix). Test dword second |
| **3C509B** (`ELNK3.VXD`) | ❌ stock MS/3Com | ✅ **already bulk at dword** — `shr ecx,2` / `rep insd` | ⛔ **27 flat-out spin loops**, confirmed at `0xd03`. **40x our VxDs' density** | Buffer **size** still unknown; ❌ the QEMU model cannot answer it |
| **Floppy** (`HSFLOP.PDR`) | ✅ **we patch it** (`maxPhys`) | ❓ unexamined | ⛔ **polls hard during a seek** | ⭐ **The most tractable pacing target we already own.** Same fix, same measured basis |
| **Mach8** (display drivers) | ❌ stock ATI | ✅ `rep outsw` (43 sites) | ❓ | A12a mode-as-lever, A12b glyph cache |
| **SB Pro** (`MSSBLST.VXD`) | ✅ **we patch it** | n/a — DMA path | ❓ does the VxD poll the DSP? | 20-bit reach fixed. **Q6 never asked** |
| **Keyboard** (`VKD.VXD`, `KEYBOARD.DRV`) | ✅ **both ours** | n/a | ✅ **1 and 2 candidates — clean** | ✅ A19 closed. No lever, despite the hot path |
| **`VDMAD.VXD` / `VPICD.VXD`** | ✅ **both ours** | n/a | ✅ **1 candidate each — clean** | ✅ A19 closed. No lever |
| **`INBRDPC.SYS`** | ✅ we patch it | n/a | n/a | Measured: slows floppies only. ⛔ Required, never disable |
| **LS-120 / EPAT** | vendor driver ships | ✅ EPP, 75-99 KiB/s | ❓ | ✅ **Done.** Our miniport retired 2026-09-21 |
| **The SCSI chain** (6 targets) | — | — | — | **A16 mode page 8** — caches per target, nobody has looked. ⚠ async may beat sync (BlueSCSI) |
| **Inboard itself** | ✅ | — | — | **A3 correctness**, A6 DMA/CPU overlap, A7 refresh |

⭐ **The pattern across the table**: of the four data paths audited for width, **three were already
optimal** and only XT-IDE needed the fix. **Polling is the opposite** — only XT-IDE is paced, and
every other driver is either unexamined or known to spin. That is where the remaining free wins are,
and it is why the owner's *"bus polling is the big lever"* is the right read.

Cost of one poll, measured 2026-09-10: **5.55 us of bus moving nothing**, against **0.22 us** for a
cache-resident delay that occupies **no bus at all**. A 25x difference, and it is free only because
the Inboard has a cache — on a stock XT it would not be.

### Order for that session

Cheapest evidence first, and **everything in the first group needs no hardware and no boot**:

1. **Offline binary pass** — `T130.MPD` (A10/A14), the 3C509B packet driver (A13), and the
   disconnect bit in `T130.MPD` (first half of A15). All `pedis.py` against files already held.
2. **One DOS run** — SCSI mode page 8 across all six targets (A16), DMA channel inventory (A5).
3. **Correctness before speed** — A3, can the 8237 reach the 384 KB E1 moved onto the Inboard.
4. **Then the measured levers** — A1, A2, A12a.

⚠ **Do not start this session until #22 is closed.** The LS-120 is the deliverable; this is the
track that runs alongside it, and the owner has said so twice.

## ACTIONS OUTSTANDING — the single list to work from

Every open optimisation question, as an **action with a method and a cost**, not a note. Nothing
here is closed by reasoning alone: each row ends in a measurement or an inspection of a binary.

**Order is by (evidence available) x (cost to get it), not by size.** A big lever with no way to
measure it ranks below a small one we can settle this week.

| # | Action | Method | Cost | Status |
|---|---|---|---|---|
| **A1** | Request merging in `XTIDEMP.MPD` | Raise `MaximumTransferLength`, coalesce adjacent SRBs; re-run the technique 93 traffic capture | driver build + 1 boot | 🎯 **next after LS-120** — 1.2-1.5x, measured not modelled |
| **A2** | The free 4% — transfer-loop overhead | `rep movsd` in the buffer paths; unroll the per-sector loop | driver build | 🎯 queued behind A1 |
| **A3** | E5c - can the 8237 reach the 384 KB E1 moved onto the Inboard? | — | — | ✅ **RETIRED 2026-09-21, not measured - answered by operation.** The owner: *"it works and has been for a few weeks, sound works fine."* Sound DMAs into conventional memory on every play; weeks of clean audio since the switches changed on 09-11 is the answer. ⚠ Strictly it proves DMA reaches *where those buffers landed*, not all of `0x40000-0x9FFFF` - but no outcome of a probe changes a decision, which is the same reason the strided E1 retest was deliberately not run |
| **A4** | **E5 — time a real DMA transfer** | PIT harness (technique 109) around a one-track floppy DMA read; us/byte against PIO `1.87` and memory-mapped `0.454` | 1 DOS run | ❓ answers @andrew-hoffman's actual question with a number |
| **A5** | **E5b — DMA channel inventory** | Confirm 0 refresh / 1 SB Pro / 2 floppy / **3 free**. Read the 8237 mask + mode registers and the installed device list | minutes, DOS | ❓ an idle channel on a bus-bound machine is worth knowing about |
| **A6** | **E5a — does DMA overlap with CPU work here?** | Run a CPU-bound loop timed by the PIT, with and without a concurrent floppy DMA transfer. If the loop time is unchanged, the Inboard really does keep running from cache while the 8237 holds the bus | 1 DOS run | ❓ Inboard-specific, and it is *why* DMA is worth more here than on a stock XT |
| **A7** | **E6 — DRAM refresh tuning** (new, 2026-09-12) | See below. Reprogram PIT channel 1, memory-pattern test, PIT-timed throughput A/B | 1 DOS run, instantly reversible | ❓ **a tax every device pays**, and E1 just changed the premise |
| **A8** | **D3 — is the Mach8 accelerator actually being used?** (elevated 2026-09-12) | `pedis.py` on the shipped display drivers: count framebuffer writes vs `0x9AE8` command writes | no hardware at all | ❓ **potentially the largest single saving on the machine** — see below |
| **A9** | E2 — what is already shadowed | Read the shadow config and confirm rather than assume | minutes | ❓ |
| **A10** | D1 — does `T130.MPD` use string I/O? | `pedis.py T130.MPD io` | minutes, no hardware | ❓ one command, never run |
| **A11** | E4a — memory-mapped storage (JR-IDE/ISA) | 86Box already models it — prove the 4.2x before buying hardware | emulator run | ❓ |
| **A12** | **The Mach8 as a coprocessor** (owner's lead, 2026-09-12) | — | — | ✅ **MEASURED offline 2026-09-12.** Accelerator installed, selected and used (651 register loads, 63 CMD). `rep outsw` already in use. Card has **1 MB**. See A12 below |
| **A12a** | **Display mode as a bus lever — 1024x768x256 today** | Benchmark at 1024x768 vs 800x600 vs 640x480 on the real machine | Control Panel + a benchmark run, no code | 🎯 **~2.6x fewer bytes at 640x480, and it frees ~470 KB more off-screen VRAM.** Owner's call - it is a trade |
| **A12b** | Does the driver cache glyphs/bitmaps in off-screen VRAM? | Dynamic: time a repeated text draw, then repeat after a mode change that halves off-screen memory | 1 boot | ❓ static counts cannot answer it |
| **A13** | 3C509B packet buffer | ✅ **DRAINED IN BULK, dword** - `ELNK3.VXD` `shr ecx,2` / `rep insd`, 2026-09-21. Buffer **size** still open; the QEMU model cannot give it | - | ✅ half closed |
| **A14** | **T130B — what does the card hold, and does `T130.MPD` use string I/O?** | `pedis.py T130.MPD io` (this is A10), plus the card's pseudo-DMA buffer size | no hardware | ❓ never examined |
| **A15** | ⭐ **Does the SCSI chain DISCONNECT, or does it hold the bus through every seek?** | Check `T130.MPD`'s identify-message handling and the registry/INF for a disconnect setting; confirm on the wire by timing a seek-heavy read while another device transfers | `pedis.py` first, then 1 boot | ❓ **the single most on-point lever found so far** - see below |
| **A16** | **SCSI cache mode pages (page 8) across the chain** | `MODE SENSE` page 8 on every target: read-cache enable, write-cache enable, prefetch. Then `MODE SELECT` to turn on what is off | 1 DOS run, no code | ❓ settable per device, and nobody has looked |
| **A17** | ⭐ **Pace `HSFLOP.PDR`'s seek polling** | Same shape as `XT_POLL_BACKOFF`: spin briefly, then a register-only cache-resident delay | binary patch + 1 boot | ❓ **a driver we already patch**, and it is named in this plan as the most tractable pacing target. Never started |
| **A18** | ⭐ **Pace `T130.MPD`'s tight poll loops** | 10 of 28 `ScsiPortReadPortUchar` sites sit in read/test/jump-back loops | binary patch + 1 boot | ✅ **owner approved patching this binary 2026-09-21.** Likely worth more than A15 disconnect: disconnect frees the SCSI bus, this frees the ISA bus |
| **A19** | Q6 on the VxDs nobody had asked | ✅ **DONE 2026-09-21, and it is a NEGATIVE - write it down rather than re-derive it.** Poll-loop density (port read + backward Jcc): `VKD` **1** in 45 KB, `VDMAD` **1** in 42 KB, `VPICD` **1** in 47 KB, `KEYBOARD.DRV` **2** in 13 KB. **None of our four VxDs is a polling target.** Control: `T130.MPD` scores 0 by this method because it polls through SCSIPORT helpers, not raw opcodes - which is why it needs call-site counting instead | offline | ✅ closed |
| **A21** | ⭐ **`ELNK3.VXD` spins flat out on the NIC** (NEW, 2026-09-21) | **27** candidate poll loops in 30 KB - **40x our VxDs' density**. Confirmed by disassembly at `0xd03`: `in ax,dx` / `test ah,0x10` / `jne` back, twice in a row, **no delay of any kind**. Each iteration is ~3.82 us of bus moving nothing | binary patch + 1 boot | ❓ stock 3Com driver, we do not patch it today. ⚠ **Unquantified** - bursty, and the NIC may complete fast. Quantify with the B2 PIT harness during network traffic before patching anything || **A20** | **dword on the T130B pseudo-DMA port** | `base+4` is a single address and the register file starts at `base+8`, so `base+5..7` are clear - unlike XT-IDE. Word → dword is 3.819 → 3.183 us/byte | binary patch, after A18 | ❓ **candidate, not a finding** - the port handshakes on DRQ and a handshaked cycle may not amortise |
| **A21a** | ⭐ **Model the 3C509B in 86Box** (owner's point, 2026-09-21) | Currently the bed has no NIC, so **A21 cannot be tested in emulation at all** - any pacing patch to `ELNK3.VXD` would have to go straight to the real machine. Modelling the card makes the bed match the machine and gives the patch somewhere safe to fail | emulator work | ❓ **this is [#20](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/20), reframed.** It was filed as *fidelity only*; it is also the **prerequisite for testing A21**. Base any port on QEMU's 3C509B (**MIT**, Antony T Curtis), never on `net_3c503.c` |
| **A22** | ⛔ **WITHDRAWN 2026-09-21, same day it was written.** I proposed "set SW1-3/4 to 64 KB and see what POST counts" as E7's cheap first step. **There is no such setting.** On a 5160 SW1-3/4 = ON/ON means *enable only bank 0*, the switches are **already there**, and on the 256-640 KB board revision bank 0 **is** 256 KB. So the minimum is already set, POST already counts 640 KB, and the card already backfills 256-640 KB. Bank 0 can only be removed **physically** - which is exactly E7 | — | ⛔ no cheap version exists |
| **A23** | ⭐ **The CPU upgrade module: the registers we never explored, and its switches** (owner, 2026-09-21) | We matched what `REVTO486` set and fixed the one register that was wrong (`CMLR`). **We never asked what else is available.** ⭐ One lever is already written down as untested: **`1000h:1` bit 4 (`XTOUT`) is SET and feipoa recommends `0`** - recorded 2026-09-12 as *"separate, minor, untested here"*. Unexplored besides: `1000h:2` = `00` (purpose never recorded), **LMROR** `1001h:2/3` = `0000` (no region marked read-only), and **SNP snooping is disabled** while ASNP is enabled. The module also has **physical switches** | one boot each, reversible | ❓ **[#40](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/40)**, logged 2026-09-21. Precedent is strong - `CMLR` was one register and took Dhrystone from **2 to 13-15** - but that was pathological (the cache covered nothing Windows used), so do not expect that size again |

---

## A12. The Mach8 is a coprocessor with its own RAM — the owner's lead, 2026-09-12

> *"we know the mach 8 is special as it has two processors... modern systems offload to GPUs and
> here we have the grandaddy of them all sat in this machine half the time underutilised - could
> it have some capability to do things by itself, handle instructions from the CPU, store to RAM
> and do some of the workload without needing to be told again"*

**Not outlandish. That is close to exactly what it is**, and it is the only device in this
machine with meaningful RAM on the *far* side of the bus bottleneck.

### The card really is two machines

The ATI Graphics Ultra carries **two independent engines**:

| half | what it is | how the CPU reaches its memory |
|---|---|---|
| **VGA side** | ATI 28800 SVGA | memory-mapped aperture at `A0000`, banked 64 KB at a time |
| **Mach8 side** | an 8514/A-compatible **drawing engine** | **I/O only**, through `PIX_TRANS` — it is not memory-mapped at all |

That second row is Necasek's finding, already recorded in
`docs/mach8_real_hardware_registers_2026_09_08.md`. Two halves, two memory paths, two costs.

### What it can genuinely do on its own

From 86Box's own implementation of the engine (`vid_ati_mach8.c`), the drawing commands carry
**both a source and a destination inside VRAM** — `src_pitch`/`dst_pitch`, `src_x`/`dest_x` — and
a separate `cpu_input` flag:

```
cpu_input = 1   ->  pixel data comes from the CPU, across the bus, through PIX_TRANS
cpu_input = 0   ->  source is already in VRAM: the card moves it INTERNALLY
```

**`cpu_input = 0` is the whole prize.** A screen-to-screen blit, a rectangle fill, a pattern
fill, a line — the pixels never cross ISA. The CPU writes a handful of command registers and the
card does the work while the CPU goes elsewhere.

So the honest version of the GPU analogy:

- ❌ It cannot run arbitrary code. No program counter, no branching, no general compute. It is a
  **fixed-function** drawing engine, not a shader.
- ✅ But *"upload once, reuse many times out of local memory, driven by short commands"* is
  precisely the economic model that makes a GPU worth having — and on a machine where every byte
  across the socket costs **~5.55 us**, that model is worth more here than it was on the 486s
  these cards shipped with.

### This is not a 5 MB machine. It is a 6 MB machine with 1 MB stranded

> *"we said we have a system with 5 MB but actually another whole 1 MB sat idle on a VGA card is
> madness"*  — owner, 2026-09-12

The Inboard carries **5120 KB**. The Graphics Ultra carries up to **another 1024 KB**. Nobody has
ever counted the machine that way, and once you do, **a sixth of the RAM in this computer is
doing nothing** — while the desktop uses perhaps 300 KB of it.

⚠ **But be precise about what that MB is worth, because the obvious idea is the wrong one.**

A RAM disk in video memory is a real old trick and it is **not** the win here. VRAM is reachable
only across the socket — the banked `A0000` aperture or `PIX_TRANS` — so every byte costs bus
time, while main memory now sits **on the Inboard** after E1 and costs almost nothing. As general
storage the stranded megabyte is *slower than the RAM we already have*, and we are not short of
that.

Its value is specific and it is large: **it is the only place to put bytes where using them again
costs nothing.** A glyph, a brush, a bitmap, a saved screen region cached off-screen is drawn by
an on-card blit — the pixels never cross the socket a second time. Every other megabyte in this
machine is on the CPU's side of the bottleneck. This one is on the far side, which is worthless
for storage and precisely right for a cache.

So the question is not *"what else could we store there"*. It is **"is the display driver using
it at all, and if not, what is it re-sending across the bus that it need not?"**

## A12 — MEASURED 2026-09-12, offline, against the shipped binaries

No hardware, no boot. `ATIM8.DRV` and `SYSTEM.DAT` read straight off the card at `D:`.

### 1. The accelerator is installed, selected, and genuinely used ✅

| evidence | result |
|---|---|
| Registry references to `ATIM8.DRV` | **7** (and `ATI.VXD` 3, `VGA.DRV` only 1) |
| `SYSTEM.INI` | `ATI mach8: 1024x768x256 (Large Font)` |
| Accelerator register loads in `ATIM8.DRV` | **651** |
| `0x9AE8` CMD writes | **63** |
| `0x82E8`/`0x86E8` CUR_Y/CUR_X | 137 / 147 |

**So "turn the accelerator on" is already done.** ATI's accelerated driver is the selected
display driver and it drives the engine hard. That closes the worry and moves the question on.

### 2. Bulk pixel transfer already uses string I/O ✅

| opcode | count |
|---|---|
| `rep outsw` | **43** |
| `rep outsb` | 2 |
| `out dx,ax` / `out dx,al` | 1645 / 1811 (command-register setup, as expected) |
| `mov ax,A000h` — the VGA aperture | **1** |

ATI knew what they were doing: the data path is `rep outsw` to `PIX_TRANS`, not a byte loop. The
same lever that won 35% on the XT-CF is **already pulled here**.

That last row also confirms Necasek independently from the binary: the Mach8's VRAM really is
reachable only through `PIX_TRANS`, so **E4's 4.2x memory-vs-I/O advantage cannot be claimed for
video**. There is no aperture to move to. ⚠ That retires the "which of the two memory paths does
it use" question I raised above — it uses the only one it has.

### 3. ⚠ CORRECTION — the off-screen memory is ~256 KB, not ~700 KB

I estimated ~700 KB idle by assuming a 640x480 desktop. The machine actually runs
**1024x768x256**, which is **786,432 bytes of visible framebuffer**.

Two consequences, and the second is a real finding:

- A 512 KB card **cannot** display that mode, so **the card has 1 MB installed**. That settles
  half of the #8 experiment from the desktop, with no hardware.
- Only about **256 KB is off-screen**, not 700 KB. The stranded megabyte is mostly *in use* —
  as framebuffer.

### 4. 🎯 The lever that is actually left: pixels per operation

With the accelerator on and `rep outsw` already in use, the remaining bus cost is **how many
pixels cross the socket**, and that is set by the display mode:

| mode | framebuffer | relative bytes for a full-screen operation | off-screen VRAM freed |
|---|---|---|---|
| **1024x768x256** (current) | 786 KB | **1.00x** | ~256 KB |
| 800x600x256 | 480 KB | **0.61x** | ~560 KB |
| 640x480x256 | 300 KB | **0.38x** | ~724 KB |

Dropping to 640x480 is **~2.6x fewer bytes** for anything the CPU has to feed, and it
simultaneously frees ~470 KB more off-screen VRAM for the driver to cache glyphs and bitmaps in —
which reduces the traffic *again*. The two gains compound.

⚠ **This is a trade, not a free win**, and it is the owner's call: 1024x768 is the 8514/A's
native mode and the reason for buying the card. But for **games and benchmarks**, where the
measure is frames and not desktop area, it is very likely the single largest video lever
available — and it is a Control Panel change, not a patch.

### 5. What is still worth measuring

- **Does the driver cache in off-screen VRAM?** 63 CMD sites is consistent with blits, but static
  counts cannot prove a glyph cache. The test is dynamic: time a repeated text draw, then the
  same draw after a mode change that halves off-screen memory.
- **The 8-bit slot ceiling.** The Graphics Ultra is a 16-bit ISA card in a machine with **only
  8-bit slots**. `rep outsw` saves CPU instructions but the bus still moves 8 bits per cycle, so
  the card's decode ceiling applies here exactly as it did to the XT-CF. Confirm before
  attributing any gain to width.
- **Benchmark before and after**, on the real machine. Everything above is static analysis.

### The resource nobody has counted

At **1 MB installed** and a 640x480x8 desktop using ~300 KB, roughly **700 KB of VRAM sits
idle** — on the far side of the bottleneck, already paid for, doing nothing.

That is what an accelerated Windows driver is supposed to use it for: caching font glyphs,
brushes and bitmaps off-screen, then blitting them on-card. Every glyph cached is a glyph never
re-sent across the bus.

⚠ **This makes the 512 KB / 1 MB question matter twice over.** It is already the open experiment
on [#8](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/8) for TC1995's self-test
bug — and it also decides how much off-screen memory exists to exploit. One config change now
answers two separate questions.

### The asymmetry worth measuring — and it ties straight into E4

**E4 measured that memory-mapped access beats I/O-mapped by 4.2x on this machine**
(0.454 vs 1.910 us/byte). The Mach8's two halves are exactly that comparison, on one card:

- the VGA aperture at `A0000` is **memory-mapped**
- `PIX_TRANS` is **I/O**

If the display driver pushes bulk pixels through `PIX_TRANS` when it could use the aperture, it
is paying the 4.2x penalty E4 identified — on the largest byte-mover in the machine. Nobody has
checked which it does.

### Actions

1. **Read the installed VRAM size** off the real card, and compute the off-screen remainder.
   Also settles half of the #8 experiment.
2. **Inspect the shipped drivers offline** (`MACHW3.DRV`, and the Win95 ATI driver on the card).
   Three counts, no hardware needed: framebuffer-aperture writes, `PIX_TRANS` writes, and
   accelerator command writes around `0x9AE8`. That tells us whether the accelerator is used at
   all, whether off-screen caching happens, and which of the two memory paths carries bulk data.
3. **Only then** consider whether anything of ours should use the card deliberately.

⚠ Same caveat as A8: **measure against the binaries and the real card, not 86Box**, while TC1995
is still fixing the emulated accelerator path (#8).

### The wider principle this is an instance of

Every device on this bus has been treated as a *destination for bytes*. The Mach8 is the first
one identified that is also a **place to leave them**. Worth asking of the others before assuming
they are not: the 3C509B has a packet buffer, the T130B has its own logic. The question is not
"how fast can we push bytes there" but **"what can stay there, and what can act on it without
being told again"**.

## A15/A16. The SCSI chain — the devices that can already work unattended

> *"we have RAM also in the CD drives too, something that can buffer commands"*  — owner, 2026-09-12

Correct, and it is stronger than it first sounds. This machine has **six SCSI targets** hanging
off the T130B, and SCSI is the one bus here that was *designed* around devices going away and
coming back.

### A15. Disconnect/reconnect — this is the mechanism, and it is free if it is already on

A SCSI target may accept a command, **release the bus entirely**, perform the seek or the spin-up
in its own time, and then reconnect when it has data. On a machine where **the bus is the
bottleneck**, that is not a throughput optimisation - it is the difference between a 100 ms
Nakamichi seek costing the whole system 100 ms of bus, or costing it nothing.

**And it is commonly turned off.** Disconnection forces a driver to handle reselection, save and
restore per-target state, and cope with commands completing out of order; plenty of simple
miniports disable it to avoid exactly that. `T130.MPD` is Adaptec's own binary, PIO-only,
configured here with **`Polling=1` and no IRQ** — which is precisely the configuration where a
lazy implementation would refuse to disconnect, because with no interrupt there is nothing to
catch a reselection.

⚠ So the honest expectation is that it is **probably off**, and if so it may not be switchable
without a driver change. **Find out before theorising** — `pedis.py` on `T130.MPD` will show
whether it ever sends an IDENTIFY message with the disconnect-permitted bit set.

### Why it matters more here than anywhere else

Every other lever in this document fights for a share of the bus. Disconnect **hands the bus
back** for the duration of the slowest thing any device does — a seek, a CD changer swapping
discs, a tape streaming. The Mach8 blit idea (A12) is the same shape applied to video; this is it
applied to the six devices that spend most of their time mechanically busy.

### A16. Cache mode pages — adjustable, per device, today

Every one of these targets carries a cache, and SCSI exposes its controls as **mode page 8**:

| field | what it does |
|---|---|
| `RCD` | read cache disable - if set, the drive's cache is being wasted |
| `WCE` | write cache enable - off by default on many period drives |
| prefetch / read-ahead limits | how much the drive reads speculatively into its own RAM |

`MODE SENSE` page 8 on each target says what is currently set; `MODE SELECT` changes it. **No
code to write, no driver to patch** — one DOS run with the existing ASPI tooling reads all six.

⚠ **`WCE` is a data-integrity trade, not a free win.** A drive that acknowledges a write before
it reaches the medium will lose it on a power cut. Read caching and prefetch are the safe half;
treat write caching as a separate decision and record it as one. This machine already has an open
data-loss issue ([#18](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/18)) and
nothing here should make that murkier.

### The same question applies to ATAPI

The LS-120 is ATAPI, which inherits SCSI's mode pages. Once the miniport works, the same
`MODE SENSE` page 8 question applies to it — and the drive's own buffer is what makes the
parallel-port transport bearable at all.

## E6. DRAM refresh — a tax every single device pays  *(new, 2026-09-12)*

**Thinking about the socket rather than the card.** What else is holding the ISA bus, all the
time, that nobody has costed?

On an IBM 5160, **PIT channel 1 pulses roughly every 15.09 us and triggers DMA channel 0 to run
a dummy read cycle**, refreshing one DRAM row. That is a bus cycle stolen from everything -
every PIO access, every DMA transfer, every instruction fetch that reaches the planar. It is
paid whether or not anything else is happening, and it is the one overhead in this document that
is **not** attributable to any driver.

### Why E1 changed the premise

`SW1-3/4` are now **ON**: 384 KB of the 640 KB no longer lives on the planar, and the board is
down to a 256 KB planar bank. The Inboard's own RAM has its own refresh arrangement and is not
serviced by channel 0.

So there is now **less planar DRAM behind that refresh cycle than the timer was set for**, and
the timer is a PIT reload value we can write.

### The action

1. Record the current PIT channel 1 reload value (do not assume 18; read it).
2. Memory-pattern test the planar region: fill, wait, verify. Establish a clean baseline.
3. Lengthen the reload, re-run the pattern test, and A/B a PIT-timed throughput measurement.
4. Back out immediately on a single bit error.

**Reversible in one `out` instruction, and it costs one DOS run.** ⚠ Get it wrong and DRAM rows
decay - this is a *correctness* risk, so the pattern test is not optional and must run long
enough to matter. Refresh must still cover every row inside the chips' retention spec.

⚠ **Unverified assumption to check first:** that channel 0 refresh services only the planar and
not the Inboard's backfill. If the Inboard *does* depend on it, this lever is closed and the
answer is worth having written down.

---

## D3 elevated. The accelerator is a way to get pixels off the bus entirely

Our own ranking says video is *"the most bytes on the bus of anything here - the framebuffer is
on ISA, so every pixel crosses"*. At a measured `~5.55 us` per 8-bit access, that is the single
largest consumer on the machine, and it has sat uncosted while we tuned storage.

**The lateral move is the same one as DMA: stop moving the bytes.** The Mach8 is an
*accelerator*. A bitblt or a rectangle fill issued through the command registers moves pixels
**inside the card** - they never cross the ISA bus at all. A scroll, a window drag, a fill, a
blit: all of them are either a few command-register writes or thousands of framebuffer writes,
depending entirely on whether the display driver bothers.

### The question, and it needs no hardware to answer

**Do the shipped display drivers actually use the accelerator, or do they write pixels?**

`pedis.py` will answer it offline against the binaries already in this repo
(`mach8_w31_display/MACHW3.DRV` for 3.x, and the Win95 ATI driver on the card): count writes to
the framebuffer aperture against writes to the accelerator command registers around `0x9AE8`.

- If it is already accelerated, we say so and drop it - and we have costed the biggest consumer
  on the bus, which is worth doing regardless.
- If it is writing pixels, the ceiling on this machine is much higher than anything storage can
  offer, and it becomes the main event.

⚠ Note the interaction with **#8**: TC1995 is currently debugging the Mach8 accelerator path in
86Box (his ADD MIX / `mix op 0x13` comment). If the emulated accelerator is not faithful yet, a
result measured in 86Box would be measuring his bug, not our driver. **Measure this against the
binaries and the real card, not against emulation**, until #8 closes.

## AGREED NEXT WORK — after the LS-120, in this order

**Updated 2026-09-12:** @andrew-hoffman's DMA question (**E5**) is added as item 3. It goes
after the two measured levers because those have numbers attached and this one does not yet,
but its measurement is the cheapest on the list — one DOS timing run against the floppy
controller, which already does DMA on this machine — and if it confirms, it is the largest
single lever here because it attacks the fixed per-access sync cost rather than the per-byte
one.

3. **E5 — time a real DMA transfer** and compare against `1.87 us/byte` PIO. Converges with
   **E4/E4a** if it confirms.

## The original order, 2026-09-11

Owner's decision. Both are host-side, both work on the present hardware, neither needs a
different card.

### 1. Request merging — 1.2x to 1.5x, measured not modelled

On the traffic technique 93 recorded (603 commands, 1,149,041 bytes) the shipped driver
spends **2,515 ms of command setup against 4,389 ms of data - 36% overhead**:

| | commands | bytes/cmd | overhead | gain |
|---|---|---|---|---|
| as shipped | 603 | 1,906 | 36% | - |
| merge x2 | 301 | 3,817 | 22% | **1.22x** |
| merge x4 | 150 | 7,660 | 12% | **1.38x** |
| 64 KB commands | 35 | 32,830 | 3% | **1.52x** |

Multiplies with the 35% word-transfer win rather than overlapping it: one attacks bytes per
transaction, the other transactions per request.

⚠ **`XT_SG` defaults to 0 because of this and we never fixed the cause.** With scatter/
gather on we measured **1,104 commands for 975 KB** against 603 for 1,149 KB - one taskfile
command **per descriptor**. Turning SG off stopped the bleeding; coalescing the descriptors
is the actual fix, and it is the same work as merging.

Needs async completion -> a queue -> merge. **That is the same restructure the LS-120 needs**
(`ScsiPortNotification` + `RequestTimerCall`), so doing the LS-120 first builds the
machinery rather than delaying this.

### 2. The free 4% — transfer-loop overhead

Technique 109 measured a transfer as **96% bus, 4% our loop**. Removing the loop overhead is
4% that costs nothing and compounds with everything above. Includes `rep movsd` for buffer
copies in Inboard-local RAM (0.135 us/byte): **`XTIDEMP.ASM` currently has ZERO string
operations and 9 byte-move lines; `XTIDETR.ASM` has 4 against 20.**

Do not defer this because it is small - that is the anti-pattern at the top of this file.

---

# THE COMPLETE LEDGER — every percent identified, nothing omitted

Compiled 2026-09-11 at the owner's request: *"I don't want to miss a single percent that we
can go after."* Every lever found so far, both sides of the connector, with its measured or
estimated size and honest status. **A lever with no number is not thereby small** - it is
uncosted, and costing it is itself a task.

## A. XT-IDE / XT-CF — the card side

| # | lever | size | status |
|---|---|---|---|
| A1 | `rep insw`/`outsw` on stride 2 | **35% read / 33% write** | ✅ **shipped** |
| A2 | Paced polling (`XT_POLL_BACKOFF`) | frees bus for every other card; invisible in a single-driver benchmark | ✅ **shipped** |
| A3 | `rep insd` (32-bit string I/O) | **+34%** (2.85 vs 3.82 us/byte) | ❌ **blocked** - stride 2 still decodes A1, `base+2` is Error |
| A4 | `READ MULTIPLE` | 0 | ❌ drive reports word 47 = 1, unsupported |
| A5 | Taskfile widening | 0 | ❌ 11.38 vs 11.54 us - noise |

## B. The Inboard — the host side of the connector

| # | lever | size | status |
|---|---|---|---|
| B1 | **Request merging** | **1.22x / 1.38x / 1.52x** at x2 / x4 / 64 KB commands. 36% of disk time is command setup | ❌ **not started - biggest remaining** |
| B2 | Scatter/gather descriptor coalescing | re-enables `XT_SG`; today it costs **1,104 commands for 975 KB** vs 603 for 1,149 KB | ❌ same work as B1 |
| B3 | Transfer-loop overhead | **4%**, free, compounds with everything | ❌ not started |
| B4 | `rep movsd` for buffer copies in local RAM | **0** | ⛔ **CLOSED 2026-09-20 - there are no bulk byte-copies to convert.** The line counts were a grep, not a reading. Every hit is init-time or dead: `xvi_loop`/`xci_loop` validate and byte-swap the 40-byte IDENTIFY model string **once per probe**; `xrd_pio8_loop` is the fallback compiled out when `XT_WORD_XFER` is on; `xrd_latch_loop` is the `XT_TR_LATCH` transport, and **this card has no high-byte latch**. The two real `rep` sites in `XTIDEMP.ASM` zero a 60-byte struct once and copy a <=36-byte INQUIRY reply. **The bulk paths already use `rep insw`/`outsw`.** Do not re-open. |
| B5 | Compression (32-bit DriveSpace) | ~480 spare CPU cycles per bus byte | ⏸ parked on **risk and footprint**, not architecture (Andrew corrected our reason) |
| B6 | Read-ahead into Inboard RAM | moves bus work off the critical path | ❓ **uncosted** |
| B7 | The VxD layer | unknown | ❓ **unmeasured.** DDK ships debug builds + symbols for IOS/SCSIPORT/DISKTSD/VMM plus `WDEB386` - still unopened |

## C. The card's own BIOS — the real-mode INT 13h path

| # | lever | size | status |
|---|---|---|---|
| C1 | Does XT+ take the `insb` or `insw` routine for XT-CF? | **up to 2x on every DOS and boot-time transfer** | ❓ **unknown - `XTIDECFG` device type answers it.** Both routines are in the flashed ROM |
| C2 | Custom XUB using 386 instructions on XT-class hardware | uncosted | ❓ ambitious; XUB is open source, 86Box can test a custom ROM safely |
| C3 | XT+ reflash itself (186 string I/O) | pure-XT build has **zero** string-I/O instructions | ✅ **done 2026-09-04** |

## D. Every other device on the bus

| # | lever | size | status |
|---|---|---|---|
| D1 | Trantor T130B - `rep insw`/`outsw` | uncosted | ❓ imports `ScsiPortRead/WritePortBufferUshort`; **nobody has checked if it uses them.** One `pedis.py <file> io` |
| D2 | T130B - polling (`Polling=1`, no IRQ) | uncosted | ❓ |
| D3 | Mach8 video | **most bytes on the bus of anything here** - framebuffer is across ISA | ❓ uncosted |
| D4 | `HSFLOP.PDR` - polls hard during a seek | uncosted | ❓ we already patch this binary, so most tractable |
| D5 | Sound (SB Pro) - DSP polling | uncosted | ❓ data path is DMA, no per-byte loop to widen |
| D6 | 3C509B network | uncosted | ❓ 16-bit card; well-written packet drivers already use `rep insw` - check first |
| D7 | Keyboard | near zero | latency-bound, few accesses per event |

## E. System-level

| # | lever | size | status |
|---|---|---|---|
| E1 | Conventional RAM off the planar | **384 KB of 640 KB no longer crosses the bus** | ✅ **done** - SW1-3/4 ON. No read-speed change; the gain is bus footprint |
| E2 | Shadowing - what is already shadowed | uncosted | ❓ confirm rather than assume |
| E3 | Wait-state tuning | 0 | ❌ already 0 wait states, cache on. Port `0x670` is write-only |
| E4 | Memory-mapped storage (JR-IDE/ISA) | **4.2x on the data phase** (0.454 vs 1.910 us/byte) | ❓ needs different hardware, but **86Box models it** - provable before buying |
| E5 | **ISA DMA instead of PIO** - @andrew-hoffman, 2026-09-11 | potentially **the whole per-access sync cost**, uncosted | ❓ closed for the XT-CF (PIO-only card), **reframed** as E5a/b/c below |
| E5a | DMA overlaps with CPU work *on this machine* - the 386 runs from local RAM and cache while the 8237 holds the bus | uncosted, Inboard-specific | ❓ unverified, but it is why DMA is worth more here than on a stock XT |
| E5b | **DMA channel 3 appears unused** | an idle system resource | ❓ confirm the inventory: 0 refresh, 1 SB Pro, 2 floppy, 3 free? |
| E5c | ⚠ **Is the 384 KB E1 moved onto the Inboard still DMA-visible?** | **correctness, not speed** | ❗ open - INBRDPC's own `maint_dmabank` exists because Inboard-held RAM is DMA-invisible. Affects sound and floppy today |

## E5. Does DMA pay the same bus penalty as PIO? - @andrew-hoffman, 2026-09-11

His question, verbatim, on [#23](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/23):

> *"does ISA DMA reading from IO ports and writing to memory (or vice versa) cause the same
> extreme wait state penalty as PIO accesses? If not, it may actually be more useful than PIO
> on this system. Keeping in mind that the buffer has to be in low memory."*

### Why it is a good question, in one line

**Our dominant cost is CPU-side, and DMA does not involve the CPU.** The measured
`~3.90 us fixed sync + ~1.87 us/byte` is what it costs the Inboard's 386 to reach down to a
4.77 MHz ISA bus and back for *each access*. An 8237 transfer never crosses that boundary - the
controller drives the bus directly at bus speed. So there is a real mechanism by which DMA could
beat PIO here by much more than it would on an ordinary machine, and it attacks the fixed 3.90 us
term rather than the 1.87 us/byte one that every width lever so far has chipped at.

### What we already know, before measuring anything

| fact | consequence | source |
|---|---|---|
| XT-CF / XT-IDE is **PIO only** - no DRQ/DACK on the card | The boot disk, the device we most want faster, **cannot use DMA at all**. This is a hard stop for the main target | photograph + Lo-tech rev 3 design |
| DMA page latch is **4 bits** | Buffer must be under 1 MB, and 386MAX's `MARK_XT` says the real XT ceiling is **640 KB** | his own 2026-08-20 finding, measured 2026-08-24 |
| DMA **channel 0 is DRAM refresh** on an XT | Only channels 1-3 are available, and channel 2 is the floppy | IBM 5160 Tech Ref |
| `T130.MPD` is **PIO only** | The one card on this machine that *can* bus-master is not being asked to | [[t130-mpd-verified-pio-t130b]] |
| The LS-120 bridge has a DMA path that writes `0x22`/`0x23` | Those **alias onto the 8259** on this XT - the #22 keyboard-killer, and it is in the *transfer* path, not init | technique 62 / TRANSPORT_SPEC |

So the honest position is: **the mechanism is plausible and the arithmetic is untested, but the
device that would benefit most cannot do it.** That is worth saying to him plainly rather than
agreeing enthusiastically.

### The measurement, which is cheap and settles it

We do not need a DMA-capable disk to answer the *physics* question. The floppy controller is on
channel 2 and already does DMA, and `HSFLOP.PDR` is a binary we already patch:

1. Time a floppy DMA read of one track with the PIT harness (technique 109), and divide by bytes
   moved. That gives **us/byte for a real DMA transfer on this machine**.
2. Compare against the `1.87 us/byte` PIO figure and the `0.454 us/byte` memory-mapped figure
   from E4.
3. If DMA lands near the ISA bus rate rather than near the PIO rate, his hypothesis is confirmed
   and the lever becomes "find or build a DMA-capable storage card", which converges with **E4a**.

Cost: one DOS-level timing run, no new hardware, no driver written.

### REFRAMED 2026-09-12 — ask it of the machine, not of the card

The owner's steer, and it is the same move that found the host side of the XT-IDE socket:

> *"perhaps dma is a lever if we flip it on its head, not going after the card itself but the
> machine it sits within... it might not be a lever explicitly for the question at hand but it
> might point us to look at how and where DMA is and make sure it is used efficiently and
> correctly."*

Taken that way the question stops being "can the disk use DMA" — which is closed, it cannot —
and becomes **three** questions, one of which is a correctness problem rather than a speed one.

#### E5a. The Inboard changes the economics of DMA, and it changes them in our favour

On a **stock XT** DMA is close to free for the device and expensive for the CPU: the 8088 needs
the bus for instruction fetch as well as data, so while the 8237 holds it the CPU stalls. That is
why DMA is usually costed as "the same bus, just a different master".

**This machine is not a stock XT.** The 386 has its own local RAM and its own cache, and E1 has
just moved 384 KB of conventional memory onto the card. So during a DMA burst the Inboard can
keep executing out of local RAM and cache instead of stalling — the transfer overlaps with real
work in a way it never could on an 8088.

That is a genuine, Inboard-specific asymmetry and it is **not** what Andrew was asking, but it
strengthens his case rather than weakening it. ⚠ **Unverified** — it assumes the Inboard does not
have to arbitrate for the ISA bus on a cache hit. Measure before believing it.

#### E5b. Channel inventory — one channel is apparently idle

| channel | owner | note |
|---|---|---|
| 0 | **DRAM refresh** | XT-only use; not available |
| 1 | **SB Pro** | `dma = 1` in the machine config |
| 2 | **Floppy** | standard, and `HSFLOP.PDR` is a binary we already patch |
| 3 | **apparently nothing** | 3C509B is I/O `0x320` / IRQ 3 with no DMA channel configured |

**Channel 3 looks free.** That is an idle system resource on a machine where the bus is the
bottleneck, and it is worth confirming rather than assuming — if a device could be given it, the
question "is DMA faster than PIO here" stops being academic.

#### E5c. ⚠ Correctness — after E1, is conventional RAM still DMA-visible?

This is the part that matters more than the speed question, and it was not visible until the
frame widened.

**E1 moved 384 KB of the 640 KB off the planar and onto the Inboard.** DMA buffers live in
conventional memory. A bus master never consults the 386 page tables — that is exactly why
`$386.SYS` carries `maint_dmabank`, tracking the lowest remapped bank on every `vremap`, and why
its INT 13h hook **stages** any transfer that reaches it. Intel's own 1988 code treats
Inboard-held memory as DMA-invisible and bounces around it.

So the open question, in one line:

```
Can the 8237 reach the 384 KB that E1 moved onto the card?
```

Three possibilities, and we do not know which:

1. The backfill responds to ISA bus cycles like planar RAM → nothing to do, and say so.
2. It is Inboard-local and page-mapped → **DMA into it silently reads or writes the wrong
   memory**, and INBRDPC's staging is the only thing standing between that and corruption.
3. It is reachable but slower → a cost nobody has measured.

Affected today: **sound (channel 1)** and **floppy (channel 2)** — both of which allocate in
conventional memory, and both of which this project has already had one 20-bit DMA reach bug in.

⚠ This is **not** offered as an explanation for [#18](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/18):
that predates E1 by days, so E1 cannot be its cause. But it is the same bug class, it is now
worth excluding, and if #18 ever reproduces *more* readily after E1 this is the first thing to
check.

**The test is cheap and it reuses a harness that has already caught this exact class once:** the
page-register trace that caught `MSSBLST.VXD` allocating at `0x4E0000` live. Poison a
conventional-memory buffer, run one floppy DMA read into it, and check whether the bytes landed.
A self-test that cannot fail is worse than none — so poison first, and do not accept zeros as
success (the LS-120 work made that mistake and lost a session to it).

### Why this belongs in the ledger even if it cannot be used today

**Never rank a lever out for being small, and never rule one out for being inconvenient.** E4
is on the list despite needing a card nobody owns, because 86Box models it and the number can be
proven before spending money. E5 is the same shape: the measurement is cheap, the answer is
reusable, and it tells us whether a DMA-capable ISA storage card is worth hunting for at all.

## The two rules this ledger exists to enforce

1. **Never rank a lever out because it is small.** 4% that costs nothing is 4%, and it
   compounds. That reasoning wrongly dismissed sound and floppy once already.
2. **Always work both sides of the connector.** The card's decode ceiling ended the
   transfer-width work and said nothing about transaction count - which is host-side, and
   is the larger lever.

---

## @andrew-hoffman, 2026-09-12 — three additions, one of which is not an optimisation

Posted on [#23](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/23). Recorded
here because two of them change existing ledger entries.

### 1. E5b is ANSWERED, and it lands on the LS-120

> *"I think the only storage device you have which could currently use DMA is the ECP Parallel
> port card which the LS 120 is attached to and it nominally claims DMA channel 3."*

**A5/E5b asked whether channel 3 was free. It is not — the ECP port claims it.** That closes the
inventory question and turns it into something better: the one storage device on this machine
that *can* do DMA is the one we are actively writing a driver for.

Our current position is `dmaEn = 0` (technique 62), i.e. we deliberately do not use it. The
vendor's DMA block path was rejected for a good reason — it writes `0x22`/`0x23`, which alias
onto the 8259 on this XT, and it does so on the **transfer** path, not merely in chipset init.

So the lever exists but the vendor's implementation of it is unusable here. **Open question:
can the ECP DMA path be driven without those writes?** That is a driver question, not a config
one, and it belongs with the LS-120 work rather than in this ledger.

⚠ Do not treat this as a speed lever until the LS-120 works at all in PIO. Gated.

### 2. His DMA-vs-PIO question, sharpened

> *"does using ISA DMA to read from IO ports and write to memory (or vice versa) cause the same
> extreme wait state penalty as PIO accesses? If not, it may actually be more useful than PIO on
> this system. Keeping in mind that the buffer has to be in low memory, and programming the DMA
> controller for a transfer will incur several penalties as well so you would want to do as large
> of a transfer as possible at once (at least 512 bytes)."*

Already **A4/E5**, and the framing adds two constraints worth writing into the method:

- **the buffer must be in low memory** — which this machine enforces anyway, since the DMA page
  register is 20-bit (technique 62). Consistent with E5c.
- **amortise the setup** — programming the 8237 costs several bus accesses, so any measurement
  must use a transfer of at least 512 bytes or it measures setup, not throughput.

A4's method already uses a one-track floppy read, which satisfies both. No change needed, but the
512-byte floor is now stated rather than assumed.

### 3. Not an optimisation — a note on method, and it is correct

> *"Seems that Claude keeps having trouble writing ASM probes over COMRade using DEBUG, and it's
> using Python scripts to produce DEBUG scripts to eventually produce the code. You have a
> macro-assembler already (MASM) and that can produce 16-bit COM binaries as well."*

**He is right, and it cost time again on 2026-09-12** — a 61 KB serial transfer of `CTCHIP34.EXE`
exceeded COMrade's 8 s op-timeout and the retry truncated the file to 1 KB.

MASM 6.11c (`Windows95_ddk/MASM611C/ML.EXE`) is already in use for every driver in this repo and
runs natively on the host. **Build probes as `.COM` files with MASM and ship the binary, rather
than generating DEBUG scripts.** DEBUG's line-length limit, its CRLF requirement, and its refusal
to run under DOS 7 are all failure modes that disappear entirely this way.

Cross-references: the DEBUG traps are recorded under technique 105; technique 114 records what a
hand-built `.COM` gets wrong if its entry point is not checked.

### Also worth acting on, outside this ledger

> *"Might want to include an instruction in claude.md that code comments should document why code
> does what it does, and not what it *used* to do - that's what the Git history is for."*

A repo-hygiene rule for `CLAUDE.md`, not a bus lever. Owner's call.

### And his caution, which is not a technical point but belongs on the record

He notes the Inboard was designed before integrated chipsets, to buy businesses a couple of years
on already-depreciated machines, and that no amount of software work transcends that. The owner's
answer — *"we don't do these things for necessity but because it's there"* — is the project's
actual position, and this ledger should be read in that light: it is an exploration of what the
hardware can be made to do, not a claim that it can be made fast.

---

# CPU LEVERS — beyond revto486, 2026-09-12

**Replication is done.** Every BL3 register that exists now matches REVTO486's dump on the
known-good DOS/3.11 configuration (`docs/cpu_cache_cmlr_2026_09_12.md`). That closes the
*known-good* question and opens the *optimal* one, which `CTCHIP/README.TXT` explicitly separates:

> *"whether (a) is OPTIMAL is a separate question from whether it is KNOWN-GOOD."*

⚠ **Benchmark the CMLR change first.** Every lever below must be measured against a post-CMLR
baseline, one at a time, or we will not know which change did what. Registers reset on power
cycle, so each trial costs a reboot.

| # | lever | now | candidate | evidence | risk |
|---|---|---|---|---|---|
| **C1** | `1000h:1` bit 4 **XTOUT** — Extended Out instruction | **1** | **0** | feipoa, the same source we took `CNPX=1` from: set 1 *"costs DOOM realtics"*. revto486 leaves it 1, so this is a genuine tuning deviation, not a replication gap | low — one bit, documented in `IBM486.CFG` |
| **C2** | `1001h:2/3` **LMROR** — 1 MB read-only mask | `0000` | ? | Marking the ROM/BIOS region read-only-cacheable is what the register is for. revto486 leaves it zero, which may be deliberate | ⚠ medium — a wrong mask makes writable memory read-only |
| **C3** | `1004h:0` bit 4 **MOVS Split** | `0` | ? | Undocumented effect in `IBM486.CFG` beyond the name. Unknown whether it helps an 8-bit bus | unknown — measure, do not assume |
| **C4** | `1000h:0` bit 3 **SNP** vs bit 4 **ASNP** | SNP `0`, ASNP `1` | — | feipoa's BARB-vs-FLUSH discussion (technique 68): an XT planar never drives `FLUSH#`, so cache invalidation must come from bus snooping. ASNP is on, which is why `CPUSET.BAT` says *"never CE without ASNP"* | ❗ **correctness, not speed** — see the note below |
| **C5** | `1004h:3` bit 1 **NA16** — bus pipelining for 16-bit | `0` | — | **Probably inert.** This is an 8-bit bus. Recorded so nobody spends a boot on it | n/a |
| **C6** | `1004h:3` bit 4 **CLP** — Cache Low Power | `0` | keep `0` | `0` = cache stays on. Already optimal for our purposes | n/a |
| **C7** | `1000h:1` bit 7 **CNPX** — cacheability of NPX operands | `1` | keep `1` | feipoa recommends 1 for FPU performance. Already set | n/a |

## C4 is the one to think about before celebrating

Extending the cacheable region to 1–16 MB means more memory is now cached than at any point in
this machine's history. The reason that is **not** a new DMA-coherency exposure is measured, not
assumed: ISA DMA here has a **20-bit page register** (technique 62), so no DMA buffer can exist
above 1 MB. Everything the 8237 can reach was already inside the cached low-640 KB region.

But this should be re-checked if anything ever gives the 8237 more reach - and it is the reason
**E5c** (can the 8237 reach the 384 KB E1 moved onto the Inboard?) stays ranked as a correctness
item rather than a speed one.

## Suggested order

1. **Benchmark the CMLR change.** Nothing below is interpretable without it, and we owe red-ray
   the figure.
2. **C1 (XTOUT)** — one bit, documented, a named source, and it is the only lever here where
   somebody else has already reported a measurable effect.
3. Stop unless the numbers justify going further. C2/C3 are undocumented territory on a CPU with
   no datasheet to hand, and the machine is bus-bound for I/O regardless of what the core does.

---

# EXTERNAL INPUT TRIAGE — LLM optimisation suggestions, 2026-09-12

The owner ran the project past another model and forwarded its output with the right framing:
*"I don't know how much it got right but always worth considering another point of view."*
Triaged here so the useful part is kept and the wrong part does not leak into the docs.

**Two items are worth keeping. One kills a proposed session before it starts.**

## Kept

### B1a. The async primitive has a name — `ScsiPortNotification(RequestTimerCall, ...)`

B1 (request merging) has always been blocked behind "needs async completion -> queue depth ->
merge", stated as a shape rather than an API. The suggestion names the SCSIPORT mechanism:
return from `XtStartIo` and be re-entered via `HwTimer` instead of completing inline.

That is the same primitive for two separate wins already in this document:

| use | what it buys |
|---|---|
| release the CPU during a transfer | the stall the driver README already flags under heavy teardown flush |
| pace a wait without spinning | A2's `XT_POLL_BACKOFF` idea, but at the SCSIPORT layer rather than inside our loop |

**Status: unverified against the DDK.** The notification type exists in SCSIPORT; whether the
Win95 IOS build honours it for a `.MPD` is not established, and that check belongs with B7 (the
DDK debug builds and symbols, still unopened). Recorded so B1 starts from an API name.

### F1. SIV misreports the CPU clock — an open question, not yet a fact

The forwarded analysis asserts SIV reads 65.7 MHz against an actual 83.5 MHz, and explains it as
a timing loop assuming a 15-cycle `AAM` where this core takes 17.

**Do not record that as a finding.** Three problems:

1. The explanation carries no source and reads as invention.
2. It conflicts with what we actually read. The pre-fix log said `0.00MHz`, no TSC, no CPUID
   (`contributor_input_ledger.md`, red-ray row).
3. **This project has never measured the core clock**, so "83.5 MHz actual" is unsupported here.

**RESOLVED SAME DAY — both walks now read.** See `cpu_cache_cmlr_2026_09_12.md`. The 65.7 MHz
figure is real and post-fix; pre-fix SIV could not resolve the CPU at all, calling it
`Generic 486 DX` with an 8 KB L1. SIV derives clock, cache size and the DX/DX2 distinction from
the same timing curve, so before the fix all three were guesses.

**The "83.5 MHz actual" remains unsupported and the `AAM` cycle-count explanation stays rejected.**
This project has never measured the core clock. Until it does, there is no known error to explain.

⚠ And the benchmark surfaced something no summary mentioned: **working sets above the 16 KB L1
are now ~2.3x slower than before the fix** — the cost of line fills on a cache-hostile access
pattern. Confined to the Inboard's local RAM, so it costs the ISA bus nothing. Written up in full
in the cache doc; it is the open question there, not here.

## Already covered — no action

| suggestion | where it already lives |
|---|---|
| Assembly inlining of the transfer loops | **B3**, costed at 4%, free, compounds |
| Multi-sector / bi-sector transfers | **A4**, ❌ closed — drive reports word 47 = 1, unsupported |
| Unrolled / paced polling loops | **A2**, ✅ shipped as `XT_POLL_BACKOFF` |
| Strip dead AT-fallback from `VKD.VXD` | **D7** — latency-bound, few accesses per event, near zero to win |
| Sweep every VxD for 20-bit DMA truncation | `tools/sweep_image_dma.py` exists; it is a **correctness** sweep. The suggestion's rationale — that truncation wastes CPU on retries — is wrong. A 20-bit truncation writes to the wrong address silently. There is nothing to retry |
| Keep data in Inboard RAM to bypass the motherboard | the premise of this entire document (the one fact at the top) |

## Wrong — recorded so it is not re-derived

| claim | what is actually true |
|---|---|
| *"You realised the A3/A0 Hi-Speed swap is a pure 8-bit highway and exploited it"* | **Inverted.** The Hi-Speed map is the one `XTIDEMP.MPD` **cannot** drive — a permutation no stride expresses. That is open issue #23. Our card is a Lo-tech rev 3 on the Compatibility-style map at stride 2 (`docs/xtide_register_maps.md`) |
| *"REP INSB ... saturates the bus"* | We ship `rep insw` — **word**, not byte. Byte-wide I/O measured 5.770 us/byte against 1.910 for the word path (E4 table). `insb` is the slow path we left behind |
| ~~*"I-O Data PK-A486BL interposer"*~~ | ✅ **CORRECT — my error, retracted 2026-09-12.** It *is* this machine's hardware: the owner identified it on the VOGONS thread the same day (I-O Data PK-A486BL interposer, BL3 60 MHz 486DLC, a PC-98 part), and `INBOARD_86BOX_PORT_PLAN.md:101` already recorded it. I called it invented **without grepping our own docs** — the exact mistake this file warns about two rows down. A forwarded model quoting the owner back at us is not hallucinating |
| *"`READ MULTIPLE` will reduce interrupt round-trips"* | The XT-CF path is **polled, no IRQ**. There are no interrupt round-trips to reduce, and A4 is closed on the drive's own capability word |

## The one that kills a session — the SIV VxD patch proposal

The proposal: reverse `SIVVXD.vxd`, clamp its `_PageAllocate` calls to `maxPhys = 0xFF`, audit it
for `0x22`/`0x23` chipset probes, and patch out an architectural check to force-load it.

**There is nothing to patch.** SIV's own debug log states its position plainly:

> *"Use of the SIV Windows 9x VXD V5.88 rejected as it has not been written!"*

SIV is not failing to load a driver. It is declining to use one **that does not exist for
Windows 9x**. There is no binary on the card, no load failure, and no status code to bypass.

The rest of the proposal is this project's own signature bug pattern-matched onto an unrelated
symptom. `maxPhys` governs **DMA buffer reach**. A tool that reads CPU registers does no DMA, so
the 20-bit ceiling cannot be its problem even if the driver existed.

**What would actually be needed** is writing a Win9x VxD for SIV from scratch — a real
contribution to red-ray's tool, plausibly welcome, and squarely a *"nice to have"*. The owner
already placed it correctly: **after #22**. It is a tooling contribution, not a bus lever, and it
is not in the ledger above.

⚠ General lesson, and it is the second time: an outside model given this project's documents will
**reuse our own findings as explanations for unrelated symptoms** — 20-bit DMA, `0x22`/`0x23`
aliasing, the Hi-Speed map. Those are the memorable parts of the write-up. Check that the symptom
actually matches before spending a session on the familiar-looking cause.

## D8. LS-120 data path runs in NIBBLE on an ECP card — established, not built

**This has been established repeatedly** (technique 108, `ls120-transport-solved-2026-09-08`,
`TRANSPORT_SPEC.md`) and the code still does not do it. Recorded here as a lever with a number
against it so it stops being re-derived.

| | |
|---|---|
| the card | **ECP-capable**, and the bridge is a Shuttle EPATRM |
| what the vendor offers | 12 read transports, 5 write: NIBBLE, UNIDIR, TOSHIBA, PS/2, EPP, **ECP Read**, **ECP Write** (`sd120ppd_sys_full_read_2026_09_13.md`) |
| what `LS_BlockRead`/`LS_BlockWrite` do | **`epat.c` mode 0 nibble**, byte at a time: `w0(7) w2(1) w2(3) w0(FF)`, then per byte `w2(6+ph)`, `r1()`, often `w2(4+ph)`, `r1()`, `j44()` |
| cost | **two status reads and two control writes per BYTE** - 4 port accesses minimum, ~8 with the setup, at ~3.9 us of Inboard-to-bus sync each |

The correct architecture was written down on 2026-09-08 and is unchanged: **nibble for the task
file, ECP `rep insb`/`rep outsb` for the payload.** Registers are a handful of accesses per
command and nibble is fine for them; the payload is 512 bytes per sector and is where all the
time goes.

⚠ The caution from technique 108 still stands and is the reason this is not a simple swap: the
vendor's **DMA-assisted** block path writes `0x22`/`0x23`, which alias onto the 8259 here. ECP
FIFO transfer is not that path, but the boundary must be checked in the mode handler before any
of it is copied - handlers are located and named in the doc above.

**Order of work:** correctness first (the write path only started moving bytes on 2026-09-13 and
is not yet proven end to end), then this. A fast transport that corrupts is worth nothing.
