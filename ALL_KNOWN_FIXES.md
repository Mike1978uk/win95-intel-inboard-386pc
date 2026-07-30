---
name: all_known_fixes
description: CHRONOLOGICAL list of all documented fixes - apply in order shown
metadata: 
  node_type: memory
  type: project
  date: 2026-07-30
  source: COMrade_Latest investigation (INBOARD_86BOX_PORT_PLAN.md)
  originSessionId: cab325c2-6696-42ae-9970-be4f59b70619
  modified: 2026-07-30T22:29:14.391Z
---

# ALL FIXES - CHRONOLOGICAL APPLICATION ORDER

## PHASE 1: Core Timing Fixes (2026-07-24 to 2026-07-26)

### Fix 1a: reg_op_waitstates (LOOP Instruction Compensation)
**Date**: 2026-07-25
**File**: `src/device/inboard386.c` lines 257-272
**Applied by**: `inboard386_apply_reg_op_waitstates()`
**Purpose**: PIC IMR test is pure LOOP with no bus access - needs cycle penalty on LOOP itself
**Call sites**: lines 558, 572 in inboard386.c
**Status**: ✅ ALREADY IN CODE
**Verify**: `grep -n "inboard386_apply_reg_op_waitstates" src/device/inboard386.c`

### Fix 1b: force_xt_imr_timing (PIC Hardware-Class Detection)
**Date**: 2026-07-25
**File**: `src/device/inboard386.c` line 691
**Code**: `pic_set_force_xt_imr_timing(1);`
**Purpose**: IMR write-visibility gates on XT hardware (not CPU class). Inboard is XT with 386+ CPU
**Status**: ✅ ALREADY IN CODE
**Verify**: `grep -n "pic_set_force_xt_imr_timing" src/device/inboard386.c`

### Fix 1c: dma_force_xt (DMA Channel 0 Refresh Timing)
**Date**: 2026-07-25
**File**: `src/device/inboard386.c` line 708
**Code**: `dma_set_force_xt(1);`
**Purpose**: Channel 0 refresh gates on XT hardware (not CPU class)
**Status**: ✅ ALREADY IN CODE
**Verify**: `grep -n "dma_set_force_xt" src/device/inboard386.c`

## PHASE 2: CPU Prefetch & Memory Timing (2026-07-24 Evening)

### Fix 2a: io_waitstates Mechanism (I/O Port Cycle Padding)
**Date**: 2026-07-24
**Files**: 
  - `src/device/inboard386.c` - `inboard386_apply_io_waitstates()` function
  - `src/cpu/x86_ops_io.h` - wired into IN/OUT opcodes
**Purpose**: Real ISA bus has fixed ~4.77MHz clock independent of CPU. IN/OUT instructions must pace from bus speed.
**Formula**: `io_waitstates = (cpu_busspeed / 4772728.0) * baseline`
**Called from**: `inboard386_speed_changed()` device callback
**Status**: ✅ ALREADY IN CODE (thoroughly tested, confirmed working but insufficient for `101` fix alone)
**Verify**: `grep -n "io_waitstates" src/device/inboard386.c` and `grep -n "io_waitstates" src/cpu/x86_ops_io.h`

### Fix 2b: cpu_rom_prefetch_cycles Override (ROM Fetch Timing)
**Date**: 2026-07-24  
**File**: `src/device/inboard386.c` - `inboard386_apply_rom_prefetch()` function
**Purpose**: During cold POST (before ROM shadow), BIOS fetches from real slow ROM chip. Must scale ROM prefetch timing.
**When active**: `rom_shadow_enabled = 0` (reset default)
**Formula**: `cpu_rom_prefetch_cycles = (int) ((8.0 * ratio) + 0.5)` where `ratio = cpu_busspeed / 4772728.0`
**Called from**: `inboard386_write_670()`, `inboard386_reset()`, `inboard386_speed_changed()`
**Status**: ✅ ALREADY IN CODE (tested thoroughly, confirmed reachable on correct CPU, but insufficient at any tested magnitude)
**Verify**: `grep -n "inboard386_apply_rom_prefetch" src/device/inboard386.c`

### Fix 2c: General Memory Timing Override (mem_read/write_cycles)
**Date**: 2026-07-24
**File**: `src/device/inboard386.c` - `inboard386_apply_mem_timing()` function  
**Purpose**: Overrides `cpu_prefetch_cycles`, `cpu_mem_prefetch_cycles`, `cpu_cycles_read[_l]`, `cpu_cycles_write[_l]`
**When active**: `rom_shadow_enabled = 0` (unaccelerated cold-boot state)
**Formula**: Scale by `cpu_busspeed / 4772728.0` ratio against CPU table baseline
**Scope**: All memory access (both ROM and RAM) until shadow RAM enabled
**Called from**: Same places as rom_prefetch (speed_changed, reset, write_670)
**Status**: ✅ ALREADY IN CODE (tested, works, but insufficient for `101` at any magnitude)
**Verify**: `grep -n "inboard386_apply_mem_timing\|cpu_cycles_read\|cpu_mem_prefetch" src/device/inboard386.c`

