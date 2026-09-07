@ECHO OFF
REM  What does the BIOS report for each floppy? INT 13h AH=08h.
REM  BL = drive type:  1=360K 5.25"   2=1.2M 5.25"   3=720K 3.5"   4=1.44M 3.5"
REM  CH = max cylinder   CL = max sector   DH = max head   DL = drive count
REM
REM  RUN THIS FROM REAL-MODE DOS, not a Windows DOS box - in a DOS box
REM  Windows virtualises INT 13h and you get Windows' view, not the BIOS's.
ECHO Querying drive 0 (A:) ... > C:\FDTYPE\FDTYPE.TXT
DEBUG < C:\FDTYPE\FD0.SCR >> C:\FDTYPE\FDTYPE.TXT
ECHO. >> C:\FDTYPE\FDTYPE.TXT
ECHO Querying drive 1 (B:) ... >> C:\FDTYPE\FDTYPE.TXT
DEBUG < C:\FDTYPE\FD1.SCR >> C:\FDTYPE\FDTYPE.TXT
ECHO.
TYPE C:\FDTYPE\FDTYPE.TXT
ECHO.
ECHO Written to C:\FDTYPE\FDTYPE.TXT
