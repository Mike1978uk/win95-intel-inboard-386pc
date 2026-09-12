# SD120PPD.SYS read in full — the DOS driver that works on this machine

2026-09-13, at the owner's insistence after I had read fragments three times:

> *"now the rest of the dos driver don't stop at that section - lets be thorough"*

## First, the coverage problem

`SD120PPD_SYS.asm` as it stood covered **0x2074–0x4E50 — 21% of the file.** It began at the
strategy entry point and stopped at the first undecodable byte, leaving **44 KB never read**.
Regenerated with resync over the whole image: **26,095 instructions, all 56,198 bytes.**

That is technique 112's rule 2 in practice: *a linear sweep stops at the first undecodable byte,
so a partial dump looks exactly like a complete one.* Everything below came out of the 79% that
nobody had looked at.

**Header:** `SCSIMGR$`, a character device — an **ASPI manager**, not a block driver.
Attribute `0xC000`, strategy `0x2074`, interrupt `0x2082`.

## Timeouts — armed in BIOS ticks, so the real budgets are seconds

`0x23b7` arms a deadline from `0040:006C`, the BIOS tick counter (18.2 Hz); `0x23ce` tests it.
So every `mov cx, N` before a `call 0x23b7` is **N/18.2 seconds**.

| site | ticks | seconds | what it is waiting for |
|---|---|---|---|
| `0x1c2c` | 180 | **9.9 s** | command phase — ready to accept a CDB |
| `0x1c40` | 1080 | **59.3 s** | **data IN** |
| `0x1c64` | 180 | **9.9 s** | **data OUT** |
| `0x1c88` | 324 | **17.8 s** | completion |
| `0x1bd6` | 180 | **9.9 s** | DRQ to appear (`0x1bcc`) |
| `0x1a12` | 90 | 4.9 s | |
| `0x7027` | 36 | 2.0 s | |
| `0x707f`, `0x745b` | 5 | 0.3 s | |
| `0x16ca` | 16200 | **890 s** | ~15 minutes — a format or full-surface operation |

### Against ours, and against the other two sources

| wait | DOS `.SYS` | `SD120PPD.MPD` | Linux `pf.c` | **ours** |
|---|---|---|---|---|
| data in / completion | 59.3 s / 17.8 s | 10 s | 8 s | `LS_SPIN_BSY` **~60 ms** |
| data out | 9.9 s | 5 s | 8 s | (was: none at all) |
| DRQ | 9.9 s | — | 8 s | `LS_SPIN_DRQ` **~30 ms** |

**Four independent implementations allow seconds. Every transport wait we have is 100-1000x
short**, and the `FINISH` state fixed today (8 s) is still under the DOS driver's completion
budget of 17.8 s.

## The phase machine — identical in both vendor binaries

`0x1bfe` waits until interrupt reason (register 2, masked to 2 bits) equals **either** of two
caller-supplied phases in `BL`/`BH`, checking BSY first:

| wrapper | BL,BH | meaning |
|---|---|---|
| `0x1c2f` | 1,1 | command — the drive wants a CDB |
| `0x1c43` | 2,3 | data IN, or already complete |
| `0x1c67` | **0,3** | **data OUT**, or already complete |
| `0x1c8b` | 3,3 | completion |

and every wrapper then **branches on which phase it actually landed on**:

```asm
0x1c67  call 0x1bfe      ; wait for 0 or 3
0x1c6a  jb  0x1c7a       ; timed out
0x1c6c  or  al, al       ; WHICH one?
0x1c6e  jne 0x1c7a       ;   phase 3 -> return, moved nothing
0x1c70  mov bx, 7
0x1c73  call 0x1bcc      ;   phase 0 -> wait for DRQ, then transfer
```

The MPD does the same thing with the same four phase pairs (rva `0x1dde`, `0x1df6`, `0x1e27`,
`0x1e5a`). **Two independent binaries, one design** — which is what makes it safe to copy.

⚠ **One difference from ours that is still open.** The vendor re-reads status *fresh* through
`0x1bcc` after the phase settles, before moving data. Ours tests `LS_LastStatus`, which is
whatever the previous `LS_PfWait` left behind. Not yet shown to matter; worth closing.

## The register offset map, confirmed from code

