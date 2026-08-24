@ECHO OFF
REM Back out everything CPUSET.BAT did: cache OFF, clock 1:1.
REM Order matters - disable the cache BEFORE dropping the clock.
C:\CTCHIP\CTCHIP34.EXE IBM486 /1000h:0:=%%00000000
C:\CTCHIP\CTCHIP34.EXE IBM486 /1002h:3:=%%xxxxx000
ECHO.
ECHO Reverted to the Win95 baseline (all registers 00, 1:1 clock).
