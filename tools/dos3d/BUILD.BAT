@echo off
rem M8CUBE32.EXE with Open Watcom; the DOS/32A extender (binw\dos32a.exe) is bound in as the stub.
set WATCOM=C:\Users\lycet\tools\openwatcom
set INCLUDE=%WATCOM%\h
set PATH=%WATCOM%\binnt64;%WATCOM%\binw;%PATH%
wcl386 -q -bt=dos -l=dos32a -ox -4r -fpi87 -fp3 -w4 -fe=M8CUBE32.EXE m8cube32.c
del m8cube32.obj
rem M8DTEST.EXE: the m8dos card layer's test card and triangle bench.
wcl386 -q -bt=dos -l=dos32a -ox -4r -fpi87 -fp3 -w4 -fe=M8DTEST.EXE m8dtest.c m8dos.c
del m8dtest.obj m8dos.obj
rem M8DTST16.EXE: the same test, 16-bit small model, as the layer goes into real-mode drivers (FS3 CUSTOM.GRA).
wcl -q -bt=dos -ms -ox -3 -fpi87 -fp3 -w4 -k8192 -DM8_HOSTED -fe=M8DTST16.EXE m8dtest.c m8dos.c
del m8dtest.obj m8dos.obj
