# DOS agent: capability plan for DOS 3D measurement (X28), 2026-11-18

Owner brief: the DOS agent runs programs, gives probe access and live performance visibility (fps) for the DOS 3D
engine and the titles we test, can be switched off for manual runs, is network-driven, and covers what COMrade does.
Design background: `docs/dos_network_agent_design_2026_11_18.md`. Exec fixed: `docs/dos_agent_milestone1_2026_11_18.md`.

## Where it stands

| Capability | State |
|---|---|
| ping, info, files in/out (CRC) | works (bed) |
| run a program, output back | works (bed): `exec`/`shell` with the agent resident (464 K free); `-Detach` hands over: the agent exits, `RUNJOB.BAT` runs with 563 K free, the agent restarts (C1, `dev-dos-c1b`) |
| memory / port read-write | not built |
| screen, keys, reboot | not built (86Box automation covers the bed only) |
| live view while a program runs | not built: the agent is a foreground program |
| fps and frame counters | `m8dos` keeps counters, printed and appended to `M8DTEST.TXT` at exit only |
| off switch | **done (C1)**: `V9XLOOP.BAT`; flag file `C:\V9XREMOT\OFF` or N at the boot prompt |
| 5160 | not deployed |

## Build order

| Step | Work | Who | Done when |
|---|---|---|---|
| C1 | **Off switch and full-memory run.** AUTOEXEC calls `V9XLOOP.BAT`: skips the agent if `C:\V9XREMOT\OFF` exists or a key is held at boot; otherwise loops agent -> job -> agent. New exec flag "handoff": the agent writes `RUNJOB.BAT`, replies, exits with errorlevel 3; the loop runs the job with output to `RUNJOB.OUT`, restarts the agent, the host collects the output. Verbs `off`/`on` write or delete the flag | Opus | bed: handoff run gets 564 K (`MEM`), output collected; `OFF` boot leaves the agent unloaded |
| C1b | Smaller resident agent: Watt-32 receive ring 40 -> 10 buffers (~46 KB less below 640 K) | Opus | `MEM /C` before/after; 1 MB put/get still CRC-clean |
| C2 | **Probes in the foreground agent:** MEM_READ/WRITE (20-bit linear), IO_IN/OUT (byte/word), SCREEN_GET (B800 + cursor), KEYS (BIOS buffer), REBOOT; `v9xctl` verbs; capability bit HW_ACCESS | Opus | same reads as COMrade byte for byte on the bed (BDA 0040:0000, port 21h) |
| C3 | **Perf mailbox in `m8dos`:** a block in DOS memory with a signature, frame count, last and average frame time (PIT, microseconds), port writes, status polls, published through a free interrupt vector; `PERF.CSV` (one row per second) at exit | Opus | bed: `M8DTST16 bench` row count = seconds run; mailbox read from DEBUG matches the printout |
| C4 | **Live tail (resident, UDP):** small TSR that owns the packet-driver handle only while the foreground agent is out (handover through its own interrupt), answers MEM/IO/SCREEN/KEYS (incl. raw 8042)/REBOOT/MAILBOX over UDP 9870 from a timer tick, never from the upcall; private stack; no DOS calls. Host: `v9xctl live-*` verbs and a `fps` poll that prints the mailbox once a second | Opus; Fable review optional (owner's OK) | bed: fps read live during `M8DTST16 bench`, 1000-request soak with no hang; resident size measured |
| C5 | 5160 deployment (owner steps below) | owner | ping, put/get, run, `MEM` on the card |
| C6 | Watt-32 upstream report of the Tx overrun | Opus drafts, owner posts | - |

COMrade stays on COM1 as the fallback; serial and network do not share a port.

## Owner steps for the 5160 (C5)

1. Copy to `C:\V9XREMOT\`: `V9XAGNT.EXE`, `DOS4GW.EXE`, `AGENT.INI`, `WATTCP.CFG` from
   `V9x-Remote-Agent\build\dos-install\`; make `C:\V9XREMOT\TEMP`. Packet driver `3C5X9PD.COM`.
2. `WATTCP.CFG`: the card's LAN address (static or `my_ip = dhcp`). `AGENT.INI`: `allowed_client=` the host's address.
3. A DOS-only boot entry (owner edits CONFIG.SYS/AUTOEXEC.BAT): `3C5X9PD 0x60`, `PATH C:\V9XREMOT;...`,
   `SET WATTCP.CFG=C:\V9XREMOT`, then `V9XLOOP.BAT` (after C1) or `V9XAGNT.EXE`.
4. Open: DOS/4GW's extended memory under the 1986 BIOS. If it fails, `DOS32A V9XAGNT.EXE`.

## C1 result (2026-11-18, night)

V9x branch `inboard-dos-handoff` (local). The protocol's existing detach option means handoff on DOS: the agent
writes `C:\V9XREMOT\RUNJOB.BAT` (no redirect: the program keeps the screen; add `> file` for output), replies
`Detached`, exits with errorlevel 3. `packaging/dos/V9XLOOP.BAT` (last line of AUTOEXEC) runs the job and restarts
the agent. Bed `vm_dosagent`: `shell -Detach "MEM /C > C:\V9XREMOT\RUNJOB.OUT"` -> largest program 563 K, agent back
(boot 5 -> 6, 7 -> 8). `shell -Detach "ECHO off> C:\V9XREMOT\OFF"` -> loop ends at `C:\>`; on again: delete the flag at
the machine and run `V9XLOOP`. Exit code of a detached job is not reported (DOS gives it only to the parent).
