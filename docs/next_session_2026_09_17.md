# LS-120 — handoff, 2026-09-17

## ⛔ Read this first: what is PROVEN is not what is BUILT

The DOS/COMrade work proved the **wire protocol**. It did not prove the **driver**.

> *"Today's proofs came from DOS probes — DEBUG assembly transliterated from the driver's
> sequences. Same wire protocol, not the same binary. Every line of `LS120MP.MPD` I changed
> is built and unexecuted."* — owner

That is technique 111a in its most expensive form. A sequence proven at a DOS prompt says
nothing about the miniport that re-implements it, and this project has twice believed
otherwise.

## State of the LS-120, by component

| component | state |
|---|---|
| Transport protocol — connect, SRST, bring-up, registers, unit-attention drain, ATAPI packet, block read, block write, 3584 ceiling, LBA 0 / 100000 / 230000 | **proven on hardware** |
| ECP per-register | **proven** |
| ECP block | decoded, partially working, **wedges** |
| `LS_BridgeProbe` presence gate | built, **never run** |
| `LS_MAX_XFER = 3584` in the driver | built, **never run** |
| Short-phase DRQ check | built, **never run** |
| SRST-as-a-state | built, **never run** |
| SCSIPORT side — SRB dispatch, timer engine, DCB, drive letter | **never worked**, `Init Failure` at 1598 units |
| FORMAT UNIT | refused — ILLEGAL REQUEST |

**The two halves have never been proven together on hardware.** The wire protocol works at a
DOS prompt; the miniport fails at bring-up before it can use it.

## Measured in the 86Box bed this session

Bed `vm_ls120win/`, `86box_upstream` build, `busy_ms = 750`. Every run now writes
`86box.log.<tag>.provenance` recording the driver md5 taken **from inside the image**, so no
bed result is unattributable again.

| binary | built | `Init` | reads | writes | outcome |
|---|---|---|---|---|---|
| `976e4114` | 09-11 20:14, `bc453ca` **DIRTY** | Success @ **6** | 10, all completed | **1, never completed** | 19,721 CONNECT/DISCONNECT retries, Explorer egg-timers |
| `5b7b88d2` (= `ff80f056`) | 09-11 12:01, `a925038` clean | Success | 46 | 3, small ones complete | **BSOD on a 30,720-byte `WRITE(10)`** |
| `762fe8ac` | 09-11 00:17, `cb4b62a` clean | Success @ 15 | 45, all completed | 3, small ones complete | **same BSOD, same 30,720-byte `WRITE(10)`**. Also the artefact published in `dist/ls120_mpd/` |

**Both Friday builds are functionally equivalent in the bed and fail at the identical
instruction**, so "which build was in the photo" no longer gates anything. Only `976e4114`
differs, and it is strictly worse - every write hangs regardless of size.

### The 30,720-byte write is confirmed, and it is the recorded open item

```
PACKET, byte count 30720
CDB 2A 00 00 00 0A 87 00 00 3C 00 00 00     WRITE(10), LBA 0x0A87, 0x3C = 60 blocks
data phase out, 30720 bytes, request length 30720
DATA OUT head 4D 5A C6 00 ...               "MZ" - COMMAND.COM
```

`a925038` predates the 4096-byte cap (`1a73970`) so it has no cap at all, and the measured
hardware ceiling is **3584 bytes per burst**. The driver asks for ~8.5x that in one data-out
phase. `LS_MAX_XFER = 3584` is built and has never executed — this is the first evidence of
what it is for.

### The bed and the hardware bring-up failure - it depends which build

For the **Friday** builds the bed diverges: they enumerate here and fail on the 5160, and
`busy_ms = 750` did not close that. So "the drive is slow to go ready" is falsified as the
explanation for *their* hardware failure.

**But HEAD reproduces the hardware symptom exactly**, which is what made this session's fix
possible:

```
86box.log.HEAD_SPP_75b7024c   driver md5 db8dce5f
[00000261] Initing ls120mp.mpd
[00000267] Init Success ls120mp.mpd
LSPROBE.TXT:  no ls120 volume
```

`Init Success` with no volume is the `BOOTLOG_final_a925038.TXT` symptom. **`Init Success` was
never evidence that bring-up worked** - `HwInitialize` is deliberately non-vetoing (`c122377`),
so a failed bring-up still reports success and simply presents no devices.

## ✅ FIXED: `LS_StatusRead` rejected 00h as a dead bus

`bbe73b2`. Both HEAD arms - ECP and SPP - died at the identical instruction with identical
1,477-byte logs, byte-for-byte the same as a working Friday run up to `W reg 1E = A0` and then
nothing. That exonerated the transport and left one call:

