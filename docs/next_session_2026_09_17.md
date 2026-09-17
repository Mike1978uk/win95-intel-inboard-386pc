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
| `762fe8ac` | 09-11 00:17, `cb4b62a` clean | *(run pending)* | | | also the artefact published in `dist/ls120_mpd/` |

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

### ⛔ The bed does NOT reproduce the hardware bring-up failure

| `976e4114` | real 5160 | bed |
|---|---|---|
| | `Init Failure` @ 1520-1598 | `Init Success` @ **6** |

Unchanged by `busy_ms = 750`. The spindle model was added specifically to close this and it
did not, so **"the drive is slow to go ready" is falsified as the bring-up explanation.**

Consequence: the bed is good for the **write path** and is **not evidence** about enumeration
on the 5160. Do not read a bed `Init Success` across to hardware.

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

## Next

1. **Finish the `762fe8ac` bed run** and compare where all three Friday builds fail.
2. **Backfill the 09-14 hardware findings into the bed** — 3584-byte burst ceiling and the
   seconds-long SRST settle. The branch stops at 09-13 and neither is modelled, which is why
   the bed cannot currently show a bring-up that bails in 2 ticks.
3. **The bring-up gap is the real blocker.** The wire protocol is proven and the miniport
   still fails at `Init` on hardware. Standing lead: `a925038` has no `LS_DetectEcp`, so it
   never restores the ECR to mode 000 — a non-SPP ECR makes the port bidirectional and breaks
   every nibble read. Works at DOS (port already SPP), fails under Windows. **Test baseline +
   ECR normalisation ONLY.**
4. Exercise the four built-but-never-run changes one at a time, in the bed, and record each
   result against its md5. Nothing on that list may be described as working until it has run.
