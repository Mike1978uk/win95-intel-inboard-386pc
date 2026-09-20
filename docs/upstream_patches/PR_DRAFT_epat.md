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

All of the following on **current master** (`5731e20a3`), patched, built with
GCC / MinGW / Ninja on Windows.

- **The drive works.** A Windows 95 guest on `ibmxt_inboard386` with an
  LS-120 on LPT1 through this bridge: the drive enumerates as `J:`, its
  directory reads, and a 92,870-byte file copied onto it landed - free space
  fell 124,841,984 -> 124,747,776, which is 92,870 rounded to the cluster. So
  enumeration, read and write all cross the bridge.
- **Non-LPT regression: nothing moved.** The same guest booted with LPT1 set
  to a plain `text_prt` printer, no EPAT device and no LPT drive, reaches the
  desktop with its IDE disk and SCSI devices intact.
- **The SuperDisk works without the bridge.** Attached as an ordinary SCSI
  removable disk, with no EPAT device in the config at all, it mounts and
  reads. The `rdisk` half of this patch therefore stands on its own.
- Developed against a **real** Imation LS-120 on a real IBM 5160 with a
  Shuttle EPAT bridge, with the emulated bridge checked against register
  captures from the physical device.

## NOT tested

- **Only the EPAT.** `EPEZ` and `EPAT+` are recognised by name; their
  differences are not modelled.
- **CD-ROM over the bridge** is wired but never exercised.
- **`RDISK_TYPE_ZIP_750` becomes selectable** as a side effect. The three
  disabled types are one contiguous enum block, so enabling the SuperDisk
  pair alone would renumber the others and break existing configs. The Zip
  750 is therefore switched on untested. Say the word if you would rather it
  stayed off and I will find another shape.
- **The Qt "new image" dialog still cannot create SuperDisk media.**
  `rdisk_types[]` and `KNOWN_RDISK_TYPES` remain at 4 behind their own
  `#if 0`, so the drive type is selectable but its media is not creatable
  from the GUI. Left alone deliberately: this build is `QT=OFF`, so a change
  there would be untested by me.
- **No real-hardware chardev passthrough** through the new ECP callbacks.
- Windows hosts only.

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
   `epp_write_data` and `epp_request_read`?** It predates them and does its
   own framing. Asked in the body rather than guessed at; mechanical to
   rework. **Not a blocker.**
2. **`src/config.c`** gains 17 lines. Confirm each is genuinely needed for
   the device and not project-local convenience.

## ✅ Cleared

- **The drive enumerates on master** - `J:`, read and write. This was the
  acceptance bar and it is met.
- **Non-LPT regression run** - passes.
- **The `[0117:0000B929] Illegal instruction` seen during bring-up is not
  ours.** It appears identically on a config with no EPAT device, so it is
  pre-existing in that guest.
- **ROM-set blocker** - gone; master builds and runs here.
