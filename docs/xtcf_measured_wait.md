# XT-CF: wait on what was measured, not on polls

Owner's idea, 2026-10-10. Every status poll is an 8-bit I/O read on the XT bus (~5.6 us) that moves no
data. Since the card's timing has been measured, the driver can stay off the bus until the card is
known to be ready, then check once. No existing driver is the reference: design for this bus.

## What the shipped driver does (`drivers/xtide_mpd/src/`)

- `XtStartIo` completes synchronously, inline. **No timer is involved**, so the Windows tick
  (MinTimeSlice, #45) does not pace the disk today.
- `XTIDE_WaitDrq` / `XTIDE_WaitNotBusy` (`XTIDETR.ASM`, `XT_POLL_BACKOFF`): 32 polls flat out (178 us
  of bus), then a poll every ~51 us with an L1-resident delay loop between them (no bus cycles).
- Measured result (`docs/captures/2026-10-10_xtlat/`): first sector ready at 232-313 us (median 238),
  so a read costs ~33 polls and finds the card ready up to ~57 us late. Every later sector and
  the final BSY check are ready at the first poll.

## Proposed: wait first, then look

1. **After a command:** a silent wait of ~220 us in an L1-resident loop, then polls a few us apart.
   Expected 1-3 polls instead of ~33, and the card is noticed sooner.
2. **Between sectors:** no wait. Go straight to the status read and the data (448/448 ready).
   Whether that one status read can be dropped is an open question; ATA expects ERR to be checked.
3. **Calibrate the loop at init against the PIT**, not a fixed count. The current
   `XT_POLL_DELAY` assumes 0.22 us per iteration at 83.5 MHz; the CPU clock is not a constant
   across machines.
4. **Optional, adaptive:** keep a running ready time per command type and wait for that.
5. **Writes:** not measured (X1 in `next_session_2026_10_11.md`). If a flash commit takes
   milliseconds, choose between a silent CPU wait (bus free, CPU held) and returning to Windows
   with `ScsiPortNotification(RequestTimerCall)` (CPU free too). **This is where the disk and the
   tick could meet:** check what resolution Win95 SCSIPORT gives a timer call. If it is the VTD
   tick, MinTimeSlice=54 makes a yield wake up to 55 ms late, so the two settings must be chosen
   together.

## How to tell it worked

Polls per command (an XTLAT-style count on the card, or a bed counter), unchanged or better
throughput, and the #45 CPU loop for whatever Windows gains back. Bed first, then the card.
