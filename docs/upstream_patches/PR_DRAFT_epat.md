# PR draft — Shuttle EPAT parallel-port ATAPI bridge

⛔ **DRAFT. Not submitted.** Needs the owner's yes, and needs the open
questions at the bottom answered first.

---

## Title

> Add a Shuttle EPAT parallel-port ATAPI bridge, and let an emulated LPT device serve ECP

## Body

86Box can attach a CD-ROM or removable disk to a parallel port, but it has no
model of the bridge chips that real parallel-port drives actually used. This
adds one: the **Shuttle EPAT**, the bridge in the Imation SuperDisk LS-120
parallel-port drive and in a good deal of other period kit.

It also fixes a gap that blocks any such device. **The ECP FIFO is filled only
by the chardev passthrough, so an ECP read from an emulated LPT device returns
`0xFF` forever.** There is no path for a device to supply or receive ECP
payload, and no way for it to see the forward address cycle that frames a
block transfer. Three optional `lpt_device_t` callbacks fix that:

| callback | why |
|---|---|
| `ecp_read_data` | supply a byte when the FIFO is empty on a reverse transfer |
| `ecp_write_data` | receive ECP payload on its own path, so a device that frames SPP block writes separately can tell an ECP byte from a stray one |
| `ecp_write_addr` | the forward address/command cycle — the EPAT is commanded this way (`0x80` arms a block read, `0xC0` a block write, `0xA0` the last byte of a read) |

All three are optional. A device that does not set them behaves exactly as
before.

## What it enables

A parallel-port LS-120 can be attached and driven through its real protocol —
CPP unlock, register access by nibble/byte/EPP/ECP, ATAPI packet phases, and
block data — so period software that talks to the bridge directly can be run
and debugged.

## A question for you before you spend time reviewing

`lpt_epat.c` was written against an older `lpt_device_t` and **does its own
strobe and EPP framing**. Since then this tree has gained `strobe`,
`read_ctrl`, `epp_write_data` and `epp_request_read`, which cover some of the
same ground.

I have deliberately **not** reworked it onto those, because I would rather ask
than guess at the shape you want. If you would prefer the device to use the
existing callbacks and drop its own framing, say so and I will rework it -
that is a mechanical change, not a redesign.

## Changed

Nine files, **+1,578 / −21**. `src/device/lpt_epat.c` is new and is most of it.
No existing behaviour is modified: the LPT changes are additive callbacks plus
their plumbing, and the `rdisk` change is an attach path.

## Tested

- **Builds clean on current master** (`5731e20a3`) with GCC / MinGW / Ninja on
  Windows, `-Werror` settings as configured by the project's own CMake.
- **Runs, on a fork.** 86Box boots a Windows 95 guest on `ibmxt_inboard386`
  with an LS-120 attached to LPT1 through this bridge; the guest's miniport
  reaches the drive and the bridge returns the drive's real ATAPI signature
  (`14 EB`). **This was on a build several thousand commits behind master, not
  on master itself** - see "Still open" below.
- Developed against a **real** Imation LS-120 on a real IBM 5160 with a
  Shuttle EPAT bridge, with the emulated bridge's behaviour checked against
  register captures taken from the physical device.

## NOT tested

- **Only the EPAT.** The `EPEZ` and `EPAT+` variants are recognised by name
  but their differences are not modelled.
- **Only one attached device**, a removable disk. CD-ROM over this bridge is
  wired but not exercised.
- **No real-hardware chardev passthrough** through the new ECP callbacks — they
  are used by the emulated device path only.
- Tested on Windows hosts only.
- ⛔ **Run on master, and the drive does not appear.** The vendor driver
  reaches the bridge - unlock frame committed, CPP chain scan runs, unit 0
  answers - and then no drive letter. See `docs/next_session_2026_09_20d.md`.
  **This blocks submission.**
- ⛔ **No non-LPT regression run** on the patched build. The changes are
  reviewed as additive and NULL-guarded, but that is reasoning, not a result.

---

## ✅ Done since the first draft

- **Diagnostics stripped.** Fourteen marked sites removed across `lpt.c`,
  `lpt_epat.c` and `rdisk.c` - the ECP write census, the LBA 0 header dumps,
  the CPP chain-scan trace, and the env-gated motor-stopped model. The patch
  went from +1,693 to **+1,578**, and `rdisk.c` from 113 changed lines to 58.
  Rebuilds clean. Verified: zero occurrences of `DIAGNOSTIC`, `ecpdiag`,
  `RDISK_START_REQUIRED` or `not for upstream` remain in the patch.
- **`hdd.c` duplicate resolved.** Our `case CDROM_BUS_LPT:` was redundant -
  upstream already maps bus value 6 through `TAPE_BUS_LPT`, and says so in a
  comment. Dropped; the minimal diff is 4 added lines.
- **Non-LPT safety reviewed.** The three `ecp_*` callbacks are NULL-guarded at
  every call site and additionally gated on `dev->ecp`; the `ide_channel`
  change only adds an exclusion for the new LPT bus type; the SUPERDISK media
  cases are additive. A device that does not set the callbacks behaves exactly
  as before. **Reviewed, not yet tested** - see below.

## ⚠ Still open BEFORE submitting

1. **Should `lpt_epat.c` use upstream's `strobe`, `read_ctrl`,
   `epp_write_data` and `epp_request_read`?** It predates them and does its own
   framing. A reviewer will ask, and they would be right to. **This is the main
   piece of work left.**
2. ⛔ **THE BLOCKER: the drive does not enumerate on master.** Run on the
   owner's own working Windows 95 image, with the vendor `SD120PPD.MPD` the
   real machine uses, the driver talks to the bridge and then no drive letter
   appears. Submitting a device nobody can get a drive out of is worse than
   not submitting. `docs/next_session_2026_09_20d.md` has the log and what it
   rules out.
3. **Non-LPT regression run.** Boot a plain IDE/ATAPI machine on the patched
   master build and confirm nothing moved. (The ROM-set blocker is gone; the
   build runs.)
4. **`src/config.c`** gains 17 lines. Check those are all genuinely needed for
   the device and not project-local convenience.
