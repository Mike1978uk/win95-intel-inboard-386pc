# SCSIPORT.PDR, DMA buffer constrained to the XT's 20-bit reach

`maxPhys 0x1000 -> 0xff` and `0xfef -> 0xff`, the two `_PageAllocate` ISA-DMA
ceilings in Microsoft's `SCSIPORT.PDR`. Four bytes, all inside a `push imm32`,
no instruction boundary moved. Technique 62.

| | md5 |
|---|---|
| stock (OSR1, off this machine's CF) | `1e3dcdf0` |
| patched | `3f85fa2b` |

## Why it was not needed before, and is now

A runtime trace across a full verified boot showed the DMA page register
programmed six times, all channel 1 - nothing but the SB Pro does ISA DMA on
this machine, so SCSIPORT's allocation was never exercised. Enabling the
Imation miniport's `ECP=1 DMA=3` is what makes it live: with ECP on, the
driver declares a `DmaChannel` and SCSIPORT owns the bounce buffer.

Without this, a buffer above 1 MB does not fault - the 4-bit page latch drops
the high bits and the transfer runs against a different physical address.
Silent wrong data.

## Blast radius

SCSIPORT also serves `XTIDEMP.MPD` (the boot disk) and `T130.MPD`. Constraining
the buffer below 1 MB is correct for all of them on this machine, but `C:` is
in the blast radius if anything is wrong. The stock file is kept on the card at
`C:\SCSIPORT.ORG`; restore it from real-mode DOS.

⚠ **UNTESTED.** Staged, never booted.
