# DOS network agent: design note (worklist X28), 2026-11-18

Desk research. Nothing built, installed or run. Claims marked (inferred) are not stated by the source.

## Finding that changes the question

The V9x agent **already has a DOS build** speaking the **same wire protocol** as the Win9x agent: foreground, Watt-32 TCP/IP, DOS/4GW, packet driver, port 9869, `allowed_client` in `AGENT.INI`, 1540 lines in `V9x-Remote-Agent/src/guest-dos/`. The existing `v9x` MCP drives it unchanged. X28 is therefore "extend that build", not "build a new agent on mTCP".

| Source | URL | Gave | Did NOT contain |
|---|---|---|---|
| V9x DOS agent doc | https://github.com/michaeldale/V9x-Remote-Agent (`docs/dos-agent.md`, local `C:\Users\lycet\RiderProjects\V9x-Remote-Agent`) | ops: HELLO/PING/INFO, EXEC (output after exit, no cancel/timeout), STAT/LIST/MKDIR, CRC32 transactional put/get; `allowed_client`; 86Box FreeDOS test VM recipe (NE2000 + SLiRP) | mem/io/keys/screen; resident size; any run on a real XT-class box; 3C509 |
| V9x protocol | same repo, `docs/protocol.md` | 24-byte header `V9XR`, v0x0100, u16 type, u32 reqId, u32 flags, payload max 65536; HELLO first; capability bits (0x20000000 is the highest used); allowed-client filter before HELLO | auth (protocol v1 is unauthenticated, per `dos-agent.md`) |
| COMrade | https://github.com/yyzkevin/COMrade (local `COMrade_Latest`, `doc/PROTOCOL.md`, `STATUS.md`) | full opcode table, below; serial-only by design | any TCP transport (STATUS.md states why, below) |

## What COMrade carries (command set to cover)

| MCP tool | COMrade opcode | DOS calls? | Needed from agent |
|---|---|---|---|
| mem_read/mem_write/mem_dump/banked_dump | 0x08/0x09, 20-bit linear, <=chunk | no | far-pointer copy |
| io_in/io_out/io_rmw/idx_read/idx_write/bus_stim/pic_snapshot | 0x0B/0x0C, byte/word | no | IN/OUT (rmw, idx, pic, stim are host-side compositions) |
| screen_read | 0x02, B800 snapshot | no | copy 4000 bytes + cursor |
| console_read | 0x0A, 8 KB ring from INT 29h hook | no | INT 29h stub |
| keys_send/key_down/key_up | 0x03 BIOS buffer, 0x0D raw 8042 scancodes | no | 8042 command 0xD2 |
| file_read/write/stat/hash/setattr, dir_list | 0x04-0x07, 0x10 | yes | InDOS-safe window |
| run_command | host-side: keys + console capture | - | keys + console ring |
| reboot | 0x0E | no | BDA flag + 8042 0xFE |
| dos_status, config_snapshot/diff, safe_policy, check_update/update_agent, desktop_screenshot | host-side or 0x01 PING, file ops | - | none new |

Serial frame: `0xAB | type | reqId:2 | len:2 | payload | crc16`, LZSS reply compression, chunk_max 1024. Resident 92 KB (README), 110 KB on a later build (STATUS.md).

## Why COMrade was serial-only (its own finding)

STATUS.md: a DOS TCP stack is cooperative; driving it from interrupt context while DOS runs an arbitrary program "either crashes the box (timer pump) or never gets serviced (idle pump)". The UART ISR is tiny and safe at any time. Load-bearing lessons if a TSR is written: private ISR stack, INT 28h/29h in asm not `__interrupt`, INT 29h tail-chains, preserve AX, drain deferred keys only from INT 28h (type-ahead race crashed the box), PSP/DTA swap around file ops. Not contained: why the 5160 runs fail (repo notes say keystroke stalls on long lines, mouse plugged in; cause not recorded).

## mTCP facts

| Question | Answer | Source |
|---|---|---|
| License | GPL-3.0 | https://github.com/retrohun/mTCP (`COPYING.TXT`; mirror of Brutman's sources, not the author's page) |
| Build | Open Watcom C 1.9 under DOS, `BUILDDOS.BAT`; C++ written "nominally", large model (inferred) | same, README |
| Layout | `TCPLIB`: ARP, DNS, ETH, IP, PACKET, TCP, TCPSOCKM, TIMER, UDP, UTILS | same |
| Receive path | packet driver upcall only supplies/fills buffers; protocol work runs only when the app polls (`PACKET_PROCESS_SINGLE`); driving it from the receive interrupt was rejected as too restrictive | https://raw.githubusercontent.com/retrohun/mTCP/master/DEVDOCS/DESIGN.TXT |
| Timer | hooks the timer interrupt, own tick count; must call `Utils::endStack()` or "your machine will crash" | same |
| Buffers | up to 40 packet buffers x 1514 B = ~60 KB, allocated at start; TCP send pool preallocated | same |
| Locking | only IRQ-disable around the buffer free list; callbacks must not use DOS, heap or C runtime | same |
| Stacks | DOS+packet drivers overflowed stacks on some machines; fixed by large stacks | https://raw.githubusercontent.com/retrohun/mTCP/master/DEVDOCS/DOSTECH.TXT |
| TSR | "no TSR to deal with": each app embeds its stack; only the packet driver is resident | https://bttr-software.de/forum/forum_entry.php?id=8257 (via search summary, not opened) |
| NetDrive | resident ~6 KB (author, VOGONS); UDP, no TCP | https://www.vogons.org/viewtopic.php?p=1217954 (search snippet, not opened) |

