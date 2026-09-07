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

## Phase 1 reconnaissance, 2026-09-07 — and the decision it surfaced

Established from `SD120PPD.SYS` (56,198 bytes, held in scratch; not yet tracked):

- It is a **plain DOS character driver named `SCSIMGR$`** — i.e. it presents as an ASPI manager,
  not as a block device. Header: attr `0xC000`, strategy `0x2074`, interrupt `0x2082`.
- Its transfer-mode table names **ten modes**: `NIBBLE Fast`, `NIBBLE Normal`, `NIBBLE Slow`,
  `NIBBLE Slow(-)`, `EPP Fast`, `EPP Normal`, `EPP BIOS(F)`, `EPP BIOS(N)`, `ECP Read`, `ECP Write`.
  So the bridge negotiates across nibble, EPP and ECP with several timing variants each.
- A raw opcode scan finds ~2,277 DX-addressed port instructions. **That is an upper bound, not a
  count** — the densest region (`0x3917`-`0x4c44`) overlaps the mode-name strings, so it is mixing
  code and data.

**Attempting to locate the mode dispatch table by pointer failed, and the failure is informative.**
Scanning for 16-bit words equal to a mode-string offset gives scattered hits with no contiguous
table. In 56 KB any particular 2-byte value is expected roughly once by chance, so those hits are
consistent with noise. Recording it as a dead end rather than reading a table into it — techniques
29 and 91a, both of which this project has already paid for.

### Why this is harder than the XT-IDE driver, and by a lot

`XTIDEMP.MPD` was tractable because **ATA is a documented standard**: the only unknown was the
register map, and that was recoverable by measurement in one COMrade session. Here the *protocol
itself* is proprietary and undocumented — a bit-banged state machine over three LPT registers with
timing dependencies and ten negotiated modes, to be recovered from 56 KB of unsymbolised 16-bit
real-mode code with interleaved data. That is the least forgiving class of target there is, and
this repo's own history says static analysis of binaries here produces confident wrong answers more
often than right ones.

### The fastest correct route is a LICENSING decision, not a technical one

Linux's `drivers/block/paride/epat.c` is a complete, working, maintained implementation of **this
exact bridge**, with `pf.c` covering ATAPI floppies — i.e. the LS-120 — on top of it. It is
**GPL-2.0**.

That gives two routes, and the choice is the owner's:

| | route | cost | licence outcome |
|---|---|---|---|
| **A** | Licence **this driver** GPL-2.0 and port from `paride` | days | `drivers/imation_ls120_mpd/` GPL-2.0; rest of repo stays MIT |
| **B** | Reverse-engineer `SD120PPD.SYS`, touch no GPL source | multi-session, high risk | everything stays MIT |

Route A is legitimate and normal — a repository can carry files under different licences, and this
one already has a third-party scope note in `LICENSE`. It turns "recover an undocumented protocol"
into "port a known-good implementation onto the SCSIPORT model", which is ordinary work.

Route B keeps the repo uniformly MIT and is what this project would do by reflex, but it is the
expensive one and it may simply not converge.

**Phase 0 is unaffected either way** — it contains no protocol at all and is licence-clean under
either route. Test it first regardless: if a polling parallel-port miniport disturbs the keyboard
on this machine, neither route matters.

## Phases 1 and 2 are pre-staged, 2026-09-07

Written while phase 0 waits for a boot. **Built and linking, and entirely untested — no part of it
has executed on any machine.** Build with `-Phase 2`; the default build is unchanged.

The driver is now three layers, and only the middle one is blocked:

| layer | what | status |
|---|---|---|
| 1 | parallel port registers (IEEE 1284) | **written** — `LS_ReadStatus`, `LS_WriteData`, … |
| 2 | EPAT bridge register access | **STUBBED** — proprietary, blocked on the licence decision |
| 3 | ATAPI packet commands over layer 2 | **written** — `LS_PacketCommand`, `LS_WaitDrq`, … |
| — | SRB → ATAPI dispatch in the miniport | **written** |

The stubs return failure rather than success on purpose: a stub that claimed to work would let the
ATAPI layer spin against nothing and present as a hardware fault.

**Why layers 1 and 3 could be written now.** ATAPI is a documented standard, and the useful fact is
that **an ATAPI command packet *is* a 12-byte SCSI CDB** — so SCSIPORT hands us a CDB in the SRB
and it goes on the wire unchanged, zero-padded if the class driver sent a 6- or 10-byte form. There
is no command translation to write at all. That is the whole reason presenting an ATAPI device as
SCSI is worth doing.

**It also narrows the licence exposure.** If route A is taken, only layer 2 — perhaps eight
functions in `LS120TR.ASM` — is derived from `paride`. Layers 1 and 3 and the miniport are
independent work and can stay MIT.

### Verified, not asserted

```
phase 0 (default)  code 7bfc5476  5120 bytes   1 port instruction   <- unchanged, still what is staged
phase 2            code a30227bc  5632 bytes   6 port instructions
xt_port_audit      0 destructive-write candidates in BOTH builds
```

