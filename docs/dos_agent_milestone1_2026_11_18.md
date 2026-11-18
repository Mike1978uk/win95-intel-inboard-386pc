# DOS agent, X28 milestone 1 (2026-11-18)

Design and plan: `docs/dos_network_agent_design_2026_11_18.md` (build plan row 1). Agent source:
`V9x-Remote-Agent/src/guest-dos` at `08dd0cf` (branch `inboard-tray-flash`), unchanged. No commits in either repo.

## Result

| Check | Result |
|---|---|
| Build | `V9XAGNT.EXE` 699,960 bytes, build id `dev-dos-x28`, against Watt-32 head `3cf6f4e` built here (`docs/host_tools.md`). `build-guest-dos.ps1` must run under Windows PowerShell 5.1: under pwsh 7 the `-dV9X_BUILD_ID="..."` argument reaches `wcc386` split ("more than one file to compile") |
| Bed | `vm_dosagent`: copy of `vm_6695_w2`'s pristine image and config (AMI 486DX-33, MS-DOS 6.22, no HIMEM), 3C509B at 320h IRQ 9, SLiRP host 9871 -> guest 9869, automation port 9899 |
| NIC | Upstream 86Box takes the 3C509B's I/O base and IRQ from its EEPROM file only (as the real card does), factory 300h/IRQ 10. Set to 320h/IRQ 9 by patching `nvr/eeprom_3c509b_1.nvr` words 08h/09h and the 0Fh checksum (low byte = XOR of the bytes of words 08h, 09h, 0Dh). `3C5X9PD 0x60` then reports I/O 0x320, interrupt 2: IRQ 9 shows as 2, as on the card |
| `ping`, `info` | pass: agent 0.17.0, "DOS 6.22", capabilities 0x406F |
| 1 MB put, then get | pass: CRC32 `6731E027` both ways, files identical (SHA-256). Put 3.2 s, get 3.0 s on the bed (wall clock, including PowerShell start) |
| `MEM` before the agent starts | 564 K free conventional (577,312 bytes). Packet driver 6,048 bytes, KEYB 6,944, COMMAND 5,072, DOS 59,405 |
| `shell dir C:\` and `exec C:\DOS\MEM.EXE` | **fail**: `system()` returns -1 at once. errno 5 (ENOMEM) under DOS/4GW. Run under DOS/32A instead (`DOS32A V9XAGNT.EXE`), errno 255 |

## Exec fixed (2026-11-18, late)

Cause: `_eth_init()` (`pcsed.c:689`) clears the Tx buffer with `sizeof(link_Packet)` = 1,570 bytes (the AX.25
member: 70-byte header + 1,500), but the fast receiver (`pcpkt2.c:59`) allocates `ETH_MAX + 10` = 1,524 for it.
46 bytes past the block, over the next MCB. Fix: size the Tx area by `sizeof(link_Packet)`
(`tools/dosagent/watt32_txbuf.patch`, applied to our Watt-32 at `3cf6f4e`). The non-fast DOS/4GW path
(`pcpkt.c:1735`, `TX_BUF() + RX_SIZE` = 1,548) has the same shortfall of 22 bytes; not built here.

Bed `vm_dosagent`, agent `dev-dos-x28b` (md5 7d347069c073c1f12fd40c9874234a8c): `shell "DIR C:\"`, `shell "MEM /C"`
and three more `MEM` calls all exit 0 with output. `MEM /C` under the agent: DOS4GW 96,400 bytes resident
(includes Watt-32's 63.8 KB real-mode block), largest program **464 K** (564 K without the agent).
Rebuild note: wmake does not track `pcpkt2.c` (it is `#include`d by `pcpkt.c`); delete
`build/watcom/flat/*.obj` before rebuilding. `configur.bat` needs `WATT_ROOT` set.

## Why exec failed: Watt-32 init breaks the DOS memory chain

Narrowed with two test programs run from AUTOEXEC (scratchpad only, not kept):

| Program | DOS/4GW | DOS/32A |
|---|---|---|
| Plain Watcom `system("dir ...")` | works | works |
| Same, then `sock_init()`, then `system()` again | first call works; after `sock_init` ENOMEM, the largest-DOS-block query fails, and at exit DOS prints "Memory allocation error / Cannot load COMMAND, system halted" | hangs (output not recovered) |

The memory dump taken after the halt (automation `memr`) shows the MCB chain ending at a free block at 1314h
whose size points to 2B19h, where there is no MCB. Bytes 2AB5Ah-2B1B6h are zero: a 1,628-byte zero run that
starts right after the Watt-32 receive queue marker (`DEAFBABE`) and the NIC's MAC address, and runs across
the MCB. Watt-32's DOS-memory area (fast real-mode receiver, `pcpkt2.c`) reaches past its own block. The
cause is not found. Checked and ruled out: the `dpmi_real_malloc` size units, and the receive ring offset
in `pktq_far_init`. A build without the fast receiver links only with `asmpkt4.asm`, which the Watcom
makefile does not build, so that was dropped. Watt-32 was put back as it was built.

Not tested: whether the agent's exec ever worked on the FreeDOS/NE2000 VM in `docs/dos-agent.md`. Not
tested: whether the corruption comes from init itself or from the first received frame.

## What the agent lacks for driving DOS game tests

| Need | Bed | 5160 | Cost |
|---|---|---|---|
| Start a program | blocked: exec above | blocked | Either fix Watt-32's overflow (one bed run: dump the MCB chain before and after `sock_init`, find the block that grows), or don't call `system()` at all. In that second way the agent writes the job to `RUN.BAT` and exits; AUTOEXEC loops `V9XAGNT`, then `CALL RUN.BAT`, then back to the agent. The game then gets all 564 K, since nothing else is resident, and output comes back by redirect plus `get`. Small: a change to the EXEC handler and a batch loop |
| Files in and out | works | should (untested) | - |
| Keys into a running game | 86Box automation `key` (`tools/bed_key.ps1`) already | none while the game runs: the agent is a foreground program | TSR (design step 3), or COMrade |
| Screen | automation `screenshot`/`text` already | none while the game runs | TSR |
| Memory/port read | automation `memr` | agent HW_ACCESS (design step 2), only between programs | step 2: medium; live: TSR |

So on the bed, automation plus this agent's file transfer already covers driving a DOS game test, once exec
works or the batch loop replaces it. On the 5160 the live-control gap stays until the UDP TSR (step 3).

Also seen: `info` shows `ListenAddress 15.2.0.10` for 10.0.2.15. The byte order is reversed, which is a
display bug in the DOS agent's INFO. Not fixed: it isn't ours (V9x repo), so it is reported here only.

## Owner steps for the 5160 (after exec is fixed; file transfer works without it)

1. Copy to `C:\V9XREMOT\`: `V9XAGNT.EXE`, `DOS4GW.EXE`, `AGENT.INI`, `WATTCP.CFG` (from
   `V9x-Remote-Agent\build\dos-install\`). Make `C:\V9XREMOT\TEMP`. The packet driver is
   `3C5X9PD.COM` (`Downloads\3com-etherdisk-4-3\PKTDVR\`).
2. `WATTCP.CFG`: the card's LAN address (static `my_ip`, `netmask`, `gateway`, or `my_ip = dhcp`).
   `AGENT.INI`: `allowed_client=` the host PC's LAN address. The protocol has no authentication.
3. A DOS-only boot (the owner edits CONFIG.SYS/AUTOEXEC.BAT): `3C5X9PD 0x60`, then
   `PATH C:\V9XREMOT;...` (DOS/4GW's stub looks for `DOS4GW.EXE` on PATH: "Can't run DOS/4G(W)"
   otherwise), then `SET WATTCP.CFG=C:\V9XREMOT`, then `V9XAGNT.EXE` as the last line.
4. Open risk, not tested: whether DOS/4GW finds the Inboard's extended memory. DOS/32A finds about 4.2 MB.
   If DOS/4GW fails, `DOS32A V9XAGNT.EXE` runs the same build.

## Files

- Bed `vm_dosagent/` (gitignored, like the other `vm_*` folders). `da.img` holds the agent and the
  packet driver. AUTOEXEC is restored to the plain agent setup. The original factory EEPROM is in the
  scratchpad as `dosagent/eeprom_300_10.nvr`.
- `docs/host_tools.md`: a row for Watt-32.
