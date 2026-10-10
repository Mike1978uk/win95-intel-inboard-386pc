# Who holds the Windows timer tick on the 5160 (#45), 2026-10-10

Static scan of the pre-monolith Win95 image (`new_golden_premonolith.img`) with `tools/vtd_callers.py`:
VxDs that call VTD_Begin_Min_Int_Period are SCSIPORT.PDR (5 ms, `mov eax, 5` at 0x1192; released at
0xD5A when its timer flag at [1311h] clears), NWSERVER.VXD (caller's interval / 4), QIC117.VXD
(caller's value) and VTDAPI.VXD (applications' timeBeginPeriod).

`BOOTLOG_5160.TXT` (the card, 2026-10-10): NWSERVER and QIC117 are not loaded. SCSIPORT serves
xtidemp.mpd (`Polling=1`, the XT-CF has no IRQ) and t130.mpd (IRQ 3); sd120ppd.mpd failed to init.

#45 measured Sleep(1) = 11.0 ms at idle - consistent with a 5 ms tick, not the native 55 ms - and a 1 ms
tick costing 15% of a CPU loop, about 170 us per interrupt. Inference, not yet measured: SCSIPORT's 5 ms
tick for the polled XT-CF costs ~3.4% of the CPU all the time.

## Measured in the bed, 2026-10-10 (supersedes the inference above)

`vm_xtide_mpd` (XT-CF stride 2, XTIDEMP.MPD polled, T130B), build `86box_xtcf` with a PIT channel 0 reload
log (`PIT_CH0_LOG=1`, `pitlog_polling1.log`). Windows reached the desktop and the image's startup batch ran
WRTEST (disk writes through the XT-CF) in a DOS box. Channel 0 reloads, in order: BIOS 65536 (54.9 ms), then
Windows **16384 (13.73 ms, 72.8 Hz)** and no other value. **SCSIPORT never set 5 ms**; the inference was wrong.

VTD (`VTD.VXD` file offset 0x9F9-0xA57) takes the smallest requested period in ms: 54 ms gives 65536, anything
else is rounded DOWN to a power of two of ms x 1193. 16384 therefore means a request of 14-27 ms (20 ms most
likely). Candidates on this machine: VTDAPI for an application's timeBeginPeriod, or a DOS box programming the
timer. At ~170 us per interrupt (#45) the 72.8 Hz tick costs ~1.2% of the CPU; back at 18.2 Hz it would be ~0.3%.
