@ECHO OFF
REM ---------------------------------------------------------------
REM Blue Lightning BL3 - replicate the KNOWN-GOOD register set that
REM revto486.sys /BL /CN /CCM /2 produces on this board.
REM
REM Every value below was READ OFF THIS MACHINE (CTCHIP34 photos,
REM 2026-08-24) running DOS 6.22 / Win 3.11 with revto486 loaded.
REM Nothing here is guessed or taken from a forum post.
REM
REM Reference reads:            Win95 currently reads:
REM   1000h:0 = 92               1000h:0 = 00   (cache entirely OFF)
REM   1001h:0 = FF               1000h:1 = 00
REM   1001h:1 = 03               1000h:2 = 00
REM   1002h:3 = 03  (2:1)
REM ---------------------------------------------------------------

REM 1. Cacheable region FIRST, before enabling the cache.
REM    LMCR = 0x03FF -> only the low 640 KB is cacheable; the adapter
REM    area A0000-FFFFF is NOT. This is the register-level equivalent
REM    of EMMExclude=A000-FFFF, and it is what the working machine does.
C:\CTCHIP\CTCHIP34.EXE IBM486 /1001h:0:=%%11111111
C:\CTCHIP\CTCHIP34.EXE IBM486 /1001h:1:=%%00000011

REM 2. Clock mode 2:1. bits 2-0 = 011.
C:\CTCHIP\CTCHIP34.EXE IBM486 /1002h:3:=%%xxxxx011

REM 3. Processor Operation Register byte 1 = 9C.
REM    From REVTO486's own MSR dump: MSR1000 = 0000 9C92, so :1 = 9C.
REM    CNPX(7)=1 cachability of NPX operands, XTOUT(4)=1, CRLD(3)=1,
REM    IKEN(2)=1. Matches feipoa's description of REVTO486 exactly.
C:\CTCHIP\CTCHIP34.EXE IBM486 /1000h:1:=%%10011100

REM 4. Cache ON, with the coherency bit this board actually uses.
REM    92 = CE(7)=1 ADS# snooping(4)=1 cache parity checking(1)=1,
REM    everything else 0. ASNP is the DMA-coherency mechanism here -
REM    do NOT enable CE without it, the T130B does bus-master DMA.
C:\CTCHIP\CTCHIP34.EXE IBM486 /1000h:0:=%%10010010

ECHO.
ECHO Applied. Verify with C:\CTCHIP\CPUSHOW.BAT
ECHO   1000h:0=92  1000h:1=9C  1001h:0=FF  1001h:1=03  1002h:3=03
ECHO Back out: C:\CTCHIP\CPUOFF.BAT  (or just power cycle)
