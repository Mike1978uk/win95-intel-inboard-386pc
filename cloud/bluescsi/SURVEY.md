# BlueSCSI v2 survey for the 5160 (issue #50)

Status: survey of source only. Nothing here was run on the 5160.

## Hardware and firmware in use

- Board: BlueSCSI V2 2023.10a, internal 50-pin, Pico 2 W (RP2350 + CYW43439) soldered on.
- Firmware flashed: `BlueSCSI_V2_Universal_2026-08-17_4d0f96f.uf2`, the latest release,
  from https://bluescsi.com/downloads
- Host: Trantor T130B, IRQ 3, Win95 driver `T130.MPD` (Trantor binary).

## Sources read

| Source | Commit | What it gave |
|---|---|---|
| https://github.com/BlueSCSI/BlueSCSI-v2 | `5e64e3b` (2026-10-03), release tag `v2026.08.17` = `4d0f96f` | Everything below |
| https://github.com/ZuluSCSI/ZuluSCSI-firmware | `e020048` (2026-10-12) | Same LUN limit; no LUN work to borrow |
| https://bluescsi.com/docs/ | - | ini options and filename conventions; nothing on LUNs beyond the filename digit |
| https://github.com/PiSCSI/piscsi/wiki/Dayna-Port-SCSI-Link | - | DaynaPort command set reference |

BlueSCSI now builds with CMake and the Pico SDK (fork `bluescsi/pico-sdk-internal`,
branch `v2.2.0-UltraSupport-rel2`), not PlatformIO. See `BUILDING.md` in the repository.
The target for this board is `Pico_2_DaynaPORT` (`PICO_BOARD=pico2_w`).

## 1. LUNs: only LUN 0 exists

Both BlueSCSI and ZuluSCSI fix the LUN count at one:

- `src/BlueSCSI_config.h:93` - `#define NUM_SCSILUN 1  // Maximum number of LUNs supported (Currently has to be 1)`
- `src/BlueSCSI.cpp:616-622` - the fourth character of the filename (`HD51...`) is parsed as a
  LUN, but anything not below `NUM_SCSILUN` is silently dropped back to LUN 0.
- `src/BlueSCSI.cpp:632-635` - a second image on an ID already taken is skipped with
  `-- Ignoring <file>, SCSI ID <n> is already in use!`. This is what the two-image test hit.
  (Earlier in chat I named the wrong log line; this is the right one.)
- `lib/SCSI2SD/src/firmware/scsi.c:704` - any command to LUN > 0 below opcode 0xD0 gets
  CHECK CONDITION, ILLEGAL REQUEST / LOGICAL UNIT NOT SUPPORTED.
- `inquiry.c:242` - INQUIRY to LUN > 0 returns peripheral qualifier 0x7F (no device). This is
  correct SCSI and is what a host scanning LUNs expects for an empty LUN.

All per-device state lives in one `TargetState` per slot (`scsi.h:94`): sense data, unit
attention, reservation, block size, `cfg`. `cfg` points at the image (`image_config_t`,
`BlueSCSI_disk.cpp:1916`). There are 8 slots (`S2S_MAX_TARGETS`, 16 on Wide), and the code
assumes slot index == SCSI ID in several places (`scsiDev.targets[img.getTargetId()]` in
`BlueSCSI_disk.cpp:1115`, `BlueSCSI_cdrom.cpp:1386`; `g_DiskImages[id]`; ini section `[SCSI<id>]`).

An existing mechanism does most of the work already: **`MapLunsToIDs`** (ini `[SCSI]`,
`scsi.c:601-612`, added for a Xebec S1410 SASI bridge). When set, a command to LUN *n* on any
selected ID is redirected to the slot whose ID is *n*, and the LUN is reset to 0, so every
handler below runs unchanged with that slot's image, sense and unit attention. What it lacks
for this machine: the slot for ID *n* also answers selection on the bus as ID *n*, and the
mapping is fixed (LUN *n* -> ID *n*) rather than per selected ID.

## 2. Removable media and media change

Already present for CD, removable (`RE`), Zip (`ZP`), MO and floppy types:

- Eject by START STOP UNIT, by the board button, or by the Toolbox; images cycle through
  files sharing the same prefix (`compare_prefix`, `BlueSCSI_disk.cpp:1088`).
- On tray close, `unitAttention = NOT_READY_TO_READY_TRANSITION_MEDIUM_MAY_HAVE_CHANGED`
  (ASC 28h), posted only when `EnableUnitAttention=1` in `[SCSI]` (`BlueSCSI_disk.cpp:1110-1116`).
- Unit attention is cleared for SCSI-1 hosts that select without ATN (`scsi.c:983-988`).
  **Whether T130.MPD selects with ATN decides whether media change reaches Windows at all.**

So "no medium, then UNIT ATTENTION" as Zip drives do it is mostly there; what is missing is
hot-swap of a file on the SD card or a USB stick as the medium source.

## 3. Disconnect / reselection: not supported

`scsi.c:1345` and `:1352` - `// TODO Support reselection.` / `// Not currently supported!`.
BlueSCSI holds the bus for a whole command and can never contact the host on its own. Any
input device (keyboard, mouse, network receive) must be polled by the host driver. This
answers one of the issue's open questions from the target side regardless of what T130.MPD does.

## 4. Network (DaynaPort)

- Built into the `Pico_2_DaynaPORT` target (`CMakeLists.txt:367-379`, CYW43 in poll mode).
  Enabled by a `NE<id>...` file on the card plus WiFi credentials in `bluescsi.ini`.
- Command set in `lib/SCSI2SD/src/firmware/network.c`: 0x08 read packets (`:227`), 0x09 stats,
  0x0A write packet, 0x0C/0x0D/0x0E enable/disable/config, 0x1A multicast, 0x40/0x80 vendor.
- Read is polled: CDB[5] bit 6 chooses one packet per READ or several (`network.c:228-234`).
- No Windows driver exists. A Win95 NDIS 3 miniport would issue these commands through the
  SCSI layer from a timer. Design only in this stream.

## 5. USB

- The Pico's native USB port runs TinyUSB in **device** mode: CDC serial plus MSC, which
  exposes the SD card to a PC (`src/tusb_config.h:42-47`, `BlueSCSI_msc.cpp`). No host-mode code.
- Host mode on the native port would cost the serial console and SD-card-over-USB, and needs
  a powered hub (the board does not switch VBUS; GPIO 24 only senses it).
- Alternative: Pico-PIO-USB host on two spare pins. On the v2 board the only free GPIOs are
  16/17, the I2C header (`BlueSCSI_platform_gpio_v2.h`; `DisableI2C` in `[SCSI]` releases
  them, `BlueSCSI.cpp:1541`). PIO-USB needs consecutive D+/D- pins, which 16/17 are, plus PIO
  state machines and a core that SCSI, SDIO and CYW43 also use. Feasibility unproven.
- Full speed only (12 Mbit/s) in either case. The T130B at ~175 KB/s is the narrower limit.

## Answer to the two-image test

The stock firmware cannot present two images on one ID; the second is skipped at boot. The
T130B, T130.MPD and CONFIG.SYS were not the cause. Separate IDs (one image per ID) work
with stock firmware today.
