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
| **after a bridge reconnect** | **6** | **`28h`** | **NOT READY TO READY CHANGE, MEDIUM MAY HAVE CHANGED** |
| after an eject/insert | 6 | `28h` | same |

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

- "medium may have changed, re-read it and retry" (`6 / 28h`) - **routine, and
  this drive raises it constantly**
- "the medium is write protected" (`7 / 27h`)
- "the drive is broken"

For removable media the conservative fallback is to mark the volume read-only,
which is what the owner sees. It also explains the `04, 04, 01` retry pattern
in the bed: every blind retry costs a full command turnaround.

**The fix:** on ERR, set `ScsiStatus = 02h` (CHECK CONDITION), copy the sense
already fetched into `SenseInfoBuffer` bounded by `SenseInfoBufferLength`, and
OR `SRB_STATUS_AUTOSENSE_VALID` into the returned status.
