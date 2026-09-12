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

---

# BENCHMARKED 2026-09-12 — the "Not done" above is now done

Both SIV runs read by this project, not summarised by anyone else. The post-fix run is
`SIV_MIKE.txt` on the CF (`siv_v5.88_beta_09/`, 2026-09-12 17:36). The pre-fix run was
recovered from the CF image `win95_postsiv.img` at byte offsets `100684602` and `1469371625` —
two separate captures, which agree.

## SIV's own identification of the CPU changed

| | before | after |
|---|---|---|
| CPU string | `Generic 486 DX` | `Generic 486 DX2 66MHz` |
| L1 it inferred | **8KB** | **16KB** |
| clock it inferred | not resolved | 65.7 MHz, FSB 32.9 MHz x2 |

SIV has **no CPUID and no TSC** on this part, so it derives all three from the timing curve.
Before the fix that curve was flat and it guessed wrong. After, it resolves the BL3's true
16 KB L1. That is independent corroboration from a tool that knows nothing about CMLR.

## Dhrystone / Whetstone

| run | integer | int time | float | float time | elapsed |
|---|---|---|---|---|---|
| D+D,W+W **before** | 2 | 3.79 | 2 | 52.56 | 59.1 |
| D+D,W+W **after** | **13** | 2.05 | 3 | 31.56 | **36.1** |
| D+W,W+D **before** | 2 | 3.77 | 1 | 69.92 | 76.2 |
| D+W,W+D **after** | **15** | 2.49 | 3 | 31.02 | **34.7** |

**Integer 2 -> 13-15.** Floating point 2 -> 3, and float time roughly halves (52.6 -> 31.6 s).
The smaller FPU gain is expected: Whetstone is bound by the arithmetic unit, not by memory.

## The cache latency walk — red-ray's actual question

Times in ms. His observation was that 16 KB and 24 KB timed the same.

| working set | before | after | |
|---|---|---|---|
| 4KB | 1.446 | **0.237** | 6.1x faster |
| 6KB | 2.513 | **0.237** | 10.6x |
| 8KB | 2.529 | **0.218** | 11.6x |
| 12KB | 2.819 | **0.228** | 12.4x |
| 16KB | 3.010 | **0.325** | 9.3x |
| 24KB | 3.193 | 7.777 | **2.4x slower** |
| 32KB | 3.357 | 7.756 | 2.3x slower |
| 128KB | 3.738 | 8.114 | 2.2x slower |
| 768KB | (~3.9) | 8.300 | |

**Before: no knee.** 16 KB to 24 KB is 3.010 -> 3.193, a 6% step — which is what red-ray was
looking at when he said the cache was not working. Everything was uncached, so working set size
barely mattered.

**After: a 24x cliff at exactly the L1 boundary.** 0.325 -> 7.777 between 16 KB and 24 KB. That
is the shape a working cache makes, and it appears at the size the hardware actually has.

## ⚠ The finding nobody's summary mentioned: outside the cache, it got SLOWER

Every working set above 16 KB is **~2.3x slower than before the fix.** This is real, it is in
both tables, and it must not be smoothed over.

**Most likely cause, untested:** the region is now cacheable, so a miss fetches a whole
**16-byte line** (SIV reports `4-way 16-byte`) instead of just the bytes asked for. A latency
walk strides deliberately to defeat reuse, so it pays for 16 bytes and uses a fraction of them
— roughly the 4x that would produce the observed 2.3x after overlap.

**It does not cost the ISA bus anything.** The walk runs in the Inboard's own 5 MB, which is
local to the card (the one fact at the top of `bus_optimisation_plan.md`). The extra line-fill
traffic stays on the card's local bus and no other device on the ISA bus pays for it.

**Is the fix still net-positive? Yes, clearly.** Dhrystone nearly halves elapsed time, the owner
reports the desktop feels more responsive, and real code has the spatial locality that a latency
walk is built to destroy. But the worst case is now worse than it was, and the honest statement
is: **cached working sets got ~10x faster, cache-hostile ones got ~2.3x slower.**

**Open, and cheap to settle:** confirm the line-fill explanation before offering it as fact. If
it holds, it is a general result for this machine — a scattered access pattern over a large
buffer is now more expensive than it was, which is worth knowing before anyone sizes a driver
buffer above 16 KB.

## What is still not measured

- **The core clock.** SIV's 65.7 MHz is inferred from timing, not read from the part, and this
  project has **never** measured the clock independently. Do not repeat any "actual is X MHz"
  figure without a measurement. See F1 in `bus_optimisation_plan.md`.
- **Disk throughput.** Expect no change: that is bus-bound at ~3.90 us per I/O access and the
  CPU cache cannot touch it. Worth one run purely to confirm the expectation.
- **`XTOUT` (C1)**, still set to 1. Now has a post-CMLR baseline to be measured against.
