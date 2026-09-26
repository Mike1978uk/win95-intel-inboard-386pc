# #7: the Setup stall is the floppy motor wait, not the display

Traced in 86Box on 2026-09-26: bed `vm_vpicd_irq2`, a fresh clone of the golden pre-monolith image
for every run, logging build `86box_3c509b` branch `diag-issue7` (heartbeat with PIC state, V86
stack, per-IRQ and INT 13h / INT 15h AH=86h counts; `INBOARD_HEARTBEAT=1`). Logs:
`docs/captures/run7a.log`, `run7b.log`.

## What the owner saw, and what changed since the issue was filed

Setup's final phase stops after **Programs on the Start menu**, before **Windows Help**. The Setup
dialog stays drawn and the mouse stops. The issue text (2026-08) describes a black screen just
before Help; the owner noticed the stall point had moved earlier, and it was not recorded until now.

## Measured

| run | VPICD | result |
|---|---|---|
| 7a | stock `VPICD_INBOARD` | froze at the same step |
| 7b | #42 patch `VPICD_INBOARD_IRQ2` | froze at the same step |

- The CPU keeps running: ring-0 VMM and VDMAD, and a V86 BIOS loop. No ring-3 code runs again.
- The PIC is clean throughout (`isr=00`). IRQ 4 is unmasked and the mouse's interrupts are still
  raised and delivered (666 -> 741 across the stall); nothing is left running to draw the cursor.
- The Mach8 never switches to its 8514 side (no display-switch write after POST), which retires
  technique 61's display-path theory for this symptom.

The V86 sample at the stall:

```
F000:ECB6  in the refresh-count delay (CX frozen at 102E, IF=0)
V86 stack  3202 0008 09B2 EFA0 ...    return F000:09B2; EFA0 is the diskette parameter table
```

`F000:096D` (U19 chip) is the diskette motor-start wait: `INT 15h AX=90FDh`, then `INT 15h AH=86h`
to wait the motor-start time from the parameter table; if that fails, `mov cx,205Eh` / `call ECA0`
counts DRAM-refresh ticks on DMA channel 0 (`in al,0`) with interrupts off. The XT BIOS does not
implement `AH=86h`, so the fallback always runs. Under Windows, VDMAD traps port 00h (its port
table, file `6420h`, maps it to its address-register handler) and the count never moves: the
System VM hangs inside the floppy BIOS call.

This fits every known property of #7: both machines, a reboot gets past it (Setup's second pass
does not touch the floppy), and the stall point depends on when the floppy is first touched.

## Fix under test: `tools/wait86/WAIT86.COM`

A 247-byte resident loaded from `AUTOEXEC.BAT` that implements `INT 15h AH=86h` by waiting on the
BIOS tick count at `0040:006C`, which Windows advances in every VM. The BIOS then never reaches the
refresh loop. Each tick wait is bounded, so a stopped timer shortens a wait rather than hanging it.

Rejected for now: un-trapping port 00h in VDMAD. The BIOS loop also depends on the DMA flip-flop
(`out 0Ch`), which VDMAD virtualises and the floppy's own channel-2 programming shares.

Not done: `HSFLOP.PDR` loading would take the floppy out of V86 altogether (see #18); that is the
long-term route.
