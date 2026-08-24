---
name: win9x-dma-driver-audit
description: Find and fix Windows 9x drivers whose DMA buffer lands outside the machine's real DMA reach - the silent "device works but the data is wrong" class of bug on XT-class and other narrow-page-register hardware. Use when a driver produces corrupt data rather than failing outright, or to audit a whole Windows 9x install before trusting it.
---

# Auditing Windows 9x drivers for DMA buffers the hardware cannot reach

## The bug class, in one paragraph

An ISA DMA controller addresses memory as `page_register << 16 | offset16`. On a PC/XT the page
latch is **4 bits**, so DMA reach is **20-bit — 1 MB**. On an AT it is 8 bits: 24-bit, 16 MB.
Essentially every ISA-era Windows driver hardcodes the AT assumption. Put a DMA buffer above the
real reach and **nothing faults**: the page register silently drops the high bits and the controller
transfers against a completely different physical address. The device works, the transfer completes,
and the data is wrong.

That is the tell. **This bug looks like corruption, not failure.** Distorted audio, garbled tape or
scanner data, a parallel-port transfer that produces plausible-but-wrong bytes. It survives ordinary
testing, and it invites you to go looking in the wrong subsystem — a real case spent weeks suspecting
a video driver.

It applies to any machine whose DMA reach is narrower than its RAM: PC/XT-class boards, accelerator
cards (a 386 in an XT keeps the XT's latch), and anything where a memory manager virtualizes DMA.

## Symptoms worth reaching for this skill

- A device produces **wrong data rather than no data**.
- The same driver behaves correctly on an AT-class machine.
- A 16-bit Windows 3.x driver for the same card works, but the 32-bit Windows 9x one does not.
  Diagnostic gold: 16-bit drivers go through `VDMAD`'s bounce buffer, 32-bit ones allocate their
  own. If 3.x is fine and 9x is not, suspect this immediately.
- The machine has more RAM than its DMA reach.

## Step 1 - confirm it is actually this, by measurement

Do not skip to patching. Instrument the **page register write** in your emulator, or read it back on
hardware, and compare what the driver asked for against what the controller latched:

```
[dmapage] ch=1 val=4E -> page=0E *** TRUNCATED, buffer is above 1MB ***
     intended 0x4E0000 = 4.875 MB (top of RAM)   actual 0x0E0000 = 896 KB (adapter ROM)
```

If your emulator does not reproduce it, **check the emulator first**. A real case: 86Box derived its
"is this an AT?" flag from CPU type, so a 386-in-an-XT was handed a full 8-bit page register - the
emulator was hiding the bug and made it look hardware-only. An emulator that cannot reproduce a
hardware failure is a bug in its own right.

## Step 2 - audit the driver

`_PageAllocate` is VMM service **`0x00010053`**. Eight dwords, pushed right to left, caller balances
with `add esp, 0x20`:

```
_PageAllocate(nPages, pType, hVM, AlignMask, minPhys, maxPhys, PhysAddrPtr, flags)
```

`maxPhys` is a maximum **page number**, and it is the whole ballgame.

```
python scripts/vxd_dma_audit.py DRIVER.VXD        # one driver, read-only
python scripts/sweep_image_dma.py disk.img        # every VxD on a raw FAT12/16 image
```

`scripts/fatls.py` pulls files out of a raw image when the card is in the machine.
`scripts/fatput.py` writes a **same-size** replacement back over the file's own cluster chain, so
the FAT and directory entry are never touched and a bad run cannot corrupt the volume.

## Step 3 - read `maxPhys` as INTENT, not as a number

This is the judgment the whole skill exists to convey. **Do not blanket-patch.**

| `maxPhys` | What the driver is saying | Action |
|---|---|---|
| `0xFFF` / `0x1000` (16 MB) | "this buffer must be ISA-DMA reachable" - a deliberate constraint, just the wrong one | **patch** to your machine's real reach |
| `0xFFFFF` (4 GB) | "anywhere" - an ordinary allocation that never goes near the DMA controller | **leave alone** |
| `<=` your reach | already correct | nothing to do |
| a register | computed at runtime | **not statically decidable** - trace it, do not guess |

Patching the "anywhere" row is an active regression: it forces an ordinary allocation into the only
DMA-capable RAM the machine has, which is scarce precisely because the reach is narrow.

## Step 4 - patch, and know why this one is safe

```
python scripts/patch_vxd_dma_maxphys.py IN.VXD OUT.VXD
```

`push 0xfff` is `68 FF 0F 00 00`; `push 0xff` is `68 FF 00 00 00`. **Same 5-byte encoding.** Exactly
one byte changes per site and no instruction boundary moves.

That property matters more than it looks. The sibling failure mode - a patch script that
mis-identifies an instruction and writes bytes that *change its length* - turns a harmless
instruction into a wild write and produces a crash that looks like a driver bug. If your patch
changes an instruction's length, stop and re-derive against the file's real decoded instruction
stream, not against a byte pattern.

The script refuses to write a no-op, and post-checks that every changed byte landed inside a
`maxPhys` push.

Graceful degradation is usually already built in: drivers that ask for a large contiguous low buffer
tend to halve and retry on failure, so a refused allocation shrinks the buffer rather than breaking
the device.

## Step 5 - deploy where the OS will actually load it

**Check that the file you are replacing is the file that gets loaded.** On Windows 9x, Setup combines
staged VxDs into a single compressed `VMM32.VXD`; after that, a replacement dropped in the staging
folder is **silently ignored**. `BOOTLOG.TXT` tells you which path a driver took:

```
Loading Vxd = VDMAD                     <- bundled inside VMM32.VXD; a file copy will NOT take
Dynamic load device  mssblst.vxd        <- loaded from the file on disk; a file copy WILL take
```

A real case lost eighteen days testing a correct fix that was never being loaded. Bundled VxDs need
the pre-monolith route (replace before the combine step); dynamically loaded ones are a plain file
copy.

For the same reason, **sweep a pre-monolith image** if you want full coverage - VxDs inside a
combined `VMM32.VXD` cannot be audited at all, because that file is `W4` compressed. On one real
install, sweeping the pre-monolith copy found three affected drivers that were completely invisible
on the combined one.

## Step 6 - verify by measurement, then by ear or eye

```
before:  [dmapage] ch=1 val=4E -> page=0E  *** TRUNCATED, buffer is above 1MB ***
after:   [dmapage] ch=1 val=09 -> page=09  ok
```

Both halves matter. The trace proves the address is now reachable; only a human confirms the data is
actually right. Check the buffer is also **alignment-safe**: an 8-bit DMA channel cannot cross a
64 KB boundary, a 16-bit one cannot cross 128 KB.

## Things that look like the fix and are not

- **`SYSTEM.INI` `[386Enh]` `DMABufferIn1MB=Yes`.** Tempting, and wrong for 32-bit drivers. It clamps
  the **size** of `VDMAD`'s own bounce buffer. A 32-bit driver allocates its own buffer and hands
  `VDMAD` the physical address - nothing bounces, so the setting has no effect on where that buffer
  lives. It genuinely does govern 16-bit Windows 3.x drivers, which is why 3.x can work fine on the
  same hardware and is a clue rather than a contradiction.
- **Blaming the driver stack above it.** The symptom appears at the top of the stack; the cause is
  one `push` instruction near the bottom.

## Ceiling values worth knowing

| Machine class | DMA reach | `maxPhys` |
|---|---|---|
| PC/XT, 4-bit page latch | 1 MB | `0xFF` |
| PC/XT, strict (386MAX `@DMA_PA_XT`) | 640 KB | `0x9F` |
| AT, 8-bit page latch | 16 MB | `0xFFF` |

The strict XT value is 640 KB, not 1 MB - above that is adapter ROM and video, not RAM. `0xFF` is
safe in practice only because those pages are not allocatable RAM.

## Submitting a driver

If you have an XT-class machine and a driver that produces corrupt data, the audit is read-only and
takes seconds:

```
python scripts/vxd_dma_audit.py YOURDRIVER.VXD
```

Send the output, and the driver if you can, and it can be checked and patched. The audit reports; it
never modifies.

## Provenance

Derived from a real fix: Sound Blaster Pro audio on an Intel Inboard 386/PC in an IBM 5160 running
Windows 95 OSR1. `MSSBLST.VXD` allocated at `0x4E0000`, the 4-bit latch truncated it to `0x0E0000`,
and the card played adapter ROM. Two bytes fixed it, confirmed on real hardware 2026-08-24.
The 4-bit page register was called correctly from the hardware by **@andrew-hoffman** before any of
it was measured.
