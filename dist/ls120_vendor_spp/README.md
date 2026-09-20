# Imation SuperDisk LS-120, parallel port — Have Disk package

The vendor's own Windows 95 driver for the parallel-port LS-120, with its
probe suppressors applied **at install** instead of typed afterwards.

On an XT-class bus the driver's EPP and chipset probes poke ports that alias
onto the 8259, and the keyboard dies. You then cannot type the switches that
would have prevented it. Putting them in the INF breaks that circle.

## What is changed from the vendor original

One line, in `[epatlsreg]` — the section the install actually references:

```
HKR,,AdapterSettings,,"PORT=0x378 /ni /de /db /sf /dp /dpc /fp"
```

Nothing else. `SD120PPD.MPD` here is the vendor binary, unmodified
(`md5 08104ffb559ae4b47b84377daee473bc`).

## Install

1. Copy this folder somewhere the machine can read — the root of `C:` is fine.
2. Control Panel → Add New Hardware → **No**, do not let Windows search.
3. SCSI controllers → **Have Disk** → point at this folder.
4. Pick *Imation SuperDisk Drive - Parallel Port*, and reboot.

Change `PORT=0x378` in the INF first if your port is elsewhere.

## Verified on a real IBM 5160 with an Intel Inboard 386/PC

| check | result |
|---|---|
| enumeration | immediate, drive letter present |
| keyboard | survives the install |
| write-protect reporting | correct |
| `WC2P9XUP.EXE` 427,273 B | `FC: no differences encountered` |
| `WC2MON.EXE` 2,975,731 B | `FC: no differences encountered` |

3.4 MB read back byte-identical. Written by the Windows miniport and read by
the DOS driver, so a symmetric error in one path cannot hide itself.

## What this does NOT claim

- **The negotiated transfer mode was never read back on the Windows path.**
  `/de` disables the Epp check and `/db` the Eppbios check; neither suppresses
  ECP detection. With the **DOS** driver the same switch set autodetects to
  `ECP Read` / `ECP Write`. What the Windows miniport settles on here is
  unmeasured — do not assume SPP, and do not assume ECP.
- Throughput is not measured. Working and fast are separate claims.
- Tested on one machine, one bridge (Shuttle EPAT), one drive (Matsushita
  LS-120 COSM 04) at `0x378`.

## Do not add `/r` or `/w`

Forcing a read or write mode here cancels against `/de` and drops the link to
nibble. `/ded` and `/fed` are not switches at all — the parser ignores them.

## Provenance

`120PPD95.INF` and `SD120PPD.MPD` are Shuttle Technology / Imation's, from the
`ls120_95` installer's `DATA.Z`, extracted with `tools/blast.py`. They are
redistributed here unmodified apart from the single INF line above, so that a
machine which loses its keyboard during a stock install can still be brought
up. Copyright remains with the original authors; the MIT licence on this
repository covers this project's own work, not these two files.
