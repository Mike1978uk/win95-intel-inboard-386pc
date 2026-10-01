@ECHO OFF
REM RAM baseline. Usage: RAMBASE A   (a one-letter label)
REM Run first after boot, from an MS-DOS Prompt. Hands off until this window
REM says done (about 5 minutes); it closes the programs it opened itself.
REM SLEEP.EXE waits without using the CPU (CHOICE /T spins).
REM PERFLOG samples every 2 s into C:\PERFLOG.CSV while this runs:
REM copies, then four programs opened one by one, then the copies again
REM with the programs still open, so the second copies have to page.
IF "%1"=="" GOTO USAGE
START C:\PERFLOG.EXE %1 300
C:\SLEEP.EXE 10
CALL C:\T130AB.BAT C
IF EXIST C:\RB%11.TXT DEL C:\RB%11.TXT
REN C:\T130AB.TXT RB%11.TXT
START C:\PSP\PSP.EXE
C:\SLEEP.EXE 40
START "C:\Program Files\Accessories\WORDPAD.EXE"
C:\SLEEP.EXE 30
START C:\WINDOWS\NOTEPAD.EXE C:\WINDOWS\WIN.INI
C:\SLEEP.EXE 15
START C:\WINDOWS\PBRUSH.EXE
C:\SLEEP.EXE 30
CALL C:\T130AB.BAT C
IF EXIST C:\RB%12.TXT DEL C:\RB%12.TXT
REN C:\T130AB.TXT RB%12.TXT
REM PERFLOG's window goes when it finishes; close the programs after that,
REM so the log covers the whole run and the next one starts clean.
C:\WINCLOSE.EXE WAIT PERFLOG
C:\WINCLOSE.EXE Paint Shop Pro
C:\WINCLOSE.EXE WordPad
C:\WINCLOSE.EXE Notepad
C:\WINCLOSE.EXE Paint
ECHO RAMBASE %1 done.
GOTO END
:USAGE
ECHO Usage: RAMBASE A   (run first after boot; hands off for 5 minutes)
:END
