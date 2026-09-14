# LS120MP.MPD — design

**What the driver must be, and the invariants that make it safe.** `IMPLEMENTATION.md` is the
build spec — command sequences, constants, the known-wrong list. This file is the architecture
and the rules the architecture exists to enforce. Where they disagree, this one is wrong and
should be fixed here.

Written 2026-09-14, from a failure the owner found by unplugging the drive.

---

## 0. The failure that prompted it

> *"if the drive is disconnected it stops windows reaching desktop. I booted with the drive
> disconnected and it sat and waited, and when I reconnected it, it got to the desktop."*

That is the whole design problem in one sentence. **A storage driver for a removable device on a
detachable cable must treat "nothing is there" as a normal, cheap, immediate answer.** Ours treats
it as a device that has not replied yet, and waits.

It is also a self-diagnosing report: reconnecting the drive released the boot, so the driver was
not crashed or looping — it was waiting, and the thing it waited for arrived.

---

## 1. The defect, measured

Three faults compound. Each is small; together they are unbounded.

### 1.1 `LS_RegRead` cannot fail

`LS120TR.ASM:608`. It ends `clc / ret`. There is no path that returns CF=1 — so every
`jc` that guards a register read is dead code:

| site | guard | status |
|---|---|---|
| `LS_AtaSoftReset` | `jc lsrst_next` | never taken |
| `LS_ReadyPoll` | `jc lrp_fail` | never taken |
| `LS_WaitNotBusy` | `jc lwnb_fail` | never taken |

With no bridge on the cable the SPP status port floats high, so both nibbles read `0Fh` and the
routine returns **`0FFh`, reported as a successful read**. `0FFh` has BSY set. The driver
therefore sees a device that is permanently busy, which is exactly the state it is built to be
patient about.

This is [[feedback-a-self-test-must-be-able-to-fail]] inverted: the *failure* value is
indistinguishable from a legitimate one, and the reader asserts success regardless.

### 1.2 One step costs seconds, not a tick

`LS_AtaSoftReset` (`LS120TR.ASM:376`) polls with `mov ecx, 0FFFFh` — the only loop in the
transport that does not use an `LS_SPIN_*` constant.

A nibble register read is **8 port accesses**: one register select, three control writes, a status
read, two control writes, a second status read. At the measured 5.77 µs per 8-bit access
(technique 109e — 3.90 µs fixed synchronisation plus 1.87 µs per byte) that is **46 µs per read**.
The comment at that loop estimates 30 µs; it is optimistic by half.

```
65,535 iterations x 46 us = 3.0 seconds, inside one timer callback
```

`LsTimer`'s contract says *"each tick does ONE bounded step"*. The step is bounded — at three
seconds.

### 1.3 The tick budget assumes the step is a tick

`lt_coldstart` retries `LS_ColdStart` for `LS_TICKS_COLD = 3000` ticks, commented *"3 s to open a
bridge that is present"*. That arithmetic is only true if a tick costs ~1 ms.

```
3,000 re-arms x 3.0 s per step = ~2.5 hours, per SRB
```

SCSIPORT issues at least one INQUIRY per target, and `LS_MAX_TARGETS = 2`. The boot does not
resume until the drive is plugged back in — which is precisely what was observed.

---

## 2. Invariants

These are the rules. A change that breaks one is a bug even if it passes on the bench.

### I1. Absence is an answer, not a timeout

The driver decides once, early and cheaply, whether a drive is on the cable. If not, it reports no
devices and does not touch the port again until asked to rescan. **No budget, no retries, no
waiting.** A machine with no LS-120 attached must boot exactly as fast as one with no driver
installed.

### I2. Every read reports whether it is real

A register read returns a value **and** a validity. All-ones and all-zeros are not values; they are
the two ways a floating bus lies. Any caller that acts on a register must handle "no answer" as
distinct from "busy".

### I3. Waits are states. Transfers are steps.

