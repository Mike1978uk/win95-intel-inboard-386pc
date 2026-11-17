@ECHO OFF
REM XTINSWA: XTINSW with the BL3 cache on, then off, for #50.
REM DOS prompt after CPUSET.BAT. No disk, floppy or sound use while it runs.
REM Cache off is 1000h:0 = 00, the state before CPUSET. Ends by restoring 92.
C:
C:\XTINSW.COM > C:\XI_ON.TXT
CD \CTCHIP
CTCHIP34.EXE IBM486 /1000h:0=&00000000 > NUL
C:\XTINSW.COM > C:\XI_OFF.TXT
CTCHIP34.EXE IBM486 /1000h:0=&10010010 > NUL
ECHO Check 1000h:0 = 92 in the next screen, then Q.
PAUSE
CTCHIP34.EXE IBM486
CD \
C:\XTINSW.COM > C:\XI_ON2.TXT
ECHO Done. Results C:\XI_ON.TXT XI_OFF.TXT XI_ON2.TXT. Reboot into Windows.
