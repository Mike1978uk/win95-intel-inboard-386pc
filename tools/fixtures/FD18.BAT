@ECHO OFF
REM Issue #18 probe: does the 32-bit floppy driver read A: correctly?
REM FDPAT.BIN is 64 KB of a non-repeating pattern, so a DMA page-latch wrap
REM shows as a mismatch rather than as plausible-looking data.
ECHO fd18 probe > C:\FDPROBE.TXT
DIR A:\ >> C:\FDPROBE.TXT
COPY A:\FDPAT.BIN C:\FDREAD.BIN >> C:\FDPROBE.TXT
FC /B C:\FDREAD.BIN C:\FDPAT.BIN > C:\FDDIFF.TXT
ECHO --- FC done, see FDDIFF.TXT >> C:\FDPROBE.TXT
