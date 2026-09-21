@ECHO OFF
REM XT-IDE request-merging A/B. Run from an MS-DOS PROMPT INSIDE WINDOWS -
REM a real-mode DOS boot bypasses XTIDEMP.MPD and measures nothing.
REM   PBENCH BASE   /   PBENCH PB17
REM
REM Appends, never overwrites: a second arm must not destroy the first.
REM Timing uses "ECHO. | TIME" - never "TIME < NUL", which re-prompts forever.
ECHO. >> C:\PBENCH.OUT
ECHO ==== ARM %1 ==== >> C:\PBENCH.OUT
ECHO. | TIME >> C:\PBENCH.OUT
COPY C:\GOODTIME\GOODTIME.MPG NUL > NUL
ECHO. | TIME >> C:\PBENCH.OUT
TYPE C:\PBENCH.OUT
