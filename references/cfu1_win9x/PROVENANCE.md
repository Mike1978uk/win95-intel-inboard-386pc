# cfu1-win9x — vendored reference, not our code

Nick / @zikolas, **<https://github.com/zikolas/cfu1-win9x>**, MIT (see `LICENSE`).
Vendored at upstream `ff4ce33f2ddeb2d27f11e9fbe3ef81ce0436aba2`, 2026-09-05. `.git` removed.

**Why it is here.** It is a *working* Windows 9x IOS port driver — the same interface as
`drivers/xtide_pdr`, built with a free modern toolchain. Diffing against it has twice found bugs in
ours that reading Microsoft's DDK sample never would (technique 81): the sample's request routine
destroying callee-saved registers, and its unguarded calldown insert on every broadcast DCB. Having
it local means grep instead of recall — we had been quoting it from memory.

**What it gave us, specifically:**

| file | what it settles |
|---|---|
| `win/vxd/CFU1.ASM` | a real AER, calldown insert, `vol_create`, `sched_volup`, and the linear-SGD walk (`rr_sg`/`rw_sg`) whose comment says "logical SGD: ptr is SECOND" |
| `win/ASYNC-ENGINE.md` | *"Design: interrupt-driven transfer engine (the freeze killer)"* — the same synchronous-PIO problem we have, and the shape of the fix |
| `win/diag/` | `CFUIOS.C`, `CSTAT.C`, `CPEEK.C` — a DeviceIoControl-based AEP census, i.e. the same instrumentation we built independently as DBGPORT |
| `win/dist/CFU1.VXD` | a **working port-driver binary** our own `tools/vxd_*.py` can audit as a control |
| `PROBE-NOTES.md` | his elimination log |

**Two things read out of it on 2026-09-05, worth not re-deriving:**

- Its AER acknowledges AEP 16..19 with `AEP_SUCCESS` and answers `AEP_FAILURE` to everything else it
  does not dispatch — so it never handles `AEP_PEND_UNCONFIG_DCB` (21) at all, and shuts down
  cleanly. `ESDI_506.PDR` also answers 21 with `AEP_FAILURE`. **Both working references answer 21
  FAILURE; we answer SUCCESS.** They disagree on 16 (ESDI_506 FAILUREs it), so 16's answer is
  unlikely to be the discriminator.
- `ASYNC-ENGINE.md` records that his synchronous polled engine **works** — it freezes the UI for the
  length of a copy (mitigated with a 4 KB `max_xfer_len` cap) but it functions. So blocking inline
  costs responsiveness, not correctness. That is consistent with our own measurement that the
  completion ladder is balanced at shutdown, and it weakens "we block inline" as the cause of the
  shutdown wedge.

Do not edit anything under this directory. Fixes belong upstream, in his repo.
