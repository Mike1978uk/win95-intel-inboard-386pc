# Handoff 2026-09-19 — the enumeration divergence is a verified, retried port open

## The question

The owner's observation, after a cold boot with the drive powered off all
night: **no drive letter.** And the point that reframes the hunt —

> the vendor code ALWAYS enumerated the drive, irrespective of drive state,
> in real mode and under Windows.

So the fault is not the transport, which works. It is the **bring-up path**.

## ⛔ READ THIS FIRST — the headline below is OVERSTATED

The verify-and-retry work in this document is real, shipped and neutral. **It
is not the explanation for the owner's cold-boot failure**, and an earlier
draft of this handoff said it was.

`TRANSPORT_SPEC.md` §3 and §5 already record the measured discriminator:

| condition | ATA status at `0x18+7` |
|---|---|
| cold, `SD120PPD.SYS` REM'd out | **`0x00`** |
| vendor driver loaded | `0x50` |

**with the CPP connect checkpoints returning `B8 58 F0` in BOTH cases.** The
bridge handshake succeeds cold. So the thing that differs between a drive that
enumerates and one that does not is **drive-level state, not bridge-level**,
and a retry around the bridge open cannot reach it.

Keep the change — it matches the vendor, costs one register write and is
measured neutral. Do not present it as the fix.

## ⭐⭐ THE FINDING: `LS_SpinUp` HAS NO CALL SITE

**START STOP UNIT is never issued. The motor is never spun up.**

```
$ grep -n "LS_SpinUp" LS120MP.ASM LS120TR.ASM
LS120MP.ASM:57:   EXTERNDEF  LS_SpinUp : NEAR
LS120TR.ASM:762:  ;  The motor work ... has MOVED to the first SRB (LS_SpinUp).
LS120TR.ASM:2095: ; LS_SpinUp - START STOP UNIT with the start bit, then TEST UNIT READY.
LS120TR.ASM:2107: public LS_SpinUp
LS120TR.ASM:2108: LS_SpinUp:

$ grep -E "call[[:space:]]+LS_SpinUp" ...
>>> NO CALL SITE ANYWHERE <<<
```

Declared, `public`, and called from nowhere — while its own header comment
says *"Called once, on the first SRB."* Corroborated from outside the source:
today's bed trace shows the whole command set as `12 / 00 / 1E / 25 / 28`,
with **no `1B` anywhere**.

**This fits the owner's report exactly.** A drive left powered off has a
stopped motor. Without START STOP UNIT, READ CAPACITY fails — and today's
trace shows READ CAPACITY failing twice with a bare `SRB_STATUS_ERROR` before
succeeding. The class driver has no sense data to tell it to wait, so on a
colder drive it gives up and no volume appears. The vendor's stack issues the
command, which is why it enumerates "irrespective of drive state".

This is the **fourth** unreachable-routine bug in this driver — after
`LS_DetectEcp`/`LS_HasEcp`, `LS_BridgeProbe` and `LS_ColdStart`. The pattern
is always the same: a routine is written, documented as being called, and
never wired in. **`grep` for a call site before believing any comment that
says when something runs.**

Wired in at the first real SRB, self-limiting (the one-shot guard lives inside
`LS_SpinUp`, not at the call site, so a second call site cannot reintroduce a
spin-up per request). Traces `SPIN`.

⚠ **Untested on hardware, and it is the one thing worth testing next.**
⚠ START STOP UNIT on a cold drive can block for seconds inside `HwStartIo`
(technique 98). If the desktop stalls on first access, that is why, and the
answer is to defer it rather than to remove it.

### Measured in the bed, and better than expected

`SPINUP` (`9e97abbf`) against `OPENRETRY` (`ea4f1dd8`), one variable:

| | START STOP UNIT | READ CAPACITY | TEST UNIT READY |
|---|---|---|---|
| without the fix | **absent** | **2 attempts** — first `DONE=04` ERROR | 3 |
| with the fix | `CDB 1B` x1 (`SPIN=1`) | **1 attempt, `DONE=01` first time** | 2 |

**READ CAPACITY now succeeds first time instead of failing and being
retried** — in a bed that models no motor at all. So the fix helps for a
second reason independent of the motor: `LS_SpinUp` issues START STOP UNIT
**then TEST UNIT READY**, and that TEST UNIT READY absorbs the pending
unit-attention condition which was failing the class driver's first real
command. On the owner's cold drive there are more such conditions queued, and
nothing tells the class driver they are retryable.