### Fix 2d: ISA Speed Calibration (isa_cycles Scaling)
**Date**: 2026-07-24
**File**: `src/device/inboard386.c` - `inboard386_apply_isa_speed()` function
**Purpose**: Decouples ISA bus speed from CPU clock (ISA always runs at ~4.77MHz regardless of CPU speed)
**Method**: Scales `cpu_s->atclk_div` multiplicatively to real ISA speed reference
**Status**: ✅ ALREADY IN CODE (tested, confirmed inert for IBM486BL - dead code path, but harmless)
**Verify**: `grep -n "inboard386_apply_isa_speed" src/device/inboard386.c`

## PHASE 3: IRQ Collision Fixes (2026-07-27)

### Fix 3: Three IRQ-Collision Fixes (Fixes POST 101 Error)
**Date**: 2026-07-27
**Reference**: Section "## 2026-07-27: five-tool CPU-speed reconfirmation, two real Inboard keyboard-driver bugs found and fixed..."
**What it fixes**: POST 101 error (keyboard/interrupt handling collision)
**Files affected**: `src/device/inboard386.c` (keyboard-related fixes) and possibly `src/pic.c` or interrupt handlers
**Status**: ✅ NEEDS VERIFICATION - Document mentions these fixes but doesn't specify exact code changes
**Next action**: Search INBOARD_86BOX_PORT_PLAN.md section 2026-07-27 for exact fixes

## PHASE 4: Configuration Fixes (2026-07-30 This Session)

### Fix 4a: Sound Blaster dma16 Disable
**Date**: 2026-07-30 (commit 20ef8b4)
**File**: `vm_win311/86box.cfg` line 29
**Value**: `dma16 = 4` (ISAPNP_DMA_DISABLED)
**Purpose**: Card uses DMA 1 only (8-bit), not 16-bit DMA
**Status**: ✅ ALREADY APPLIED
**Verify**: Check current vm_win311/86box.cfg

### Fix 4b: CPU Speed to Real Hardware Value
**Date**: 2026-07-30
**File**: `vm_win311/86box.cfg` line 7
**Value**: `cpu_speed = 83500000` (83.5 MHz)
**Purpose**: Real hardware: 40 MHz crystal × 3 multiplier
**Status**: ⚠️ CURRENTLY BROKEN (reverting from 100 MHz caused black screen - needs investigation)
**Note**: 100 MHz was temporary testing ONLY. Correct value is 83.5 MHz per CHECKCPU verification.

## CRITICAL ISSUE: Video Black Screen at 83.5 MHz

**Problem**: ROM prefetch calculation is broken at 83.5 MHz
**Hypothesis**: ROM prefetch baseline of 8 may be wrong for this speed
**Possible fix**: Change ROM prefetch baseline from 8 to 2 (match memory prefetch baseline)
**File to modify**: `src/device/inboard386.c` line 392

## DO NOT RE-TEST THESE (THOROUGHLY EXHAUSTED)

1. ❌ `enable_5161` - proven doesn't cause `1801` 
2. ❌ `cpu_waitstates` alone - dead path for IBM486BL (gated to CPU_286-386DX)
3. ❌ `io_waitstates` magnitude tuning - 3 magnitudes tested (calculated, 10x, 5000), all fail
4. ❌ `cpu_rom_prefetch_cycles` magnitude tuning - 3 magnitudes tested (8, 60, 500x), all fail except 500x changes failure point
5. ❌ `mem_timing` magnitude tuning - tested at calculated ratio and 10x, both fail
6. ❌ PIT tick rate - verified 1.193182 MHz fixed (not CPU-dependent)
7. ❌ Timer catch-up - verified while-loop catches up correctly
8. ❌ `isa_cycles`/`pit_fast.c` - confirmed dead code path for IBM486BL (not classified as `is486`)

## Root Cause FOUND (Not Yet Fixed)

Channel 1 PIT is mid-countdown from earlier BIOS setup (~65536 ticks ≈ 55ms at 1.193MHz). Self-test needs this pre-existing countdown to COMPLETE before its own reload takes effect. With 8088, loop is slow enough that CX budget completes within that time. With accelerated CPU, cycle-padding doesn't ensure total elapsed real time reaches ~55ms gap.

**Resolution**: Either (a) debugger-based analysis to find exact CX budget and calculate required per-iteration timing, or (b) pragmatic ROM patch adjusting CX threshold.

---

## APPLICATION CHECKLIST

- [ ] Verify Fix 1a (reg_op_waitstates) in code
- [ ] Verify Fix 1b (force_xt_imr_timing) in code
- [ ] Verify Fix 1c (dma_force_xt) in code
- [ ] Verify Fix 2a (io_waitstates) in code
- [ ] Verify Fix 2b (cpu_rom_prefetch_cycles) in code
- [ ] Verify Fix 2c (mem_timing) in code
- [ ] Verify Fix 2d (isa_cycles) in code
- [ ] Find and apply Fix 3 (IRQ collision fixes) from 2026-07-27 section
- [ ] Apply ROM prefetch baseline fix (change 8 to 2) for video black screen
- [ ] Rebuild
- [ ] Test
- [ ] Commit all verified fixes
