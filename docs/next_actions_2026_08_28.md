# Next actions — 2026-08-28

Ordered by value per unit of machine time. Cheapest test first; state the cost before taking it.

---

## 1. LS-120 native Windows driver — as a *positive control*, not a template

**Cost: one boot.** The reasoning that led here needs correcting first.

`SD120PPD.SYS` is an **ASPI** driver — a real-mode DOS API layer. IOS did not identify the LS-120
as SCSI. It saw a real-mode ASPI driver it could not vouch for and refused to load miniports **as a
class**, as a safety measure. That is a different layer from a `.MPD` miniport, so it gives no
template for writing one.

What it *is* good for: if the LS-120's own 32-bit driver loads and works, that proves the punt is
genuinely cleared and the path is open for `T130.MPD`. Do this **before** the T130 attempt, so a
T130 failure cannot be blamed on a still-blocked IOS.

## 2. Trantor T130B miniport — the real driver, now in the repo

**Cost: one install in 86Box, on a copy.** [#19](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/19).
Driver committed to [`drivers/trantor_t130b/`](../drivers/trantor_t130b/).

Established by inspection on 2026-08-28, so it does not need re-deriving:

- **It supports the T130B.** `T130.MPD` carries `T13B SCSI Host Adapter`, `T130B`, `2.00` at `0x20a0`.
- **PIO only.** `DMAConfig=0`; imports are `ScsiPortRead/WritePortUchar/Ushort/BufferUshort` and
  `StallExecution`. No DMA import exists. **The 20-bit DMA reach cannot affect it** — the biggest
  risk this machine poses to a storage driver does not apply.
- **Manual install, no PnP ID.** Binds by Have Disk selection, so the card variant cannot cause a
  *detection* failure; only the driver's own probe can.
- **`Polling=1` must be added by hand** — the `[Poll]` section is in `T128.INF`, not `T130.INF`.
  Not optional here: the card is jumpered for no IRQ and all three the INF offers (3, 5, 7) are
  taken by the 3C509B, the SB Pro and the Intek21.
- **`UpdateInis=DoubleBuffer`** writes `DoubleBuffer=1` into `MSDOS.SYS` globally.

Prerequisite: the `IOS.INI` whitelist from [#17](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/17).
Attach a SCSI **CD-ROM**, not a disk. Work on a copy.

## 3. XT-IDE 386 ROM build, and shadowing the option ROM

**Cost: a flash, with a spare card as fallback.** Both are **speed, not mode**.

Be clear about what this cannot do: no 32-bit Win9x driver exists for 8-bit XT-IDE, so the boot
disk stays real-mode whichever ROM is flashed. The 386 build uses instructions the Inboard can
execute; shadowing the option ROM into RAM removes 8-bit bus fetches on every disk access. They
stack, and they make the real-mode path cheaper. They do not remove it. See item 5 for the thing
that would.

The BDA question is already settled — `0040:0075` reads `01`, correct — so that is not the blocker.

## 4. POST 101 at 2688/3072 — one sweep

**Cost: one sweep run.** [#14](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/14).
Both sizes that ever produced `101` collapse to **2048 KB** under the settings-dialog bug fixed
upstream in `9ee5197`, and 2048 is not a valid board size. Run `tools/post101_sweep.ps1` at
`mem_size = 2048` written straight into the cfg. If `101` returns, that is the cause.

## 5. A 32-bit Win9x port driver for 8-bit XT-IDE — newly plausible

**Cost: large. Do not start it casually.** But the feasibility has changed, and the reason is
concrete rather than hopeful. See [`docs/win9x_port_driver_feasibility.md`](win9x_port_driver_feasibility.md).

## 6. 3C509B in 86Box

**Cost: none — done.** [#20](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/20).
Upstream discussion [86Box#6447](https://github.com/86Box/86Box/discussions/6447) already wants it
and has the datasheet, ROM dumps and patent. Our measured values were
[posted there 2026-08-28](https://github.com/86Box/86Box/discussions/6447#discussioncomment-18187167),
along with two things that thread lacked: that the card must work with **no PnP BIOS at all** on an
XT (ID-port contention at `0x110`, not a BIOS PnP service), and that the 2004 QEMU 3C509B patch is
diffed **on top of** the same author's ISA-PnP patch. We are not writing the device model; the
comment says so.

## 7. Fenix's extended-memory reports — one control run

**Cost: one boot of a stock IBM XT.** **Replied to him 2026-08-28** —
[comment](https://github.com/86Box/86Box/issues/7805#issuecomment-5451251670); his report had sat
with zero replies on a closed issue. His comment is about **extended** memory, not conventional;
our 640 KB figure is untouched by it.

Likely expected XT behaviour: a 5160 has **no CMOS**, and the Inboard machine attaches no NVR —
correct. ASQ and the HDC utility read extended size from CMOS `0x17`/`0x18`; 86Box returns `0xff`
for unmapped ports (`src/io.c:335`), so those two bytes read `0xFFFF` KB ≈ **64 MB**, exactly what
he reports. Settle it by running the same utility on a **stock IBM XT** in 86Box. If a plain 5160
does the same, it is not ours. Also worth asking which build he is on — his comment postdates the
08-25 memory fixes but his binary may not.

## Parked

- **SIV re-run.** The only section that would tell us something a datasheet does not is
  `[latency]`, and that is the one that hangs. Not worth machine time against items 1 and 2.
- **XT-IDE ROM dump.** Demoted — it was mainly to identify the build, and that is now known from
  the owner directly. A revert path, not a blocker.
