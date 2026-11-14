# Fable review, Part 3: the XT-IDE miniport (`XTIDEMP.MPD`)

Brief: `docs/fable_driver_review_brief.md`. Branch `worktree-agent-a5f2e4562c5d10217`, based on
master `e274262`. Reviewed: `drivers/xtide_mpd/src/XTIDEMP.ASM` and `XTIDETR.ASM` as built with the
card's flags, `-PollBackoff -FastXfer -WaitFirst -YieldCommit -TightTaskfile -SkipDrvHd`, plus
`XTIDEMP.INF` and `build.ps1`. Line numbers are this branch's head.

Nothing here has run. Every commit assembles and links with the DDK toolchain under the card's
flags; none has been booted, in a bed or on the 5160. Each needs `WRTEST` and a boot and shutdown
before it goes near the card (`docs/xtcf_taskfile_switches_2026_11_04.md`, stages 1 and 2).

## Finding zero: what the card runs

A build of master `e274262` with the card's flags gives code hash `45fe8c64`, the hash
`docs/5160_tuned_config.md` records for the card's `XTBOTH` (built here in `build\base`; other
folder names gave other hashes for the same source). So master is the card's source. `dist/xtide_mpd/XTIDEMP.MPD`
is the older `db88f64d` build; the card's build is in neither `dist/` nor `build_ledger.tsv`.

## Findings, ranked

### 1. `-SkipDrvHd` trusts a register other code can write (data safety) - part fixed, part open

`XTIDETR.ASM:1005-1041`. The switch skips the DRV/HEAD write when the value equals the last one
this driver wrote. If the register holds anything else, the drive decodes the next command's LBA
bytes with the wrong LBA bit, unit or high bits: a read or a write of the wrong sector, with no
error. Three ways that can happen were in the code:

| Case | Status |
|---|---|
| CHS drive: the head is in DRV/HEAD, and a drive that reports the last sector transferred (ATA-1 to ATA-3 do) advances it when a command crosses a track | fixed, `4162f99`: never skip in CHS |
| LBA drive: a command that crosses an LBA 24-27 group (every 8 GB) leaves the far group in the register | fixed, `4162f99`: the value is forgotten after such a command |
| The probe's read of LBA 0 left `E0h` as the known value, but the BIOS serves the disk in real mode from then until the first SRB and writes DRV/HEAD itself (`A0h+head` if XUB addresses this drive in CHS) | fixed, `c9142ea`: the value is forgotten when the probe ends |
| After takeover: any INT 13h call Windows reflects to the XUB (a reset, `AH=00h`, makes XUB issue SRST, and SRST clears the register), or a DOS-box program writing `30Ch` directly (the ports are not trapped) | **open** |

The first two do not apply to this card (LBA, and Windows 95 does not reach past 8 GB). The third
did apply, and the card has run since 11-08 with `WRTEST` clean, so XUB evidently leaves `E0h`
here; that is inference from a run, not a reading.

The open case is why this is ranked first. The switch's measured gain is nil (`XTSECT` 55.79 s
before and after, `5160_tuned_config.md`), and its cost is an assumption about every other writer
of one register on a boot disk. **Recommendation: build the card's driver without `-SkipDrvHd`.**
Owner's decision; nothing changed here.

Probe, if the switch is to stay (read-only, in a DOS box under Windows with the miniport loaded):
`DEBUG`, `i 30c` (expect `E0`); assemble and run `mov ah,0 / mov dl,80 / int 13 / int 3`; `i 30c`
again. Any value other than `E0` means a real-mode call reached the card and the next skipped
select would have been wrong. Also `i 30c` at a plain DOS prompt after a `DIR`: `E0` means XUB
addresses the card in LBA, `A0`-`AF` means CHS.

### 2. A write fault without ERR was reported as success (data safety) - fixed, `150163b`

`XTIDETR.ASM:1228` and `596-610`. The check after a write's commit tested `ERR` only. Status bit 5
(write fault; DF from ATA-4) can be set alone when the drive could not store the data. Both
completion paths (inline and `XTIDE_CommitPoll` for the timer) now fail the SRB on either bit. This
card's idle status is `50h`, so nothing changes for a healthy write. Not tested: no way to provoke
a write fault.

### 3. The probe claimed a disk whose sector 0 it could not read (contract) - fixed, `3ce02a9`

`XTIDETR.ASM:1624-1625`, `1678`. `XTIDE_Probe` returned found as soon as IDENTIFY validated, even
when its own read of LBA 0 then failed (fail codes 7, 8, 9). Windows would take the boot volume
from the BIOS onto a read path already known not to work. It now reports not-found, and the disk
stays in compatibility mode. A missing `55AA` still claims the disk (a blank disk is legal).

### 4. The CDB's block count is not checked against `DataTransferLength` (contract) - open

