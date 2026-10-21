# SCSICACH.EXE - read each SCSI device's cache settings (Windows 95)

Lists every target Windows 95's ASPI layer (`WNASPI32.DLL`) can see, with its caching page
(MODE SENSE page 8): write cache (WCE) and read cache (RCD). Read only - it never changes a
setting. Works with any adapter Windows 95 drives, miniports included.

Run `SCSICACH.EXE`; it writes `C:\SCSICACH.TXT`, one line per device. A device with no caching
page reports `no page 8` with its sense key and ASC.

Source: [`tools/scsicache/scsicach.c`](../../tools/scsicache/scsicach.c), Open Watcom
(`wcl386 -bt=nt -l=nt`). Example output from the 5160: `docs/captures/2026-10-21_scsicache/`.
