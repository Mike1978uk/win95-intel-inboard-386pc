@ECHO OFF
REM LSENUMJ.BAT, then a clean shutdown so the result is flushed before the
REM harness stops the emulator. Enumeration probe, targeted at J: only.
REM
REM LSWRITE.BAT scans D:..K:, but on this image D:-I: are the Zip and the
REM Nakamichi changer LUNs. An IF EXIST against a not-ready CD raises
REM "Not ready reading drive X - Abort, Retry, Fail?" and the batch then
REM blocks forever waiting for a keypress, with nobody at the keyboard.
REM The LS-120 is J: on this chain, so go straight there.
ECHO ls120 enumeration probe > C:\LSPROBE.TXT
IF EXIST J:\PATTERN.BIN GOTO FOUND
ECHO no ls120 volume at J: >> C:\LSPROBE.TXT
GOTO END
:FOUND
ECHO drive J >> C:\LSPROBE.TXT
DIR J:\ >> C:\LSPROBE.TXT
COPY C:\COMMAND.COM J:\ENUMTEST.BIN >> C:\LSPROBE.TXT
DIR J:\ENUMTEST.BIN >> C:\LSPROBE.TXT
:END
RUNDLL32.EXE USER.EXE,ExitWindows
