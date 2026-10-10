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

## Writes, measured on the 5160 (10-10)

`docs/captures/2026-10-10_xtlat/`: ~3 ms to the first DRQ, ~1.1 ms at each 16 KB boundary, then **~110 ms**
of commit after the last sector (30 of 32 commands). So the answer to item 5 is: yield on writes. A 110 ms
wait dwarfs any tick - even a 55 ms tick wakes the driver at most half a commit late - and today that time
is the CPU frozen and the bus full of status reads. MinTimeSlice still matters: the finer the tick, the
smaller the overshoot, so choose T1's value with the write path in mind.

## Who gets the freed bus

While the driver holds the CPU (synchronous, as today), the bus time a poll no longer takes is free only
for DMA - sound, floppy, refresh. A read of 8 sectors is ~7.7 ms of PIO and ~0.3 ms of wait, so the read
gain is ~1-3%, mostly noticing the card sooner. The larger gain needs the CPU released during a wait, so
it can draw to the Mach8 or run code meanwhile; that pays only for waits of milliseconds - writes, if
XTWLAT shows them. Step 1: wait on the measured time. Step 2, if writes are long: yield during them.

## How to tell it worked

Polls per command (an XTLAT-style count on the card, or a bed counter), unchanged or better
throughput, and the #45 CPU loop for whatever Windows gains back. Bed first, then the card.
