# What the emulator cannot prove — the 5160 differences for the XT-IDE port driver

Companion to `xtide_pdr_runbook.md`. That file says what to do; this one says which parts of the
driver have never executed on the target and why, so a hardware failure can be read against a list
written before the run rather than explained afterwards.

Everything below is a **known** difference between `vm_xtide_pdr/` and the real 5160. Nothing here
is a prediction about whether the driver works.

## Split the risk: 86Box proves the IOS side, the 5160 proves the transport

| layer | proven where | why |
|---|---|---|
| registration, `DRP_FC_DYNALOAD`, the AER dispatch | 86Box | machine-independent VxD loader behaviour |
| calldown insert, request path, our own TSD, volume publishing | 86Box | pure IOS logic, and every 2026-09-01 bug lived here |
| register stride, high-byte latch, bus timing | **nowhere yet** | 86Box's `hdc_xtide.c` is a different card |
| the Real Mode Mapper handoff for the boot volume | **nowhere yet** | not designed, let alone run |

Logic faults present as protection errors four layers from their cause. Transport faults present as
wrong bytes, a timeout, or a status that will not clear. Doing the logic in emulation first means
the hardware run only has to explain the legible kind.

## The differences, and what each one costs

### 1. Register stride — the emulator structurally cannot test ours

86Box emulates a classic XTIDE at **stride 1**. The Lo-tech XT-CF rev 3 in the 5160 leaves A0
undecoded, so its taskfile is at **`base+2N`** — measured 2026-08-31, alternate status at
`base+14*2` = `31Ch`. A stride-2 build has therefore **never executed a single instruction**.

Mitigated, not removed: `XTIDE_DetectStride` now settles the stride by reading Status (`+07*S`)
and Alternate Status (`+14*S`) and keeping the candidate where they agree — one register on any
correct map, measured `92h` against `50h` on the wrong one. It **writes nothing**, which is what
makes it safe; the original objection to autodetection was that probing stride 2 by writing on
stride-1 hardware puts `0ECh` into DEVICE CONTROL and leaves SRST asserted.

**First hardware run should still pin `-Stride 2`.** Autodetect is one more thing that has never
run on the card; test it on a run where a failure is attributable.

### 2. There is one disk, and it is the boot disk

No slave, and adding one is not cheap: CF-to-IDE adapters are commonly master-only in True IDE
mode, two CF cards on one channel are unreliable, master/slave is a jumper on the device, and
XTIDECFG would need reconfiguring. So:

- `-ClaimMask 1` (master) is the only option on this machine — there is no secondary-disk halfway
  house of the kind that produced drive D: in emulation;
- the safety net is a **host-side image of the CF**, taken with the card in the reader. For CF that
  is strictly better than a scratch device: it captures the exact tested state and restores by file
  copy.

### 3. `-ReqMarker` must be OFF

The marker writes a sector at LBA 16000 of the **claimed unit**. In emulation that is a scratch
image; on the 5160 it is the CF the machine boots from. The whole instrumentation channel that made
the emulator runs cheap is unavailable here, by design.

What replaces it: `BOOTLOG.TXT`, read from the card in the reader afterwards, plus the probe's own
delay channel — `XTIDE_FailCode` seconds burnt in `XTIDE_Probe`, which the boot log timestamps at
roughly 33 ms per tick.

| fail code | meaning |
|---|---|
| 0 | success |
| 1 | no I/O resource reached the DDB |
| 2–5 | IDENTIFY: BSY stuck / rejected / no DRQ / model string not sane |
| 6 | slave did not answer with its DEV bit |
| 7–9 | transfer: BSY stuck / no DRQ or ERR / geometry is zero |
| 10 | 512 bytes read, no `55AA` |
| **11** | **neither stride made Status and AltStatus agree** |
| 12–15 | write path |
| 24+n | read verified, no slave to write to (n = partition type) |
| 40+n | read and write both verified |

### 4. Transport: the real card answered 8-bit PIO with no latch

Measured from DOS on 2026-08-31: full IDENTIFY through `0x300`, 8-bit PIO, **no high-byte latch**.
`XTIDE_TryTransport` autodetects PIO8 then LATCH and keeps whichever validates, so this is handled
— but note the emulator exercises the *other* branch, so the branch that will run on hardware is
the less-tested one.

**ATAPI/CD is out of scope on this card, and as of 2026-09-05 that is measured, not inferred.**