| | rule | bound |
|---|---|---|
| **wait** — for BSY to clear, for DRQ, for spin-up | never a loop. One poll per tick, re-arm, count ticks | as many ticks as the budget allows |
| **transfer** — the CDB, a data block | atomic; it cannot be suspended mid-phase | `MaximumTransferLength` x the transport's measured per-byte cost |

The distinction is the whole architecture. A wait has no upper bound that is knowable in advance,
so it must not be spent inside a callback. A transfer's cost is arithmetic from its length, so it
can be.

### I4. Every time bound is derived, not chosen

A spin count is a time budget divided by a measured per-access cost. Write both numbers in the
source next to the constant. `LS_SPIN_BSY equ 2000 ; ~90 ms` is the correct form; `0FFFFh` with no
comment is how §1.2 happened.

### I5. The worst case is a stated number

For every configuration — drive absent, drive present but empty, drive spinning up, cable
unplugged mid-transfer — the design states the wall-clock cost and where it is enforced. §5 is that
table. A configuration missing from it has not been designed, only coded.

### I6. Calibrate at init; do not hardcode this machine

From the 09-13 handoff, and it stands. The vendor shipped a hand-picked switch table (`/rx` 0-11,
`/wy` 0-4, `/di`) and it fails here because nobody chose a rung for a 386 accelerator in a 1983
chassis. Measure the largest transfer that survives, on whatever machine the driver loaded on. One
binary, no switches.

---

## 3. Layers

Four, each with one job and a stated failure mode. The current code has these boundaries already;
what it lacks is the failure mode at each one.

| layer | job | fails by |
|---|---|---|
| **port** | SPP/ECP register access at `base`, `base+1`, `base+2`, `base+402h` | cannot fail — it is `in`/`out` |
| **bridge** | CPP connect/disconnect, container addressing, nibble and ECP block moves | **"no bridge answered"** — the layer §1.1 is missing |
| **device** | ATAPI task file, packet commands, sense | "the drive refused", with a sense key |
| **miniport** | SRB dispatch, the state machine, SCSIPORT contract | an `SRB_STATUS` |

The rule that keeps them honest: **a lower layer never waits on behalf of a higher one.** The
bridge layer answers "here is a byte" or "nothing answered"; it does not decide that a busy drive
is worth another second.

---

## 4. Presence detection

Invariant I1 needs a test that is definitive, cheap, and cannot be fooled by a floating bus.
**Linux already has all three tiers of it**, in `reference_gpl/epat.c` — locally held, do not
fetch.

### 4.1 The container map, because it decides which tier is which

`epat.c:45` — `cont_map[3] = { 0x18, 0x10, 0 }`, and the two macro pairs at `:200-206`:

| macro | container | offset | reaches |
|---|---|---|---|
| `WR` / `RR` | 2 | **0** | the **EPAT's own** registers — no drive involved |
| `WRi` / `RRi` | 0 | **0x18** | the **IDE task file** — the drive |

Our `CONT_TASKFILE = 0x18` matches, and the bridge's own register space at offset 0 is the one we
have never used. That is where tiers 1 and 3 live, and it is why they work with the drive
unplugged.

### 4.2 Three tiers, each answering a different question

**Tier 1 — is a bridge on the cable?** `epat.c:296-298`, the version read:

```
connect
WR(0x0A, 0x38)      ; bridge register 0A - select the version code
ver = RR(0x0B)      ; bridge register 0B
disconnect
```

Linux prints `ver` rather than comparing it, so we have no expected constant. For presence that
does not matter: `00h` and `FFh` are the two ways a floating bus lies, and **anything else means a
bridge answered**. Record the value the first time and compare on later boots — a bridge that
changes its version between boots is a bus problem, not a bridge.

Cost: connect frame, one write, one read, disconnect. **~250 µs.**

**Tier 2 — is a drive behind it?** `epat.c:261-268`, the task-file echo:

