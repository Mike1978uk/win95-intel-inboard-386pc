# Complete Windows 95 boot fix inventory (XT + Inboard 386/PC), consolidated 2026-08-22

Moved out of `.claude/skills/inboard-hw-debug/SKILL.md` on 2026-09-11. It is a
reference checklist rather than a diagnostic method, so it does not need to be in
context while debugging something unrelated - but it is still THE canonical list and
**must be updated here, not re-derived**.


This is THE canonical list of everything required to get Windows 95 (OSR1) booting to a working
desktop on the `ibmxt_inboard386` machine profile in 86Box. It exists because this exact list was
scattered across ~15 dated memory files, which cost real time to re-derive when porting fixes into
a fresh upstream clone for a PR. **Read this before re-diagnosing a Win95 boot hang from scratch —
check whether the symptom matches something already solved here first.** Update this table itself
(don't just add another dated memory file) whenever a new required fix is found — this list should
always be the current source of truth, not an archive.

Two categories matter, because they interact with a fresh clone/PR differently:
- **Emulator/code fixes** — live in 86Box's own C source (`386_dynarec.c` etc.). These must be
  manually re-applied to any fresh clone of upstream 86Box; they will NOT be present just because
  you have the right disk image.
- **Disk-image fixes** — patches baked into the Windows 95 `.img` file itself (VxD/DRV binary
  patches, a flipped byte in `INBRDPC.SYS`). These travel WITH the disk image regardless of which
  86Box build boots it — if two builds share the same `.img` file, both already have these; they
  are never a source of difference between builds and never need "porting" into emulator code.

### Emulator/code fixes (must exist in `386_dynarec.c` unless noted)
1. **Base PIC-IMR/DMA-refresh timing fix** — `dma_force_xt`/`pic_set_force_xt_imr_timing` wiring
   in `device/inboard386.c`'s init. Already part of upstream 86Box mainline since PR #7626 merged —
   a fresh clone has this for free, nothing to port.
2. **`E362-E3AC` IRQ1-suppression self-test fix**, with the 2026-08-22 correction: exit on
   `E3AE`/`E38E`/**`E3AD`** (not just the first two), and `in_negative_test` set ONLY on `E3AE`. See
   the "RESOLVED, 2026-08-22" section below for the full story — this is the fix that closed `301`.
3. **`E507` DMA-refresh status-flag force** (`AL |= 0x01` at `F000:E507`).
4. **Mach8 PIT-readback delay-loop fix** — force the elapsed-ticks register past target at
   `C000:0x7B37`/`0x7B23`/`0x7B16` (three known ROM-revision addresses; add a new one here if a
   fourth ROM revision is ever encountered, following the same live-CS:PC-trace method).
5. **Mach8 option-ROM waitstate exemption** (dated 2026-07-26 — easy to miss, it's NOT near the fix
   above in the file's history even though both are Mach8-related): zero out
   `io_waitstates`/`reg_op_waitstates`/`cpu_prefetch_cycles`/`cpu_mem_prefetch_cycles`/
   `cpu_rom_prefetch_cycles`/`cpu_cycles_read(_l)`/`cpu_cycles_write(_l)`/`isa_cycles` for the
   duration of `CS==0xC000` execution, restoring the real values the instant CS leaves 0xC000.
   Without this, the Mach8 option ROM's self-test stretches from real-hardware-instant into
   65-100+ real seconds, because this project's system-BIOS waitstate inflation gets wrongly
   applied to the option ROM's own hundreds of I/O operations too.
6. **Segment-650B / `INT 68h` wild-jump fix** (`patchint68`, dated 2026-08-04; **revised
   2026-08-23**): the first time `CS==0x0EAF`, point the `INT 68h` IVT vector (`0x1A0`) at the
   BIOS's own IRET at **`F000:FF53`** (write `53 FF 00 F0`). **No stub is injected.** The original
   version wrote a `0xCF` byte into INT F0h's own vector slot (physical `0x3C0`) and pointed there;
   Michal Necasek asked in review of PR #7749 why it didn't just use the BIOS's existing IRET, and
   he was right. Verified: byte at file offset `0x7F53` of the U18/F800 chip is `0xCF` in **both**
   1986 revisions (09MAY86 and 10JAN86), the only BIOSes this machine accepts. Strictly better - no
   injected code, and no assumption that INT F0h is unused this early in boot. **The real-hardware
   twin `ivt68fix/IVT68FIX.ASM` must be kept in step** (now 20 bytes / 4 writes, was 26 / 5). Without this, VMM32's real-mode VxD loader's uninitialized `INT 68h` call walks
   off into the raw IVT as code and eventually wild-jumps into segment `650B` with bogus calling
   context. **This one is easy to forget when porting** — it belongs to a completely different
   investigation (Win95 keyboard/boot, not the 101/301 POST-error family), so it's not in the same
   part of the file as the other fixes and won't turn up if you only grep for "301" or "101".
7. **`kbc_xt.c` `blockedtimeout` self-heal** — bounded auto-clear of `kbd->blocked` (~50 poll ticks)
   if nothing ever acknowledges the XT keyboard strobe via port 0x61 bit 7. Needed because some
   guest keyboard handlers (written assuming AT-style hardware) never perform this XT-only
   acknowledgment.
8. **`cpu_table.c` 83.5 MHz CPU speed table entry** (`486BL3`/`83.5`) and **`m_xt.c` LPT device**
   addition — both omitted from PR #7626's file list, straightforward to re-port (small, self-
   contained diffs).
9. **`snow_enabled = 0`** in the video device's config section — NOT a code fix, a config
   recommendation, only relevant for plain-CGA setups (Mach8/VGA setups don't need it). 86Box's CGA
   snow simulation is accurate on its own; it just desyncs with this project's timing overrides.

### Disk-image fixes (already baked into any `osr1_XT_customvkd_test.img`-family file — never port these into emulator code)
1. **`INBRDPC.SYS` self-test-skip patch** (`vxd-patches/osr1/INBRDPC_selftest_skip.SYS`, file offset
   `0x6BA` flipped `3C00`→`FFFF`) — without this, boot stalls at "Please wait while Setup updates
   your configuration files..." in a redundant loop in the `0206:06xx` self-test wrapper. WITH it,
   that same region is visited only briefly (~1 real second, a single legitimate ~2000-iteration
   A20-toggle burst) and boot proceeds to the Startup Menu / GUI.
2. **Custom `VKD.VXD`** built from Microsoft's 1995 Windows 95 DDK sample source — removes
   `VKD_Int_09`'s check of port 0x64's AT-only "data available" bit before reading port 0x60 (the
   Inboard has no real 8042, so that bit never sets, silently discarding every keystroke otherwise).
3. **`KEYBOARD.DRV`** patch (file offset `0xf14`) — an independent, separate port-0x64 check inside
   Windows' own keyboard driver, needed alongside the VKD fix (Windows 3.11 needed both `IBKBD.DRV`
   and `IBVKD.386` for the same underlying reason — same pattern here).
4. **Headless keystroke-injection capture gate** — not a boot requirement per se, but needed for
   *unattended/scripted* testing specifically: see Technique 9 (`inject_key.txt`) for how to send
   real keystrokes into a running VM without a physical keyboard.

### Known OPEN issue — do not confuse this with "the boot recipe isn't fully known"
There is a genuinely unresolved, separate problem, first found 2026-08-22 (this project's OWN fork
`local` vs. a fresh upstream clone, NOT anything to do with Windows 95 specifically): the two builds
execute a different number of raw CPU instructions through an early BIOS timing-calibration loop on
byte-identical ROM, traced to a register (`AX`) already differing only ~30 instructions after CPU
reset, for a reason never found. This is NOT part of the boot recipe above — every item in that list
is fully solved and, as of 2026-08-22, confirmed present in both `local` and a from-scratch upstream
clone. This open issue is a CPU-core-timing mystery that can make ANY sufficiently timing-sensitive
self-test loop (not just the ones already fixed above) take a different number of iterations between
the two builds — up to and including looking like a hang when it's actually just taking far longer
than expected. Concrete, never-executed next step: dump `pit_const` (the actual cycle-to-tick
conversion value, `dev->pit_const` in `pit.c`) for both PIT counters, at the same early instruction
index, in both builds — if it differs despite identical `cpu_busspeed`/waitstate inputs, that pins
the mechanism.

### For a future OSR2 attempt
The disk-image fixes above (self-test-skip, VKD.VXD, KEYBOARD.DRV) were derived specifically against
OSR1's files/offsets — OSR2 will very likely need its own equivalent patches at different file
offsets (OSR2 uses a different `VKD.VXD`/`KEYBOARD.DRV` build), even though the underlying root
causes (missing port-0x64 "data available" bit, uninitialized `INT 68h` vector, INBRDPC.SYS self-
test redundancy) should be identical in kind. The emulator/code fixes above are OS-version-
independent and should apply unchanged. Technique 35 (read the real DDK source rather than guessing
at binary offsets) is the proven path for re-deriving the VKD/KEYBOARD.DRV patches against OSR2's
actual files.
