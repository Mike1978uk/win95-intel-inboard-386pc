Draft, not posted. Owner to review and send.

```
Simon got a real IBM 5150 running Windows 95. Thank you.

What he found: the 08-24 release image's Windows directory, our buffered-mode VMM32.VXD in C:\WINDOWS\SYSTEM, and C:\WIN386.SWP deleted (the swap file that ships in the image; Windows makes a new one). That cleared the "Insufficient memory" stop.

What changed: the VPICD with IRQ 2 as IRQ 9 and buffered-mode 8259 programming (ICW4 09h, as IBM's XT BIOS and Intel's IBVPICD do) is now the standard build for every machine, because it gives an XT the most usable IRQs. It has run on the real 5160 since 2026-11-18. The older unbuffered and non-IRQ 2 builds are history.

Pre-monolith, drop in WINDOWS\SYSTEM\VMM32\:
https://github.com/Mike1978uk/win95-intel-inboard-386pc/raw/master/vxd-patches/VPICD_INBOARD_BUF.VXD

Installed system, replace C:\WINDOWS\SYSTEM\VMM32.VXD (keep the original as VMM32.IRQ):
https://github.com/Mike1978uk/win95-intel-inboard-386pc/raw/master/vxd-patches/VMM32_buffered.VXD

This is the 08-24 release image's own VMM32.VXD with the two patches applied. The release image itself still ships the unpatched file.

We do not yet know why deleting WIN386.SWP helps. If anyone can test with a fresh swap file on a 5150, I would like to hear what happens.

Steps are in FIXES.md under VPICD.

Simon, thank you for your continued input and for testing on real 5150 hardware.
```
