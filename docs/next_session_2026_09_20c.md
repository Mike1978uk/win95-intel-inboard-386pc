# Mid-session handoff, 2026-09-20 afternoon

Written mid-session. The headline is a **retraction**: the figure that has been
ranking this project's optimisation work was measured on a transfer that does
not deliver correct data.

---

## 1. ⛔ The DOS+ECP write is corrupt, and the "Windows stack" figure is withdrawn

Full record: `docs/dos_ecp_write_is_corrupt_2026_09_20.md`.

| arm | timed | verified |
|---|---|---|
| Windows + EPP | 43.45-51.90 s / 4 MB | ✅ `FC: no differences encountered` |
| DOS + ECP | 34.55-35.70 s / 4 MB | ⛔ **never checked** |

Today's repeat verified it. **First 16,384 bytes correct, then ~62% of bytes
wrong**, differences across all four byte positions of a dword. The source was
ruled out first: `C:\BOUNDARY.BIN` on the CF hashes `a2ea9a7a...`, identical to
the known-good copy.

`T2.TXT` on the card holds last night's DOS timings and **contains no `FC`
line**. `T3.TXT`, the Windows run, does, and passed.

**Withdrawn:** *"DOS+ECP does the same 4 MB in 35 s, so the Windows stack costs
10-18 s and is the bigger lever."* Also withdrawn: today's restatement of it as
"~8.9 s, ~20%" by access arithmetic - same DOS number, same flaw.

**Not withdrawn:** the ECP **read** result. `GOODTIME.MPG`, 36.7 MB, written
over EPP and read back over ECP, FC clean. *Serving is reading.* The ECP write
direction had never been exercised. Technique 129.

⚠ Only copy 2 (an overwrite) was verified corrupt; copy 1's output was
overwritten before FC ran. **Whether the create path is also broken is
unknown** - the corrected batch now tests it.

---

## 2. What was measured today, and stands

All self-contained, controlled, unaffected by §1.

### I/O access cost by width (`docs/iowidth_measured_2026_09_20.md`)

| width | us/byte | vs byte |
|---|---|---|
| byte | 5.695 | — |
| word | 3.819 | -32.9% |
| dword | **3.183** | **-44.1%** |

Three runs, 0.4% spread. An undecoded-port control matched the Intek21 to
**0.1%**, so the Intek card adds no wait states and the cost is the Inboard's.
**Quote 44%, not the 51% the two-point fit predicted** - the fit is 10.5%
sub-linear.

### ISA memory vs I/O (`docs/isa_memory_vs_io_2026_09_20.md`)

| path | fixed | per bus cycle | byte us/B |
|---|---|---|---|
| ISA **I/O** | **3.752** | 1.943 | 5.695 |
| ISA memory `0xD8000` | **0.883** | 1.978 | **2.861** |
| ISA memory `0xD0000` | 0.864 | 2.901 | 3.765 |
| ISA memory `0xB8000` Mach8 | 0.909 | 3.805 | 4.714 |
| `0xF000` - **shadowed, not on the bus** | 0.962 | 0.151 | 1.113 |

**Per-cycle is the same; the fixed term is 4.25x worse for I/O.** Three real
memory windows agree on the fixed term to 3%. A card's zero-wait-state setting
moves the **per-cycle** term only.

⛔ **This corrects E4 in `bus_optimisation_plan.md`**, which was recorded
CONFIRMED and justified buying memory-mapped storage: `F000` is shadowed not
"across the bus", `B800` is 4.714 us/B not 0.454, and the I/O row halves 3.82
twice. **Memory-mapped storage is worth ~2x, not 4.2x**, and only with a
low-wait-state card.

⛔ **Nothing on this machine can use it.** Every data path but the Mach8
framebuffer is port-only. **The T130B has no memory window** - no Trantor ROM
on the machine (the two present are a floppy BIOS at `0xD0000`, base `0x03F0`
IRQ 6, and XUB212 r638 at `0xD8000`), no `MemConfig` in `T130.INF`, and
`T130.MPD` imports only I/O-space calls. Do not re-propose.

### Closed

