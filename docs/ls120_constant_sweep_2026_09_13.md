# LS-120 driver: a full constant sweep against the vendor binary

2026-09-13. Owner's instruction, and it is technique 112 applied to the whole driver rather
than to one symptom:

> *"look for other outliers across the whole driver not just the start up - we have the
> disassembled driver lets compare it - a full sweep will be less time consuming than multiple
> bed runs"*

He is right on the cost as well as the method. This pass took no boots and no hardware.

## Sources compared

| | what it is | why it counts |
|---|---|---|
| `SD120PPD.MPD` | Imation's own Win95 miniport, disassembled in full (`SD120PPD_MPD.asm`, 22,503 lines) | **works on this exact hardware** |
| `reference_gpl/pf_extract.c` | Linux `pf.c`'s constants, held locally | a second independent working implementation |
| `LS120MP.ASM` / `LS120TR.ASM` | ours | the one under test |

## The headline: our waits are ITERATION COUNTS, theirs are TIME BUDGETS

Every vendor wait is `count x ScsiPortStallExecution(10)`, so the duration is explicit and does
not depend on how fast a register read happens to be. Linux is the same shape: `udelay(50)` per
poll. **Ours are bare iteration counts**, which means their duration is set by whatever a nibble
register read costs on the machine they land on — and on this machine that is ~30 us
(technique 109: ~3.9 us of Inboard-to-bus sync per port access, ~8 accesses per nibble read).

## Every delay and wait in the vendor binary

Extracted mechanically: `call 0x5201` is the `ScsiPortStallExecution` thunk (IAT `0x1187c`),
`call 0x520d` is its millisecond wrapper (`stall(1000)` in a loop).

| rva | shape | budget | waits for |
|---|---|---|---|
| `0x1ac7` | `esi = 500,000`, `stall(10)` | **5 s** | BSY clear |
| `0x1d35` | `edi = 1,000,000`, `stall(10)` | **10 s** | BSY clear **or** DRQ set |
| `0x1d80` | `[ebp+0x10] / 10`, `stall(10)` | caller's, in us | a specific phase (ireason match) |
| `0x14b2` | `60,000,000 / 10`, overridable from `[0x203f0]` | **60 s** | command completion |
| `0x2043` | `delay_ms(2000)` | **2 s** | settle after reset, before probing |
| `0x206d` | `delay_ms(50)` | **50 ms** | between two probe steps |

## Linux `pf.c`, from the local extract

```c
#define PF_SPIN_DEL  50                              /* us per poll */
#define PF_TMO       800                             /* jiffies     */
#define PF_SPIN      (1000000*PF_TMO)/(HZ*PF_SPIN_DEL)   /* = 160,000 */
```

**160,000 x 50 us = 8 seconds**, paced.

## Ours, and the outliers

| what | ours | vendor | pf.c | verdict |
|---|---|---|---|---|
| **post-command "data done" wait** | `LS_SPIN_BSY` **2,000 unpaced ~= 60 ms** | 5 s | **8 s** | ⛔ **133x short** |
| **wait for DRQ** | `LS_SPIN_DRQ` **1,000 unpaced ~= 30 ms** | 10 s | 8 s | ⛔ **266x short** |
| reset settle | `0FFFFh` = 65,535 ~= **2 s** | 2 s | — | ✅ **matches** |
| ready state (state machine) | `LS_TICKS_READY` 8000 ticks = **8 s** | — | 8 s | ✅ matches |
| cold start | `LS_TICKS_COLD` 3000 = 3 s | — | — | plausible |
| retries | `LS_MAX_RETRY` 5 | — | 5 | ✅ matches |
| post-CDB delay | `LS_Delay1ms` 1 ms | 50 ms at its own probe step | `mdelay(1)` | ✅ sourced from pf.c |
| per-command timeout | **none** | 60 s, overridable | — | ⚠ absent |
| pacing between polls | **none** | `stall(10)` | `udelay(50)` | see below |

### Two of our own constants are right, which is what makes the other two indefensible

`LS_TICKS_READY` is commented *"8 s, pf.c's PF_SPIN"* — **correct**, and it is the same number
this sweep derives independently. `LS_AtaSoftReset` uses 65,535 iterations and its comment
reasons about the cost explicitly: *"a nibble read is ~8 port accesses at ~3.9 us ... roughly
30 us an iteration - no calibrated delay to get wrong"*.

So the driver contains both the right budget and the right reasoning — **and neither was applied
to `LS_PfWait`, the routine whose own comment says it is "Linux pf.c's pf_wait, transliterated".**
It transliterated the logic and not the timeout.

### The pacing question answers itself on this machine

pf.c paces at 50 us and the vendor at 10 us because on ordinary hardware a register read is
quick enough to hammer the port. Here a nibble read costs **~30 us of bus** on its own, so the
loop is already self-pacing at close to pf.c's rate. **We do not need to add a delay; we need
the right count.** That is also why raising the count does not make the bus problem worse than
pf.c's own design.