`0x343e` maps a logical register number to a bridge offset:

```asm
dx == 8  ->  0x16        ; device control
dx == 9  ->  0x17        ; alternate status
else     ->  dx | 0x18   ; the ATA task file at 0x18
```

This is technique 106's offset map, now read out of the vendor's own code rather than inferred.

## Transfer modes are a dispatch table, not a parameter

`0x2472` (write) and `0x244b` (read) do not touch a port directly. They take a mode index from
`[0xbdb]` / `[0xbd9]`, shift it left 3, and `call word ptr cs:[bx + 0x4f15]` / `cs:[bx + 0x4e9d]`.

So each transfer mode is a **separate routine**, selected at load time by the `/rx` (read, 0-11)
and `/wy` (write, 0-4) switches. Handlers live at `0x3915`-`0x3C26` (read) and `0x4847`-`0x4A17`
(write).

**A cross-check that the tables are right:** decoding 12 write entries produces garbage from
index 9 onward (`0x8b52`, `0x830d`, `0x0d96`), and the vendor's own help text says `/wy` ranges
**0 to 4**. The table has five valid entries and the rest is the next structure. Ranges agreed
before any of them were trusted.

**Relevance to the bus-occupancy track:** this is a menu of twelve read transports on the same
hardware, each a readable routine. Costing them against each other is a static exercise.

## ⛔ The I/O footprint — and the keyboard boundary, with counts

| kind | count |
|---|---|
| `out dx` / `in dx` (parallel port, base-relative) | **1628 / 421** |
| `out imm` / `in imm` (fixed ports) | **157 / 86** |

Every fixed-port access, with the XT system blocks flagged:

| port | count | |
|---|---|---|
| `0x20` | 4 | ⛔ 8259 |
| `0x21` | 28 | ⛔ 8259 mask |
| **`0x22`** | **68** | ⛔ **aliases onto the 8259 on this XT** |
| **`0x23`** | **24** | ⛔ **same** |
| `0x24` / `0x25` | 3 / 5 | ⛔ same block |
| `0x2f` | 2 | ⛔ |
| `0x42` / `0x43` | 4 / 1 | ⛔ PIT |
| `0x61` | 4 | ⛔ PPI |
| **`0x94`** | **16** | ⛔ inside the DMA page block |
| `0xa0` / `0xa1` | 4 / 12 | AT slave PIC — absent here |
| `0xec`/`0xed`/`0xf9`/`0xfb`/`0xae`/`0xaf` | 41 | chipset config |

**243 fixed-port accesses, and the dangerous ones are exactly what technique 75 measured.**
This is now a count from a complete disassembly rather than a partial scan.

**The rule stands and is now quantified: take the vendor's TIMING and its PHASE MACHINE. Never
its chipset init.** `/ni` exists because the vendor knows these probes are optional; on this
board they are not merely optional, they reprogram the interrupt controller.

## What is still not read

Honest list, so nobody thinks this closes the file:

- the ASPI request dispatcher (`SC_EXEC_SCSI_CMD` and friends) end to end;
- the twelve read-mode handlers individually — named and located, not costed;
- the `0x16ca` 890-second path;
- error and sense handling, and the retry policy;
- the EPP/ECP detection that `/de`, `/db`, `/ded` disable.

---

# Second pass — the open list, worked through

## The transfer modes, resolved to names

Each table slot's handler is preceded in the image by its own name string, so the indices the
`/rx` and `/wy` switches take resolve exactly:

| `/rx` | read mode | | `/wy` | write mode |
|---|---|---|---|---|
| **0** | **NIBBLE Fast** | | **0** | WRITE Fast |
| **1** | **NIBBLE Normal** | | **1** | WRITE Normal |
| **2** | **NIBBLE Slow** | | **2** | WRITE Slow |
| 3 | UNIDIR Fast | | 3 | EPP Normal |
| 4 | UNIDIR Normal | | 4 | EPP Normal (variant) |
| 5 | UNIDIR Slow | | | |
| 6 | TOSHIBA Fast | | | |
| 7 | TOSHIBA Normal | | | |
| 8 | PS/2 Fast | | | |
| 9 | PS/2 Normal | | | |
| 10 | EPP Normal | | | |
| 11 | EPP Normal (variant) | | | |

