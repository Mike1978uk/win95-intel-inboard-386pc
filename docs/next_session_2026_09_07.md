# Next session — from 2026-09-07

Written at the end of 2026-09-06. **Two issues closed in two days**: #21 (XT-CF miniport) on the
5th, #19 (T130B SCSI chain) on the 6th.

---

## State of the machine

The real 5160 now runs **the boot disk and the entire SCSI chain in 32-bit protected mode**.

```
[000C2616] Initing xtidemp.mpd    Init Success        <- the XT-CF, C:
[000C262E] Initing t130.mpd       Init Success        <- the T130B, the SCSI chain
[000C260D] Initing hsflop.pdr     Init Success        <- the floppy, patched build
rmm.pdr   Dynamic load success ... never reaches INITCOMPLETE
INITCOMPLETESUCCESS = IOS / SCSIPORT / DiskTSD / CDTSD / VFAT / CDFS / IFSMGR
shutdown  7 stages started, 7 closed, none unpaired
IOS.LOG   does not exist
```

Boot log kept at `docs/bootlogs/BOOTLOG_2026-09-06_t130b_hardware_pass.TXT`.

**The CF was imaged on 2026-09-06** to `image_backups/` before any further installs. This is the
furthest this project has ever got; do not install anything without a current image.

### What the SCSI chain enumerates

| node | class | device |
|---|---|---|
| `SCSI\XT-IDE__TRANSCEND` | GenDisk | the CF, via `XTIDEMP.MPD` |
| `SCSI\NECITSU_M2512A` | GenDisk | Fujitsu M2512A MO, ID 0 |
| `SCSI\NAKAMICHMJ-5.16S` | GenCD | Nakamichi changer, ID 2 |
| `SCSI\IOMEGA__ZIP_100` | GenDisk | Iomega Zip 100, ID 3 |
| `SCSI\YAMAHA__CRW4416S` | GenCD | Yamaha CRW4416S CD-RW |
| **`SCSI\HP______C1537A`** | **Unknown** | **HP DAT tape, ID 4** |
| **`SCSI\UMAX____ASTRA_610S`** | **Unknown** | **UMAX Astra 610S scanner, ID 6** |

The two `Unknown` nodes are the **"new hardware found, no driver" prompt** at startup, and they are
the **correct end state** — Windows 95 ships no class driver for a SCSI tape or scanner. Both are
reached over ASPI (`APIX.VXD` + `WNASPI32.DLL`/`WINASPI.DLL`, present and loading) by their own
applications. Nothing to install at the kernel level.

---

## THE ORDER, as agreed with the owner

Split by **where the work can happen**, because two of these do not contend.

### 1. Floppy — in the VM. No machine time, no owner needed.

The only outstanding item that can be developed in emulation, and the reason is measured, not
assumed: **@andrew-hoffman reproduced it in 86Box.** He ran `HSFLOP_XTDMA.PDR` there, the floppy
worked and was faster than real mode, A: and B: left compatibility mode — and after changing disks
a few times he hit `Fatal Exception 0E` and a corrupted image. He attached the corrupt image; it
analysed as **a stale VCACHE page flushed to the wrong disk**, i.e. a media-change detection
failure, not the DMA-reach bug the patch fixes.

Three separate problems, do not conflate them:

