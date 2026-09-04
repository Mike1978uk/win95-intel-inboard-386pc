# REX-CFU1 — first test round on Windows 98

Copy `dist/CFUDIAG.EXE` and `dist/CFU1.VXD` into the **same directory** on
the Win98 box (floppy/CF, anywhere). Insert the REX-CFU1. If the New
Hardware wizard appears, hit **Cancel** (the INF is a later step).

Then from a DOS box (windowed command prompt):

```
CFUDIAG /IDS
```
→ note every `PCMCIA\...` line (this gives us the exact PnP ID + CRC for
the INF — it's the one with RATOC in it).

```
CFUDIAG /DETECT
```
Expected output (nothing plugged into the USB port):

```
CFU1.VXD loaded, version 1.00
COR (attr 0xFC) readback: 42 (expect 42)
PCIC status: EF            (bit5 READY + bits2-3 card-detect are the ones that matter)
SL811 hwrev: 20 (SL811HS v1.5)
SL811 regs 00-0F: ...
buffer RAM test (0x10-0xFF, 2 passes): PASS (0 bad)
INTSTAT idle: before-clear xx, 3ms after clear 40
detect: INTSTAT=40 -> NO DEVICE (bus SE0)
```

Then with a USB device (stick/mouse/anything) plugged into the card:

```
CFUDIAG /DETECT
```
→ hoping for `FULL-SPEED device attached`. **If it still says NO DEVICE,
the card's VBUS switch is probably off** — that's the unknown latch at
base+2; tell Claude and we'll walk its bits over COMrade-style iterations.

IRQ delivery (uses the VxD; pick a free IRQ from Device Manager, try 5,
10, 11):

```
CFUDIAG /SOF /IRQ 5
```
→ `IRQ5 SOF interrupt test: ~1000 ints in ~1s` = full success.

If the VxD fails to load, everything still works minus the IRQ test via:

```
CFUDIAG /RAW
```

Useful switches: `/SOCKET 1` (other slot), `/BASE 280` (alt I/O base),
`/WIN D400` (alt attribute window if D000 is occupied), `/OFF` (power the
socket down).

Please capture the full text output of each run (redirect with
`CFUDIAG ... > OUT1.TXT` works fine).
