# LS120MP.MPD - parallel-port LS-120 miniport (issue #22)

First build with a working transport. **Never yet run under Windows** - treat the
first install as a test, not a deployment.

| File | Purpose |
|---|---|
| `LS120MP.MPD` | the miniport, 6,144 bytes |
| `LS120MP.INF` | installs it as a SCSI adapter, base `0x378` |

Code hash `37ba7d18` (the md5 moves with the PE timestamp; the code hash does not).
Built from `drivers/imation_ls120_mpd` with `build.ps1 -Phase 2`.

## What was fixed

The drive powers up **held in ATA reset**. Until SRST is pulsed on device control
every task-file register reads `00h`, which is indistinguishable from an absent
bridge - and is what every probe saw for two sessions. The reset is not in
`epat.c`; it lives one layer up, in Linux's `pcd.c`.

Proven cold on the real 5160 on 2026-09-10 with Imation's `SD120PPD.SYS` REM'd
out: the task file returns the ATAPI signature `14 EB`, error `01` (diagnostics
passed). Full account: `drivers/imation_ls120/TRANSPORT_SPEC.md` section 5.

## Install

1. Copy both files to the machine.
2. Device Manager -> Add New Hardware -> **do not** let it detect -> Have Disk ->
   point at `LS120MP.INF`.
3. Confirm the node's Settings tab shows `PORT=0x378`.
4. **Add the driver to `WINDOWS\IOS.INI` `[SafeList]`** - without it IOS declines
   every miniport on this machine. See `docs/ios_safelist_howto.md`.
5. Reboot.

REM out `SD120PPD.SYS` in `CONFIG.SYS` first, or the two will fight over the port.

## What is not proven

- Anything under Windows, including whether a polling miniport that owns the
  parallel port disturbs the keyboard here. That question is still open (#22).
- The block data path is a nibble byte-loop: correct, and slow. ECP is documented
  in the transport source but not wired.
- `0` destructive-write candidates (`xt_port_audit.py`), so the `0x22`/`0x23`
  8259-aliasing killer in Imation's own driver is absent by construction.
