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
