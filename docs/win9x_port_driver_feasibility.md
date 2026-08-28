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

## Microsoft shipped a sample port driver, and we already have it

Found 2026-08-28 in the Windows 95 DDK already on this machine
(`OneDrive/Desktop/XT_project/Windows95_ddk`). This is the thing to start from — it is the same
layer as Nick's driver, but it is Microsoft's own reference, with the headers and libraries:

```
BLOCK/SAMPLES/PORT/SAMPLE/
    PORT.ASM        6,519   VxD init, DRP declaration, IOS registration
    PORTAER.ASM    17,314   the AEP handler - the core of a port driver
    PORTISR.ASM     5,624   interrupt service routine
    PORTREQ.ASM     6,655   IOP request handling (the actual reads and writes)
    PORTDDB.INC / PORTINFO.INC / MAKEFILE
BLOCK/INC/          MINIPORT.INC, SCSIPORT.INC, MINIPORT.H, SCSIPORT.H
BLOCK/LIB/          SCSIPORT.LIB
INC32/              VMM.INC, IOS.INC, DRP.INC, BLOCKDEV.INC, ILB.INC
MASM611C/           MASM 6.11 - the period-correct assembler
```

`PORT.ASM` declares the same structure Nick's does, which is a good cross-check that we understand
it correctly:

```asm
Drv_Reg_Pkt DRP <EyeCatcher, DRP_MISC_PD, offset32 Port_Async_Request,                  offset32 port_ilb, PortName, PortRev, PortFeature, Port_IF>
```

**The AEP surface is small.** `Port_Async_Request` dispatches on exactly five function codes:

| AEP | what an XT-IDE driver would do |
|---|---|
| `AEP_INITIALIZE` | claim the I/O range, no IRQ |
| `AEP_DEVICE_INQUIRY` | ATA `IDENTIFY DEVICE` through the 8-bit path; return `AEP_NO_MORE_DEVICES` when the scan ends |
| `AEP_CONFIG_DCB` | fill in geometry and capacity |
| `AEP_IOP_TIMEOUT` | recovery |
| `AEP_BOOT_COMPLETE` | stay resident or unload |

`PORTREQ.ASM` is where read and write requests are serviced, and is the file that would carry the
8-bit data-path work. **`PORTISR.ASM` is probably not needed at all** — an XT-IDE card is normally
jumpered without an interrupt, so the driver polls, which removes a whole file and the VPICD
interaction with it.

That is a genuinely small surface for a first cut: keep the AEP structure verbatim, replace the
inquiry with an ATA identify, and replace the request path with 8-bit PIO.

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

## The XT-IDE register interface, and why development can happen in emulation

**86Box already emulates the XT-IDE card** — `src/disk/hdc_xtide.c`, device `xtide_device`
("PC/XT XTIDE"). So the driver can be written and debugged entirely in emulation, against a
machine that already boots Windows 95, with no risk to the real 5160 at any point.

Better: that device **logs every register access with the guest instruction pointer** —
`[CS:EIP] [R] reg = val`. For bring-up that is close to an ideal debugger; you can see exactly what
the driver issued and what came back, instruction by instruction.

Register map, read out of `xtide_read()` / `xtide_write()`. Default base `0x300`, a 16-byte I/O
range; option ROM defaults to `0xD0000`.

| offset | read | write |
|---|---|---|
| `+0x0` | data **low** byte — and the hardware latches the high byte into `+0x8` | data **low** byte — commits the 16-bit word using whatever is in `+0x8` |
| `+0x1`–`+0x7` | ATA task file, 8-bit direct, no latch | same |
| `+0x8` | the latched **high** byte | sets the high byte for the *next* `+0x0` write |
| `+0xE` | alternate status | device control |

**The access order differs between read and write, and getting it wrong would cost a day:**

```
read  a 16-bit word:  inb(base+0x0)   -> low   (this latches the high byte)
                      inb(base+0x8)   -> high
write a 16-bit word:  outb(base+0x8, high)    (latch first)
                      outb(base+0x0, low)     (this commits the word)
```

So reads are **low then high**; writes are **high then low**. Offsets `0x9`–`0xD` and `0xF` are
unused.

**Open question for the real card:** the above is 86Box's model. XT-IDE exists in more than one
hardware revision, and the address-line wiring is not identical across them. Confirm which revision
the actual card is before trusting this map on hardware — in emulation it is authoritative by
definition, which is another reason to start there.

**Also useful:** 86Box's XT-IDE offers a BIOS build selector — "Regular XT" and "XT+ (V20/V30/8018x)"
on the 8-bit card, "Regular AT" and "386" on `xtide_at_device`. Worth checking whether the 386 build
can be paired with the 8-bit card in emulation; if it can, item 3 of the next-actions file gets a
free dry run before anything is flashed.

## What the LS-120 failure taught us about writing this driver

Root-causing #22 produced design constraints for our own driver, not just a fix for someone else's.
Detail in [`xt_io_aliasing_gotcha.md`](xt_io_aliasing_gotcha.md).

**1. Never use immediate-port I/O to anything outside our own range.** That is precisely what killed
the keyboard: `out 22h,al` with a hardcoded system address. The structural fix is the one Trantor
used and Shuttle did not — do every access through the port base supplied by configuration, so the
driver *cannot* name a system port. Audited: `T130.MPD` does this and is clean; `SD120PPD.MPD` does
not and has 98 destructive writes.

**2. Poll. Never touch the 8259.** An XT-IDE card is normally jumpered without an interrupt, so the
driver has no reason to read or write the PIC at all. That removes `PORTISR.ASM` from the DDK
sample *and* removes any possibility of repeating this bug.

**3. Byte I/O only for the task file — and understand why.** The `66 e7 22` word writes in the
LS-120 driver split into two byte writes on an 8-bit bus, hitting two registers instead of one.
**That is the same bus behaviour that makes the XT-IDE high-byte latch necessary in the first
place.** A 16-bit `in`/`out` on the data register does not do what a 386 programmer expects here;
that is exactly why offset `+0x8` exists. Use byte accesses and drive the latch explicitly.

**4. Put I/O delays between back-to-back accesses.** Both drivers are full of the `eb 00` idiom
(`jmp $+2`) between consecutive port operations. On a 4.77 MHz 8-bit bus this is not superstition —
peripherals need recovery time. Our transfer loop will need the same.

**5. Audit our own binary before deploying it.** `xt_port_audit.py` should be run against our driver
as part of the build, not just against other people's. Dogfooding it is the cheapest possible check.

**6. Emulation success does not imply hardware success.** 86Box does not model the PIC aliasing, so
a driver that strays into an aliased port would pass in emulation and fail on the real machine.
Develop in 86Box for the fast iteration, but treat a real-hardware run as the only proof.

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

   The spare is a shelf card, **not currently fitted**, with a rewritable ROM (owner, 2026-08-28).
   Its ROM build does not matter here — a protected-mode port driver talks to the card's I/O
   registers directly and never calls the option ROM. What *does* matter is that once it is fitted
   alongside the primary, **two XTIDE option ROMs would both hook `INT 13h`**. Decide that deliberately before fitting the card: either
   disable the second ROM and let the driver claim a controller the BIOS never enumerated, or
   configure one ROM to serve both. Two ROMs quietly fighting over `INT 13h` would produce
   confusing results that look like driver bugs.
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
