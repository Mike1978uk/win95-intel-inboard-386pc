@ECHO OFF
REM BackPack probe from inside Windows 95, dropped into the StartUp folder.
REM The drive letters are read under Windows, not at the DOS prompt, so a
REM drive that loads in DOS but is lost when Windows starts shows up here.
REM Every drive probed must hold a disc: DIR on an empty drive raises
REM Abort, Retry, Fail? and the batch blocks with nobody at the keyboard.
ECHO bpck windows probe > C:\BPWIN.TXT
ECHO --- enumeration (vendor BPDRIVES) >> C:\BPWIN.TXT
C:\CDDRIVES.EXE >> C:\BPWIN.TXT
ECHO --- D: (BackPack, real-mode driver + MSCDEX) >> C:\BPWIN.TXT
DIR D:\ >> C:\BPWIN.TXT
IF NOT EXIST C:\BPWIN_E.FLG GOTO DONE
ECHO --- E: (IDE CD-ROM, Windows' own driver) >> C:\BPWIN.TXT
DIR E:\ >> C:\BPWIN.TXT
:DONE
ECHO probe complete >> C:\BPWIN.TXT
RUNDLL32.EXE USER.EXE,ExitWindows
