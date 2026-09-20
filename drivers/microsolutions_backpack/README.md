# Micro Solutions BackPack — parallel-port CD-ROM

Reverse-engineering notes for a second parallel-port bridge, requested twice on
the back of the EPAT work: @andrew-hoffman on
[issue #23](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/23)
and @JoshRodd on [86Box/86Box#8010](https://github.com/86Box/86Box/pull/8010).
Tracked in [#37](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/37).

**We own the drive**, so every constant here can end up with a hardware
reading behind it, as the EPAT's did.

## The binary

`BPCDDRV.SYS`, 53,090 bytes, v4.02.CB, *Copyright (c) 1995-2002 Micro
Solutions, Inc. / Written by Ron Proesel*. A raw DOS device driver — no MZ
header, device header at offset 0.

| field | value |
|---|---|
| attributes | `C800` — character device, open/close |
| strategy | `00BC` |
| interrupt | `00C7` |

`BPCDDRV_SYS.asm` is a **full sweep**: 23,591 instructions, 48 `db`, 99.8%
decoded. Produced by `tools/dosdrv_disasm.py`, which emits `db` and resyncs
rather than stopping at the first undecodable byte — the trap recorded at the
head of `../imation_ls120/SD120PPD_SYS.asm`, where an earlier dump silently
covered 21% of the file.

## I/O census

1,291 decoded I/O instructions.

| form | count |
|---|---|
| `out dx, al` | 598 |
| `in al, dx` | 394 |
| `in al, 0x61` | 53 |
| `out 0x22, al` | 42 |
| `in al, 0x23` | 23 |

⚠ **`0x22`/`0x23` alias onto the 8259 on an XT bus.** That is the same
mechanism that made the LS-120 vendor driver kill the keyboard on the 5160.
Irrelevant to emulation, but a BackPack on that machine would need equivalent
probe suppression.

## The port protocol, first decoded sequence at `0x09EE`

The driver carries a per-adapter structure in `SI`:

| offset | use |
|---|---|
| `[si]` | base I/O port (word) |
| `[si+2]` | a byte written to DATA **inverted** |
| `[si+7]`, `[si+8]` | saved DATA / CONTROL state |
| `[si+0x14]` | delay loop count |

```
mov dx,[si]      ; base+0 DATA
inc dx           ; base+1 STATUS
in  al,dx
test al,1
out dx,al        ; conditional
inc dx           ; base+2 CONTROL
in  al,dx
test al,1        ; nStrobe
and al,0xFE
out dx,al  x5    ; five writes - a settling sequence, not a typo
sub dx,2         ; back to DATA
in  al,dx        ; bidirectional read
mov al,[si+2]
not al           ; ⭐ BackPack inverts data on the wire
out dx,al
add dl,2         ; CONTROL
call 0x1810      ; status helper
and al,0x10      ; IRQ enable
or  al,4         ; nInit
out dx,al
```

Two things worth carrying into any model:

- ⭐ **Data is inverted on the wire** (`not al` before `out`), which matches
  Linux `paride/bpck.c`'s treatment.
- The delay at `0x0A23` is an **`xchg` with memory** loop — a bus-locked
  instruction used deliberately as a calibrated period delay.

## Not yet done

- Cross-read against Linux `paride/bpck.c` — **not held locally**, public GPL.
- `lpt_sniffer` in 86Box to watch the detect sequence from the driver itself.
- Register traces off the real drive.
- `src/device/lpt_bpck.c`, and wiring `CDROM_BUS_LPT` (declared in upstream
  `cdrom.h`, referenced by nothing).

⭐ **None of this needs the 5160.** BackPack is not XT-specific, so the model
can be built and evaluated on a stock emulated machine with an ordinary
parallel port.

## Measured against the model, 2026-09-20

`src/device/lpt_bpck.c` (kept here under `86box/`) implements the wire
protocol, transliterated from 86Box's `lpt_ditto.c` — the Ditto is a different
product, but Iomega shipped it on this same Micro Solutions bridge, so layer 1
is common and already proven. Layer 2 is a logging stub: reads return what was
written, and every access is traced.

Run in a clean 486 bed (`tools/fixtures/86box.cfg.bpck`) with the vendor
`BPCDDRV.SYS` v4.02.CB loaded by `DEVICE.COM`. 6,790 traced lines.

**The protocol is right.** The driver knocks three times on SELECT with the
unit address on the data lines, connects, and reads the ident response as the
complement pair (`R1 -> 40`, then `78`). It then runs a register conversation:

| order | access | our stub returned |
|---|---|---|
| 1 | read `04` | `00` |
| 2 | write `04` | — |
| 3 | write `05` | — |
| 4 | read `0B` | `00` |
| 5 | write `07` | — |
| 6 | write `04` | — |

then disconnects and retries. It rejects the pod because the stub answers zero;
what those two reads must return is the open question.

### The register map, from the binary

Two layer-1 primitives, both taking the register number in `AL`:

- `0xC05` — address a register
- `0xD5F` — read the addressed register

⭐ **The ATA task file is at `0x40`.** At `0x1573` the driver forms the register
number as `mov al,0x40` / `or al,[bp+8]`, then calls the block transfer at
`0x126A` with a count and a far buffer pointer. So task-file offset *n* is
register `0x40 | n`, which is where `lpt_ditto.c` puts the Ditto's 765 — the
bridge reserves the same window for whatever controller is fitted.

- `0x0B` bit 7 is a flag: the helper at `0x16D7` addresses `0x0B`, reads it and
  returns `AL & 0x80`.
- Registers the driver addresses at all: `0x0A`, `0x0B`, `0x0C`, `0x0D`, `0x0E`,
  `0x13`, `0x2C`, `0x38`, `0x40`, `0x80`.

### Next

Give `0x04` and `0x0B` the values the driver accepts, then hang the ATAPI
engine already written in `lpt_epat.c` off the `0x40` task file, bound to a
CD-ROM rather than the LS-120. `CDROM_BUS_LPT` is declared in `cdrom.h` and
referenced by nothing, so the bus enum is already there.

⚠ **This driver is not XT-safe.** It writes `0x22` and reads `0x23`, which alias
onto the 8259 on an XT bus — the mechanism that killed the keyboard with the
LS-120 vendor driver. Irrelevant on an AT-class machine, which is where all of
this was measured.

## The knock must not be gated on being connected

The first model connected once and then spun: 6,790 traced lines, one connect,
no chain scan. The knock test had been put behind `if (!dev->connected)`, so a
connected pod could never be re-addressed. Evaluating it unconditionally — the
host may knock at any time, and only a SELECT edge returns early — changed the
trace completely: 55,756 lines, a full chain scan of units 01..09+, two
connects, and the driver going on to real work.

## The pod identifies itself from a 93C46 EEPROM

Register `0x06` is the serial EEPROM, bit-banged. From the trace:

| bit | line |
|---|---|
| `0x08` | CS |
| `0x02` | DI |
| `0x01` | CLK |

The opening sequence `08, 0C, 0E, 0F, 0E, 0E, 0F, 0E, 0C, 0D, 0C...` clocks in
`1`, `1`, `0`, then six zeros — a 93C46 START, READ opcode, address 0. The
driver then reads register `0x00` **1024 times**: 64 words of 16 bits, the
whole device. `lpt_ditto.c` does not model this at all — it treats `0x06` only
as an interrupt arm, because the Ditto driver never reads the EEPROM.

⭐ **This is the last unknown before the ATA layer.** The driver rejects the pod
because the EEPROM reads as zeros. What it needs is:

1. which bit of register `0x00` carries DO;
2. the word layout it parses, and the value that says "CD-ROM";
3. then the task file at `0x40` can be given an ATAPI drive.

## The EEPROM protocol, verified

Modelled in `lpt_bpck.c` and confirmed by trace: the driver reads **all 64
words, addresses `00` to `3F` in order**. So the decode is right.

| line | bit of register `0x06` |
|---|---|
| enable | `0x08` |
| CS | `0x04` |
| DI | `0x02` |
| CLK | `0x01` |
| DO | **bit 7 of register `0x00`** |

Routines in the driver: `0x7736` CS on, `0x773E` CS off, `0x7752` clock one bit
out, `0x7776` read DO, `0x7746` wait for ready, `0x77E9` write, `0x782D` read a
word (`1`,`1`,`0`, six address bits, then 16 bits in MSB first).

### The pod ID table, at `0x78E8`

A 16-bit ID selects the pod type and its capability flags:

| ID | effect |
|---|---|
| `0x0603`, `0x0604` | flag `0x20` |
| `0x0401` | flags `0x10` and `0x02` |
| `0x0402` | flag `0x08` |
| `0x0801` | `[si+0x0C] = 6`, flag `0x02` |
| `AH == 0x07` | `[si+0x13] = 5` |
| `AH == 0x0D` | flag `0x04` |
| `AH == 0x00` | probes the port for bidirectionality |
| `0x1101` | flag `0x01`, and `[si+6] |= 0x41` |

### What is still not known

⛔ **Filling all 64 words with `0x1101` changed nothing** — the trace is
identical and the task file at `0x40` is still never touched. So the ID is not
read from a raw word: something validates the image first, a header or a
checksum, and that check fails on a uniform pattern.

⭐ **The cheapest way past this is the hardware.** The owner has the drive, so
a dump of a real pod's 64 words settles the layout, the checksum and the ID in
one go — no further disassembly. Until then the model reads as an erased part.

## BPCKEE.COM — dumping a real pod

`tools/gen_bpckee.py` builds a standalone DOS program that reads all 64 EEPROM
words and writes them to `BPCKEE.BIN` as little-endian words. **No BackPack
driver is loaded and none is needed**, which is what makes it XT-safe: it does
`IN`/`OUT` on `0x378`-`0x37A` and three INT 21h calls, and nothing else. No PIC
masking, no `0x22`/`0x23`, plain 8086 encodings only.

⭐ **Verified in the bed before it went near hardware.** The model was loaded
with `0xA000 | word` — a pattern neither zeros nor `FF`s, so a wrong dump could
not pass — and the program returned all 64 words correctly.

Run it at a DOS prompt with the pod powered on and nothing else on the chain,
then read `BPCKEE.BIN` back. Those 128 bytes are what the model needs.

## ⛔ Measured on the 5160, 2026-09-20: the knock does not work on hardware

COMrade, pod plugged in and powered, no driver loaded, port `0x378`:

| step | `0x379` |
|---|---|
| idle | `0x78` |
| after `0x37A` = `04` | `0x78` |
| after `0x37A` = `0C` | `0x78` |
| after `04`, `0C` (knock complete) | `0x78` |

**The status line never moves.** The host decides a pod is present by the status
changing across the knock, so on this evidence the pod is not taking the link.
`BPCKEE.COM` returned 128 zero bytes for the same reason: DO is bit 7 and bit 7
of `0x78` is 0, so every bit clocked out read zero.

⛔ **What this invalidates.** The knock in `lpt_bpck.c` was transliterated from
`lpt_ditto.c`, which models a *different product* on the same bridge. It was
then "confirmed" by the vendor driver connecting to our model — but that only
proves the model is self-consistent, not that it matches a BackPack CD pod. The
bed cannot falsify a layer-1 assumption, because both sides of it are ours.

Note also that real idle status is `0x78`, where the model uses `0xD8`.

⭐ **Next: take the sequence from the hardware, not from a model.** Either
capture the vendor driver's own port writes on a machine where it runs, or
transliterate its `0xC05`/`0xD5F` primitives — which are jump-table dispatched
by protocol mode, so they need a careful pass. Until then the connect sequence
is unverified against the real pod, and everything above it is built on sand.

## ⛔ The pod does not answer on the 5160 at all

With `BPCDDRV.SYS NOFAST UNIDIR` loaded from the DOS prompt on the 5160, pod
plugged in and powered, `CDDRIVES.EXE` reports:

```
BPDRIVES Version 4.00
No BACKPACK drives are available.
```

So the **vendor's own driver does not detect the pod on this machine**. Three
observations now agree: the driver finds nothing, `BPCKEE.COM` returned 128
zero bytes, and the status register never moved across a hand-driven knock.

⛔ **This supersedes the conclusion above that "the knock is wrong".** The knock
may well be wrong — it is still unverified against real hardware — but it is not
what is being measured here, because nothing answers the vendor sequence either.
The fault is below the protocol.

What it is not: the parallel port itself, which carries the EPAT/LS-120 bridge
on this same machine at `0x378`.

Worth checking next, cheapest first: pod power supply actually delivering;
the cable; whether this pod needs a bidirectional or EPP-capable port that the
5160's card does not provide; and whether the drive behind the bridge spins up
at all. **Until the pod answers something, no emulator work can be validated
against it** - and the bed cannot substitute, because both sides of the
handshake there are ours.

✅ The keyboard survived `NOFAST UNIDIR`, so the `0x22`/`0x23` hazard did not
bite on this path.

## ⭐ The pod is fine; the 5160 is not the machine; the knock is wrong

The owner's steer settled it: the 5160's parallel card is in **ECP/EPP mode**,
because that is the LS-120's shipping transport there (`/fe`). A BackPack pod
wanting plain SPP sees nothing it understands, which is why *nothing* answered
on that machine - vendor driver included.

On a **Toshiba Libretto** the vendor driver loads and mounts the drive at `E:`.
LPT1 there is `0x378`, the same address.

With the drive proven working on that machine, `BPCKEE.COM` still returned
**128 zero bytes**. That is the positive control the 5160 could never provide:

⭐ **Our connect sequence is wrong.** Not the pod, not the cable, not the port.
The knock was transliterated from `lpt_ditto.c`, a model of a different product,
and it does not open a link to a real BackPack CD pod.

⚠ One confound remains: the vendor driver was resident and owns the pod, so it
may hold the link in a state that ignores a knock. The clean run is a reboot
with **no driver loaded**, then `BPCKEE.COM` alone. Do that before concluding.

### Standing lesson

A model cannot validate its own handshake. Our driver connected to our bridge
because both halves were written from the same guess; three real machines were
needed to show it. Get the sequence from the vendor driver's own port writes -
the primitives are `0xC05` and `0xD5F`, jump-table dispatched by protocol mode.

## The working reference, on the Libretto

With the vendor driver loaded, `CDDRIVES.EXE` reports:

```
BPDRIVES Version 4.00
The following BACKPACK drive is available:
  Drive E: CD-ROM (10X)
```

and the drive reads real media - 32.5 MB across ten directories off a pressed
disc. So the pod is a **10X BackPack CD-ROM**, and the whole stack works on a
machine whose parallel port is in plain SPP.

That is the positive control every earlier run lacked, and it is what makes the
zero dump conclusive rather than ambiguous.

⛔ **The drive's own firmware is not reachable this way.** MSCDEX and the block
driver expose media, not the device: there is no command to read the pod ROM,
and the identity we need is the 93C46, which still requires a working knock.

### Probed by hand on the Libretto, with the drive working

`0x379` idle reads `0x7F`. Driving our knock by hand through COMrade -
`0x378` = unit, then `0x37A` = `04`, `0C`, `04`, `0C` - leaves it at `0x7F` at
every step. On a machine where the vendor driver mounts the drive, **our
connect sequence moves nothing**. The knock is wrong; nothing else is.

⭐ **Next lead, cheaper than solving the knock:** the resident driver read all
64 EEPROM words at init, so its copy is in the Libretto's memory and COMrade
can read memory. Find the TSR's adapter structure and the contents fall out
without needing the protocol at all. The shadow bytes are at `[si+0x10]`
(reg 5), `[si+0x11]` (reg 6), `[si+0x12]` (reg 7), `[si+0x13]` (reg 0x1A),
with flags at `[si+6]`, `[si+0xB]` and `[si+0xF]` - enough to recognise the
structure in a memory dump.

### ⛔ Correction: port mode is NOT the established difference

The section above reasons that the 5160 failed because its card is in ECP/EPP
while a BackPack wants plain SPP. **That is withdrawn.** The owner reports the
Libretto is most likely in ECP mode too, and the drive works there.

So what actually separates the two machines is **not established**. Candidates,
none measured: the parallel card itself, bus speed and timing on an 8 MHz XT
bus, drive strength, or the pod needing something the 5160's card cannot do.

What IS established, and does not depend on the above:

- the pod is a working 10X BackPack CD-ROM that reads pressed media;
- the vendor driver mounts it on the Libretto and finds nothing on the 5160;
- our knock moves the status line on neither machine, including the one where
  the drive works. **That is the fault we own.**

## The live driver, read out of the working machine

COMrade on the Libretto, drive mounted at `E:`. `MEM /D` puts the `IO` block at
segment `020D`; the devices inside it run in listed order, and the `BPCDDRV$`
device header - next pointer, attributes `C800`, strategy `00BC`, interrupt
`00C7`, then the name and a `BPcd` signature - sits at linear **`0x6E00`**.

An 88-byte `.COM` copied all 33,168 resident bytes to a file, which came back
over COMrade as `capture/BPCDDRV_resident_libretto.bin`.

⭐ **Diffing the resident image against the on-disk `BPCDDRV.SYS` isolates
exactly what the driver learned from the pod** - 89 changed runs, everything
else identical. Two of them are ASCII written at runtime and present nowhere in
the file on disk:

| offset | value | |
|---|---|---|
| `0x7FD1` | `17627007` | serial |
| `0x7FDA` | `2696` | model or part number |

and at `0x7F9E` the adapter structure carries `78 03` - the base port - with
`0x7FA0` onward holding the mode and shadow bytes the disassembly names.

⭐ **This is the EEPROM surfacing through the driver rather than the protocol.**
It does not yet give the 64 raw words, but it gives what the driver *derived*
from them on a pod that works, which is the thing the model has to satisfy.

### Why this route matters

The knock is still wrong and the pod still answers nothing we send. But the
driver that does work is sitting in memory on a machine we can read, so its
conclusions are recoverable without solving the protocol first. Next: locate
where those strings are parsed, and the ID word from the table at `0x78E8`
should be in the same structure.

## The connect sequence, from the driver at 0x09EE

⛔ **This was in this file from the first day** - the section above records
`mov al,[si+2]` / `not al` / `out dx,al` and stars it: *"BackPack inverts data
on the wire"*. The model's knock was transliterated from `lpt_ditto.c` anyway,
and three machines were spent rediscovering what the page already said. Read
the component's own notes before building against it.

The full sequence:

1. read STATUS; if bit 0 set, write it back;
2. read CTRL; if bit 0 set, clear it and **write five times** - a settling run;
3. save DATA, then write **`NOT [si+2]`** to the data lines;
4. CTRL = `(old & 0x10) | 0x04` - INIT alone, preserving the IRQ bit; delay;
5. write **`[si+2]`** plain to DATA; delay;
6. `xor al,8` three times, each followed by a delay - the three SELECT edges;
7. `or al,2` - AUTOFD, opening the address probe, which then shifts
   `[si+2] & 7` out in three steps.

Every step is separated by the `[si+0x14]` delay loop, which is an `xchg` with
memory - a bus-locked instruction used deliberately as a calibrated period.

⭐ **`[si+2]` is `0x03` on this pod**, read from the live adapter structure at
`0x7F9E` in the resident image - not the `0x00` a unit-0 assumption gives.

### Still zero, and the likely reason

With the corrected sequence the pod still returns 128 zero bytes, and a
hand-driven knock still leaves `0x379` at `0x7F`. **The vendor driver was
resident throughout, with the drive mounted at `E:`** - a pod that has already
taken the link will not answer a new knock. The outstanding test is therefore:
boot with the driver **not loaded** (Win98 F8, step-by-step, decline the
`BPCDDRV` line) and run `BPCKEE.COM` against an idle pod.

## Where it actually stands

Tested on the Libretto in safe-mode command prompt - **no driver loaded**, pod
powered, port confirmed SPP (ECR at `0x77A` reads `0x15`, mode bits `000`) and
the control register's direction bit clear, so writes do reach the pins.

`BPCKEE.COM` returns 128 zero bytes with the connect sequence transliterated
from `0x09EE` and the address byte `0x03` taken from the live driver. A
hand-driven knock leaves `0x379` at `0x7F` throughout.

⛔ **The remaining fault is one layer down.** The connect is now the driver's
own, but the register framing in the dumper - address a register, write it,
read it back as nibbles - is still the `lpt_ditto.c` guess. The driver does
none of that directly: it calls `0xC05` (address), `0xC90` (write) and `0xD5F`
(read), each of which dispatches through a jump table on the protocol mode in
`[si+3]`.

⭐ **`[si+3]` is `0` on this pod**, from the live structure - so only the mode-0
arm of those three routines has to be transliterated, not all of them. That is
the next job, and it is bounded.

### The ident test, for when the framing is right

From `0x0A83`: with AUTOFD set, status bits 3-5 must equal `[si+2] & 7`; with
AUTOFD cleared, their complement must. That is the presence check, and our
model already implements exactly this pair - the design was right, the framing
around it was not.

## ⭐ The pod answers: it is at chain address 7

Brute-forcing the three address bits settled what reading the live structure
got wrong. `BPCKSCAN.COM` runs the connect for units 0-7 and records the status
either side of the AUTOFD edge:

```
unit 7: AUTOFD set 7F (bits 3-5 = 7)   AUTOFD clear C7 (bits 3-5 = 0)
units 0-6: 7F / 7F - no response
```

⭐ **That is the driver's own presence test passing**: bits 3-5 equal the
address with AUTOFD set, and its complement with AUTOFD cleared. `C7` is the
first time this pod has moved the status line for us.

⛔ `[si+2]` = `0x03` from the live image is **not** the chain address, or the
structure base was misread. Do not trust that reading; the scan is authority.

### The read framing is right, and proves it

With the link up, every register reads `0x77` - exactly `xlatb` of an idle
`0x7F` under the driver's own table (`((s>>3)&7)|((s>>4)&8)`, verified against
all 256 entries). So the decode path is correct and is faithfully reporting an
idle bus, not producing garbage.

### What remains

The link comes up at the ident probe and is gone by the first register access,
so **the connect has a tail we stop short of**: `0x0AB1` to `0x0B66` in the
driver, after the complement check - it clears AUTOFD, re-tests, then branches
on `[si+3]` and `AH`. Transliterate that and the registers should talk.

Tools, all driven over COMrade against the live Libretto:
`tools/gen_bpckee.py` (EEPROM dumper, `--unit`/`--port`), plus
`tools/backpack/BPCKSCAN.COM` (address scan) and `BPCKDIAG.COM` (status and
register probe) - the diagnostics that turned "128 zero bytes" into a located
fault.

## ✅ The EEPROM is read

The connect tail was the whole of it. Driver `0x0AB1`-`0x0B64`: after the
complement check, clear AUTOFD (`ch &= 0xFD`), then write **`ch ^ 8`** and keep
that as the carried control byte. Without those two writes the pod passes the
presence test and goes straight back to idle - which is exactly what we saw.

With them, `BPCKEE.COM --unit 7` returns the pod's 93C46:

```
00: 07F8 0000 0082 4F54 4853 4249 2041 4443
08: 522D 4D4F 5820 2D4D 3531 3230 2F42 3632
10: 3639 0000 ...
2E: 3204
37: 3731 3236 3037 3730 0C08 07CD 0C08 07CD
```

ASCII: **`TOSHIBA CD-ROM XM-1502B/2696`** and the serial **`17627007`**.

⭐ **Independently confirmed.** `2696` and `17627007` are the two strings found
earlier in the *resident driver's own memory* on the same machine, arrived at
by a completely different route. The read is genuine, not an artefact.

Saved as `capture/pod_eeprom_93c46.bin`.

### The full working recipe

| step | detail |
|---|---|
| chain address | **7** (found by scanning, not from the driver's structure) |
| connect | settling writes; `NOT addr` to DATA; CTRL = `(old & 0x10) \| 4`; `addr` to DATA; three `xor 8` SELECT edges; `or 2` for the ident probe |
| presence | status bits 3-5 = addr with AUTOFD set, complement with it cleared |
| take the link | clear AUTOFD, then write `ch ^ 8`; carry that byte forward |
| address a reg | reg to DATA; CTRL = `(carried \| 1) ^ 2` |
| write a reg | value to DATA; CTRL `\|= 1`; `^= 4`; then `&= 0xFE` five times |
| read a reg | CTRL `&= 0xFE` five times; `^= 4`; read status; restore; read again |
| nibble decode | `((status >> 3) & 7) \| ((status >> 4) & 8)`, verified over all 256 |
| every step | separated by a settling delay |

That is the layer the model needs, measured rather than inferred.
