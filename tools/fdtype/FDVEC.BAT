@ECHO OFF
REM  #25: WHO owns the three vectors that decide the floppy geometry answer?
REM  Dumped as offset,segment little-endian - read each pair backwards.
REM
REM    0000:004C = INT 13h  - what Windows asks. Answering 720K for both.
REM    0000:0078 = INT 1Eh  - the diskette parameter table pointer.
REM                           Expect F000:EFA0 (A0 EF 00 F0) - the same value
REM                           AH=08h handed back, which would explain ES:DI.
REM    0000:0100 = INT 40h  - Sergey's Multi-Floppy BIOS says at POST it is
REM                           here, with the drives configured CORRECTLY.
REM
REM  Segment tells you the owner: F000 = system BIOS, C800-DE00 = an option
REM  ROM, anything lower = a resident (INBRDPC.SYS hooks INT 13h at 0575:0122).
REM
REM  RUN FROM REAL-MODE DOS, not a Windows DOS box.
ECHO INT 13h @ 0:4C / INT 1Eh @ 0:78 / INT 40h @ 0:100 > C:\FDTYPE\FDVEC.TXT
DEBUG < C:\FDTYPE\FDVEC.SCR >> C:\FDTYPE\FDVEC.TXT
ECHO.
TYPE C:\FDTYPE\FDVEC.TXT
ECHO.
ECHO Written to C:\FDTYPE\FDVEC.TXT
