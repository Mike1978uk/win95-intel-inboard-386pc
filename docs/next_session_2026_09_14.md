# Where everything stands — end of 2026-09-13

Supersedes `next_session_2026_09_13.md`. Read this one first.

---

## ⭐ ECP READS AND WRITES. The owner round-tripped a file edit on it.

He opened the drive in the bed, edited `HELLO.TXT`, saved it and reopened it with his change —
the same milestone SPP reached the night before, now on ECP. A 92 KB file copied in-guest and
verified **byte-exact from the host** (`ea923fa0…` both sides).

## ✅ RETRACTED: "ECP writes corrupt LBA 0"

The 8 changed bytes at offset 3 of the boot sector are **Windows 9x's own "IHC" signature** —
CHICAGO reversed — stamped into the OEM ID field by Volume Tracker on any access. Named by
@andrew-hoffman on #22; <https://www.os2museum.com/wp/the-ihc-damage/>.

Corroborated independently before his comment was read: bytes 5-7 were `49 48 43` in all three
runs, always written straight after a PREVENT MEDIUM REMOVAL, the drive's own copy read back
clean beforehand, and Windows kept using the volume happily afterwards.

**The real fault was the emulator dropping bytes** (below). The volume read empty because the
sector was 145 bytes short, not because of the OEM field. Rewriting `MSWIN4.1` over the
signature was harmless and pointless — Windows restamps it.

**General rule, now technique 120e:** a difference that lands exactly on a semantic field
boundary is somebody writing that field, not a transport dropping bytes.

---

## What was wrong, and what fixed it

### 1. 86Box's ECP FIFO had no back-pressure — 145 bytes lost per 512

```
before:  port=512  fifo=368  full=145  ->dev=367
after:   port=512  fifo=513  full=0    ->dev=512
```

16-byte FIFO drained on a 2.5 MHz timer; a write arriving full was silently discarded. Real
ECP holds the ISA cycle until there is room. Fixed by delivering one queued byte at the moment
the stall would have happened.

### 2. The ECP transport never COMMANDED the bridge

`LS_BlockReadEcp`/`LS_BlockWriteEcp` streamed the FIFO with the bridge never told a block was
coming. It "worked" only because our own model short-circuited — a shortcut that would have
become an invisible prerequisite on hardware (technique 110).

An EPAT block transfer is framed by one **ECP address cycle** — a write to `base+0` while the
ECR is in ECP mode — carrying `0x80` read block, `0xC0` write block, `0xA0` last byte of a
read. `SD120PPD.SYS` rva `0x4736`/`0x4DB9`; Linux `epat.c` sends the same bytes as
`w3(0x80)`/`w3(0xc0)` with an EPP host. **The bridge model now refuses unframed data.**

Forward is control `0x04`, reverse `0x20` — bit 2 is nInit, which an ECP host drives LOW to
request the reverse channel. Not one bit apart.

### 3. ECP writes now pace themselves, 16 bytes per ECR poll

A bare `rep outsb` assumes the port stalls the bus when the FIFO fills. The vendor never
assumes that: it divides every transfer by a word in its own data area (`[0C18h]`, shipped as
**16** — the FIFO depth), waits for ECR bit 0, and sends one chunk at a time. Ours now does the
same, and the emulator's `relief=` counter reads **0**, which is the proof the driver no longer
depends on a stall.

⚠ **This is a hardware-readiness item, not a tidy-up.** If the owner's card does not drive
`nWait`, the old code would have lost bytes on the bench exactly as it did in the bed.

### 4. Transfer length capped at 4096

A transfer is moved INLINE at one byte per port access, so `MaximumTransferLength` is also a
length of time the system is held: at 5.55 us per access, 30,720 bytes is ~170 ms on ECP and
~340 ms on nibble, the latter with IF clear. At 65024 a single `WRITE(10)` started and never
completed — **identically on the SPP build**, so it was the size and not the transport.

---

## ⛔ THE ONE OPEN BLOCKER: a deterministic stall at 383,488 bytes

Copying a 9 MB file to the LS-120 stops after **exactly 98 write commands** (93 x 4096 +
5 x 512) — the same number on the chunked and unchunked builds, so it is not caused by any of
today's work. The guest burns 100% CPU with **zero** drive traffic afterwards.

The heartbeat puts it in the **same VMM loop that killed the `.PDR`** and was never root-caused:

```
C000321C:  cmp word [edi+68],0 / je +8 / mov eax,[edi+6C]   ; EDI = a TCB
C000323F:  mov eax,[edx-4] / test eax,eax / je / test [eax+8],ecx / mov edx,eax / jmp back
           ecx = 80001842   eax = C10CD880
```

A linked-list walk that does not terminate. **Technique 90d prescribed dumping that list at
`TCB+6Ch` and nobody has ever done it.** That is the next measurement.

Ruled out so far:
- **not the chunking** — unchunked build stalls at the same 98
- **not the transport** — SPP stalls too, at the larger transfer size
- **not disk space** — 124 MB free, and Windows raises no warning
- the last command before the stall completes cleanly (`status 40`, `DISCONNECT`), so the
  driver is idle and the stall is above it

⏳ **Running when this was written:** a control that copies the same 9 MB file **C: to C:**,
never touching the LS-120. If that stalls too, the LS-120 and its driver are innocent and the
fault is in the bed, the emulator or Windows.

---

## Diagnostics: three brakes removed, and one lesson

The bed was being throttled by its own instrumentation, badly enough to look like a hang:

| | was | now |
|---|---|---|
| `[ECPDIAG] device supplied` / per-nibble register trace | one line **per byte** | removed |
| `MEMWATCH` (VMM thread list, `.PDR` era) | **armed by default**, 3 lines + 2 dumps per hit | `INBOARD_MEMWATCH=1` |
| `HEARTBEAT` (`.PDR` era) | **armed by default**, 16 paged reads per report | `INBOARD_HEARTBEAT=1` |

A folder copy produced **67 MB of log in twenty minutes** and the emulator then advanced about
1 KB of log per 15 s. The owner reported the UI unresponsive and the copy stalled; he was right
about the symptom and it had nothing to do with the driver. Written up as technique 120.

`tools/ls120_bed_run.ps1 -Heartbeat` arms them for one run when something has stopped talking.

---

## The bed

```
86box_upstream/build/src/86Box.exe        branch lpt-epat-bridge
tools/ls120_bed_run.ps1                   fresh cfg + system image + MEDIUM every run
tools/fixtures/LSWRITE.BAT                copies COMMAND.COM (92 KB) to the medium
tools/fixtures/LSBULK.BAT                 copies a 9 MB zip to the medium
tools/fixtures/LSCFONLY.BAT               the same 9 MB copy, C: to C:, never touching the drive
```

The `-Startup` switch drops a batch into the guest's StartUp folder so the bed drives itself
with nobody at the keyboard.

⚠ **A force-killed run loses the guest's write cache.** One run's `DIR` listed a 92,870-byte
file that was not on the medium at all. Verify from the host, after the guest has flushed.

⚠ **Do not put `TIME /T` in a fixture.** It waits for input under Win95's `COMMAND.COM` and
the batch never reaches the next line. Cost one run.

---

## Owed, and process

- ⏳ **@andrew-hoffman** has been replied to on #22 — the IHC retraction, the FIFO fault, and
  the comment rule. He also asked for a **repository summarising cleanup** once the driver work
  is done; the owner's framing is that the history should be facts, wins, mistakes and next
  actions, because noise costs contributor input. **Queued, not done.**
- **New standing rule:** never push a branch to the 86Box fork without testing it AND the
  owner's explicit go-ahead. A fork push runs the full CI matrix and mails him the failures.
  Nothing from 2026-09-13 has been pushed; every commit is local.
- **New `CLAUDE.md` rule:** comments say *why the code is the way it is*, never what it used to
  do. Applied to today's ECP code in the same session; there is a backlog of older ones still
  describing behaviour that no longer exists.

## The next three things

1. **Dump the VMM list at `TCB+6Ch`** and find why the walk does not terminate. This is the
   `.PDR` wedge, now reproducible on demand for the first time — which it never was.
2. **Read the ECP path is still unframed for flow control.** `LS_BlockReadEcp` is a bare
   `rep insb` with no ECR poll; the vendor waits for ECR bit 1 before each chunk. It works in
   the bed only because our model hands bytes over on demand. **Fix before hardware.**
3. Pace the status poll. With the spindle model honest, polls per command fell 3,419 -> 1,968,
   and 1,968 is `LS_SPIN_BSY`. The drive answers in 15 ms while we burn a full unpaced spin.
