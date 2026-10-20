# Host tools

Tools installed on the development PC and what each is used for. The owner is happy for a
better tool to be installed when a job needs one; add it here when it is.

| Tool | Where | Used for | Limits |
|---|---|---|---|
| Open Watcom v2 snapshot | `C:/Users/lycet/tools/openwatcom` | 16-bit NE display driver: `wlink`, `wrc`, `ms2wlink` | installed 2026-10-12; velocity9x uses it; spike `drivers/mach8_display/spike/` |
| Win95 DDK parts in Win98 DDK layout | `C:/Users/lycet/tools/ddk95_as98` | velocity9x `-DdkRoot`: ML, VxD LINK, INC32, DIBENG.LIB copied from `XT_project/Windows95_ddk`; Open Watcom `lib386` extracted 2026-10-16 for host tests | - |
| NASM | `C:\msys64\mingw64\bin\nasm.exe` | assembling DOS `.COM` probes (`tools/m8eedump/`) | - |
| Capstone 5 (Python) | pythoncore `pip` | 16/32-bit disassembly of ROMs, `.COM`, drivers | - |
| `tools/pedis.py`, `tools/vxd_disasm.py`, `tools/sysdis.py` | repo | PE miniports, LE VxDs (`CD 20` services), DOS `.SYS` | technique 112 |
| MASM 6.11c + DDK LINK | `Windows95_ddk\` | VxDs and miniports | `custom_vkd/build.ps1` |
| 7-Zip | `C:\Program Files\7-Zip\7z.exe` | ZIP, most LZH, 7z | no LHarc `-lh1-` |
| WinZip | `C:\Program Files\WinZip\winzip64.exe` | LHarc `-lh1-` packs (ATI 1994 drivers) | GUI only; no command line installed |
| MSYS2 MinGW64 | `C:\msys64\mingw64\bin` | 86Box builds, `objdump` | must be on `PATH` for `cmake --build` |
| COMrade (`comrade` MCP) | COM2 to the 5160 | port and memory reads, file transfer, running probes | DOS agent only for ports and memory |
| 86box-mcp (`86box` MCP), dafugg, GPL-2.0 | `C:/Users/lycet/RiderProjects/86box-mcp`, branch `windows-local` = upstream `4fc9bd5` + `fb36524` (Windows window/stop patch); venv `server/.venv` (Python 3.12) | drive an 86Box VM: keys, mouse, screenshot, text screen, memory, registers; held time makes runs repeatable | installed 2026-10-18; needs the `AUTOMATION=ON` build `86box_auto/` (master `5468d5952`, rebuilt 2026-10-18; since 10-20 branch `auto-irq9` adds `ef082884b`, IRQ 9 as IRQ 2 on the XT, which the beds' 3C509B needs); headless unless `window=True`; rewrites the bed's `86box.cfg` without `uuid`; debugger tools need a `GDBSTUB=ON` build (dynarec off). Early project: check upstream for changes before relying on a tool; the `wait text` stall is fixed by 86Box #8396, retested on `86box_auto`; `vm_stop` sometimes hits "Access is denied" on `automation.token` (86Box has exited; delete the temp folder) |
| V9x Remote Agent 0.12.0 (`v9x` MCP), Michael Dale, BSD-2 | release (SHA256SUMS checked) in `C:/Users/lycet/tools/v9x-remote-agent`; MCP `host/v9xremote-0.12.0/mcp/v9x_mcp.py` (Python 3.12, `--host 127.0.0.1`); guest agent in `vm_5160_mcp` `C:\V9XREMOTE` | Win9x guest over TCP: exec, shell, files, screenshot, input; no memory, port or register access | installed 2026-10-20, https://github.com/michaeldale/V9x-Remote-Agent; unauthenticated, so the SLiRP forward of 9869 must be blocked from the LAN; `INSTALL.BAT` needs `UNINSTALL.BAT` (not 8.3), so copied by hand; needs TCP/IP in the guest |
| `pyfatfs` 1.x (+ `setuptools<81` for `pkg_resources`) | pip, Python 3.12 | write files into a bed's FAT disk image with the VM stopped (`PyFatFS(img, offset=63*512)`) | write new files only: overwriting an existing one corrupted the chain on 2026-10-12; back up the image first |

Missing, worth having if a job needs it: an `-lh1-` command-line extractor (`lha` or `lhasa`),
so LZH packs need no hand step.
