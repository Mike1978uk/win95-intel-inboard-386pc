# Mach8 levers applied to the other drivers - static audit, 2026-11-01

Desk reading only; nothing here is measured on the card. Levers (from `docs/next_session_2026_11_02.md`):
L1 compact aligned hot code between port accesses (each access flushes L1); L2 shadow a write-only register
and skip unchanged writes; L3 wait for the one bit needed, not for idle. Costs used: 5.55-5.70 us per I/O byte,
3.82 us/byte for `rep insw/outsw` (`docs/iowidth_measured_2026_09_20.md`).

## Verdict

The storage drivers are bus-bound by their data phase (96-98% of a request). The levers apply, but the
ceiling is small: well under 1% on 4 KB requests for the XT-CF miniport, ~5% on the T130B. Nothing here is
worth doing before the Mach8/DirectDraw work; ranked list below.

## Ranked candidates

| # | driver | candidate | lever | likely gain per request | confidence |
|---|---|---|---|---|---|
| 1 | T130B (`T130.MPD`, vendor) | Register traffic per 128-byte block: ~8.6 accesses (~49 us) against ~489 us of data, ~10% of bus time. Cut the block-ready and handshake polls | L3 (+L1) | up to ~5% of throughput if half the polls go | low: binary patch, no source |
| 2 | swapcomp (via the XT-CF miniport) | `SwapComp_Hook` sends each page to PAGEFILE separately, so every compressed page pays the miniport's fixed cost (below) | L3 (fewer, larger requests) | ~0.4 ms per extra request on reads (first DRQ 232-313 us + ~12 accesses), more on writes | uncertain: depends on pages per request; the SWAPCNT counters would say |
| 3 | xtide_mpd write path | Complete the SRB when the last sector is in and defer the BSY wait to the next command's `WaitNotBusy` | L3 | up to the commit time, 0.8 to ~110 ms per write request | risky: loses the ERR status of that write, and FLUSH / SYNCHRONIZE CACHE / SHUTDOWN (success without touching the card, `xsi_ok`) would have to wait. Owner decision |
| 4 | xtide_mpd | Skip the DRV/HEAD write and its BSY wait when unchanged | L2 | 1 write + >=1 read = ~12 us (0.08% of 4 KB) | uncertain, see below |
| 5 | xtide_mpd | Group `XTIDE_p*` / `XTIDE_o*` into one data line; align poll loops | L1 | a few us | low; a poll pass is 5.55 us against 5.70 us per raw access |

## xtide_mpd (XT-CF miniport, shipped)

Source: `drivers/xtide_mpd/src/XTIDEMP.ASM` (SCSIPORT glue) and `XTIDETR.ASM` (transport). All assembler, no C.

Per request: `XtStartIo` (XTIDEMP.ASM:645) -> `XTIDE_XferRun` (XTIDETR.ASM:1654) -> `XTIDE_ReadSectors` (:1252) /
`XTIDE_WriteSectors` (:1292) -> `XTIDE_ProgramTaskfile` (:1143).

| step | port accesses |
|---|---|
| `WaitNotBusy` before programming (:1146) | 1+ read (alt status) |
| DRV/HEAD write (:1203), `WaitNotBusy` (:1208) | 1 write, 1+ read |
| SecCnt, SecNum, CylLo, CylHi, Cmd (:1210-1227) | 5 writes |
| per sector: `WaitDrq` (:1268 / :1325) | 1+ read |
| per sector: `rep insw` / `rep outsw` (:879, :1431) | 256 word accesses, already the fast form |
| write only: commit poll (:1341) or `WaitNotBusy` (:1357) | 1+ read, paced |

Fixed cost per command: ~9 accesses = ~50 us. A 4 KB read is 8 sectors = 15.6 ms of data, so the fixed cost is
0.3%; a one-sector read is 2.0 ms against ~50 us plus the card's 232-313 us first-DRQ latency.

