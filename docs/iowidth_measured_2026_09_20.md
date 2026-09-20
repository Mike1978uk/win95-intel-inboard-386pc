# The third point, measured: dword is worth 44% over byte-wide

Real 5160, Inboard 386/PC, 2026-09-20, LS-120 powered off. `tools/gen_iowidth_probe.py`,
run through COMrade as `DEBUG < C:\IOWIDTH.SCR`. 512 bytes written to one port
three ways, PIT channel 0 latched either side of each loop, interrupts off.

Predictions were committed to `docs/xt_bus_optimisation_roadmap.md` in
`99e8009`, **before** the run.

## Raw

`d 200 l 6`, three little-endian 16-bit tick deltas:

| run | port | raw | `outsb` x512 | `outsw` x256 | `outsd` x128 |
|---|---|---|---|---|---|
| 1 | `0x378` Intek21 | `98 0D 22 09 98 07` | 3480 | 2338 | 1944 |
| 2 | `0x378` Intek21 | `96 0D 18 09 98 07` | 3478 | 2328 | 1944 |
| 3 | `0x278` nothing decodes | `9A 0D 26 09 98 07` | 3482 | 2342 | 1944 |

Run-to-run spread is **0.4% worst case**, and the dword arm returned the
identical count three times out of three.

## Derived

PIT channel 0 at 1.193182 MHz, 0.83810 us/tick. Mean of the two `0x378` runs:

| access width | ticks | total us | us per access | **us per byte** | vs byte-wide |
|---|---|---|---|---|---|
| byte, 512 accesses | 3479 | 2915.8 | 5.695 | **5.695** | — |
| word, 256 accesses | 2333 | 1955.3 | 7.638 | **3.819** | **-32.9%** |
| dword, 128 accesses | 1944 | 1629.3 | 12.729 | **3.183** | **-44.1%** |

Fitting `sync + n x byte_cycle` on the byte and word points:

```
sync + 1b = 5.695        ->  byte cycle = 1.943 us
sync + 2b = 7.638            fixed sync = 3.752 us
```

Technique 109e fitted **reads** at 1.87 / 3.90. Writes come out at 1.94 / 3.75.
The model holds across direction.

## The answer, against the recorded outcomes

| predicted | meaning | measured |
|---|---|---|
| ~1738 | the linear fit holds, width worth ~51% | |
| ~2334 | amortises two bytes but not four | |
| ~3525 | re-synchronises per ISA cycle, width dies at 16 bits | |
| | | **1944** |

**Width does not die at 16 bits.** Dword is a further 16.6% below word and
44.1% below byte-wide.

It is **10.5% worse than the linear extrapolation**, though. The marginal byte
inside a dword costs `(12.729 - 3.752) / 4 = 2.244 us` against 1.943 inside a
word, so the third and fourth bytes are dearer than the second. The model is
sub-linear in width. **Quote 44%, not the 51% the two-point fit promised.**

## The control matters as much as the result

`0x278` decodes to nothing on this machine and came back within **0.1%** of the
Intek21 at `0x378`, at all three widths. So:

- **The Intek21 inserts no wait states.** There is nothing to win by changing
  the card, and nothing about these numbers is specific to it.
- The whole 5.695 us is the Inboard synchronising a 16 MHz 386 to a 4.77 MHz
  bus. It is a property of the accelerator, not of any peripheral.

## What it is worth on the LS-120, and the caveat that bounds it

Windows EPP write, best measured run: 4,000,000 B in 43.45 s = 10.86 us/byte.

| | us/byte | seconds of the 43.45 |
|---|---|---|
| bus term, byte-wide | 5.695 | 22.78 |
| bus term, dword | 3.183 | 12.73 |
| **saving** | **2.512** | **10.05 s = 23%** |

On the 36,735,152-byte file that is about **92 seconds**.

⚠ **This is an upper bound, and the reason is structural.** The probe writes the
LPT *data* register, which no peripheral answers. A real EPP transfer goes to
base+4 and each byte carries the bridge's nWAIT, which extends every **bus
cycle** and therefore does **not** amortise across a dword. What is proven here
is that the Inboard's per-access sync is worth 2.512 us/byte to amortise.
Whether the EPP path lets us collect it is a separate question, and only the
transfer itself can answer it.

⚠ Part of the win is `rep` loop overhead rather than bus sync: 512 iterations
become 128. At ~5 clocks per iteration on a 16 MHz 386 that is ~0.31 us each,
so ~119 us of the 1287 us saved, about **9%**. The other 91% is bus.

## Method notes

- **The transport timed out on two of the three runs and the machine had
  finished anyway.** `run_command` returned `DOS request op=0x3 timed out after
  8.0s` while the run completed normally; `screen_read` afterwards had the
  answer. Do not read a COMrade timeout as a failed run — read the screen.
- A `<` written as `&lt;` reached DOS literally and started DEBUG on a missing
  file, leaving it at its `-` prompt with no shell for `run_command` to talk to.
  `q` gets out.
- `{Esc}` to clear a half-typed command line leaves a `\` and a continuation
  line on screen. Harmless, but it makes the transcript confusing.
- Two generator bugs were caught before the script was sent, both of which would
  have produced a plausible wrong number rather than an error: `print()` in text
  mode on Windows turned each `\r\n` into `\r\r\n`, and the whole patch was one
  400-character `e` line, past DEBUG's ~256-character input limit. **A truncated
  `e` writes a short patch that still runs.** The capstone read-back in the
  generator is what makes that class of bug visible.
