# The L1 cache is enabled and covers nothing Windows uses — 2026-09-12

Read off the real 5160 over COMrade, real-mode DOS, `C:\CTCHIP\CPUSHOW.BAT` (read-only).

## What the CPU actually holds

| register | value | meaning |
|---|---|---|
| `1000h:0` | `92` | CE **enabled**, ASNP enabled, CPC enabled |
| `1000h:1` | `9C` | CNPX, XTOUT, CRLD, IKEN enabled |
| `1000h:2` | `00` | |
| `1001h:0/1` | `FF` / `03` | LMCR = `03FF` — **low 640 KB cacheable** |
| `1001h:2/3` | `00` / `00` | LMROR — nothing read-only |
| **`1001h:4`** | **`00`** | **CMLR — nothing from 1–16 MB is cacheable** |
| `1001h:5/6` | `00` / `00` | ECMLR — nothing above 16 MB |
| `1002h:3` | `03` | 2:1 clock — doubling confirmed live |
| `1004h:3` | `00` | NA16 off |

Windows 95 lives entirely above 1 MB. **So the cache is on and does nothing for Windows.**

This reconciles two observations that looked contradictory: the owner's clock double plainly
works, and red-ray's measurements show no cache effect. Both are correct.

## The value to write, and where it comes from

`CPUSET.BAT` never wrote `1001h:4`. That was deliberate and recorded at the time:

> *"NOT replicated on purpose: 1001h bytes 2/3/4 … read those off CPUSHOW.BAT on the 3.11
> machine and they can be added properly."* — `CTCHIP/README.TXT`

The reference was finally taken. REVTO486 1.04's own dump, on the DOS/3.11 configuration that
has run this machine for years:

```
MSR1000  = 0000 9C92    (processor operations)
MSR1001L = 0000 03FF    (1meg read only) (1meg cacheable)
(32-63)H = 0000 00F0    (reserved)       (cache memory limit)
MSR1002L = 0300 0000    (clock settings)
```

`MSR1001` is 64-bit: low dword = bytes 0-3, high dword = bytes 4-7. So `(32-63)H = 0000 00F0`
gives **`1001h:4` = `F0`**, ECMLR `00`.

Decode confirmed against four registers read independently off the CPU: `9C92`, LMCR `03FF`,
LMROR `0000`, clock `0300` → `1002h:3 = 03`. Every one matches. The only register that differs
between the reference and Win95 is the one that was never written.

## The change

Sixth line in `CPUSET.BAT`, after the LMCR writes and before the cache is switched on:

```
CTCHIP34.EXE IBM486 /1001h:4=&11110000
```

Verify with `CPUSHOW.BAT` — `1001h:4` should read `F0` — then benchmark.

Nothing is permanent: the README states registers reset on power cycle, so a bad value costs a
reboot, not a reinstall.

### Why this is not a DMA-coherency risk

Extending the cacheable region above 1 MB adds no snooping exposure on this machine: ISA DMA
here has a **20-bit page register** (technique 62), so no DMA buffer can exist above 1 MB. Every
DMA buffer already sits inside the cached low-640 KB region today.

⚠ Do not raise CMLR beyond installed RAM. The Inboard aliases its BIOS shadow at `0x5E0000` /
`0x5F0000` ≈ 5.9 MB, above the 5 MB fitted. Caching an alias window invites stale data.

## Correction to a standing lead

The 2026-09-12 handoff suspected CTCHIP's writes were not landing, because `22h`/`23h` alias
onto the 8259 on this XT (technique 75). **They do land** — every value written by `CPUSET.BAT`
read back correctly in a separate, write-free invocation.

That implies the BL3 claims those I/O cycles internally rather than driving them onto the bus.
If it did not, writing an index byte to `22h` (A0 = 0, the PIC's command port) would fire ICW1
and kill interrupts on every boot. **Technique 75's aliasing applies to devices probing chipset
ports, not to the CPU's own configuration registers.**

## What CTCHIP's screen does and does not prove

`CTCHIP34` prints the register after a write. That print is **not** a read-back — proven by
running the same batch under 86Box, where `cpu_read()` returns `0xFF` for these indices and the
writes are dropped entirely, and CTCHIP still displays `92 / CE: Internal Cache: enabled`.

`CPUSHOW.BAT` takes no write argument, so its display *is* a read. Use it, not the post-write
echo, as evidence.

## Not done

- The change is not applied. The CF was in the reader, then the machine was booted to the
  DOS/3.11 image; the Win95 side has not had `CPUSET.BAT` edited.
- No benchmark yet, so the size of the win is unmeasured.
- `1000h:1` bit 4 (`XTOUT`) is set. feipoa recommends `0`. Separate, minor, untested here.
