@ECHO OFF
REM Non-LPT regression probe for the EPAT patch on 86Box master.
REM
REM Deliberately probes NO drive letters: an IF EXIST against a not-ready
REM changer LUN raises "Abort, Retry, Fail?" and blocks with nobody at the
REM keyboard. Reaching StartUp at all proves Windows booted; the DIR proves
REM disk I/O still works with the LPT device replaced by a plain printer.
ECHO non-lpt regression probe > C:\NLPROBE.TXT
VER >> C:\NLPROBE.TXT
DIR C:\WINDOWS\*.INI >> C:\NLPROBE.TXT