```asm
;  LS_PacketCommand, between drive select and byte count
	mov	bl, ATA_ST_BSY OR ATA_ST_DRQ
	call	LS_PfWait
	jc	lpc_fail
```

`LS_StatusRead` raised the dead-bus carry for **00h**, while `LS_PfWait`'s float handler
applies the **FFh-only** rule and cannot tell that carry from a real dead bus - so it spun out
all 22,000 iterations, returned carry, and every command failed before the byte count. The
FFh-only narrowing recorded in `next_session_2026_09_15.md` had been applied to the other two
readers and **missed here**.

The short-phase DRQ check is not wrong. It is simply the first code to call `LS_PfWait` before
a command, so it walked into a latent bug Friday's build never touched.

### Result, verified from the host

`FIX_STATUSREAD_fe657dc8` / md5 `1ac15b08`, SPP:

| | |
|---|---|
| `Init Success`, `drive D` | enumerates |
| `DIR` | reads |
| `ECPTEST.BIN` | **92,870 bytes** - 3x the transfer that BSOD'd both Friday builds |
| host-side md5 vs source | **`ea923fa0…` identical** |

`LS_MAX_XFER = 3584` is what makes that work: SCSIPORT splits the copy into bursts the bridge
can carry. **All five built-but-never-run changes have now executed.**

## Retractions and corrections made this session

- ⛔ **`ls120-emulation-bed-works-2026-09-11`'s "models no timing" is retracted.** Drive
  latency has been modelled since `92239ba2c`/`c4e5ca6df`/`20c76af08` (09-12/09-13).
- ⛔ **"512 bytes stalls too, so size is not the discriminator" was wrong** — that was the
  `976e4114` control, which has a separate defect where *every* write hangs. The
  "size, not transport" finding stands.
- ⛔ **The 12:10 photo build was identified by assuming a 12:01 build was deployed within
  nine minutes.** The ledger proves when a build happened, not that it reached the card.
  `762fe8ac` (00:17) remains a live candidate.
- 🛑 **A binary on the card was renamed `LS120MP_FridayEnumerating.MPD` by guess.** Its md5 is
  `762fe8ac` — the 00:17 build, not the 12:01 one. The real 12:01 build sits beside it as
  `LS120MP.bad`. **Name a build from `build_ledger.tsv`, never by hand.**

## Machine state

- CF in the reader at `D:`. Vendor driver REM'd in `CONFIG.SYS` (both `SD120PPD.SYS` and
  `ASPIHDRM.SYS`); restore with `copy C:\CONFIG.B4 C:\CONFIG.SYS`.
- **`D:\WINDOWS\SYSTEM\IOSUBSYS\LS120MP.MPD` does not exist** — only `.B13`-`.B25` backups.
  The driver is not installed.
- Friday's own `CONFIG.B4S` (15:24) and `CONFIG.B4B` (16:58) both have the vendor driver
  REM'd; `CONFIG.LS` (09-10 16:58) has it active. So it was switched off somewhere between
  Thursday evening and 15:24 Friday — **not pinned for 12:10**.
- `vm_ls120win/nvr/mach8.nvr` was missing and has been restored. Without it Win95 dies with a
  protection error on every image.

## Proven in the bed - all four combinations, byte-exact

| transport | 92 KB (`LSWRITE`) | 9 MB Creative ZIP (`LSBULK`) |
|---|---|---|
| **SPP** `fe657dc8` / `1ac15b08` | `ECPTEST.BIN` 92,870, md5 `ea923fa0…` | `BULK.ZIP` 8,996,287, md5 `ea039999…` |
| **ECP** `7097b586` / `fc2088ab` | `ECPTEST.BIN` 92,870 | `BULK.ZIP` 8,996,287, md5 `ea039999…` |

Every file checked **from the host** against its source, not from the guest's own `DIR`. The
9 MB case is the owner's own real-use test - the copy that used to fail.

## Real-use validation of the SHIPPING build

`SHIP_AUTO_FIXED.MPD` - `code 9a7668d5`, md5 `e328ed54`, **auto** mode. Owner-driven in the
bed, not a fixture:

1. 9 MB Creative ZIP copied to the medium - **worked**
2. Explorer disk format - **clean**
3. Reboot
4. ScanDisk - **clean**

Host-side check of `rd.img` afterwards: 246,528 sectors (exactly the LS-120 capacity and the
image size), boot signature `55 AA`, media `F8`, **`FAT1` and `FAT2` byte-identical** across
123,392 bytes each, **0 bad clusters**. Two FAT copies written through the transport and
agreeing is a write-integrity check for free.

