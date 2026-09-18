# 2026-09-18c — the card read back, and the bed brought in line with it

Two things this session: what the CF actually says after the owner's hardware
run, and the bed changes he asked for.

## 1. The hardware run, read off the card

The owner updated the driver from `C:\LS120MP` (the install source), Device
Manager picked up the new build, **the drive seeked immediately**, Explorer
then hung. He checked the node's resources before the hang: `0278-027F`. After
a forced power-down he changed the node to `03BC-03BF`: **no Explorer hang, the
drive view loads, still no drive.**

Read off `D:` afterwards rather than reasoned about:

| | |
|---|---|
| `IOSUBSYS\LS120MP.MPD`, `D:\LS120MP\LS120MP.MPD` | md5 `1c8cfb4f` — the ECP-reachable build, both paths |
| `BOOTLOG.TXT` (14:52) | `Initing ls120mp.mpd` → `Init Success`, **2 ticks** |
| `IOS.LOG` | absent |
| `SCSIPORT`, `DiskTSD` | `INITCOMPLETESUCCESS` |
| hive, our node | `AdapterSettings = PORT=0x378` present |

Two ticks is `LsFindAdapter` returning on one status read, which is what it is
written to do. It says nothing about bring-up (`HwInitialize` is non-vetoing).

### The node's I/O range is NOT the port the driver drives

`LsFindAdapter` takes the Settings string in preference to `AccessRanges`, and
the hive confirms the string is there. So `0278` vs `03BC` cannot change which
port we talk to, and **the Explorer-hang difference is unexplained.** Recorded
as unexplained rather than given a mechanism — see technique 124.

### But LPT1 owns 0x378, and we drive it anyway

```
ForcedConfig  378-37A   DeviceDesc  ECP Printer Port (LPT1)   HardwareID *PNP0401
ForcedConfig  3BC-3BF   DeviceDesc  LS-120 EPAT  f93851ec  1f79095  auto
```

Windows' own parallel-port driver holds `0378-037A`. Our miniport drives that
port without claiming it, so CONFIGMG sees no conflict and Device Manager
never flags one — the conflict exists only on the wire. The same boot log has
`LPTENUM` and `SPOOLER` initialising, and LPTENUM performs IEEE-1284
negotiation on LPT1 to read a device ID.

That matters here specifically: [09-18's finding](../drivers/imation_ls120/MEASURED_FACTS.md)
is that the peripheral **stays in ECP** and every nibble read returns `F5`
unless 1284 is properly terminated. A second 1284 initiator on the same wire
is a candidate for "the drive responds, nothing enumerates".

**Not proven. The test is two clicks:** Device Manager → Ports → *ECP Printer
Port (LPT1)* → disable in this hardware profile → reboot. Costs the printer for
that boot. If the LS-120 then enumerates, that is the answer.

## 2. The bed now models the drive's latency after reset

`86box_upstream` `d26bfc252`, `src/device/lpt_epat.c`. A new `reset_ms` config
option: on SRST release the bridge holds `BSY`, and a command issued inside the
window returns status `C1` / error `04` (ABRT) — which is what the bench
returned when a probe was bounded at ~0.29 s, against the same script reading
LBA 0 first time at ~2.3 s.

Default is `0`, the previous instant behaviour, so no other bed changes.
Verified firing, not assumed:

```
EPAT: W reg 16 = 04
EPAT: W reg 16 = 00
EPAT: SRST released, settling for 2500000 us
```

This closes the gap named in `next_session_2026_09_18b.md`: the bed could not
reproduce a bring-up timeout because the emulated drive was ready instantly.
`busy_ms` (post-command BSY) was already modelled; the reset settle was not.

## 3. The bed's SCSI chain now matches the real machine

Read out of the card's own `SYSTEM.DAT` - `SCSITargetID` + **`SCSILUN`** +
`CurrentDriveLetterAssignment` per node, walking forward from each node rather
than taking the nearest preceding string, which bleeds across records
(technique 65).

| id | lun | device | letter | modelled as |
|---|---|---|---|---|
| 0:00 | 0 | NECITSU M2512A, magneto-optical | `D` | `mo_01` |
| 0:02 | **0-4** | NAKAMICH MJ-5.16S, **5-disc changer** | `E F G H I` | `cdrom_01..05` |
| LPT | - | MATSHITA LS-120 COSM 04 | **`J`** | `rdisk_01` |
| 0:04 | 0 | HP C1537A tape | - | not modelled, no letter |
| 0:06 | 0 | UMAX Astra 610S scanner | - | not modelled, no letter |

Six removables before `J`, and they account for the live map exactly:
`C` XT-CF, `D` MO, `E-I` changer, `J` LS-120.

⚠ **The changer is one id with five LUNs.** 86Box addresses `bus:id` with no
LUN, so the five slots are stood up as five ids (`0:01`-`0:05`) - wrong
topology, right drive-letter count. The count is the point: it is what puts
the LS-120 at `J:` in the bed as it is on the bench.

⛔ **The YAMAHA CRW4416S and IOMEGA ZIP 100 are NOT modelled.** They have nodes
in the hive, but their remembered letters (`F` and `D`) **collide with live
devices**, which is what a node left behind by an earlier configuration looks
like. Modelling them would push the LS-120 two letters past `J`. An earlier
draft of this bed included the Zip on that evidence and it was wrong. **If the
owner confirms either drive is physically on the chain, the letter arithmetic
changes and so does this table** - and note there are then 8 devices for 7
usable SCSI ids, so one changer slot would have to go.

Bed config is `vm_ls120win/86box.cfg.master` (gitignored, which is why the
settings are written out here):

```
cdrom_01..05  scsi 0:01 0:02 0:03 0:04 0:05   (the changer's five slots)
mo_01         scsi 0:00                       (NECITSU M2512A)
rdisk_01      lpt                             (the LS-120 itself)
[Shuttle EPAT parallel-port ATAPI bridge]  busy_ms = 750   reset_ms = 2500
```

All keys confirmed read - they survive 86Box's rewrite of `86box.cfg`
(technique 69), verified by counting them back out of the rewritten file.

## Still not faithful, and deliberately left alone

`hdd_01_speed = ramdisk` in the bed. The XT-CF is nothing like a ramdisk and
this is the same class of gap as the two closed above, but fixing it slows
every boot. The owner's call.

## Next

1. **The LPT1 test.** Disable the ECP Printer Port node, reboot, see whether
   the drive enumerates. Two clicks, and it needs nothing from the bed.
2. **Boot the bed with `reset_ms = 2500` and the full chain.** For the first
   time it can reproduce a bring-up timeout and it puts the LS-120 at the same
   drive-letter position as the bench.
3. The 1284 state machine and the 3,584-byte ceiling, so bed ECP results start
   meaning something.
