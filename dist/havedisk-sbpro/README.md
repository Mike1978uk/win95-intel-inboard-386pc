# Sound Blaster Pro — Have Disk package for the 5160 + Inboard 386/PC

Installs the SB Pro exactly as Windows 95 does, but sources `MSSBLST.VXD` from
here — the patched build — instead of from the retail CABs.

Suggested by [@andrew-hoffman](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/5),
who pointed out that rewriting the INF to reference extracted files is simpler and safer than
repacking the CABs themselves.

**Untested.** Built, internally consistent, never installed on hardware or in emulation.

## This is an alternative, not a replacement

The original route — install the stock driver, then copy the patched `MSSBLST.VXD` over the top —
**remains supported and stays the default.** It is the one confirmed on real hardware; this package
is not. Both are listed side by side in
[`FIXES.md`](https://github.com/Mike1978uk/win95-intel-inboard-386pc/blob/master/FIXES.md).

| | copy over the top | this package |
|---|---|---|
| status | **confirmed on real hardware** | **untested** |
| steps | install driver, then copy one file | one install |
| failure mode if you get it wrong | forget the copy, silently keep the bug | INF refuses, visibly |
| works for VMM32-resident VxDs | no | no |

Use this one if you want a single install with nothing to remember afterwards. Use the copy route if
you want the exact thing this project measured.

Direct downloads for the copy route:
[patched `MSSBLST.VXD`](https://github.com/Mike1978uk/win95-intel-inboard-386pc/raw/master/vxd-patches/sound/MSSBLST_INBOARD.VXD)
· [stock original](https://github.com/Mike1978uk/win95-intel-inboard-386pc/raw/master/vxd-patches/sound/MSSBLST_stock.VXD)

## Why

Stock `MSSBLST.VXD` asks `_PageAllocate` for a DMA buffer anywhere below 16 MB. This machine's
DMA page latch is 4 bits — 1 MB of reach — so a buffer above that is silently truncated and the
card plays whatever sits at the wrong physical address. Not a crash; wrong data. The patched build
asks for 1 MB. Root cause and measurement in `FIXES.md` and issue #5.

## Contents

| file | | md5 |
|---|---|---|
| `SBPRO-XT.INF` | generated from OSR1 `WAVE.INF` | `d8f8d1025c9027954dfa3a0a3c1bb67b` |
| `MSSBLST.VXD` | **patched** — `maxPhys` 16 MB → 1 MB | `dcf32b4a7d8dbcc47e659847742417b6` |
| `MSSBLST.DRV` | stock, unmodified | `832370426db9827742fb13e5aa838697` |
| `MSOPL.DRV` | stock, unmodified | `628634f75fee940a4ab7ca6b85abbaed` |

## How the INF was produced

Not hand-written. The install sections were taken **verbatim** from OSR1's own `WAVE.INF` by
following the section references out of `[PNPB002_Device]` transitively —
`LogConfig`/`CopyFiles`/`AddReg`/`DelFiles`/`UpdateInis`/`UpdateIniFields` — which pulls in twelve
sections and names exactly three files. Only the wrapper changed: `[Version]` drops `LayoutFile`,
and `[SourceDisksNames]`/`[SourceDisksFiles]` point here instead of at the CABs.

That matters because the install logic is Microsoft's, byte for byte. Nothing about the device,
its resources or its registry entries has been reinterpreted.

Worth noting the resource defaults it carries are already right for this machine:

```
[PNPB002_Device.FactDef]
IOConfig=220-22F   IOConfig=388-38B   IRQConfig=5   DMAConfig=1
```

## Installing

Control Panel → Add New Hardware → **No** to autodetect → *Sound, video and game controllers* →
Have Disk → **type the path to this directory.**

**Do not use Browse.** It defaults to `A:\`, this machine has no floppy controller installed, and
that read never returns — that was the whole of issue #3.

## What this approach does not cover

Anything Setup bundles into `VMM32.VXD`. `VDMAD.VXD`, `VPICD.VXD` and `VKD.VXD` are combined into a
single compressed (`W4`) blob during Setup, so a file dropped in afterwards never loads regardless
of how it got there — see Technique 60. Those still need the pre-monolith route
(`vxd-patches/deploy_premonolith.sh`).

The distinction is whether the driver is **dynamically loaded from a file at boot** (`MSSBLST.VXD`,
`HSFLOP.PDR`, `LPT.VXD`, `QIC117.VXD`, and any `IOSUBSYS` miniport) or **combined into VMM32**. Only
the first kind can be shipped this way.