`XTIDEMP.ASM:795-821`. The transfer length is the CDB count times 512; `Srb.DataTransferLength` is
never read. A request whose buffer is shorter than its CDB says makes a read write past the buffer
in ring 0, and a write send whatever follows the buffer to the disk. DiskTSD is not expected to
send one; an ASPI or pass-through client can. The fix is two instructions (refuse with
`SRB_STATUS_INVALID_REQUEST` when count x 512 > `DataTransferLength`), but if Windows ever sends a
short length in normal use, the fix refuses boot-disk I/O, so it wants one observation first.
Probe: a diagnostic build that counts SRBs where the two differ (one `dd` in the device extension,
read with a debugger or returned in a vendor INQUIRY byte), run through a boot, a copy and a
shutdown in the bed. Zero means the check is safe to add.

### 5. Every wait and timeout is a loop count, calibrated once for one CPU state (uncertain) - open

`XTIDETR.ASM:131-145`. `XT_POLL_DELAY`, `XT_SPIN_SLOW`, `XT_RD_FIRST_WAIT`, `XT_WR_FIRST_WAIT`,
`XT_COMMIT_WAIT` and `XT_COMMIT_POLLS` all assume 0.22 us per `dec/jnz` pass, measured 2026-09-10.
The cache range fix is dated two days later (memory note `cpu_cache_cmlr_fixed_2026_09_12`), and
`docs/xtcf_measured_wait.md` item 3 (calibrate against the PIT at init) was not built. If a pass
is now shorter, the first waits end early (cost: polls, not data) and the 1.1 s busy timeout
shrinks in proportion; a timeout that undercuts a slow flash commit turns it into a write error
and a retry. If the cache is off (`CPUOFF.BAT`), every wait stretches instead. Probe: time 100,000
passes of the same loop against PIT channel 0 on the card as configured today (an XTLAT-style
`.COM`), from a locked 32-bit segment if possible. If it is not 0.22 us, rescale or calibrate at
`XtFindAdapter`.

### 6. No ATA cache flush at FLUSH, SHUTDOWN or SYNCHRONIZE CACHE (data safety off this card) - open

`XTIDEMP.ASM:660-663`, `722`. All three succeed without touching the drive. Correct for this CF
card, which has no volatile write cache, and with `-YieldCommit` a write's SRB completes only
after its commit. On a stride-1 XT-IDE with a real drive whose write cache is on, shutdown can
power off with data unwritten. Fix when a tester has such a drive: `FLUSH CACHE` (`E7h`) when
IDENTIFY word 83 bit 12 is set. Not changed: no hardware here executes it.

### 7. No recovery from a wedged drive (contract) - open

`XTIDEMP.ASM:603-632`. `XtResetBus` never touches the card, and nothing issues SRST. A drive stuck
busy fails each request after `XT_SPIN_SLOW` (about 1.1 s of CPU held, per attempt) until it
frees itself. An error also completes as `SRB_STATUS_ERROR` with no `ScsiStatus` and no sense
data, and REQUEST SENSE is refused. Windows retries and reports the failure, so data is not at
risk; the machine crawls instead of hanging. Left alone: a reset written blind on a boot disk is
worse than none. Probe: the bed with the fork's `IDE_COMMIT_US` diagnostic set above the timeout,
to see what DiskTSD does with the error.

### 8. The latch transport is tried on stride 2, where it cannot exist (low) - open

`XTIDETR.ASM:1600-1608`, `693`. If IDENTIFY fails as PIO8, the probe repeats it through the
high-byte latch at `base+8`. At stride 2 that port is Cylinder Low, so the second attempt reads
256 of the 512 data bytes and leaves the drive holding DRQ, on a disk the BIOS is about to use
again. Only reached when the first attempt has already failed. Fix: skip the latch attempt when
the stride is 2. Not changed: no failing case to test it on.

### 9. `PORT=` accepts any address (low) - open

`XTIDEMP.ASM:504-582`. Any one to four hex digits are taken as the base. The probe then reads
`base+7`, `+0Eh`, `+1Ch`, and if two of them agree it writes five registers and `ECh`. `PORT=20`
would address the 8259; a base above `3FFh` aliases at XT 10-bit decode (`720h` is the 3C509B at
`320h`). Fix: refuse a base below `100h`, above `3E0h` or not a multiple of 20h, and fall through
to the node. Not changed: it narrows what an installer may type, which is the owner's call.

### 10. Dead state from the retired port driver was locked in RAM (RAM) - fixed, `3e21e86`

The transport still assembled the IOS port driver's DCB tables, request marker, delay loop, a
512-byte MBR buffer and, in every build, the write self-test with its two 512-byte buffers. None
was reachable with the card's flags. Removed; the self-test is now assembled only by `-WriteTest`.
The card build goes from 11,776 to 9,216 bytes (`.data` C25h to 551h, `.text` F46h to E4Eh).
Whether that frees a 4 KB page depends on how IOS maps the image; not measured.

Further, not done: `XTIDE_IdBuf` and `XTIDE_SecBuf` (`XTIDETR.ASM:244`, `297`) are used only by
the probe and never together; sharing one saves 512 bytes more.

### 11. Comments describing the port driver (canary) - fixed in part, `2e6d731`

