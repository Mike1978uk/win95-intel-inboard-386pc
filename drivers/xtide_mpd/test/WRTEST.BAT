@ECHO OFF
REM ---------------------------------------------------------------------
REM  WRTEST.BAT - exercise the miniport's WRITE path, then shut down.
REM
REM  Goes in the StartUp folder. Copies C:\DOS (about 5.6 MB across 131
REM  files) to C:\WRDST, so VFAT issues many multi-cluster writes - which
REM  is the shape that carries scatter/gather, and scatter/gather is what
REM  the old .PDR misread straight into a volume's root directory.
REM
REM  Nothing here verifies anything. Verification is host-side, against
REM  the image, because a round trip through one broken driver cannot see
REM  its own error.
REM ---------------------------------------------------------------------
ECHO stage=start > C:\SHUTLOG.TXT
IF NOT EXIST C:\WRDST\NUL MD C:\WRDST
ECHO stage=copy_begin >> C:\SHUTLOG.TXT
XCOPY C:\DOS\*.* C:\WRDST\ /S > C:\WRXCOPY.LOG
IF ERRORLEVEL 1 ECHO stage=copy_ERROR >> C:\SHUTLOG.TXT
IF NOT ERRORLEVEL 1 ECHO stage=copy_ok >> C:\SHUTLOG.TXT
REM  Let the cache settle before asking for a shutdown, so the teardown
REM  flush is a flush and not a race with the copy.
ECHO stage=settle >> C:\SHUTLOG.TXT
CHOICE /T:Y,25 /C:YN > NUL
ECHO stage=shutdown >> C:\SHUTLOG.TXT
RUNDLL32.EXE USER.EXE,ExitWindows
