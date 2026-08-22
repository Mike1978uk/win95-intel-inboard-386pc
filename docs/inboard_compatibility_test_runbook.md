# Inboard 386/PC — compatibility test runbook (mechanical, safe for a cheap model)

**Status of the hard work: DONE.** Root cause found and fixed (see below). What remains is a
repetitive, mechanical matrix run. This runbook is written so a lower-cost model (or a person) can
execute it without re-deriving anything. **Do not re-investigate the root cause. Do not change any
source code.** Only change config values, run, and record the result.

## What was fixed (context, do not redo)
The `ibmxt_inboard386` machine shared `ibmxt_config`, whose default BIOS is the **1982** IBM XT ROM.
That ROM is **incompatible** with the Inboard (INBRDPC.SYS's ROM-signature check at `F000:E05B`),
which produced the **POST 101** reported on 86Box PR #7626. Selecting `bios = ibm5160_050986` in a
config was *silently ignored* because that entry didn't exist in the shared list.

Fix: `ibmxt_inboard386_device` now has its own `ibmxt_inboard386_config[]` listing **only** the two
compatible 1986 ROMs, defaulting to `ibm5160_050986`. The incompatible ROMs are now unselectable.

## Build / run environment (exact, copy-paste)
- Clone tree: `C:\Users\lycet\AppData\Local\Temp\claude\C--Users-lycet-RiderProjects-86Box-Inboard\185e77f2-1aae-46ef-b9a3-bc1591b3012c\scratchpad\upstream_test`
- Build dir: `<clone>\build\test` → `ninja`
- Run dir:   `<clone>\rundir`  (binary `86Box_clone_clean.exe`)
- **Always** prepend MinGW to PATH in the SAME shell that launches, or you get a Qt5 DLL error:
  `$env:Path = 'C:\msys64\mingw64\bin;' + $env:Path`
- **Always** pass `-WorkingDirectory <rundir>` to `Start-Process`.

## Critical rules
1. **One VM at a time.** All test configs share the same Windows 95 disk image
   (`vm_win95_osr1\osr1_XT_customvkd_test.img`). Running two concurrently risks corrupting it.
   Always `Stop-Process -Force` the previous one and wait ~2s before starting the next.
2. **Each config needs its own `uuid`**, or 86Box conflates saved per-VM state.
3. Allow **~95 seconds** per boot before screenshotting — that has been enough to reach the desktop
   in every passing run so far.
4. Forced kills mid-boot set Windows 95's "dirty shutdown" flag, so the *next* boot may show the
   Startup Menu with a ~13s countdown that **defaults to Safe Mode** (Safe Mode is broken on this
   hardware — it unloads INBRDPC.SYS). If a run shows that menu, select **1 (Normal)** and re-run.

## How to make a variant config
Start from `rundir\86box_clone_win95_test.cfg`. Change ONE thing at a time:
- CPU: `cpu_family = <name>` and **delete** the `cpu_speed` / `cpu_multi` lines (they're 486BL3-
  specific; deleting lets 86Box pick valid defaults for the chosen family).
- Video: `gfxcard = <name>`
- BIOS: `bios = <name>` under the `[IBM XT (Inboard 386/PC)]` section.
- Always change `uuid` to something unique.

## Pass criteria
- **PASS** = reaches the Windows 95 desktop (icons: My Computer / Network Neighborhood / Recycle Bin).
- Also acceptable as PASS: the "Windows Mouse Support" dialog (expected — configs use
  `mouse_type = none`).
- **FAIL** = POST error `101` / `301` on screen, a hang with no progress, or never leaving the splash.
- Record the exact screen text for any failure.

## The matrix

### Results so far
| # | Variant | Setting | Result |
|---|---------|---------|--------|
| 1 | BIOS 09MAY86 + Mach8 (baseline) | `bios=ibm5160_050986`, `gfxcard=mach8_vga_isa` | ✅ **PASS** — desktop |
| 2 | BIOS 10JAN86 + Mach8 | `bios=ibm5160_011086` | ✅ **PASS** — desktop |
| 3 | **Default BIOS** (no `bios=` line) | *(PR #7626 reporter's exact scenario)* | ✅ **PASS** — desktop |
| 4 | Generic VGA | `gfxcard=vga` | ❌ **FAIL** — keyboard POST error; boot **halts awaiting F1**. Pressing F1 continues to the Win95 GUI, but it does not boot unattended, so this counts as a fail. |

| 5 | Intel 386DX | `cpu_family=i386dx` (no `cpu_speed`/`cpu_multi`) | ❌ **FAIL** — beep (POST passed) then black screen |
| 6 | AMD 386SX | `cpu_family=am386sx` (no `cpu_speed`/`cpu_multi`) | ❌ **FAIL** — beep then black screen |

**⚠️ Tests #5/#6 are NOT a clean CPU comparison — re-run before trusting them.** Those configs had
`cpu_speed`/`cpu_multi` deleted, so 86Box picks the family's *default* bus speed. The Inboard device
derives its whole wait-state/mem-timing compensation from `cpu_busspeed`
(`inboard386_apply_io_waitstates()` / `inboard386_apply_mem_timing()`), and below ~4.77 MHz it takes
an entirely different (unscaled) branch. So #5/#6 changed CPU **and** speed simultaneously. To test
CPU family properly, set an explicit `cpu_speed` for the chosen family and keep it comparable to the
baseline rather than deleting the line. POST passing (the beep) but video going black afterwards
points at the OS-boot/video-mode stage, not at CPU instruction emulation.

**Finding #4 matters and needs following up before the PR:** the baseline Mach8 config boots clean,
but swapping to the generic `vga` card produces a keyboard POST error requiring F1. That points at
an interaction between the video card choice and the keyboard self-test region this project already
patches (`F000:E362-E3AC` / `E3AD` / `E3C6`), not at the video card itself. Two cheap next checks:
(a) does the same failure occur on the *other* VGA cards (#11-13 below)? If only `vga` fails, it's
card-specific; if all non-Mach8 cards fail, the self-test fix is Mach8-timing-dependent and needs
widening. (b) Re-run #4 and note whether the on-screen error is `301` or `101`.

### Still to run
CPU variants (keep `gfxcard=mach8_vga_isa`, `bios=ibm5160_050986`; delete `cpu_speed`/`cpu_multi`):
| # | `cpu_family` | Notes |
|---|--------------|-------|
| 5 | `i386dx` | Intel 386DX — closest to the real card |
| 6 | `am386sx` | AMD 386SX |
| 7 | `am386dx` | AMD 386DX |
| 8 | `cx486dlc` | Cyrix 486DLC |
| 9 | `ibm486bl2` | IBM Blue Lightning 2x |
| 10 | `ibm486slc3` | IBM SLC3 |

Video variants (keep `cpu_family=ibm486bl3`, `bios=ibm5160_050986`):
| # | `gfxcard` | Notes |
|---|-----------|-------|
| 11 | `et4000ax` | Tseng ET4000 ISA |
| 12 | `ati28800` | ATI VGA Wonder |
| 13 | `tvga8900d` | Trident |

**Do not test CGA/EGA** — Windows 95 requires VGA-class video. (CGA is still valid for *DOS-level*
POST checks, but not for this desktop-boot matrix.)

TI 486SXL is **not available** for this machine — its CPU packages are
`CPU_PKG_386DX | CPU_PKG_486BL | CPU_PKG_486DLC` and no TI part is listed under them. Skip it.

## Record results here
Append a row per run: variant, pass/fail, and for failures the exact on-screen text. When the matrix
is complete, the passing set becomes the "known-working configurations" list for the PR description.

## CPU-family limitation — FOUND, not yet solved (2026-08-22)

**Plain Intel/AMD 386DX/386SX do not boot** (POST beeps, black screen, never reaches the RAM count).
Structural cause, confirmed by reading `cpu.c:1885`:

```c
if ((cpu_s->cpu_type == CPU_IBM486SLC) || (cpu_s->cpu_type == CPU_IBM486BL) ||
    cpu_iscyrix || (cpu_s->cpu_type > CPU_486DLC) || cpu_override_interpreter) {
    cpu_exec = exec386;        /* ALL of this project's CPU-core fixes live here */
} else
    cpu_exec = exec386_2386;   /* plain 386DX/386SX -> NONE of the fixes exist */
```

Every Inboard fix (Mach8 option-ROM PIT delay loop, `E362-E3AC` IRQ1 self-test, `E507` DMA refresh,
`patchint68`, the C000 wait-state exemption) is implemented **only inside `exec386()`**. CPU families
routed to `exec386_2386()` therefore run with no fixes at all and hang in the Mach8 option ROM's PIT
delay loop — which is *before* the RAM count, exactly matching the observed symptom. **This is not a
speed problem**; the fixes simply never execute.

**Working CPU families** (routed to `exec386`): `ibm486bl3` (validated to desktop), `ibm486bl2`,
`ibm486slc3`, `cx486dlc` (Cyrix; via `cpu_iscyrix`).
**Non-working**: `i386dx`, `am386dx`, `am386sx`.

**Attempted and did NOT work**: setting `cpu_override_interpreter = 1` in the config's `[General]`
section. Either the flag is consulted before config load, or it is not sufficient on its own — not
diagnosed further (session budget). Do NOT assume this is a one-line fix; verify where `cpu_set()`
runs relative to config application before trying again.

**Two possible real fixes for a future session:**
1. Have the Inboard machine/device force `cpu_override_interpreter = 1` early enough that
   `cpu_set()` (machine.c:95) sees it — needs the init-ordering question above answered first.
2. Factor the fix block out of `exec386()` into a shared helper and call it from `exec386_2386()`
   too. More invasive but ordering-independent, and arguably the correct structure for upstream.

**Impact on the PR**: this is a pre-existing limitation, NOT a regression introduced by the BIOS fix.
State the validated CPU list explicitly in the PR description rather than blocking on it. Note the
historically-authentic CPU for this card is a 386DX/16 — so this is worth fixing eventually for
fidelity, as a separate change.

### Exact location for the CPU-compatibility fix (found 2026-08-22, implementation not started)
- `exec386()`      → `src/cpu/386_dynarec.c`  (contains ALL Inboard fixes today)
- `exec386_2386()` → **`src/cpu/386.c:225`**   (separate file; currently has NONE of them)

Planned fix (option 2, ordering-independent): extract the Inboard fix block out of `exec386()` into
one shared helper — e.g. `void inboard_post_fixups(void);` — and call it from the top of BOTH
interpreters' per-instruction loops. Put the helper somewhere both can include (a small new header,
or `src/device/inboard386.c` with a prototype in `86box/inboard386.h`) so upstream sees one
implementation, not a copy-paste. The block to move is the ~160-line region in `386_dynarec.c`
covering: the C000 wait-state exemption, the Mach8 PIT delay-loop fix (`0x7B37/0x7B23/0x7B16`), the
`E362-E3AC` IRQ1 self-test + range safety net, the `E507` DMA-refresh force, and `patchint68`.
