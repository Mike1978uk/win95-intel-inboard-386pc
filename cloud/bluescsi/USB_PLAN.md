# USB on the 5160 through SCSI - plan (issue #50)

Status: plan. Nothing here is built. Line references are BlueSCSI-v2 `5e64e3b` and the SDK's
TinyUSB `86ad6e5`; T130.MPD evidence is in `T130MPD.md`.

## The idea

Win95 OSR1 on the 5160 has no USB stack and the bus could not carry one. The BlueSCSI's Pico
becomes the USB host instead, and presents what it finds as SCSI devices: LUNs behind the one
BlueSCSI ID on the T130B. USB devices come and go; the LUNs do not. Each LUN is a fixed slot,
and a USB device appears as the *content* of its slot - the way a Zip disk appears in a Zip
drive - so Win95 never has to rescan the bus.

| ID 5 LUN | Slot | Backed by | Win95 sees | Win95 driver |
|---|---|---|---|---|
| 0 | Disk | SD card image `HD5...` | Fixed disk | Built in |
| 1 | Second disk (optional) | SD card image `HD51...` | Fixed disk | Built in |
| 2 | USB storage | Whichever stick is plugged in | Removable disk; no stick = no media | Built in |
| 3 | Network | DaynaPORT, over WiFi now, a USB Ethernet adapter later | Network device | NDIS 3 VxD (ours, DESIGN.md §3) |
| 4 | Keyboard and mouse | USB HID | Processor-type device | VxD (ours) feeding VKD/VMOUSE |

Each slot is enabled by an empty marker file named in the existing scheme: `US52.img` (USB
storage), `NE53.img` (network), `HI54.img` (HID). Up to 8 slots in all (`S2S_MAX_TARGETS`).

## The gate: does Win95 address LUNs through T130.MPD?

`T130.MPD` sends no IDENTIFY, so a LUN travels only in CDB byte 1 (`T130MPD.md`). Everything
above depends on Win95 filling that byte. REPORT.md test 1 answers it, in two steps:

1. Default firmware settings (BlueSCSI reports SCSI-2).
2. If LUN 1's commands arrive as LUN 0: `EnableSCSI2 = 0`, so INQUIRY reports SCSI-1
   (`inquiry.c:259-262`). Win95 loads `SCSI1HLP.VXD` on the 5160
   (`docs/bootlogs/BOOTLOG_2026-09-06_t130b_hardware_pass.TXT:252`); my understanding, not
   verified, is that this helper puts the LUN into the CDB for SCSI-1 devices.

If both fail, the same slots go on separate IDs (one per function, LUN 0 each), which needs free
IDs on the chain but no other change.

## What exists, what does not

| Piece | State | Source |
|---|---|---|
| USB host stack | In the Pico SDK's TinyUSB: hub, mass storage (`tuh_msc_read10`/`write10`), HID, CDC serial, vendor | `lib/tinyusb/src/class/msc/msc_host.h:96-112`, `src/host/hub.c` |
| USB host in BlueSCSI | Absent; the port is a USB device (serial console, SD over USB) | `src/tusb_config.h:42-47` |
| USB Ethernet host | Absent from TinyUSB (no CDC-ECM/NCM or ASIX host; partial RNDIS only). A new class driver | `lib/tinyusb/src/class/net/` |
| DaynaPORT frame path | Present, fed by WiFi (`platform_network_send`, inbound queue) | `lib/SCSI2SD/src/firmware/network.c` |
| 5 V at the USB socket | The board feeds `+5VP` to the Pico's VBUS pin (pad 40); budget unknown | `hardware/Desktop_50_Pin/...kicad_pcb`, RP1 |
| Win95 removable-disk support | Present (Zip and Jaz already work through `T130.MPD`) | `docs/captures/2026-10-21_scsicache/` |
| Win95 keyboard/mouse injection | Model only: `references/cfu1_win9x` (MIT) feeds VMOUSE/VKD from a polled USB device | `references/cfu1_win9x/README.md` |

Fixed constraints:
- BlueSCSI cannot interrupt the 5160 and `T130.MPD` would not accept a reselection, so keyboard,
  mouse and network are read at a measured interval (DESIGN.md §3 A).
- Unit attention never reaches Win95 through `T130.MPD`; a stick swap shows as no-media then
  media, and Explorer may need a refresh.
- OSR1 reads FAT16 only: sticks of 2 GB or less, FAT16.
- USB is full speed (12 Mbit/s); the T130B (~175 KB/s) sets the speed for everything.

## Phases

### 0 - parts and choices (owner, no build)
- Micro-USB OTG adapter (micro-B male to USB-A female), powered USB 2.0 hub, FAT16 stick <= 2 GB.
- Accept losing the USB serial console and SD-over-USB in host builds. Flashing is unaffected
  (BOOTSEL is in the RP2350's ROM); logging stays in `log.txt`.
- Run REPORT.md tests 0 and 1 (the gate above).

### 1 - host mode bring-up (firmware, small)
TinyUSB host instead of device (`CFG_TUH_ENABLED`, MSC, HUB, HID), `tuh_task()` in the main loop,
log lines for attach/detach, VID:PID, class, MSC capacity. No SCSI change.
Test: plug and unplug a stick, a keyboard and a mouse through the hub; read `log.txt`. Then the
stick without the hub: does the board's 5 V carry it?

### 2 - USB storage slot (firmware, medium)
`US52.img` makes LUN 2 a removable disk backed by the first USB mass-storage device, through a
USB backend in `ImageBackingStore` so the existing disk code serves it unchanged. No stick:
NOT READY / MEDIUM NOT PRESENT.
Test on the 5160: a removable drive letter; copy 1 MB each way and compare with `fc /b`; unplug,
open the drive (not ready), replug, refresh (F5).

### 3 - keyboard and mouse slot (firmware + probe, then VxD; large)
Firmware queues boot-protocol reports and returns them from LUN 4 with one vendor READ.
A Win32 probe (like `DAYNAPRB.EXE`) prints them and measures READ cost at 14/28/55 ms; mouse feel
at those intervals is the go/no-go. Then the VxD: reads at that interval through the ring-0 SCSI
path (the open question shared with the network driver, DESIGN.md §3) and injects into VKD/VMOUSE
as CFU1 does.

### 4 - network slot over WiFi (VxD; large)
The DaynaPORT already works over WiFi. The NDIS 3 VxD (DESIGN.md §3) is the work; `DAYNAPRB.EXE`
(REPORT.md test 4) is its first proof.

### 5 - USB Ethernet behind the network slot (firmware, medium-large)
A CDC-ECM host class for TinyUSB (bulk frames in and out, the simplest USB Ethernet class) and a
switch in the DaynaPORT path from WiFi to it (`[SCSI] NetworkBackend = USB`, name to be decided).
Win95 sees the same DaynaPORT; no driver change. Needs an adapter that offers a CDC-ECM
configuration - to be confirmed for a specific model before buying. ASIX or Realtek's own
protocols would each be a further driver.

### 6 - more than one of each
Through a hub: a second storage slot (`US55.img`), and so on, up to the 8-slot limit.

## Next steps

1. Owner: Phase 0 parts; REPORT.md tests 0 and 1, including step 2 of the gate if needed.
2. Cloud or local session: Phase 1 build (small); owner runs its bench test.
3. Phase 2, then 3 and 4 in the order the owner prefers; 5 last, since WiFi already covers network.
