# FLUSHP 1 with the BL3's ASNP and CRLD bits cleared, 2026-11-14

Real 5160, DOS, `tools/flushprobe/FLUSHA.BAT` run by the owner (SW1 as fitted).
`FA_BASE.TXT`: registers as boot leaves them (`1000h:0=92`, `1000h:1=8C`).
`FA_ASNP.TXT`: `1000h:0=82` (ASNP, ADS# snooping, off).
`FA_CRLD.TXT`: meant `1000h:1=84` (CRLD off); the write does not take, see below.
Columns and test letters as in `../2026-11-12_flushprobe/README.md`.

Second session, COMrade (one copy), same night: `FB_ASNP.TXT` is FLUSHP 1 with `1000h:0` read back
as **82, ASNP disabled** (CTCHIP34 IBM486 with no write argument, output to a file), then restored
and read back as 92. Same table as the base run: **ASNP does not cause the flush.**

CRLD was NOT tested: a write to `1000h:1` (8C to 84) reads back unchanged (8C), in a batch file
verified intact. The byte appears locked after `CPUSET.BAT` sets it at boot (IKEN in the same byte
is "settable once per reset"). `FA_CRLD.TXT` therefore ran with CRLD on. Testing CRLD needs the
value changed inside CPUSET.BAT at boot.

First run: all three tables match to within a few PIT counts (IN 1 KB 84 us, 8 KB 638 us, OUT 81 us,
8-IN burst 124 us). The owner did not see the register screens, hence the second session.
