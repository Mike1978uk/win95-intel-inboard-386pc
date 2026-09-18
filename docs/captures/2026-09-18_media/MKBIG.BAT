@ECHO OFF
REM Build a ~9 MB test file on C: by doubling a known seed, so the bulk
REM round trip to the LS-120 has something substantial to carry.
REM 701,259 -> 1.4M -> 2.8M -> 5.6M, then + 2.8M + 0.7M = ~9.1 MB.
REM Seeded from a real file rather than zeros: a run of zeros can survive a
REM transport that drops bytes, so it cannot prove a copy is byte-exact.
COPY /B C:\WINDOWS\TWAIN.HLP C:\SEED.BIN
COPY /B C:\SEED.BIN+C:\SEED.BIN C:\B2.BIN
COPY /B C:\B2.BIN+C:\B2.BIN C:\B4.BIN
COPY /B C:\B4.BIN+C:\B4.BIN C:\B8.BIN
COPY /B C:\B8.BIN+C:\B4.BIN+C:\SEED.BIN C:\BIG.BIN
DEL C:\B2.BIN
DEL C:\B4.BIN
DEL C:\B8.BIN
DIR C:\BIG.BIN
