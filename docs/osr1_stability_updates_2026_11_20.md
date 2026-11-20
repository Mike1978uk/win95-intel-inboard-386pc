# Windows 95 stability updates for OSR1 / 950a, 2026-11-20

## Summary

- The card is 4.00.950a, which is retail 950 plus Service Pack 1 (or OSR1). SP1 is already in.
- No Microsoft update found fixes an Explorer start-up hang, a dead Start menu or a shutdown hang.
- Only candidate worth testing: OLE32 Update (405 KB, one file, about 558 KB on disk). Note: SP1 already ships an OLE32 fix; whether this is a later one is not shown on the page.
- Do not install the Y2K update or Winsock 2 update for stability: they replace VMM-level files (VDHCP, VTCP stack) and add RAM and disk cost.
- First check on the card: `QFECHECK.EXE` and the Version tab of OLE32.DLL, COMCTL32.DLL, KERNEL32.DLL, to see what is already updated.

## Which build the repo says

| Where | What it says |
|---|---|
| `docs/captures/2026-11-05_baselines/README.md:3` | "Real 5160 ... Windows 95 4.00.950 a" |
| `docs/scsi_miniport_costing.md:134` | "this guest is OSR1 build 950" |
| `docs/win95_boot_fix_inventory.md:9` | "Windows 95 (OSR1)" |
| `docs/mtm_bed_setup_2026_11_12.md:3` | bed "Win95 4.00.950 with DirectX 7.0a" |
| memory `xt_next_stage_plan_2026_08_04.md:29` | "Windows 95 A / OSR1 (4.00.950A, Feb 1996)" |
| memory `vxd_patch_version_mismatch_2026_08_02.md:137` | actual stock is 4.00.950 retail |

"Service Pack" is not recorded for the card; the first match is 950a, which is SP1 level. Not recorded: whether any later QFE (OLE32, Kernel32 socket, comctl32, Y2K) is installed. DirectX 7.0a is on the bed, which replaces DirectX files only.

## Updates

Status key: read = page fetched, snippet = search result only. All apply to 950/950a, none to 950B/OSR2.

| Update | Files | KB | Stated fix | Size | Builds | Obtainable | Status |
|---|---|---|---|---|---|---|---|
| Service Pack 1 | SP1 package; contents below | Admin.doc is Q146237 | "Includes all of the fixes that were previously posted online" | 1,265 KB | 950 to 950a | WinWorld list has "Windows 95 Service Pack 1", 1.2 MB | read |
| SP1: OLE32 | Ole32.dll 558,704 B, 12-31-95 | Q148752 (tool note) | Stops Office files keeping "extraneous data from previously deleted files" | in SP1 | 950 | in SP1 | read |
| SP1: Shell32 | Shell32.dll | none | file copied onto itself "can be truncated to a zero-byte file size"; NDS printer browse | in SP1 | 950 | in SP1 | read |
| SP1: Vredir, Vserver, NWServer | VxDs | none | Samba / share security; wildcard delete on Samba | in SP1 | 950 | in SP1 | read |
| SP1: password list, Sage, Exchange, Lpt.vxd | Net.exe, Sage.dll, Exchupd.exe, Lpt.vxd | Q148752 for Net/Sage | encryption; Plus! float precision; ECP port | in SP1 | 950 | in SP1 | read |
| OLE 32 Update | W95OLEUPD.EXE / oleupd.exe | not on page | "shutting down the applications in a certain order may cause Windows 95 to crash" (16-bit ODBC); links kept across drives; no stale data in OLE docs | 405 KB | 950/950a; page says skip on 950B | WinWorld has DCOM95 OLE update (1.18 MB, different package); the 405 KB file not located | read |
| Windows Socket Update - Kernel 32 | w95krnlupd.exe / krnlupd.exe | not on page | "very small memory leak" on opening and closing sockets; resource drain, growing swapfile | 337 KB | 950/950a, not OSR2 | not located | read |
| Windows Socket 2 Update | W95Sockets2 | not on page | "Resolves Winsock 2 issues and updates the TCP/IP stack" | 986 KB | 950/950a | WinWorld "Winsock 2 Update" 923.88 KB | snippet (index only) |
| Windows Library Update | SPEU.exe | likely Q197298 (inferred) | "resolves an issue that can cause some third party software to behave unexpectedly" | not listed | not stated | not located | read |
| Windows 95 Backup Update | not seen | none | backup performance | 449 KB | 950 | not located | snippet (index only) |
| AMD-K6-2/350 update | not seen | Q192841 is the related article | timing loop sensitive to CPU frequency (IOS protection error) | not listed | 950 | WinWorld "AMDK6 Update" 245.79 KB | snippet |
| Year 2000 Update (+ Corporate companion) | w95y2k.exe, corpupd.exe; replaces command.com, VDHCP.386 and "some system files" | hotfix 172707 mentioned | leap year, date/time settings, call logging | 10 MB free disk needed; IE 4.01 SP2 "necessary for some systems" (about 72 MB) | 950 and OSR2 | WinWorld OEM ISO 62.7 MB | read |
| Fast CPU fixes (third party, not Microsoft) | Fix95CPU | none | Win95 fails on very fast CPUs | 2.94 MB | all | WinWorld "Win95 Fast CPU Fixes" | snippet; irrelevant at 83 MHz |
| COMCTL32 leak fix (third party) | comctl32.dll 5.00.0910.1306 | none | claimed no resource leak | 307 KB on WinWorld | all | WinWorld "Win95 Comctl Leak Fix" | snippet |