⛔ **The bed could not test the MOTOR case, and here is the proof** —
`86box_upstream/src/disk/rdisk.c`:

```c
case GPCMD_START_STOP_UNIT:
    switch (cdb[4] & 3) {
        case 1: /* Start the disc and read the TOC. */
            break;              <- no-op
```

Our `LS_StartCdb` sends `cdb[4] = 1`. **The bed models no motor state at all**,
so the command is accepted and changes nothing. A bed run proves the command
is now ISSUED and that it is harmless; it cannot show it fixes anything,
because the bed has no cold drive to fix. Technique 127b.

**This is the gap worth closing next, and it is cheap.** Give `rdisk.c` an
opt-in `start_required` (default off, same shape as the EPAT device's existing
`busy_ms`/`reset_ms`): when set, the drive answers NOT READY with ASC `04`/`02`
(*"logical unit not ready, initializing command required"*) until START STOP
UNIT arrives with the start bit. That is faithful ATAPI behaviour for a
removable drive, and it turns this fix from untestable into falsifiable at
emulator cost — the bed would then fail to enumerate WITHOUT the fix, which is
the control this whole question has lacked.

### The other candidate, not pursued

**The reset settle.** `0x00` is a drive still in SRST (technique 122: a settle
is seconds, a status poll is milliseconds, and they must not share a budget).

## The answer to "how does the vendor differ" — real, but at the wrong layer

`drivers/imation_ls120/TRANSPORT_SPEC.md` §4 decodes the vendor's cold
port-open at `0x2C42`. Compared against what this driver actually does:

| vendor `0x2C42` | ours, before today |
|---|---|
| `ECR &= 0x34` (read-modify-write) | ✓ same |
| **checkpointed CPP probe (`0x2DBB`)** | ✗ **absent** |
| control kick `04 0C 0E 0E 0E 04 04`, only if the probe failed | ✓ present, unconditional |
| **retry up to `0x32` (50) attempts** | ✗ **absent — one pass** |
| **escalate to 8x-repeated frame writes (`[0xc15]=1`)** | ✗ **absent** |
| `CPP(0x30) CPP(0x40) CPP(0x50) CPP(0x00)` preamble | ✓ same |
| chain scan 0..7 for `0xFFAA`, connect `CPP(0xE0\|unit)` | ✗ bare `CPP(0xE0)` |

**We opened the port once and assumed it worked.** The vendor opens it,
*checks that silicon answered*, and on a failure widens the frame pulse and
opens again, up to fifty times. That is exactly the difference between an
enumeration that is state-independent and one that is not.

The spec even says why Linux does not help here: *"`epat.c` has none of this
[...] paride assumes the bridge is already awake, because on the machines it
was written for the BIOS or a prior driver left it that way."*

## Shipped

`b076a2e` — **`LS120MP.MPD` code `ea4f1dd8`**, built `-Phase 2 -Mode spp
-Trace`, tree clean.

- `LS_CppReps` (default 2, escalates to 8) — `LS_CppFrame` writes each frame
  byte that many times. The vendor keeps the same count in `[0c15h]`.
- `LS_BridgeVerify` — the bridge's own version register (`0Ah`/`0Bh`), which
  answers with no drive attached. `00h` and `FFh` are the two ways a floating
  bus lies.
- `LS_OpenReset` now loops: open, verify, and on failure widen and retry, up
  to 50. Traces `OPNO`/`OPNF` with the attempt count.

⚠ **`LS_OpenReset` is the live path.** `LS_PortOpen`, `LS_BringUp` and
`LS_ColdStart` are all separate copies of the same sequence and **none of
them is called by the miniport** — checked, not assumed. `LS_PortOpen` got
the same treatment; `LS_BringUp` did not, because nothing calls it.

## Bed result — the new path RAN, and that is ALL it can show

Tag `OPENRETRY`, `vm_ls120win`. The trace ring is the evidence that the code
executed rather than merely linked (technique 81):