- **B4** (`rep movsd` for buffer copies): **no bulk byte-copies exist**. Every
  hit was init-time model-string work, a fallback compiled out on the shipped
  build, or the latch transport this card lacks.
- **Technique 126i**: `TBW.BAT` deleted the target **once before the pair**, so
  copy 2 overwrote rather than repeated. The "copy 2 was faster, so the latch
  never tripped" reading does not hold.

---

## 3. B1 - where it actually is

**`MaximumTransferLength` is already 65536.** The cap is
`NumberOfPhysicalBreaks = 0`, which promises SCSIPORT one physical run per SRB.
This miniport never touches a physical address (`MapBuffers` TRUE,
`NeedPhysicalAddresses` FALSE, `rep insw` on a virtual `DataBuffer`), so the
promise cost transfer size for nothing.

Now a build option: **`-PhysBreaks 17`** (64 KB of 4 KB pages). Default
unchanged.

⚠ **`XT_SG` does not exist in the source** - the plan's "re-enables `XT_SG`" is
stale.

⚠ **Async completion is a separate lever.** `xsi_complete` fires
`RequestComplete` then `NextRequest` back to back, so **queue depth is 1 by
construction**. Bigger *single* SRBs come from the two fields above; *merging
multiple* SRBs needs async + `MultipleRequestPerLu`. Two mechanisms, and only
the second needs the trunk. The LS-120's `LsPendSrb`/`LsTimer` is a working
in-repo reference for that trunk when it is wanted.

### The two arms, and a trap avoided

The live driver is code **`20535159`**, commit `ecb53d4`, tree clean, flags
`-DXT_NO_WRITETEST -DXT_POLL_BACKOFF -DXT_FAST_XFER`.

**`XT_FAST_XFER` gates the entire `rep insw`/`outsw` fast path - the shipped
35%/33% win.** The first two builds today omitted it and would have measured a
driver nobody runs. Rebuilt with the live flags, today's HEAD reproduces
`20535159` **exactly**, so the source has not drifted and the live binary is
rebuildable.

| arm | code | flags |
|---|---|---|
| baseline | `20535159` | `-PollBackoff -FastXfer` |
| test | `01be936b` | `-PollBackoff -FastXfer -PhysBreaks 17` |

Pre-flight: `XT_MAX_CHUNK` is 128 sectors (fits the 8-bit ATA count register),
all request state is `dd`, and `XferRun` already loops chunks - so a larger SRB
is handled. **The untested surface is that 64 KB has never actually been
exercised**, because `NumberOfPhysicalBreaks = 0` capped it, and this driver
serves the boot volume.

---

## 4. Staged on the CF (done while it was in the host reader)

| file | what | verified |
|---|---|---|
| `\WINDOWS\SYSTEM\IOSUBSYS\XTIDEMP.PB` | the `-PhysBreaks 17` arm | code `01be936b` at the destination |
| `\WINDOWS\SYSTEM\IOSUBSYS\XTIDEMP.B20` | byte copy of the live driver, for revert | code `20535159` |
| `\TBD.BAT` | 4 MB write+verify, **now deletes before each copy** | md5 matched |
| `\TBS.BAT` | **256 KB** write+verify - seconds, not 7 minutes | md5 matched |
| `\BOUND256.BIN` | 262,144 bytes, first 256 KB of `BOUNDARY.BIN` | md5 matched |

⛔ **`XTIDEMP.PB` is staged, NOT installed.** Do not copy it over `XTIDEMP.MPD`
until the bed has passed it. Revert is `COPY XTIDEMP.B20 XTIDEMP.MPD`.

**Use `TBS` before `TBD`.** Corruption starts at 16 KB, so 256 KB catches it
sixteen times over and verifies in seconds. `TBS` writes FC to its own
`T5FC.TXT` and reports that file's **size** - about 60 bytes means clean.

⚠ `D:\BOUNDARY.BIN` on the LS-120 cartridge is **known corrupt**. Delete it.

