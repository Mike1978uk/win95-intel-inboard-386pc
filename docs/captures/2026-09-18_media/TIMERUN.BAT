@ECHO OFF
REM Time SPP2 against ECP2 with the guest's own clock, so the serial link's
REM ~34 ms round trip is outside the measurement. The two scripts are byte
REM identical except the transport call target, so the difference between the
REM pairs is the transport.
REM
REM Alternating S,E,S,E rather than S,S,E,E: the drive's own state drifts
REM between runs (two identical SPP runs measured 16.86 s and 11.97 s), so
REM interleaving stops a drift being read as a transport difference.
ECHO. | TIME > C:\TT.TXT
DEBUG < C:\SPP2.SCR > C:\TS1.OUT
ECHO. | TIME >> C:\TT.TXT
DEBUG < C:\ECP2.SCR > C:\TE1.OUT
ECHO. | TIME >> C:\TT.TXT
DEBUG < C:\SPP2.SCR > C:\TS2.OUT
ECHO. | TIME >> C:\TT.TXT
DEBUG < C:\ECP2.SCR > C:\TE2.OUT
ECHO. | TIME >> C:\TT.TXT
