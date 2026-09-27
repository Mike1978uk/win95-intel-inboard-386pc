@ECHO OFF
REM RAM baseline. Usage: RAMBASE A   (a one-letter label)
REM Run first after boot, from an MS-DOS Prompt. Hands off until PERFLOG's
REM own window says done (about 5 minutes); then close the programs it opened.
REM PERFLOG samples every 2 s into C:\PERFLOG.CSV while this runs:
REM copies, then four programs opened one by one, then the copies again
REM with the programs still open, so the second copies have to page.
IF "%1"=="" GOTO USAGE
START C:\PERFLOG.EXE %1 300
CHOICE /C:N /T:N,10 > NUL
CALL C:\T130AB.BAT C
IF EXIST C:\RB%11.TXT DEL C:\RB%11.TXT
REN C:\T130AB.TXT RB%11.TXT
START C:\PSP\PSP.EXE
CHOICE /C:N /T:N,40 > NUL
START "C:\Program Files\Accessories\WORDPAD.EXE"
CHOICE /C:N /T:N,30 > NUL
START C:\WINDOWS\NOTEPAD.EXE C:\WINDOWS\WIN.INI
CHOICE /C:N /T:N,15 > NUL
START C:\WINDOWS\PBRUSH.EXE
CHOICE /C:N /T:N,30 > NUL
CALL C:\T130AB.BAT C
IF EXIST C:\RB%12.TXT DEL C:\RB%12.TXT
REN C:\T130AB.TXT RB%12.TXT
ECHO RAMBASE %1 done. Wait for PERFLOG's window to say done, then close
ECHO Paint Shop Pro, WordPad, Notepad and Paint.
GOTO END
:USAGE
ECHO Usage: RAMBASE A   (run first after boot; hands off for 5 minutes)
:END
