# Inboard 386/PC: fix POST 101, and make non-486BL CPUs work

Follow-up to #7626. First of all, apologies — both for the holes left in the original submission and
for the delay in getting back to this; I was away on vacation and could not pick it up until now.
Thanks to @QuantumByteRider for the report, and for the detail in it, which turned out to point
straight at the real problem.

Both items from that report are addressed:

| Reported on #7626 | Status |
|---|---|
| `The BIOS gives error 101 at POST` | Fixed — root cause in section 1 below |
| `the "(1988) i386SX" Machine entry is Duplicated` | Already corrected in-tree by someone else; confirmed only one entry remains on current master |
| #7638 — memory reported "BAD", closed not-planned | Same root cause; fixed. See section 5 |

The original PR shipped a machine that did not work out of the box. Two separate causes: I excluded
whole files from the submission as "debug scaffolding" and silently dropped real fixes tangled inside
them, and — the actual cause of the POST 101 — the machine defaulted to a BIOS revision it cannot
work with. Both are addressed here, along with two further defects found while validating the fix.

## 1. The reported POST 101 — an incompatible BIOS was selectable, and was the default

`INBRDPC.SYS` v1.1 (02/17/89), the card's own required DOS driver, verifies a 3-byte signature at
`F000:E05B` as part of its ROM-shadow self-check. **The 1982-dated 5160 ROMs do not carry that
signature**, so the driver cannot work with them. This is a genuine ROM-revision mismatch, not an
emulation gap — real Inboard installations of that era paired with a later ROM revision.

`ibmxt_inboard386_device` shared `ibmxt_config`, whose `.default_string` is the 1982 ROM. So
selecting the machine and pressing go booted an incompatible BIOS, which is exactly the reported
POST 101.

It was also failing *silently*, which is what made it hard to spot: the 1986 revisions were not in
that shared list at all, so a `bios = ibm5160_050986` line in a config file was not a valid option,
was ignored with no warning or log line, and fell back to the 1982 default. Locally I had added
those entries to `ibmxt_config`, so it worked on my tree and not on anyone else's — my mistake, and
the reason I could not initially reproduce what was reported.

**Fix:** the machine now has its own `ibmxt_inboard386_config[]` listing only the two compatible 1986
revisions (`ibm5160_050986` as default, plus `ibm5160_011086`). This makes the incompatible ROMs
unselectable by construction rather than merely discouraged. `ibmxt_config` is deliberately left
untouched, so the plain IBM XT machine is unaffected.

If you would prefer this expressed differently — e.g. keeping the shared list and filtering, or
allowing the 1982 ROMs with a warning — I am happy to rework it.

## 2. POST fix-ups never ran on 386-class CPUs

`cpu_set()` routes 386DX/386SX to `exec386_2386()` and 486BL/486DLC-class parts to `exec386()`. All
of the Inboard POST fix-ups lived only in `exec386()`. Selecting a plain 386DX — the CPU this
accelerator was actually sold to pair with — therefore ran with none of them and hung in the ATI
Mach8 option ROM's PIT delay loop, before the memory count. It presented as a beep followed by a
black screen, which looks like a video or speed problem and is neither.

**Fix:** the fix-ups now live in one shared `inboard_post_fixups()`, called from both interpreter
loops at the same point in each.

**On blast radius**, since this touches the hottest loop in the emulator and I would ask the same
question in review: both call sites are gated on `inboard386_present`, a flag set only while the
card's device is actually instantiated. On every other machine the cost is one predictable,
well-predicted test per instruction and nothing inside the function can run. That gate is load-bearing
rather than belt-and-braces — most of the individual fix-ups are address-gated to this BIOS's own
self-test code and would genuinely be inert anywhere else, but two are segment-scoped (`CS == 0xC000`
for the Mach8 option ROM, `CS == 0x0EAF` for the VMM32 `INT 68h` vector) and an unrelated guest could
otherwise reach them. Caught while preparing this PR; called out here rather than left implicit.

## 3. Double-throttled memory timing on 386-class CPUs

`inboard386_apply_waitstates()` also drove `cpu_waitstates`, which `cpu_update_waitstates()` honours
only for `CPU_286..CPU_386DX`. It was dead on 486BL but live on 386DX, where it stacked on top of
`inboard386_apply_mem_timing()`'s own bus-speed-scaled override of the same variables
(`cpu_cycles_read`/`write`/`prefetch`), compounding with `io_waitstates` and `reg_op_waitstates`.
The 386-class parts ran an order of magnitude too slow as a result.

**Fix:** zeroed there, so exactly one memory-timing mechanism is ever in play.

## 4. Fixes dropped from the original submission, restored

Excluding whole files as "debug scaffolding" also dropped real fixes: the 83.5 MHz CPU table entry
(`cpu_table.c`), the XT keyboard acknowledgment self-heal (`kbc_xt.c`), and the standard parallel
port in `machine_ibmxt_inboard386_init()` (the real machine has an Intek21 TK9901 card in slot 7;
the port setup was in my tree but the function returned before reaching it in what was submitted).
All are included here, without the debug code that caused me to exclude them in the first place.

The duplicate `(1988) i386SX` machine entry, also reported, was already corrected in-tree — thank you.

## 5. This also resolves #7638 (memory reported "BAD")

Issue #7638 — "386 Inboard PC memory configuration error", closed not-planned — is the same root
cause. `INBRDPC.SYS` reported all memory as "BAD" with only 640K usable regardless of `mem_size`,
because that reporter's config has no `bios=` line and was therefore on the 1982 ROM, against which
the driver's ROM-shadow self-check cannot pass. Confirmed by testing. They were also running
`cpu_family = i386dx`, so they were hitting defect 2 at the same time — an incompatible ROM *and* a
CPU family running none of the fix-ups.

The RAM-size dropdown granularity also raised in that issue is separate and expected behaviour;
@OBattler's answer there (use 3072 / 5120) was correct.

That issue carried a suggestion to move this machine to a dev branch "until documentation of the
actual board is found", which was a fair call given the state it was in. The documentation does
exist and this port is built on it — Intel's own manual, disassembly of `INBRDPC.SYS`, cross-checked
against SuperFury's UniPCemu implementation and Al Williams' original 1990 `a20()` code — and with
this PR the machine boots to a working desktop across the configurations below. I have asked for a
re-test there rather than assuming it is closed.

## Testing

Verified booting through POST to a working Windows 95 desktop:

| Configuration | Result |
|---|---|
| `ibm5160_050986` (09MAY86) + ATI Mach8, 486BL3/83.5 | pass |
| `ibm5160_011086` (10JAN86) + ATI Mach8 | pass |
| **Default settings, no `bios=` line** — the exact reported case | pass |
| `i386DX`/25 MHz + ATI Mach8 | pass |

No POST 101 or 301 in any of the above.

## Known limitations, stated honestly

- With the generic `vga` card the machine boots, but POST stops for an F1 keypress before continuing;
  it then reaches the desktop normally. The Mach8 path does not do this. I have added a range-based
  safety net so the affected self-test gate can no longer stick permanently, but I have not fully
  characterised this one and would rather flag it than quietly ship it.
- Not yet exercised: `am386dx`, `ibm486bl2`, `ibm486slc3`, and the ET4000 / ATI28800 / Trident cards.
  No reason to expect problems, but I have not run them, so I am not claiming them.
- Windows 95 requires VGA-class video; CGA is fine for DOS-level testing only.

Happy to take this in whatever direction suits the project — and sorry again for the state the first
submission was in.
