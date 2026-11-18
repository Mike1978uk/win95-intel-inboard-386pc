# V9x DOS agent, as run on the IBM 5160 + Intel Inboard 386/PC

The DOS build of Michael Dale's [V9x Remote Agent](https://github.com/michaeldale/V9x-Remote-Agent) with the
additions this project made to drive a real XT-class machine over the network from a host PC: run a program with
the agent unloaded, switch the agent off, and read or write memory, I/O ports, the text screen and the keyboard
buffer, and reboot. These files are byte-identical to the ones running on the 5160 (2026-11-18).

Source: [michaeldale/V9x-Remote-Agent#2](https://github.com/michaeldale/V9x-Remote-Agent/pull/2), branch
`dos-agent-handoff-probes` on [Mike1978uk/V9x-Remote-Agent](https://github.com/Mike1978uk/V9x-Remote-Agent/tree/dos-agent-handoff-probes),
commit `9cc8441`, on top of V9x 0.19.0.

## Credits

- **V9x Remote Agent** - Michael Dale and contributors, BSD 2-Clause (`V9X-LICENSE.TXT`). The DOS build, its
  protocol and `v9xctl.ps1` are theirs.
- **COMrade** - Kevin Moonlight, [github.com/yyzkevin/COMrade](https://github.com/yyzkevin/COMrade) (MIT). The probe
  set (memory, ports, screen, BIOS key buffer, reboot) and the reboot steps follow COMrade; no code is copied.
- **Watt-32** - Erick Engelke, Gisle Vanem and contributors, [github.com/gvanem/Watt-32](https://github.com/gvanem/Watt-32).
  Linked into `V9XAGNT.EXE` with one fix: the fast packet receiver's Tx buffer was 46 bytes short and broke the
  DOS memory chain, so no program could be run ([gvanem/Watt-32#133](https://github.com/gvanem/Watt-32/issues/133)).
- **DOS/4GW 1.97** - Rational Systems / Tenberry, as shipped with Open Watcom.

## Files

| File | What it is |
|---|---|
| `V9XAGNT.EXE` | the agent (DOS/4GW, Watt-32), build `dev-dos-con2` |
| `DOS4GW.EXE` | the DOS extender it needs, on `PATH` |
| `V9XLOOP.BAT` | the loop: agent, then any handed-over job, then the agent again |
| `AGENT.INI` | port and the one host allowed to connect |
| `WATTCP.CFG` | network settings (`my_ip = dhcp`) |
| `V9X-LICENSE.TXT` | the V9x licence |
| `SHA256.TXT` | hashes of the above |

Not included: a packet driver. See the next section.

## Packet drivers: any network card

The agent does not talk to the network card. It talks to a **packet driver**, a small resident DOS program that
the card's maker (or the Crynwr packet driver collection) supplies for almost every ISA, PCI and PCMCIA network
card of the DOS era. Load the one for your card before the agent, and the agent works the same on all of them.

1. **Find the driver for your card.** It is usually named after the card: `NE2000.COM`, `3C509.COM`, `3C5X9PD.COM`,
   `PCNTPK.COM`, `RTSPKT.COM` and so on, and is often in a `PKTDRVR` folder on the card's driver disk.
2. **Load it with a software interrupt between 60h and 80h**, conventionally `0x60`. Most drivers take the
   interrupt first, then the card's IRQ and I/O base if it cannot find them itself. Read the driver's own help
   (run it with no arguments, or `-?`) for its exact order:

```
NE2000 0x60 3 0x300        (interrupt 60h, IRQ 3, I/O 300h)
3C5X9PD 0x60               (3C509: reads IRQ and I/O from the card)
```

3. **Check it loaded.** A packet driver prints its interrupt, I/O base, IRQ and the card's Ethernet address. If it
   prints an error or nothing, fix that first: the agent cannot get further than the driver.
4. **Watt-32 finds it by itself.** It searches interrupts 60h-80h for the driver's `PKT DRVR` signature. To pin a
   different one, add `PKT.VECTOR = 0x61` (for example) to `WATTCP.CFG`.

On the 5160 the line is `3C5X9PD 0x60` (3Com EtherDisk 4.3); the card is at I/O 320h, IRQ 9, which an XT delivers as
IRQ 2. Two traps worth knowing:

- **The IRQ must be one the machine has.** On an 8-bit XT there are IRQ 2-7 only, and IRQ 9 on a 16-bit card
  arrives as IRQ 2.
- **In 86Box** the NE2000 with `NE2000 0x60 3 0x300` (or the card's configured IRQ and base) is the simplest test;
  with SLiRP networking, forward a host port to the guest's 9869.

## Install

1. Copy the files to `C:\V9XREMOT\` and make `C:\V9XREMOT\TEMP`.
2. `AGENT.INI`: set `allowed_client=` to your host's address. The protocol has no authentication, and the probes can
   write memory and ports, so keep it on your own network.
3. `WATTCP.CFG`: `my_ip = dhcp`, or a fixed `my_ip`, `netmask` and `gateway`.
4. Boot to DOS with the packet driver loaded, then end `AUTOEXEC.BAT` with:

```
C:\V9XREMOT\3C5X9PD.COM 0x60
SET WATTCP.CFG=C:\V9XREMOT
PATH C:\V9XREMOT;%PATH%
CALL C:\V9XREMOT\V9XLOOP.BAT
IF ERRORLEVEL 4 COMMAND /K ECHO Agent stopped. V9XLOOP starts it again.
```

On the 5160 this is a `DOSNET` entry in the `CONFIG.SYS` boot menu, so Windows 95 and the DOS agent share one disk;
a failed agent start falls through to Windows so the machine stays reachable.

## Use

- **At the machine:** the agent prints `V9x DOS agent on <address>:9869 - Esc for a DOS prompt`. Esc gives a prompt;
  `V9XLOOP` hands the machine back to the agent. N at the 3-second prompt skips it for one boot, and a file
  `C:\V9XREMOT\OFF` keeps it off.
- **Files and programs, from the host:** `v9xctl.ps1` from the V9x repo (`ping`, `info`, `put`, `get`, `exec`,
  `shell`). `exec`/`shell` run with the agent resident; add `-Detach` to hand the machine over: the agent exits, the
  job runs with all conventional memory, and the agent restarts.
- **Probes:** [`tools/dosagent/v9xdos.py`](../../tools/dosagent/v9xdos.py) - `mem`, `memw`, `in`, `out`, `screen`,
  `keys`, `reboot`. They answer while the agent is waiting, not while a handed-over program runs.
- **Switching boots unattended:** [`tools/dosagent/bootmode.ps1`](../../tools/dosagent/bootmode.ps1) `dos | windows |
  status` edits `menudefault` through whichever agent is up and reboots.

## What it was tested on

86Box (MS-DOS 6.22, 3C509B, packet driver) and the real IBM 5160 + Inboard 386/PC (Windows 95's DOS 7, 3C509B at
320h, IRQ 9 delivered as 2): ping, info, file put/get, exec, the handoff with the agent coming back, the off flag,
Esc to a prompt, every probe, and reboot. A handed-over `M8DTST16 bench` ran at the same frame rate as by hand.
Not tested: FreeDOS, other network cards.

Plan and results: [`docs/dos_agent_capability_plan_2026_11_18.md`](../../docs/dos_agent_capability_plan_2026_11_18.md).
