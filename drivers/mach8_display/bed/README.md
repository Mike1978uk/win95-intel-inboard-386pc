# Loading our display driver in a bed

Windows 95 loads the display driver named in the registry (`...\Class\Display\0000\DEFAULT`, value `drv`) through
`display.drv=pnpdrvr.drv`. Editing SYSTEM.INI by hand works for one boot; the first settings change rewrites it.

- `OWFB.REG` - make the registry load `owfb.drv`. Double-click in Windows, then restart.
- `ATIM8.REG` - back to ATI's driver. From DOS (F8, command prompt only): `REGEDIT C:\ATIM8.REG`.

FRAMEBUF-based builds also need `VFLATD.VXD` in `C:\WINDOWS\SYSTEM` and `device=vflatd.vxd` in `[386Enh]`; the ATI
install never had it. They drive the 28800 VGA half only (512 KB): 640x480 or 800x600 at 256 colours.