## Relevance to the symptoms

| Symptom | Documented fix |
|---|---|
| Explorer stops at wallpaper, no taskbar | None documented. SP1 Shell32 fix is about file copy. |
| Start menu not responding | None documented. A KB search found only that a mismatched comctl32.dll (an NT copy over the Win95 one) caused Explorer load errors, and newer comctl32 4.70 (5/19/97) fixes a different issue. |
| Low memory, 5 MB | Kernel32 socket update (a leak, only with sockets in use). OSR2 notes mention only "Faster MailTo on 8-MB computers". Nothing for 5 MB specifically. |
| File system / cache | OSR2 adds FAT32 and "Updated Vredir.vxd"; no VFAT or VCACHE bug fix is listed in Q155003. Nothing separate for 950a found. |
| Shutdown hang | Q145926 is troubleshooting only (bad drivers, exit sound, Startup/Run items; use BOOTLOG.TXT). OLE32 Update mentions a crash on shutting down 16-bit ODBC apps in a certain order; not a general fix. |

## Risks with our custom files

- No update page lists the files it replaces, except SP1 (Ole32.dll, Net.exe, Sage.dll, Shell32.dll, Lpt.vxd, Vredir, Vserver, NWServer) and Y2K (command.com, VDHCP.386). None named IOS, VFAT, VCACHE, VMM32 or VPICD/VKD/VDMAD, so no documented clash with `vxd-patches/osr1/`. Not verified: file lists inside the packages were not unpacked.
- Y2K and Winsock 2 add or replace network VxDs. The NIC and IRQ 9 route (VPICD) are already sensitive; do not add them for this goal.
- Lpt.vxd from SP1 is "not installed by default"; do not enable it, the T130B and LPT work depends on the stock driver.
- Updates carry QFE version stamps; any VxD they replace must be re-diffed against `vxd-patches/osr1/` before use. Take a copy of `C:\WINDOWS\SYSTEM` first. Uninstall is not available for Corpupd; OLE32 and Kernel32 updates do not state it.
- Every installer wants disk and a reboot; 5 MB RAM is below what the Y2K update's IE prerequisite (IE 4.01 SP2) comfortably needs.

## Dead ends

- Microsoft never published a stability list for 950a. The Q161020 page gives versions and QFECHECK only, no update list.
- No KB article found tying Explorer start-up or Start menu hangs to a 950/950a update.
- Q155003 (OSR2 description) is a feature list, not a bug list.
- hpcfactor.com patch guide: bot-check page, not readable.
- toastytech.com comctl32 page: redirect loop between http and https, not read.
- Microsoft download pages for each update are archived copies only; installers for OLE32 405 KB and Kernel32 337 KB not located.

## Sources

- https://learn.microsoft.com/en-us/previous-versions//cc751433(v=technet.10) : SP1 contents; SP1 only lists drivers and tools, not per-fix detail. Read.
- https://ftp.zx.net.nz/pub/archive/ftp.microsoft.com/MISC/KB/en-us/146/237.HTM : SP1 Admin.doc (Q146237), the ten fixes quoted above. Read.
- https://jeffpar.github.io/kbarchive/kb/148/Q148752 : Ole32.dll, Net.exe, Sage.dll sizes and dates in SP1. Read.
- https://www.zx.net.nz/mirror/www.microsoft.com/windows95/downloads/index.html : mirror of the Microsoft download index, all updates with sizes. Read.
- https://www.zx.net.nz/mirror/www.microsoft.com/windows95/downloads/contents/WURecommended/S_WUServicePacks/W95OLE32Update/Default.html : OLE32 Update text. Read.
- https://www.zx.net.nz/mirror/www.microsoft.com/windows95/downloads/contents/WUAdminTools/S_WUNetworkingTools/W95Kernel32/Default.html : Kernel32 socket leak update. Read.
- https://www.zx.net.nz/mirror/www.microsoft.com/windows95/downloads/contents/WURecommended/S_WUServicePacks/MFCLibrary/Default.html : Library update. Read.
- https://www.zx.net.nz/mirror/www.microsoft.com/windows95/downloads/contents/WURecommended/S_WUFeatured/win95y2k/Default.html : Y2K update requirements. Read.
- https://learn.microsoft.com/en-us/previous-versions//cc751501(v=technet.10) : Y2K Corporate Update readme (VDHCP.386, command.com, hotfix 172707). Read.
- https://techshelps.github.io/MSDN/KB/Source/win95x/q161020.htm : version numbers (950 / 950 SR1 / 1111), QFECHECK, no update list. Read.
- https://techshelps.github.io/MSDN/KB/Source/win95x/q155003.htm : OSR2 feature list, no bug fixes. Read.
- https://winworldpc.com/product/windows-95/patches : where installers are (SP1, Winsock 2, DCOM95 OLE, AMDK6, comctl leak, Y2K ISO). Read; downloads not tested.
- https://techshelps.github.io/MSDN/KB/Source/win95x/q145926.htm : shutdown troubleshooting (Q145926). Snippet.
- https://ftp.zx.net.nz/pub/archive/ftp.microsoft.com/MISC/KB/en-us/192/841.HTM : AMD K6-2 IOS error (Q192841). Snippet.