Not contained in anything fetched: a TCP-library-in-TSR example, library reentrancy statement, per-app resident size for telnetd/ftpsrv/httpserv, API (`CODING.TXT` has style rules only), current release and date (brutmanlabs.org refused the connection; brutman.com redirects to it). Conclusion (inferred): mTCP TCP cannot be called from an interrupt or INT 28h idle hook; no known TSR use of its TCP code. NetDrive shows a resident packet-driver client works when it avoids the TCP stack.

3C509B: packet driver `3C5X9PD.COM`, configure first with the 3Com config program and disable PnP; I/O 320h and IRQ 9 are valid values. https://retrocmp.de/transfer/3c509b.htm, https://www.hackerb9.com/3C509B-nestor (search summaries, not opened; no confirmation of 320/IRQ 9 together on an XT, where IRQ 9 arrives as IRQ 2). The repo already runs this NIC under Win95 on the card.

## Options

| | (a) Foreground agent | (b) TSR on timer / upcall | (c) Split |
|---|---|---|---|
| Shape | V9x DOS agent, last line of AUTOEXEC, program launched via `system()` | all ops from interrupts | foreground agent for files/exec + small UDP TSR for no-DOS ops |
| Reachable while a program runs | no (agent blocked in `system()`) | yes, if stack avoided | mem/io/screen/keys/reboot yes; files no |
| Needs mTCP/Watt-32 in TSR | n/a | yes: unsafe, COMrade ruled it out | no, raw UDP on packet driver |
| Code to write | opcodes only | stack in interrupt context | opcodes + ~1 KB-class UDP handler |
| Risk | low; exists | high | medium: send from upcall context |
| Conventional RAM | ~10 KB packet driver + DOS4GW stub (not measured) | >60 KB buffers | + <=8 KB (target, not measured) |

## Recommendation: (a) now, (c) second

Foreground V9x DOS agent covers files, exec, reliability and the existing MCP. It cannot do `mem_read` or `io_in` against a running program, which is the COMrade use that matters for hardware work; (c)'s tail covers it. Keep COMRADE serial as the fallback (COM1 is free of the NIC).

Memory budget (targets; measure before trusting): packet driver ~10 KB; DOS4GW and Watt-32 buffers sit in extended memory (4.2 MB reachable, DOS4GW needs INT 15h extended memory, check it works on the Inboard map); conventional left for the program under test ~600 KB. UDP tail <=8 KB with a private 1 KB stack, 4 packet buffers, no heap.

## Protocol

Reuse V9XR over TCP 9869 for (a): the `v9x` MCP and `v9xctl` need no change except new tools. Add opcodes (0x00B0-0x00CF are unused in `protocol.h` by grep, confirm) and two capability bits (0x40000000, 0x80000000 are the last free):

| Cap | Messages |
|---|---|
| HW_ACCESS 0x40000000 | MEM_READ/WRITE (u32 addr, u16 len), IO_IN/OUT (u16 port, u8 width, u32 val) |
| DOS_CONSOLE 0x80000000 | SCREEN_GET, KEYS_SEND (BIOS and raw), CONSOLE_GET (needs INT 29h ring, only in the TSR), REBOOT |

For (c)'s UDP tail use a COMrade-shaped frame (SYNC, type, reqId, len, CRC-16) inside UDP port 9870, ops idempotent, host retries, replies carry reqId. The 5160 DOS agent and the TSR share `proto.h` opcode numbers so one host class speaks both.

Security: `allowed_client` single IP in `AGENT.INI`, filtered before HELLO (V9x behaviour). Protocol is unauthenticated and mem/io/reboot make it more dangerous than Win9x file access: LAN-only, allowed_client mandatory, UDP tail drops any other source IP, `safe_policy` stays host-side.

## Build plan

| Step | Work | Test | Done when |
|---|---|---|---|
| 1 (first milestone) | Build V9x `build-guest-dos.ps1` (Open Watcom + Watt-32); bed with 3C509B 320/IRQ 9 (auto-irq9 branch), `3C5X9PD.COM`, `WATTCP.CFG`, SLiRP forward 9869 | `v9x_ping`, `v9x_info`, put/get with matching CRC, `exec --shell "dir C:\"` in an 86Box bed | ping + 1 MB round trip CRC-clean, and conventional RAM free recorded by `MEM` |
| 2 | Add HW_ACCESS and DOS_CONSOLE in the foreground agent (DOS4GW linear 0 maps first MB, inferred); MCP tools for them | mem_read of BDA 0040:0000, io_in 21h on the bed; then the same on the 5160 with owner present | matches COMrade output byte for byte on the same reads |
| 3 | UDP TSR tail (packet-driver upcall queues request; reply sent from timer-tick, not from upcall), INT 29h ring, 8042 keys | run a long DOS program, `mem_read` and `screen_read` during it; 1000-request soak with loss injected | no hang, no lost state, resident size measured |

Open risks: DOS4GW on the card (extended memory detection, Inboard bank layout); Watt-32 receive mode on the 3C509 driver; packet-driver re-entrancy on send; INT 28h gating for any file op in the tail (do not add). IRQ 9 delivered as IRQ 2 must be unmasked by the driver; VPICD is irrelevant in DOS.
