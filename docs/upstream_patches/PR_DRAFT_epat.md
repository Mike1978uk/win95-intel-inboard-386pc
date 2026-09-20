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

## Changed

Nine files, **+1,693 / −21**. `src/device/lpt_epat.c` is new and is most of it.
No existing behaviour is modified: the LPT changes are additive callbacks plus
their plumbing, and the `rdisk` change is an attach path.

## Tested

- **Builds clean** on `5731e20a3` with GCC 14 / MinGW / Ninja on Windows,
  `-Werror` settings as configured by the project's own CMake.
- **Runs**: 86Box boots a Windows 95 guest on `ibmxt_inboard386` with an
  LS-120 attached to LPT1 through this bridge; the guest's miniport reaches the
  drive and issues ATAPI packets over it.
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

---

## ⚠ Open questions to settle BEFORE submitting

1. **Should `lpt_epat.c` use upstream's `strobe`, `read_ctrl`,
   `epp_write_data` and `epp_request_read`?** It predates them and does its own
   framing. A reviewer will ask, and they would be right to. This is the main
   piece of work left.
2. **`hdd.c`**: our `case CDROM_BUS_LPT:` was redundant — upstream already maps
   bus value 6 through `TAPE_BUS_LPT`. Dropped. Confirm nothing else in the
   patch duplicates something upstream grew independently.
3. **Splitting.** The three `ecp_*` hooks are a much smaller review surface
   than the bridge, but hooks with no consumer usually get rejected, so they
   probably have to go together. Decide deliberately.
4. **`src/config.c`** gains 17 lines. Check those are all genuinely needed for
   the device and not project-local convenience.
