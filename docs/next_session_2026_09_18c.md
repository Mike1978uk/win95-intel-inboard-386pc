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

### LPT1 owns 0x378 and we drive it unclaimed - and that is NOT the bug

```
ForcedConfig  378-37A   DeviceDesc  ECP Printer Port (LPT1)   HardwareID *PNP0401
```

Windows' own parallel-port driver holds `0378-037A` while our miniport drives
that port from `AdapterSettings`, claiming nothing. CONFIGMG has nothing to
arbitrate, so Device Manager cannot show it. That much is true and is worth
knowing (technique 125).

**It was then offered as the likely cause, and that was wrong.** The bed's own
hive carries the identical node:

| | bed `ls120win_clean.img` | real card |
|---|---|---|
| `ECP Printer Port (LPT1)` `*PNP0401` | present, `ForcedConfig 378-37A` | present, `ForcedConfig 378-37A` |
| our LS-120 node's range | **`278-27F`** | `3BC-3BF` (was `278-27F`) |

**The bed enumerates the drive with that node present and with our node on
`278-27F`.** So LPT1 ownership is exonerated, and so is the node's I/O range -
independently of the source argument above. One free read of the bed image
killed the theory before anyone touched the 5160.

The owner also said, correctly, that he set the ECP port up before the Friday
on which the drive enumerated. Both lines of evidence agree. ~~Do not re-raise this.~~ **⚠ THAT WAS WRONG - see section 6.** The bed
cannot exonerate LPT1: in the bed `lpt1_device = lpt_epat`, so LPT1 *is* the
bridge and no printer-port driver exists to compete with us.

The residual, and it is small: a driver that drives a port it has not claimed
is still a latent hazard on any machine where LPTENUM or the spooler is doing
1284 work at the wrong moment. Worth a line in the INF, not an investigation.

### The node's I/O range is not the port the driver drives

`LsFindAdapter` takes `AdapterSettings` (`PORT=0x378`) in preference to
`AccessRanges`. The bed proves it end to end: node at `278-27F`, driver talks
`0x378`, drive enumerates. So the owner's `0278` -> `03BC` change altered
nothing about the transport, and **the Explorer-hang difference it produced
remains unexplained.** Recorded as unexplained (technique 124).

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

Topology stated by the owner on 2026-09-18; ids, LUNs and letters read out of
the card's `SYSTEM.DAT` (`SCSITargetID` + **`SCSILUN`** +
`CurrentDriveLetterAssignment`, walking forward from each node rather than
taking the nearest preceding string, which bleeds across records).

**Internal, always powered - this is the baseline, and it is what the bed now
models:**

| id | lun | device | letter | modelled as |
|---|---|---|---|---|
| 0:02 | **0-4** | NAKAMICH MJ-5.16S, 5-disc changer | `E F G H I` | `cdrom_01..05` |
| 0:03 | 0 | IOMEGA ZIP 100 | `D` | `rdisk_02` |
| LPT | - | MATSHITA LS-120 COSM 04 | **`J`** | `rdisk_01` |

**External, and the owner turns these off at times, so they are NOT modelled:**
NECITSU M2512A (MO, 0:00), YAMAHA CRW4416S (0:01), UMAX Astra 610S scanner
(0:06). HP C1537A tape (0:04) takes no drive letter either way.

That resolves the duplicate letters in the hive: the MO and the Zip both
remember `D`, and the Yamaha and a changer slot both remember `F`, because the
external set comes and goes. An earlier draft of this document read the Zip as
a stale node and dropped it. **It is the internal drive; the MO is the one
that is sometimes absent.**

⚠ **The changer is one id with five LUNs** and 86Box addresses `bus:id` with no
LUN, so the slots are five ids - wrong topology, right drive-letter count. The
count is the point: it is what puts the LS-120 at `J:` as on the bench.