## The other outlier: the command surface

Vendor `HwStartIo` (rva `0x29ac`) dispatches on `SRB->Function` and answers **four**:

| code | function |
|---|---|
| `0x00` | `EXECUTE_SCSI` |
| `0x02` | `IO_CONTROL` |
| `0x10` | **`ABORT_COMMAND`** |
| `0x12` | **`RESET_BUS`** |

Everything else gets `SRB_STATUS_INVALID_REQUEST` (`mov byte ptr [eax], 6`) and a notification.

**We answer one** (`LS120MP.ASM:755`) and fail the rest. `ABORT_COMMAND` is what SCSIPORT sends
when a command has taken too long — so on the exact path where our short timeouts bite, we then
fail the recovery request as well. `HwResetBus` is registered, so the direct entry point exists;
the SRB form is not handled.

## What this predicts about the observed failure

The bed (2026-09-13) issued READ CAPACITY **20 times**, each giving up after **2,058 status
polls** — `LS_SPIN_BSY` hit dead on. The drive holds BSY past 500 ms after a media command.
60 ms against 500 ms+ is the whole fault, and the hourglass with no error dialog is what a
bounded timeout with no device error to report looks like.

## Changes this sweep justifies

1. **Give the post-command wait a real budget** — 8 s, pf.c's `PF_SPIN`, the number already used
   correctly elsewhere in this driver.
2. **Do not spend it inline.** 8 s of spinning inside a timer callback is technique 98's
   "a result the user would not tolerate". The wait belongs in the state machine, one poll per
   tick, exactly as `LS_ST_READY` already works.
3. **Answer `ABORT_COMMAND` and `RESET_BUS`** rather than failing them.

⚠ **Not changed, and why:** `LS_Delay1ms` stays at 1 ms — it is `pf_atapi`'s `mdelay(1)` at the
same point in the same sequence. The vendor's 50 ms is at a different step of its own probe and
is not the same delay.

---

## The fourth source, and the boundary that must not be crossed

Owner, mid-sweep, and both points were needed:

> *"don't forget the dos has more switches though so it may differ in process if switches are pulled"*
> *"we don't want to reintroduce the keyboard bug inadvertently"*

### `SD120PPD.SYS` changes PROCESS, not just configuration

The DOS driver's own help strings name switches that select different code paths, not
parameters:

| switch | what it changes |
|---|---|
| **`/dm`** | *"disable read multiple mode"* — so the DOS driver **has** a read-multiple path |
| **`/di`** | *"Operate in polled mode"* — so it can also run interrupt-driven |
| **`/rx` 0-11 / `/wy` 0-4** | the 12x5 transfer-mode matrix (`TRANSPORT_SPEC.md` §9) |
| `/sf` | skip fast-mode detection |
| `/ni` | skip chipset initialisation — **what this machine runs** |

**Consequence for this sweep:** the constants above come from the **miniport**, which is the
right reference for a miniport. But the DOS driver is the binary that demonstrably works on this
machine *today*, and it reaches the drive by a path that may be switch-selected. Its own loop
counters (`0x8000` x22, `0xFFFF` x5) are in the same seconds-not-milliseconds bracket, which
corroborates rather than contradicts — but **no timing conclusion here should be extended to the
DOS path without checking whether a switch gates it.**

### ⛔ Where the keyboard bug lives, and why this change cannot reach it

Issue #22's original cause (technique 75) is the vendor probing for host chipsets by writing
`0x22`/`0x23`/`0x24`/`0x25`/`0x94` — ports that **alias onto the 8259 on this XT**, leaving the
interrupt mask with IRQ 1 disabled. Those writes live in the vendor's **chipset initialisation
and mode detection**, and technique 108 found them in the **DMA block-transfer path** as well.

**So the sweep has a hard boundary: take the vendor's TIMING, never its INITIALISATION.**
Timing constants are numbers. Init and mode-detection are the code that kills the keyboard.

Verified for the binary built today (`code c30ea1a2`, commit `0515903`):

```
xt_port_audit.py LS120MP.MPD
  candidates in XT system ports : 1
  plausible (confidence >= 3)   : 0
  of which WRITES               : 0   <-- the dangerous ones
```

The single candidate is a false positive: `LS120TR.ASM:531  mov al, 022h` is the **first byte of
the EPAT unlock frame** `22 AA 55 00 FF 87 78`, written to the LPT *data* port. Every port this
driver can form is `LS_BasePort` (`0x378`) plus an LPT offset — there is no hardcoded port
anywhere in it.

**And the structural argument is stronger than the audit.** The new `FINISH` state calls exactly
one thing: `LS_ReadyPoll`. That is the same routine `LS_ST_READY` already calls on every boot,
on the real 5160, with the keyboard intact — Phase 0 passed 2026-09-07, and all three boot logs
of 2026-09-12 show `Init Success` with no keyboard loss. **The change adds no port access of any
kind**; it moves an existing wait from a spin into the tick loop.
