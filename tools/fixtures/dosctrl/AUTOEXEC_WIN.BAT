@ECHO OFF
PROMPT $p$g

PATH C:\PAGEMGR\IMGFOLIO;C:\PAGEMGR;C:\WINDOWS;C:\WINDOWS\COMMAND;C:\DOS
SET TEMP=C:\TEMP

CALL C:\CTCHIP\CPUSET.BAT

mode con codepage prepare=((850) C:\WINDOWS\COMMAND\ega.cpi)
mode con codepage select=850
keyb uk,,C:\WINDOWS\COMMAND\keyboard.sys
SET BLASTER=A220 I5 D1 T4

echo Setting time and Date
c:\clock\smwclock s
echo.

ECHO vendor real-mode stack > C:\DOSPROBE.TXT
DIR D:\ >> C:\DOSPROBE.TXT
DIR E:\ >> C:\DOSPROBE.TXT
REM IRET patch from Win 95 build. Must stay the LAST line - technique 38.
C:\IVT68FIX.COM

REM echo Starting Comrade
REM c:\Comrade.exe
