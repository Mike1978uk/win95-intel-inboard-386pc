# Mach8 read data available - OBattler's Solitaire report (2026-10-18)

## The report

86Box Discord, 2026-10-18. Mamoru (OBattler): the Mach8 hangs Windows 3.0 Solitaire, with both the
stock 8514/A driver and ATI's; build 10377 works, 10378 hangs. win2kgamer posted the VM (kept locally at
`C:\Users\lycet\OneDrive\Desktop\XT_project\ATI\Mach8-Win30`, bed copy `vm_mach8_w30/`). TC1995: "it's a
data available bug".

Jenkins build 10378 is exactly one commit: ours, `942e2de86` (#8289).

## Cause

#8289 made the Graphics Ultra's GE_STAT run the end-of-transfer check before reporting bit 8 (read data
available), so the flag drops with the last word the host takes. For an 8514/A line read (`cmd >> 13 == 1`)
the check is `!SY`. A one-pixel line read starts with SY already 0, so the first poll cleared the flag
before the host had taken the only word, the bit was never reported, and the driver spun.

Logged with a temporary GE_STAT/PIX_TRANS read hook (`MACH8_DRLOG`, removed):

```
01DD:014F 9ae8/2=0200 cmd=3318 sy=0 da=0/1 back=0     <- hang build: first poll, flag already gone
01DD:014F 9ae8/2=0300 cmd=3318 sy=0 da=1/1 back=0     <- fix: bit set
01DD:0158 e2e8/2=0202 back=1                          <- driver takes the pixel
```

Solitaire reads ~500 single pixels at start-up, so it hung on the hourglass before a card could be dragged.
Longer line reads worked: SY reaches 0 on the word that takes the last pixel, which also sets `cmd_back`.

## Fix

Fork `Mike1978uk/86Box` branch `mach8-dataready`, `6ca622ca1`: on the Graphics Ultra the line-read case
clears the flag only once `cmd_back` is set (the engine has delivered the last word). Both GE_STAT read
paths (9AE8 word, 9AE9 byte). Mach32 and the 8514/ULTRA add-on keep their code path.

## Tests

| Test | Result |
|---|---|
| `vm_mach8_w30` (the reporter's VM: Win 3.0, ASUS 386, Mach8 8-bit), upstream master | Solitaire hangs at start-up (owner) |
| Same bed, fix | Solitaire starts, plays, stable (owner) |
| `vm_mach8_8514drv` (Win 3.11, stock 8514.DRV, Inboard XT), fix | Solitaire and File Manager fine (owner) |
| `vm_mach8_w311` (Win 3.11, ATI MACHW3.DRV), fix | Solitaire fine (owner) |
| A6, Graphics Ultra, 49 probes, master vs fix (`dr_base`/`dr_fix`) | 48/49 identical; M8STAT differs between runs of one build (timing, as on 10-16) |
| A6, Mach32, 49 probes, master vs fix (`dr_*_mach32_isa`) | 48/49 identical; M8REGS byte 13 is `9B` in the master run and `9C` in the fix run, and `9C` in all 11 earlier Mach32 runs - the outlier is the master run |
| A6, IBM 8514/A, the 10-11 list of 34 probes (no TEST.COM probes, no M8CONF), master vs fix | 34/34 identical |
| G11: `vm_8514a_at` (ASUS 386, IBM VGA + 8514/A), fix | Windows 3.11 desktop (owner) |
| G11: same bed with an ET4000AX, fix | POST, DOS, Windows start-up logo; then black (Windows is set up for 8514.DRV); emulator up for 95 s |
| Not run | 8514/ULTRA add-on (code path unchanged); a real Graphics Ultra |

G1 pass, G7 master unchanged since the branch point, G8 one file +4/-2, G9 the tested exe was built from the
committed tree 9 s before the commit, G10 owner by hand (three Windows beds).

## A second finding, not a bug: 8514.DRV's caption buttons

The owner saw a one-pixel row of wallpaper at the top of each window's control-menu and min/max boxes, and
a white strip under the MDI menu-row buttons, under the stock 8514.DRV only. It is present on the pre-fix
build, on the build before #8289 (`1741108a7`), and on a plain IBM 8514/A on an AT (`vm_8514a_at`).
A register log (`diag-8514cmd`, local only) shows the driver itself writes CUR_Y = 109 for the first row of
an 18-row button bitmap inside a 19-row caption (rows 108-126), with no register read in between; no
command ever paints row 108 at those x positions. The emulator draws what it is told. ATI's MACHW3 ships its
own button bitmaps and does not show it. Not verified on a real 8514/A.

## Harness notes

- `fatcp.py` refuses to write any image while any 86Box runs: an owner VM open voids an A6 run.
- `bed_launch.ps1` used a PowerShell 7-only `GetFullPath` overload; `a6_regress.sh` runs it under 5.1, so
  every A6 run since `9e2cbd0` failed before the VM started. Fixed `451c463`.
- The plain 8514/A needs a 16-bit slot; 86Box refuses it on the Inboard XT. `vm_8514a_at` is the AT bed:
  Win 3.11 image with the Inboard lines removed from CONFIG.SYS and Windows' stock keyboard drivers
  (`ibkbd.drv`/`ibvkd.386` require the Inboard).

## PR

Opened 2026-10-18 as https://github.com/86Box/86Box/pull/8394 (head `6ca622ca1`), wording approved by the owner. Merged 2026-10-18 14:56 UTC as `0f7cf1381`.

The text as submitted:

Title: `mach8: fix read data available on one-pixel line reads (Solitaire hang)`

Body:

```
#8289 changed when the ATI Graphics Ultra reports GE_STAT bit 8 (read data available): the end-of-transfer check now runs before the bit is reported, so the flag drops with the last word the host takes. For an 8514/A line read that check is SY == 0, and a one-pixel line read starts with SY already 0. The first poll therefore cleared the flag before the host had taken the only word, and any driver waiting for the bit hung. Windows 3.0 Solitaire reads single pixels at start-up and hung on the hourglass, with both the stock 8514/A driver and ATI's (reported by OBattler on Discord; build 10377 works, 10378 hangs).

On the Graphics Ultra the line-read case now clears the flag only once the engine has delivered the last word (cmd_back), in both the word (9AE8) and byte (9AE9) GE_STAT reads. Mach32 and the 8514/ULTRA add-on are unchanged.

Tested:
- The reporter's VM (Windows 3.0, ASUS 386, Graphics Ultra on an 8-bit bus): hangs on master, Solitaire starts and plays with the fix.
- Windows 3.11 with the stock 8514.DRV and with ATI's MACHW3.DRV: Solitaire and File Manager fine.
- Our Graphics Ultra probe set (49 register and draw probes, compared byte for byte): identical to master except one status-timing probe that also varies between runs of the same build.
- Mach32 and IBM 8514/A probe sets: identical to master (one Mach32 register byte differs in the master run, not the fix run, which matches all earlier runs).
- A 386 with IBM VGA + 8514/A, and the same machine with an ET4000AX: both boot to the OS.

Not tested: the 8514/ULTRA add-on (its code path is not changed); a real Graphics Ultra running Solitaire.

Thanks to OBattler for the report and TC1995 for pointing at the data-available bit.
```
