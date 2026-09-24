@ECHO OFF
REM LS-120 probe for a minimal bed: find the test disk by its PATTERN.BIN on
REM any letter D: to K:, list it and write to it. Safe only where every CD-ROM
REM in the config holds a disc - IF EXIST on an empty drive raises
REM Abort, Retry, Fail? and nobody may be at the keyboard to answer it.
ECHO ls120 search probe > C:\LSPROBE.TXT
FOR %%D IN (D E F G H I J K) DO IF EXIST %%D:\PATTERN.BIN ECHO drive %%D >> C:\LSPROBE.TXT
FOR %%D IN (D E F G H I J K) DO IF EXIST %%D:\PATTERN.BIN DIR %%D:\ >> C:\LSPROBE.TXT
FOR %%D IN (D E F G H I J K) DO IF EXIST %%D:\PATTERN.BIN COPY C:\COMMAND.COM %%D:\ENUMTEST.BIN >> C:\LSPROBE.TXT
FOR %%D IN (D E F G H I J K) DO IF EXIST %%D:\ENUMTEST.BIN DIR %%D:\ENUMTEST.BIN >> C:\LSPROBE.TXT
ECHO probe complete >> C:\LSPROBE.TXT
