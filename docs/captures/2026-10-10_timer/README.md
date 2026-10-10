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
