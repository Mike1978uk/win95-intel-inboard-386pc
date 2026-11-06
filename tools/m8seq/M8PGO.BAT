@ECHO OFF
REM Run from Command Prompt Only. Each probe writes its own .BIN here, so a hang loses only the probe it hit.
C:
CD \M8P
ECHO 1/6 M8GRAD   polygon fill colour sources. Screen unchanged, a few seconds.
M8GRAD
ECHO 2/6 M8DACX   shared palette. A flash.
M8DACX
ECHO 3/6 M8BNDRY  memory boundary. A flash.
M8BNDRY
ECHO 4/6 M8LATCH  latch copy timing. A flash.
M8LATCH
ECHO 5/6 M8VFLIP  WATCH: 1 box grey, 2 boxes blue, 3 boxes swaps, 4 boxes even tone. 24 seconds.
PAUSE
M8VFLIP
ECHO 6/6 M8W128   128 KB window. Riskiest. If it hangs, power off and tell me.
PAUSE
M8W128
ECHO Done. Restart Windows and tell me.