⛔ **The externals cannot be added on top.** Five changer slots plus the Zip
already uses six of the seven usable ids (7 is the initiator). Modelling the
externals-on state means giving up changer slots, i.e. giving up the letters
that put the LS-120 at `J`. It is one or the other.

Bed config is `vm_ls120win/86box.cfg.master` (gitignored, which is why the
settings are written out here):

```
cdrom_01..05  scsi 0:00 0:01 0:02 0:04 0:05   the changer's five slots
rdisk_02      scsi 0:03                       IOMEGA ZIP 100
rdisk_01      lpt                             the LS-120 itself
[Shuttle EPAT parallel-port ATAPI bridge]  busy_ms = 750   reset_ms = 2500
```

All keys confirmed read - they survive 86Box's rewrite of `86box.cfg`
(technique 69), counted back out of the rewritten file.

## Still not faithful, and deliberately left alone

`hdd_01_speed = ramdisk` in the bed. The XT-CF is nothing like a ramdisk and
this is the same class of gap as the two closed above, but fixing it slows
every boot. The owner's call.

## 4. BUILT: the non-blocking init, from the two working references

`6db3616`, code **`c376510b`**. Staged on the card in both paths and never yet
booted on hardware.

What changed, and why each line is there —
`docs/ls120_enumeration_from_working_drivers.md` has the evidence:

| before | now | reference |
|---|---|---|
| `LsInitialize` calls `LS_BringUp`, which spins `LS_SPIN_RESET` = `0FFFFh` ~3.0 s | `LS_OpenReset` pulses SRST and returns at once | `PC2xInitialize`; vendor `HwInitialize` does no port I/O at all |
| nothing announced | `ScsiPortNotification(ResetDetected, devext)` | both references, and it is what makes SCSIPORT rescan |
| no timer anywhere | `ScsiPortNotification(RequestTimerCall, devext, LsTimer, 1000)` | vendor rva `48bbh` / `PC2X.C:932` |
| every wait inline | `LsTimer` polls the settle one nibble read (~30 us) per tick, 6000 ticks | technique 122: the settle and a status poll get separate budgets |
| an SRB during bring-up hit a dead bus | held in `LsPendSrb`, returned **without completing**, run when ready | this is what stops SCSIPORT concluding there is nothing there |

Emitted code verified against the binary, not the source (technique 123):

```
0x00115b  call 0x48d           ; LS_OpenReset - pulse, no wait
0x001160  mov  [state], 0      ; SETTLING
0x001167  mov  [left], 0x1770  ; 6000 ticks
0x00117e  push 3               ; ResetDetected
0x001188  push 0x3e8 / push 0x111b4 / push 6   ; RequestTimerCall, LsTimer, 1 ms
0x00119f  mov  eax, 1 / ret 4
```

`LsTimer` re-arms at the top before doing any work, as both references do.

### Staged on the card

| | |
|---|---|
| `D:\WINDOWS\SYSTEM\IOSUBSYS\LS120MP.MPD` | md5 `508ab8d3`, code `c376510b` |
| `D:\LS120MP\LS120MP.MPD` | same — install source AND `IOSUBSYS`, per the 09-14 lesson |
| previous build | kept as `LS120MP.B28` in both paths |
| `D:\LS120MP\LS120MP.SPP` | **fallback**: identical but `-Mode spp`, code `5084a71c` |
| INF model name | `LS-120 EPAT  c376510b  6db3616  auto` — Device Manager shows which build is installed |

The rebuilt tree copy has a different md5 (`09e53865`) and the **same code
hash**. Only the PE timestamp differs; the code is identical.

### What this build does NOT fix, stated plainly

- **Commands still run inline once the drive is ready.** Only the *settle* moved
  to the timer. A 3584-byte read is ~140 ms of held machine at 39 us/byte, so
  Explorer may still be unhappy during bulk transfer. Step 4 of the spec — turn
  each `LS_*Wait` into a polled state — is not done. Mounting first.
