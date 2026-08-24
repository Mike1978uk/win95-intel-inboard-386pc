# The 20-bit DMA problem, and a repeatable audit for it

**Status 2026-08-24: FIXED AND CONFIRMED ON REAL HARDWARE.** Verified first in the `vm_golden`
twin, then deployed to the CF card and confirmed on the 5160. Audio clean on both.

```
before:  [dmapage] ch=1 val=4E -> page=0E  *** TRUNCATED, buffer is above 1MB ***
after:   [dmapage] ch=1 val=09 -> page=09  ok
```

`0x090000` = 576 KB. It is also 64 KB-aligned, so a 64 KB buffer cannot straddle the 64 KB
boundary an 8-bit DMA channel is unable to cross. Evidence in `docs/evidence/`.

## The problem in one sentence

An Intel Inboard 386/PC in an IBM 5160 keeps the XT motherboard's **4-bit DMA page latch**, so DMA
reach is **20-bit (1 MB)**. Every ISA-era Windows driver assumes **24-bit (16 MB)**. A driver that
allocates its DMA buffer above 1 MB does not crash — the page register silently drops the high bits
and the 8237 transfers against a completely different physical address.

## How it was measured, not guessed

A gated trace was added to `dma_page_write()` in `86box_full/src/dma.c` (env var
`INBOARD_DMA_TRACE`, capped, off by default). With the Sound Blaster Pro playing:

```
[dmapage] ch=1 val=4E -> page=0E *** TRUNCATED, buffer is above 1MB ***  (xt_latch=1)
```

| | Page | Physical base | What is actually there |
|---|---|---|---|
| Windows intended | `0x4E` | `0x4E0000` = 4.875 MB | the real buffer — 128 KB below the top of 5 MB RAM |
| The 8237 used | `0x0E` | `0x0E0000` = 896 KB | adapter ROM / BIOS extension space |

The card plays whatever is in ROM space. That is the distortion in issue #5 — audible on **real
hardware and in the emulator on the same disk image**, which is what made it tractable.

@andrew-hoffman called this on 2026-08-20 from the hardware alone. He was right.

## Why `DMABufferIn1MB=Yes` did not fix it

Tested and measured: the same `val=4E → page=0E` truncation, unchanged.

Disassembling stock `VDMAD.VXD` shows why. At `OBJ3:0x162`:

```asm
cmp eax, 0x100          ; 256 pages = 1 MB
jbe keep
mov eax, 0x100
keep:
mov dword ptr [0x2100], eax
```

`DMABufferIn1MB` clamps `[0x2100]`, the **size** of VDMAD's own bounce buffer. It says nothing about
where anyone else's buffer lives. VDMAD is not the naive party here — it already distinguishes
machine classes (`cmp byte ptr [0x211a], 2` guards three separate 24-bit masks).

**Windows 95's sound driver allocates its own buffer** via `_PageAllocate` and hands VDMAD the
physical address. VDMAD faithfully programs it. Nothing is bouncing.

This also explains why Windows 3.11 works on the same hardware: its sound driver is a 16-bit `.DRV`
whose DMA goes through VDMAD's bounce buffer, which `DMABufferIn1MB=True` *does* govern.

## The actual mechanism

`_PageAllocate` is VMM service `0x00010053`. Eight dword arguments, pushed right to left, caller
balances with `add esp, 0x20`:

```
_PageAllocate(nPages, pType, hVM, AlignMask, minPhys, maxPhys, PhysAddrPtr, flags)
```

`maxPhys` is a maximum **page number**. `MSSBLST.VXD` (Microsoft's Sound Blaster VxD) at
`OBJ4:0x0069`:

```asm
push 0xe          ; flags
push ebx          ; PhysAddrPtr
push 0xfff        ; maxPhys  = 4095 pages = 16 MB   <-- the ISA assumption
push 0            ; minPhys
push eax          ; AlignMask
push 0            ; hVM
push 1            ; pType
push ecx          ; nPages
VxDCall 0x00010053
```

VMM honours `maxPhys` and allocates from the top of available RAM downwards — landing at `0x4E0000`.

## The fix

`maxPhys` `0xFFF` → `0xFF` (pages 0…0xFF = exactly 1 MB).

`push 0xfff` is `68 FF 0F 00 00`; `push 0xff` is `68 FF 00 00 00`. **Same 5-byte encoding**, so one
byte moves per site and no instruction boundary shifts — the failure mode that corrupted `VDMAD` in
`patch_vdmad.py` is structurally impossible here.

Contiguous low memory is scarce, but `MSSBLST`'s first call site already halves `nPages` and retries
on failure (`OBJ4:0x78  shr eax,1`), so a refused allocation shrinks the buffer rather than breaking
playback.

## The sweep across the whole install (2026-08-24, after the hardware confirmation)

`tools/sweep_image_dma.py <image>` walks a raw disk image, decodes every VxD and reports a verdict
per file. Run it against a **pre-monolith** image: on a combined install the VxDs inside `VMM32.VXD`
are invisible, because that file is `W4` compressed.

| File | maxPhys | Where it lives | Verdict |
|---|---|---|---|
| `MSSBLST.VXD` x2 | `0xFFF` | `WINDOWS\SYSTEM` | **fixed, confirmed on hardware** |
| `LPT.VXD` | `0x1000` | `WINDOWS\SYSTEM` | patched, built, not deployed |
| `QIC117.VXD` | `0xFFF` | `WINDOWS\SYSTEM` | patched, built, not deployed |
| `SCSIPORT.PDR` x2 | `0x1000`, `0xFEF` | `IOSUBSYS` | only loads with a SCSI miniport |
| `VFINTD.386` | `0xFFF` | `C:\DOS` | DOS/Win3.x era, not loaded by Win95 here |
| `IOS.VXD` x2 | `0xFFF` | **inside `VMM32`** | hold — see below |
| `VFAT.VXD` | `0x1000` | **inside `VMM32`** | hold |
| `VFBACKUP.VXD` | `0xFFF` | **inside `VMM32`** | hold |
| `MMDEVLDR.VXD` | `0xFFFFF` | `WINDOWS\SYSTEM` | **leave alone** — "anywhere", not a DMA buffer |

A further ~20 files call `_PageAllocate` with `maxPhys` supplied in a **register** — not statically
decidable. `VDMAD`, `V86MMGR`, `IFSMGR`, `NDIS`, `HSFLOP` and `DOSMGR` are all in that group. They
need runtime tracing, not disassembly.

### Why the three inside VMM32 are deliberately not patched

Not caution for its own sake — a measurement. Across a full verified boot, the page register was
programmed **six times, all channel 1**:

```
[dmapage] ch=1 val=00 -> page=00 ok
[dmapage] ch=1 val=09 -> page=09 ok
[dmapage] ch=1 val=09 -> page=09 ok
```

Nothing else on this machine does ISA DMA at all. `IOS.VXD` is the I/O Supervisor; patching it costs
a full pre-monolith rebuild and puts the boot path in the blast radius, to fix something that is not
happening. Revisit if a trace ever shows a second channel being programmed.

### A refinement not yet made

`maxPhys = 0xFF` is a 1 MB ceiling. Andrew's parenthetical — "or really first 640k" — is the
stricter and more accurate one, and 386MAX encodes `@DMA_PA_XT = 0x000A0000`. `0xFF` is safe in
practice because pages `0xA0`–`0xFF` are not RAM, so `_PageAllocate` cannot hand them out, and the
allocation empirically landed at `0x09`. `0x9F` would be strictly correct. Deliberately not changed:
the `0xFF` build is verified on real hardware, and re-cutting a proven artefact needs a re-test, not
a rationale.

## The audit across every VxD on the machine

`tools/vxd_dma_audit.py` decodes each VxD's real instruction stream (VxD `INT 20h` + inline DWORD
convention included), finds every `_PageAllocate`, and reads `maxPhys` statically.