```
WRi(6, 0xA0)        ; drive/head - select master
WRi(2, k ^ 0xAA)    ; sector count
WRi(3, k ^ 0x55)    ; sector number
RRi(2) must equal k ^ 0xAA
```

Two different patterns into two different registers, so a bus stuck high, a bus stuck low, and a
bridge that merely echoes the last byte written all fail. Linux sweeps `k` over 256 values and
both device banks; **two values on the master are enough for presence** — the sweep is a
signal-integrity test, not a detection test. One `k` each side of `0xAA`, and stop.

**Tier 3 — does the data path carry bytes correctly?** `epat.c:274-281`. This one is the find:

```
WR(0x13, 1); WR(0x13, 0); WR(0x0A, 0x11)   ; all BRIDGE registers
read 512 bytes
expect buf[2k] == k  and  buf[2k+1] == 0xFF - k
```

**The EPAT has a built-in test-pattern generator**, and it is in the bridge, so this verifies a
512-byte block read **with no drive attached and no media in it**. It is the only test we have
ever had that isolates the transport from the device.

That directly discharges technique 111b's item 3 — *"a block read returns real payload you can
recognise by eye"* — and it can settle, off the bench, whether the ECP and nibble block paths
agree, which is currently only testable against a drive that may itself be at fault.

### 4.3 Where each runs

| tier | where | on failure |
|---|---|---|
| 1, bridge version | `LsFindAdapter` | `SP_RETURN_NOT_FOUND` — SCSIPORT unloads us, boot proceeds at full speed |
| 2, task-file echo | first SRB, `LS_ST_COLDSTART` | fail that SRB `SELECTION_TIMEOUT`; no budget spent |
| 3, ramp pattern | diagnostic build and DOS probe only | not in the boot path |

`LsFindAdapter` currently returns `SP_RETURN_FOUND` unconditionally, with a comment explaining
that a phase-0 build which declines to load would test nothing. That was right for phase 0 and is
wrong now.

A bridge that is powered but not yet settled is a real thing on a cold boot, so allow a small
fixed number of tier-1 attempts — **three, ~250 µs each, one `ScsiPortStallExecution(100)`
between** — then decline. Worst case **under 1 ms**, against the ~2.5 hours in §1.3.

**Tier 1 and tier 2 must stay separate.** The signature check `LS_ColdStart` does today (`14 EBh`
in BCLO/BCHI) proves an ATAPI *device* is present and out of reset; it says nothing about whether
a bridge is on the cable, because with no bridge it reads `FF FF` and simply fails. Conflating
them is what gives an unplugged cable the patience intended for a slow drive.

### 4.4 What none of them cover

The drive being unplugged **after** the driver loads. That surfaces as a transfer or a wait that
stops answering, and is handled by I2 — the read reports "no answer", and the state machine fails
the SRB immediately with `SRB_STATUS_SELECTION_TIMEOUT` rather than spending a budget. Removable
media on a cable is the case this driver exists for; treat a disappearing device as expected
input, not as an error condition.

---

## 5. Cost budget

Every row is enforced somewhere named. A row with no enforcement site is a hole.

| situation | budget | enforced by |
|---|---|---|
| no bridge on the cable | **< 1 ms, once** | §4.2, `LsFindAdapter` |
| bridge present, no drive | cold-start budget, then fail the SRB | `LS_TICKS_COLD`, ticks that now cost ~1 ms |
| drive spinning up | 8 s, as 8000 ticks | `LS_TICKS_READY`, pf.c's `PF_SPIN` |
| drive busy mid-command | 8 s, as ticks | `lt_finishwait` |
| one wait step | **≤ 2 ms** | derived: 2 ms / 46 µs ≈ **40** iterations |
| one transfer step | `LS_MAX_XFER` x per-byte cost | stated per transport below |
| cable pulled mid-transfer | one step, then `SELECTION_TIMEOUT` | I2 |

