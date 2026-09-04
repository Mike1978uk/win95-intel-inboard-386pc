# Next session — 2026-09-05

**Supersedes `next_session_2026_09_04.md`, which is retracted at the top of its own file.**

## Where the XT-IDE port driver (#21) actually stands

The driver works. It boots Windows, mounts the volume, reads and writes correctly through an
8-bit XT-CF on a stride-2 register map, and on the real 5160 it takes the disk in protected mode
with the real-mode mapper unloaded. **One defect remains: Windows will not finish shutting down.**

## The bug, closed to one register

```
C0003184:  8B 6B 08            mov ebp,[ebx+8]
           39 05 F4 E9 00 C0   cmp [C000E9F4], eax
C000318D:  75 ED               jne -19        ; loop while they differ; EAX never reloaded
```

| | EAX | EBX | outcome |
|---|---|---|---|
| clean (driver absent) | `C0FCE28C` — a VMM pointer | `C15200E8` | falls through; shutdown completes |
| hung (driver present) | **`00000000`** | `C15200E8` — **the same object** | spins forever |

`[C000E9F4]` is never written in **either** run, so the global is not the variable. The clean run
exits because EAX already matches. **With our driver loaded, VMM enters this wait holding a null
handle where a valid pointer belongs.**

Everything downstream follows: the state write at `C00032B4` never runs (0 writes with the driver,
213 without), so nothing advances and the shutdown waits forever.

### Do this first

EAX is set before `C000317F`. The caller path on a **clean** run is:

```
C0002657..C0002665 -> C0002795 -> C000352E..C0003559
-> C0001300..C0001354            (dispatcher; C0001300 is the return address)
-> C000317F .. C000318F          (the loop)
-> C0003190..C0003268 -> C0001359 C0001360 -> C000329A..C00032B4
```

Trace backwards from `C0001300`–`C0001354` and find what returns zero. It will be an object VMM
asks for at `System_Exit` that our calldown insert changes.

## The bed — this is the enabling piece, do not lose it

`vm_xtcf_faithful/` is the **first emulator bed that matches the real card**, and the hang only
reproduces there. It has your CF's own driver (`0fe2431a`) restored and ready.

86Box needed two fixes to model the card, both on `Mike1978uk/86Box` branch `xtcf-lotech-stride2`:

- **stride-2 decode** — the Lo-tech XT-CF rev 3 does not decode A0, so registers sit at `base+2N`
  over 32 ports. New `Register stride` option, default 1, so every existing config is unchanged.
- **8-bit PIO** — the card's BIOS sends `SET FEATURES 01h` and the data register becomes byte-wide
  with no high latch. 86Box was reading 16 bits per access and dropping every second byte.
- plus a BIOS entry for the XT+ r638 image read back off the card
  (`roms/hdd/xtide/ide_xtcf_lotech.bin`).

Without these the driver silently falls back to a stride-1 path, never properly claims the disk,
`RMM.PDR` serves it instead, and **the bug cannot appear**. That is why four emulator shutdowns
looked clean and told us nothing. See technique 90.

Diagnostics in that fork are marked `DIAGNOSTIC, not for upstream` and must be stripped before any
PR: the CS:EIP heartbeat and caller trace in `386_dynarec.c`, the `0E0h` debug port in
`inboard386.c`, the linear write watchpoint in `mem.c`, and the `ENABLE_XTIDE_LOG` define.
One genuine upstream fix is mixed in and should be kept: `xtide_init` never called `log_open`.

## Eliminated by measurement — do not re-run these

| candidate | how it died |
|---|---|
| the LS-120 real-mode chain (`SD120PPD.SYS`, `ASPIHDRM.SYS`) | REM'd on the CF, hardware still hangs. `IOS.LOG` disappeared entirely, so IOS's complaint and the hang are independent |
| the published logical volume | `-NoVolume` still hangs |
| claiming master+slave | `-ClaimMask 1` still hangs |
| the polling contract / `Set_Global_Time_Out` | the wait is ordinary VMM code a clean shutdown walks straight through |
| a timeout storm | 3,582 status polls where one expiry needs 400000 |
| the fast-fail clamp built 09-04 morning | never armed — `AEP_SYSTEM_SHUTDOWN` does not reach our handler |

## Two standing bugs found on the way, neither chased

1. **`AEP_SYSTEM_SHUTDOWN` never reaches `Port_Async_Request`** — proven by zero output from the
   `0E0h` counters. So `XTIDE_VolDown` has never run. `AEP_UNCONFIG_DCB` is tested first and
   returns early; that is the likely explanation and it is untested.
2. **The duplicate `D:` is solvable.** With `-NoVolume` the machine shows only `C:`, and Windows'
   own `DiskTSD` assigns it (your bootlog shows `INITCOMPLETE = DiskTSD`). The driver does not need
   to be its own TSD. That closes the single-disk goal independently of the hang.

## Provenance — the reason 2026-09-04 cost a day

A shutdown hang was chased for a day in a binary (`70298a8f`) that could not be rebuilt from any
commit: built from an uncommitted tree on 09-03, deployed straight into the emulator image, source
lost. `build.ps1` now prints the commit, shouts on a dirty tree, and appends md5 → provenance to
`drivers/xtide_pdr/build_ledger.tsv`. **Commit before deploying anything you will draw a
conclusion from.** Technique 89.
