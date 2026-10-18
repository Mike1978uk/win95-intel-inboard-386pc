# 86box-mcp on native Windows (2026-10-18)

[86box-mcp](https://github.com/dafugg/86box-mcp) (dafugg, GPL-2.0) is an MCP server that drives 86Box
through its automation socket: keys, mouse, screenshots, the text screen, memory, registers, and
"held" emulated time. Its README lists Linux as the documented host; the 86Box side was already known to
work on Windows over TCP. This note records running the **server itself natively on Windows 11**, without
WSL, and what that needed.

## Result

Works natively on Windows, with a two-spot patch to `server/box86mcp/vm.py`:
https://github.com/Mike1978uk/86box-mcp/tree/windows-local (commit `fb36524`, on upstream `4fc9bd5`).

Smoke test with the server's own `VM` class on a Windows 3.0 / ASUS ISA-386C / Mach8 VM, headless and
held:

| Step | Result |
|---|---|
| `start(held=True)` | automation socket on `tcp:127.0.0.1`, protocol 1, 86Box 7.0, 0.5 s |
| `info` | machine, CPU, RAM, video card and disk geometry reported |
| `step(3000/10000/20000)` | emulated clock 3 s, 13 s, 33 s exactly (194, 895, 2297 frames) |
| `text()` | BIOS summary box, HIMEM, SMARTDrive, `C:\>` prompt |
| `screenshot()` | 720x400 PNG |
| `memr(0x400, 16)` | BIOS data area: COM 3F8/2F8, LPT 378 |
| `regs()` | CPU registers |
| `stop()` | 86Box exits, temp files removed |

Then registered with Claude Code (`claude mcp list` reports it connected).

## What the patch changes

1. `stop()`: the fallback for an 86Box that does not quit uses `os.killpg` and `SIGKILL`, which Windows does
   not have. On Windows it now kills the guard process; the guard's stdin closing already stops 86Box.
2. `start(window=True)`: required `DISPLAY` and forced Qt's `xcb` platform. On Windows it now leaves Qt's
   native platform, so the normal 86Box window shows. Headless (`offscreen`) works unchanged.

Nothing else was Linux-only in the server: the guard (`guard.py`) and the TCP transport with a token file
work as written.

## Setup

1. Build 86Box with the automation socket (MSYS2 MinGW64, configured with `MSYSTEM=MINGW64`):
   `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DQT=ON -DAUTOMATION=ON -DCMAKE_EXE_LINKER_FLAGS=-mwindows`.
   Master `8c7e50cc8` (2026-10-18) has it. Add `-DGDBSTUB=ON` only for the debugger tools; that build turns
   the dynarec off and uses a console window.
2. Clone the fork's `windows-local` branch, then `py -3.12 -m venv server\.venv` and
   `server\.venv\Scripts\python -m pip install -e server`.
3. Register it (one line):
   `claude mcp add 86box -s local -e BOX86_BINARY=<path to 86Box.exe> -e BOX86_TRANSPORT=tcp -e PYTHONIOENCODING=utf-8 -e "PATH=C:\msys64\mingw64\bin;C:\Windows\System32;C:\Windows" -- <clone>\server\.venv\Scripts\box86mcp.exe`
   - `PATH` must include `C:\msys64\mingw64\bin`, or a MinGW-built 86Box cannot find its Qt DLLs.
   - `PYTHONIOENCODING=utf-8`: the text screen contains CP437 box drawing, which a cp1252 console cannot print.

## Not tested yet

- `window=True` on Windows, and the `stop()` kill fallback (only the normal `quit` path ran).
- The MCP tools called through Claude Code; the smoke test drove the `VM` class directly.
- The debugger tools (`GDBSTUB=ON` build) and the DOS helpers (need `mtools`).

## Notes

- `start()` rewrites the VM's `86box.cfg` without its `uuid` line (written back with LF endings). Run it on a
  copy of a VM you care about.
- Cacodemon345 suggested WSL2 on the 86Box Discord; WSL2 would need more changes, because the server
  launches 86Box itself and passes it a token-file path under the Linux temp folder, which a Windows
  `86Box.exe` cannot read.
