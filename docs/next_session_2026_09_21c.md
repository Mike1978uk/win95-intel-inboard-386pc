# Handoff — 2026-09-21c

Read this one first. Supersedes `next_session_2026_09_21b.md`.

---

## Start here: one experiment is staged and 6 minutes from an answer

**The XT-IDE `-PhysBreaks 17` A/B.** Everything is on the CF already.

| | |
|---|---|
| baseline | `XTIDEMP.MPD` = `db88f64d` (active now), backed up as `XTIDEMP.B20` |
| test arm | `XTIDEMP.PB` = `a6bfb48b` |
| workload | `C:\GOODTIME\GOODTIME.MPG`, 36,735,152 bytes — 7x total RAM, so the cache cannot hide it |
| scripts | `C:\PBENCH.BAT`, `C:\USEPB.BAT`, `C:\USEBASE.BAT` |

**Procedure.** Boot Windows → **MS-DOS Prompt** → `PBENCH BASE`. Reboot → command prompt →
`USEPB` → reboot to Windows → `PBENCH PB17`. Restore any time with `USEBASE`.

⚠ **It must be an MS-DOS Prompt INSIDE Windows.** A real-mode DOS boot uses the BIOS path and
never touches the miniport — the easiest way to get a meaningless number here.

⛔ **The first attempt failed on a harness bug of mine**, not the driver: `TIME < NUL`
re-prompts forever on EOF, so both runs hung before the `COPY` and measured nothing. Fixed to
`ECHO. | TIME`. **Do not use `TIME < NUL` in a DOS batch.**

✅ **One result survived it:** Windows 95 **boots and runs** on the PhysBreaks arm, so the
"64 KB SRBs have never been exercised on the boot volume" risk is **retired**.

⛔ **Do not adopt `a6bfb48b` as the published driver** even if it wins. It came from a DIRTY
tree and cannot be rebuilt bit-for-bit. Rebuild from a clean tree first — `FIXES.md` hands out
an md5.

### Why this is the last XT-IDE lever

Word transfers ✅ shipped · paced polling ✅ shipped · A2 "the free 4%" ⛔ phantom (the loop is
already one `rep insw` per sector) · dword ⛔ blocked (stride 2 decodes A1) · memory aperture ⛔
no card has one. **Request merging is what is left**, and the cap was never IOS or DISKTSD —
it is ours: we advertise 64 KB then set `NumberOfPhysicalBreaks = 0`, which our own source
comment calls *"a promise we did not need to make"*.

Verified byte-by-byte: exactly **one functional byte** differs, at **`0x0D57`**
(`c7 46 1c 00` → `c7 46 1c 11`). ⚠ `next_session_2026_09_20c.md` records `0x0D5E` — **seven
bytes out**; checking there reads `0x02` and makes a good build look wrong. The other 16
differing bytes are PE timestamp, checksum and CodeView records.

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
