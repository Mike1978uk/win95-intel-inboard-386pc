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
