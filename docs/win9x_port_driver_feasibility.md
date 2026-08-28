# Can we write a 32-bit Windows 9x driver for 8-bit XT-IDE?

**Short answer: yes, it is possible, and it stopped being speculative on 2026-08-28.** It is still
a large job, and the honest risk is concentrated in one place — the boot disk. There is a staged
route that avoids that risk entirely for the first attempt.

This would help well beyond this project. Every XT-class machine running Windows 9x on an 8-bit
IDE/CF adapter is stuck in MS-DOS compatibility mode for the same reason.

## Why the previous answer was "no"

Recorded earlier as *"No Win9x 32-bit XT-IDE driver exists and never will — do not search, build."*
The first half is still true: none exists, and searching for one is wasted time. The second half —
that building one was impractical — is what has changed.

## What changed

[**zikolas/cfu1-win9x**](https://github.com/zikolas/cfu1-win9x), by **Nick (@zikolas)** — a
Windows 95/98 driver for the RATOC REX-CFU1 USB host CF card, **MIT licensed**. RATOC shipped
Windows CE drivers only; he wrote a PC driver from scratch. It is a working, current, readable example of exactly the
thing we would need to do.

`win/vxd/CFU1.ASM` is ~177 KB of VxD assembly that, among much else:

```
cfu_drp  DRP  <EyeCatcher, DRP_MISC_PD, OFFSET32 CFU1_IosAer, DRP_FC_DYNALOAD, 0>
cfu_ilb  ILB  <>
         VxDCall IOS_Register
```

That is the whole shape of a storage port driver: a **Driver Registration Packet** declaring a
port driver, an **IOS Linkage Block** filled in by `IOS_Register`, and an **AEP handler** servicing
`AEP_INITIALIZE` and `AEP_BOOT_COMPLETE`. A `.PDR` *is* a VxD that does this. He also walks DCBs
via `ISP_GET_FIRST_NEXT_DCB`, broadcasts `AEP_CONFIG_DCB`, and — because his device is
dynamically registered — writes his own TSD to parse the MBR and `ISP_ASSOCIATE_DCB` a volume.

**The toolchain is modern and free.** `win/build.sh` builds the VxD with **JWasm + Open Watcom v2**,
on macOS ARM64, against patched Win98 DDK includes:

```sh
jwasm -q -coff -D BLD_COFF -D IS_32 -D MASM6 -D DEBLEVEL=0 -Iddkinc -Fo=vxd/CFU1.obj vxd/CFU1.ASM
wlink format windows vxd dynamic option quiet name vxd/CFU1.VXD file vxd/CFU1.obj export CFU1_DDB.1
```

No period-correct MASM 6.11 environment is required. This project already builds VxDs from 1995 DDK
source — `VKD.VXD` was rebuilt that way — so the capability is proven here, not merely available.

## Why XT-IDE is *easier* than his case

His hardest problems come from **hot-plug and dynamic registration**, and they are documented in
his own comments: `DISKTSD` never engages a dynamically-registered driver, the DCB keeps
`DCB_DEV_MUST_CONFIGURE` set, the TSD layers never see the DCB, so he had to become his own TSD.

An XT-IDE controller is a fixed, non-removable device present at boot. Registered at boot time in
the normal way, `DISKTSD` and `VOLTRACK` engage on their own. **The entire class of problem that
cost him the most does not arise.**

The device side is also far simpler: no USB protocol stack, no hub enumeration, no async transfer
engine. It is PIO reads and writes to ATA task-file registers, with the 16-bit data register split
across two 8-bit accesses through the card's high-byte latch. **No DMA at all**, which on this
machine is a positive — the 20-bit DMA reach that broke the sound and floppy drivers cannot apply.

## Where the real risk is

**The boot disk.** A driver for the drive Windows booted from has to take over from the real-mode
INT 13h path mid-boot. That handoff — `RMM.PDR` relinquishing the unit — is the delicate part, and
getting it wrong means an unbootable machine rather than a failed test.

Note also his own warning about the storage path in general: after substantial work, bulk copies
through his IOS storage code still blue-screened with file corruption. Storage is the unforgiving
end of the IOS stack. Treat any machine running an experimental port driver as a lab machine.

## The staged route that removes the boot risk

There is a spare XT-IDE card. Use it.

1. **Install the spare as a second, non-boot controller** with its own CF card. The boot disk stays
   on real-mode INT 13h, exactly as now.
2. **Write the port driver for the secondary controller only.** If it never loads, or faults, the
   machine still boots — nothing is staked on it.
3. **Prove the stack**: does the unit appear in Device Manager, does the Performance tab stop
   naming it as real-mode, does read/write verify byte-exact against the same CF read under DOS?
4. **Only then** consider the boot disk, with a known-good driver and a tested revert path.

Step 3 is the milestone worth having. It converts "no 32-bit driver is possible on this hardware"
into a measured result either way, at no risk to a working install.

## Before starting

Do items 1 and 2 of [`next_actions_2026_08_28.md`](next_actions_2026_08_28.md) first. `T130.MPD` is
an existing, vendor-written, PIO-only miniport that either loads on this machine or does not. That
single test tells us whether this machine's IOS stack will accept *any* 32-bit storage driver —
which is the assumption everything above rests on, and it costs one install rather than a project.

## Credit

If anything here is built on `CFU1.ASM`, **Nick (@zikolas)** is credited by name and by link in the
driver source, the repository README and any write-up — not only in the MIT notice the licence
requires. His [GitHub profile](https://github.com/zikolas) carries a body of clean-room DOS and
Win9x work for hardware nobody else has driven. The project owner knows him personally and will
tell him directly if we take this route.

Intent if it works: publish the driver as its own release here, and write it up publicly rather
than leaving it buried in this repository — the problem it solves is not specific to this machine.

## Sources

- [zikolas/cfu1-win9x](https://github.com/zikolas/cfu1-win9x) — **Nick (@zikolas)**, MIT. `win/vxd/CFU1.ASM` (the IOS
  port driver and TSD), `win/build.sh` (JWasm + Open Watcom v2 recipe), `win/get-ddk.sh`,
  `win/ASYNC-ENGINE.md`, `PROBE-NOTES.md`, and `docs/why-not-the-windows-usb-stack.pdf`. Retrieved
  2026-08-28. The MIT notice must travel with anything adapted from it.
- Also by the same author and relevant to this machine's constraints:
  [vsbpcmcia](https://github.com/zikolas/vsbpcmcia) — Sound Blaster emulation for **DMA-less**
  machines via PIO passthrough.
- [XTIDE Universal BIOS](https://www.xtideuniversalbios.org/) — the real-mode side, and the source
  of the 386 build discussed in item 3 of the next-actions file.
