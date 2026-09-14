# Next session - 2026-09-15

Supersedes the earlier 09-15 draft, which covered only the presence gate.
`next_session_2026_09_14.md` is now history: its truncation work is answered
below and its `/di` / `/w0` plan is overtaken.

## Start here

**The LS-120 reads and writes correctly from DOS on our own code path, with the
vendor driver removed.** Proven on real media. And the measurement that matters:

> **A single ATAPI data burst on this drive is 3584 bytes - 7 sectors.** A
> 4096-byte WRITE(10) is answered with a 3584-byte burst, the transfer is left
> mid-flight with DRQ still asserted, and the next command dies ABORTED COMMAND.

**`LS_MAX_XFER = 4096` is one sector over that, and neither the probe nor
`LS_PacketCommand` loops bursts.** So the driver would move 3584 bytes of a
4096-byte write, leave 512 behind, and report success. That is silent data loss
and it is the first thing to fix.

## Machine state

- CF in the 5160. **`CONFIG.SYS` has both `SD120PPD` lines REM'd** - `D:` does
  not exist. Restore with `copy C:\CONFIG.B4 C:\CONFIG.SYS` (backup verified,
  crc `18043ce5`, 397 bytes). `INBRDPC.SYS` untouched.
- The NOS disk is still in the drive and **still has its original filesystem** -
  `EB 3E 90 MSWIN4.0`, 512 b/sector, 4 sectors/cluster, 2 FATs of 241, 32
  sectors/track, 8 heads. `D:\FC.EXE` and the session's test files are gone from
  DOS's view only because the driver is unloaded, not because anything erased
  them.
- LBA 100000 holds a 00..FF ramp from the media write test.
- Driver binary on the card is still `220be39e`. The presence-gate build
  `9d89bcc3` (commit `560e9fd`) is **still not deployed and still never run**.
- Repo clean, nothing pushed.

## Proven today, on hardware

Vendor driver unloaded for all of it. Captures in
`docs/captures/2026-09-14_ls120/`.

| | evidence |
|---|---|
| register reads | status `50` (DRDY\|DSC), ATAPI signature |
| register writes | SRST pulses, drive goes BSY, raises unit attention |
| **block read** | LBA 0 = real FAT boot sector, `MSWIN4.0` … `WINBOOT SYS` `55 AA` |
| **block write, drive buffer** | WRITE BUFFER / READ BUFFER 512 bytes byte-exact, correct order |
| **block write, MEDIA** | WRITE(10) → READ(10) at LBA 100000, 512 bytes byte-exact |
| **it is on the platter** | re-read after SRST, buffer poisoned to `EE`, no write that run - same pattern |
| multi-sector | 7 sectors (3584) clean both ways; 8 sectors (4096) short-bursts |
| unit attention | two queued, ASC `29` then `28`, one cleared per REQUEST SENSE |

**All of it is SPP/nibble. ECP has not been exercised at all.**

## What the driver must change, with numbers

1. **Transfer ceiling.** Either cap at **3584** (7 sectors) or implement burst
   looping. Capping is one constant; looping is correct and is what `pf.c` does.
   Until one of them lands, any write over 3584 bytes loses data silently.
2. **The reset settle is a LONG wait and must not share a budget with a
   per-command poll.** Measured: `--spin 2000` (~0.29 s) aborts every command
   after SRST, deterministically, byte-identical across runs. `0xFFFF` (~2.3 s)
   works first time. So `LS_SPIN_RESET = 2000` (~92 ms), shipped this morning,
   cannot work - and `lt_coldstart` re-pulses SRST every retry, restarting the
   reset forever. `DESIGN.md` I3 and change 5.
3. **Drain the sense until it comes back clean**, not a fixed count. The drive
   queued two unit attentions and reads only worked once both were gone.
4. **Calibrate per transport** (`DESIGN.md` §5.2) - and note 3584 is an SPP
   number. ECP may differ; nothing says they share it.

## The format: attempted, void, and how to do it properly

FORMAT UNIT (CDB `04 00 …`, FmtData=0, no parameter list) was issued and the
packet ran to completion. **The result is void and the disk is unchanged.**

Two reasons, both mine:

- **`MARK` sat on the FORMAT UNIT result slot** (`SLOTS+0xA0` = `1EA0`), so its
  status and error were overwritten before they could be read. Fixed: `MARK` is
  `1F00` and the layout check now tests it against `SLOTS`.
- **The check run reset the drive.** Every probe starts with an SRST, and SRST
  aborts an in-progress format. The check ran 90 s after the fire - on a format
  the owner expects to take ~10 minutes, that run would have killed it.

`--noreset` now exists for exactly this. The correct sequence:

1. `--formatunit` to fire it, and **read the slot** to confirm the drive accepted
   the command rather than rejecting it.
2. Leave it alone for ~10 minutes. Do not run anything that resets.
3. `--noreset` poll runs to watch BSY clear.
4. Then an ordinary probe to read LBA 0 and see a blank surface.

⚠ If the drive rejects FmtData=0, the parameter-list form needs sourcing from a
primary reference before being tried on real media - do not guess a defect-list
header.

## `FORMAT.COM` vs FORMAT UNIT

Different things, and the distinction decided the plan:

- **FORMAT UNIT** is a raw ATAPI command. No drive letter, no mount, no bulk
  transfer - the drive formats its own surface internally, so transport speed
  barely matters. The probe can issue it.
- **`FORMAT D:`** is a filesystem tool and needs a block device. That means the
  vendor driver, or ours once it loads under Windows.

A full "clean bill of health" is really both: FORMAT UNIT for the surface, then
filesystem structures, which need a block driver.

⚠ An earlier claim in this session that a format would take ~47 minutes was
**wrong twice over** - it assumed nibble when the card is ECP-capable, and it
assumed the host streams every sector when FORMAT UNIT is drive-side. Roughly
10 minutes, drive-bound, is the right expectation.

## The probe had four bugs, all of the same family

`gen_rw_probe.py` had never run successfully - there is no `RW.OUT` beside the
09-11 INQUIRY captures. Four defects, every one of them **something that fails
by silently not existing**:

1. `jcxz BRE` is +158 from the jump; a short jump reaches +127. DEBUG prints an
   error, **re-prompts at the same address and carries on without the
   instruction**, so the guard its own comment calls load-bearing was absent.
   With `cx=0` the read loop ran 65536 times over the probe's own code. Three
   wedges, three reboots.
2. `MARK` at `0580` and slots at `0600-067F` sat inside a main block that had
   grown to `0716` - the probe wrote results into its own instruction stream.
3. Adding a CDB pushed the media READ(10) onto `1D00`, which was `SLOTS`.
4. `MARK` at `1EA0` landed on the FORMAT UNIT slot.

The layout check now measures every short jump, and tests data against code,
against the CDB table, and against the other data regions. **Each guard was
verified to fail on the bug it was written for** before being trusted.

## Next, in order

1. **Fix the transfer ceiling** - the data-loss bug. Cap at 3584 or loop bursts.
2. **Fix the reset settle** - it needs seconds, as a state, pulsed once.
3. **Repeat the whole DOS proof on ECP.** It is the untested half and the owner
   has asked for both. `LS_NegotiateEcp` exists in the driver; the probe has no
   ECP path at all and needs one.
4. **Format properly**, per the sequence above - between the SPP and ECP proofs,
   so ECP verifies against a known-clean surface.
5. Then Windows: deploy `9d89bcc3`, boot with the vendor still REM'd, and see
   whether the driver loads now that nothing else owns `0x378`.