Further names in the image that do not appear in the primary tables — so they are reached by the
detection path rather than by `/rx`: `NIBBLE Slow(-)`, `UNIDIR two wait`, `EPP Fast`,
`EPP BIOS(F)`, `EPP BIOS(N)`, `WRITE Fast(+)`, `WRITE Slow(-)`, **`ECP Read`**, **`ECP Write`**.

### ⭐ This is a bus-occupancy finding, not just trivia

**Our driver reads in nibble mode — indices 0-2, the slowest family the bridge has.** UNIDIR,
TOSHIBA, PS/2, EPP and ECP are all present, on this same hardware, as selectable routines. The
vendor probes for the fastest that works (`/sf` exists to *skip* that detection) and says so when
it cannot: *"Specified mode failed. Loading with detected modes."*

A nibble read is ~8 port accesses per register byte. At the measured ~3.9 us of Inboard-to-bus
sync per access (technique 109e) that is the dominant cost of every status poll and every data
byte we move. **Costing the faster families against nibble is a static exercise on routines we
now have located.** Added to `bus_optimisation_plan.md` as a lever, not yet costed.

## The command timeout is caller-settable

```asm
0x16b0  mov cx, 0x32a           ; 810 ticks = 44.5 s, the default
0x16b3  cmp word ptr [0x951], 0
0x16b8  je  0x16ca              ;   0 -> use the default
0x16ba  cmp word ptr [0x951], -1
0x16c1  mov cx, 0x3f48          ;  -1 -> 16200 ticks = 890 s
0x16c6  mov cx, word ptr [0x951];else the caller's own value
```

So **44.5 s default, overridable per command, -1 meaning ~15 minutes** — the ASPI SRB timeout.
The MPD has the same idea with a 60 s default at `[0x203f0]`. Ours has no per-command timeout at
all.

## Identity and provenance, from the strings

| | |
|---|---|
| `0x6440` | `EPATRM Device Module 5.32b 28th April, 1997` |
| `0x6406` | `ATAPI LS-120 module V5.23b` / `23rd April, 1997` |
| `0x0746`, `0xd80a` | `SHUTTLE EPATRM` — matches the bridge on the bench |
| `0xdab4` | `ASPI Manager For Dos Ver 5.32b`, Shuttle Technology |
| `0x09c5`-`0x0d3e` | four dated sub-modules, A/B/C/F, 1995-1997 |

## Host-adapter status decode, free

The driver carries the text for every HA status it reports, which is a decode table for anything
we see on the wire: `Selection Timeout`, `Data Over/Under Run`, `Unexpected Bus Free`,
`Bus Phase Sequence Failure`, `Specified LUN Busy`, `Reservation conflict`,
`Unknown Target Status`, plus `Sense Bytes :`.

## The ASPI entry point

DOS device command 3 (IOCTL read) at `0x20c2` calls `0x5c14` and returns `DS:SI` as a far pointer
in the caller's buffer — the standard ASPI handout. Everything above sits behind that entry.

## Still not read, and now a shorter list

- the ASPI request dispatcher end to end from `0x5c14`;
- the individual mode handlers' inner loops (located and named, not costed);
- error/sense handling and any retry policy — a first search for a decrementing retry counter
  found none, which is itself worth confirming rather than asserting;
- the EPP/ECP detection that `/de`, `/db`, `/ded`, `/sf` disable.

---

# Third pass — the whole open list

## ⭐ The ECP handlers, and they are SAFE to adopt

Owner: *"we use an ECP card."* These are the routines that matter, and they are readable.

**ECP read**, from `0x3cce`; **ECP write**, from `0x493e`. Both work the same way:

```asm
mov dx, word ptr [0xbfa]   ; the parallel-port base
add dx, 2                  ; base+2  = LPT control
mov al, 4
out dx, al
add dx, 0x400              ; base+0x402 = ECR, the ECP Extended Control Register
mov al, 0x74
out dx, al                 ; ECR = 0x74 -> ECP FIFO mode
mov cx, 0xffff
in  al, dx                 ; poll ECR
test al, 1                 ; bit 0 = FIFO empty
loope ...                  ; spin until the FIFO can take/give a byte
...                        ; move the byte at base+0
mov al, 0x34
out dx, al                 ; ECR = 0x34 -> back to byte/PS2 mode
```

