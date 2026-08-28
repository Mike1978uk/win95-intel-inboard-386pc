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

## Before installing any other stock driver here

```
python dist/post-install-fixes/scripts/xt_port_audit.py YOURDRIVER.MPD
```

It flags fixed-port writes landing in the XT's aliased device blocks. It already cleared
`T130.MPD` (zero destructive writes) before that driver went near the machine.
