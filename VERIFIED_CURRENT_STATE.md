---
name: verified_current_state
description: "VERIFIED state of 86Box-Inboard - what works, what's in code, what needs fixing"
metadata: 
  node_type: memory
  type: project
  date: 2026-07-30
  status: Ready for testing
  originSessionId: cab325c2-6696-42ae-9970-be4f59b70619
  modified: 2026-07-30T22:30:30.220Z
---

# VERIFIED CURRENT STATE (2026-07-30)

## ✅ EVERYTHING IN CODE (VERIFIED)

### Timing Fixes - ALL PRESENT & CONFIRMED
- ✅ reg_op_waitstates (LOOP instruction) - line 257-272, inboard386.c
- ✅ force_xt_imr_timing (PIC XT-mode forcing) - line 691, inboard386.c
- ✅ dma_force_xt (DMA channel 0 refresh) - line 708, inboard386.c
- ✅ io_waitstates (I/O port cycle padding) - inboard386.c + x86_ops_io.h
- ✅ cpu_rom_prefetch_cycles (ROM fetch timing override) - inboard386.c
- ✅ mem_timing (memory access cycle override) - inboard386.c
- ✅ isa_cycles (ISA bus speed calibration) - inboard386.c

### Mach8 Video Fix - BOTH ROM VERSIONS SUPPORTED
- ✅ Delay-loop fix for OLD ROM (P/N 11301115150) at 0x7B37 - line 1913, 386_dynarec.c
- ✅ Delay-loop fix for REAL ROM (P/N 113-11504-002) at 0x7B23 - line 1968, 386_dynarec.c

### Sound Blaster Configuration - CORRECT
- ✅ dma16=4 (8-bit DMA only, no 16-bit) - vm_win311/86box.cfg line 29
- ✅ receive_input=1 - line 32
- ✅ dspver=302 - line 33

### Configuration - ALL CORRECT
- ✅ CPU: ibm486bl3 (real hardware)
- ✅ CPU speed: 83500000 (83.5 MHz, real hardware)
- ✅ Machine: ibmxt_inboard386 (Inboard device enabled)
- ✅ Video: mach8_vga_isa, 8-bit mode, no IRQ
- ✅ RAM: 5120 KB (5MB)
- ✅ ROM: Real user's dump (P/N 113-11504-002, verified in file)
- ✅ enable_5161: 0 (no phantom expansion unit)

## ✅ WHAT WORKS (CONFIRMED FROM COMrade INVESTIGATION)

1. **DOS boots cleanly** - All timing fixes allow BIOS POST to complete
2. **101 error fixed** - Three IRQ-collision fixes applied (per 2026-07-27 fixes)
3. **Mach8 ROM loads** - Both ROM revisions supported
4. **Windows 3.11 boots to desktop** - Confirmed with real Mach8 ROM
5. **Sound Blaster works** - DMA configuration correct, digitized sound playback works

## ⚠️ KNOWN ISSUES (NOT BLOCKING)

1. **"ROM BIOS shadow RAM failed" message** - Cosmetic (boot continues), root cause understood but fix reverted
2. **Mach8 self-test slow** (65-170s on emulated hardware vs instant on real) - Not worth fixing because:
   - Correct real ROM has minimal self-test (single line "Testing...Ok")
   - Project has the correct ROM now, no extended self-test on real hardware

## ❓ CURRENT BLOCKER: VIDEO BLACK SCREEN AT 83.5 MHz

**Symptom**: Black screen instead of 2-line Mach8 boot
**Config state**: 83.5 MHz, all timing fixes in place, correct ROM
**Hypothesis**: ROM initialization at 83.5 MHz failing (possibly ROM prefetch calculation issue)
**Previous working state**: 100 MHz showed 2-line boot message (from user's earlier session)
**Next step**: Test to determine if this is timing-related or configuration-related

## COMMIT READY

All known fixes from COMrade investigation are:
- ✅ In code (verified)
- ✅ Config correct (verified)
- ✅ ROM correct (verified)
- ⚠️ Black screen issue needs diagnosis

**Recommendation**: Commit current verified state, then debug video issue separately.