Per-byte transport costs, from technique 109e's 5.77 µs per access:

| transport | accesses/byte | µs/byte | 4096 B |
|---|---|---|---|
| nibble | 4 | 23 | 94 ms |
| ECP `rep insb` / `rep outsb` | 1 | 5.8 | **24 ms** |

**94 ms is too long to hold the system for one step.** Either ECP is mandatory for data, or
`LS_MAX_XFER` drops on the nibble fallback. The second is the honest answer, because ECP is
negotiated and a refusal falls back to nibble at runtime: **set `MaximumTransferLength` from the
transport actually negotiated**, not from a build constant. That is I6 applied to a second
quantity.

### 5.1 The write boundary, measured 2026-09-14

There is a second ceiling on `MaximumTransferLength` and it is not about time at all.
`docs/ls120_write_boundary_2026_09_14.md`:

> **A write burst to this drive is intact for the first 5120 bytes and silently garbage beyond
> it. Split into separate operations of 5120 bytes or less it works indefinitely.**

Nine runs on the real machine. 4096 clean, 5120 clean, 8192 and 16384 good to 5119 and corrupt
after — deterministic across repeats, identical on different clusters, identical with random
content, and 16 KB written as four separate 4 KB files is byte-perfect. So the drive and the
medium are fine and the limit is per-burst.

Three things follow, and they are the reason this file's invariants are shaped the way they are:

- **`LS_MAX_XFER = 4096` is measured-safe**, two sectors under the boundary. It was picked as
  "8 sectors"; it now has a number behind it.
- **The boundary is not a constant** — an earlier session saw 6144 on the same drive. I6 is not
  a stylistic preference; a pinned 5120 would be wrong on the next machine and possibly on this
  one after a reboot.
- **Every failing run reported success.** DOS printed `1 file(s) copied` nine times out of nine.
  That is I2 and section 7 stated as a measurement rather than a principle.

⚠ All nine runs had the **vendor** driver loaded and owning the port (technique 110). The
boundary is a property of the transport we share; it has not been shown that our own driver
hits the same wall.

---

## 6. Changes this implies

Ordered. Each is independently testable.

1. **`LS_RegRead` returns validity.** `CF=1` when the assembled byte is `00h` or `FFh`. Both
   nibbles reading `0Fh` is the specific absent-bus signature and is worth its own flag. The dead
   `jc` guards in three callers become live. *(I2)*
2. **Bound `LS_AtaSoftReset`.** Replace `0FFFFh` with a derived constant and a comment carrying
   the arithmetic. The reset itself stays where it is; only the wait moves to the state machine.
   *(I3, I4)*
3. **Tier-1 presence gate in `LsFindAdapter`.** §4.2. This is the change that answers the owner's
   report. *(I1)*
4. **Split "no bridge" from "no drive" in `LS_ColdStart`** — tier 1 versus tier 2, two failure
   returns, two responses. *(§4.3)*
5. **Re-derive every `LS_SPIN_*` from the 2 ms step budget.** `LS_SPIN_BSY` 2000 → ~40, with the
   remaining patience expressed as ticks. *(I3, I4)*
6. **`MaximumTransferLength` from the negotiated transport.** *(I6, §5)*

**1-3 are the fix for the reported defect.** 4-6 are the design catching up with it.

---

## 7. What is not changing

- **The timer-callback architecture is right.** `HwInitialize` empty, `HwStartIo` returning
  without completing, a self-re-arming engine — two independent implementations agree
  (`IMPLEMENTATION.md` §1) and the restructure landed on 09-11. Nothing here reopens it.
- **The ATAPI command sequence stays** (`IMPLEMENTATION.md` §2). It is transliterated from `pf.c`
  and four separate improvisations against it were all wrong.
- **The sense-drain behaviour stays** (§3). Unit attention is cleared by reading the sense, and
  INQUIRY is exempt — that is why bring-up looked healthy while every real read was refused.

