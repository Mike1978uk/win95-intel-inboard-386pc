@ECHO OFF
REM CPYTIME.BAT - time two copies of C:\GOODTIME (36 MB) and append the times to C:\CPYTIME.TXT.
REM Run in a Windows DOS box. The clock is the SmartWatch-set system time, read with TIME to 1/100 s.
REM The tick setting in force is recorded with each run, so runs at different settings stay apart.
ECHO ---- run >> C:\CPYTIME.TXT
FIND /I "MinTimeSlice" C:\WINDOWS\SYSTEM.INI >> C:\CPYTIME.TXT
IF EXIST C:\GTCOPY.TMP DEL C:\GTCOPY.TMP
ECHO copy 1 start >> C:\CPYTIME.TXT
ECHO.|TIME|FIND /I "current" >> C:\CPYTIME.TXT
COPY /B C:\GOODTIME C:\GTCOPY.TMP > NUL
ECHO.|TIME|FIND /I "current" >> C:\CPYTIME.TXT
DEL C:\GTCOPY.TMP
ECHO copy 2 start >> C:\CPYTIME.TXT
ECHO.|TIME|FIND /I "current" >> C:\CPYTIME.TXT
COPY /B C:\GOODTIME C:\GTCOPY.TMP > NUL
ECHO.|TIME|FIND /I "current" >> C:\CPYTIME.TXT
DEL C:\GTCOPY.TMP
ECHO CPYTIME: done, times in C:\CPYTIME.TXT