**Logged as [issue #27](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/27)** - LS-120: DOS ECP writes corrupt everything past the first 16 KB.

---

## 4b. Bed result: `-PhysBreaks 17` PASSES the correctness gate

`vm_xtide_mpd`, image cloned from `xtidemp.img`, driver injected into **both**
`\WINDOWS\SYSTEM\IOSUBSYS\` and the `\XTIDEMP\` install source (the
install-source lesson), both verified back out as code `5670a4a1`.

The bed baseline is code **`f8e1e3a2`** (commit `a75b7f3`, flags as live **plus**
`-DXT_FORCE_BASE=0300h`) - note the bed and the card were on **different
builds**, so no bed number has ever been comparable to a card number. Today's
HEAD reproduces both baselines exactly.

| check | result |
|---|---|
| `Initing xtidemp.mpd` -> `Init Success xtidemp.mpd` | ✅ |
| `INITCOMPLETESUCCESS` DiskTSD / SCSIPORT / VFAT / IFSMGR | ✅ all four |
| boot depth | reached `InitDone = TSRQuery` - fully up |
| BOOTLOG is from **this** run | ✅ md5 and size differ from the source image |

✅ **The illegal instruction is pre-existing bed behaviour, not the change.**
The run logged `[0117:0000B929] Illegal instruction 00008B55 (FF)`. A baseline
run through the **same `-L` channel** produced a log **byte-identical apart
from the timestamp line**, same illegal instruction included.

⚠ The six older `stderr_*.txt` files are **21 bytes each** - only
`SDL: version 3.4.12`. They never captured the pclog channel, so "absent from
previous runs" would have been meaningless. Technique 127a: a control has to
come through the same instrument.

⚠ Boot success is not a data-path test. It shows SCSIPORT accepted the larger
`NumberOfPhysicalBreaks` and the volume mounted; it does not show a 64 KB
transfer is byte-correct, and 64 KB has still never been exercised.

## 5. Next, in order

1. **Bed-test `XTIDEMP.PB`** for correctness. It serves the boot volume; the
   bed is free and this is exactly what it is for.
2. **`TBS D:` on the 5160** - does the DOS+ECP *create* path corrupt too, or
   only the overwrite? That decides whether this is a write bug or an
   overwrite bug.
3. **Then `-PhysBreaks` on hardware**, with a verified baseline this time.
4. **LS-120 dword** remains the best-evidenced transport lever at 44%/byte, and
   is still blocked on a readout of `[0x20D9D]` / `[0x20BF7]`.
5. **The Windows-stack work has no measurement supporting it any more.** It may
   still be real; nothing establishes it. Do not rank it first again without a
   verified pair.

## 5b. ⛔ The 86Box branch is NOT in a fit state to submit

Checked 2026-09-20 with the repo-hygiene procedure.

`86box_upstream` is on **`lpt-epat-bridge`**, **34 commits ahead** of
`86Box/86Box` master and **21,833 behind**. A PR from that base is
unmergeable, and the branch is not submission-shaped regardless:

- it carries **`03ddc5eb1`**, TC1995's own upstream Mach8 fix cherry-picked in,
  which would conflict with itself;
- it carries project-only diagnostics - `LS-120 trace ring dump to the host`,
  `Make the leftover diagnostics opt-in`;
- `src/io.c` holds an uncommitted access-width tally added today as a local
  instrument. **It must not go upstream.**

What a submission needs: rebase onto current master, drop the cherry-pick,
drop or gate the diagnostics, squash ~34 iterative commits into a coherent
series, and a PR body saying what was tested and what was not. That is a
session of its own.

⛔ Standing rule: no push to the fork without the owner's yes on a **tested**
branch (`feedback-no-pushes-without-go-ahead`).

## 6. Method notes worth keeping

- `run_command` over COMrade **times out at 8 s while the machine carries on**.
  Read the screen; do not call it a failed run.
- A redirected file's directory entry does not update until close, so
  `file_read`/`file_stat` on a log being written reports the stale size. Read
  it off the card, or wait for the handle to close.
- **Do not read multi-MB files over the serial link.** `T4.TXT` was 13.3 MB;
  the CF went into a host reader instead. Owner's standing preference, and the
  same applies to `CONFIG.SYS` edits - say which lines to change and let them
  do it in `EDIT`.
