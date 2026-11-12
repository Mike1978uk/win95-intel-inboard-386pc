# Monster Truck Madness trial in a bed, 2026-11-12

Bed copy `vm_5160_mtm` (from `vm_5160_mcp`, uuid line removed, 5 MB, Mach8, Win95 4.00.950 with DirectX 7.0a).
Run by `tools/bed_launch.ps1`, reached at 127.0.0.1:9869. The card and `vm_5160_mcp` were not touched.
Bed speed is not meaningful (the bed misprices port I/O).

## The package

`mtmtrial.exe`, 12,229,632 bytes: Wextract 4.70 cabinet self-extractor, one cabinet, 169 files (sizes in the cabinet).
Extracted on the host with 7-Zip only; never run on the host. Driven by `MTMTRIAL.INF` (AdvancedINF 2.0, `DefaultInstall`).

| Group | Files |
|---|---|
| Game, 2.4 MB | `MONSTERX.EXE` 2,461,696; `MONSTERX.HLP/CNT`, `ACCESSIB.HLP`, `MTPSS.HLP`, `README.TXT`, `SMACKW32.DLL`, `COMIC.TTF` |
| Data, 21 MB | `GAMEX.POD` 3.8 MB, `STARTUPX.POD` 5.8 MB, `TRUCKX.POD` 3.8 MB, `UIX.POD` 7.7 MB, `MONSTERX.INI`, `MONSTERX.INF` (sound buffer settings) |
| Network | `MPSERIAL/MPSOCKET/MPWSOCK.DLL`, renamed on install to `DPSERIAL/DPSOCKET/DPWSOCK.DLL` in the game's `SYSTEM` folder |
| DirectX redist | `DXINST.EXE`, `DXSETUP.EXE`, `DSETUP*.DLL`, `DIRECTX.INF`, `DDRAW`, `D3DIM`, `D3DRM`, `DSOUND`, `DPLAY`, `DPSERIAL`, `DPWSOCK`, `DDHELP`, `D3DHALF`, `D3DRGBF`, `D3DRAMPF`, `D3DRM8/16/24/32F`, `D3DRG8/16/24/32F`, `D3DXOF`, `DSOUND.VXD`, `DDRAW16`, `VJOYD.VXD`, plus about 80 OEM display and audio driver files (`ATI*`, `S3*`, `CIRRUS*`, `MGA*`, `TSENG*`, `ES*`, `SB*`, `AZT*`...) |
| Other | `JOY.CPL`, `W95INF16/32.DLL`, `ADVPACK.DLL`, `LICENSE.TXT`, `MONSTER.TXT` |

DirectX version: every DirectX DLL reads **4.03.00.1096** (DirectX 3 era, dated 1996-05/06). The dialog and README call it DirectX 2.
Bed has 4.07.00.0716 (`HKLM\Software\Microsoft\DirectX\Version`), DLLs dated 1999-09-09, with the project's own `DDRAW.DLL` (352,528) and `D3DIM.DLL` (625,936).

## What setup does

Run in the bed as `rundll32 ADVPACK.DLL,LaunchINFSection MTMTRIAL.INF,DefaultInstall,4`. The real Wextract command line was not
confirmed (its resources were not readable); the INF is the same. Wextract's own extraction was killed by the agent timeout at
136 of 169 files (guest at 83 MHz), so the extracted files were pushed from the host.

Dialogs seen, in order:
1. "Where would you like to copy the Monster Truck Madness Trial?" default `C:\Monster Truck Madness Trial`; "folder does not exist, create?" Yes.
2. Copy progress, then Version Conflict on `joy.cpl` (bed 4.7.700 is newer): kept the bed's file.
3. "Installing Microsoft(R) DirectX(tm) 2", "Version checking directx\<file>": `DXINST.EXE`, run by `RunPostSetupCommands`. **No choice offered**; it checks versions per file.
4. "You must restart your computer": No, then restarted by hand later.

