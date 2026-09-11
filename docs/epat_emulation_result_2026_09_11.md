# The LS-120 transport runs in emulation — 2026-09-11

A DOS guest in 86Box drove the emulated Shuttle EPAT bridge through a **complete ATAPI
INQUIRY** and read the reply back. The probe was not rewritten for emulation: it is
`docs/captures/2026-09-11_ls120/INQ9.SCR`, **byte for byte the DEBUG script that ran on the
real 5160**, so both sides exercise the same code path (§9 of `IMPLEMENTATION.md`).

This is what #22 needed: the driver can now be developed and debugged with full visibility,
instead of one boot per iteration on hardware a floor away.

## The reply, against the real drive

| INQUIRY field | real MATSHITA LS-120 | emulated | |
|---|---|---|---|
| 0 peripheral device type | `00` | `00` | match |
| 1 RMB (removable) | `80` | `80` | match |
| 2 ANSI version | `00` | `00` | match |
| 3 response data format | `01` | `21` | the emulated drive's own value |
| 4 additional length | `7B` | `1F` | hw offers 127 bytes, emu fills the 36 asked for |
| 8-15 vendor | `MATSHITA` | `86Box   ` | right position, right width |
| 16-31 product | `LS-120 COSM   04` | `86B_RD00        ` | right position, right width |
| 32-35 revision | `0270` | `3.50` | right position, right width |

**Every structural field matches and every string lands in the right byte positions.** A
byte-identical reply was never the goal — the emulated drive is a different drive.

The task file agrees too, which matters more than the strings:

| | hardware | emulated |
|---|---|---|
| interrupt reason after the data phase | `02` (data in) | `02` |
| byte count lo/hi | `24 00` (36) | `24 00` |
| error register | `00` | `00` |
| final status | `80` — **BSY** | `40` — READY |

That last row is a real difference and worth keeping in view: the physical drive still returns
BSY with a clean error register, which is the **spin-up timeout** that is still open against
the driver. The bridge completes immediately by design (it models no drive latency), so
**emulation will not reproduce that timeout.** Tune timeouts against hardware, logic against
emulation.

## The protocol trace, which is the point of the exercise

```
EPAT: CONNECT
EPAT: W reg 16 = 04            <- SRST asserted
EPAT: W reg 16 = 00            <- released
EPAT: device reset: status 50, signature 14 EB
EPAT: W reg 1E = A0            <- drive select
EPAT: W reg 1C = 24            <- byte count = 36
EPAT: PACKET, byte count 36
EPAT: R reg 1F -> 48           <- READY|DRQ, the drive wants the CDB
EPAT: block write start
EPAT: CDB 12 00 00 00 24 00 00 00 00 00 00 00
EPAT: data phase in, 36 bytes, request length 36
EPAT: R reg 1A -> 02           <- interrupt reason: data in
EPAT: block read start
EPAT: command done, status 40
EPAT: DISCONNECT
```

Nothing like this was ever visible on hardware without a logic analyser.

## How to run it again

```
86Box.exe -P <vmdir> -R <roms> -L <vmdir>/86box.log -N
```

`<vmdir>/86box.cfg` needs `lpt1_device = lpt_epat` under `[Ports (COM & LPT)]` and, under
**`[Other removable devices]`**:

```ini
rdisk_01_parameters = 0, lpt
rdisk_01_lpt_port   = 0
rdisk_01_image_path = rd.img
```

Put the probe in the guest with `tools/fatcp.py`, and have `AUTOEXEC.BAT` run it so the whole
thing is unattended:

```
C:\WINDOWS\COMMAND\DEBUG.EXE < C:\EINQ.SCR > C:\EINQ.OUT
```

Then read the result back out with `tools/fatls.py <img> --get EINQ.OUT <local>`.

## What cost time, so it does not again

1. **`[Removable disks]` is not a section.** It is **`[Other removable devices]`**. The wrong
   name parses silently and creates nothing — technique 4, again.
2. **`-V` is `--vmname`, not verbose.** It swallows the following argument and 86Box then does
   nothing, with no error.
