# Handoff — 2026-09-21c

Read this one first. Supersedes `next_session_2026_09_21b.md`.

---

## ✅ RESOLVED: the XT-IDE A/B ran, and request merging buys nothing

| arm | elapsed | throughput |
|---|---|---|
| baseline `db88f64d` | **93.15 s** | **385.1 KiB/s** |
| `-PhysBreaks 17` `a6bfb48b` | **93.26 s** | 384.7 KiB/s |

**0.12% apart, baseline faster** — noise. Modelled at 1.88x, delivered nothing.
`docs/captures/xtide_request_merging_result_2026_09_21.md`. **#30 closed. Baseline stays.**

⭐ **Why**: 385 KiB/s is **2.1x** the ~180 KB/s ceiling the driver source derives for byte-wide
8-bit PIO — we are already at the rate `XT_FAST_XFER` bought. The transfer is **bus-bound per
byte, not per command**, so removing commands changes nothing.

✅ **XT-IDE is closed.** Word transfers shipped · paced polling shipped · "the free 4%" a
phantom · merging measured null · dword blocked by the register map · no card here has a memory
aperture. **Nothing further without different hardware.**

✅ Risk retired on the way: Windows 95 boots and runs normally on the PhysBreaks arm, so 64 KB
SRBs on the boot volume are harmless.

⚠ Scope: one large sequential read. Small-random was not tested — but sequential was the case
modelled at 1.88x and the one the plan ranks by.

---

## 86Box: both PRs green and answered

| | |
|---|---|
| [#8010](https://github.com/86Box/86Box/pull/8010) | `25e28f178`, MERGEABLE, CI green |
| [#8012](https://github.com/86Box/86Box/pull/8012) | `90d81f013`, MERGEABLE, CI green |

Two maintainer comments resolved: jriwanek's merge conflict, and dhrdlicka's objection to the
LPT dropdown. Both bridges now instantiate from the drive's bus assignment like `lpt_ditto`;
`src/char/char.c` has dropped out of both diffs. Retested with **no `lpt1_device` in either
config**. The untested EPAT-carries-a-CD commit was dropped rather than worked around.

**Nothing is owed to anyone.** Waiting on maintainers only.

---

## The big measurement of the session

⭐ **An I/O access costs far more than its bus time.** SW1 on the CPU module is ON, so every
I/O read and write **flushes the L1**. Measured:

| | |
|---|---|
| 1 KB working set, cached | **39.5 ns/byte** |
| same, one I/O access per KB | **128.1 ns/byte** |
| penalty | **3.24x** |

Cross-checked against E1's independent **135 ns/byte** for conventional RAM. Record:
`docs/captures/io_cache_flush_2026_09_21.md`.

➡ **The plan's 5.695 us per I/O access is a FLOOR.** Request merging (#30) and paced polling
(#41) are both worth more than their measured case — every transaction removed also removes a
flush. ⛔ SW1 cannot be turned off: feipoa says an IBM system *"cannot even run DOOM"* without it.

---

## Issues

**Closed:** #32 — the NIC is already drained with `rep insd`, no lever.
**New:** [#40](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/40) CPU module
registers (⭐ `XTOUT` is set where feipoa recommends `0`, untested since 2026-09-12) ·
[#41](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/41) pace the polling in
`T130.MPD`, `HSFLOP.PDR`, `ELNK3.VXD`.

⛔ **The clock is done.** 40 MHz crystal, 2x = 80 MHz, part rated 60. 3x at 40 = 120 is too
high; 100 will not power on. And the crystal clocks **the card**, not just the core, so a lower
crystal at 3x trades card speed for core speed — the wrong direction on a bus-bound machine.
**Switches are optimal**: SW1 ON (required), SW2 2x, SW3 coprocessor enabled (equipment word
`0x5263` bit 1 confirms).

---

## Corrections to our own record this session

E1 said **256 KB planar for ten days**; the chips are HYB4164, so it is **64 KB** and E1 cut
conventional memory on the bus by **90%, not 60%** · technique 128d called the T130B aperture
"unchecked" when it was closed · E7 closed on a derived source then reopened on the primary
one · the `0x0D5E` offset · `TIME < NUL`.

⭐ **The pattern, and it is the most useful line here**: nearly every one was caught by checking
a cheap fact against the machine, the owner, or a primary source — never by reasoning harder.
Several proposals died on contact with a fact the owner already had. **Ask before writing the
plan.**
