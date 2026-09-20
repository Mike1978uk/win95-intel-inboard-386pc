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