Phase 0's code hash is **identical** before and after this work, so the conditional assembly is
genuinely inert and the binary on the card is unaffected. That is the check, rather than trusting
`ifdef`.

All six port instructions in the phase-2 build are DX-addressed through the LPT base taken from
`PORT=`. There is no immediate-port I/O anywhere in either build, so neither can reach an XT system
port by construction.

### A MASM note worth keeping

Exported transport routines must be `public NAME` + a plain `NAME:` label, **not** `PROC`/`ENDP`.
Under `.MODEL FLAT, STDCALL` a `PROC` gets C-style decoration and the caller's `EXTERNDEF NAME
: NEAR` then fails to resolve (`unresolved external symbol "_LS_PacketCommand"`). `XTIDETR.ASM`
uses the label form throughout for exactly this reason.

## Build

```
pwsh -File drivers/imation_ls120_mpd/build.ps1
```

MASM 6.11c + the DDK's VC++ 2.0-era `LINK.EXE` against `SCSIPORT.LIB` — XTIDEMP's toolchain unchanged. Every build prints its commit and appends to `build_ledger.tsv`; a binary that cannot be traced to a commit is not evidence (technique 89).

`-Base 0x378` pins the port and ignores the device node, for testing a base without a reinstall.

## Phase 1 decision and target, 2026-09-07

**Licence: stay MIT. Reverse-engineer, do not port from `paride`.** Owner's decision. So the
protocol has to come from our own observation of hardware and of software the owner owns and
was shipped with the drive - not from GPL source.

### The target is the DOS driver, not the Win95 miniport

`SD120PPD.MPD` (Win95) does its device I/O through **SCSIPORT library calls** - its import table
carries only `ScsiPortRead/WritePortBufferUchar/Ulong`, no raw `in`/`out` on the data path, and
it calls them through jump thunks. A byte scan finds nothing and a linear disassembly desyncs.
It is the wrong end of the telescope.

`SD120PPD.SYS` (DOS, 56,198 bytes, 1997-04-28, md5 `cbb42e8eb7847869e274e45f258cf718`) does raw
port I/O with nothing in between:

| | Win95 `.MPD` | DOS `.SYS` |
|---|---|---|
| validated `in`/`out` | 3 (none on the data path) | **2,242** |
| I/O route | SCSIPORT thunks | raw, `DX`-addressed |
| documented switches | none | `/ni /de /db /sf /dp /fp` |

The owner's call, and it is the right one: the `.SYS` is small, it is the thing whose switches
are known to work in real mode, and its transport is visible.

⚠ Header first: offset 0 is the **DOS device driver header** (`next=FFFFFFFF attr=C000
strategy=2074 interrupt=2082`), so a linear disassembly from zero desyncs immediately. Validate
raw opcode candidates by disassembling a window that *ends* on each one - 2,630 raw candidates
reduce to 2,242 real (technique 75's rule, applied properly).

### I/O cluster map

| ops | range | character |
|---|---|---|
| 880 | `0x3917`-`0x4e48` | mixed; contains the `out 22h/23h` chipset probes that kill the keyboard (technique 75) |
| **337** | **`0x25b9`-`0x2ce0`** | **pure `DX`-addressed - the EPAT transport** |
| 281 | `0xb0c2`-`0xbb2c` | mixed, touches `21h`/`2fh` |
| **97** | **`0x2da5`-`0x2f4c`** | **pure `DX`-addressed** |

### Confirmed shape of the transport

```asm
mov dx, word ptr [0x0bfa]     ; LPT base port  <- global
cmp byte ptr [0x0bd7], 0x0c   ; bridge mode     <- global
add dx, 2                     ; -> control register (base+2)
in  al, dx / and al, 0x1f / or al, 0x10 / out dx, al
; else:
mov al, 1 / out dx, al / out dx, al        ; data register (base+0)
add dx, 2 / mov al, 0x11 / out dx, al x8   ; control, 8x repeat = bus-speed padding
mov al, 0x14 / out dx, al x8
```

Standard parallel-port bit-banging: `base+0` data, `base+2` control, repeated writes as timing
padding for a slow bus. **Two globals carry the configuration** - `[0x0BFA]` the LPT base and
`[0x0BD7]` the bridge mode.

### Next, in order

1. Map the register-access primitives around `0x25b9` and `0x2da5` into a **written
   specification** - what sequence selects a register, reads it, writes it.
2. **Confirm it against the hardware, not the binary.** The drive is connected to the parallel
   port and powered with no driver loaded, and COMrade's DOS build has `io_in`/`io_out`. A live
   probe is original measurement, which is both better evidence and cleaner provenance than any
   disassembly.
3. Implement fresh from the specification. Never transcribe.

⚠ `0x378` is LPT1 and is **not** aliased by the XT system-board decoder (that covers `000-0FF`),
so probing it does not risk the PIC. But write nothing to `0x22`-`0x25` or `0x94` - that is the
exact bug that cost the keyboard (technique 75).
