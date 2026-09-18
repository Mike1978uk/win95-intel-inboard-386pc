# The LS-120's genuine responses, by drive and media state

MEASURED on the real 5160, 2026-09-18, over DOS COMrade with `SD120PPD.SYS`
REM'd out of `CONFIG.SYS`. Every row here is a capture in
`docs/captures/2026-09-18_media/`, not an inference. Where a row is derived
rather than measured it says so.

Probes are `INQ9.SCR` and four derivatives, each differing from it in exactly
four lines (CDB, the byte count into register `1Ch`, the data-in length, the
dump range). `INQ9.SCR` itself re-ran byte-identical to its 2026-09-11 capture
on the same day, so the harness is a control, not a variable.

## ⛔ Read this first: the harness raises UNIT ATTENTION and hides the answer

Every non-exempt command came back **status `51h`, error `64h`** - ERR set,
sense key 6 - until a REQUEST SENSE cleared the condition:

| when | sense key | ASC | meaning |
|---|---|---|---|
| after SRST | 6 | `29h` | POWER ON, RESET OR BUS DEVICE RESET OCCURRED |
| after SRST, **second** read | 6 | `28h` | NOT READY TO READY CHANGE, MEDIUM MAY HAVE CHANGED |
| after an eject/insert | 6 | `28h` | same |
| once drained, steady state | **0** | `00h` | NO SENSE - and it stays clean across runs |

⚠ **An SRST queues MORE THAN ONE condition**, and each REQUEST SENSE pops
exactly one: `29h` first, then `28h`. A single sense read does not drain the
drive. Measured 2026-09-18 (`Q2A`, `N2`) and consistent with the 2026-09-11
finding already in the record. An earlier draft of this file said the drive
raises `28h` "on essentially every reconnect" - **that was wrong**; `N2` shows
a clean key 0 on a plain repeat with nothing changed.

**INQUIRY is exempt from unit attention, which is the only reason `INQ9.SCR`
ever worked.** READ CAPACITY and MODE SENSE are not, and both aborted.

So the probes come in two forms: the `*.SCR` originals, which pulse SRST, and
`MSENSE2 / RDCAP2 / RQSENSE2`, which do not. **Run `RQSENSE2` first to clear
the condition, then the query.** That ordering is a prerequisite, not a
convenience - a query run without it aborts and that is the probe's fault, not
the drive's (technique 110).

## MODE SENSE(10), page 3Fh, allocation length 8 - the mode parameter header

| media state | bytes at 0700 | WP bit | capture |
|---|---|---|---|
| **write-enabled** | `00 66 31 00 00 00 00 00` | **0** | `Q3_msense_wrenabled_clean.OUT` |
| **write-protected** | `00 66 31 80 00 00 00 00` | **1** | `W3_msense_writeprotected.OUT` |

```
byte 0-1  mode data length      0066h = 102
byte 2    medium type           31h
byte 3    device-specific parameter   bit 7 = WP   <- the ONLY byte that differs
byte 4-5  reserved
byte 6-7  block descriptor length     0000h
```

**The drive reports write-protect correctly and honestly**, and nothing else in
the header moves. So a "write protected" message from Windows while the tab is
write-enabled does not come from the drive.

## THE MATRIX - all three media states, measured

| state | MODE SENSE header at 0700 | medium type | WP | READ CAPACITY | sense after |
|---|---|---|---|---|---|
| **write-enabled** | `00 66 31 00 00 00 00 00` | `31h` | **0** | ok | key 0 NO SENSE |
| **write-protected** | `00 66 31 80 00 00 00 00` | `31h` | **1** | ok | key 0 NO SENSE |
| **no media** | `00 66 00 80 00 00 00 00` | **`00h`** | **1** | **ERR** `51h`/`24h` | key **2**, ASC **`3Ah`** MEDIUM NOT PRESENT |

### ⛔ WP=1 does NOT mean "write protected"

**An empty drive also reports WP=1.** The discriminator is the **medium type**
byte, not the WP bit:

| medium type | WP | means |
|---|---|---|
| `31h` | 0 | disk present, writable |
| `31h` | 1 | disk present, tab set |
| `00h` | 1 | **no disk** |