Writes:
- `C:\Monster Truck Madness Trial\` (8 files) and `\SYSTEM\` (9 files: 3 `.POD`+`UIX.POD`, `MONSTERX.INF/.INI`, 3 DP DLLs). 25.0 MB.
- `HKLM\Software\Microsoft\Games\Monster Truck Madness Trial Version`, `InstallDir` = `C:\MONSTE~1\`.
- `C:\WINDOWS\SETUP.INI` progman groups: `MSGames` / `MonsterX` links to `MonsterX.exe` and `ReadMe.txt`.
- `HKLM\...\CurrentVersion` gained only `GrpConv=grpconv.exe -o`.
- `HKLM\Software\Microsoft\DirectX` **unchanged** (Version 4.07.00.0716). No MSAGENT keys.
- `C:\WINDOWS\SYSTEM` (`WININIT.INI` renames, applied at the restart): `D3DRAMPF`, `D3DRGBF`, `D3DRM8F/16F/24F/32F`, `D3DRG8F/16F/24F/32F`,
  `D3DHALF` DLLs (new; DirectX 7 has none) and `MSOPL.DRV`, `MSSBLST.DRV/.VXD` renamed to `.001/.002` (sound drivers swapped).
- Not replaced: `DDRAW`, `D3DIM`, `D3DRM`, `DSOUND`, `DPLAY`, `DDHELP`, `DSOUND.VXD`, `DDRAW16`. The DirectX 3 installer left the newer files alone.
- Not run: the Compact/CD options; this is a trial with no CD.

Before and after lists: `C:\MTMREC\*` in the bed disk image (`vm_5160_mtm\card.img`).

## What the game says

All from `MONSTERX.EXE`, not from setup:
1. At 16 bpp and 8 bpp alike, on start: **"Low Memory" - "Monster Truck Madness may not function properly without 24MB of available physical and/or virtual memory. Please close unused applications to free additional memory."** OK / Help. The bed has 5 MB RAM. OK continues.
2. At 16 bpp only, next: **"Your display is currently set to more than 256 colors. This will cause a dramatic frame rate slowdown unless you are using a 3D hardware accelerator. Please click the help button for instructions on how to change the display to 256 colors."** "Don't warn me again" (INI `showColorWarningFlag`, `dontShowColorWarning`). OK continues to a black screen (game mode change, never reached a picture in 170 s).
3. At 8 bpp 800x600 (copy set by `MTM8.REG`: `BitsPerPixel=8`, `Resolution=800,600`, then restart): no colour warning; the title screen ("Microsoft Monster Truck Madness") draws, menu bar Race / View / Options / Help. `Race` menu: New Race F2, End Race Esc, Pause F3, Save/Resume, Hall of Fame, Exit.

So the 24 MB text is a warning, not a refusal, at 5 MB. The 24 MB RAM run asked for was not made: the warning is non-fatal.
Not yet known: whether a race starts and runs in 5 MB.

## Memory check

`MONSTERX.EXE` imports `GlobalMemoryStatus` (KERNEL32). Strings: "Low on memory.  Turning off music.", "Low on memory...",
"No room for frame buffer.  Please run SETUP", "Out of memory.", "Memory Write BW : %f MB/Sec" (a speed probe at start).
Imports `DirectDrawCreate`, `DirectDrawEnumerateA`, DDRAW.DLL; also `WING32.DLL`, `DISPDIB.DLL`, `SMACKW32.DLL`, `winmm`. It does not import `D3DIM` by link;
string "required Direct3D features:" and INI `useDirect3D=0`, `maxD3DTextures=240`.

## Graphics path

From `MONSTERX.INI` (trial default) and the exe: `useDirect3D=0` selects the game's own software renderer on DirectDraw surfaces (palettised 8 bpp
primary; the 256-colour warning is its own). `useDirect3D=1` uses Direct3D (HAL if the driver has one, else the `D3D*F` software drivers the
installer adds). `autoMinFrameRate=8`, `autoQuality=0`, `ditherFlag=1`, `filterFlag=1`, `textureResolution=1`, `gamePIXX=320`, `gamePIXY=400`, `pixelDoubleBlit=0`, `localBusVideo=1`,
`autoFullScreen=1`, `useZFlag=1`. Unconfirmed in the bed which path ran after the title screen.

## Demo (not resolved)

Strings in the exe: `showDemoFlag` (INI `[Game]`, default 0), `demo.sit`, "Load which demo :", "Save demo (CR to abort) :", "Unable to load demo", `demoRecordPtr`, `demoLevel`.
No command-line switch found by string search. The demo is a recorded input file (`demo.sit`), not in the trial's files.
Unattended launch, frame-rate overlay and its key, run time: **not found.** Next step: open the Options/Help menus and the title screen's buttons in the bed.

## Files to put on the card without running setup

Needed (same as the setup's writes, minus DirectX):
- `C:\Monster Truck Madness Trial\` with `MONSTERX.EXE`, `*.HLP/CNT`, `README.TXT`, `SMACKW32.DLL`, `COMIC.TTF`.
- `...\SYSTEM\` with the four `.POD`, `MONSTERX.INI`, `MONSTERX.INF`, `DPSERIAL/DPSOCKET/DPWSOCK.DLL`.
- Registry: `HKLM\Software\Microsoft\Games\Monster Truck Madness Trial Version` `InstallDir`. The game's own key `HKLM\SOFTWARE\Microsoft\Monster Truck Madness\1.0` is created by the game.
- Set `MONSTERX.INI` `dontShowColorWarning=1`, desktop 8 bpp, for a clean start.
- DirectX: the card has DirectX 7.0a (same as the bed), so the `D3D*F` DLLs were the only additions; copy them to `C:\WINDOWS\SYSTEM` only if `useDirect3D=1` is tried.

State left: bed `vm_5160_mtm` still running (PID 12676), game open at the title screen, desktop 8 bpp. Delete `vm_5160_mtm` (5.9 GB) when done.
