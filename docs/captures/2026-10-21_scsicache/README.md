# SCSI caching page per device, real 5160 (#31 part 2)

`tools/scsicache/scsicach.c` (Win32, WNASPI32, read only: MODE SENSE(6) page 8, retried on unit
attention and without DBD). Run through the V9x agent, whole chain online with media in.

| Target | Device | Page 8 |
|---|---|---|
| 2:1 | Yamaha CRW4416S | write cache on, read cache on |
| 2:5 | Iomega Jaz 2 GB | write cache on, read cache on |
| 1:0 | LS-120 (EPP) | write cache off, read cache on |
| 2:0, 2:2, 2:3, 2:4 | Fujitsu MO, Nakamichi changer, Zip 100, HP DAT | no caching page (5/24 invalid field, with and without DBD): not host-settable |
| 2:6 | UMAX scanner | not a disk (5/20) |

The two drives that write in volume already cache writes; the rest expose no setting.
Nothing left to switch on for #31.
