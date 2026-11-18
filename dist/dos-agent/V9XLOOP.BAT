@ECHO OFF
REM Runs the V9x DOS agent in a loop. Start it as the last line of AUTOEXEC.BAT.
REM   C:\V9XREMOT\OFF present  -> the agent is not started (manual use).
REM   N at the prompt          -> skip the agent for this boot only.
REM   errorlevel 3             -> the host sent a detached job: run RUNJOB.BAT
REM                               with the agent unloaded, then start it again.
REM   Esc at the console      -> the agent exits with errorlevel 4; the loop ends.
REM Any other exit ends the loop too; errorlevel stays the agent's, so the
REM caller can tell Esc (4) from a failed start (1).
CHOICE /C:YN /T:Y,3 Start the V9x agent
IF ERRORLEVEL 2 GOTO END
:LOOP
IF EXIST C:\V9XREMOT\OFF GOTO END
IF EXIST C:\V9XREMOT\RUNJOB.BAT DEL C:\V9XREMOT\RUNJOB.BAT
C:\V9XREMOT\V9XAGNT.EXE
IF ERRORLEVEL 4 GOTO END
IF NOT ERRORLEVEL 3 GOTO END
IF NOT EXIST C:\V9XREMOT\RUNJOB.BAT GOTO LOOP
CALL C:\V9XREMOT\RUNJOB.BAT
C:
CD \
GOTO LOOP
:END
