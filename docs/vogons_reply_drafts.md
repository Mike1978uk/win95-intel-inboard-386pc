# VOGONS reply drafts — thread t=112093

**Nothing here is sent. The owner sends.**

Thread: <https://www.vogons.org/viewtopic.php?t=112093>

House rules for these (same as the git ones): the ask in the first two sentences, one post one
ask, say what was tested and what was not, link the doc instead of pasting the reasoning, under
300 words. Disclose AI assistance once, plainly, and do not argue about it.

---

## 1. To @disruptor — ST01 (reply 30). READY, ~290 words

**Why a second reply is warranted:** reply 31 already pointed him at the repo, which answers the
question he asked. It does not answer the question he did not know to ask. His card transfers by
**ISA DMA** and ours are all PIO, so the template covers the plumbing and stops exactly where his
card starts — and both hazards below are silent-wrong-data failures, not errors.

> Source is all up at <https://github.com/Mike1978uk/win95-intel-inboard-386pc>, MIT. The miniport
> that drives the XT-CF is in `drivers/xtide_mpd/` — source, INF and build script. It is a plain
> SCSIPORT miniport, so a `.MPD` is an ordinary i386 PE and builds with the Win95 DDK's
> `ML /coff` plus `LINK /SUBSYSTEM:NATIVE`; no C compiler needed. A Win9x miniport also needs no
> ASPI layer, so that gap on the ST01 stops mattering once you are in protected mode.
>
> Two warnings before anyone spends a weekend on it. Both were measured on this machine, and both
> fail silently rather than erroring.
>
> **1. ISA DMA on an XT-class board reaches 20 bits, not 24.** The page latch is four bits wide.
> A DMA buffer above 1 MB does not fault — the high bits are dropped and the controller transfers
> against a different physical address. Wrong data, no error anywhere. It bit us on the Sound
> Blaster and again on the floppy driver. The ST01 is a DMA card, so it is directly in the path.
> In a Win9x driver the lever is `_PageAllocate`'s `maxPhys` argument: `0xFFF` (16 MB, the ISA
> assumption) becomes `0xFF`.
>
> **2. Ports `0x20`–`0x3F` all answer as the 8259 on a real XT.** The board decodes only A5–A9, so
> a driver probing for a chipset at `0x22`/`0x23` is writing to the interrupt controller. This is
> in IBM's own XT Technical Reference I/O map, and we measured it. It killed keyboard input on
> this machine for a fortnight before we found it.
>
> Both written up with the measurements:
> `docs/xt_dma_20bit_audit_2026_08_24.md` and `docs/xt_io_aliasing_gotcha.md`.
>
> Happy to look at an ST01 register map if your colleague can dump one. The DMA path is the part
> that needs designing; the plumbing is the easy half.

## 2. To @red-ray — SIV. NOT a reply, a deliverable

He has supplied everything asked for and is waiting on **us**: `SIV32L V5.88 Beta-09` run as
`SIV32L -DBGCPU -EXIT=20 > SIV_DBGOUT.log`, plus the latency figures, plus a save made without
command-line flags. He expects the CPU to report `IBM 486BLX2` — worth checking against ours,
which has run as a Blue Lightning at 3x since #9 closed.

`SIV_DBGOUT.log` is a file on `C:`, so COMrade lifts it straight off the card. Send the log, not
a status update.

**Say plainly that MCA testing is not something we can do** — there is no MCA machine here. That
closes half his open ask at no cost, and leaving it unanswered reads as ignoring it.

---

## SENT 2026-09-12 — to @red-ray, the CPU cache result

Posted by the owner. Kept here because it is the closing half of a loop this ledger opened.

**What he asked** (reply 41): *"Looking at the time for 16KB is much the same as for 24KB, are you
100% sure the L1 cache is enabled?"* — plus the observation that the machine benchmarks much
slower than a Cyrix Cx486DLC at ~33 MHz.

**What was sent:**

> red-ray — you asked whether I was 100% sure the L1 cache was enabled. It was enabled, and you
> were still right that something was wrong: it was caching nothing Windows uses.
>
> Read back from the CPU with CTCHIP34 over a serial link, not inferred:
>
> - `1000h:0 = 92` — CE set, ADS# snooping on. The enable bit was never the problem.
> - `1001h:0/1` — LMCR = `03FF`: only the low 640 KB cacheable.
> - **`1001h:4` — CMLR = `00`.** Nothing between 1 MB and 16 MB cacheable at all.
> - `1001h:5/6` — ECMLR = `0000`.
>
> Windows 95 lives entirely above 1 MB, so it was running completely uncached while every "is the
> cache on?" check said yes. That is why your 16 KB and 24 KB timings come out the same, and very
> likely why the machine benchmarks slower than a Cx486DLC at 33 MHz.
>
> The right value came from the machine's own history: REVTO486 1.04's register dump under
> DOS/Win 3.11 — a configuration that has run this board for years — shows CMLR = `F0`. That is now
> set at boot and verified by read-back on the hardware. Benchmarks to follow; I have not measured
> the gain yet.
>
> One note for SIV, offered as information rather than complaint: the debug log says *"Use of the
> SIV Windows 9x VXD V5.88 rejected as it has not been written!"*, and it reports 0.00 MHz for
> CPU/FSB/L2 with no TSC and no CPUID on this part. So on this machine SIV could not read CPU state
> directly and was inferring from the latency walk — and the inference was correct. The "Generic
> 486 DX L1 8KB" line is a table default, not a reading; the BL3 has 16 KB.
>
> Thank you — nobody would have looked without your logs.
>
> Details: https://github.com/Mike1978uk/win95-intel-inboard-386pc/blob/master/docs/cpu_cache_cmlr_2026_09_12.md
>
> *(AI assistance: Claude helps with this project's analysis and writing; all measurements are from
> the real hardware.)*

**Still owed to him:** the benchmark figures. The post says so explicitly rather than implying a
win that has not been measured.
