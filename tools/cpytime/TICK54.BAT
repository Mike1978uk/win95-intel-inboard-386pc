@ECHO OFF
REM TICK54.BAT - put the MinTimeSlice=54 SYSTEM.INI in place. Run from DOS, then restart Windows.
COPY C:\WINDOWS\SYSTEM.54 C:\WINDOWS\SYSTEM.INI > NUL
FIND /I "MinTimeSlice" C:\WINDOWS\SYSTEM.INI
ECHO TICK54: done - restart Windows
