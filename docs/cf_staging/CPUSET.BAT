@ECHO OFF
REM ---------------------------------------------------------------
REM Blue Lightning BL3 - apply the KNOWN-GOOD register set that
REM revto486.sys /BL /CN /CCM /2 produces on this board.
REM
REM Values read off THIS machine (CTCHIP34 screens + REVTO486's own
REM MSR dump, 2026-08-24). Nothing here is guessed.
REM
REM   reference (3.11+revto486)     Win95 before this runs
REM     1000h:0 = 92                  00   cache OFF
REM     1000h:1 = 9C                  00
REM     1001h:0 = FF                  00   nothing cacheable
REM     1001h:1 = 03                  00
REM     1002h:3 = 03  (2:1 / 2x)      00   1:1 / 1x  <- half speed
REM
REM NOTE the CD below: CTCHIP34 looks for IBM486.CFG in the CURRENT
REM directory, not next to the EXE. Without it you get "file not
REM found" even though the .CFG is sitting right beside CTCHIP34.EXE.
REM ---------------------------------------------------------------
C:
CD \CTCHIP

REM 1. Cacheable region FIRST, before switching the cache on.
REM    LMCR = 0x03FF -> only low 640 KB cacheable; A0000-FFFFF NOT.
CTCHIP34.EXE IBM486 /1001h:0=&11111111
CTCHIP34.EXE IBM486 /1001h:1=&00000011

REM 2. Clock mode 2:1.
CTCHIP34.EXE IBM486 /1002h:3=&00000011

REM 3. Processor Operation Register byte 1 = 9C.
CTCHIP34.EXE IBM486 /1000h:1=&10011100

REM 4. Cache ON with ADS# snooping - never CE without ASNP, the
REM    T130B does bus-master DMA.
CTCHIP34.EXE IBM486 /1000h:0=&10010010

CD \
ECHO.
ECHO CPU set. Verify: C:\CTCHIP\CPUSHOW.BAT
ECHO   want 1000h:0=92  1000h:1=9C  1001h:0=FF  1001h:1=03  1002h:3=03
