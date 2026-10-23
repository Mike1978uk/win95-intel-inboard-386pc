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

`T130.MPD` sends no IDENTIFY message (`T130MPD.md`), so the LUN travels only in CDB byte 1 and
the firmware takes it from there (`scsi.c:594`). Whether Win95's SCSI layer and disk/CD layers fill
that byte for LUN 1-7 is the first question for the 5160 (REPORT.md test 1, read-only first). If it scans only LUN 0, the fallback is one
device per ID with stock firmware, and the T130 driver's LUN handling becomes its own issue.
DOS ASPI disk drivers may scan only LUN 0 too; that is separate from Win95.

## 2. Removable LUN with media change (configuration, no firmware change)

Already in stock firmware: `RE` prefix = removable disk; eject by START STOP UNIT, the board
button or the Toolbox; NOT READY / MEDIUM NOT PRESENT (ASC 3Ah) while ejected
(`BlueSCSI_disk.cpp:1948`); UNIT ATTENTION 28h on reinsertion when `EnableUnitAttention = 1`.
Several images with the same prefix (`RE52_a.img`, `RE52_b.img`) cycle on each eject.

Settled by `T130MPD.md`: T130.MPD always selects without ATN, so BlueSCSI drops unit attention
for it and Win95 learns of a change only from MEDIUM NOT PRESENT. If that proves not enough
(REPORT.md test 3), a firmware option to keep unit attention for SCSI-1 hosts is the fix.

Not done here: swapping in a file copied to the SD card while running, or a USB stick as the
medium. Both need the USB or WiFi side first.

## 3. Network: DaynaPort, protected mode only

Owner's decision (2026-10-23): no real-mode driver. The Win95 driver is an NDIS 3 VxD; nothing
in `CONFIG.SYS` or a DOS box.

### Prior art

| Driver | Host | What it shows |
|---|---|---|
| `DP.COM`, https://github.com/mbbrutman/daynaport-dos-packet-driver-asm (GPL-3.0) | DOS, over ASPI | The command sequence works on a PC. Real mode, so not used; reference only. Polls at 18 Hz, or on DOS idle. |
| https://github.com/jflitton/daynaport-scsilink-linux-driver | Linux | A kernel driver against BlueSCSI v2 and ZuluSCSI. |
| `scsidayna.device`, https://aminet.net/package/driver/net/daynascsi | Amiga | Reaches the device through the OS's SCSI layer, as the Win95 VxD must. |
| Apple's DaynaPORT driver | Mac | Polls. BlueSCSI's own comment, `network.c:228-234`: "polled mode (old SCSI Manager, VM present)". A Mac SE does this with less CPU than the 5160. |

Every known DaynaPORT host driver polls. Network over SCSI on a machine this size is proven; the
question here is only how often to ask and what each ask costs on this bus.

### Commands (verified in `lib/SCSI2SD/src/firmware/network.c`)

| CDB | Use |
|---|---|
| `08 00 00 hi lo C0` | READ: each frame is a 6-byte header (length, 3 zero bytes, flags; 0x10 = another follows) then the frame. Empty queue: 6 zero bytes. `C0` allows several frames per READ (at most two full ones). |
| `0A 00 00 hi lo 00` | WRITE one frame, length in the CDB. |
| `09 00 00 00 12 00` | MAC address (first 6 of 18 bytes). |
| `0E 00 00 00 00 80` / `..00` | Enable / disable; enabling empties the queue. |
| `0D` | Add a multicast address. |

The firmware queues 20 frames (`network.h:27`; 12 only on the S/PDIF build) and drops beyond that.

### Receive: three ways, cheapest to build first

**A. Fixed, measured interval (owner's proposal).** The VxD issues a READ every T ms from a VMM
timer, never in a loop. This is still asking rather than being told, but its cost is fixed and
known in advance: one empty READ is a selection, a 6-byte CDB, 6 data bytes and status, and the
probe measures what that costs through `T130.MPD` (`tools/daynaprobe`, `busy%` column). Two rules
bound T:
- frames arriving faster than 20 per T are lost, so T comes down while READs return frames or
  the 0x10 flag (backlog), and goes back up when they come back empty;
- the Windows tick on the 5160 is 13.7, 27.5 or 54.9 ms (`docs/captures/2026-10-10_timer/`), so
  T is a multiple of it unless the VxD programs its own timer.
Works with stock firmware and any `T130.MPD`.

**B. Interrupt by reselection (no empty READs). Not possible with `T130.MPD`** - it sends no
IDENTIFY, so never grants disconnect, and never arms the 53C80 for reselection (`T130MPD.md`).
Kept below as the design for a future T130B miniport of our own. The VxD issues one READ with disconnect
privilege and leaves it outstanding. With nothing queued, BlueSCSI disconnects and frees the bus;
when a frame arrives it reselects the T130B, the 53C80 raises IRQ 3, and `T130.MPD` completes the
READ. Zero bus time while idle. Needs, in order of cost to find out:
1. The miniport sends IDENTIFY with disconnect privilege. `T130.MPD`: no.
2. The miniport arms Select Enable and handles reselection. `T130.MPD`: no; its IRQ only ends the
   command in progress.
3. BlueSCSI reselection. `scsiDisconnect()` and `scsiReconnect()` exist from SCSI2SD but are
   commented out (`scsi.c:1495-1600`, `/* TODO REENABLE`). Bringing them back for the network
   device alone, on its own ID, is the firmware work; the single-command main loop must then
   accept other IDs' commands while the READ is parked.
4. IOS timeouts: a parked READ can wait minutes. The VxD sets a long timeout and re-issues
   on expiry.

**C. B with A as fallback** when 1 or 2 fails.

Decision: A is the design. B needs our own T130B miniport as well as firmware reselection.

### Ring 0

Unknown: how a VxD sends a SCSI command to a device another driver owns. Lead: `APIX.VXD` already
does exactly this for `WNASPI32`, and loads on the 5160 with `T130.MPD`
(`docs/bootlogs/BOOTLOG_2026-09-06_t130b_hardware_pass.TXT`). Read the Win95 DDK's IOS pass-through
request, or disassemble `APIX.VXD` from the card. Answer it before any VxD code.

### Proving it before the VxD

`tools/daynaprobe/DAYNAPRB.EXE` (Win32 over `WNASPI32`, protected mode) does each step the VxD
will: find the device, read the MAC, enable, send an ARP probe and wait for the reply, then read at
0, 14, 28, 55, 110 and 220 ms intervals and report frames, backlog and time spent in commands.
REPORT.md test 4.

What it would replace: the 3C509B, freeing an ISA slot and an IRQ.

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
