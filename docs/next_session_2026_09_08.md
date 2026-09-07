# Session start — 2026-09-08

Rewritten 00:15 on 2026-09-08, at the end of the session that closed #25. Read top to bottom.

**One job in front: the LS-120 (#22). One free re-test first: #18.**

---

## 1. What changed last session

### ✅ #25 CLOSED — and the fix was hardware, not software

`INT 13h AH=08h` reported **720K for both floppies** on a machine with a 1.44 MB 3.5" and a
1.2 MB 5.25". Root cause: **option ROM scan order**.

Sergey's Multi-Floppy BIOS claims `INT 13h` only when `INT 13h` is still the stock `F000:EC59` —
its own source comment reads *"this means that no hard drive BIOS was installed"*. Trantor at
`CA000` was scanned first, claimed `INT 13h`, so Sergey settled for `INT 40h`. Nothing in the
`INT 13h` chain routed there, and the **1986 system BIOS** answered from its own 720K table.

**Fix: one DIP switch. Trantor's ROM `CA000` → `DA000` (`SW3 OFF, SW4 ON, SW5 ON`).** Sergey is
then scanned first, its rule fires, and it owns `INT 13h`.

Verified end to end on **DOS 6.22 and Windows 95** — correct geometry, both drives reading real
media, no crashes. Storage stack untouched: `Init Success xtidemp.mpd`, `rmm.pdr` never reaching
`INITCOMPLETE`, `IOS.LOG` absent.

Evidence: [`evidence/int13_ah08_ROOT_CAUSE_2026-09-07.md`](evidence/int13_ah08_ROOT_CAUSE_2026-09-07.md).

### Also fixed, both confirmed by re-running

- **CPU register batch error.** `COMMAND.COM` honours redirection *inside* `REM` lines. A `->` in a
  comment wrote a junk file named `ONLY` on every boot since 2026-08-24; a `<-` in another caused
  the `File not found`. **Never put a bare angle bracket in a `.BAT`.**
- **TSLCD error.** `MSCDEX /d:TSLCD` was orphaned — `TSLCDR.SYS` REM'd out of `CONFIG.SYS` since
  the SCSI chain came out. Now REM'd with the reasoning inline.

### Machine state

| segment | occupant |
|---|---|
| `C0000-C7FFF` | Mach8 (32 KB) |
| `C8000`, `CA000` | free |
| `D0000` | Sergey — **owns `INT 13h`** |
| `D8000` | XTIDE — boots the CF |
| `DA000` | Trantor — **ROM does not initialise here**; SCSI works via its drivers |

`FD08FIX` (the software fix built before the real one was found) is **removed from the card** —
binary deleted, `IOS.INI` `[SafeList]` entry taken out. Source kept in `fd08fix/` as the
documented alternative for anyone who cannot reorder their ROMs.

---

## 2. FIRST: re-test #18. It may already be fixed, and it costs one boot.

**#18 is floppy data corruption — wrong bytes, not a failure.** Every measurement of it was taken
while the BIOS was reporting **the wrong geometry for both drives**.

That is a plausible common cause: a transfer driven against 9-sector/720K parameters on a drive
that is really 18-sector/1.44 MB is exactly how correct-length, wrong-content results arise.

**Re-run the #18 corruption test before anything else on the floppy side.** The harness needs no
special tooling — `COPY` plus `file_hash` over COMrade, no host transfer:

1. hash a source file on `C:`
2. copy it to a floppy, hash the copy
3. re-hash the copy — proves the read path is stable
4. write a **second** copy to different sectors, hash that
5. re-hash the source — proves `C:` did not drift

Each step kills exactly one explanation. If #18 is gone, the release blocker cleared itself as a
side effect of the #25 fix.

---

## 3. THEN: LS-120 (#22) — specification target settled

### Established

| | |
|---|---|
| bridge | **SHUTTLE EPATRM** |
| drive | Matsushita LS-120, firmware **COSM 04** |
| port | `0x378`, IRQ 7, DMA 3 — Intek TK9901, ECP-capable (measured: ECR `0x77A` answers `0x35`) |
| transfer mode | **ECP Read / ECP Write** — the driver's *free* choice, with `/de` and `/sf` removed |
| `dmaEn` | **0** — the vendor uses PIO, and DMA must stay off here (technique 62) |
| keyboard | **safe with `/ni`** — PIC IMR `0xAC`, unchanged across three measurements |

⚠ `/ni` skips the chipset init **and the drive still works**, so the writes that aliased onto the
PIC and killed the keyboard in #22 are **unnecessary, not merely avoidable**. Our driver will not
contain that code at all.

### Offline work — no hardware needed

All local and md5-matched to the card (`SD120PPD.SYS` = `cbb42e8eb7847869e274e45f258cf718`).

1. **Find the ECP entries in the mode tables.** Read table `0x4E9D`, write table `0x4F15`,
   stride 8. Match entries whose name pointers are `ECP Read` (`0x3cba`) and `ECP Write`
   (`0x491e`). **Only 10 entries of each were dumped; there are more.**
2. **Disassemble those two handlers** — that is the entire transport we need. Ignore the other
   modes.
3. **Find the connect sequence** preceding `0x25c1` — the `0x2da5` cluster and the callers of
   `0x25a0`. Hand-running `0x25c1` failed because it is the tail of a branch, without the connect
   logic before it.
4. **Write the specification, then implement fresh.** Licence decision: **stay MIT,
   reverse-engineer.** Never transcribe.

### Known primitives

```asm
244b  READ  reg:  bx = [0BD9] * 8 ; al = reg (from dl) ; dx = [0BFA] LPT base
                  call word ptr cs:[bx + 4E9Dh]
2472  WRITE reg:  bx = [0BDB] * 8 ; ah = value ; al = reg ; dx = [0BFA]
                  call word ptr cs:[bx + 4F15h]
```

`[0BFA]` LPT base · `[0BD7]` port type (`0x0C` = ECP-capable) · `[0BD9]`/`[0BDB]` read and write
method selectors.

### Size target

The vendor `.MPD` is 79,872 bytes because it **detects**; we can **hardcode** — port, IRQ, bridge,
mode and "no chipset init" are all known. Skeleton (5,120) + ECP transport + ATAPI ≈ **8–12 KB**.

### Still needs hardware

- The connect sequence verified against the bridge.
- ⚠ **Corruption re-test on a fresh NOS disk.** Two writes of one source gave two *different*
  wrong CRCs while the source re-read identically — non-deterministic, so not a systematic
  transport bug. The disk was **known-bad and previously recovered**, so the result is
  uninterpretable. Unresolved.

---

## 4. Optional / owed

- **`DE000`** — untried Trantor ROM address (`SW3 ON, SW4 OFF, SW5 OFF`). Above `D0000` so it keeps
  the #25 fix, and may restore the SCSI option ROM that `DA000` does not. Judged from POST banners
  alone.
- **Michal Necasek is owed a reply** on his 8514/A article, which reframed issue #8 — pending the
  512 KB Mach8 experiment. See `contributor_input_ledger.md`.
- **Sergey card ROM-address switches do not respond** (SW2.4/SW2.5 changed, no effect; prior
  capacitor failure on that card). **Irrelevant to #25 now** — recorded so it is not rediscovered.
  If that card is ever repaired or rebuilt, Sergey at `C8000` with Trantor left at `CA000` is the
  tidiest arrangement.

---

## 5. Method notes earned last session

- **Do not read the option-ROM map with a memory manager loaded.** 386MAX maps UMB over free
  regions and hides ROMs. A wrong conclusion came from exactly that.
- **A `.COM` starts executing at `0100h`** — the first instruction must jump to the installer, not
  the resident handler. Getting that wrong hung the machine twice.
- **A handler returning by anything other than `IRET` must restore `IF` itself.**
- **Never install an untested `INT 13h` hook from `AUTOEXEC.BAT`** — test it from the prompt, where
  failure costs a reboot rather than an unbootable card.
- **COMrade calls are serial — never issue them in parallel.** `.BAT` and `.SCR` files written over
  the bridge must be CRLF.
- **Prove a zero result could have matched.** A `re.escape()` on a raw-bytes pattern reported 0
  hits and nearly hid the mode tables entirely.
