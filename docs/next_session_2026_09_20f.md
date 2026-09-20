# Handoff — 2026-09-20f

Read this one first. Supersedes `next_session_2026_09_20e.md` for *what to do
next*; that file keeps the measurement detail.

---

## Where to pick up

**The clean bed is built and has never been run.** `vm_clean_epat/` — a
generic 486 (`ami495`), a boot disk, the EPAT bridge on LPT1, the LS-120, and
**nothing else**. No SCSI chain, no changer LUNs, no XT-IDE, no Inboard.

Boots straight to DOS (`BootGUI=0`) with the vendor driver in `CONFIG.SYS`:

```
DEVICEHIGH=C:\SD120PPD\SD120PPD.SYS /port:378 /IRQ:7 /de /db /ni /sf /dpc /dp /fp /fe
DEVICEHIGH=C:\SD120PPD\ASPIHDRM.SYS
```

`AUTOEXEC.BAT` writes `C:\EPPROBE.TXT`. Config kept as
`tools/fixtures/86box.cfg.clean_epat`; the VM directory is gitignored.

⚠ **Two assumptions in it need the owner's eyes, which is why it was not run:**

1. **The drive letter is a guess.** The probe looks at `D:`. With no SCSI
   chain that should be right, but drive mapping has been wrong twice today.
2. **The media read is unconfirmed.** `rd.img` is the verified-good LS-120
   image, but "the probe found a file" has not been demonstrated on this
   machine.

`INBRDPC.SYS` is deliberately absent from this `CONFIG.SYS`. That is **not**
the forbidden "disable INBRDPC as a test variable" — this machine has no
Inboard, and the driver would be meaningless or harmful on it.

---

## The open question: why no drive in the bed?

Three theories, all wrong, and the history matters more than any of them:

| theory | disproved by |
|---|---|
| the card carries a stale trace build | the registry: **zero** `PortDriver` entries name `LS120MP`; the bound driver is `sd120ppd.mpd` (`08104ffb`, pristine) |
| the model lacks EPP | implemented it — **zero EPP lines**; the driver never gets that far |
| ? | — |

What actually happens, every time:

```
EPAT: unlock frame committed ... CPP init
EPAT: CPP unit 0 id -> FFAA
EPAT: CPP unit 1..7 id -> 0000
(no CONNECT, nothing further)
```

A working run reaches `EPAT: CONNECT` / `LPT1 drive attached`. So the driver
scans the chain, dislikes the result, and stops **before choosing a
transport**.

**Cheapest next check first:** does the bed's LPT base address actually match
the `PORT=0x378` the driver is told to use? Rule it out before touching the
scan. The boring explanation has won repeatedly today.

**Then, offline:** what does the vendor driver compare the scan ID against?
`drivers/imation_ls120/SD120PPD_SYS.asm` at `0x2767` decodes the command
(`and al,0xF8`; `cmp al,0x10` unit group) and stores an ID at `[0xbbe]`, a
byte at `[0xbbd]`. What it tests them against is the unanswered part.

---

## Standing rules confirmed today

- ⭐ **None of the parallel work is Inboard-specific.** A stock VM is the
  test and a pass there is a pass. Framing it around this project is a
  weakness, not provenance — PR #8010 has been cleaned of it.
- ⭐ **Read `PortDriver`, not the file list.** A driver in `IOSUBSYS` is not a
  driver in use. IOS binds by registry.
- ⭐ **Read `lpt_ditto.c` before proposing anything parallel.** It already
  implements the BackPack protocol *and* both EPP callbacks.
- **GitHub is a channel with rules too.** Five notifications went onto #8010
  in one evening, one of them a correction of the message before it. Verify
  first, post once, batch edits.
- **Build escapes, do not type them.** The backslash-through-heredoc trap
  (technique 84) bit three times today; `chr(92) + "n"` is what works.

---

## State of the tree

| | |
|---|---|
| 86Box PR **#8010** | open at `2f31e6f85`, framing cleaned, CD-ROM claim corrected |
| branch `epat-epp` | `d38a636c3` — EPP wiring, builds, **not** in the PR |
| issues | **#28–#38** open; #32 answered, #35 closed out, #18 parked |
| your CF | needs nothing — vendor driver, `/fe`, EPP already on |

## Closed today, do not reopen

- ⛔ **EGACACHE does nothing** — measured A/B, switch verified on the line and
  a real 32 KB ROM verified at `C000:0000`.
- ⛔ **Only `0xF000` is shadowed.** No configuration moves an option ROM off
  the bus. `0xD8000` cannot be shadowed: the Inboard reserves two windows and
  an arbitrary option ROM is not one of them.
- ⛔ **`T130.MPD` already uses string I/O** on the data path.
- ⛔ **3C509B already drains in bulk** (`ELNK3.VXD`/`.DOS`, `REP` string I/O).
