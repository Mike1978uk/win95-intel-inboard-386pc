# LS-120 — handoff, 2026-09-14 (late)

## Machine state

CF in the reader at `D:` at time of writing. Vendor driver REM'd in `CONFIG.SYS`
(restore with `copy C:\CONFIG.B4 C:\CONFIG.SYS`). The LS-120 device node was removed
from Device Manager by the owner and has **not** been reinstalled.

On the card:

| path | contents |
|---|---|
| `D:\LS120MP\LS120MP.MPD` | code `6b7939bd`, 6144 bytes — `a925038` rebuilt |
| `D:\LS120MP\LS120MP.INF` | model name `Parallel-port LS-120 (Shuttle EPAT) - a925038 12:01 build` |
| `D:\WINDOWS\SYSTEM\IOSUBSYS\LS120MP.MPD` | same binary |
| `D:\WINDOWS\INF\OEM3.INF`, `OEM4.INF` | skeleton registrations **retired** (originals kept as `.SKL`) |

Previous binaries kept as `LS120MP.B13`…`B25`; the skeleton pair as
`D:\LS120MP\LS120MP.SEP7` and `LS120MP.IN0`.

## What was actually wrong

**The install source held the phase-0 skeleton, and the device node was registered
against the skeleton INF.** `D:\LS120MP\LS120MP.MPD` was
`2026-09-07T11:07:39Z  code 7bfc5476  md5 61fcab5d  5120 bytes  3f56e63  clean  (none)` —
flags `(none)`, i.e. **no `-DLS_PHASE2`**. That build registers with SCSIPORT, reads one
register and reports no devices, by design; its INF says so in capitals.

Because that folder is the INF's `SourceDisksFiles` location, **every driver refresh
restored it over `IOSUBSYS`**. Sessions repeatedly replaced only the `.MPD` beneath a node
whose `DriverDesc` stayed `PHASE 0 SKELETON` for ever, so:

- the machine showed no way to tell one build from another — **no iteration record**;
- a deploy that was silently overwritten looked exactly like a deploy that worked;
- the owner's only means of identifying a build was one photograph with a timestamp.

Confirmed in both backup images: `LS120MP\` is `5120 + 3723` bytes on 09-10 **and** 09-12,
while `IOSUBSYS` held 7168-byte phase-2 builds.

## ⛔ Retraction: the "2-tick Init Success" was the skeleton

`BOOTLOG.OLD` (2026-09-09 14:04:54) reads `Init Success` in **2 ticks**, and that number has
been the good-boot reference ever since. On that date `IOSUBSYS\LS120MP.MPD` was the
5120-byte `61fcab5d` skeleton dated 2026-09-07 11:18. Two ticks is a driver that reads one
register and returns. **It is not evidence that anything worked.** Every comparison resting
on it — including the hardware-vs-bed argument — is void.

The only record of the drive working is the owner's photograph, Friday 2026-09-11 12:10,
mapped at `L:`. The ledger identifies that build unambiguously, because nothing was built
between 12:01 and 12:41:

```
2026-09-11T12:01:24Z  code 6b7939bd  md5 ff80f056  6144 bytes  a925038  clean  -DLS_PHASE2
```

`a925038` rebuilds to code `6b7939bd` exactly. **Identify a build from `build_ledger.tsv`,
never from a comment in the source** — a comment in `LS120TR.ASM` named
`bc453ca`/`976e4114` as "the build that enumerated this drive" and cost an evening.

## Fixed this session

- Build stamps code hash, commit and mode into the INF model name, so an installed driver
  names itself in Device Manager.
- Deploy must write **both** the install source and `IOSUBSYS`.
- Transport budgets restored for the synchronous miniport: reset settle 92 ms → 3.0 s,
  BSY → 1.0 s, DRQ → 0.5 s. The async restructure had cut them ~10x because it drew its
  seconds from a tick budget a synchronous miniport does not have.
- Dead-bus test narrowed to `FFh` only; `00h` is a legitimate post-SRST status.

## Measured on hardware tonight (both committed with captures)

- **SPP works.** Four `READ(10)`s, LBA 0 returns a real `MSWIN4.0` boot sector. Both queued
  unit attentions (ASC `29`, `28`) drain as documented. `RW_1930.OUT`.
- **ECP is broken.** Identical script, only the block helper swapped: every task-file
  register reads `F5` (BSY+ERR) after one ECP burst. `RWECP_1935.OUT`. The probe's leave
  already mirrors `LS_EcpLeave`, so this reproduces the driver's defect at DOS — the bisect
  that cost three fix-and-run cycles in Windows, now one flag.

## Next

1. Install from `D:\LS120MP` — one entry, `a925038 12:01 build`. Boot.
2. If it enumerates but has no letter: `UserDriveLetterAssignment = II` is still armed in
   the hive, pinning the drive to `I:` which the Nakamichi holds. Device Manager → Disk
   drives → `MATSHITA LS-120 COSM 04` → Settings → reserved drive letters → `L`.
3. ECP bisect at DOS: stub the burst, then the descriptor alone, then one chunk.
4. Backfill into the 86Box bed: 3584-byte burst ceiling, two queued unit attentions,
   seconds-long SRST settle, `FORMAT UNIT` refusing FmtData=0.
