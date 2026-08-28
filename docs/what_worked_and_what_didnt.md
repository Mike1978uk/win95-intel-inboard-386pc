# What worked, and what didn't

A flat inventory, so a newcomer can see the state of this project without reading the
history. **[FIXES.md](../FIXES.md)** has the downloadable files and md5s; the
**[writeup](windows95_on_inboard386pc_writeup.md)** has the full narrative.

Nothing is listed as working unless it has been run.

---

## What worked

### Upstream, merged into 86Box

| PR | What it fixed |
|---|---|
| [#7626](https://github.com/86Box/86Box/pull/7626) | The Inboard 386/PC hardware model itself — ported from SuperFury's [UniPCemu](https://superfury.itch.io/unipcemu) `hardware/inboard.c` |
| [#7749](https://github.com/86Box/86Box/pull/7749) | POST 101 (machine defaulted to an incompatible 1982 ROM); 386DX ran no POST fix-ups at all; double-throttled memory timing |
| [#7765](https://github.com/86Box/86Box/pull/7765) | `bad extended memory` — the high `0x5F0000` alias must read shadow RAM, not ROM. Now reports **0k** |
| [#7760](https://github.com/86Box/86Box/pull/7760) | `rammap()` NULL deref on a page-table walk through unbacked physical memory — a guest could crash 86Box outright |
| [#7761](https://github.com/86Box/86Box/pull/7761) | The reserved block is 128 KB at a fixed `0x5E0000`–`0x5FFFFF`, not 64 KB derived from RAM size |
| [#7766](https://github.com/86Box/86Box/pull/7766) | POST 1801 on every boot — the Inboard machine must not default to a 5161 expansion unit |
| [#7771](https://github.com/86Box/86Box/pull/7771) | The XT 4-bit DMA page latch. 86Box truncated the page register, but gated it on `dma_at` (= `is286`); an Inboard is an XT board with a 386 on it, so it was handed an 8-bit page register it does not have. Gate on `dma_force_xt \|\| !dma_at` instead. **@andrew-hoffman flagged the gap on issue #3** |

### Upstream, reported and fixed by 86Box

- **[86Box/86Box#7805](https://github.com/86Box/86Box/issues/7805)** — the Machine settings dialog
  rounded RAM down to an invalid size (`5120 → 4096`, `3072 → 2048`) because it snapped with a
  bitmask from zero rather than from the machine's minimum. Reported by @andrew-hoffman, diagnosed
  at source level here, **fixed by OBattler in `9ee5197` and closed 2026-08-28**. We raised no PR.

### Guest-side, confirmed on real hardware

- **`VKD.VXD` rebuilt from the 1995 DDK source.** `VKD_Int_09` tests port `0x64`, which an
  XT has no 8042 to answer, so every keystroke was discarded. First known working Windows
  95 keyboard input on real Inboard hardware.
- **`KEYBOARD.DRV`** — the same port-`0x64` assumption one layer up.
- **`VPICD.VXD`** — neuters the phantom slave 8259 at `0xA0`/`0xA1` (36 sites). An XT has one PIC.
- **`VDMAD.VXD`** — neuters the phantom second DMA controller. Fixes the SB Pro BSOD at
  `VDMAD(01)+00001660`.
- **`MSSBLST.VXD`** — `maxPhys 0xFFF → 0xFF`. Clean Sound Blaster Pro audio.
- **`INBRDPC.SYS` self-test skip** and **`NODIAGS`** — both load-bearing, not conveniences.
- **`ivt68fix`** — real-mode INT 68h vector fix.
- **ATI Mach8 accelerated video** at 1024x768x256, using **Windows 95's own** `ATIM8.DRV` +
  `ATI.VXD`. The driver was never missing; the device node had no resources.
- **`CTCHIP34` from `AUTOEXEC.BAT`** replaces `revto486.sys` entirely — stable, faster, and
  recovers both the 2x multiplier and 640 KB of cacheable memory. Closed issue #9.
- **Networking** — stock Windows 95 3C509B driver, hand-configured IP, browsed frogfind.com.
- **SCSI** — CD, MO and Zip 100 drives via `CONFIG.SYS` / `AUTOEXEC.BAT`.

**Video, sound and networking all run at the same time on the real 5160.**

### Deployed but NOT confirmed — do not treat as working

- **`HSFLOP.PDR`** — `maxPhys 0x1000 → 0xFF`, the same fix as the sound driver. On the card and
  md5-verified, but **`BOOTLOG.TXT` shows Windows never loads it**, so it is inert. `RMM.PDR`
  (Real Mode Mapper) loads instead and `ESDI_506.PDR` does not: the whole storage stack is
  **real-mode BIOS**. Three earlier "invalid" probe runs were chasing a driver that was never
  running.
- **Floppy drives.** A controller is installed and correctly resourced (I/O `03F2-03F5`, IRQ 06,
  DMA 02), but reads still stall part-way through a directory listing. **A: and B: do not work
  yet** — an earlier version of the README said they did, which was wrong.

### Methods that generalise

- **`ForcedConfig` vs `BootConfig` in `SYSTEM.DAT`** tells a real-but-unconfigured device
  node apart from one Windows invented. Fixed the Mach8; explained the phantom PS/2 mouse.
- **Measure, don't argue.** Several confident conclusions here were overturned by somebody
  putting a probe on the wire. The negative list below is mostly that.

---

## What didn't work

Kept deliberately short. Each line is a dead end somebody else does not need to re-walk.

### Wrong from the start

- **1982-dated IBM XT BIOS.** Incompatible with the Inboard — `INBRDPC.SYS` checks a
  signature at `F000:E05B` that those revisions do not carry. **Use a 1986 revision only.**
  This was the real cause of the POST 101 reported against #7626, and it failed *silently*:
  a `bios =` line naming a 1986 ROM was not a valid option elsewhere and was ignored.
- **Two fixed-address patches to defeat that `F000:E05B` check.** Both failed. The value is
  reset-vector-relative runtime data, not a fixed signature. A loadable BIOS shim is parked
  as [#10](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/10).
- **The BIOS-shadow alias derived from `mem_size`** (`0xF0000 + mem_size*1024`). It is fixed
  at `0x5F0000`. The two agree only at `mem_size = 5120`, which is why the bug hid so long.
- **Board sizes other than 1024 / 3072 / 5120.** Those three are the only valid ones; 2688
  is not, despite appearing in a shared config.

### Driver dead ends

- **`ULTRA.DRV`** and **`MACHW3.DRV`** (the Windows 3.1x Mach8 drivers). Tried first, failed.
  Microsoft's own `[ATI8]` section in `MSDISP.INF` was in the box the whole time.
- **Chasing a "missing" Mach8 driver at all.** ATI never wrote one for Windows 95, which is
  what made "none exists" so easy to believe.
- **Treating the PS/2 mouse node as a device to fix.** It was fiction — a `BootConfig` with
  a `DetFunc` and no hardware behind it. There was nothing to configure.

### Measurement and tooling failures — the expensive ones

- **`scripts/vxd_dma_audit.py` reported a false negative**, clearing a genuinely broken
  driver: "`maxPhys` is a register — not statically decidable" → "0 allocations exceed the
  reach." Fixed in `935fe99`.
- **A verification run against a stale binary.** "#7638 fully fixed" was wrong because the
  exe predated the fix. **`stat` the exe against `git log` before trusting any run.**
- **A `gh issue reopen` that reported success and didn't stick.** Re-read state after any
  `gh` write.
- **"The clone runs too fast."** Disproved: the pacing variables are identical. The local
  build simply executes ~3.45× fewer guest instructions/sec because of ~148 debug I/O sites
  in `exec386()`. Trace hooks change timing — use a quiet build for any timing test.
- **A DMA probe that ran too short and too noisy to be valid** — twice. Check the build is
  quiet first: `grep -oE '^\[[a-z0-9_]+\]' stderr.txt | sort | uniq -c`.

### Configuration traps

- **Sergey's floppy ROM at `0xC8000` in the emulator** — hangs POST. Intentionally not loaded.
- **The 5161 expansion unit attached by default** — POST 1801 on every boot. It had been
  "ruled out" in July by a test that never actually ran.
- **`bios =` with no valid entry** black-screens rather than warning.
- **The 3C509B model in this fork is an init-only stub**, not functional. Networking works
  on real hardware, not in emulation.
- **Replacing a VxD after Setup has combined it into `VMM32.VXD`.** Silently ignored. Use a
  [pre-monolith image](../vxd-patches/README.md).
- **Patch scripts that report nothing.** They can no-op silently — confirm `Patched: N`, N > 0.

---

## Still to upstream

**Nothing.** The 4-bit DMA page latch was the last item and merged 2026-08-25 as
[86Box/86Box#7771](https://github.com/86Box/86Box/pull/7771).

The guest-side patches are Windows files, not emulator code, and stay hosted here, in
[FIXES.md](../FIXES.md).

## Worked: reading `IOS.LOG` before theorising (2026-08-28)

Windows' own I/O Supervisor log named three blockers nobody had suspected, the decisive one being
`Punting miniports because of unknown ASPI driver SD120PPD` — IOS refusing to load miniport
drivers **as a class**. `SD120PPD.SYS` is a parallel-port LS-120 driver, unrelated to the disk or
the SCSI chain.

Commenting it out (plus `ASPIHDRM.SYS`) and adding `inbrdpc.sys` to `IOS.INI`'s `[SafeList]` took
units running on real-mode drivers from **six to one**. The survivor is the XT-IDE boot disk, which
`rmm.pdr` still maps and which no 32-bit driver will ever claim.

This did not fix the disk. It made issues #18 and #19 testable at all — before it, any 32-bit
storage driver would have failed for a reason unrelated to itself. Measurement in
[#17](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/17).

**Not separated:** two variables changed in one boot. The punt line is unambiguously the ASPI
removal; the whitelist's own effect is untested. **Caveat:** `MODISK2.SYS` claimed no units that
boot, so its "Unsafe/Monolithic" flags may only have been dormant for lack of MO media.
