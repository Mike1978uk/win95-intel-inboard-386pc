@ECHO OFF
REM Control: copy the same 9 MB file WITHOUT touching the LS-120, so a stall
REM can be attributed to the bed rather than to the driver under test.
ECHO cf-only control > C:\LSPROBE.TXT
ECHO start >> C:\LSPROBE.TXT
COPY C:\CREATI~1\CREATI~1.ZIP C:\CFONLY.ZIP >> C:\LSPROBE.TXT
ECHO end >> C:\LSPROBE.TXT
DIR C:\CFONLY.ZIP >> C:\LSPROBE.TXT
