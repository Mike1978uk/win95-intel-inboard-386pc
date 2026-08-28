# The LS-120 miniport and the keyboard: a chipset probe that lands on the PIC

Issue [#22](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/22). Analysis of
`SD120PPD.MPD` (79,872 bytes, 1997-05-26), the Shuttle/Imation LS-120 parallel-port miniport.

**Status: CONFIRMED on real hardware 2026-08-28. The aliasing was measured; nothing in the chain
is now assumed.**

## The measurement

COMrade, at the DOS prompt on the real 5160, read-only:

| port | value |
|---|---|
| `0x21` — PIC1 data / IMR | `0xAC` |
| `0x23` | `0xAC` |
| `0x25` | `0xAC` |
| `0x31` | `0xAC` |
| `0x3F` | `0xAC` |

Every odd port across `0x20`–`0x3F` returns the interrupt mask. **The 8259 is aliased across the
whole range**, so `out 23h, al` *is* `out 21h, al`. The mask at rest under DOS is `0xAC`
(`10101100`): IRQ 1 clear, i.e. the keyboard enabled, as expected.

The driver writes `0x02` there — `00000010`, **IRQ 1 set, keyboard masked, everything else
unmasked** — then ORs in `0x04`. That is the observed symptom precisely: keyboard dead, everything
else untouched.

A small corroboration: COMrade's own port table labels `0x23` "VL82C420 cfg data" — the VLSI
chipset family is one of the very things this driver probes for by name.

## The code

At file offset `0x00eaba` in `.text`, inside the chipset-identification routine:

```asm
fa              cli
e4 21           in  al,21h        ; read the 8259 interrupt MASK
50              push eax          ; save it
eb 00 eb 00                       ; I/O delay
b0 0a           mov al,0Ah
e6 21           out 21h,al        ; write mask = 0x0A   -> bit 1 set = IRQ1 = KEYBOARD MASKED
eb 00 eb 00
e4 23           in  al,23h        ; read back from 0x23
3c 0a           cmp al,0Ah        ; did the value stick?
58              pop eax
e6 21           out 21h,al        ; restore
fb              sti
```

The same shape appears again at `0x00eaa3` writing `0xA0`, and similar sequences at `0x00ddf5`,
`0x00de17`, `0x00e695` and `0x00eb1f`. These are real instructions, not byte coincidences — the
`cli` / save / write / read-back / restore / `sti` structure is unambiguous.

**What it is trying to do:** detect a host chipset whose configuration registers live at the
classic `0x22`/`0x23` index/data pair. Write a known value, read it back, and if it survives,
conclude the chipset is present. On a machine that has such a chipset this is harmless, because
`0x22`/`0x23` are the chipset's own registers.

## Why it is not harmless on an IBM 5160

Two facts collide.

1. **Port `0x21` is the 8259A interrupt mask register**, and this machine has exactly one PIC.
   `0x0A` masks bit 1 — **IRQ 1 is the keyboard**.
2. **The 5160 decodes I/O incompletely.** The 8259 is selected across `0x20`–`0x3F` with `A0`
   choosing the register, so **`0x22` aliases to `0x20` and `0x23` aliases to `0x21`.**

So on this machine the read-back at `0x23` returns the value just written to `0x21` — the probe
**succeeds against a chipset that does not exist.** The driver then proceeds to configure that
imaginary chipset through `0x22`/`0x23`, and every one of those writes lands on the 8259's command
and mask registers instead.

That is a sufficient explanation for losing keyboard input while the mouse (a COM port, not IRQ 1)
keeps working.

## The write that is never undone

The probe above **does** restore the mask, so the probe alone is not the fault. The damage is in the
*configuration* path that runs after the probe returns a false positive. At `0x00cc54`:

```asm
50              push eax
b0 21           mov al,21h
e6 22           out 22h,al        ; chipset index = 0x21
eb 00 eb 00
b0 02           mov al,02h
e6 23           out 23h,al        ; chipset data  = 0x02      <-- never restored
eb 00 eb 00
b0 c2           mov al,0C2h
e6 22           out 22h,al        ; chipset index = 0xC2
eb 00 eb 00
e4 23           in  al,23h
0c 04           or  al,04h
e6 23           out 23h,al        ; read-modify-write         <-- never restored
58              pop eax
c3              ret
```

`push eax` / `pop eax` preserves the *register*, not the *port*. Nothing writes the old mask back.

Translated through the alias on a 5160:

| intended | actual on a 5160 | effect |
|---|---|---|
| `out 22h, 21h` | `out 20h, 21h` | garbage into the 8259 **command** register |
| `out 23h, 02h` | `out 21h, 02h` | **mask = 0x02 — IRQ 1 masked. Keyboard dead.** |
| `in 23h` / `or 04h` / `out 23h` | `in 21h` / `or 04h` / `out 21h` | **mask = 0x06 — IRQ 1 and IRQ 2 masked** |

Final state: interrupt mask `0x06`, keyboard and cascade masked, no restore. A serial mouse on
IRQ 3 or IRQ 4 is untouched — which is exactly what was observed.

Further unrestored read-modify-writes on `0x22` follow at `0x00fc42`, `0x00fc57`, `0x00fc6c`,
`0x00fc7f` and `0x00fc9a` (`in al,22h` / `or` / `and` / `out 22h,al`), all landing on the 8259
command register on this machine.

## Why the DOS driver was fine

The DOS driver was loaded as:

```
DEVICEHIGH=C:\SD120PPD\SD120PPD.SYS /port:378 /IRQ:7 /de /db /ni /sf /dpc /dp /fp
```

`/ni` is documented in the binary's own help text as **"Skip chipset initialization"**. It skips
precisely this routine. The miniport has no equivalent — there is no switch text, no
`AdapterSettings`, and no IRQ string anywhere in it (see #22). The probe cannot be turned off, only
patched out.

## The one thing still to confirm

The aliasing claim is reasoned from XT decoding, **not yet measured on this machine**. It is a
read-only, entirely safe test with COMrade:

```
read port 0x21      ; the real interrupt mask
read port 0x23      ; if this returns the same value, 0x23 aliases to 0x21
```

If they match, the mechanism is confirmed. **Do not write to `0x21` as a test** — that masks
interrupts on a live machine.

## An 86Box fidelity gap, and why the bug may not reproduce in emulation

`src/pic.c` maps the PIC with:

```c
io_sethandler(0x0020, 0x0002, ...);   /* pic_init() - XT and AT: ports 0x20-0x21 only */
io_sethandler(0x0020, 0x0008, ...);   /* pic_init_pcjr() - the PCjr does alias, 0x20-0x27 */
```

So 86Box gives the XT **no alias above `0x21`**. In emulation `in 23h` returns `0xFF` from an
unmapped port, the compare fails, and the driver concludes the chipset is absent — **the bug likely
does not reproduce in 86Box at all.**

That is worth recording twice over:

- It means this particular fix must be validated on real hardware, not in emulation — the opposite
  of the usual advice in this project.
- It is a **fidelity gap in 86Box** of the same family as the XT 4-bit DMA page latch
  ([#7771](https://github.com/86Box/86Box/pull/7771)): real XT hardware decodes fewer address lines
  than the emulator assumes. Worth raising upstream **after** the alias is measured on the real
  machine, and not before — the measurement is the whole value of the report.

## Patch options

Static analysis has located both structures needed.

**A. Neuter the chipset probe.** The routine spans roughly `0xf3fa`–`0xf7b7` (identified by
references to its chipset name strings: `IBM PS/1`, `Winbond W`, `NS PC`, `Eagle`, `VLSI`,
`Intel AIP`, `386/486 SL`, `Compatible`, `Hard Config. Epp`). Forcing it to report "no chipset"
reproduces `/ni`. This is the patch that addresses the keyboard directly.

**B. Force a safe transfer mode.** A 16-byte-record dispatch table begins at file `0xa430`:

```
{ DWORD routine_VA; DWORD routine_length; DWORD routine2_VA; DWORD length2; }
```

Verified: the `NIBBLE Fast` entry at `0xa430` points at `0x18b49` with length `0x2b`, and
`0x18b74 - 0x18b49 = 0x2b` exactly. The driver copies these routines into a buffer and assembles
its transfer loop at runtime.

Read entries run `0xa430`–`0xa510` (NIBBLE Fast/Normal/Slow, UNIDIR Fast/Normal/Slow, TOSHIBA
Fast/Normal, PS/2 Fast/Normal, EPP Normal ×2, EPP BIOS(N), ECP Read, NIBBLE Slow(-)); write entries
run `0xa520`–`0xa5a0`. Pointing every read entry at `NIBBLE Slow` and every write entry at
`WRITE Slow(-)` makes the driver conservative whatever detection decides — a table edit rather than
a logic patch, which is much easier to get right and to verify.

B alone does not stop the probe, so **A is the one that matters for the keyboard**. B is worth
doing anyway for reliability on a 4.77 MHz bus.

## Not done

Nothing has been patched. No file has been modified. The next step is the COMrade port read above,
because it decides whether this whole analysis is right.