- **The ECP negotiate at ready is carried over unchanged** from the previous
  build. It has never once succeeded on hardware, which is why `LS120MP.SPP`
  exists. Boot the `auto` build first; if the drive still does not appear, swap
  in the `.SPP` copy and boot again. One variable.
- Steps 5 of the spec (`SpecificLuExtensionSize`, `MapBuffers`, the three extra
  SRB functions) are untouched.

## 5. RESULT: the drive mounts in the bed, and ECP is the blocker

Two bed runs, identical in every respect except the transport pin, on the
non-blocking init build. Driver's own trace ring, read out of physical
0B9000h by the emulator's dumper.

### The bring-up fix works

Both runs trace `ENTR FADP FOUN PULS ... HOLD REDY RSUM` - the vendor's shape
end to end: pulse and return, hold the SRB that arrives during the settle,
resume it when the drive answers. `Init Success` fell from 7 ticks to 3.

### The transport is the remaining fault

| | ECP (auto, code `c376510b`) | SPP (code `0d98c894`) |
|---|---|---|
| INQUIRY | `DONE=04` SRB_STATUS_ERROR | **`DONE=01` SUCCESS** |
| TEST UNIT READY | never reached | `04`, then `01` |
| PREVENT/ALLOW MEDIUM REMOVAL | - | `01`, `01` |
| READ CAPACITY | - | `04`, `04`, then `01` |
| READ(10) | - | **`01`, `01`** |
| drive letter | none | **`J:` mounted** |

The ECP failure is NOT a transfer failure. `86box.log` for that run shows
`data phase in, 36 bytes, request length 36` - the whole INQUIRY reply crossed
the wire - and the command was then reported as an error. So the fault is in
the POST-transfer status read, which is exactly the 2026-09-18 hardware
measurement: without a 1284 terminate the peripheral stays in ECP and every
nibble read returns `F5`.

⚠ **This is a bed result.** The bed's ECP is a model, not the Intek21 TK9901.
It is *consistent* with ECP never having worked on the bench, and that is all
it is. Do not write this up as an ECP finding about the hardware.

### Shipped: SPP is now the default on the card

| path | md5 | what |
|---|---|---|
| `D:\WINDOWS\SYSTEM\IOSUBSYS\LS120MP.MPD` | `d8154f1d` | **SPP, code `5084a71c`** |
| `D:\LS120MP\LS120MP.MPD` | `d8154f1d` | same - install source too |
| `D:\LS120MP\LS120MP.SPP` | `d8154f1d` | the same file, kept under its own name |
| `D:\LS120MP\LS120MP.AUT` | `508ab8d3` | the ECP/auto build |
| `*.B28` | | the 09-18 morning build |

`git diff 28e85f4..47b191b -- src` is empty, so the shipped SPP binary is the
same source as the traced build that mounted the drive; it differs only by
`-Trace`. **Never booted on hardware.**

### Instruments

- ⛔ **`-DbgPort` is a DEAD SWITCH.** `build.ps1:93` adds `-DLS_DBGPORT` and no
  source file has consumed it for days. Every build that claimed to use it
  emitted nothing. Delete it or reconnect it. Technique 123.
- `-Trace` (the 0B9000h ring) **works in the bed** and is what produced
  everything above, collected by `LS120_TRACE_DUMP=1` into `lstrace.bin`.
- `4a2ed1c` makes the three post-data-phase exits in `LS_PacketCommand` name
  themselves (`PFWT` / `PERR` / `PDRQ`) and print `LS_LastStatus`. They all
  shared one label, so a failure said only that it failed. Diagnostic build
  `aa0d89be` is parked at `%TEMP%\ls120_failtrace.mpd`; one bed run with it
  says which guard the ECP path trips and with what status byte.


## 6. HARDWARE, evening: the drive is fine and the fault is Windows-side

Over DOS COMrade, with `SD120PPD.SYS` REM'd out in `CONFIG.SYS` and the machine
cold:

