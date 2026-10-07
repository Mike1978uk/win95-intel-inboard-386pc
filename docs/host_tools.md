# Host tools

Tools installed on the development PC and what each is used for. The owner is happy for a
better tool to be installed when a job needs one; add it here when it is.

| Tool | Where | Used for | Limits |
|---|---|---|---|
| NASM | `C:\msys64\mingw64\bin\nasm.exe` | assembling DOS `.COM` probes (`tools/m8eedump/`) | - |
| Capstone 5 (Python) | pythoncore `pip` | 16/32-bit disassembly of ROMs, `.COM`, drivers | - |
| `tools/pedis.py`, `tools/vxd_disasm.py`, `tools/sysdis.py` | repo | PE miniports, LE VxDs (`CD 20` services), DOS `.SYS` | technique 112 |
| MASM 6.11c + DDK LINK | `Windows95_ddk\` | VxDs and miniports | `custom_vkd/build.ps1` |
| 7-Zip | `C:\Program Files\7-Zip\7z.exe` | ZIP, most LZH, 7z | no LHarc `-lh1-` |
| WinZip | `C:\Program Files\WinZip\winzip64.exe` | LHarc `-lh1-` packs (ATI 1994 drivers) | GUI only; no command line installed |
| MSYS2 MinGW64 | `C:\msys64\mingw64\bin` | 86Box builds, `objdump` | must be on `PATH` for `cmake --build` |
| COMrade (`comrade` MCP) | COM2 to the 5160 | port and memory reads, file transfer, running probes | DOS agent only for ports and memory |

Missing, worth having if a job needs it: an `-lh1-` command-line extractor (`lha` or `lhasa`),
so LZH packs need no hand step.
