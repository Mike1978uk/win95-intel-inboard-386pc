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

## ACTIONS OUTSTANDING — the single list to work from

Every open optimisation question, as an **action with a method and a cost**, not a note. Nothing
here is closed by reasoning alone: each row ends in a measurement or an inspection of a binary.

**Order is by (evidence available) x (cost to get it), not by size.** A big lever with no way to
measure it ranks below a small one we can settle this week.

| # | Action | Method | Cost | Status |
|---|---|---|---|---|
| **A1** | Request merging in `XTIDEMP.MPD` | Raise `MaximumTransferLength`, coalesce adjacent SRBs; re-run the technique 93 traffic capture | driver build + 1 boot | 🎯 **next after LS-120** — 1.2-1.5x, measured not modelled |
| **A2** | The free 4% — transfer-loop overhead | `rep movsd` in the buffer paths; unroll the per-sector loop | driver build | 🎯 queued behind A1 |
| **A3** | **E5c — can the 8237 reach the 384 KB E1 moved onto the Inboard?** | Poison a conventional-memory buffer, run one floppy DMA read into it, verify the bytes landed. Page-register trace as the cross-check (the harness that caught `MSSBLST.VXD` at `0x4E0000`) | 1 DOS run | ❗ **correctness, not speed** — do this before A1. Sound and floppy allocate there today |
| **A4** | **E5 — time a real DMA transfer** | PIT harness (technique 109) around a one-track floppy DMA read; us/byte against PIO `1.87` and memory-mapped `0.454` | 1 DOS run | ❓ answers @andrew-hoffman's actual question with a number |
| **A5** | **E5b — DMA channel inventory** | Confirm 0 refresh / 1 SB Pro / 2 floppy / **3 free**. Read the 8237 mask + mode registers and the installed device list | minutes, DOS | ❓ an idle channel on a bus-bound machine is worth knowing about |
| **A6** | **E5a — does DMA overlap with CPU work here?** | Run a CPU-bound loop timed by the PIT, with and without a concurrent floppy DMA transfer. If the loop time is unchanged, the Inboard really does keep running from cache while the 8237 holds the bus | 1 DOS run | ❓ Inboard-specific, and it is *why* DMA is worth more here than on a stock XT |
| **A7** | **E6 — DRAM refresh tuning** (new, 2026-09-12) | See below. Reprogram PIT channel 1, memory-pattern test, PIT-timed throughput A/B | 1 DOS run, instantly reversible | ❓ **a tax every device pays**, and E1 just changed the premise |
| **A8** | **D3 — is the Mach8 accelerator actually being used?** (elevated 2026-09-12) | `pedis.py` on the shipped display drivers: count framebuffer writes vs `0x9AE8` command writes | no hardware at all | ❓ **potentially the largest single saving on the machine** — see below |
| **A9** | E2 — what is already shadowed | Read the shadow config and confirm rather than assume | minutes | ❓ |
| **A10** | D1 — does `T130.MPD` use string I/O? | `pedis.py T130.MPD io` | minutes, no hardware | ❓ one command, never run |
| **A11** | E4a — memory-mapped storage (JR-IDE/ISA) | 86Box already models it — prove the 4.2x before buying hardware | emulator run | ❓ |
| **A12** | **The Mach8 as a coprocessor — off-screen VRAM and on-card blits** (owner's lead, 2026-09-12) | Count the installed VRAM, work out how much is off-screen, and check whether the driver caches there. See below | binary inspection first, then one DOS run | ❓ **the one device here with its own RAM on the far side of the bottleneck** |

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
| B4 | `rep movsd` for buffer copies in local RAM (0.135 us/byte) | small, free | ❌ `XTIDEMP.ASM` has **zero** string ops / 9 byte-move lines; `XTIDETR.ASM` 4 / 20 |
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
