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

REM Boot-menu choice from CONFIG.SYS. A driver renamed to .OFF does not load.
C:
CD \WINDOWS\SYSTEM\IOSUBSYS
REM Test, not GOTO %CONFIG%: an unset CONFIG would end the batch before IVT68FIX.
IF "%CONFIG%"=="LEAN" GOTO LEAN
IF "%CONFIG%"=="SAFE" GOTO LEAN
:FULL
IF EXIST SD120PPD.OFF IF NOT EXIST SD120PPD.MPD REN SD120PPD.OFF SD120PPD.MPD
IF EXIST NEROCD95.OFF IF NOT EXIST NEROCD95.VXD REN NEROCD95.OFF NEROCD95.VXD
GOTO MENUDONE
:LEAN
IF EXIST SD120PPD.MPD REN SD120PPD.MPD SD120PPD.OFF
IF EXIST NEROCD95.VXD REN NEROCD95.VXD NEROCD95.OFF
:MENUDONE
CD \

REM INT 15h AH=86h wait, so the floppy motor wait never counts DMA refresh (#7).
C:\WAIT86.COM

REM IRET patch from Win 95 build. Must be the last command before Windows - technique 38.
C:\IVT68FIX.COM
REM Safe Mode after a normal CONFIG.SYS; WIN must follow IVT68FIX directly.
IF "%CONFIG%"=="SAFE" WIN /D:M

REM echo Starting Comrade
REM c:\Comrade.exe