Already done: word-wide transfers (:850-879, :1414-1431); polls paced with a register-only delay (runs from
L1, no bus cycle) after 32 flat-out polls; `XT_WAIT_FIRST` spins from L1 through the measured latency before
the first poll; `XT_YIELD_COMMIT` overlaps a long write commit with Windows (lever 3 for the CPU, not for the
request's latency).

Lever 2 (candidate 4): the taskfile registers cannot be shadowed wholesale. ATA devices update Sector Count and
the address registers as a command completes, so a skipped SecCnt/LBA write can leave the device's value, not
ours, in the register: **do not skip those**. DRV/HEAD is the exception: for a CF card below 128 GB the LBA
bits 24-27 are zero, so the value is a constant (`E0h` | unit) and only the unit select matters. Skipping it
when the same unit was selected last, and the BSY wait after it, is legal ATA. Not verified that this CF card
tolerates it; the XTIDE Universal BIOS rewrites it every command. The gain does not justify the risk unless
candidate 2 shows many small requests.

Lever 3: the protocol forbids the narrow-looking waits. `WaitNotBusy` before the taskfile write is required
(registers are undefined while BSY is set) and `WaitDrq` must see BSY clear before DRQ means anything. The
sector-2-onward `WaitDrq` is one read each (0.3% of a sector); keep it.

Lever 1: no C. Between accesses sit `mov edx,[mem]` / `mov eax,[mem]` pairs from scattered `.DATA`
(`XTIDE_pSecCnt` ... `XTIDE_oCylHi`); each is refetched after every access. Packing them into one aligned
16-byte block might save a few line fills of a ~50 us command. Uncertain, probably under 5 us.

How to measure: time 1000 reads of one sector and of 8 sectors at fixed LBAs with the PIT method (techniques 109
and 128) before and after; the one-sector case shows the fixed cost best. Prove the patched `.MPD` loaded
(technique 74) first. Candidate 2 needs the SWAPCNT counters (pages per request) before any code.

## trantor_t130b (shipped, vendor binary)

`drivers/trantor_t130b/T130.MPD` is Adaptec's file, unaltered (README). No source; a change is a binary patch
like `drivers/imation_ls120/patch_*.py`. Disassembly notes: `docs/t130_mpd_review_2026_09_26.md`.

Counts from the 86Box bed (same doc): per 128-byte block ~2.3 bus-status reads (`base+0Ch`, handshakes), ~2.2
card-status reads (`base+0`, block-ready spin) and ~4 others, against 128 bytes of word I/O at 3.82 us/B.
Register traffic is ~10% of bus time (1.21 s of 13.2 s). The 5380 handshake and the card's block-ready bit
cannot be skipped, but the real-card count is higher than the emulator's (the emulated card never makes the
driver wait), so the real fraction is unknown. Candidates: pace the block-ready spin as XT-CF does (check, then
register-only delay; issue #41), and read bus status once per phase change rather than per byte. Measure with
real-card access counts; the emulator has no bus hold time (#43). Idle traffic is ~790 accesses per 2 s (0.2% of
the bus plus an L1 flush each), likely CD-ROM media polling: unconfirmed.

## swapcomp, nmiwatch, shadowram

- **swapcomp** (`drivers/swapcomp/src/SWAPCOMP.ASM`): no port access of its own. LZ4 is register/memory work
  on a 4 KB bounce buffer. `SwapComp_Hook` (:170) splits every request into one page per call to PAGEFILE
  (:195-205), so each compressed page costs one full miniport command; see candidate 2.
- **nmiwatch**: one read of port 62h per second plus a counter update in the NMI path. Nothing to gain.
- **shadowram**: runs once at `Sys_Critical_Init`; no runtime path.

## Not our code

- **3c509b**: `drivers/3c509b/3c509.asm` is Crynwr's DOS packet driver; Windows 95 uses `ELNK3.VXD`, a
  Microsoft binary we only patch (`patch_elnk3_nomcast.py`, the IRQ 9 patch). No source; not audited.
- **xtide_cdrom**: `xtidecd.asm` is a DOS ATAPI driver (sava/lpproj, OBattler's XT-IDE port), kept as a
  reference (`drivers/xtide_cdrom/README.md`), not shipped; nothing runs it on the card. If a Win9x ATAPI path
  is built on it, the same limits hold: BSY clear before the 12-byte packet, DRQ before each burst, and the
  8-bit latch protocol costs two accesses per word.

## Not proposed

- 32-bit string I/O or a data aperture: ruled out (stride 2 decodes A1; no card has an aperture).
- Skipping BSY/DRQ checks, or shadowing SecCnt/LBA registers: the ATA protocol and the device forbid it.
- A faster XT-CF poll rate: the paced loop exists to keep the bus free.
