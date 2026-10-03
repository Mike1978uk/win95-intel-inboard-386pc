@ECHO OFF
REM Timed paging run. Usage: PAGERUN [label]   (default label R)
REM Run first after boot, from an MS-DOS Prompt; hands off until it says done.
REM PAGETIME opens four programs and switches between them, timing each step
REM into C:\PAGETIME.TXT; PERFLOG samples the counters into C:\PERFLOG.CSV
REM alongside. RAMBASE stays the page-count baseline; this is the clock.
REM Both programs add SHADRAM's state (-S16, -N) and the start time to the
REM label, so every run is unique in the same two files.
SET PR=%1
IF "%PR%"=="" SET PR=R
START C:\PERFLOG.EXE %PR% 300
C:\SLEEP.EXE 10
C:\PAGETIME.EXE %PR%
C:\WINCLOSE.EXE WAIT PERFLOG
ECHO PAGERUN %PR% done.
SET PR=
