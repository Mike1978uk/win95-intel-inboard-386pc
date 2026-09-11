# Where everything stands — end of 2026-09-11

The handoff trail stopped at `next_session_2026_09_09.md` while two days of work happened.
That gap is itself the lesson of this session: **stale state reads as regression** — settled
questions get reopened and the owner has to correct them.

**Read this, then the index at the top of `drivers/imation_ls120/TRANSPORT_SPEC.md`, before
touching anything.**

---

## Closed — do NOT reopen

| | |
|---|---|
| **Floppy geometry AND drive letters** | #25, closed **2026-09-07**, fixed in **hardware**: Trantor's option ROM `CA000` → `DA000` (`SW3 OFF, SW4 ON, SW5 ON`) so Sergey's Multi-Floppy BIOS claims `INT 13h`. Verified DOS 6.22 + Win95, both drives reading real media. Drive letters were never broken - that claim was retracted on 2026-09-07 |
| **E1 — where conventional memory lives** | SW1-3/4 now **ON** per Intel's manual. 384 KB of the 640 KB comes off the planar. Read speed unchanged (5290 ticks both ways, ROM reference identical across boots); the gain is **bus footprint**, not latency |
| **LS-120 transport** | INQUIRY reads `MATSHITA LS-120 COSM 04 / 0270`. The old 36-zeros bug was the **CDB going out as register writes instead of a block write** |
| **LS-120 boot hang** | Spin constants were sized against an unmeasured 1 us/access; the real figure is **5.55 us**, making them 5.8 s and 2.9 s |

## The machine, current

- XT-CF runs **XUB 2.1.2 (XT+)**, reflashed 2026-09-04 — `roms/hdd/xtide/ide_xtcf_lotech.bin`.
  ⚠ `roms/xtcf_card/XTCF_D8000_asfound_2026_08_31.bin` is the **stale 2.0.4** image.
- SW1-3/4 **ON**. `BUFFERS=30,0` (raised 2026-09-11; buffers live in the HMA under `DOS=HIGH`).
  Backups on the card: `CONFIG.B4S`, `CONFIG.B4B`.
- `LS120MP.MPD` in `IOSUBSYS` is **phase 0** (`md5 61fcab5d`), which boots.
- The vendor DOS driver is back behind `REM` in `CONFIG.SYS`.

## LS-120 (#22) — the open work

**Everything needed to build it is in `drivers/imation_ls120_mpd/IMPLEMENTATION.md`.** Read
that, not this. Summary of why the last builds failed:

Our driver does device I/O in `HwInitialize`, completes inline in `HwStartIo`, and blocks in
spin loops. **The vendor does none of those** — `HwInitialize` has no device I/O and returns
TRUE in 20 instructions; `HwStartIo` returns without completing; the engine is a
**self-re-arming 1 ms timer** via `ScsiPortNotification(RequestTimerCall)`. The DDK's
`PC2X.C` has the same shape. **Every timeout tuned this session was a constant inside a shape
the OS does not expect, which is why each "allow more time" change made the stall worse.**

Also established: a **CHECK CONDITION is cleared by reading the sense**, not by the next
command; several unit attentions queue after a reset; and **INQUIRY is exempt from them** —
which is why bring-up looked healthy while every read was refused.

And the transport target is **ECP**, not nibble. The card is ECP-capable and the vendor
negotiates `ECP Read`/`ECP Write` here. We implement selector 0 of a 12x5 matrix.

## Optimisation — agreed order, after the LS-120

`docs/bus_optimisation_plan.md` carries **the complete ledger, 26 levers**, both sides of the
connector. Agreed next: **request merging** (1.2-1.5x; 36% of disk time is command setup),
then **the free 4%** (loop overhead and `rep movsd` in buffer paths).

The framing that produced it: **optimise both sides of the connector, and never rank a lever
out for being small.** The card's decode ceiling ended the transfer-width work and said
nothing about transaction count, which is host-side and larger.

## Full disassemblies now in the repo

`drivers/imation_ls120/SD120PPD_MPD.asm` (22,503 instructions, **complete** — section table
proves it) and `SD120PPD_SYS.asm`, with `DISASSEMBLY.md` recording what was read out of them.
`tools/pedis.py` takes any PE driver as its first argument, so the same pass works on
`T130.MPD`, `ESDI_506.PDR`, `SCSIPORT.PDR`.

## What cost time today, so it does not again

1. **Reading an issue's comment thread instead of its state.** `DRIVPARM` was proposed for a
   problem closed four days earlier.
2. **Appending to a 1,100-line spec without reading it.** ECP capability was re-derived on
   hardware when §4f already recorded it — and it cost the owner a boot.
3. **Fetching `epat.c` from the web** when it was in `reference_gpl/`.
4. **Four hard resets** from probe bugs: no `cli` in a probe, clamp any device-supplied
   length, validate DEBUG block layout, explicit jump on every timeout path.
5. **8.3 filenames.** `RAMTIME2B.OUT` truncated and destroyed the baseline half of an A/B.

---

## EPAT bridge in 86Box — where it actually is (added end of 2026-09-11)

Branch **`lpt-epat-bridge`** on `Mike1978uk/86Box`, in the local `86box_upstream/` clone.
`86box_upstream/` is **not pristine** — it carries this project's diagnostic commits too.

| commit | |
|---|---|
| `e6db949` | bridge skeleton — `lpt_device_t` plugin, unlock frame, checkpoints |
| `7bbe8ff` | protocol logging as a first-class feature |
| `a07249a` | register access: direct addressing, nibble read, SRST |
| `ba8b406` | **`RDISK_BUS_LPT` now creates a drive** — see `docs/epat_emulation_plan.md` |

**Next edit, precisely:** `src/device/lpt_epat.c` answers register reads out of a stub
`regs[0x20]` array (`epat_read_status()`, and the write path at the `dev->regs[dev->reg_addr]`
assignment). Replace that with the real drive: call `rdisk_get_lpt_device(port)` at attach,
keep the returned `scsi_device_t *`, and read/write `((rdisk_t *) sd->sc)->tf` instead. The
CDB, once assembled, goes to `scsi_device_command_phase0(sd, cdb)`; CHECK CONDITION is
`sd->sc->tf->status & ERR_STAT`.

**Before any upstream PR:** set `ENABLE_EPAT_LOG` to 0 (technique 93). **Do not submit
upstream without asking first** — and the suggestion to raise an 86Box issue asking whether
they want a parallel-port bridge at all, before writing a thousand more lines, is still
unanswered.

## LS-120 driver — unchanged and still the point of all this

Nothing in the driver moved today. `drivers/imation_ls120_mpd/IMPLEMENTATION.md` remains the
whole build spec, and **§8's order of work is still item 1: restructure to §1** — empty
`HwInitialize`, `HwStartIo` that returns without completing, state machine in the timer
handler. The emulation work exists so that restructure can be debugged with visibility instead
of a boot per iteration.
