@ECHO OFF
REM Which RAM answers 0-64 KB: the Inboard (fast) or the planar (XT bus)?
REM Run from real-mode DOS only (WBINVD is privileged under Windows).
DEBUG < C:\MEMLO0.SCR > C:\MEMLO0.TXT
DEBUG < C:\MEMLO1.SCR > C:\MEMLO1.TXT
ECHO MEMLO done.
