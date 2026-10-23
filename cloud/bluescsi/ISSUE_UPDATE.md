Draft for issue #50. Not posted - for the owner to edit and post.

---

Status on the BlueSCSI side. Everything below is built, untested on the 5160.

**Why two images on one ID did not both appear:** stock BlueSCSI, and ZuluSCSI which it derives from, support LUN 0 only (`NUM_SCSILUN 1` in `src/BlueSCSI_config.h`). The second image is skipped at boot as "SCSI ID already in use". The T130B and T130.MPD were not the cause.

**What was built:** an opt-in firmware change, `[SCSI] EnableLUNs = 1`, where an image named `HD51_...` answers as LUN 1 of ID 5. Each extra LUN reuses a spare device slot that never answers on the bus, so the existing disk, removable, CD and DaynaPort code runs unchanged per LUN. With the option off the firmware behaves as stock. Patch and a UF2 for the Pico 2 W are in `cloud/bluescsi/` on branch `claude/bluescsi-5160-firmware-o12wij`, with the test plan in `REPORT.md`.

**What the source settled:**
- BlueSCSI never disconnects or reselects, so it can never interrupt the PC. Network receive and any input device have to be polled, as you pointed out about bus time.
- Removable media change (no medium, then UNIT ATTENTION) is already in stock firmware. Whether it reaches Win95 depends on T130.MPD selecting with ATN, which the test log will show.
- DaynaPort already runs on the Pico 2 W. A Win95 driver is the missing part, and the first question is how a ring-0 NDIS driver sends SCSI commands through IOS.
- USB host is not in the firmware. A USB stick as a LUN is the sensible first step, after LUNs are proven on the 5160.

**Next, on the 5160:** does Win95 through T130.MPD see ID 5 LUN 1 at all. If it does not scan LUNs, the fallback is one device per ID with stock firmware.

Sources: https://github.com/BlueSCSI/BlueSCSI-v2 (source, commit 5e64e3b), https://github.com/ZuluSCSI/ZuluSCSI-firmware (same LUN limit, nothing to borrow), https://bluescsi.com/docs/ (ini options).

Thanks again, Andrew - the bus-time point and the WINDISK media-change model shaped this - and for your continued input.