```
  3  OPNO = 00000001        <- verified open, answered on attempt 1
 40  CDB0 = 00000025  PFWT = 00000080  DONE = 00000004   READ CAPACITY -> ERROR
 44  CDB0 = 00000025                   DONE = 00000004   ...again
 47  CDB0 = 00000025  PSTS = 00000040  DONE = 00000001   ...third time, SUCCESS
 51  CDB0 = 00000028  PSTS = 00000040  DONE = 00000001   READ(10) SUCCESS
 55  CDB0 = 00000028  PSTS = 00000040  DONE = 00000001   READ(10) SUCCESS
```

`OPNO = 1` is the expected bed answer: the model always responds on the first
frame, so the retry never fires and **the escalation is never exercised**.

**Control run `NORETRY_CONTROL`** — the same source with the change reverted
(`ccc5fbdd`), same flags, same bed. Identical command mix:

| | INQUIRY | TUR | PREVENT | READ CAP | READ(10) |
|---|---|---|---|---|---|
| `OPENRETRY` (`ea4f1dd8`) | 1 | 3 | 2 | 2 | 2 |
| `NORETRY_CONTROL` (`ccc5fbdd`) | 1 | 3 | 2 | 2 | 2 |

The EPAT event mix is identical but for **exactly one line**:

```
OPENRETRY        27: EPAT: W reg 0A = 38      <- LS_BridgeVerify selecting the version code
NORETRY_CONTROL  (absent)
```

54 register writes against 53. That is the whole measured difference between
the two binaries, and it is the change doing precisely one thing. The change
is **neutral in the bed**, which is the most a bed that never fails an open
can say.

⛔ **A trap worth not repeating.** The first reading compared these runs
against archived ones showing 42, 70 and 493 `READ(10)`s and called the new
build a stall. Those archived runs were driven by a startup batch exercising
the drive; these had nothing running after enumeration. **The provenance file
did not record `-Startup`**, so the archive could not answer the question.
Fixed — `tools/ls120_bed_run.ps1` now records `-Startup`, `-ConfigSys`,
`-Autoexec`, `-NoDriver` and the run length. Technique 127a.

⭐ **The sense problem is visible here in miniature.** INQUIRY returned `0A`
(`SRB_STATUS_SELECTION_TIMEOUT`) twice, and READ CAPACITY returned `04`
(`SRB_STATUS_ERROR`) twice, before each succeeded on a later retry. The class
driver got through only because it retried blind — it has no sense data to
retry *on*. A cold drive queues more unit-attention conditions, so it has more
of these to absorb, and nothing tells the class driver they are retryable.

⛔ **The bed cannot test the fix.** Its EPAT model always answers on the first
frame, so the retry never fires and the escalation is never exercised.
Technique 90: a bed that does not model the failure cannot reproduce it.
**Only the 5160 can show whether this fixes the cold case.**

## ⛔ Correction to the record — a killed theory was killed under the wrong conditions

`docs/next_session_2026_09_18f.md` lists as KILLED:

> single port accesses are too fast; the vendor repeats every access 2-4x
> (`0x3A90`, `0x3B17`, `0x3B52`) — killed by `INQPARK.OUT`, single accesses
> returned correct data

That run was on a **warm, already-open bridge**. The vendor only switches to
repeated writes **after a port open has already failed** — the cold case. So
the theory was tested in the one condition where it predicts nothing, and the
null is void. Technique 110, again.

## ⭐ Modelling the real drive in the bed — measured vs reproduced

The owner's question, and it is the right one: *"can it be modelled in the vm,
make the vm behave more like real hardware [...] some precondition being met
feels right."*

We already hold the characterisation — `docs/ls120_media_state_responses.md`
(technique 126). What is missing is not measurement, it is that the bed does
not reproduce most of it, so our driver is tested against a drive that is far
more forgiving than the owner's. Ranked by what each would CATCH:

| # | measured on the 5160 | bed today | what modelling it would catch |
|---|---|---|---|
| **1** | motor stopped until START STOP UNIT | **no motor state at all** — `case 1: break;` | a driver that never spins the drive up. **Exactly today's bug** |
| **2** | SRST queues **TWO** conditions, `29h` then `28h`, **one per REQUEST SENSE** | **one** condition, cleared by any non-sense command | a sense drain that stops after one read, and a driver that never passes sense up |
| **3** | powered off = status **`80h` BSY forever**; ATAPI signature still returns | not modelled | the fast-absence path, which is **unreachable today** because it only tests `FFh` |
| **4** | disconnected = **`77h`** | not modelled | same |
| **5** | `FFh` **never occurs** | `FFh` is the only absence value we test for | that our absence test is looking for a value the hardware never produces |
| 6 | empty drive: medium type `00h`, WP=1, READ CAPACITY `51h`/`24h` | partly modelled | WP=1 being misread as "write protected" |

