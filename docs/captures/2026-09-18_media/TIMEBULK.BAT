@ECHO OFF
REM ECPBULK (bulk over ECP, WITH the 1284 terminate) against SPP2, interleaved.
REM ECPBULK is ECP2 plus call 1600 at both block tails - the script that has
REM never existed until now. ECP2 itself is void: no terminate, F5 everywhere.
ECHO. | TIME > C:\TB.TXT
DEBUG < C:\ECPBULK.SCR > C:\TB1.OUT
ECHO. | TIME >> C:\TB.TXT
DEBUG < C:\SPP2.SCR > C:\TB2.OUT
ECHO. | TIME >> C:\TB.TXT
DEBUG < C:\ECPBULK.SCR > C:\TB3.OUT
ECHO. | TIME >> C:\TB.TXT
DEBUG < C:\SPP2.SCR > C:\TB4.OUT
ECHO. | TIME >> C:\TB.TXT