- **`INQ9.SCR` re-run byte for byte** - the same script as the 2026-09-11
  known-good capture, pushed to the card and run. Output is **identical** apart
  from the DEBUG load segment:
  `MATSHITA LS-120 COSM   04 0270` at `0700`, phase bytes `80 00 00 08 08 02 24 00`.
- So the **drive, the bridge, the cable and the transport sequence are all
  good, right now.** That retires "maybe the drive was in the right state that
  one time" - it is in the right state today and Windows still cannot use it.
- The failing boot ran **`d8154f1d` (SPP, code `5084a71c`)** - hash read back off
  the card, not assumed - the same binary that mounts `J:` in the bed.
- `Initing ls120mp.mpd` -> `Init Success` in **2 ticks** on hardware, so the
  non-blocking init works on the bench too.

### The bed/bench difference that actually stands

`vm_ls120win/86box.cfg`: **`lpt1_device = lpt_epat`**. In the bed LPT1 *is* the
bridge. There is no emulated parallel port and no printer-port driver competing
for it. On the 5160, LPT1 is a real ECP port that Windows owns
(`ForcedConfig 378-37A`, `*PNP0401`), with `LPTENUM` performing 1284 device-ID
negotiation on it, and the bridge hanging off the same cable.

**So the earlier exoneration of LPT1 was invalid** - it compared against a bed
that cannot show the problem. Same shape as technique 90: "it does not reproduce
in the emulator" is a claim about the emulator.

**Next test, two clicks at the machine:** Device Manager -> Ports -> *ECP Printer
Port (LPT1)* -> disable in this hardware profile -> reboot. Costs the printer for
one boot.

## 7. STILL OPEN: two builds have mounted the drive, for different reasons

⛔ **Today was NOT the first time.** `a925038` - fully synchronous, no timer,
the shape this session replaced - was **photographed** on 2026-09-11 12:10
mapping the drive at `L:`, 119.0 Mb, Windows creating a file system.

So the record is:

| build | shape | LPT1 node | result on the 5160 |
|---|---|---|---|
| `a925038` / `ff80f056` | synchronous, inline waits | **unknown** | mounted at `L:` (photographed) |
| everything after it | synchronous, inline waits | enabled | nothing |
| `5084a71c` (today, SPP) | non-blocking init + timer | enabled | nothing |
| `5084a71c` (today, SPP) | same binary | **disabled** | **mounted at `J:`, reads** |

**The unanswered question is whether the LPT1 node existed on 2026-09-11.** The
owner thinks he set the ECP port up before that Friday but is not certain. It
decides which of two stories is true:

- if LPT1 was **enabled** on 09-11, then LPT1 contention is not sufficient to
  block the drive, something else changed between `a925038` and now, and
  disabling LPT1 today is a workaround for a different fault;
- if LPT1 was **added after** 09-11, the whole September regression has one
  cause and the bisect that has been hanging over this work is unnecessary.

Cheap ways to settle it, in order: the install date of the LPT1 node's INF, the
`SETUPLOG.TXT` / `DETLOG.TXT` timestamps on the card, or a `SYSTEM.DA0`
comparison against an image snapshot taken before 09-11.

## Next

1. **Bisect against the build that enumerated.** `762fe8ac` (code `7d389ebc`,
   commit `cb4b62a`, **clean tree**, flags `-DLS_PHASE2` only) is on the card
   as `D:\LS120MP\LS120MP_FridayEnumerating.MPD` and is rebuildable. There are
   **45 driver commits** between it and `1f79095`. Three earlier commits
   (`4c83515`, `c19efe4`, `a6c759c`) already tried pairing it with newer
   transports, so read those before repeating the attempt.
2. **Boot the bed with `reset_ms = 2500` and the baseline chain.** For the
   first time it can reproduce a bring-up timeout, and the LS-120 sits at the
   same drive-letter position as the bench.
3. The 1284 state machine and the 3,584-byte ceiling, so bed ECP results start
   meaning something.

⛔ **Not** the LPT1 node. Exonerated above, twice over.
