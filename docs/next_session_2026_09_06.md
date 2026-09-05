# Next session — after 2026-09-06

**Issue #21 is closed.** The XT-CF SCSI miniport works on the real 5160. This file supersedes the
plan that was here; the driver's own page is `drivers/xtide_mpd/README.md`.

## What passed, and what the evidence is

`XTIDEMP.MPD` md5 `561fb45b598ef5985e5a803016321f76` — published byte-identical as
`dist/xtide_mpd/XTIDEMP.MPD`, so the artefact that ran is the artefact anyone can download.

Read off the CF afterwards, not reported from the screen:

```
[001612B7] Initing xtidemp.mpd
[001612CE] Init Success xtidemp.mpd
[0016136F] INITCOMPLETESUCCESS = DiskTSD
[00161372] INITCOMPLETESUCCESS = SCSIPORT
           rmm.pdr  Dynamic load success ... never reaches INITCOMPLETE
shutdown   7 stages started, 7 closed, none unpaired
WINDOWS\IOS.LOG   absent
```

`RMM.PDR` standing down is the boot-disk takeover. `C:` is served guest → IFSMGR → VFAT →
DiskTSD → SCSIPORT → `XTIDEMP.MPD` → XT-CF. Desktop reached, no errors in Device Manager, clean
shutdown to the safe-to-turn-off screen. `BOOTLOG.TXT` is itself a write that reached the medium
through the driver.

Four sessions of the `.PDR` wedging Windows at shutdown, closed by replacing the layer rather
than debugging it (technique 94).

## The forced-node question: moot, not answered

The previous handoff asked for the auto-assigned-`0300` cell to be run. It never was, and it no
longer decides anything for this driver.

The installed node holds a **forced** `ForcedConfig 0300-031F` — the exact cell that hung 3/3 on
the bed — and the shutdown was clean. But this build takes its base from `AdapterSettings`
(`PORT=0x300`) and never reads the node's resources, so what CONFIGMG assigned stopped mattering.

**Do not record this as "forcing is safe".** It is "the driver no longer cares", which is a
different and better property.

Two facts worth keeping:

- **Nothing owns `0300` on the real 5160.** Checked in Device Manager's resource list by the
  owner, and corroborated in the hive: exactly one node claims `0300-031F` and it is ours. No
  stale `hdc` node survives; the two `PORT.PDR` strings left in `SYSTEM.DAT` are filename
  entries in Setup's inventory, not a device node.
- **The conflict reported at install was probably phantom.** Installing without rebooting after
  removing the old node had CONFIGMG skip the free `0300` and assign `0340`, then object when
  `0300` was set by hand. A claim not yet released by the just-removed node fits. Unproven —
  the hive is post-reboot state and cannot show what CONFIGMG believed at the time.

So the supported install order is: **remove the old node → reboot → Add New Hardware.** Stated as
a recommendation, not a mechanism.

## Next: T130B on the real hardware (#19)

Already staged on the CF at `C:\T130XT\` — `T130.MPD` (Adaptec's, unmodified, md5
`9cc532791b9e911bfba89afbc920c4c7`), `T130XT.INF` and install notes. All md5-verified at the
destination and CRLF-clean.

Prerequisites verified on the card on 2026-09-06, nothing to do:

- `inbrdpc.sys` is in `[SafeList]` in `WINDOWS\IOS.INI` (line 290)
- the real-mode SCSI/ASPI chain is REM'd out of `CONFIG.SYS` — `MA13B`, `TSLCDR`, `MODISK2`,
  `NASPIBUF`

**Image the CF first.** Then: Add New Hardware → decline autodetect → SCSI controllers → Have
Disk → `C:\T130XT` → reboot.

**Do the CD-ROM arm before attaching a disk.** It proves the driver loads and claims the bus, but
does not exercise DiskTSD/VFAT/IFSMGR — which is precisely where the port driver's shutdown bug
lived. A control that does not reach the failing layer is not a control (technique 94).

Afterwards, from DOS or with the card in a reader: `BOOTLOG.TXT` for `Init Success t130.mpd` and
paired `Terminate`/`EndTerminate`; `IOS.LOG` should not exist.

Watch for one collision: `XTIDEMP.INF` offers `340-35F` among its four ranges. Our node is forced
to `0300-031F` so it is fine today, but a from-scratch reinstall could take `0340` from the T130B.

## Then: the rest of the real-mode chain (#17)

The goal the owner named — retire every real-mode storage driver on the machine. The boot disk is
done. Remaining: the SCSI peripherals (CD, MO, Zip 100), which T130B addresses, and the floppy
(#18). #22's LS-120 comes back into scope after that, and its root cause is already known — the
miniport's chipset probe writes to the 8259 through the XT's I/O aliasing (technique 75).

## Still unmeasured on the XT-CF driver

- **Sustained write load.** The hardware run was a boot, a look at `C:`, and a shutdown.
- **Responsiveness under a heavy teardown flush.** `XtStartIo` still completes every transfer
  inline, so a large flush blocks the machine for its duration.
  `ScsiPortNotification(RequestTimerCall, …)` is the mechanism and it is half the reason a
  miniport was worth writing. Technique 98: a shutdown long enough that the owner reaches for the
  power switch is a failed shutdown, not a slow one.
- **Stride 1** — a stock XT-IDE card. Supported in the code, never executed by anyone, on any
  machine. This is the thing an outside contributor could settle that the project cannot.
- **Build provenance.** The ledger records this binary's tree as `DIRTY` at commit `6869455`, so
  it is not provably rebuildable (technique 89). Not re-derived, by the owner's call, because the
  artefact itself is tracked and published.