**1 is implemented today** (see below). **2 is the highest-value one left**,
because it is the control the sense fix needs: with only one condition queued,
a driver that drains once looks correct, and on the real drive it is not.

### Implemented: `RDISK_START_REQUIRED`

`86box_upstream/src/disk/rdisk.c`, opt-in via the environment, default off —
the same shape as the EPAT device's `busy_ms`/`reset_ms`. When set, the drive
answers NOT READY with ASC `04` / ASCQ `02` (*"logical unit not ready,
initializing command required"*) to every `CHECK_READY` command until START
STOP UNIT arrives with the start bit.

That is faithful ATAPI behaviour for a removable drive, and it converts the
spin-up fix from untestable to **falsifiable**: with it set, the bed should
fail to enumerate WITHOUT the fix and succeed WITH it. That is the control
this question has never had.

## The owner's plan — run both vendor drivers in the bed

Right idea. Reproduced today, and it matches the existing `vendor_win` and
`vendor_ver` runs: the vendor scans the chain and then **goes quiet** — no
further EPAT traffic at all.

```
EPAT: CPP unit 0 id -> FFAA      (then units 1..7 -> 0000)
```

⛔ **Do NOT read the `[0117:0000B929] Illegal instruction` line as a crash.**
It appears in **66 of 69** logged bed runs, *including the known-good
`BM3_bed_bytemode_enumerates` run that enumerated correctly*. It is background
noise in this bed. An earlier draft of this handoff called it the vendor
driver crashing; that was wrong, and the control that disproved it (grep the
successful run for the same line) cost one command.

The real anomaly is narrower and still unexplained: our spec says the vendor
scans **until** it sees `0xFFAA`. Unit 0 answers `FFAA` and it scans all eight
anyway, then stops talking to the bridge.

**Ruled out today:** `CPP(0x40)`/`CPP(0x50)` being logged as "unknown command"
by the model is probably *not* the cause — in `epat.c` they are part of a
chip-variant unlock ladder with no state change, so the model's accept-and-
ignore is already the right behaviour. Do not invent semantics for them
(technique 124).

Also noted: the model matches `CPP_CONNECT` as an exact `0xE0`, not masked, so
a vendor that selected a non-zero unit would not connect. Unit 0 answers here,
so it is not today's bug — but it is a real model limitation.

## Next, in order

1. **Hardware.** Deploy `ea4f1dd8`, power the drive off, cold boot. This is
   the only test that can pass or fail the change. Keep `LS120MP.SPP`
   (`d8154f1d`) as the revert.
2. If it still does not enumerate, the next gap on the same list is the
   **checkpointed probe** (`0x2DBB` — verify status after *each* magic byte,
   not just at the end) and the **chain scan**, which would tell us whether
   the bridge is answering at a unit other than 0.
3. The sense fix stays queued and is probably the same job as the "media is
   not formatted" failure — `AutoRequestSense` is `FALSE` and every failure
   path returns a bare `SRB_STATUS_ERROR` with no `ScsiStatus` and no
   `SenseInfoBuffer`, so the class driver cannot tell `6/28h` retry-me from a
   hard error. We drain sense exactly once, in bring-up, and never per-SRB.

## Structural gaps against the vendor, measured today

Import counts, read off both binaries with `tools/pedis.py`:

| | vendor `SD120PPD.MPD` | ours |
|---|---|---|
| SCSIPORT imports | **13** | **2** |

The vendor uses `ScsiPortGetDeviceBase`/`FreeDeviceBase` (claiming the I/O
range — we drive `0x378` unclaimed, technique 125), `ScsiPortStallExecution`,
`ScsiPortCompleteRequest`, `ScsiPortGetLogicalUnit`, `ScsiPortLogError` and
the buffer read/write primitives. We use `Initialize` and `Notification`.

Not claimed as today's bug. Recorded because it is measured, and because
`GetDeviceBase` is the sanctioned answer to a port we currently take without
asking.