Settled from a photograph of the board. The silkscreen reads `XTIDE Universal BIOS / Adapter Type
XT-CF` and `IO: 300-31Fh` — 32 ports, which is the stride-2 decode, independently corroborated.
The logic is two `CD74HCT688` comparators, an `SN74HCT139` and a buffer. **There is no 74x373 /
74x573 / 74x574 on the card: no high-byte latch.** XT-CF is the XTIDE Universal BIOS device type
that transfers through the CF card's *own* `Set Features 01h` 8-bit PIO mode, so the data path is
8 bits by design. ATAPI has no 8-bit data mode, so it cannot work through this card. The board
photo also closes `bDevice = 0x0A`, open since 2026-08-31: it is XT-CF, not XTIDE rev 2.

**The same conclusion had been held for five weeks on much weaker grounds, and that is worth
recording.** `XTIDE_TryTransport` tries PIO8 first and keeps the first branch that validates. The
device on the cable is a CF card, which accepts 8-bit PIO on a latched card and an unlatched one
alike, so the run of 2026-08-31 could not have distinguished them — it reported which branch
succeeded, not what the board can do. "D8–D15 unconnected" was written into four documents, three
saying *permanently*, on the strength of a test that could not have come out the other way.

It was briefly reopened on 2026-09-05 by the vendor's description of this exact card (TexElec,
<https://texelec.com/product/isa-compactflash-adapter/>): *"This card will cut the data in half
and send it one byte at a time to the bus instead of two like a native IDE controller"*, plus
*"will also work with ATA-2 compliant hard drives"*. Read as board behaviour that describes a
latch. It is not: the **CF card** does the splitting, in its own 8-bit mode, and the hard-disk
claim holds only for drives that implement that optional mode. Marketing copy about the device on
the cable, mistaken for a statement about the card. One photograph ended it — see technique 95.

The 40-pin header is **not** the constraint. The owner already runs an IDE cable from it to a
separate IDE-to-CF adapter, so a drive can be attached with no modification. The bus width is the
constraint, and no command set makes 8 wires carry 16 bits. CD-ROM on this machine has to come
from an interface that is natively 8-bit — see `docs/next_session_2026_09_06.md`.

### 5. No interrupt, and a 4.77 MHz bus

The card is jumpered without an IRQ, so every wait is a poll. `XT_SPIN` is 400000 iterations of
`in al,dx` — roughly **0.6 s** on this bus. That number is the unit of the arithmetic in Technique
78: one timeout costs tens of boot-log ticks, so a failure that costs two ticks did not time out
and no spin loop ran.

### 6. Port aliasing across `0x20`–`0x3F` — not modelled in 86Box

The XT decodes I/O in 32-byte blocks (IBM's own map, and the DubaiXTClone schematics show
`XA0`–`XA4` never reaching the decoder). 86Box maps the PIC as two ports, so none of this
reproduces there.

This driver touches only `0x300`–`0x31F` and writes nothing outside it, so it is not directly
exposed. It matters here for one reason: **a clean emulated run is not evidence about fixed-port
behaviour on this machine**, and any future addition to the driver that writes a fixed port below
`0x100` needs `dist/post-install-fixes/scripts/xt_port_audit.py` run over it first.

### 7. The boot volume already has an owner

`RMM.PDR`, the Real Mode Mapper, drives storage on this machine through the option ROM's real-mode
INT 13h — that is what Technique 74 established. The boot disk's DCB therefore **already exists**
when we load. Owning C: means claiming that DCB and getting IOS to retire the mapper, not creating
a volume; `XTIDE_WantIop` currently refuses any DCB we did not claim, and both that and `ClaimMask`
would have to change deliberately.

This handoff is the one part of the stack nothing has touched. It is also the actual goal of issue
#3 — MS-DOS compatibility mode ends when the *boot* volume is 32-bit — which is why it gets a
design and an emulator run on a clone before it gets a hardware run.

### 8. Emulator leniency that does not exist on silicon

Technique 37, kept here because it has already caused one wrong conclusion on this project:
`kbc_xt.c` auto-clears the XT keyboard latch after ~50 ticks when nothing acknowledges it. Real XT
hardware has no such escape. If a hardware run loses keyboard input where the emulator did not,
that is the difference, not a regression — and per Technique 76 a Windows-side fault that costs
input is close to undiagnosable in place, so the evidence has to be written to disk during the
failing boot and read from the card afterwards.

## Deployment discipline (Technique 75)

- Copy from the host with the card in the reader. A `.PDR` in `IOSUBSYS` is open and locked while
  Windows is running, and a `COPY` over it fails with a sharing violation in a window that closes
  before anyone reads it.
- `md5sum` **at the destination**, against the artefact you meant to ship — not the staging copy.
  A void deployment has already been reported as a negative result once on this project.
- CRLF anything text. `.gitattributes` handles the repo; writing straight to the card bypasses it.
- Take the host-side image **before** the run, not after.
