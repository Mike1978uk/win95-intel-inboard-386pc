@echo off
rem M8CUBE32.EXE with Open Watcom; the DOS/32A extender (binw\dos32a.exe) is bound in as the stub.
set WATCOM=C:\Users\lycet\tools\openwatcom
set INCLUDE=%WATCOM%\h
set PATH=%WATCOM%\binnt64;%WATCOM%\binw;%PATH%
wcl386 -q -bt=dos -l=dos32a -ox -4r -fpi87 -fp3 -w4 -fe=M8CUBE32.EXE m8cube32.c
del m8cube32.obj
