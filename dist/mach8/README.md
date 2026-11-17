# Mach8 display driver for Windows 95 (with DirectDraw and Direct3D)

A Windows 95 display driver for the **ATI Graphics Ultra (Mach8, 38800)**, written for an IBM 5160 with an
Intel Inboard 386/PC, where every byte crosses an 8-bit ISA bus. It draws with the Mach8's 8514/A engine
and adds a DirectDraw and Direct3D HAL, which ATI's own Windows 95 driver does not have.

**Status: preview.** These are the exact files running on the project's 5160. Read "Known problems" first.

## Files

| File | What it is | MD5 |
|---|---|---|
| `M8DISP.DRV` | the display driver (16-bit) | `3BE3D08810007C18D8144B4EC66CEF3C` |
| `M8DD32.DLL` | DirectDraw / Direct3D HAL (32-bit), loaded by the driver | `A3B101749F4BAAFCC15DA9354EF12608` |
| `M8DISP.REG` | selects `m8disp.drv` and adds the 16-bit colour modes | |
| `ATIM8.REG` | puts ATI's driver back | |
| `M8D3D.INI` | optional settings for DirectX programs | |

Built from commit `32bfaac` of `drivers/mach8_display/src` (`build.ps1`, Open Watcom). Source licence
GPL-3.0-or-later; the driver frame is from boxv9x / vmdisp9x (MIT, `drivers/mach8_display/src/LICENSE-MIT.txt`).

## Requirements

- An ATI Graphics Ultra (Mach8). The 8514/ULTRA add-on uses the same chip but has **not** been tried.
- Windows 95, with Windows' own **"ATI Graphics Ultra (mach8)"** display driver already installed
  (Control Panel, Display, Settings, Change Display Type; it comes from `msdisp.inf` on the Windows CD).
  This driver replaces only ATI's `.drv` file and keeps ATI's `ati.vxd`.
- For DirectDraw and Direct3D: DirectX installed. Tested with DirectX 7.0a only.
- A monitor that takes 800x600.

## Install

1. Copy `M8DISP.DRV` and `M8DD32.DLL` to `C:\WINDOWS\SYSTEM`.
2. Double-click `M8DISP.REG` and answer Yes.
3. Restart Windows.
4. Optional: Display Properties, Settings: **High Color (16 bit)** at 640x480 or 800x600, then restart again.
   Windows 95 asks for a restart on every colour-depth change.
5. Optional: copy `M8D3D.INI` to `C:\`.

## Back to ATI's driver

From Windows: double-click `ATIM8.REG`, restart. If Windows will not start: boot to the command prompt
(F8, "Command prompt only"), copy `ATIM8.REG` to `C:\` and run `REGEDIT C:\ATIM8.REG`, then restart.

## What it does

- **Modes:** 256 colours at 640x480, 800x600 and 1024x768 drawn on the card; 16-bit colour at 640x480 and
  800x600, kept in system RAM and sent to the card as 256 colours (the Mach8 has no 16-bit mode).
- **GDI:** fills, copies, lines and text on the Mach8 engine; only what changed crosses the bus.
- **DirectDraw:** full-screen 256-colour programs, with two-page flipping at 640x480.
- **Direct3D:** triangles drawn by the card's engine, not the CPU. Settings per program in `C:\M8D3D.INI`.

## Known problems

- **Blue screen after a full-screen program changes colour depth** (`fatal exception 0E at 0137:BFF9A07C`),
  usually when the *next* program starts. Workaround: set the desktop to the program's mode first (for
  example 640x480, 256 colours, for most DirectDraw samples), so no mode change happens.
- Programs that draw their own pixels through Lock (the DirectX SDK's flip2d, for example) run at under
  1 frame a second on an 8-bit bus.
- DirectDraw overlays and the faster 8 bpp Lock path are in later builds, not in this one.
- Tested on one machine: IBM 5160, Inboard 386/PC with a 486 Blue Lightning at 83 MHz, 5 MB, and in 86Box.
  Not tried on any other PC or card.

## For the project

Update this folder only with files that have run on the 5160, with their MD5 and source commit, in the same
commit as the change to this README. Work in progress: `docs/worklist.md` (sections D and X).
