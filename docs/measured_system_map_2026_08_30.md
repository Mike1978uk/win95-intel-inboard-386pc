# Measured system map — real 5160 + Inboard 386/PC, 2026-08-30

Every value here was **read off the real machine** with COMrade at a real-mode DOS prompt. Nothing
is inferred from a datasheet, a jumper list or the emulator. Where a reading is not fully explained,
it says so — an unexplained measurement is still worth more than a confident guess.

Read-only throughout. No port was written.

## Why this is not a sweep, and never should be

A blanket `IN` across the I/O space would be actively harmful on this machine:

- reading a UART receive register **consumes** a byte;
- reading an ATA data register **pops the sector buffer**;
- the 8259 poll register can **acknowledge an interrupt**;
- the XT DMA page registers are **write-only** and float `0xFF` — there is nothing to read, and a
  probe built on writing then reading them back **cannot work here** (Technique 75).

So this is a targeted pass at known bases, and it should stay one.

## Interrupt controller

`PIC1 IMR` (`0x21`) = **`0xAC`**, at a DOS prompt with COMrade running.

| IRQ | mask | state | owner |
|---|---|---|---|
| 0 | 0 | **enabled** | timer |
| 1 | 0 | **enabled** | keyboard — healthy since the LS-120 miniport came out |
| 2 | 1 | masked | — |
| 3 | 1 | masked | 3C509B (no packet driver loaded in DOS) |
| 4 | 0 | **enabled** | COM1 — COMrade itself |
| 5 | 1 | masked | Sound Blaster Pro |
| 6 | 0 | **enabled** | floppy |
| 7 | 1 | masked | LPT |

### The 8259 alias, re-measured

| port | value |
|---|---|
| `0x21` | `0xAC` |
| `0x23` | `0xAC` |
| `0x25` | `0xAC` |

Identical, as [`xt_io_aliasing_gotcha.md`](xt_io_aliasing_gotcha.md) first measured and the
[IBM PC/XT Technical Reference](https://archive.org/details/IBMPCXTIBM51555160TechnicalReference6280089MAR86)
documents (8259A decoded across `020-03F`, a range not two ports).

**A detail worth keeping:** COMrade's own port-name table labels `0x23` and `0x25` as
*"VL82C420 cfg data"* and *"VL82C420 cfg data(2)"*. The tool assumes an AT chipset lives at those
addresses — which is precisely the assumption that made `SD120PPD.MPD` reprogram the interrupt
controller and kill the keyboard (issue #22, Technique 75). The bug class is not hypothetical or
confined to 1997 drivers; it is baked into a tool we use today. Reading is harmless. Writing is not.

## XT-IDE — Lo-tech XT-CF v2.0, slot 8

| port | value | meaning |
|---|---|---|
| `0x301`–`0x305` | `0x00` | taskfile, quiescent |
| `0x306` | `0x09` | **unexplained** |
| `0x307` | `0x09` | **unexplained — identical to `0x306`** |
| `0x30E` | **`0x50`** | alternate status: `DRDY\|DSC`, a present, ready, idle drive |

**Base `0x300`, settled two independent ways:** the XTIDE BIOS boot banner reports `D800h` then
*master at 300h*, and `0x30E` answers `0x50`. The option ROM agrees — `XTIDE204`,
*-=XTIDE Universal BIOS (XT)=-* v2.0.0 (2013-10-22), base `0x0300`, **control block `0x0308`**,
which places alternate status at `0x308+6 = 0x30E`, exactly the port that answered.

`0x320` and `0x340` were never candidates: the 3C509B and T130B are there.

**The `0x09` is not explained and must not be waved through.** Two adjacent taskfile registers do
not return the same value, and `0x09` is wrong for both drive/head and status. Either XT address
aliasing, or this card's taskfile decodes differently from a plain XT-IDE. That second possibility
decides which transport [issue #21](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/21)
must implement — XTIDE Universal BIOS drives both the rev1/rev2 high-byte latch **and** XT-CF 8-bit
PIO, and they are not the same code. `wPortCtrl = 0x308` fits both, so it does not discriminate.
**`XTIDECFG.COM` shows the device type outright — run it before writing transport.**

## ATI Mach8 / 8514-A — issue #8

| port | value | register |
|---|---|---|
| `0x2E8` | `0x05` | `DISP_STAT` — SENSE and HORTOG set, VBLANK clear |
| `0x42E8` | `0xAB` | `SUBSYS_STAT` |

`SUBSYS_STAT` = `0xAB` = `1010 1011`:

| bits | value | meaning |
|---|---|---|
| 3–0 | `1011` | monitor ID `0xB` |
| 4 | `0` | 8PLANE clear |
| 5 | `1` | **GPIDLE — graphics processor idle** |
| 6 | `0` | **INVALIDIO clear — no bad I/O latched** |
| 7 | `1` | VBLNKFLG |

**The accelerator is present, responding on its sparse I/O block, idle, and reporting no invalid
I/O.** Modest but real: #8's "RAM Addressing" self-test failure is not a dead or unresponsive
accelerator. Recorded as a data point, **not** a diagnosis — the self-test exercises accelerator
memory (Technique 73) and nothing here touches that.

## Trantor T130B — slot 4, issue #19

| port | value |
|---|---|
| `0x340` | `0x80` |
| `0x344` | `0x49` |
| `0x345` | `0xEF` |

**Something decodes at `0x340`** — these are not floating `0xFF`. The card answers where
`drivers/trantor_t130b/T130-XT.INF` expects it, which is worth knowing before that INF is installed
on real hardware.

⚠️ **Deliberately not decoded further.** These look like NCR 5380 registers (`+4` bus status,
`+5` bus and status), but the Trantor cards place the 5380 at an offset inside their own I/O window
alongside vendor control registers, and that mapping has not been established here. Reading
5380 bit patterns as SCSI bus state without the correct offset would be exactly the sort of
plausible-but-wrong decode Technique 44 warns about. **Non-`0xFF` is the finding. Nothing more.**

## Parallel port — Intel21 TK9901, slot 7

| port | value |
|---|---|
| `0x379` | `0x00` |

Jumpered `0x378` (JP6), IRQ 7 (JP4), DMA 3 (JP3/JP5), ECP/EPP mode (JP1/JP2).

⚠️ **`0x00` is unexpected and unexplained.** An idle LPT status register normally reads with BUSY
(bit 7, inverted) high — `0x78`-ish, or `0xFF` with nothing decoding. All-zero is neither. Possible
causes, none tested: the card is in ECP/EPP mode where that register means something different, or
the read did not reach the card. `LPTENUM` loads normally in `BOOTLOG.TXT`, so the port is not
absent. **Flagged, not concluded.**

## What was deliberately not probed

- **Sound Blaster Pro (`0x220`)** — several of its readable registers consume state (DSP read-data
  in particular). Nothing needed it.
- **Any DMA page register** — write-only, floats `0xFF`. See above.
- **`0x300` itself** (the data register) — reading it can pop the sector buffer, and the CF is the
  live boot disk.

## Standing rule this session reinforced

Two of the eight readings taken are unexplained (`0x306`/`0x307`, and `0x379`). Both are recorded
as unexplained rather than fitted to a story. The project's own history is full of cases where a
plausible reading of an unverified value cost a session — see Technique 44 (a static decode that
was self-consistent and wrong) and Technique 59 (a byte read at an address the machine was never
executing). **A measurement you cannot explain is data. An explanation you cannot measure is not.**
