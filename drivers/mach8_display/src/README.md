# M8DISP.DRV - Windows 95 driver for the ATI Graphics Ultra (Mach8)

GPL-3.0-or-later. The frame (Enable, DIB Engine thunks, resources, `ddk/` headers) comes from Michal
Necasek's boxv9x as carried in JHRobotics' [vmdisp9x](https://github.com/JHRobotics/vmdisp9x) (MIT,
`LICENSE-MIT.txt`, commit `767761d0`). Design and plan: `docs/mach8_driver_design.md` (D8).

Build: `.\build.ps1` (Open Watcom; no DDK - the DIB Engine import library is built from
`ddk/dibeng.lbc`, whose ordinals match the Windows 95 DDK's `DIBENG.LIB`). Output `build\M8DISP.DRV`.

D8a, the first build: 800x600x8 only; solid fills and screen-to-screen copies on the engine; every other
screen operation through the read-back fallback (`screen.c`); software cursor; counters per GDI entry in
`m8_stats`. Not yet: DOS full-screen switching (INT 2Fh 4001h/4002h), other modes, ReEnable.

Load it in a bed: `bed/M8DISP.REG` (see `bed/README.md`); `M8DISP.DRV` in `C:\WINDOWS\SYSTEM`.
