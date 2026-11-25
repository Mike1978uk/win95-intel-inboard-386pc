# SBDS stage 5 (display handshake) on the bed (2026-11-25)

SBDS md5 `ffa902a5` (trace) with `SBDS_IoCtl` taking the Mach8 hot data's address; M8DD32 built from
current source plus `m8dd_sbds_hand` in `DriverInit` (md5 `da721b7d`; the bed's older DLL kept as
`C:\WINDOWS\SYSTEM\M8DD32.OLD`, md5 `c0777c12`).

- First try blue-screened in Doom: fatal exception 0E at 0028:C0381634 in VXD SBDS(02)+0. The handler had
  been placed in the init code segment, which is discarded after boot. Moved to locked code; fixed.
- Second run, no crash. Stats: ticks 1,949, underruns 5 (7 last run), voices refused 0, sleeps 11 / wakes
  10, most voices 2, lead 140 ms now and max.
- **Ticks deferred 0, ticks mixed while busy 0: the handshake never saw the display busy.** Most likely
  SBDS never got the address (CreateFile("\.\SBDS") failing, DriverInit running before SBDS is ready, or
  hot = 0 then), not that H_BUSY was never set in 1,949 ticks. Not yet proven: add a "hot set" count and
  an IoCtl call count to the stats, and check what CreateFile returns in M8DD32 (trace to C:\M8TRACE.TXT).
