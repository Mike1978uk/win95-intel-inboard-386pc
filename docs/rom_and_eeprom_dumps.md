# ROM and EEPROM dumps

Firmware and configuration images taken from the cards in this 5160, kept so others with the
same hardware, and emulator authors, have a known-good copy. Emulator `.nvr` files are not
listed; they are generated, not read.

## Option ROMs

| Card | File | MD5 | Notes |
|---|---|---|---|
| ATI Graphics Ultra (Mach8) BIOS, 32 KB | [`roms/video/ATI_MACH8.bin`](../roms/video/ATI_MACH8.bin) | `92700634…1f5c1d8c` | matches the card's `113-11504-002` BIOS; identical to `roms/video/mach8/BIOS.BIN` |
| Mach8, other images | [`roms/video/mach8/`](../roms/video/mach8/) | | `11301113140_*` (8 KB) and an older `11301115150` BIOS (64 KB); what each came from is not recorded |
| Trantor T130B SCSI BIOS v2.14, 8 KB | [`roms/scsi/trantor_t130b_bios_v2.14.bin`](../roms/scsi/trantor_t130b_bios_v2.14.bin) | `67f28e88…99e857c` | a downloaded image, not a read of the card; its first 6 KB match what the card exposes at `CA000h` on the 5160 (CRC-32 `f64b78ef`) |
| Sergey Kiselev Multi-Floppy BIOS 2.2, 8 KB | [`roms/network/Sergey_FDD.bin`](../roms/network/Sergey_FDD.bin) | `df93d1d5…b3997b` | at `D0000h` on the 5160, on his floppy/serial controller. Project: [github.com/skiselev/floppy_bios](https://github.com/skiselev/floppy_bios) |
| Sergey Kiselev Multi-Floppy BIOS 2.7, 8 KB | [`roms/network/Sergey_FDD_v2.7.bin`](../roms/network/Sergey_FDD_v2.7.bin) | `b727f971…ece4c42e` | read from a 2764 EPROM programmed 2026-05-31; not the version the 5160 was running on 2026-09-07 (2.2) |
| Lo-tech XT-CF, as found, 8 KB | [`roms/xtcf_card/XTCF_D8000_asfound_2026_08_31.bin`](../roms/xtcf_card/XTCF_D8000_asfound_2026_08_31.bin) | `86ff8885…f31b18e52` | XTIDE Universal BIOS 2.0.4, read off the card at `D8000h` before reflashing |
| Lo-tech XT-CF, as flashed, 8 KB | [`roms/xtcf_card/IDE_XTP_configured_2026_08_31.bin`](../roms/xtcf_card/IDE_XTP_configured_2026_08_31.bin) | `2512f5a0…4b7c` | XTIDE Universal BIOS r638 XT+, configured for this machine; what the card runs now |

## Mach8 EEPROM for the emulator

[`roms/video/mach8/eeprom_flexview2x_56hz_800x600.nvr`](../roms/video/mach8/eeprom_flexview2x_56hz_800x600.nvr)
(MD5 `f1d81e21…ee920030`) goes in a bed's `nvr/mach8.nvr`. Written by ATI's own `INSTALL.EXE` (M8UTL) →
Set Power Up Configuration, inside the bed, 2026-10-06, to the settings C-INFO reported on the real card:

- Monitor Type: ALR FlexVIEW2X, 56 Hz, 800x600
- Power-up video mode VGA C80; VGA ROM 8-bit; VGA bus I/O 8-bit; coprocessor bus I/O auto-select

Not a read of the card's EEPROM: the settings match, the bytes are what INSTALL writes for them. The card's own EEPROM was read on 2026-10-07 (Serial EEPROMs, below) and differs in 32 of 64 words.
Without it the ROM refuses accelerated modes above 640x480, so ATI's Windows 95 driver (`ATIM8.DRV`)
reports a display adapter problem; DOS tools and the Win 3.x driver set modes themselves and do not
show it. `tools/bed_launch.ps1` warns when a Mach8 bed's EEPROM is blank.
`vxd-patches/realhw_backups/mach8.nvr.configured-by-INSTALL-20260823` is an earlier INSTALL run whose
monitor setting was not recorded; it allows 640x480 only.

## Serial EEPROMs

Each was read by a small DOS program that touches only the device's own ports, and each is
checked against something independent of the read.

| Device | File | Read by | Checked against |
|---|---|---|---|
| 3Com 3C509B-COMBO NIC (93C46, 64 words) | [`drivers/3c509b/eeprom/3c509b_combo_owner_card.bin`](../drivers/3c509b/eeprom/3c509b_combo_owner_card.bin) | [`tools/gen_eedump_com.py`](../tools/gen_eedump_com.py) → `EEDUMP.COM`, on the 5160 | the card's own checksum (word 0Fh) matches |
| ATI Graphics Ultra (Mach8), 64 words | [`roms/video/mach8/eeprom_card_5160_2026_10_07.nvr`](../roms/video/mach8/eeprom_card_5160_2026_10_07.nvr) | [`tools/m8eedump/m8eedump.asm`](../tools/m8eedump/m8eedump.asm) → `M8EEDUMP.COM`, on the 5160, following the option ROM's read routine (3A4Eh) | words 8 and 9 match ScratchPad1 (56EEh = 0820h), which the ROM fills from them at POST |
| Micro Solutions BackPack CD-ROM pod (93C46, 64 words) | [`drivers/microsolutions_backpack/capture/pod_eeprom_93c46.bin`](../drivers/microsolutions_backpack/capture/pod_eeprom_93c46.bin) | [`tools/gen_bpckee.py`](../tools/gen_bpckee.py) → `BPCKEE.COM` | the drive name and serial match strings found separately in the vendor driver's resident memory |

### 3C509B-COMBO

Read through the ISA ID port (`0x110`) with the contention mechanism: ID sequence, then
command `80h|n` and sixteen reads per word. The card is never activated and nothing is
written. Words are stored little-endian.

- Product ID `9450` (COMBO), MAC `00:20:AF:6F:10:5E`, I/O `0x320`, IRQ 3 at the time of the
  read, boot-ROM window `D0000h`.
- Word 13h = `0004`: ISA contention only, Plug and Play disabled.
- Words 20h onward carry the ID string `3Com 3C509B EtherLink III`.
- The secondary checksum (word 17h) does not match the range the technical reference gives
  for it. The primary checksum does.

Used as the EEPROM template of the 86Box 3C509B model; see #42.

### BackPack pod

Bit-banged through the bridge's register `0x06` at chain address 7. Holds the drive
identity `TOSHIBA CD-ROM XM-1502B/2696` and serial `17627007`. Full account in
[`drivers/microsolutions_backpack/README.md`](../drivers/microsolutions_backpack/README.md#-the-eeprom-is-read).