### The vendor's status, stated precisely

`SD120PPD.SYS` is retired as a **behavioural** reference: it reports successful copies while
silently truncating writes, so diffing our data path against it can only reproduce its bug.

It remains a sound **architectural** reference. `SD120PPD.MPD`'s shape — empty `HwInitialize`, a
1 ms self-re-arming timer, four SRB functions — is independently corroborated by the DDK's
`PC2X.C`, and that corroboration does not depend on the vendor's write path being correct.

**Do not let the truncation finding be read as "the vendor got everything wrong".** It got the
architecture right and the transfer sizing wrong, and we are about to inherit the first and must
not inherit the second.

### And read back what we wrote

While the transport is unproven, a write is followed by a read-back and a compare. Silent
truncation is data loss on any machine; it is only rarer elsewhere. Technique 79 applies — a
round trip through the same code path cannot detect a wrong address, so the verification that
counts is from **outside the guest**, off the medium.

---

## 8. Open

| | |
|---|---|
| What does bridge register `0Bh` read on **our** EPAT? | Unknown — Linux prints it, never compares. Capture it once from DOS and write it down; presence only needs "not `00h`, not `FFh`" |
| The 30,720-byte write stall | reproduces on **both** transports, so it is size, not transport. §5's per-step budget is the suspect, and tier 3 can now test it with no drive |
| Per-byte nibble cost | stated as 4 accesses/byte from the read path's shape; count it in `LS_BlockReadSpp` before trusting the 94 ms |
| Hot-unplug during a transfer | I2 covers detection; nothing yet resets the bridge cleanly afterwards |

---

## 9. What this is worth, and to whom

The owner's framing, 2026-09-14, and it is the reason the invariants are written as rules rather
than as fixes:

> *"if we model how we avoid corrupting the drive like the vendor does then we likely have the
> first real LS-120 driver for an XT — it also is good then for any other user as we build in the
> checks in the driver."*

Three things follow, and they are the argument for doing this properly rather than getting our own
machine working.

**The checks travel; our test coverage does not.** We can test one 5160, one bridge, one drive.
The vendor tested more machines than we ever will and still shipped a driver that reports success
while truncating a write. The difference a driver can make on hardware nobody has is not more
testing — it is refusing to report success it has not verified (§7, read back what we wrote) and
stating its own limits in numbers (I5). Those hold on machines we have never seen. A hand-picked
switch table does not, which is I6.

**None of the invariants are XT-specific.** I1 through I5 are about a device on a cable that may
not be there and a callback that must return. An LS-120 on a parallel port is that case on any
machine; the XT only makes the costs large enough to notice. The 5.55 µs bus access is what turned
a latent 3-second spin into a visible boot stall — so this hardware is a good place to find these,
not the only place they matter.

**EPAT is a device class, not one drive.** The bridge is one of the Linux `paride` family, and the
same chip fronts CD-ROM, Zip and tape. Nothing in 86Box models parallel-port ATAPI at all, so a
working `lpt_epat.c` is a new capability there rather than a fix — which is why the branch is worth
finishing and submitting (`lpt-epat-bridge`, ~24 commits, and the four gaps named in the 09-14
handoff).

**And §4.2's tier 3 is what makes that model testable.** The ramp generator lives in the *bridge*,
so the bed can be proven byte-correct against a 512-byte known pattern **without modelling a drive
at all** — bridge registers `13h` and `0Ah`, then 512 bytes of `k, 0FFh-k`. That is a far smaller
first milestone than a full ATAPI device model, it is a regression test for the transport on both
sides, and it is the piece that lets the emulator reproduce a hardware failure instead of passing
every case (which, per the handoff, is what the bed does today and why it cannot falsify anything).
| Does tier 3 run on a bridge with no drive attached? | `epat.c` does it between connect and disconnect with no device selection, so it should. **Unverified on this bridge** — and it is the cheapest thing on this list to check, because the drive is already unplugged |
