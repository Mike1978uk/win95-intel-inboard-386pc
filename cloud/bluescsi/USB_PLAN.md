# USB devices as SCSI devices on the 5160 - plan (issue #50)

Status: plan. Nothing here is built. Line references are BlueSCSI-v2 `5e64e3b` and the SDK's
TinyUSB `86ad6e5`; evidence for the T130.MPD points is in `T130MPD.md`.

Goal: plug USB devices into the BlueSCSI's Pico; the 5160 sees ordinary SCSI devices on the T130B.

## What exists, what does not

| Piece | State | Source |
|---|---|---|
| USB host stack | Present in the Pico SDK's TinyUSB: hub, mass storage (`tuh_msc_read10`/`write10`), HID, CDC serial, vendor. Runs on the Pico's own USB port or on PIO-USB. | `lib/tinyusb/src/class/msc/msc_host.h:96-112`, `src/host/hub.c` |
| USB host in BlueSCSI | Absent. The port is a USB **device** (serial console, SD card over USB). | `src/tusb_config.h:42-47` |
| USB Ethernet host (ASIX, CDC-ECM/NCM) | Absent from TinyUSB; only a partial RNDIS host. Would be a new class driver. | `lib/tinyusb/src/class/net/` (device only) |
| 5 V on the USB socket | The board feeds its own 5 V (`+5VP`) to the Pico's VBUS pin (pad 40); VSYS (pad 39) is unconnected. A device on an OTG adapter is powered by the board, current budget unknown. | `hardware/Desktop_50_Pin/Desktop_50_Pin_TopConn.kicad_pcb`, footprint RP1 |
| Win95 disk driver for a SCSI removable disk | Present: DiskTSD through `T130.MPD`, as the Zip and Jaz already work. | `docs/captures/2026-10-21_scsicache/` |
| Win95 path for keyboard/mouse reports | Absent. Needs a VxD; `references/cfu1_win9x` (MIT) feeds VMOUSE/VKD from a polled USB device and is the model. | `references/cfu1_win9x/README.md` |

Constraints from earlier work:
- `T130.MPD` sends no IDENTIFY, so LUNs travel only in CDB byte 1 and unit attention never reaches
  Win95. **USB storage goes on its own SCSI ID at LUN 0** so it does not depend on REPORT.md test 1.
- BlueSCSI cannot interrupt the 5160 (no reselection, and `T130.MPD` would not accept one), so
  keyboard and mouse input is read at a measured interval, as for the network (DESIGN.md §3 A).
- Full speed USB (12 Mbit/s) is faster than the T130B (~175 KB/s); the T130B sets the speed.

## Phases

### Phase 0 - decisions and parts (owner; no build)

1. A free SCSI ID for USB storage. On 2026-10-21 IDs 0-6 were all in use; ID 6 (scanner) is the
   suggestion, unplugged while testing.
2. Parts: micro-USB OTG adapter (micro-B male to USB-A female); a powered USB 2.0 hub; a USB stick
   of 2 GB or less formatted FAT16 (FAT32 only if the 5160 runs Win95 OSR2 - which build is it?).
3. Accept that the USB host build has **no USB serial console and no SD-card-over-USB**. Flashing
   is unaffected: BOOTSEL uses the RP2350's ROM bootloader. Logging stays in `log.txt` on the card.

### Phase 1 - host mode bring-up (firmware, small)

Build a `Pico_2_DaynaPORT` variant with TinyUSB host instead of device (`CFG_TUH_ENABLED`, MSC,
HUB, HID), `tuh_task()` in the main loop, and log lines for attach/detach, VID:PID, class, and
MSC capacity. No SCSI change.

Test on the bench (BlueSCSI powered, no 5160 needed, or on the 5160 untouched):
- stick in via the hub: `log.txt` shows `USB attach`, VID:PID, `MSC LUN 0: <n> blocks of 512`;
- unplug: `USB detach`; SCSI disks on the card keep working throughout.
- Then without the hub, stick direct on the OTG adapter: does the board's 5 V carry it?

### Phase 2 - USB stick as a SCSI removable disk (firmware, medium)

- New image prefix, e.g. an empty file `US6.img`: ID 6 is a removable disk backed by the first USB
  mass-storage LUN instead of a file. Implemented as a USB backend in `ImageBackingStore`, so the
  existing disk code (`BlueSCSI_disk.cpp`) serves it unchanged; reads and writes call
  `tuh_msc_read10`/`write10` and run `tuh_task()` until they complete.
- No stick: NOT READY / MEDIUM NOT PRESENT (the same path as an ejected image). Stick inserted:
  ready again. No unit attention reaches Win95 (`T130MPD.md`), so Win95 may show a stale directory
  until refreshed.
- Off unless a `US` file is on the card.

Test on the 5160 (REPORT.md to gain a test 5):
1. Boot with the stick in: a removable drive under the T130B in Device Manager; `SCSICACH.EXE`
   lists ID 6.
2. Copy a 1 MB file each way; compare with `fc /b`. Time it: expect about the T130B's rate.
3. Unplug, open the drive (not ready), replug, refresh (F5): new contents shown.
Fail modes to note: no device (INQUIRY), I/O errors (the backend), stale contents after a swap.

### Phase 3 - keyboard and mouse (firmware + VxD, large)

- Firmware: HID host keeps a queue of boot-protocol reports; a SCSI processor-type device (type 03)
  on its own ID or LUN returns them with one vendor READ, empty when nothing happened.
- First a Win32 probe like `DAYNAPRB.EXE` that prints reports and measures the READ cost at
  14/28/55 ms. Mouse feel at those intervals is the go/no-go.
- Then a VxD: reads at the measured interval through the ring-0 SCSI path (the same open
  question as the network driver - `APIX.VXD`, DESIGN.md §3), injects into VKD/VMOUSE as CFU1 does.

### Phase 4 - more than one device

Hub: each mass-storage device or LUN to its own ID or hosted LUN (hosted only if test 1 passes).

### Not planned

- USB Ethernet adapters: no TinyUSB host driver; WiFi DaynaPORT already covers network.
- USB video (DisplayLink): out of reach, as the issue says.

## Alternative if the native port must stay a USB device

Pico-PIO-USB host on GPIO 16/17 (the I2C header, released by `DisableI2C`). Keeps the serial
console, but shares PIO and a CPU core with SCSI, SDIO and WiFi; unproven on this board.
Only if Phase 0 item 3 is unacceptable.

## Next steps

1. Owner: Phase 0 answers (free ID, parts, Win95 build, console trade-off).
2. Owner: REPORT.md tests 0-1 with the current firmware. They do not block USB, but test 0 proves
   the self-built firmware loads, which Phase 1 relies on.
3. Cloud or local session: Phase 1 build; owner runs the bench test.
4. Phase 2 build and test 5 on the 5160.
5. Phase 3 decided from the probe's numbers.
