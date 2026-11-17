# #50: 512-word rep insw with the BL3 L1 on and off, real 5160, 2026-11-18

`tools/flushprobe/XTINSWA.BAT` at a DOS prompt after `CPUSET.BAT`: XTINSW with the cache on (`1000h:0=92`),
off (`00`), on again. Four passes of four bursts; PIT counts (0.838 us). Owner ran it; restore confirmed
on screen (`ctchip_restored.jpg`: 92, CE and ASNP enabled).

| per 1 KB (512 words) | on | off |
|---|---|---|
| A rep insw alone | 3.93 ms (3.84 us/B) | 3.93 ms |
| B rep insw, then rep lodsd over it | 4.05 ms | 4.06 ms |
| C rep lodsd alone | 65 us | 138 us |

The transfer is bus-bound; the L1 does not change it (A within 0.05 %). The touch after a read is cold
either way (I/O already invalidates once per burst), so disabling the L1 saves nothing and slows
everything else. Andrew's L1-off idea (#50, 10-23) does not pay on this machine. ON and ON2 agree.
