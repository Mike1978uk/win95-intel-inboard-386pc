# The 20-bit DMA problem, and a repeatable audit for it

**Status 2026-08-24: root cause measured, fix built and verified as a file, NOT yet deployed or
tested on either machine.** Deployment is the first task of the next session.

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
| `vxd-patches/sound/MSSBLST_INBOARD.VXD` | patched, 2 bytes, re-audited clean |
| `86box_full/src/dma.c` trace | `INBOARD_DMA_TRACE=1`, capped, inert otherwise |
| Deployment | **none — nothing has been tested yet** |

## Next session, in order

1. Deploy `MSSBLST_INBOARD.VXD` to `C:\WINDOWS\SYSTEM\MSSBLST.VXD` **in the emulator first** — it is
   not a `VMM32` bundled VxD, so no pre-monolith image is needed and it is revertible by copying the
   stock file back.
2. Play a sound with `INBOARD_DMA_TRACE=1`. Success is `[dmapage] ch=1` showing a page `≤ 0xFF` with
   no `TRUNCATED` marker, and clean audio.
3. If clean, deploy the same file to the CF card and confirm on the 5160.
4. Then `LPT.VXD` and `QIC117.VXD` for completeness — neither is in use today, but the machine should
   be correct, not just working.
5. Tell @andrew-hoffman. His 4-bit page register call was right, and the reason it took this long is
   that the first test of it was run against a corrupted VDMAD and recorded as void rather than
   negative — which is the only reason it survived to be retested.

## Still open, unrelated to this

Issue #7's black screen has a candidate mechanism (port `0x4AE8` bit 0 hands the display from the
VGA core to the 8514) recorded as Technique 61, and reproduces in the emulator. Not investigated
further today.
