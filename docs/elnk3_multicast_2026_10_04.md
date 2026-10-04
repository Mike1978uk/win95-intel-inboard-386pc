# ELNK3.VXD: the 3C509B accepts every multicast frame - 2026-10-04 (#41)

The 3C509B's receive filter has four bits (individual, multicast, broadcast, promiscuous) and no
multicast hash: multicast is all or none. `ELNK3.VXD` maps Windows' NDIS packet filter onto it at
file `0x44a4`; a multicast-list request (`NDIS_PACKET_TYPE_MULTICAST`) sets the all-multicast bit.

Windows 95 TCP/IP asks for one, so the card runs with filter **7** (logged by a diagnostic line in the
86Box 3C509B model, bed `vm_3c509b_irq2`). Every mDNS, SSDP and IPv6 multicast frame on the LAN is then
read across the 8-bit bus (`rep insd`, ~3.2 us/byte) and dropped by the stack.

The rest of the driver is already lean for this bus: receive and transmit use `rep insd`/`rep outsd`
(the fastest width measured here), and its wait loops are short command-busy and EEPROM polls.

**Patch:** `drivers/3c509b/patch_elnk3_nomcast.py` turns `or eax, 2` at `0x44c7` into `or eax, 0`
(md5 `73f85aee`). An explicit all-multicast request still opens the card.

| bed boot | filter | frames received | NETPROBE pings |
|---|---|---|---|
| stock `ELNK3.VXD` | 7 | 6 (DHCP, ARP, 4 replies) | 4/4 |
| patched | **5** | 6 (same) | 4/4 |

The bed's slirp network carries no LAN multicast, so it shows the patch is safe, not what it saves.
On the 5160 idle CPU is already ~1.4% (30 s PERFLOG runs), so the gain is bus time and at most a
percent or two of CPU, depending on the LAN. Not yet run on the 5160.
Raw: `docs/captures/2026-10-04_elnk3_mcast/`.

## On the 5160, 2026-10-04 23:41 (one boot)

Patched driver loaded (md5 `73f85aee`, `ELNK3` init success). Idle `KERNEL\CPUUsage`, 30 s PERFLOG runs
(first sample dropped): stock 1.27% and 1.40%, patched **1.07%**. Within the noise of a 30 s run: no
worse, possibly slightly better; the owner then browsed to three web sites (DNS, TCP, HTTP all
working), so the patched driver stays; the saving is bus time when the LAN carries multicast. Stock kept as
`C:\WINDOWS\SYSTEM\ELNK3_orig.VXD`. Raw: `docs/captures/2026-10-04_elnk3_mcast_card/`.
