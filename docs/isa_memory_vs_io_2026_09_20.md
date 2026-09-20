# An ISA I/O access costs 2.87 us more to start than an ISA memory access

Real 5160, Inboard 386/PC, 2026-09-20, LS-120 powered off. `tools/gen_memwidth_probe.py`,
512 bytes copied out of an option ROM with `rep movsb` / `movsw` / `movsd`, PIT
channel 0 latched either side, interrupts off. Same instrument and same method
as `docs/iowidth_measured_2026_09_20.md`, which measured ports.

Reading an option ROM has no side effects, which is why the source is a ROM
rather than any card's data window.

## Raw

| run | source | raw | `movsb` x512 | `movsw` x256 | `movsd` x128 |
|---|---|---|---|---|---|
| 1 | `0xD8000` XT-CF, XUB212 r638 | `D4 06 C6 05 3A 05` | 1748 | 1478 | 1338 |
| 2 | `0xD8000` | `D8 06 C6 05 3A 05` | 1752 | 1478 | 1338 |
| 3 | `0xD0000` the other option ROM | `FC 08 F4 07 6E 07` | 2300 | 2036 | 1902 |

Runs 1 and 2 differ by 0.2%; the word and dword arms are identical.

## Derived, against the port numbers

PIT at 1.193182 MHz, 0.83810 us/tick. Fitting `sync + n x cycle`, where an
8-bit slot forces `n` bus cycles for an `n`-byte access:

| path | fixed sync | per bus cycle | byte, us/B | word, us/B | dword, us/B |
|---|---|---|---|---|---|
| **ISA I/O** (`0x378` and `0x278`) | **3.752** | 1.943 | 5.695 | 3.819 | 3.183 |
| **ISA memory** (`0xD8000`) | **0.883** | 1.978 | **2.861** | **2.420** | **2.190** |
| ISA memory (`0xD0000`) | 0.864 | 2.901 | 3.765 | 3.333 | 3.113 |

Both memory fits are near-exact at four bytes: `0.883 + 4 x 1.978 = 8.795`
against 8.760 measured, and `0.864 + 4 x 2.901 = 12.468` against 12.454.
**Memory is linear in width; I/O was 10.5% sub-linear.**

## What it says

1. **The per-bus-cycle cost is the same for I/O and memory** (1.943 against
   1.978 us). **The fixed synchronisation is not: 3.752 us for I/O against
   0.883 us for memory.** The Inboard pays **2.87 us extra to start every I/O
   access** and nothing like it for a memory access. An XT's mandatory I/O
   wait state accounts for one clock, 210 ns, of that. The remaining ~2.66 us
   is the accelerator, and the likely reason is that I/O is serialising on a
   386 - the write buffer must drain and the access must complete in order -
   while a memory read can be pipelined.

2. **Byte-wide memory beats dword I/O.** 2.861 against 3.183 us/byte. A card
   with a memory aperture, moved a byte at a time, is faster than the best
   possible port-based transfer.

3. **Dword memory is 2.6x better than byte-wide I/O**, 2.190 against 5.695.

4. **The sync is the Inboard's; the per-cycle is the card's.** Two different
   memory windows gave the same sync to 2% and per-cycle costs differing by
   0.923 us - about 4.4 clocks of 4.77 MHz, i.e. the `0xD0000` card inserts
   roughly four wait states where the XT-CF inserts none.

## Which answers the wait-state question

A card's "zero wait state" setting moves the **per-cycle** term, not the sync.
On these numbers that is 2.901 -> 1.978 us, about 0.92 us/byte, **~24% on a
memory-mapped path** - real, worth having, and the T130B already has it. It is
not a way of bypassing the bus; it means the card does not stretch the standard
cycle. It is also a **smaller** lever than the memory-versus-I/O choice, which
is worth 2.87 us per access.

And it is not available on the port side at all: the `0x278` control in the
port run matched the Intek21 to 0.1%, so nothing there was adding wait states
to remove.

## The read/write confound, checked

The port probe wrote (`outs`) and this one reads (`movs`), so the comparison
could in principle be read-versus-write rather than memory-versus-I/O.
Technique 109e measured `rep insb` **reading** a port at 5.87 us/byte, against
5.695 for `outsb` here. **I/O reads and I/O writes cost the same**, so the gap
to 2.861 us/byte for a memory read is the memory/IO distinction, not direction.

## Caveats

- **Reads only.** A ROM cannot be written, so writes to a card's memory
  aperture are untested and must not be assumed symmetric.
- `movs` performs a local RAM write per element, which is included in these
  figures. That inflates the memory numbers, so the memory advantage is if
  anything **understated**.
- ~9% of the width saving is `rep` loop overhead rather than bus, as in the
  port run.
- **The ROMs are not shadowed.** 2.861 us/byte is far above what Inboard-local
  RAM would cost, so these reads are genuinely crossing the bus. That was the
  probe's built-in self-check and it passed.

## Who can actually use this

| card | aperture | usable? |
|---|---|---|
| Lo-tech XT-CF rev 3 | I/O `0x300-0x31F` only | **no** - ports are all it has |
| Intek21 parallel | I/O `0x378` only | **no** |
| **Trantor T130B** | none - **checked, see below** | **no** |
| Mach8 video | framebuffer at `0xA0000`/`0xB8000` | already memory-mapped, already collecting this |

## ⛔ The T130B has no memory window. Checked 2026-09-20, avenue closed.

Three independent lines, all negative:

1. **No Trantor ROM is present.** A `55 AA` scan of `C000`-`E000` found exactly
   two option ROMs: `0xD8000` is `XUB212-=-XTIDE Universal BIOS (XT+)=-` r638,
   and `0xD0000` is a **floppy BIOS** - its own config words read base `0x03F0`,
   IRQ `6`, second channel `0x0000`. Neither is the T130B, which matches
   `bios_addr=0`.
2. **The vendor's resource declaration has no memory range.**
   `T130.INF`'s `[*T130.LogConfig]` lists `IOConfig`, `IRQConfig` and
   `DMAConfig=0`, and **no `MemConfig`**. A card with an aperture declares one.
3. **The shipped driver is I/O-space only.** `T130.MPD` imports
   `ScsiPortRead/WritePortUchar`, `...PortUshort` and `...PortBufferUshort` -
   all I/O - and nothing memory-mapped.

What would reopen it: documentation of a T130B memory decode with no ROM, or
fitting a **T128**, which is the memory-mapped member of the family.

## So the finding is currently unexploitable, and that is the honest summary

Every data path on this machine except video is port-only: XT-CF `0x300-0x31F`,
Intek21 `0x378`, T130B `0x340`, 3C509B `0x320`, SB Pro `0x220`. **The only
memory-mapped data path is the Mach8 framebuffer, which already gets the
benefit.** Nothing in the storage stack can collect the 2.87 us today.

It still changes three things:
- **Card selection.** Any future card with a data aperture is worth ~2x over an
  equivalent port-mapped one, and that is now a measured number rather than a
  hunch.
- **The cost model.** The 3.90 us sync is an **I/O** figure. Do not apply it to
  memory-mapped work; memory is 0.883 us.
- **Emulator fidelity.** 86Box charges I/O and memory ISA accesses alike. The
  real machine does not, by 4.25x on the fixed term.

⛔ This does not rescue the LS-120 or XT-IDE paths. Both are port-only by
construction, and for them the width work stands as the lever.