Any code that reads bit 7 of byte 3 and concludes "write protected" will call
an empty drive a protected one. Read byte 2 first.

### READ CAPACITY is the cleaner presence test

With no media it fails with error register `24h` - sense key **2 (NOT READY)**
plus ABRT - and the following REQUEST SENSE gives ASC `3Ah` MEDIUM NOT PRESENT.
That is unambiguous and needs no interpretation.

Capture files: `Q3_msense_wrenabled_clean.OUT`, `W3_msense_writeprotected.OUT`,
`E3_msense_nomedia.OUT`, `E4_rdcap_nomedia.OUT`,
`E5_rqsense_after_nomedia_rdcap.OUT`.

⚠ Ejecting the disk did **not** by itself produce a unit attention on the next
REQUEST SENSE (`E1` reads key 0). The NOT READY only surfaces on a command that
actually needs the medium. Do not rely on a sense poll to notice an eject.

## DRIVE state, as distinct from MEDIA state - measured 2026-09-18

Raw parallel-port registers, read directly with COMrade `io_in`, data lines
parked low first:

| drive | `0x378` data | `0x379` status | `0x37A` control | `0x77A` ECR |
|---|---|---|---|---|
| powered ON, bridge idle | `00` | `00` | `04` | `15` |
| **powered OFF, still plugged** | `00` | `00` | `04` | - |

**A raw port read cannot tell a switched-off drive from a present idle one.**

Run the probe instead and the difference is stark:

| | phase bytes at 0600 | meaning |
|---|---|---|
| powered ON | `80 00 00 08 08 02 24 00` | reaches DRQ, returns the INQUIRY data |
| **powered OFF** | `80 01 00 80 80 01 14 EB` | **status stuck at `80h` BSY, never clears** |

### ⛔ Two traps this exposes in our own code

1. **`FFh` is not the only absence.** `LS_SettleStep` declares `LS_ST_ABSENT`
   only on `FFh`. A powered-off drive reads `80h`, which we treat as "still
   coming out of reset", so we poll the **entire 6000-tick (6 s) settle budget**
   every boot before giving up. It does not hang - which is the point of the
   redesign - but the fast path never fires and the reason is wrong.

2. **The ATAPI signature is NOT a presence test.** Byte count reads `14 EB` with
   the drive switched off. `LS_BringUp` checks exactly that signature to decide
   the drive answered, and it would pass. **The only discriminator measured so
   far is BSY clearing.**

Capture: `PWROFF_inq9_drivepoweredoff.OUT`, against the powered-on control
`../2026-09-11_ls120/INQ9NOW.OUT`.

⚠ Not yet measured: **cable unplugged**. That needs the 5160 shut down first -
DB25 has no staggered ground pin and both boxes are separately mains-powered.

## INQUIRY - the control

`MATSHITA LS-120 COSM   04 0270`, phase bytes `80 00 00 08 08 02 24 00`,
byte-identical to the 2026-09-11 capture. Run it first in any session: if it
differs, the link or the drive has changed and nothing after it is evidence.

## What this means for the driver

Our miniport sets `AutoRequestSense = FALSE` and never writes `ScsiStatus`,
`SenseInfoBuffer` or `SRB_STATUS_AUTOSENSE_VALID` - `grep` finds no reference
to any of them. `LS_RequestSense` exists and is called once internally to clear
the condition, and the sense bytes are then discarded.

So the class driver receives a featureless `SRB_STATUS_ERROR` and cannot tell
apart:

- "medium may have changed, re-read it and retry" (`6 / 28h`) - routine, and
  queued two-deep after any reset
- "the medium is write protected" (`7 / 27h`)
- "the drive is broken"

For removable media the conservative fallback is to mark the volume read-only,
which is what the owner sees. It also explains the `04, 04, 01` retry pattern
in the bed: every blind retry costs a full command turnaround.

**The fix:** on ERR, set `ScsiStatus = 02h` (CHECK CONDITION), copy the sense
already fetched into `SenseInfoBuffer` bounded by `SenseInfoBufferLength`, and
OR `SRB_STATUS_AUTOSENSE_VALID` into the returned status.
