# Imation / Shuttle LS-120 parallel-port driver — patched for XT-class hardware

**Status: patch applied and deployed once; result inconclusive. Not yet a verified fix.**
Tracked as [issue #22](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/22).

## Why this exists

`SD120PPD.MPD` probes for a host chipset at ports `0x22`/`0x23` (and `0x24`/`0x25`, and writes the
PS/2 setup register at `0x94`). On an IBM 5160 the 8259 is decoded across `0x20`–`0x3F` — **measured**:
ports `0x21`, `0x23`, `0x25`, `0x31` and `0x3F` all return the interrupt mask. So those writes land
on the interrupt controller, and one sequence leaves the mask at `0x06` (IRQ 1 and IRQ 2 masked)
with no restore.

Background: [`docs/xt_io_aliasing_gotcha.md`](../../docs/xt_io_aliasing_gotcha.md).

The DOS build of the same driver has `/ni` ("Skip chipset initialization") and it was in use on this
machine. The protected-mode miniport exposes no equivalent, so the writes are patched out instead.

## Files

| file | md5 | what |
|---|---|---|
| `SD120PPD.MPD.orig` | `08104ffb559ae4b47b84377daee473bc` | untouched vendor binary, 79,872 bytes, 1997-05-26 |
| `SD120PPD.MPD.patched` | `eedc94fcfdf7d8916a4598cf8c8157e5` | 98 writes NOPed |
| `SD120PPD.sites` | | offset:opcode:port map, needed by `--revert` |

Copyright remains with Adaptec/Imation. Kept unmodified alongside the patch so the change is
reproducible and reversible.

## What the patch does

Replaces every `out` to `0x22`, `0x23`, `0x24`, `0x25` and `0x94` with two `NOP`s — **98 sites**,
byte (`E6`) and word (`E7`) forms. Reads are left alone: reading the 8259 has no side effect, and
detection then simply finds nothing, which is what `/ni` achieves.

It deliberately does **not** touch `out 21h`. Those sites are paired save/restore around the probe
and are already neutral; removing one half would be worse than leaving both.

Verified: size unchanged, all edits inside `.text`, control flow untouched, and `--revert`
round-trips byte-perfect to the original md5.

Regenerate from the original with:

```
python dist/post-install-fixes/scripts/patch_sd120ppd_chipset.py SD120PPD.MPD.orig \
       -o SD120PPD.MPD.patched
```

## How to apply

Install the driver normally first (Have Disk, `OEM0.INF`), then replace the miniport. It lives in
`C:\WINDOWS\SYSTEM\IOSUBSYS\SD120PPD.MPD`.

**Over COMrade, from DOS** — the machine keeps running, no image shuffling:

```
copy C:\WINDOWS\SYSTEM\IOSUBSYS\SD120PPD.MPD C:\WINDOWS\SYSTEM\IOSUBSYS\SD120PPD.ORG
```
then `file_write` with `src_path = drivers\imation_ls120\SD120PPD.MPD.patched`, and confirm with
`file_hash` against the same host file — it must report `match: true`.

**Or with the CF mounted on a host:** copy the file, keeping a `.ORG` backup beside it.

Do it from DOS, not from within Windows — the file is not locked there.

## How to revert

Three independent routes, in increasing order of effort:

1. Copy `SD120PPD.ORG` back over `SD120PPD.MPD`.
2. `patch_sd120ppd_chipset.py <file> --revert --map SD120PPD.sites`.
3. Device Manager → SCSI controllers → the Imation/Shuttle entry → Remove → reboot. **Works with the
   mouse alone**, which matters if the keyboard is the thing that broke.

## What happened on the first attempt, and how to test it properly

Deployed 2026-08-28 and verified in place. **The drive continued to work** — so removing the chipset
writes does not break the transfer path, which was the stated risk. **The keyboard did not come
back.**

That result is **not attributable**, because DirectX 7.0a, WinZip, InfoPro and SIV had all been
installed between the last known-good boot and the fault. Any of them could be responsible.

To get a real answer, on a clean image:

1. Start from a build **without** DirectX 7.0a. It buys nothing here — the Mach8 is a 2D 8514/A
   accelerator with no DirectDraw path, and DirectSound on an SB Pro gains nothing over the standard
   driver. It is risk surface with no benefit.
2. **Confirm the keyboard works.** Baseline first.
3. Install the LS-120 driver. **Reboot. Test the keyboard.** This is the single-variable test that
   was never actually run.
4. If the keyboard survives with the *stock* driver, the chipset-probe theory is dead and the patch
   is unnecessary — record that.
5. If the keyboard dies, swap in `SD120PPD.MPD.patched`, reboot, and test again. That is the clean
   before/after.

**One install, one boot, every time.** That is the whole difference between a result and a
confound.

## 2026-09-07: the configuration surface is exhausted too (desk analysis, no machine time)

The vendor README documents `PORT=`/`IRQ=` only. The binary's `AdapterSettings` parser
(`0x138c1`-`0x139c3`, one helper at `0x132c0`) actually knows **nine** keywords: `BLK`, `DMA`,
`ECP`, `MSN`, `NATN`, `NDPC`, `port`, `size`, `irq`.

**Four of them are dead strings.** Counting every absolute reference across `.text`:

| global | refs | verdict |
|---|---|---|
| `MSN` `[0x204a8]` | 1 | parser write only - never read |
| `NATN` `[0x204ac]` | 1 | parser write only - never read |
| `NDPC` `[0x204b0]` | 1 | parser write only - never read |
| `BLK` `[0x20494]` | 3 | all inside the parser - never read outside it |
| `ECP` `[0x204bc]` | 6 | real readers at `0x12ad3`, `0x13239`, `0x1401e`, `0x14a1a` |
| `DMA` `[0x204c4]` | 3 | real reader at `0x1404c` |

Parsed, stored, never consumed - vestigial from the shared Shuttle codebase. The live knobs are
`port`, `irq`, `size`, `ECP`, `DMA` only.

**And none gates the destructive writes.** Those sit at `VA 0x18DA6`-`0x1A175`, `0x1C54B`-`0x1CF7B`
and `0x1F9CA`-`0x1FCE9`; every live `ECP`/`DMA` reader is in `0x12AD3`-`0x14A1A`, nowhere near them.
**There is no `AdapterSettings` equivalent of the DOS `/ni` (Skip chipset initialization).**

The scan is self-checking: it finds four cross-function readers of `ECP`, so it detects readers
outside the defining function. It would miss a runtime-computed reference.

So the "persuade the vendor binary to behave" family is exhausted from both ends - its code paths
(six patches, six nulls) and its configuration surface. What remains is writing our own `.MPD`,
for which `XTIDEMP.MPD` is now a shipped template.

⚠ **Licence, to settle before any code:** the obvious protocol reference is Linux's
`drivers/block/paride/epat.c`, **GPL-2.0**. This repo is MIT. Register facts are not copyrightable;
a port of that code would be a derivative work.

## 2026-09-10: the miniport is open

`tools/pedis.py` disassembles it (`python tools/pedis.py all > SD120PPD.asm`; also `imports`, `io`,
`str`, `dis <rva> [n]`). The `.asm` is not tracked - it is a derived work of a vendor binary and
regenerates in one command.

It is a plain i386 PE, ImageBase `0x10000`, `.text` at RVA `0x400`. **Addresses in the earlier
section above are VAs; subtract `0x10000` for the RVA** the tool prints.

`DriverEntry` (RVA `0x3793`) builds one `HW_INITIALIZATION_DATA` and calls `ScsiPortInitialize`
twice, with `AdapterInterfaceType` 1 then 3:

| field | RVA |
|---|---|
| `HwFindAdapter` | `0x3854` |
| `HwInitialize` | `0x3277` |
| `HwStartIo` | `0x29ac` |
| `HwInterrupt` | `0x2892` |
| `HwResetBus` | `0x2914` |
| `HwDmaStarted` | `0x48ac` |
| `HwAdapterState` | `0x384f` (stub, `mov al,1 / ret 0Ch`) |
| `NumberOfAccessRanges` | 1 |

It imports thirteen SCSIPORT entry points and nothing else - `GetDeviceBase`, `FreeDeviceBase`,
`Initialize`, `Notification`, `CompleteRequest`, `GetLogicalUnit`, `LogError`, `StallExecution`,
`ConvertUlongToPhysicalAddress`, and the four `Read/WritePortBuffer Uchar/Ulong`. That list is the
whole SCSIPORT surface our own `.MPD` needs; the `Ulong` buffer pair is the ECP FIFO path.

`HwFindAdapter` scans `0x378`, `0x3BC`, `0x278` in that order when no `port` is given.

**The chipset writes live in exactly two functions**, `0x81a8` and `0xc839`, and `0x81a8` is reached
from `HwResetBus` (`0x2914`) as well as from `0x3183` and `0x1fdc`. So they are on the normal
service path, not an init-only path - which confirms from a second direction why no `AdapterSettings`
key can stand in for `/ni`, and why the blanket NOP patch is the only configuration-free answer.

One correction to the table above: `ECP=0` does have a second effect the reference count missed. At
`0x401c`-`0x4063` a zero `ECP` key clears the ECP flag, which then skips storing the DMA channel into
the device extension (`[esi+0x20]`) and sets `[esi+0x51]=1` instead. So `ECP=0` does suppress the
DMA *transfer* path, even though it does not suppress the chipset probe. `DMA=0` on its own is
useless - the parser coerces a zero `DMA` key to `3`.

## The DOS driver's full switch list, for reference

Pulled from `SD120PPD.SYS`'s own help text - several were not previously recorded:

```
/dm  disable read multiple mode        /de   Disable Epp check
/dpc Disable SHUTTLE-PCMCIA-P support  /ded  Disable Epp Dword Xfers
/rx  x = 0..11 (read timing)           /db   Disables Eppbios check
/wy  y = 0..4  (write timing)          /dp   Skip PS/2 Dma Arbitration disable
/di  Operate in polled mode            /fp   Disable PS/2 Dma Arbitration
/fe  force 386sl EPP initialization    /ni   Skip chipset initialization
/fev force VLSI chipset EPP init       /sf   Skips fast mode detection
/ix  force int in x (7 or 5)           /pd   Enables power down operation
/IRQ:x  force irq x (1 to 15)          /P:xxx force portbase
```

`/fe` and `/fev` name **why** those ports are written: 386SL and VLSI chipset EPP setup. On an XT
they land on the 8259 instead.

## Before installing any other stock driver here

```
python dist/post-install-fixes/scripts/xt_port_audit.py YOURDRIVER.MPD
```

It flags fixed-port writes landing in the XT's aliased device blocks. It already cleared
`T130.MPD` (zero destructive writes) before that driver went near the machine.