| File | maxPhys | Verdict |
|---|---|---|
| `MSSBLST.VXD` ×2 | `0xFFF` (16 MB) | **patch** — Sound Blaster Pro, issue #5 |
| `LPT.VXD` | `0x1000` (16 MB) | **patch** — parallel port ECP DMA |
| `QIC117.VXD` | `0xFFF` (16 MB) | **patch** — floppy tape |
| `MMDEVLDR.VXD` | `0xFFFFF` (4 GB) | **ignore** — "anywhere", an ordinary allocation, not a DMA buffer |

That last row is the judgment the tool encodes and a blanket patch would get wrong. `~16 MB` is a
driver *deliberately* declaring an ISA DMA constraint. `0xFFFFF` means "don't care" — forcing it low
would waste the only DMA-capable RAM the machine has.

**Caveat:** VxDs bundled inside `VMM32.VXD` cannot be audited this way — that file is `W4`
compressed. Audit them from their pre-monolith staging copies instead.

## What exists right now

| Artefact | State |
|---|---|
| `tools/vxd_dma_audit.py` | works; read-only; run on any VxD |
| `tools/fatls.py` | read-only FAT12/16 lister/extractor for the raw images |
| `vxd-patches/patch_vxd_dma_maxphys.py` | generic; refuses a no-op; post-checks its own output |
| `vxd-patches/sound/MSSBLST_stock.VXD` | extracted from the emulator image |
| `vxd-patches/sound/MSSBLST_INBOARD.VXD` | patched, 2 bytes, re-audited clean; **verified in the emulator** |
| `tools/fatput.py` | writes a same-size replacement file into a raw image over its own cluster chain |
| `tools/shot.ps1` | PrintWindow screenshot of the running 86Box window |
| `86box_full/src/dma.c` trace | `INBOARD_DMA_TRACE=1`, capped, inert otherwise |
| Deployment | emulator: **done and verified**. Real hardware: pending |

## Next session, in order

1. ~~Deploy `MSSBLST_INBOARD.VXD` in the emulator first.~~ **DONE 2026-08-24.** Written straight
   into the image with `tools/fatput.py` — same size, so it goes over the file's own cluster chain
   and the FAT and directory entry are never touched. Reverting is the same command with
   `MSSBLST_stock.VXD`.
2. ~~Play a sound with `INBOARD_DMA_TRACE=1`.~~ **DONE.** `page=09`, no `TRUNCATED`, audio clean.
   Windows 95's own startup sound triggers it — no need to drive the GUI.
3. ~~Deploy the same file to the CF card and confirm on the 5160.~~ **DONE. Clean audio on real
   hardware.** Deployed with `tools/deploy_sound_fix.sh /d`, which md5-checks the card's existing
   file and refuses anything it was not derived from.
4. ~~Then `LPT.VXD` and `QIC117.VXD` for completeness.~~ **Built** —
   `vxd-patches/dma/LPT_INBOARD.VXD`, `vxd-patches/dma/QIC117_INBOARD.VXD`, both re-audited clean.
   Not deployed; neither device is in use, so this is correctness, not a fix.
5. Tell @andrew-hoffman. His 4-bit page register call was right, and the reason it took this long is
   that the first test of it was run against a corrupted VDMAD and recorded as void rather than
   negative — which is the only reason it survived to be retested.

## Still open, unrelated to this

Issue #7's black screen has a candidate mechanism (port `0x4AE8` bit 0 hands the display from the
VGA core to the 8514) recorded as Technique 61, and reproduces in the emulator. Not investigated
further today.
