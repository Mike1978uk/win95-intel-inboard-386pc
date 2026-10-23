# BlueSCSI v2 on the 5160 - report and test plan (issue #50)

**Built, untested on the 5160.** Read this first; `SURVEY.md` has the evidence, `DESIGN.md`
the reasoning.

## Findings

1. **Why the two-image test failed:** stock BlueSCSI (and ZuluSCSI) supports LUN 0 only
   (`NUM_SCSILUN 1`). The second image on ID 5 is skipped at boot with
   `-- Ignoring <file>, SCSI ID 5 is already in use!`. T130B, T130.MPD and CONFIG.SYS were
   not the cause.
2. **BlueSCSI never disconnects or reselects** (`scsi.c:1345`). It can never signal the 5160
   unprompted, so any network or input driver has to poll.
3. **Removable media change already exists** in stock firmware (`RE` images, eject,
   UNIT ATTENTION with `EnableUnitAttention=1`). No firmware change was needed for it.
4. **DaynaPort already exists** for the Pico 2 W. No Windows driver exists; DESIGN.md §3.
5. **USB host is not in the firmware** at all. Design only; DESIGN.md §4.

## What was built

| File | What |
|---|---|
| `firmware/0001-Add-hosted-LUNs-behind-one-SCSI-ID-EnableLUNs.patch` | The change, against BlueSCSI-v2 `5e64e3b`. 8 files, +167/-10. |
| `firmware/BlueSCSI_Pico_2_DaynaPORT_hostedluns.uf2` | Built for Pico 2 W (`Pico_2_DaynaPORT` target), reports version `2026.08.18-dev` |
| `firmware/SHA256SUMS` | Checksums of both |
| `tools/daynaprobe/DAYNAPRB.EXE` | Win32 probe: DaynaPORT through WNASPI32, ARP round trip, interval cost table (test 4). Not run anywhere yet. |

