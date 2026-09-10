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
