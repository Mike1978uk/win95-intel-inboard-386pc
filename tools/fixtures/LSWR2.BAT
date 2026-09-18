@ECHO OFF
REM Bed write test for the LS-120, which now enumerates at J:.
REM Writes, then reads back, so a silent failure cannot look like success.
REM Every step appends, so the log shows how far it got even if DOS stops.
ECHO ls120 write probe > C:\LSPROBE.TXT
IF NOT EXIST J:\NUL ECHO no J: volume >> C:\LSPROBE.TXT
IF NOT EXIST J:\NUL GOTO END
ECHO --- dir before --- >> C:\LSPROBE.TXT
DIR J:\ >> C:\LSPROBE.TXT
ECHO --- write --- >> C:\LSPROBE.TXT
COPY C:\COMMAND.COM J:\WRTEST.BIN >> C:\LSPROBE.TXT
ECHO --- dir after --- >> C:\LSPROBE.TXT
DIR J:\ >> C:\LSPROBE.TXT
ECHO --- read back --- >> C:\LSPROBE.TXT
COPY J:\WRTEST.BIN C:\RDBACK.BIN >> C:\LSPROBE.TXT
DIR C:\RDBACK.BIN >> C:\LSPROBE.TXT
ECHO --- done --- >> C:\LSPROBE.TXT
:END
