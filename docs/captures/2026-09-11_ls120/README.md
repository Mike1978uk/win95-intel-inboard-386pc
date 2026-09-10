# LS-120 hardware captures, 2026-09-10/11

Raw DEBUG output, kept because the decoded values are the evidence for
`TRANSPORT_SPEC.md` section 6 and skill techniques 111/111a/111b.

| File | What it proves |
|---|---|
| `INQ3.OUT` | Packet command reaches step 2: PACKET accepted, DRQ raised, 12-byte CDB taken, then completion with no data phase. Status `50`, error `00` |
| `INQ4.OUT` | **The finding.** Byte count `0x0024` - the drive prepared exactly 36 bytes - while the data read returned 36 zeros. Splits "device refused" from "we cannot collect it" |
| `RAMTIME.OUT` | E1 memory locality, first attempt. **Numbers are not trustworthy** - see below |

## INQ4 decoded

```
[0600] status = 50   [0601] error = 00   [0602] step = 09 (read attempted without DRQ)
[0610] interrupt reason = 03
[0611] byte count lo    = 24   <- 36 bytes ready
[0612] byte count hi    = 00
[0613] status = 50   [0614] error = 00
[0700..0723] = 36 x 00
```

## RAMTIME decoded - RE-RUN NEEDED

```
[0600] conventional 2000h = 0000
[0602] video       B800h = FFFF
[0604] ROM         F000h = 12FB
[0606] conventional 4000h = 0000
```

BIOS tick deltas (18.2 Hz). `FFFF` and `12FB` (4859 ticks = 267 s) are impossible for a run
that took seconds, so the absolute figures are wrong - suspect the tick read or the pass
counter. **Do not quote these numbers.**

What survives: both conventional-memory regions completed in under one tick while both
known-bus regions took many. That is directionally strong - conventional memory looks local
to the Inboard, not across the bus - but it needs a clean measurement before E1 is closed.
Prefer PIT channel 0 latch reads over the BIOS tick for the re-run.

## The scripts themselves

`INQ3.SCR`, `INQ4.SCR` and `RAMTIME.SCR` are the exact DEBUG scripts that produced the `.OUT`
files above. Kept so nothing has to be re-derived: send one to the box and run

```
DEBUG < C:\INQ4.SCR > C:\INQ4.OUT
```

`INQ4.SCR` is the one to build on - it is the full ATAPI packet command with per-stage bail-outs
and a step counter, and it photographs the phase registers after the CDB.

Generators: `drivers/imation_ls120/tools/gen_inquiry_probe.py` (INQ3 shape) and
`gen_phase_probe.py` (INQ4 shape).

### Two DEBUG-script rules these cost us

1. **Conditional jumps are +/-127 bytes.** A `jnz` to a target ~300 bytes away is rejected by
   DEBUG, which then assembles the NEXT script line at the same address - silently shifting
   everything after it into garbage. This hard-wedged the machine twice, needing the power
   switch. Structure as `jz <a few bytes ahead>` + near `jmp <anywhere>`, with every target in
   its own `a <addr>` block so DEBUG computes the offsets.
2. **A bounded loop still needs an explicit jump on the timeout path.** Falling out of the
   bottom of an `a` block runs into unassembled memory. Always end with `jmp <exit>`.

Technique 109b/109c, plus these two, are why these scripts are shaped the way they are.