Corrected: the transport's header ("carried across unchanged", no longer true), the probe's header
(`Port_initialize`, `PortDDB`, the AEP pointer), the `NEVER fail AEP_INITIALIZE` block and the two
`MPD:` notes contradicting it, the boot-log delay channel, "phase 2" plans already built, the
emulator debug port `0E0h`, `XT_ALLOW_SLAVE` and `XTIDEMP.LNK` (neither exists). Also removed: two
opcode equates listed as "answered" that the dispatch refuses (REQUEST SENSE, MODE SENSE 10), and
a load of `DataTransferLength` whose result was discarded.

Left for the owner, alert only:
- `XTIDEMP.ASM:297-301` and `build.ps1` (`-Base`) say "the INF now offers one range, which is the
  real fix". `XTIDEMP.INF` says that change was reverted on 2026-09-05 and offers four.
- `build.ps1:69,73` and `docs/xtcf_taskfile_switches_2026_11_04.md` cite
  `docs/driver_review_2026_11_04.md`, which is not on master (it is on
  `claude/driver-optimisation-review`).
- Dated measurements and incident accounts remain in both files (`XTIDETR.ASM:35`, `64`, `111`,
  `460`, `665`, `758`, `798`, `1286`; `XTIDEMP.ASM:119`, `240`, `297`, `316`, and the
  `XT_CLAIM_RANGE` block). CLAUDE.md wants the constraint kept and the date and story moved to
  `docs/`. Most carry a live constraint, so each needs a rewrite, not a deletion.
- `XTIDETR.ASM:111-132` quotes 5.55 us per I/O; the project's figure since 09-20 is 5.695.
- `XTIDEMP.ASM:136`: `XT_COMMIT_WAKES` is "1.4 s at the fastest tick". On this card
  (`MinTimeSlice=54`) it is 5.5 s. Whether that is under DiskTSD's SRB timeout was not checked.
- `drivers/xtide_mpd/README.md` describes the 09-10 build as current.

## Checked, nothing found

1. **Contract.** `HW_INITIALIZATION_DATA` and `PORT_CONFIGURATION_INFORMATION` fields against the
   DDK headers; NULL `HwInterrupt` with level 0; one request at a time (no `NextRequest` while a
   commit is pending, so the transport's globals have one owner); every `XtStartIo` exit completes
   the SRB or arms the timer; `XtResetBus` and `XtTimer` cannot both complete one SRB; absent
   target and LUN return selection timeout; zero-length and past-end requests; READ(6) count 0 =
   256; READ CAPACITY in LBA and CHS; short INQUIRY buffers. `-TightTaskfile`: the five values,
   ports and order match the plain path, and EBX, ESI, EDI, EBP are restored.
2. **DMA reach.** No DMA: `DmaChannel` -1, `Master` FALSE, `MapBuffers` TRUE, every byte by
   IN/OUT through a virtual address. The skill was not loaded; there is no buffer to audit.
3. **Port aliasing.** At base 300h the driver touches 300h-31Ch only (word access at 300h/301h,
   latch 308h only on stride 1). Nothing below 100h, nothing in the NIC's 320h or the T130B's
   340h. See finding 9 for a mistyped base.
4. **Interrupts and reentrancy.** No interrupt is connected. The only interrupt-time code is
   `XtTimer`: one status read, then complete or re-arm. Not settled: whether SCSIPORT calls
   `XtStartIo` from inside `NextRequest` at timer time, which would run a whole transfer there
   (see "not checked").
5. **Locked RAM and bus time.** A one-chunk read is one status read, four or five taskfile OUTs,
   then per sector one status read and 256 word reads: 1,956 us of 512-byte data against about
   6 us of status. Nothing left to remove per sector; the data phase is the cost and it is at the
   measured word rate. READ MULTIPLE is already ruled out (IDENTIFY word 47 = 1). RAM: finding 10.
6. **Comments.** Finding 11.

## Not checked

- Anything at run time: no bed, no card, by instruction.
- Win95 SCSIPORT's side of the contract (what it does with `SRB_STATUS_ERROR` without sense, the
  SRB timeout value, whether `NextRequest` re-enters `XtStartIo`). `SCSIPORT_reference.PDR` was
  not disassembled for this.
- XUB 2.1.2's source, for what it writes to DRV/HEAD and when (finding 1's open case).
- The stride-1 and latch paths beyond reading them; `-PhysBreaks`, `-ClaimRange`, `-RealModeInit`,
  `-Stride`, `-Base` builds beyond confirming the default and `-WriteTest` builds still link.
- `drivers/xtide_mpd/test/` and the INF's install behaviour.

## Commits

| Commit | Subject |
|---|---|
| `4162f99` | XT-CF: skip DRV/HEAD only while its value is known |
| `150163b` | XT-CF: treat a write fault as a failed write |
| `3ce02a9` | XT-CF: decline a drive whose sector 0 will not read |
| `3e21e86` | XT-CF: remove the port driver's dead state |
| `2e6d731` | XT-CF: correct comments that describe the port driver |
| `c9142ea` | XT-CF: forget DRV/HEAD when the probe ends |

Head with the card's flags: 9,216 bytes. No code hash is given: it moved with the output folder
name (`90e64379` in `build\final`, `af20373a` in `build\both`). `build_ledger.tsv` was not
updated; rebuild at the usual path for a ledger row.
