@ECHO OFF
REM FLUSHA: FLUSHP 1 with the BL3 cache bits that might cause the I/O flush.
REM DOS only. No floppy, T130B or sound use while it runs: ASNP keeps
REM bus-master DMA coherent. Ends by restoring 1000h:0=92 1000h:1=8C.
REM The 1000h:1 (CRLD) writes do not take: the byte locks after CPUSET.
C:
CD \CTCHIP
C:\FLUSHP.COM 1 > C:\FA_BASE.TXT
ECHO ASNP off: 1000h:0 = 82
CD \CTCHIP
CTCHIP34.EXE IBM486 /1000h:0=&10000010 > NUL
ECHO Check 1000h:0 = 82 in the next screen, then Q.
PAUSE
CTCHIP34.EXE IBM486
C:\FLUSHP.COM 1 > C:\FA_ASNP.TXT
ECHO ASNP back on, CRLD off: 1000h:0 = 92, 1000h:1 = 84
CTCHIP34.EXE IBM486 /1000h:0=&10010010 > NUL
CTCHIP34.EXE IBM486 /1000h:1=&10000100 > NUL
ECHO Check 1000h:0 = 92 and 1000h:1 = 84 in the next screen, then Q.
PAUSE
CTCHIP34.EXE IBM486
C:\FLUSHP.COM 1 > C:\FA_CRLD.TXT
ECHO Restoring 1000h:1 = 8C
CTCHIP34.EXE IBM486 /1000h:1=&10001100 > NUL
CD \
ECHO Done. Results C:\FA_BASE.TXT FA_ASNP.TXT FA_CRLD.TXT. Reboot into Windows.
