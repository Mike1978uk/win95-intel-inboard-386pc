@ECHO OFF
REM TICKDEF.BAT - put the default-tick SYSTEM.INI (no MinTimeSlice) in place. Run from DOS, then restart.
COPY C:\WINDOWS\SYSTEM.BSR C:\WINDOWS\SYSTEM.INI > NUL
FIND /I "MinTimeSlice" C:\WINDOWS\SYSTEM.INI
ECHO TICKDEF: done - restart Windows (no line above means the default tick)