**ECR mode field is bits 7:5** - `0x74` selects ECP, `0x34` selects byte mode, and bit 0 is the
FIFO-empty flag the loop waits on. The hardware does the handshake; the driver polls one status
register instead of driving four control transitions per byte.

### The safety answer, measured rather than assumed

```
ECP READ  : 0 fixed-port accesses  <- entirely base-relative
ECP WRITE : 0 fixed-port accesses  <- entirely base-relative
```

**Nothing in either handler touches `0x22`, `0x23`, `0x94` or any other XT system port.** Every
address is `[0xbfa]` plus 0, 2 or 0x402. So the transfer path is adoptable; technique 108's
warning was about the vendor's **DMA-assisted** path, which is different code.

### Where the keyboard-killer actually lives

The dangerous writes cluster in **eight regions**, each with the same 4-write/2-read shape:
`0x3c56`, `0x442a`, `0x4499`, `0x4516`, `0x45b7`, `0x49d3`, `0x4bbc`, `0x4c34`. A worked example:

```asm
0x3c56  out 0x22, ax
0x3c58  in  al, 0x22
0x3c5a  and al, 0x1f
0x3c5c  or  al, 0x21
0x3c5e  out 0x22, al        ; read-modify-write a chipset config register
0x3c65  cmp byte ptr [0xbc0], 3   ; and it is GATED ON CHIPSET TYPE
```

These sit **adjacent to** the mode handlers - they are the EPP/ECP *detection and chipset setup*
that `/ni`, `/de`, `/db` and `/fe` disable - but they are separate routines from the transfer
handlers. That is the boundary, and it is now precise: **copy the handler, never its neighbour.**

## The ASPI surface, mapped

IOCTL read (DOS device command 3) calls `0x5c14`, which returns `DS:SI = CS:0x55cd`. That entry
validates the host-adapter number, sets `SRB[1] = 0x80` (SS_PENDING), and dispatches:

- **commands 0-6** through a jump table at `cs:0x5651`;
- **commands > 6** through a vendor-extension table at `0xffd` (6 slots).

| cmd | ASPI function | handler |
|---|---|---|
| 0 | `SC_HA_INQUIRY` | `0x5959` |
| 1 | `SC_GET_DEV_TYPE` | `0x59e9` |
| 2 | **`SC_EXEC_SCSI_CMD`** | **`0x5a69`** |
| 3 | `SC_ABORT_SRB` | `0x5885` |
| 4 | `SC_RESET_DEV` | `0x5946` -> `call 0x1625` |
| 5 | `SC_SET_HA_PARMS` | `0x594a` |
| 6 | `SC_GET_DISK_INFO` | `0xc626` |

The EXEC path marshals from SRB offsets `+0x2a/+0x2c` (data pointer), `+0x0a` (data length),
`+0x17` (CDB length), `+0x03` (target), `+0x0f/+0x11`, `+0x40/+0x42/+0x44`.

## Error reporting, and an honest negative on retries

The driver carries the text for every host-adapter status it can report, which doubles as a
decode table for anything seen on the wire: `Selection Timeout`, `Data Over/Under Run`,
`Unexpected Bus Free`, `Bus Phase Sequence Failure`, `Specified LUN Busy`,
`Reservation conflict`, `Unknown Target Status`, `Sense Bytes :`, plus
`ERROR: TargetStatus =` / `HostAdapterStatus =` / `Sense Bytes`.

**No retry counter was found.** A search for a constant 3/4/5/6/10 stored to a byte and
decremented within 60 bytes returned nothing. Stated as a negative rather than a conclusion - it
is a heuristic, and a retry loop structured differently would not match it. What *is* certain is
that the driver leans on long timeouts (9.9-59.3 s) rather than short ones with retries, which is
the opposite of ours: ~60 ms with `LS_MAX_RETRY` 5.

## What remains genuinely unread

- the inner loops of the twelve read and five write mode handlers, individually - located and
  named, and the ECP pair now read, but the rest not walked instruction by instruction;
- `SC_EXEC_SCSI_CMD`'s body at `0x5a69` past its SRB marshalling;
- the EPP/ECP detection logic inside those eight probe regions, as opposed to their port writes.
