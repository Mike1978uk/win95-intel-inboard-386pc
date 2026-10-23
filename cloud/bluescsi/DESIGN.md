# BlueSCSI v2 design for the 5160 (issue #50)

Status: design. Built, untested on the 5160. Line numbers refer to BlueSCSI-v2 `5e64e3b`;
evidence for each claim is in `SURVEY.md`.

Goal: one BlueSCSI ID (5) on the T130B carrying several devices as LUNs - disk, removable
disk, DaynaPort network, later USB storage - so the 5160 sees ordinary SCSI devices and the
Pico does the work.

## 1. Hosted LUNs (firmware, this stream)

### Behaviour

- Opt-in: `[SCSI] EnableLUNs = 1`. Off by default; with it off, the firmware behaves exactly
  as stock. That keeps the change offerable upstream.
- Filename: the existing convention, fourth character = LUN. `HD5_boot.hda` is ID 5 LUN 0,
  `HD51_data.hda` ID 5 LUN 1, `RE52_zip.img` ID 5 LUN 2 removable, `NE53.img` ID 5 LUN 3
  DaynaPort. LUN 1-7. Without `EnableLUNs` the fourth character is ignored, as today.
- Per-LUN ini section `[SCSI5L1]` (Vendor, Product, Type, BlockSize ...), falling back to the
  `[SCSI]` defaults. `[SCSI5]` still applies to LUN 0 only.
- A LUN with no image still answers INQUIRY with peripheral qualifier 0x7F (no device), which
  is what a scanning host expects.

### Mechanism

Each hosted LUN takes a spare device slot, so every existing handler - disk, CD, removable,
network, sense data, unit attention - runs unchanged on it. This is what `MapLunsToIDs`
(`scsi.c:601`) already does for a SASI bridge; the difference is that a hosted slot is never
selectable on the bus and is found by (selected ID, LUN) rather than by ID == LUN.

1. `S2S_TargetCfg` (`scsi2sd.h`) gains `hostId` and `hostedLun`, taken from the 60 reserved
   bytes, so the struct size is unchanged.
2. Loading (`findHDDImages`) defers images with LUN > 0 until all LUN 0 images are placed,
   then puts each in the lowest unused slot. Deferring means a hosted LUN can never take a
   slot that a later `HD2...` file needs for real ID 2.
3. Selection skips hosted slots, in both places it matches IDs: `scsi.c:956` and the PHY
   interrupt `scsiPhy.cpp:76`. A device at the slot's number elsewhere on the bus is unaffected.
4. Command phase (`scsi.c:586`): once the LUN is known (IDENTIFY, or CDB byte 1 for SCSI-1),
   a non-zero LUN that matches a hosted slot switches `scsiDev.target` to that slot and sets
   LUN to 0. Unit attention on the hosted slot is cleared for SCSI-1 hosts, matching what
   selection does for LUN 0 (`scsi.c:983-988`).
5. The drive map in the log prints hosted slots as `ID 5 LUN 1`.

Limits: 8 slots in all (16 on Wide), so LUN 0 images plus hosted LUNs cannot exceed 8. A
hosted slot's number also keys its eject button and Toolbox entry; that is cosmetic here.

### Driver impact (T130.MPD)

None expected. Whether Win95's SCSI layer sends INQUIRY to LUN 1-7 through T130.MPD is the
first question for the 5160 (REPORT.md test 1). If it scans only LUN 0, the fallback is one
device per ID with stock firmware, and the T130 driver's LUN handling becomes its own issue.
DOS ASPI disk drivers may scan only LUN 0 too; that is separate from Win95.

## 2. Removable LUN with media change (configuration, no firmware change)

Already in stock firmware: `RE` prefix = removable disk; eject by START STOP UNIT, the board
button or the Toolbox; NOT READY / MEDIUM NOT PRESENT (ASC 3Ah) while ejected
(`BlueSCSI_disk.cpp:1948`); UNIT ATTENTION 28h on reinsertion when `EnableUnitAttention = 1`.
Several images with the same prefix (`RE52_a.img`, `RE52_b.img`) cycle on each eject.

Open question for the 5160: unit attention is dropped for hosts that select without ATN
(SCSI-1). If T130.MPD selects without ATN, Win95 never sees the change and only learns of it
from MEDIUM NOT PRESENT. The log line `compatMode` tells which.

Not done here: swapping in a file copied to the SD card while running, or a USB stick as the
medium. Both need the USB or WiFi side first.

## 3. Network: DaynaPort (firmware ready, driver design only)

Firmware: the `Pico_2_DaynaPORT` build already has it; with `EnableLUNs` it can sit at ID 5
LUN 3. Nothing to change.

Win95 driver (not in this stream - new driver code):
- NDIS 3 miniport, one VxD. Sends DaynaPort commands (0x08 read, 0x0A write, 0x09 stats,
  0x0C-0x0E enable/config) through IOS to the T130B, never touching the card directly.
- Receive must be polled from a timer: BlueSCSI cannot reselect (`scsi.c:1345`), so it can never
  interrupt. 0x08 with CDB[5] bit 6 set returns several packets per command, which matters at
  ~175 KB/s.
- Unknown: how a ring-0 NDIS driver issues SCSI commands to an IOS-managed device on Win95.
  The two candidates are an IOS IOR with SCSI pass-through and the ASPI VxD (APIX). That
  question decides whether the driver is practical and is the first thing to answer before code.
- What it would replace: the 3C509B, freeing an ISA slot and an IRQ.

## 4. USB (design only)

The Pico's native port is a USB device today (serial console + SD card access). Two routes:

| Route | Cost | Gain |
|---|---|---|
| Native port in host mode (TinyUSB host) | Lose the serial console and SD-over-USB; needs a powered hub | USB stick appears as a hosted LUN; no PC driver |
| Pico-PIO-USB host on GPIO 16/17 (I2C header, `DisableI2C=1`) | PIO state machines and a core shared with SCSI, SDIO and WiFi; unproven | Keeps the native port |

Both are full speed only. A USB stick as a LUN needs no 5160 driver; keyboard and mouse would
need a vendor command plus a polling VxD, as the issue says. Recommendation: do the USB stick
first, on the native port, as a separate stream after hosted LUNs are proven on the 5160.

## Order on the 5160

1. Hosted LUNs: does Win95 see ID 5 LUN 1? (Everything else depends on this.)
2. Removable LUN: eject and reinsert; does Win95 notice?
3. DaynaPort on a LUN: does the firmware join WiFi and answer INQUIRY as a network device?
   (Driver later.)
