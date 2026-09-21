@ECHO OFF
REM XT-IDE request-merging A/B. Run from an MS-DOS PROMPT INSIDE WINDOWS -
REM a real-mode DOS boot bypasses XTIDEMP.MPD and measures nothing.
REM   PBENCH BASE   /   PBENCH PB17
REM
REM Timing: "ECHO. | TIME" feeds TIME a blank line so it prints and RETURNS.
REM Do NOT use "TIME < NUL" - on EOF it re-prompts forever and never runs
REM the copy at all (cost two runs on 2026-09-21).
ECHO ARM %1 > C:\PBENCH.OUT
ECHO. | TIME >> C:\PBENCH.OUT
COPY C:\GOODTIME\GOODTIME.MPG NUL > NUL
ECHO. | TIME >> C:\PBENCH.OUT
TYPE C:\PBENCH.OUT
