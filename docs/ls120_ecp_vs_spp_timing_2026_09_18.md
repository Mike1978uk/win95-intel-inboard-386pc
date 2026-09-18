# ECP vs SPP timing — what was measured, and why it does NOT say ECP is slower

Measured on the real 5160, 2026-09-18, DOS COMrade. Read the conclusion before
the numbers: **this session did not produce a valid ECP bulk timing, and
nothing here refutes the expectation that ECP is faster.**

## Method

`SPP2.SCR` and `ECP2.SCR` are byte-identical except the transport call target
(`call 1A00` vs `call 1190`), so a run-time difference between them should be
the transport. Timed with the guest's own clock from a batch, so the serial
link's ~34 ms round trip is outside the measurement (the owner's suggestion —
the machine has a SmartWatch RTC and DOS reports hundredths).

`TIMERUN.BAT` interleaves S, E, S, E so drift cannot be read as a transport
difference.

## The numbers

| run | elapsed |
|---|---|
| SPP2 | **15.15 s** |
| ECP2 | **26.70 s** |
| SPP2 | **11.75 s** |
| ECP2 | **24.88 s** |

⛔ **These do not mean ECP is slower.** See below.

## Why the ECP runs are void

The ECP captures are full of **`F5`**:

```
SPP  :1E10  51 03 51 64 01 00 00 02-D0 01 58 00 01 00 00 02
ECP  :1E10  F5 F5 F5 F5 01 00 F5 F5-F5 F5 F5 F5 01 00 F5 F1
```

`F5` is the documented poisoned-port signature: without the 1284 terminate the
peripheral stays in ECP and every subsequent nibble read returns `F5`
(`docs/captures/2026-09-17_ls120/RESULTS.md`). So the ECP runs spent their time
**timing out on poisoned register reads**, not moving data. The extra ~11 s is
the failure, not the transport.

**`ECP2.SCR` is dated 09-17 23:51 — twenty minutes BEFORE `ECPTERM2.SCR`
(09-18 00:13), the script that discovered the terminate.** It is a pre-fix
script and should not be used for any timing.

## ⛔ The finding that matters: no ECP bulk script exists

Diffing `ECP2.SCR` against `ECPTERM2.SCR`:

- `ECPTERM2` **uses `call 1A00` — the SPP transport — for all nine bulk
  transfers.** Its addition is a small block that reads task-file register
  `1Dh` three ways (nibble → ECP → nibble) to prove negotiate+terminate works.
- `ECP2` does bulk over ECP (`call 1190`) but has no terminate.

So:

| | bulk transport | terminate | valid? |
|---|---|---|---|
| `SPP2` | SPP | n/a | ✅ baseline |
| `ECP2` | **ECP** | ❌ missing | poisons the port |
| `ECPTERM2` | **SPP** | ✅ correct | proves ECP *register* reads only |

**Nothing has ever run bulk transfers over ECP with a correct terminate.**
That confirms, by reading the scripts rather than asserting, what the project
memory already says: *ECP bulk is UNTESTED, not refuted.*

## The one solid number from today

SPP, the same nine-transfer workload, four runs: **16.86, 15.15, 11.97,
11.75 s**. A 1.43× spread between identical runs — worth knowing before
treating any single SPP timing as precise. Likely drive state (spin-up,
seek position); not yet attributed.

## SETTLED: ECP bulk was built and it does not deliver

`ECPBULK.SCR` is `ECP2.SCR` with `ECPTERM2`'s terminate (routine `1600`)
grafted onto **both** block tails - the read tail at `11F0` and the write tail
at `12C0`, since both leave the peripheral in ECP. `1600-16FF` was free in
`ECP2`, so nothing moved. The diff is exactly two `call 1600` lines plus the
routine.

Interleaved against `SPP2`, guest clock:

| run | elapsed | data buffers at `2200`/`2800` |
|---|---|---|
| ECPBULK #1 | **21.25 s** | **`EE` - poison, untouched** |
| SPP2 #1 | 15.66 s | `EB 3E 90 4D 53 57 49 4E ...` "MSWIN4.0" |
| ECPBULK #2 | **21.75 s** | **`EE` - poison, untouched** |
| SPP2 #2 | 16.26 s | boot sector |

### The terminate works. The bulk transfer does not.

- **No `F5` flood** - 31 occurrences, the same as the SPP run; the broken
  `ECP2` had 56. So the terminate does its job and the port is left usable.
- **Every data buffer is still `EE`.** The buffers are poisoned before the run,
  so this is evidence of absence, not a stale read
  ([[feedback-a-self-test-must-be-able-to-fail]]).
- The 21 s is time spent timing out, not transferring - it is *longer* than SPP
  while delivering nothing.

Reproduced twice, 21.25 / 21.75 s, both empty.

### ⚠ Scope this claim carefully

What is measured: **this implementation of ECP bulk does not deliver on this
hardware.** `ECP2`'s bulk path was written on 09-17 *before* the 1284
negotiation/termination was understood, so its **negotiate** may be as wrong as
its terminate was. Adding the terminate fixed the port poisoning and nothing
else.

What is NOT measured, and must not be claimed:

- that ECP bulk is impossible on this bridge
- that the vendor's ECP bulk path (`SD120PPD.SYS` `0x4465` / `0x4BD3`) would
  fail the same way - it has never been transliterated and run

**ECP *register* reads do work** with negotiate + terminate - that is
`ECPTERM2`, four runs one variable apart. So the split is: ECP good for
registers, not yet working for bulk.

### What this justifies

Shipping **SPP as the default** on the card is now backed by measurement rather
than by the bed: on real hardware SPP moves the data and this ECP path does
not. The ECP work remains worth doing - the arithmetic still says ~6.7x on the
data phase - but it needs the vendor's own bulk sequence transliterated, not
`ECP2`'s pre-fix guess with a terminate bolted on.

## What would actually settle it

Build `ECP2.SCR` + the corrected terminate — i.e. take the bulk ECP path
(`call 1190`) and add `ECPTERM2`'s terminate to its exit. Then time it against
`SPP2` the same way. Until that script exists, **the ECP-vs-SPP bulk question
is open and the expectation that ECP wins is unrefuted.**

Predicted, so there is something to check against: nibble is ~7 port accesses
per byte, ECP ~1. At the measured `3.90 us` fixed sync + `1.87 us`/byte per
access that is ~39 us/byte against ~5.8 us/byte — about **6.7x** on the data
phase alone, less end to end once command overhead is included.

⚠ And the owner's own caveat stands: ECP may only pay on **large** transfers,
like the Hi-Speed lever on the XT-IDE. A nine-transfer mixed workload of small
commands is not where it would show best.

## Captures

`TS1_spp.OUT`, `TE1_ecp.OUT` (the void one), `T_SPP_run1.OUT`, `TIMERUN.BAT`.

## A trap this session fell into, recorded so it is not repeated

The first `TIMERUN.BAT` was written with shell `printf`, which turned `\E` in
`C:\ECP2.SCR` into an **ESC character**. COMMAND.COM could not open the input,
aborted the line *before* creating the output file, and the ECP runs took a
constant **0.38 s** — which looked like a 43x win. The tells were that the
output file did not exist at all and that the time was implausibly constant.
`inboard-hw-debug` technique 84 already says never to build backslash strings
in the shell; use the Write tool. **A suspiciously good result gets verified,
not published.**