### 🔑 Windows' format never issues FORMAT UNIT - DEMOTE that open item

CDB census for the run: **3,266 `WRITE(10)`**, 330 `READ(10)`, 23 TEST UNIT READY, 21
PREVENT/ALLOW, 3 READ CAPACITY, 2 INQUIRY, 2 REQUEST SENSE, and **zero `CDB 04`**.

Explorer's format on a removable is **filesystem-level** - boot sector, both FATs, root
directory - and never sends the ATAPI `FORMAT UNIT`. So the hardware refusal recorded in
`FMT2_illegal_request.OUT` (ILLEGAL REQUEST, `FmtData=0`) **does not block the workflow the
owner actually wants**. It remains a real gap for a DOS-level format and nothing more.

Note the bed's `FORMAT UNIT` is a stub that completes instantly unless the medium is
read-only (`rdisk.c` ~1333) - it cannot fail, so it would have been weak evidence anyway. It
was never called.

## STAGED ON THE CF, 2026-09-17 - not yet booted

`e328ed54` (**auto**, `code 9a7668d5`, commit `4ea1dee`) written to **both** locations:

| path | md5 |
|---|---|
| `D:\LS120MP\LS120MP.MPD` (install source) | `e328ed54` |
| `D:\LS120MP\LS120MP.INF` (stamped, CRLF 135/135) | `d6adc0c3` |
| `D:\WINDOWS\SYSTEM\IOSUBSYS\LS120MP.MPD` (load path) | `e328ed54` |

⛔ **An earlier staging put `fc2088ab` on the card - an `LS_FORCE_MODE=2` build, which the
source itself calls "PINNED AT BUILD TIME - a test build".** Forcing makes `LS_DetectEcp` set
`LS_HasEcp = 1` without probing, and **ECP block has never once succeeded on this hardware**
(`ECP7_block_fails.OUT`). The nibble fallback survives forcing only for a *negotiation
refusal*, which is not the observed hardware failure. Replaced with the auto build, which
probes honestly and falls back. **Do not ship a `FORCE_MODE` build.**

Verified at the destination, not the staging copy. Previous card files backed up as
`LS120MP.I17` / `LS120MP.M17`; the INF they replace still said `a925038 12:01 build`, which is
the mislabelling that cost the week. Device Manager will now read
`LS-120 EPAT  7097b586  bbe73b2  ecp`.

**Prerequisites checked:** `inbrdpc.sys` is safelisted in `IOS.INI` (line 290) - that is the
gate that matters, the unrecognised `INT 13h` hooker, not our miniport. Vendor driver REM'd in
`CONFIG.SYS`, so this is a clean control.

**`IOSUBSYS\LS120MP.MPD` did not exist before this** and the device node was removed, so this
is an **install**, not a refresh: Add New Hardware, **decline autodetect**, point it at
`D:\LS120MP`.

## Next

1. **Boot the 5160.** The hardware bring-up failure has the same symptom as the one fixed
   here, so the same cause is a good candidate - **a prediction, not a result.** If it still
   fails, the bed now reproduces this class of bug, so diff the hardware `BOOTLOG` against a
   bed run rather than theorising.
3. Backfill the remaining 09-14 hardware findings into the bed: the 3584-byte burst ceiling
   and the seconds-long SRST settle are still not modelled (the branch stops 09-13), plus
   `FORMAT UNIT` refusing `FmtData=0`.
4. **`dist/ls120_mpd/LS120MP.MPD` is `762fe8ac`** - the 00:17 build, which predates the CDB
   block-path fix and BSODs on a 30,720-byte write. Whatever `FIXES.md` claims for the LS-120
   needs checking against that before the next release.

## Process notes from this session

- **The ledger dirties its own tree.** It is tracked, so the first build of a pair records
  `tree clean` and the second `DIRTY` purely because the first appended a row. Read the `code`
  column, not `tree`, when attributing a pair built back to back.
- **Two mode builds overwrite each other.** `-Mode ecp` and `-Mode spp` both write
  `build/LS120MP.MPD`; the second silently destroyed the first, putting `be9e215a` in the
  ledger and nowhere on disk. Copy each to a distinct name immediately after building.
- **The md5 moves when the source does not.** `be9e215a` and `124648b5` are the same source
  (`code 7bc9ebec`) built twice - the PE timestamp differs. **The `code` column is the
  attribution; the md5 identifies a file, not a build.**
- `tools/ls120_bed_run.ps1` now writes `86box.log.<tag>.provenance` recording the driver md5
  read back from **inside the image**. Fifty archived bed runs predate it and cannot be
  attributed - including the `x512_bulk` log showing 3,077 completed writes.