3. **Use `C:\WINDOWS\COMMAND\DEBUG.EXE`, not `C:\DOS\DEBUG.EXE`**, on a Win95 image — or keep
   `SETVER` in `CONFIG.SYS`. MS-DOS 6.22's DEBUG refuses to run under DOS 7 and reports it on
   **stderr**, which DOS cannot redirect: the output file is created, stays empty, and the
   failure looks exactly like a probe that ran and found nothing.
4. **Set `BootGUI=0` in `MSDOS.SYS`** or Windows starts over the top of the probe.
5. **The image must be a size the drive type supports.** The generic type takes ZIP-100
   geometry, `96 * 2048 * 512` = 100,663,296 bytes.
6. **Poison the output file and drop stage markers.** `ECHO S1 > C:\S1.TXT` before and
   `S2.TXT` after proved DEBUG ran and exited while producing nothing, which is what pointed
   at stderr. An empty output file on its own says almost nothing.

## Still open, from the same trace

- `unlock frame committed with unknown command 40` / `50` — the probe issues CPP commands
  `0x30 0x40 0x50 0x00 0xE0`. Only `0xE0` (connect) and `0x30` (disconnect) are modelled.
  Harmless here, but they should be recognised or explicitly documented as ignored.
- `W reg 9F out of range` — the trailing `w0(0xFF)` of a CPP frame is being read as a tagged
  register address (`0xFF & 0x60` is non-zero). Cosmetic, but it should not reach the register
  decoder at all.
- **Writes are untested.** Only INQUIRY has run. `PHASE_DATA_OUT` and `epat_pio_request(out=1)`
  have never executed.

---

## READ(10) — and then the actual bug, reproduced

`RD10.SCR` (`CDB 28 00 00 00 00 00 00 00 01 00 00 00` — READ(10), LBA 0, one block) was run
unmodified from the same capture set.

**First, the transport.** The media was poisoned with a position-revealing pattern
(`bytes(range(256)) * 2`) so that zeros could not be mistaken for success — an all-zero image
would have made a failed read look identical to a clean one. The guest read back:

```
1000  00 01 02 03 04 05 06 07-08 09 0A 0B 0C 0D 0E 0F
11F0  F0 F1 F2 F3 F4 F5 F6 F7-F8 F9 FA FB FC FD FE FF
```

All 512 bytes, in order, through the nibble block-read path. A single dropped or doubled byte
anywhere in the block would have shifted the pattern visibly.

**Then the bug.** The physical drive fails this same read, and the emulated drive did not —
because `rdisk_reset()` clears `unit_attention`, so our SRST was wiping the very condition the
hardware raises. A real ATAPI drive raises a unit attention when it is reset. The bridge now
does the same, and the two agree:

| | real LS-120 | emulated |
|---|---|---|
| error register | `64` | **`64`** |
| interrupt reason | `03` | **`03`** |
| byte count | `00 02` | **`00 02`** |
| status | `51` | `41` (differs only in DSC) |
| data | all zeros | all zeros — the signature did **not** come through |

Sense key **6, UNIT ATTENTION**. That is the "media is not formatted" failure, now reproducible
on demand, with the same error register the real drive returns.

`rdisk.c` already implements the whole condition, `ALLOW_UA` included — which is why INQUIRY
succeeds while READ is refused, exactly as `IMPLEMENTATION.md` §3 describes and exactly as it
behaved on the bench. **The driver's recovery path can now be developed against this.**

## What this changes for the driver

`IMPLEMENTATION.md` §8 orders the work: restructure to §1, then §3 error handling. Both can now
be done with a protocol trace in front of you. The one thing that still needs hardware is
**timing** — the bridge completes immediately and models no drive latency, so the spin-up
timeout (§4) will never reproduce here.

## Reusable bed

`vm_epat/86box.cfg.master` is the configuration. The guest is any DOS image with
`C:\WINDOWS\COMMAND\DEBUG.EXE`; the probes are the `.SCR` files in
`docs/captures/2026-09-11_ls120/`, used **unmodified** so that emulation and hardware exercise
the same code path.
