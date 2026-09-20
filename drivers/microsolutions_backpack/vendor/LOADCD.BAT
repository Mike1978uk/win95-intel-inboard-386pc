:
: This batch file can be used to load the BACKPACK drivers after a
: system has booted up. The DEVICE.COM program on this disk is
: used to load the BACKPACK device driver.
:
device=bpcddrv.sys
if exist bpcddrv$ mscdex /d:bpcddrv$
