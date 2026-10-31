# Next tests on the 5160 - BlueSCSI at ID 5 in an external enclosure (issue #50)

Everything here is built, untested on the 5160. Do the tests in order; each one needs the one
before it to pass. Background: `REPORT.md` (tests in full), `T130MPD.md` (why the LUN steps),
`USB_PLAN.md` (what these tests unlock). All paths are on branch
`claude/bluescsi-5160-firmware-o12wij`.

## Before you start

1. **Back up** your working disk image and your current SD card.
2. **Flash** before the board goes in the enclosure (it needs the Pico's USB socket): hold BOOTSEL,
   plug the Pico into a modern PC, copy
   `cloud/bluescsi/firmware/BlueSCSI_Pico_2_DaynaPORT_hostedluns.uf2` to the `RP2350` drive.
   To go back: the same with `BlueSCSI_V2_Universal_2026-08-17_4d0f96f.uf2` from
   https://bluescsi.com/downloads
3. **Enclosure:** power the BlueSCSI from the enclosure's supply. Termination jumper `TRM_ON`
   only if the enclosure is the last device on the external chain; `TRM_OFF` otherwise.
4. **ID 5 must be free.** On 2026-10-21 the Jaz drive was at ID 5: move or unplug it.
5. **SD card** (FAT32, root):
   - `bluescsi.ini` from `cloud/bluescsi/sdcard/` - fill in `WiFiSSID` and `WiFiPassword`.
   - `HD5_512.hda` - your working image, renamed.
   - The other files are added per test below.
6. Copy `tools/daynaprobe/DAYNAPRB.EXE` to `C:\` on the 5160 (needed for test 4).

After **every** boot: copy `log.txt` off the card and rename it `log_test0.txt`, `log_test1a.txt`
and so on. The log is the result; the screen is only a hint.

## Test 0 - new firmware, nothing else changed

Card: `bluescsi.ini`, `HD5_512.hda` only.

- Check `log.txt` first: it must contain `2026.08.18-dev` and `-- EnableLUNs = Yes`. Without
  both lines the new firmware did not load and no later result counts.
- Pass: Win95 boots and the ID 5 disk works exactly as it does today.
- Fail: anything different. Revert the firmware and stop; send `log_test0.txt`.

## Test 1a - a second disk as LUN 1 (read-only)

Prepare `HD51_512.hda`: a small formatted image with a different volume label and one file,
`LUN1.TXT`, that `HD5_512.hda` does not have. On a modern PC, set the **read-only** attribute on
**both** `HD5_512.hda` and `HD51_512.hda` (`attrib +r`), so a mix-up cannot write anywhere.

- Log: `---- Hosted as ID 5 LUN 1 in slot ...`.
- Pass: Device Manager shows a second disk on the T130B, and its drive letter shows `LUN1.TXT`
  and LUN 1's label. Then clear read-only on both and copy a file to the new drive and back.
- Fail: the new drive letter shows LUN 0's files (log commands say `LUN 0`) - go to 1b.
- Fail: no second disk at all - note what Device Manager shows; send the log; stop.

## Test 1b - only if 1a showed LUN 0's files

In `bluescsi.ini`, remove the `;` in front of `EnableSCSI2 = 0`. Images stay read-only. Boot.

- Pass and fail as 1a. A pass here means Win95's SCSI-1 helper does the LUN addressing.
- Fail again: LUNs are out with `T130.MPD`; the plan moves to one ID per function. Stop.

## Test 4 - network through Windows 95 (needs 1a or 1b to pass)

Add an empty file `NE53.img` to the card (keep the `EnableSCSI2` setting that passed). In Windows
95, open an MS-DOS prompt and run, with your router's IP:

```
C:\DAYNAPRB 2 5 3 -arp 192.168.1.1
```

`2` is the T130B's ASPI adapter number (it was 2 on 2026-10-21; the first line of the output gives
the count). It takes about a minute and writes `C:\DAYNAPRB.TXT`.

- Pass: lines for `device ... "Dayna"`, a MAC, `interface enabled`, `PASS: ARP reply from ...`,
  then six interval lines.
- Fail at INQUIRY or MAC: send `DAYNAPRB.TXT` and the log.
- Fail only at the ARP reply: check the log for WiFi joined; try another device's IP.

## What to send back

- `log_test0.txt`, `log_test1a.txt` (and `log_test1b.txt`), `log_test4.txt`
- `C:\DAYNAPRB.TXT`
- One line per test: pass or fail, and what Device Manager and Explorer showed.

Those decide the next build: the USB host bring-up (`USB_PLAN.md` phase 1), with USB devices as
LUNs if test 1 passed, or on their own IDs if it did not.
