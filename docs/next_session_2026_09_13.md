# Where everything stands — end of 2026-09-12

Read this, then `drivers/imation_ls120_mpd/IMPLEMENTATION.md`, before touching anything.

---

## THE ONE THING TO DO FIRST

**Boot the machine and read `BOOTLOG.TXT` for a single line: `Initing ls120mp.mpd`.**

The restructured driver is already on the card. Until that line appears, **nothing about the
driver's behaviour can be concluded** — and on the last two boots it did not appear at all.

| if the log says | it means | next |
|---|---|---|
| `Initing ls120mp.mpd` / `Init Success` | it loaded — the restructure can finally be judged | check Device Manager for the drive, then try media |
| nothing at all | IOS skipped it again | Device Manager → SCSI controllers: yellow `!` or greyed out? |

## The machine, current state

- **CF is at `D:` on the PC** (was, at end of session — it needs to go back in the 5160).
- `D:\WINDOWS\SYSTEM\IOSUBSYS\LS120MP.MPD` = **the restructured build, md5 `976e4114`, 7168 bytes**.
  - Backup: `LS120MP.MP0` (md5 `61fcab5d`, the phase-0 build). Revert is
    `copy LS120MP.MP0 LS120MP.MPD`.
  - `LS120MP.NEW` is the same bytes as the deployed `.MPD`; harmless clutter, delete when you like.
- `CONFIG.SYS` has **`SD120PPD.SYS` and `ASPIHDRM.SYS` both REM'd out** — the vendor DOS driver
  must stay out, or it owns `0x378` and IOS silently skips our miniport (`DontLoadIfConflict=Y`).
- `D:\CR0.SCR` staged — a DEBUG script that reads CR0 for red-ray's cache question.
- SW1-3/4 **ON** (E1). XT-CF runs **XUB 2.1.2 XT+**.

## LS-120 (#22) — what happened on 2026-09-12

### Done

- **The driver is restructured to `IMPLEMENTATION.md` §1 and it is proven in the binary**, not
  asserted: `LsInitialize` is **9 instructions with zero port I/O** (returns TRUE — the vendor's
  is 20 and does the same); `LsStartIo` is 68 instructions with **zero**; all 61 port
  instructions live in the transport, reached only from the timer.
- The waits are now **tick counts in a state machine** (`COLDSTART → READY → ISSUE → SENSE →
  DONE`), one bounded step per 1 ms tick. pf.c's 8 s spin-up budget is 8000 callbacks instead of
  a blocked CPU; transport spins cut from ~1 s to ~90 ms because the drive is known idle first.
- `LS_BringUp`'s INQUIRY and sense-drain are **gone from init**. The drain was *hiding* the unit
  attention the class driver is entitled to see and retry on.
- **The 86Box EPAT bridge works** — six commits on `lpt-epat-bridge` at `Mike1978uk/86Box`, all
  pushed. INQUIRY, a byte-verified 512-byte READ(10), and **the "media is not formatted" failure
  reproduced with the same error register (`64`) the real drive returns**. See
  `docs/epat_emulation_result_2026_09_11.md`.
- The bridge is also ported into `86box_full` and builds clean there.

### The correction that matters

I claimed the driver "has never loaded". **Wrong** — `BOOTLOG.OLD` (2026-09-09 14:04) contains
`Initing ls120mp.mpd` / `Init Success ls120mp.mpd`. It has loaded and succeeded. The two 09-11
boots that show nothing are the ones where the **DOS driver was live and conflicted**; that is now
REM'd out. Read *all three* boot logs (`.TXT`, `.PRV`, `.OLD`) before concluding anything.

### Not done / not proven

- **The restructured driver has never run.** No boot with it yet.
- **Writes are untested** everywhere — `PHASE_DATA_OUT` and `epat_pio_request(out=1)` have never
  executed, in emulation or on hardware.
- The Windows-level test in emulation never completed a boot. Causes, in order: `86box_upstream`
  cannot boot Win95 (fixes live in `86box_full`); I dropped `IVT68FIX.COM` from AUTOEXEC; and I
  used CGA, which gave the owner green lines. `86box_full` has **no heartbeat hook**, so there is
  no progress visibility — fix that before spending more runs on it.

## Optimisation — a PER-COMPONENT AUDIT session is planned

⛔ **Do not start it until #22 closes.** Owner's instruction, twice.

`docs/bus_optimisation_plan.md` now carries **ACTIONS OUTSTANDING (A1–A16)**, **THE
PER-COMPONENT AUDIT** (six questions per device + a register), and **THE THIRD STRATEGY** (don't
move the bytes at all — the one not bounded by the card's decode ceiling).

**The Mach8 audit is already DONE** (offline, 2026-09-12) and both of my expectations were wrong:
the accelerator is **on and selected** and driven hard (651 register loads, 63 CMD writes), and it
already uses **43 `rep outsw`** — not lazy programming. Free findings: the card has **1 MB**
(settles half of #8 with no hardware), off-screen VRAM is **~256 KB not ~700 KB**, and VRAM is
`PIX_TRANS`-only so **E4's 4.2× cannot be claimed for video**. The remaining video lever is the
**display mode** (A12a), which is a trade and an owner decision.

**Never examined at all:** `T130.MPD` (#1 in our own occupancy ranking), the six-target SCSI
chain, the 3C509B. First pass on all three is `pedis.py` against binaries already held — no
hardware, no boot. ⭐ Strongest untested lever: **A15, does the SCSI chain DISCONNECT** — a target
that releases the bus during a seek costs the system nothing instead of everything.

## Replies owed

| to | where | what |
|---|---|---|
| **@TC1995** | [#8](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/8) | His last **two** comments are unanswered. He is not blocked on us — he is debugging his own `mix op 0x13`. **The outstanding work is ours**: the 86Box-side diff of the three captured ports, and the 512K/1MB experiment (now half-answered: the card has 1 MB) |
| **@andrew-hoffman** | [#23](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/23) | His DMA-vs-PIO question. Recorded as **E5/A4**; answer it with a **number**, and separately mention the three leads it produced (A3 especially — a correctness question, not a speed one) |
| **red-ray** | VOGONS / [#26](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/26) | Cache: is L1 enabled in CR0? `D:\CR0.SCR` is staged. Also run `C:\CTCHIP\CPUSHOW.BAT` — the batch file itself says the values should read `1000h:0=92 1000h:1=9C 1001h:0=FF 1001h:1=03 1002h:3=03`. ⚠ Lead: CTCHIP reaches those registers through ports **`22h`/`23h`, which alias onto the 8259 on this XT** — if the writes are not landing, the cache is never enabled, which is exactly what he is measuring |
| **disruptor** | #26 | ST01 question, still owed |

## What cost time on 2026-09-12, so it does not again

1. **Reading only the newest `BOOTLOG.TXT`** and concluding the driver had never loaded. The
   evidence was in `BOOTLOG.OLD`.
2. **Not checking my own patch output.** A config edit printed `gfxcard = cga` back at me — the
   replacement had silently failed on line endings — and I carried on. The owner had to point at
   the green lines.
3. **Deviating from the known-good VM config** to save boot time (CGA instead of `mach8_vga_isa`).
4. Dropping `IVT68FIX.COM` from AUTOEXEC — load-bearing for the Win95 boot here.
5. Using `86box_upstream` for a Windows-level test at all.

New techniques from the day: **114** (a `.COM`'s first instruction), **115** (a question can be a
lead), **116** (model the device, and drive it with the unmodified hardware probe).
