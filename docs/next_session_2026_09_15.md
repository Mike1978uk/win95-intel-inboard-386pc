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


## RESULT: the a925038 baseline also fails (2026-09-14 22:50)

`BOOTLOG_final_a925038.TXT`, binary on the card verified `5b7b88d2` / code `6b7939bd`,
in **both** the install source and `IOSUBSYS`:

```
[0016D800] Initing ls120mp.mpd
[0016D802] Init Success ls120mp.mpd      <- 2 ticks
```

**No drive.** So restoring the photographed build is not sufficient, and the install-source
fix - real though it was - was not the whole story.

### ⛔ Correction to the retraction above

**2 ticks does not uniquely mean "skeleton".** `a925038` *does* call `LS_BringUp` at init,
and it still returned in 2 ticks. Its `LS_AtaSoftReset` polls through `LS_RegRead`, which
ends in an unconditional `clc`, so a task file reading `00h` looks like BSY-clear, the reset
"completes" instantly, the ATAPI signature check fails and bring-up bails. The skeleton and a
failing phase-2 build produce the same 2 ticks.

The 09-09 retraction still stands, but on **file** evidence, not on the tick count: the
Thursday image shows `IOSUBSYS\LS120MP.MPD` was the 5120-byte Sept 7 binary on that date.

### The real question, restated

**The transport works perfectly from DOS and reads nothing from Windows, minutes apart, same
hardware.** Tonight: four clean `READ(10)`s and a real boot sector off LBA 0 at 19:20; at
22:50 the same drive's task file gives a phase-2 miniport nothing. Chasing *which build* was
largely beside the point - every phase-2 build fails identically if the port is not usable
under Windows.

### Top lead: the ECR is never normalised in a925038

`a925038` has **no `LS_DetectEcp`**. Today's transport does, and it does something the
baseline never does:

```
;  RESTORE BEFORE DECIDING. Any ECR mode with bits 7:5 set makes the port
;  bidirectional, and the nibble path reads status through it - so a probe
;  that leaves byte mode behind breaks the SPP fallback it was probing for.
        mov     al, bl
        and     al, 01Fh                ; mode 000 - plain SPP
        out     dx, al
```

Under DOS the port is already in SPP, so the baseline works. Under Windows, if anything has
left the ECR in a non-SPP mode, **every nibble register read returns garbage** - which is
exactly `00h` task file, instant bring-up failure, 2 ticks, no drive. This also matches the
owner's own instinct on 2026-09-14 that the *port* was worth changing.

Corroborating: `F5` on all registers after an ECP burst at DOS is the same class of fault -
the port left in the wrong mode poisons the nibble path.

**Test first next session:** take `a925038` and add **only** `LS_DetectEcp`'s ECR
normalisation (write `ECR & 1Fh`, mode 000) at the top of `LS_BringUp`. One change, and the
INF now stamps the build so Device Manager will say which one is installed.

Ordering note from the same boot: `ls120mp.mpd` inits at `0016D800`, `lptenum.vxd` loads at
`0016D834` - 52 ticks later - so `lptenum` is not the contender at init time. `lpt.vxd`
loading earlier has not been ruled out.

### Also this session

The owner changed the adapter's port setting to `378h` (same as LPT1) in Device Manager and
**Explorer froze**. Not diagnosed. Note `LS120MP.INF` carries `DontLoadIfConflict=Y`, and its
`LogConfig` requests `378-37F`, so a resource conflict with the printer port is a live
possibility and would make IOS decline the miniport silently.

## Next

1. Install from `D:\LS120MP` — one entry, `a925038 12:01 build`. Boot.
2. If it enumerates but has no letter: `UserDriveLetterAssignment = II` is still armed in
   the hive, pinning the drive to `I:` which the Nakamichi holds. Device Manager → Disk
   drives → `MATSHITA LS-120 COSM 04` → Settings → reserved drive letters → `L`.
3. ECP bisect at DOS: stub the burst, then the descriptor alone, then one chunk.
4. Backfill into the 86Box bed: 3584-byte burst ceiling, two queued unit attentions,
   seconds-long SRST settle, `FORMAT UNIT` refusing FmtData=0.
