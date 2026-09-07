@ECHO OFF
REM  #25: does INT 40h disagree with INT 13h?
REM  Sergey's Multi-Floppy BIOS reports at POST that it installs on INT 40h,
REM  and its EEPROM is configured 1.44 MB (A:) / 1.2 MB (B:) - CORRECT.
REM  Windows asks INT 13h, which answers type 3 / 720K for BOTH drives.
REM
REM  If INT 40h returns BL=4 / BL=2 while INT 13h returns BL=3 / BL=3, then
REM  whoever owns INT 13h is answering without consulting Sergey's ROM, and
REM  that is the bug. Same drives, same boot, two vectors.
REM
REM  BL = drive type:  1=360K 5.25"  2=1.2M 5.25"  3=720K 3.5"  4=1.44M 3.5"
REM  RUN FROM REAL-MODE DOS, not a Windows DOS box.
ECHO INT 13h drive 0 (A:) ...  > C:\FDTYPE\FDCHAIN.TXT
DEBUG < C:\FDTYPE\FD0.SCR    >> C:\FDTYPE\FDCHAIN.TXT
ECHO INT 13h drive 1 (B:) ... >> C:\FDTYPE\FDCHAIN.TXT
DEBUG < C:\FDTYPE\FD1.SCR    >> C:\FDTYPE\FDCHAIN.TXT
ECHO INT 40h drive 0 (A:) ... >> C:\FDTYPE\FDCHAIN.TXT
DEBUG < C:\FDTYPE\F400.SCR   >> C:\FDTYPE\FDCHAIN.TXT
ECHO INT 40h drive 1 (B:) ... >> C:\FDTYPE\FDCHAIN.TXT
DEBUG < C:\FDTYPE\F401.SCR   >> C:\FDTYPE\FDCHAIN.TXT
ECHO.
TYPE C:\FDTYPE\FDCHAIN.TXT
ECHO.
ECHO Written to C:\FDTYPE\FDCHAIN.TXT
