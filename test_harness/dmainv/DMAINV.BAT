@ECHO OFF
REM Read-only 8237 and PIT channel 1 probe. Real DOS only, not a Windows DOS box.
DEBUG < C:\DMAINV.SCR > C:\DMAINV.TXT
ECHO DMAINV done: C:\DMAINV.TXT
