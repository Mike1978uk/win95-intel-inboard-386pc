# Next session - handoff from 2026-09-26 evening

## Done today

- **T130B A/B on the 5160: IRQ 5 took 23% off a 1 MB copy to the Zip** (18.80 -> 14.48 s).
  Recorded in `docs/t130_mpd_review_2026_09_26.md`; Andrew told on #42. #42 proceeds.
- `tools/timerres/TIMERRES.EXE` built (gcc -m32 + ld i386pe + dlltool, `build.sh`). Measures the
  real Windows tick and its CPU cost. Not yet run on the 5160.
- VC++ 4.2 cloned to `..\MSVC420` (not in the repo); recorded in `resources_and_sources.md`.
- **VPICD IRQ 2 patch written, untested**: `vxd-patches/patch_vpicd_irq2.py` ->
  `VPICD_INBOARD_IRQ2.VXD` (md5 `6c304f8c`). Why and how: `docs/vpicd_irq2_patch_2026_09_26.md`.

## Waiting on the owner

- Settings and timer tests captured as [#45](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/45).
- The 5160 is being rebuilt (CF restored, SB Pro refitted, T130B jumper off IRQ 5).

## Next, in order

1. **Bed test of the VPICD patch** - ask first. `vm_3c509b`, card `irq = 9`, exe
   `86box_3c509b` (carries `ef082884b`). Pre-monolith deploy; prove `VPICD` loaded from the
   file. Control run with stock `VPICD_INBOARD.VXD`. The 09-24 failure: log ends at
   `3C509B: IRQ 9 up`, black screen with cursor, 86Box still running.
2. `TIMERRES` on the 5160, then the polled `T130AB` with and without `TIMERRES HOLD`.
3. The settings issue's A/Bs, one variable each.

## Update, evening 2026-09-26: #7 root-caused and fixed in 86Box

- **#7:** Setup's stall is the XT BIOS floppy motor-start wait falling back to counting DRAM
  refresh on DMA channel 0, which VDMAD traps under Windows. `tools/wait86/WAIT86.COM` (INT 15h
  AH=86h) fixes it: Setup reached the desktop on the first pass, no reboot, first time ever.
  `docs/issue7_setup_stall_2026_09_26.md`. Not yet on the 5160.
- **#42:** the patched VPICD does not cause #7 (stock and patched stall identically without WAIT86).
- Diagnostic 86Box: `86box_3c509b` branch `diag-issue7` (heartbeat, PIC, V86 stack, IRQ and
  INT 13h / INT 15h counters; `INBOARD_HEARTBEAT=1`). Bed `vm_vpicd_irq2`, golden clone per run.
- Next: patch VPICD into the CF's post-monolith `VMM32.VXD` with VxDLIB, proven first on
  `vm_3c509b/card_dhcp.img` (NIC at IRQ 9), then the 5160 with `WAIT86`.

## CF staged for the 5160, 2026-09-26 late (owner backed it up first)

| file on the CF | md5 | what |
|---|---|---|
| `WINDOWS\SYSTEM\VMM32.VXD` | `f4933989` | patched VPICD inside; the exact file tested in `vm_3c509b_irq2` |
| `WINDOWS\SYSTEM\VMM32ORG.VXD` | `b89fd581` | the original, for a one-copy revert |
| `WAIT86.COM` + `AUTOEXEC.BAT` line | `978f7dcc` | `AUTOEXEC.B42` is the original |
| `T130IRQ\` | INF `c052cbf4`, MPD `9cc53279` | `drivers/trantor_t130b/T130-XT-IRQ3.INF` |
| `TIMERRES.EXE` | `db4b2862` | current build |
| `STEPS42.TXT` | | the owner's step sheet: TIMERRES first, then IRQ 9 / IRQ 3 |

The CF's original `VMM32.VXD` is byte-identical to `vm_3c509b/card_dhcp.img`'s, so no re-derivation.

## ▶ NEXT SESSION START - the owner reports back from the 5160

The owner runs `C:\STEPS42.TXT` on the 5160 and brings the CF. Read first:

- `TIMERRES.TXT` - the real Windows tick, and its CPU cost (#45's timer lead).
- `T130P1.TXT` / `T130P2.TXT` - polled copy at the default tick / with `TIMERRES HOLD` (1 ms).
- `T130I3.TXT` - T130B on IRQ 3 with the NIC on IRQ 9. Compare with 18.80 s polled, 14.48 s IRQ 5.
- `BOOTLOG.TXT` - `VPICD` must load (bundled form); the WAIT86 banner shows at boot.
- Whether the network works on IRQ 9, `DIR A:` never hangs, Device Manager is clean.

Then, depending on the result:
- **#42:** write it up; close or report on the issue with Andrew thanked; ship the VPICD route
  (`vxd-patches/patch_vmm32_vpicd_irq2.py` + patcher9x) and `T130-XT-IRQ3.INF` into `dist/`.
- **#7:** the dated status block for the issue body is drafted in this session's transcript
  ("root cause found, fix confirmed in 86Box, not yet tested on the real 5160") - **not posted**;
  post once the 5160 result is in, with the owner's approval. Keep open until then.
- `WAIT86` stays resident permanently (owner asked): any V86 floppy access with the motor off can
  hang the same way while `HSFLOP.PDR` does not load (#18).

Pending, not started:
- `tools/bed_launch.ps1`: warn when a device's config section exists but nothing selects the
  device (86Box drops `net_01_card` on exit under a different build - cost a run tonight).
- Control run with the stock monolith in `vm_3c509b_irq2` (the 09-24 failure is the stand-in).
- 86Box diagnostics live on `86box_3c509b` branch `diag-issue7` (not for upstream).

Tools added tonight, outside the repo: `..\MSVC420` (VC++ 4.2), `..\patcher9x` (built with MinGW;
needs `..\fasm` on PATH to rebuild), `..\fasm` (FASM 1.73.35), and NASM via MSYS2
(`/c/msys64/mingw64/bin/nasm.exe`). All listed in `docs/resources_and_sources.md` except FASM/NASM.
