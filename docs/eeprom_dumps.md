# EEPROM dumps

Serial EEPROMs read off real hardware during this project. Each was read by a small DOS
program that touches only the device's own ports, and each is checked against something
independent of the read. Emulator `.nvr` files are not listed; they are generated, not read.

| Device | File | Read by | Checked against |
|---|---|---|---|
| 3Com 3C509B-COMBO NIC (93C46, 64 words) | [`drivers/3c509b/eeprom/3c509b_combo_owner_card.bin`](../drivers/3c509b/eeprom/3c509b_combo_owner_card.bin) | [`tools/gen_eedump_com.py`](../tools/gen_eedump_com.py) → `EEDUMP.COM`, on the 5160 | the card's own checksum (word 0Fh) matches; MAC matches the one the packet driver reports |
| Micro Solutions BackPack CD-ROM pod (93C46, 64 words) | [`drivers/microsolutions_backpack/capture/pod_eeprom_93c46.bin`](../drivers/microsolutions_backpack/capture/pod_eeprom_93c46.bin) | [`tools/gen_bpckee.py`](../tools/gen_bpckee.py) → `BPCKEE.COM` | the drive name and serial match strings found separately in the vendor driver's resident memory |

## 3C509B-COMBO

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

## BackPack pod

Bit-banged through the bridge's register `0x06` at chain address 7. Holds the drive
identity `TOSHIBA CD-ROM XM-1502B/2696` and serial `17627007`. Full account in
[`drivers/microsolutions_backpack/README.md`](../drivers/microsolutions_backpack/README.md#-the-eeprom-is-read).
