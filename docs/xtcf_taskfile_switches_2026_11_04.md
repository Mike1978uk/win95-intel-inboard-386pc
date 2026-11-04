# XT-CF miniport: two per-command switches and how to test them (#57)

Built for checking only: `XTIDETR.ASM` assembled with JWasm v2.20, `XTIDEMP.ASM` not (it needs
the DDK's `miniport.inc` and `srb.inc`). No `.MPD` has been linked or run. Branch
`claude/xtcf-tight-taskfile`. Review that proposed them: `docs/driver_review_2026_11_04.md` item 4,
branch `claude/driver-optimisation-review`.

## What changed

Two build switches, both off by default. **With both off, the assembled code and data are
byte-for-byte master's** (JWasm, the shipped flags `-DXT_NO_WRITETEST -DXT_POLL_BACKOFF
-DXT_FAST_XFER -DXT_WAIT_FIRST -DXT_YIELD_COMMIT`: `.text` 0x90F and `.data` 0xBEA bytes, identical
disassembly and data). The shipped driver is unchanged unless a switch is given.

| switch | define | what it does | expected gain |
|---|---|---|---|
| `-TightTaskfile` | `XT_TIGHT_TASKFILE` | loads the five taskfile values and their ports into registers before the first OUT, so no memory read follows an OUT (each OUT empties the L1) | ~10 line fills a command, ~15 us |
| `-SkipDrvHd` | `XT_SKIP_DRVHD` | writes DRV/HEAD, and waits for the select, only when its value changes | one OUT, at least one status read and their refills a command, ~10-20 us |

With both on: `.text` +115 bytes, `.data` +4. No 686-only instructions. Every load in the
tight block was checked against its relocation (`XTIDE_oCylHi`, `oCylLo`, `oSecNum`, `ReqCount`,
`pSecNum`, `pSecCnt`, `pCylHi`, `pCylLo`, `pStatus`, `Cmd`).

`-SkipDrvHd` forgets the last value (so the next command selects again) at the start of the probe
(`XTIDE_TryIdentify` selects units itself), when a select is not honoured, and after any failed
read or write (`xfr_fail`). Nothing else writes DRV/HEAD; `XtResetBus` does not touch the card.
The wait for BSY before programming the taskfile is kept, so a write commit left running by
`-YieldCommit` is still waited out.

Not done, with reasons:
- **READ/WRITE MULTIPLE** (review 4c): needs IDENTIFY word 47, SET MULTIPLE at init and after any
  reset, and per-block loops. Next, once these two pass.
- **Status port in a register across sectors** (review 4d): dropped. The port cannot stay in a
  register across `XTIDE_ReadData` without a stack spill, and the spill costs the same line fill
  it would save.

## Building (owner, DDK)

The current card build plus one switch, each in its own folder under `build\` (git-ignored), so
no test build overwrites the shipped one:

```
.\build.ps1 -PollBackoff -FastXfer -WaitFirst -YieldCommit -TightTaskfile -OutDir build\tight
.\build.ps1 -PollBackoff -FastXfer -WaitFirst -YieldCommit -SkipDrvHd     -OutDir build\skip
.\build.ps1 -PollBackoff -FastXfer -WaitFirst -YieldCommit -TightTaskfile -SkipDrvHd -OutDir build\both
```

Each prints its md5 and adds a ledger row with its flags. Note the md5s: they prove later which
file loaded (technique 74).

## Tools

- `tools/xtsect/XTSECT.EXE` (new): times 500 absolute sector reads, one sector at a time and then
  eight, three passes, through Windows 95's INT 25h service (VWIN32), which goes through the
  miniport. **Read only.** `XTSECT <label>` appends to `C:\XTSECT.TXT`. A line marked
  `(cached?)` was served faster than the bus allows and does not measure the driver.
- `WRTEST` (`tools/fixtures/WRTEST.BAT`, `drivers/xtide_mpd/test/`): the existing write and
  compare test, 129 files.

## Stage 1: the 86Box bed

Bed `vm_5160_now` (today's card image, as on 2026-10-11). Not `vm_xtide_mpd`: its image falls
back to RMM silently (`xtcf_measured_wait.md`). The bed proves the driver loads, reads and writes
correctly; its timings do not count (it misprices I/O, `mach8_driver_method.md` §3).

For `tight`, then `skip`, then `both`:
1. VM stopped: copy the build's `XTIDEMP.MPD` over the image's `C:\WINDOWS\SYSTEM\IOSUBSYS\XTIDEMP.MPD`
   (`tools/fatcp.py`), keeping the original first.
2. Boot. `BOOTLOG.TXT`: XTIDEMP loads and `Init Success`, and RMM does not take the disk.
3. Run `WRTEST`: 129/129 match.
4. Run `XTSECT bed_<name>`: every read succeeds.
5. Shut down cleanly; boot once more (the shutdown path is where a port driver has hung before).

Pass: all five for all three builds.

## Stage 2: the 5160

Only after stage 1 passes. One build at a time, `both` last.

1. **Baseline first, on the current driver:** `XTSECT base` three times, nothing else running.
2. **Back up the exact current file** (`a67fefc4`), not the older `XTIDEMP.SHP`:
   `copy C:\WINDOWS\SYSTEM\IOSUBSYS\XTIDEMP.MPD C:\XTIDEMP.A67`.
3. Copy the test build over `XTIDEMP.MPD`; reboot.
4. Prove it loaded: md5 of the file matches the build's, and `BOOTLOG.TXT` shows `Init Success`.
5. `WRTEST`: 129/129. Then `XTSECT <name>` three times.
6. **Revert** if anything misbehaves: F8 at boot, command prompt only,
   `copy C:\XTIDEMP.A67 C:\WINDOWS\SYSTEM\IOSUBSYS\XTIDEMP.MPD`.

Pass: WRTEST clean, a normal boot and shutdown, and the one-sector mean `us/read` lower than the
baseline by more than the baseline's own run-to-run spread. The one-sector figure shows the
per-command cost best; the eight-sector figure should barely move.

Results to `docs/captures/<date>_xtcf_switches/` and a row each in this file.

| build | bed | 5160 WRTEST | 5160 one-sector us/read (3 runs) | vs base |
|---|---|---|---|---|
| base `a67fefc4` | - | - | | - |
| tight | | | | |
| skip | | | | |
| both | | | | |
