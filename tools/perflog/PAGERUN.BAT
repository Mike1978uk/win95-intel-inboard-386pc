@ECHO OFF
REM Timed paging run. Usage: PAGERUN A   (a one-letter label)
REM Run first after boot, from an MS-DOS Prompt; hands off until it says done.
REM PAGETIME opens four programs and switches between them, timing each step
REM into C:\PAGETIME.TXT; PERFLOG samples the counters into C:\PERFLOG.CSV
REM alongside. RAMBASE stays the page-count baseline; this is the clock.
IF "%1"=="" GOTO USAGE
START C:\PERFLOG.EXE %1 300
C:\SLEEP.EXE 10
C:\PAGETIME.EXE %1
C:\WINCLOSE.EXE WAIT PERFLOG
ECHO PAGERUN %1 done.
GOTO END
:USAGE
ECHO Usage: PAGERUN A   (run first after boot; hands off for 5 minutes)
:END
