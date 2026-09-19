# Handoff 2026-09-20 — LS-120 works. EPP to test, then upstream the bridge.

The LS-120 enumerates reliably under Windows 95 on the real 5160. It took the
vendor's own miniport, configured — not a driver of ours. Full account of how
and why: `docs/next_session_2026_09_19.md`.

---

## 1. What is on the CF, and what each file does

Everything below is on `C:\` unless stated. All were written CRLF and
CRC-verified at the destination.

### The two experiments

| run this | sets `AdapterSettings` to | purpose |
|---|---|---|
| `EPPGO.BAT` | `PORT=0x378 /ni /de /db /sf /dp /dpc /fp /fe` | **the EPP test** |
| `FIXBACK.BAT` | `PORT=0x378 /ni /de /db /sf /dp /dpc /fp` | **back to verified** |

`FIXBACK.BAT` also restores the stock SCSIPORT. It does the registry **first**,
then the driver — reverting the driver first would briefly leave a 16 MB DMA
ceiling against a 4-bit page latch, which is the unsafe combination.

All of these are **real-mode DOS only**, not a Windows DOS box. Reach DOS from
the boot menu: *Command prompt only*.

### Everything else staged

| file | what |
|---|---|
| `ECPBACK.BAT` / `ECPOFF.REG` | registry only → verified config |
| `SPREVERT.BAT` | driver only → stock SCSIPORT |
| `ECPGO.BAT` / `ECPON.REG` | ECP, no IRQ — **hangs init, do not use** |
| `ECPIRQ.BAT` / `ECPIRQ.REG` | ECP + IRQ 7 — boots, **hangs on data** |
| `DMAGO.BAT` / `DMAON.REG` | ECP + IRQ 7 + DMA 3 — boots, **hangs on data** |
| `SCSIPORT.ORG` | stock SCSIPORT, crc `8d5c9698` |
| `SCSIXT.PDR` | patched SCSIPORT, crc `48c2ee88` |
| `SPDEPLOY.BAT` | puts the patched one into `IOSUBSYS` |
| `CONFIG.B4E` | CONFIG.SYS with the DOS driver ACTIVE |
| `ECPSTEPS.TXT`, `DMASTEPS.TXT` | on-console notes |
| `SPC0..5.BIN`, `SPA.BIN` | transfer scratch — **safe to delete** |
| `LS120VEN\` | the vendor install package (INF + `sd120ppd.mpd`) |

`CONFIG.SYS` currently has the DOS driver **REM'd out**, which is what you
want for Windows — a real-mode block driver forces MS-DOS compatibility mode.
Un-REM it only to read the LS-120 from DOS.

---

## 2. The EPP test

```
boot menu -> Command prompt only
C:\EPPGO.BAT
reboot to Windows
```

### Why this is worth a boot

SPP/nibble costs **~4 port accesses per byte**. EPP costs **1**. On this
machine bus transactions are the entire cost model — technique 109 measured
~3.9 us of fixed synchronisation per access — so this is a ~4x cut in the only
thing that actually costs. The same shape of win as XT-IDE Hi-Speed mode.

### Why it is not the thing that already failed

`docs/next_session_2026_09_19.md` retracts technique 75's *"EPP IS CLOSED"*.
What was measured is that EPP **auto-detection** kills the keyboard. Mode 6
(EPP) is the branch that **avoids** the 8259-aliasing ports entirely:

```
0x8d78  cmp byte ptr [0x20bd9], 1   ; EPP path
0x8d81  add dx,3 / in / and 0f8 / or 6 / out    ; base+3 only
0x8d96  jmp 0x8de3                  ; never reaches out 0x23 / out 0x22
0x8d98  cmp byte ptr [0x20bd2], 3   ; mode 3 - THIS is the destructive one
```

`/fe` forces EPP init **without probing**, and it is live: set at `0x341d`,
read at `0x26d4`, one instruction after `/ni`, into the same flags word.
`/de /db` stay on, so detection never runs. The port is known EPP-capable —
measured 2026-09-14, `EPP7_port_is_epp_capable.OUT`.

### What to check, in order

1. **Keyboard.** `/fe` forces a chipset-specific init path that has never run
   here. If the keyboard dies, that is the answer and `FIXBACK.BAT` reverts.
2. **Drive letter.**
3. **A file copy completes** without an egg timer.
4. **VERIFY THE BYTES.** Not optional. Un-REM the DOS driver in `CONFIG.SYS`
   (or `COPY C:\CONFIG.B4E C:\CONFIG.SYS`), reboot to DOS, then:

```
CD \CREATI~1
FC /B WC2P9XUP.EXE D:\CREATI~1\WC2P9XUP.EXE
```

Expect `FC: no differences encountered`. Written by the Windows miniport, read
back by the DOS driver — two different code paths, so a symmetric error cannot
cancel. That is what makes it evidence.

⚠ Keep COMrade commands **under ~50 characters**. Typing stalls past that and
a partial command line looks like a failed command (technique 105). Finish a
stalled line with `keys_send`, do not re-issue — a re-issue produced
`COPYCOPY` on 2026-09-19.

### If it fails

`C:\FIXBACK.BAT`, reboot. You are back on the verified configuration.

---

## 3. Next session's goal — upstream the bridge to 86Box

**This is the outcome that benefits people outside this project**, and the
owner is right that it is the genuinely reusable artefact. 86Box already has
LS-120/SuperDisk drive types in `rdisk.c`; what it has never had is a
**parallel-port bridge to hang them off**. Ours models one.

### What exists

| | |
|---|---|
| branch | `86box_upstream` `lpt-epat-bridge`, **34 commits** ahead |
| device | `src/device/lpt_epat.c`, 1392 lines, `lpt_epat_device` exported in `lpt.h` |
| models | CPP connect/disconnect, chain scan, task-file registers, nibble **and** PS/2 byte mode, ECP registers, block read/write, drive BSY after SRST (`busy_ms`, `reset_ms`) |

### What validates it — this is the strong part

Hardware captures replayed **unmodified** against the model, byte for byte
(technique 116): `docs/captures/2026-09-18_ls120/BM1_*`, `BM2_*`,
`INQ9_from_card.SCR`. A model driven by the same probe that ran on the metal
is a far better fidelity argument than "it looks right".

### What must be cleaned before a PR

1. **Strip the diagnostics.** `lpt_epat.c` lines 622, 658, 1130 are marked
   `DIAGNOSTIC ... remove once X is settled`. They are settled.
2. **`src/disk/rdisk.c` must go back to stock.** My `RDISK_START_REQUIRED`
   stopped-motor model is explicitly `DIAGNOSTIC, not for upstream` — it is an
   `getenv` hack for testing our own driver and has no business in a PR.
   Revert that file entirely.
3. **Decide the device's config surface.** `busy_ms` / `reset_ms` were added
   to reproduce a specific bug. Upstream wants defaults that model a real
   drive, not knobs shaped around our investigation.
4. **Read `.claude/skills/repo-hygiene/SKILL.md` before writing the PR.**
   Title and opening paragraph must stand alone; state what was tested and on
   what; say plainly what was not.

### Honest caveats to put IN the PR, not discover in review

- The model was built to reproduce one drive (Matsushita LS-120 behind a
  Shuttle EPAT) on one host. It is validated against that, not against the
  EPAT family.
- **ECP bulk is modelled but does not work on the real card**, so that path of
  the model is unexercised by hardware. Say so.
- Nibble and PS/2 byte-mode register access are the paths with byte-exact
  hardware corroboration.

### Suggested scope

One PR: the EPAT bridge device plus whatever minimal `lpt.c` plumbing it
needs. **Do not** bundle the Inboard work, the DMA page-width fix, or
anything about our own miniport — every one of those is a separate concern,
and PR #7626 already taught this project that a wide PR loses real fixes in
the noise (technique 41).

---

## Standing state, for anyone picking this up cold

- **Working configuration:** vendor `sd120ppd.mpd` + `PORT=0x378 /ni /de /db
  /sf /dp /dpc /fp`. Enumerates every boot, keyboard alive, write-protect
  reported correctly, **3.4 MB verified byte-identical** across two files.
- **ECP: closed.** Three configurations, both plausible mechanisms eliminated
  with every prerequisite met. Do not re-litigate; the parameter surface is
  exhausted.
- **EPP: open**, staged, untested.
- **Our own miniport** (`drivers/imation_ls120_mpd`) is superseded for daily
  use but still the right vehicle for understanding the transport. Its real
  bugs found on 2026-09-19 — `LS_DrainSense` and `LS_SpinUp` both unreachable
  — are fixed and committed.
