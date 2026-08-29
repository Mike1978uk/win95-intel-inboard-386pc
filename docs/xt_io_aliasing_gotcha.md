# The XT I/O aliasing gotcha: why a stock Windows 9x driver can kill your keyboard

**Measured on real hardware, 2026-08-28.** An IBM 5160 with an Intel Inboard 386/PC, running
Windows 95.

If you run Windows 9x on XT-class hardware, this will eventually bite you, and the symptom looks
nothing like the cause. It is not specific to this project, this machine, or the driver that
exposed it.

---

## The one-paragraph version

An IBM PC/XT decodes I/O addresses incompletely. Its devices answer across far wider port ranges
than their documented addresses. Meanwhile, PC/AT-era drivers routinely probe for host chipsets by
writing to configuration ports and reading them back. On an XT those configuration ports **are**
the interrupt controller. The probe's read-back succeeds against nothing, the driver concludes it
has found a chipset, and its follow-up configuration writes reprogram the 8259. Your keyboard stops
working and the driver reports no error at all.

---

## The measurement

The claim is easy to state and easy to doubt, so here it is measured rather than asserted. Reading
ports on the running machine — which is possible at all because of
**[COMrade](https://github.com/yyzkevin/COMrade)** by **Kevin Moonlight**, ported to Windows 95 as
`COMR95.EXE` by **Ahmad Byagowi**. Without a way to read a live 5160's I/O ports from outside, this
would have stayed a plausible theory:

| port | documented as | value read |
|---|---|---|
| `0x21` | 8259 PIC — interrupt mask | `0xAC` |
| `0x23` | *nothing on an XT* | `0xAC` |
| `0x25` | *nothing on an XT* | `0xAC` |
| `0x31` | *nothing on an XT* | `0xAC` |
| `0x3F` | *nothing on an XT* | `0xAC` |

**Every odd port from `0x21` to `0x3F` returns the interrupt mask.** The 8259 is selected across the
whole `0x20`-`0x3F` block, with `A0` alone choosing between command and data. `out 23h, al` is
`out 21h, al`. So is `out 25h`, `out 31h`, `out 3Fh`.

The same is true of the other XT device blocks — the DMA controller, the PIT, the PPI and the DMA
page registers each answer across a wide aliased range. The page registers are **write-only**, so
you cannot confirm those by reading; do not test them by writing on a machine you care about.

Two asides worth knowing:

- The mask read as `0xAC` = `10101100`. Bit 1 clear means IRQ 1 — the keyboard — is enabled, which
  is what you would expect at a DOS prompt.
- Emulators may not model this. 86Box maps the XT PIC with
  `io_sethandler(0x0020, 0x0002, ...)` — two ports, no alias; only `pic_init_pcjr()` maps eight. So
  **this entire class of bug cannot reproduce in 86Box**, and a clean emulated run tells you nothing
  about real hardware. That is a fidelity gap in its own right.

---

## Why it decodes this way — documented by IBM, and visible in a schematic

The measurement above is not a quirk of one board. It is how IBM designed the machine, and two
primary sources say so independently. Both were pointed at by **@andrew-hoffman**.

### IBM says so in its own address map

The *IBM PC/XT Technical Reference* (5155/5160, 6280089, MAR86) lists the system board devices as
owning **ranges**, not addresses:

| Device | Hex Range |
|---|---|
| DMA controller, 8237A-5 | `000-01F` |
| **Interrupt controller, 8259A** | **`020-03F`** |
| Timer, 8253-5 | `040-05F` |
| PPI 8255A-5 | `060-06F` |
| DMA page registers | `080-09F` |
| NMI mask register | `0AX` |

Thirty-two ports for the 8259, thirty-two for the 8237, thirty-two for the DMA page registers.
The aliasing is not undocumented behaviour to be discovered by measurement — it is the published
address map, and it has been in print since 1983. The document also notes that hex `000` to `0FF`
is reserved for the system board, which is exactly eight 32-byte blocks.

<https://archive.org/details/IBMPCXTIBM51555160TechnicalReference6280089MAR86>

### The mechanism, from a machine-readable schematic

The Technical Reference's schematics are scanned images, so the wiring cannot be read
programmatically. The **DubaiXTClone** — a modern PC/XT-compatible board with full KiCAD sources —
can be, and it keeps the original system-board I/O decode. Its decoder sheet carries exactly these
nets:

```
inputs   XA5  XA6  XA7  XA8  XA9  ~AEN
outputs  ~DMACS  ~INTRCS  ~T/CCS  ~PPICS  ~WRTDMAPGREG  ~WRTNMIREG
```

**`XA0` through `XA4` are not connected to the I/O decoder at all.** Five address lines select one
of eight 32-byte blocks; the bottom five bits are simply not part of the decision. Each chip then
decodes only the low address lines it is physically wired to, and the 8259 has exactly one — `A0`.
Hence `0x20` and `0x22` are the same command port, `0x21` and `0x23` and `0x25` and `0x3F` are the
same data port.

<https://github.com/spencer-uk/DubaiXTClone/>

### What these sources did not give

Recorded so nobody re-checks them for this:

- **NuXT** (<https://github.com/monotech/NuXT>) is a faithful and well-documented XT recreation, but
  it uses the Faraday **FE2010A** single-chip XT controller. The decode is inside an ASIC, so there
  is no discrete decoder to read. Its bundled FE2010A datasheet may still be useful for other
  questions; it was not for this one.
- The pin-by-pin mapping of which address line drives which `74LS138` select input was not traced.
  It does not matter here — the input *set* is the whole claim.

---

## The worked example

The Imation/Shuttle LS-120 parallel-port driver for Windows 95, `SD120PPD.MPD` (1997). Installing
it gives a working drive letter and **silently kills keyboard input**. The mouse keeps working.

Three separate destructive patterns, all in one binary.

### 1. The chipset probe at `0x22`/`0x23`

```asm
fa              cli
e4 21           in  al,21h        ; read the mask, save it
50              push eax
b0 0a           mov al,0Ah
e6 21           out 21h,al        ; write 0x0A
e4 23           in  al,23h        ; read back from "the chipset"
3c 0a           cmp al,0Ah        ; did it stick?
58              pop eax
e6 21           out 21h,al        ; restore
fb              sti
```

This one is careful — it saves and restores. But because `0x23` aliases to `0x21`, the read-back
returns exactly what was written, so **the probe always succeeds**, and the driver proceeds to
configure a chipset that is not there.

### 2. The configuration that never restores

```asm
50              push eax
b0 21           mov al,21h
e6 22           out 22h,al        ; "chipset index" -> PIC COMMAND register
b0 02           mov al,02h
e6 23           out 23h,al        ; "chipset data"  -> MASK = 0x02, IRQ 1 masked
b0 c2           mov al,0C2h
e6 22           out 22h,al
e4 23           in  al,23h
0c 04           or  al,04h
e6 23           out 23h,al        ; MASK = 0x06, IRQ 1 + IRQ 2 masked
58              pop eax
c3              ret
```

`push eax` / `pop eax` preserves the *register*. Nothing preserves the *port*. Final state:
interrupt mask `0x06`, keyboard dead, no restore anywhere. A serial mouse on IRQ 3 or 4 is
untouched — which is exactly the reported symptom.

### 3. A second config pair at `0x24`/`0x25`, and PS/2 writes at `0x94`

```asm
b0 61   mov al,61h
e6 24   out 24h,al     ; -> PIC COMMAND register
e4 25   in  al,25h     ; -> reads the mask
0c 01   or  al,01h
e6 25   out 25h,al     ; -> masks IRQ 0, the TIMER
```

A second site ORs `0x08`, masking IRQ 3. Elsewhere the driver writes `0x7F` and `0xFF` to port
`0x94`, the PS/2 system-board setup register, which on an XT falls inside the DMA page block.

**And 19 of the writes are word-width** (`66 e7 22` = `out 22h, ax`). On an 8-bit bus a word write
splits into two byte writes, hitting *both* registers of the pair. Those matter more than the byte
writes, not less — and they are easy to miss if you only look for `E6`.

---

## Why the DOS version of the same driver was fine

Because someone hit this in 1997 and added an escape hatch. The DOS driver's own help text, pulled
out of the binary's strings:

```
/ni  - Skip chipset initialization
/de  - Disable Epp check
/db  - Disables Eppbios check
/sf  - Skips fast mode detection
/dp  - Skip PS/2 Dma Arbitration disable
/fp  - Disable PS/2 Dma Arbitration
```

The working DOS line on this machine used **all of them**. The protected-mode miniport exposes
**none** — no switch text, no `AdapterSettings` registry value, nothing. Which is the real lesson:
*a DOS driver may have a documented way to skip its probes while the Windows driver for the same
hardware does not.*

---

## What to do about it

### Before installing any stock driver

Audit it. `dist/post-install-fixes/scripts/xt_port_audit.py` scans a `.MPD`/`.VXD`/`.PDR`/`.SYS` for
fixed-port I/O landing in the XT's aliased device blocks:

```
$ xt_port_audit.py SD120PPD.MPD
  candidates in XT system ports : 283
  plausible (confidence >= 3)   : 29
  of which WRITES               : 29   <-- these are the dangerous ones
```

**Note the 283 against the 29.** A raw byte scan is *not* evidence — `E4`-`E7` occur constantly
inside data and mid-instruction. The tool scores each hit on the idiom around it (`cli`,
`mov al,imm8`, the `eb 00` I/O-delay pattern, `push`/`pop`), and you should still read the context
of anything it flags before believing it. It also cannot see `DX`-addressed I/O, which no static
tool can resolve.

### If it is already installed and your keyboard is gone

You do not need to reinstall. Device Manager still works with the mouse alone: find the device,
Remove, reboot.

### To fix the driver

`dist/post-install-fixes/scripts/patch_sd120ppd_chipset.py` replaces every `out` to `0x22`, `0x23`,
`0x24`, `0x25` and `0x94` with two `NOP`s — **98 sites**, byte and word forms. It leaves the reads
alone, because reading the 8259 has no side effect and the detection then simply finds nothing,
which is what `/ni` achieves in the DOS build.

It deliberately does **not** touch `out 21h`. Every one of those sites is a paired save/restore
around the probe, so they are already neutral; removing one half of a pair would be worse than
leaving both.

Verified: `Patched: 98`, file size unchanged, all edits inside `.text`, control flow untouched,
and `--revert` round-trips **byte-perfect** back to the original md5. That last check is worth
doing on any binary patch — the first two attempts here did not round-trip, once because the site
map recorded the offset but not the original opcode, and once because the revert restored the
opcode byte and left the port byte as `0x90`.

---

## The generalisation

This is the same shape as another bug found on this machine: Windows 9x drivers assuming a 24-bit
DMA reach where the hardware has 20 bits. Both are **a driver assuming AT-class hardware on a
machine that decodes fewer address lines.**

So the rule is not "beware this driver". It is:

> On XT-class hardware, treat every stock driver's hardware probe as a write to a device you did
> not intend to touch, until you have read its port accesses and proved otherwise.

Symptom to recognise: **one interrupt-driven device stops working and the others do not.** Keyboard
gone, serial mouse fine, no error message anywhere. Look at the 8259 mask before you look at the
driver stack.

---

*Every measurement here was taken with [COMrade](https://github.com/yyzkevin/COMrade) (Kevin
Moonlight; Windows 95 port `COMR95.EXE` by Ahmad Byagowi). The root cause was found by static
analysis, but it stayed a theory until COMrade could read the ports on the running machine — the
difference between "this would explain it" and "this is what the hardware does".*

*Findings from the [Windows 95 on an Intel Inboard 386/PC](https://github.com/Mike1978uk/win95-intel-inboard-386pc)
project. Detail in [issue #22](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/22)
and [`ls120_keyboard_root_cause.md`](ls120_keyboard_root_cause.md).*
