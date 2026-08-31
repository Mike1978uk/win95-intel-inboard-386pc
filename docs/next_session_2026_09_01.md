# Next session — 2026-09-01

## The one rule for this session

**Do not test driver loading on the real 5160.** Whether a `.PDR` loads is a Windows VxD-loader
question and is entirely machine-independent. Reproduce it in 86Box, iterate there in seconds, and
only go to the real machine once `Init Success port.pdr` appears in an emulated boot log.

Eleven boots were spent on the real machine on a question the emulator answers for free. That is
the single biggest process failure of 2026-08-31.

---

## State

`PORT.PDR` `0681f388eab8173b483074aa00caa11d` (16,544 bytes) is deployed on the CF in both
`WINDOWS\SYSTEM\IOSUBSYS\` and `C:\PORTPDR\`. **It does not load.** Nothing is broken by this — a
port driver that fails to load has no effect on the running system.

`BOOTLOG.TXT` shows, every time:

```
[…] Initing port.pdr
[…] Init Failure port.pdr        1-4 ticks, and NO instrumented delay ever fires
```

An arrival marker on the **first instruction** of `PORT_Device_Init` never fires, so the VxD's
control procedure is never entered. The image is refused by the loader before any control message.

## What is PROVEN, and does not depend on the driver

All measured on the real hardware; none of it needs revisiting.

- **Register map:** stride 2, register N at `base + 2N`. Established by write/readback
  (`11h,22h`->`302,303` both read `00`; `33h,44h`->`304,305` both read `44h`), not by residue.
- **Control block** is a second strided bank: `alt status = base + 14 * stride` = `31Ch` here,
  `base+0Eh` on a classic XTIDE. Sweep showed `310`-`31A` = `FFh`, `31C` = `50h`.
- **8-bit PIO through `0x300`, no high-byte latch.** Marker test: `5Ah` written to `base+8` read
  straight back mid-DRQ, so `base+8` is cylinder low.
- **Full IDENTIFY works**: `word0 = 044Ah`, `3949/16/63`, LBA28 3,980,592 sectors, model
  **`TRANSCEND`**, block mode max 1 (no READ MULTIPLE), PIO mode 4 / 120 ns.
- **Card:** Lo-tech XT-CF rev 3 (sold by TexElec). Option ROMs: Mach8 `C0000` 32K, Sergey
  Multi-Floppy `D0000` 8K, XT-CF `D8000` 8K.
- **ROM flashed** XT v2.0.0b3+ -> XT+ r638, verified; geometry unchanged (`987/64/63`, heads and
  sectors identical so CHS-to-LBA mapping is untouched). Backup in `roms/xtcf_card/`.
- **Real-mode baseline: ~240 KB/s**, and only ~25% of that time is the data transfer. That is the
  number the 32-bit driver has to beat.

## What is BLOCKING

The DDK block-port sample cannot produce a loadable VxD as documented. It ships **no `.DEF`** and
`MASTER.MK` has no `.DEF` convention. Three image faults found and fixed by diffing against
`WINDOWS\SYSTEM\IOSUBSYS\HSFLOP.PDR`, which loads on the same machine:

| # | fault | fix | verified |
|---|---|---|---|
| 1 | `mod_flags 0x00028000` — not marked dynamically loadable | `VXD PORT DYNAMIC` in `PORT.DEF` | flags now `0x00038000`, matches |
| 2 | no segment had `PRELOAD` — nothing resident to hold the DDB/control proc | canonical `SEGMENTS` map | obj flags now `0x2045/0x2015/0x2023`, matches |
| 3 | `pagesize 512` (OS/2 layout) | `/ALIGN:4096` | now `4096`, `numpreload 1`, matches |

**All three were real and none was sufficient.** Every comparable LE header field now matches
`HSFLOP.PDR` and the image still will not load, so the remaining difference is below the header —
object page table, fixups, or something the loader checks that we have not compared.

## Driver-side fixes already in, waiting for the driver to run

These are correct and will matter the moment loading works. Do not re-derive them.

- `EDX` clobber: DEV was read through a register left pointing at alternate status (technique 78).
- Wrong register stride (was 1, is 2).
- Validator accepted garbage — `printable >= 4` anywhere; now requires an unbroken run.
- `Port_device_inquiry` tested an **uninitialised EAX** (its `sniff_for_drive` call is commented
  out) and answered `AEP_FAILURE` on a coin flip. Phase 1 now answers `AEP_NO_MORE_DEVICES`.
- **Never fail `AEP_INITIALIZE`** — IOS drops the driver permanently, so the CONFIGMG callback that
  supplies resources never arrives. From zikolas/cfu1-win9x.
- Probe outcome is delay-coded into the boot log; success also reports the model string's length
  (9 = `TRANSCEND`), so `~315 ticks` is the full-success signature.

## Startup list

1. **Set up an 86Box Win95 VM for driver-load testing.** Any machine type — this is not an Inboard
   question. Candidate images already in the repo: `vm_golden/new_golden_premonolith.img`,
   `vm_test_canonical/premonolith_canonical.img`. Mount host-side, drop `PORT.PDR` into
   `WINDOWS\SYSTEM\IOSUBSYS\`, boot logged, read `BOOTLOG.TXT`. Target: `Init Success port.pdr`.
2. **Diff below the header.** Object page table, fixup records and the resident name table against
   `HSFLOP_reference.PDR` (in the repo root). The header matches; the fault is deeper.
3. **Consider abandoning the DDK sample's build entirely.** zikolas/cfu1-win9x builds a *working*
   Win9x VxD with JWasm + Open Watcom v2 and a known-good `.DEF`/link recipe. Cloned at
   `../cfu1-win9x`. Borrowing his build harness may be faster than fixing MASM/LINK output.
4. Only then: real hardware, `F8` -> Logged.

## Owed

- **@andrew-hoffman** — the T130B outcome on the real 5160. Unchanged, still outstanding.
- **Nick (@zikolas)** — credited in `drivers/xtide_pdr/README.md` and `docs/resources_and_sources.md`
  for the `AEP_INITIALIZE` and CONFIGMG findings. Owner knows him personally and will tell him.

## Process failures to not repeat

1. **Test machine-independent questions in the emulator.** Eleven boots, all avoidable.
2. **Diff the whole header, not the fields you suspect.** Faults 1 and 2 cost a boot each because
   only guessed-at fields were compared. Fault 3 took one command once everything was diffed.
3. **Instrument on the near side of the call you doubt.** A delay placed *after* `IOS_Register`
   could not distinguish "never called" from "never returned" — three boots lost to that.
4. **`Initing` is not `Init Success`.** Phase 0 was recorded as passing on the wrong log line, so a
   driver that never loaded was believed working for four sessions.
5. **Compare against something known to work, early.** `HSFLOP.PDR` was in the same directory,
   loading successfully, in every boot log read.