1. **DMA reach (#18's original title).** SETTLED. The `maxPhys 0x1000 -> 0xFF` patch is right, and
   `HSFLOP_XTDMA.PDR` (md5 `8e695d00c20c6e43084d91d8ed111c52`) now genuinely **loads and inits on
   the real 5160** — for over a month it was deployed but never loaded, so nothing measured about
   it meant anything (technique 74). FIXES.md was still filing it as inert until today.
2. **Media-change / stale cache.** The live bug. Data loss. Keep floppies read-only meanwhile.
3. **Geometry and drive letters.** Two nodes, `ROOT&FDC&000000` and `ROOT&FDC&000010`, both generic
   `GENERIC NEC FLOPPY DISK`, neither matching the real TEAC FD-505 (1.44MB 3.5" as A:, 1.2MB
   5.25" as B:), neither holding a drive letter. Root cause is structural: **a 5160 has no CMOS**
   for Windows to read drive types from. Probably wants its own issue rather than living in #18.

**Asset from the XT-IDE work that applies directly here** (the owner asked about this and it is
worth not re-deriving): during the miniport investigation we mined Microsoft's own port drivers and
found **`HSFLOP.PDR` is the best-behaved reference for a driver that must survive a device that may
not answer.** It blocks *legally* — `Set_Global_Time_Out` at `2D95h`, `Wait_Semaphore` 43 bytes
later at `2DBEh`, one `Signal_Semaphore_No_Switch` elsewhere. A reference copy is kept at
`roms/xtcf_card/HSFLOP_reference.PDR`, and the tooling to read it exists: `tools/vxd_disasm.py`
(handles the `CD 20` + inline 4-byte service id that desyncs naive disassemblers),
`tools/vxd_aep_audit.py`, `tools/vxd_isp_audit.py`, `tools/pdr_vxd_services.py`.

So the media-change path can be **read rather than guessed at**. `VOLTRACK.VXD` and `DISKVSD.VXD`
are also in `IOSUBSYS` and are the obvious next things to point that toolchain at.

⚠ **Fidelity caveat, named up front:** the VM's floppy is 86Box's own controller, not the Sergey
ISA card (whose ROM at `0xC8000` is deliberately not loaded — it hangs POST). For a bug living in
Windows' cache/media-change layer that should be irrelevant. If the VM stops matching the bench,
suspect this first (technique 90).

### 2. Scanner and tape — on the real machine. Owner's hands, cheap.

Neither needs a kernel driver. Both are ASPI applications on top of `T130.MPD`:

- **Scanner** — install UMAX's Win95 package **stock**. See the correction below; do NOT carry the
  Win3.11 workaround across.
- **Tape** — `NBACK_SE` (NovaBack) is already on the card at `D:\NBACK_SE`. It drove this exact
  drive under 3.11 over ASPI, and ASPI is now the supported path rather than a workaround.

**One install, one boot, every time** (technique 76). The August keyboard loss was unattributable
precisely because DirectX, WinZip, InfoPro, SIV and the LS-120 driver all went on between good
boots.

⚠ **Correction the owner made on 2026-09-06, and it matters:** the note in
`memory/real_hardware_peripherals.md` saying the Astra 610S "does not use `WINASPI.DLL` — renaming
it away is what made the scanner work" is **scoped to Windows 3.11**, where the whole chain ran
through the DOS real-mode ASPI driver (`MA13B.SYS`) and `WINASPI.DLL` was a 16-bit shim in front of
a path the scanner software was not using. Under Win95 + `T130.MPD`, `WINASPI.DLL` is the
**expected** interface. Install stock and re-measure. The memory file now carries the general rule:
**a workaround is scoped to the stack it was found in** — and every 3.11-era note in that inventory
(GEAR, the MO, the CD-RW) is in the same position.

### 3. LS-120 (#22) — last, hardware-only, and costed before it starts.

Reframed. The old approach is dead: six binary patches, six nulls, and the decisive bisect proved
it is Imation's code executing. What changed is that **we can now write miniports** — `XTIDEMP.MPD`
is shipped and hardware-confirmed. So "build a driver based on the DOS switches" becomes: write our
own `.MPD` speaking the Shuttle EPAT parallel protocol, taking its port from `AdapterSettings`, and
never executing a chipset probe at all — structurally immune to the IRQ-1 kill rather than patched
around it (technique 75: drivers using ScsiPort helpers against a configured base are safe by
construction).

**It cannot be developed in the VM.** 86Box does not model the `0x20-0x3F` PIC aliasing that causes
the bug (`pic.c` maps two ports, no alias), and parallel-port LS-120 emulation does not exist in the
codebase at all (`RDISK_BUS_LPT` is a dead enum). Hardware-only. The DOS driver already works.
**Name the switch out loud before starting** — probe or fix (technique 77).

Cheap cleanups to fold in whenever convenient:
- The stale `SCSIAdapter\0000` node (the August Imation adapter, driver renamed to `SD120PPD.MP_`,
  does not load) still holds a live LS-120 disk node at `L:`. **This is why the LS-120 appears in
  the registry — it is NOT something `T130.MPD` found.** The owner asked; it is parented on
  `ROOT&SCSIADAPTER&000000` while T130 is `\0001`.
- A stale `ESDI\TRANSCEND` node from before the miniport.
- `AUTOEXEC.BAT` has a dead `LH C:\TSCSI\MSCDEX /d:TSLCD /m:10` whose `TSLCD.SYS` is REM'd out of
  `CONFIG.SYS`.

---

## Open thread: submitting the driver to XTIDE Universal BIOS

The owner raised this and wants **credit and a link back to the repo**. Groundwork done today; the
approach itself is **not drafted and nothing has been sent.**

- ✅ **MIT licence added** (`LICENSE`, commit `d836005`), Copyright (c) 2026 Mike Lycett, with a
  full third-party scope note. This was the blocker: with no licence the driver was
  all-rights-reserved by default, so nobody could legally redistribute it and the XUB team would
  have had no terms to point at. MIT's notice requirement **is** the credit mechanism.
- ⚠ **`xtideuniversalbios.org` "Reports" is the wrong door.** The site is a **Trac** instance;
  *View Tickets → Reports* are saved ticket queries against their **BIOS bug tracker**. Posting a
  Windows 95 driver there is a category error.
- **Right venues, in order:** (1) the **Vintage Computer Forums** XTIDE thread, named on their own
  front page as the primary discussion venue — post it yourself so attribution is inherent;
  (2) direct email to the maintainers, `aitotat@gmail.com` (Tomi Tilli) and
  `krille_n_@hotmail.com` (Krille), whose front page invites contact, asking for a wiki link;
  (3) a wiki page only if offered.
- **The pitch that makes it a collaboration, not an advert:** the driver has only ever run on a
  Lo-tech XT-CF rev 3 at **stride 2**, 8-bit PIO. **Stride 1 — a stock XT-IDE card — is supported
  in the code and has never been executed by anyone.** That community has those cards. Ask for it.
- Note XUB is GPL-2.0; our driver is independent of their code (hand-written MASM, no XUB source),
  so MIT is not a conflict — but say so plainly in the post rather than leaving it implicit.

**Next action:** draft the forum post and the maintainer email for the owner to review and send.
Nothing goes out without him.

---

## Corrections and facts established 2026-09-06

- **The tape drive is an HP C1537A**, not the C1555D recorded in
  `memory/real_hardware_peripherals.md`. The device's own SCSI inquiry string wins.
- **`RMM.PDR` loading but never reaching `INITCOMPLETE`** is the storage-takeover discriminator, and
  it now reads in the good direction for both the boot disk and the SCSI chain.
- **FIXES.md's "⛔ Deployed but not loaded" section is retired.** It was true when written
  (2026-08-25) and had been false since the IOS punt cleared. A document that was true when written
  goes stale silently — this is the second time the tidy pass has caught one this week.
- `T130.MPD` md5 `9cc532791b9e911bfba89afbc920c4c7`: installed copy, staged copy and
  `drivers/trantor_t130b/T130.MPD` all hash the same and match the archive.org provenance.

## Not tested, and worth not overclaiming

- Reads and writes to actual MO, Zip, CD or CD-RW **media** under Windows. The devices enumerate and
  bind; that is not the same as verified transfers.
- Floppy read/write correctness on the real 5160 since `HSFLOP` started loading.
- Stride 1 on any XT-IDE card, by anyone.
