# LS120MP.MPD — a Windows 95 miniport for a parallel-port LS-120

**Status: PHASE 0 — skeleton. It does not drive the drive, and installing it will not give you a drive letter.** That is deliberate. Tracked as [issue #22](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/22).

## Why write one at all

Imation's own `SD120PPD.MPD` works, and kills keyboard input on this machine. The one thing proven about that is narrow: rename it out of `IOSUBSYS`, change nothing else, and the keyboard returns instantly. **The mechanism is not known** — six binary patches produced six nulls, including one that NOPed all 98 chipset writes *and* all eight `out 0x21` sites, and on 2026-09-07 its whole `AdapterSettings` surface turned out to contain no equivalent of the DOS driver's `/ni` either. Both ends of "persuade the vendor binary to behave" are now exhausted.

What changed is that we can write miniports: `XTIDEMP.MPD` is shipped and hardware-confirmed on this exact machine (issue #21), so there is a proven template, a working toolchain and a known-good INF.

## Phase 0 exists to test one premise, cheaply

The plan rests on an assumption — *a driver that never probes and never asks for an interrupt is safe by construction* (technique 75). That is a hypothesis, not a fact, and it would be expensive to discover it was wrong after writing a transport.

So this build registers with SCSIPORT, reads the LPT status register once, reports no devices, and sits there.

**Verified property, and it is checkable rather than asserted:**

```
xt_port_audit.py         : 0 destructive-write candidates
raw opcode census        : exactly 1 port instruction in the entire binary
                           0x000104de  in al, dx
```

There is **no `OUT` instruction anywhere in this driver**. It is structurally incapable of writing to any I/O port, so whatever a phase-0 boot does to the keyboard, it cannot be an I/O write from our code.

### The test

Prerequisites: `inbrdpc.sys` in `[SafeList]` (issue #17), and **Imation's `SD120PPD.MPD` node removed in Device Manager** — leaving it loaded would confound the whole thing.

1. Confirm the keyboard works. **Baseline first**, on the boot before.
2. Install: Add New Hardware → decline autodetect → SCSI controllers → Have Disk → `LS120MP.INF`.
3. **Reboot. One install, one boot, no other changes** (technique 76).
4. Test the keyboard.
5. Read `BOOTLOG.TXT` for `Init Success ls120mp.mpd` — that it installed and that it *ran* are different claims (technique 74). Delete the old `BOOTLOG.TXT` first.

| outcome | what it means | next |
|---|---|---|
| keyboard fine, `Init Success` | premise holds | build the transport, phase 1 |
| keyboard dies | the fault is **structural** to a parallel-port miniport here, not Imation's code | stop; the plan is wrong and the finding is worth more than the driver |
| no `Init Success` | it never ran — the run is void, not a negative | fix the install, retest |

Reverting is a file rename or a node removal, and the drive is on the DOS driver throughout, so a bad outcome costs a reboot.

## What is still ahead, honestly

Phase 0 is perhaps a twentieth of the work. The rest, in order:

**Phase 1 — the EPAT bridge.** The hard part. Shuttle's EPAT is a proprietary parallel-to-ATAPI bridge; getting at the ATAPI registers means implementing its register-access protocol and its mode negotiation (nibble / byte / EPP, the DOS build's `NIBBLE Fast`…`EPP BIOS(N)` string table). Derivable by disassembling `SD120PPD.SYS`, which we hold.

**Phase 2 — ATAPI packet commands.** `INQUIRY`, `TEST UNIT READY`, `READ CAPACITY`, `REQUEST SENSE`. The LS-120 is an ATAPI device behind the bridge, so this is packet commands over phase 1's transport.

**Phase 3 — `READ(10)`/`WRITE(10)`, and media change.** Removable media is where the floppy driver still corrupts disks after a few swaps ([#18](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/18)). Expect that class of problem here too, and do not assume it is easier.

This is a multi-session build, not a weekend. It also cannot be developed in emulation: 86Box models neither the `0x20-0x3F` PIC aliasing that makes this machine special nor parallel-port LS-120 hardware at all (`RDISK_BUS_LPT` is a dead enum). **Hardware only**, which means it moves at the speed of the owner being at the machine.

⚠ **Licence, to settle before phase 1 and not after.** The obvious protocol reference is Linux's `drivers/block/paride/epat.c` — **GPL-2.0**, against this repo's MIT. Hardware register facts are not copyrightable; a port of that code would be a derivative work. The clean route is disassembling the vendor binary we own for interoperability, which is what this project does routinely, and not reading `epat.c` while writing ours.

## Build

```
pwsh -File drivers/imation_ls120_mpd/build.ps1
```

MASM 6.11c + the DDK's VC++ 2.0-era `LINK.EXE` against `SCSIPORT.LIB` — XTIDEMP's toolchain unchanged. Every build prints its commit and appends to `build_ledger.tsv`; a binary that cannot be traced to a commit is not evidence (technique 89).

`-Base 0x378` pins the port and ignores the device node, for testing a base without a reinstall.