Toolchain: Ubuntu `gcc-arm-none-eabi` 13.2.1 (BlueSCSI's own builds use 14.x), CMake 3.28,
`bluescsi/pico-sdk-internal` `v2.2.0-UltraSupport-rel2`, `pico-extras` `sdk-2.2.0`. The stock
source built for the same target before the change (1,326,592 bytes); the change adds 3.5 KB.
No compiler warnings in the touched files. Nothing was run: there is no simulator and no
unit test suite in BlueSCSI.

What the change does (opt-in, stock behaviour when off):

- `[SCSI] EnableLUNs = 1` makes the fourth character of an image name its LUN:
  `HD51_data.hda` is ID 5 LUN 1. LUN 1-7.
- Each such image takes a spare device slot that never answers selection on the bus; a
  command to ID 5 LUN 1 is redirected to it, so all existing disk, removable, CD and network
  code runs unchanged per LUN, with its own sense data and unit attention.
- Per-LUN settings in `[SCSI5L1]`; `[SCSI5]` remains LUN 0.
- With `Debug = 1` every command is logged with its LUN: `---- COMMAND: <opcode> (<name>) LUN 1`.

Limits: LUN 0 images plus hosted LUNs share 8 slots. A hosted LUN's block size comes from the
filename (`_512`) or the `[SCSI]` default, not from `[SCSI5L1] BlockSize`. A hosted LUN
cannot be a ROM drive. A hosted removable LUN cycles through every image beginning with the
same three characters, which includes the LUN 0 image of that ID if it shares the prefix.
Use a different type prefix for LUN 0 (`HD5...`) and the removable LUN (`RE52...`).
The `Img0..` image-list keys of a hosted LUN are read from `[SCSI<slot>]`, not `[SCSI5L1]`.

## Flashing and reverting

1. Hold BOOTSEL on the Pico, plug its micro-USB into a modern PC, release. A drive `RP2350`
   appears.
2. Copy `BlueSCSI_Pico_2_DaynaPORT_hostedluns.uf2` to it. The Pico reboots.
3. Revert: the same with `BlueSCSI_V2_Universal_2026-08-17_4d0f96f.uf2` from
   https://bluescsi.com/downloads

**Prove the patched firmware loaded before reading any result** (inboard-hw-debug
Technique 74): `log.txt` on the SD card must show `2026.08.18-dev` and `-- EnableLUNs = Yes`.
A negative result without those lines is not a result.

## SD card for the tests

FAT32 card. `bluescsi.ini`:

```ini
[SCSI]
EnableLUNs = 1
EnableUnitAttention = 1
Debug = 1

[SCSI5L1]
Vendor = "BLUESCSI"
Product = "LUN1 DISK"
```

Images (any size the 5160 already handles; reuse the image that works today as LUN 0):

| File | Becomes |
|---|---|
| `HD5_512.hda` | ID 5 LUN 0, the existing working image, renamed |
| `HD51_512.hda` | ID 5 LUN 1, a second fixed disk (copy of a small formatted image) |
| `RE52_a.img`, `RE52_b.img` | ID 5 LUN 2, removable, two images to swap between (test 3) |

`Debug = 1` logs every command with its LUN, which decides test 1. A `---- MESSAGE_OUT` line
straight after `---- SELECTION` means T130.MPD selects with ATN (SCSI-2); its absence means
SCSI-1, and then unit attention is suppressed by design. That decides test 3.

## Tests on the 5160, in order

Each test: power the BlueSCSI and 5160 from cold, boot Win95, then copy `log.txt` off the card.

### Test 0 - firmware loaded, nothing broken (stock layout)

Card holds only `HD5_512.hda` and an ini with `EnableLUNs = 1`.
- Pass: log shows `2026.08.18-dev`, `EnableLUNs = Yes`; Win95 boots and the drive works as
  it does today.
- Fail: anything different from today. Revert and stop; the change broke LUN 0.

### Test 1 - does Win95 see LUN 1?

`T130.MPD` sends no IDENTIFY message (`T130MPD.md`), so the LUN reaches BlueSCSI only if Win95
puts it in CDB byte 1. If it does not, every command meant for LUN 1 lands on LUN 0 - including
writes. So the first boot is read-only:

1. Back up `HD5_512.hda`. Give `HD51_512.hda` a different volume label and one marker file
   (`LUN1.TXT`) that the LUN 0 image does not have.
2. On a modern PC, set the read-only attribute on both images (`attrib +r`, or Properties >
   Read-only). BlueSCSI then refuses all writes to them (`ImageBackingStore.cpp:113`).
3. Boot, open the new drive letter, and look for `LUN1.TXT` and the label.

- Log at boot: `---- Hosted as ID 5 LUN 1 in slot 0` (slot number may differ). Each command line
  shows `LUN 1` or `LUN 0`: that is the CDB byte, the only LUN this host sends.
- Pass: Device Manager shows a second disk under the T130B; the new drive letter shows
  `LUN1.TXT` and LUN 1's label. Only then clear the read-only attribute on `HD51_512.hda`.
- **Fail - stop:** the new drive letter shows LUN 0's files. Win95 does not put the LUN in the
  CDB; hosted LUNs cannot be used with `T130.MPD` for disks. Fall back to separate IDs.
- Fail, with log showing INQUIRY to LUN 1 answered: Win95 saw it but did not use it. Note
  Device Manager's entry and stop for review.
- Fail, with **no command to LUN 1 in the log**: Win95/T130.MPD does not scan LUNs. This is
  the key negative result. Fallback: separate IDs with stock firmware (`HD4...`, `HD5...`).

### Test 2 - DOS (optional, cheap)

Same card, boot to DOS with the existing ASPI drivers. Does the DOS disk driver show the
LUN 1 drive? Expect not; record either way.

### Test 3 - removable LUN and media change

Add `RE52_a.img` and `RE52_b.img` (different files on each, e.g. `A.TXT` and `B.TXT`).
1. In Explorer, open the LUN 2 drive: `A.TXT` shows.
2. Right-click the drive in Explorer and choose Eject (sends START STOP UNIT). On the
   2023.10a board the eject buttons are on the I2C front-panel header, not on the board.
3. Open the drive again.
Expect no UNIT ATTENTION: `T130.MPD` selects without ATN, and BlueSCSI drops unit attention
for such hosts (`T130MPD.md`). The only signal Win95 gets is NOT READY / MEDIUM NOT PRESENT
while the tray is open.
- Pass: Explorer shows `B.TXT` without a reboot.
- Partial: drive reports not ready, then `B.TXT` only after a refresh (F5). Likely; note it.
- Fail: Explorer still shows `A.TXT` (stale cache). Then a firmware option to keep unit
  attention for SCSI-1 hosts is the next step - SCSIPORT handles CHECK CONDITION, unlike the
  Mac Plus the suppression was written for.
- No Eject on the menu: Win95 does not treat the LUN as removable. Note Device Manager's
  "Removable" box for it and stop.

### Test 4 - DaynaPort through Windows 95's ASPI layer (`DAYNAPRB.EXE`)

Add `NE53.img` (empty file) and WiFi settings in `[SCSI]` (`WiFiSSID`, `WiFiPassword`, per
https://bluescsi.com/docs/). Copy `tools/daynaprobe/DAYNAPRB.EXE` to the 5160 and, in Windows 95
(protected mode, `T130.MPD` loaded), run from an MS-DOS prompt or Start > Run:

```
DAYNAPRB 2 5 3 -arp 192.168.1.1
```

`2` is the T130B's ASPI adapter number (2 in `docs/captures/2026-10-21_scsicache/SCSICACH.TXT`;
the probe prints the count), `5 3` is ID 5 LUN 3, and the IP is your router. With the network
device on its own ID at LUN 0, plain `DAYNAPRB -arp 192.168.1.1` finds it by INQUIRY. Takes about
a minute; writes `C:\DAYNAPRB.TXT`.

- Pass, step by step: `device ... "Dayna"`, a MAC, `interface enabled`, `PASS: ARP reply from ...`,
  then six interval lines.
- The interval table is the result for the driver design (DESIGN.md §3 A): `cmd_us` is what one
  READ costs through `T130.MPD`, `busy%` the share of time spent in READs at that interval, `more`
  how often frames were waiting (interval too long for the traffic).
- Fail at INQUIRY or the MAC: the ASPI path does not reach the LUN; retry with the network
  device on its own ID before anything else.
- Fail at the ARP reply only: check the log for WiFi joined; try another IP on the LAN.

Expect no MESSAGE_OUT lines in `log.txt` for any T130.MPD command: the driver sends no IDENTIFY
(`T130MPD.md`). The probe puts the LUN in CDB byte 1 for that reason.

## Not done

- Win95 DaynaPort NDIS driver (protected mode). DESIGN.md §3 names the first question to answer:
  how a ring-0 driver issues SCSI commands through IOS on Win95. `DAYNAPRB.EXE` proves the
  protocol from ring 3 first.
- USB host (stick as a LUN). DESIGN.md §4; recommended as the next stream once test 1 passes.
- Upstream submission. The patch is shaped for it (opt-in, no change when off), but it should
  only be offered after test 1 and test 3 pass on real hardware, and the PR text is yours.
- Push of this branch: GitHub refused write access to this session (403). See the session
  for the fix.
