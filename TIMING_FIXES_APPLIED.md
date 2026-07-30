# 86Box Inboard Timing Fixes - Foundation for All Future Work

## Date Applied
2026-07-30 (consolidated from 2026-07-24/07-25 investigation in COMrade_Latest)

## Summary
All timing fixes discovered and validated during the intensive debugging session have been incorporated into 86box_full/ source tree. These fixes resolve critical instruction-timing issues that prevented the Inboard 386/PC emulation from booting properly on 386+ CPU cores with the original 1982 IBM XT BIOS.

## Root Cause (Not a Device Bug)
The Inboard device code itself is fully exonerated. The boot failures (`101` POST error, three-beep pattern) are caused by **CPU instruction execution timing mismatches** between 386+ cores and the original 8088-based BIOS self-tests. The BIOS's POST routines contain fixed-cycle-budget loops calibrated for genuine 8088 timing; on faster CPUs, these loops complete too quickly and timeout.

## Three Critical Fixes Applied

### Fix 1: PIC IMR Test Timing (`reg_op_waitstates` for LOOP instruction)

**Files Modified:**
- `src/cpu/x86_ops_jump.h` - LOOP opcode handlers
- `src/cpu/cpu.c` / `src/cpu/cpu.h` - `reg_op_waitstates` global variable
- `src/device/inboard386.c` - Apply scaling via `inboard386_apply_reg_op_waitstates()`

**What it does:**
The BIOS's PIC IMR test contains a pure 131072-iteration LOOP with no memory or I/O access. Earlier mechanisms (io_waitstates, cpu_waitstates, cpu_rom_prefetch_cycles) all target memory/I/O only and couldn't touch this loop. `reg_op_waitstates` adds a cycle penalty directly to LOOP instruction timing, scaled by the CPU-to-ISA-bus-speed ratio.

**Why it matters:**
Without this fix, the IMR test's delay loop completes too fast, and the system hangs at the first hard POST error. With it, the test passes and execution proceeds to real bugs that follow.

**Line References:**
- `src/cpu/x86_ops_jump.h`: LOOP handlers (opLOOPNE_w, opLOOPNE_l, opLOOPE_w, opLOOPE_l, opLOOP_w, opLOOP_l)
- `src/device/inboard386.c:257`: `inboard386_apply_reg_op_waitstates()` function

### Fix 2: PIC Interrupt Mask Register Timing (`force_xt_imr_timing`)

**Files Modified:**
- `src/pic.c` - Add `force_xt_imr_timing` flag and `pic_set_force_xt_imr_timing()` function
- `src/device/inboard386.c:691` - Call `pic_set_force_xt_imr_timing(1)` on init

**What it does:**
The PIC (8259 Interrupt Controller) on genuine XT hardware has specific timing requirements for IMR (Interrupt Mask Register) writes to become visible. 86Box incorrectly gates this behavior on `is286` (CPU classification) rather than actual XT vs AT hardware presence. The Inboard is logically an XT machine even though it has a 386+ CPU, so we need to force XT-class PIC timing regardless of CPU package.

**Why it matters:**
Without this fix, IMR write-visibility is delayed or incorrect, causing interrupt tests to fail. This is a per-machine-class behavior, not a per-CPU-class one.

**Line References:**
- `src/pic.c:64`: `static int force_xt_imr_timing = 0;`
- `src/pic.c:297`: `pic_set_force_xt_imr_timing()` function
- `src/device/inboard386.c:691`: Initialization call

### Fix 3: DMA DRAM Refresh Channel Activation (`dma_force_xt`)

**Files Modified:**
- `src/dma.c` - Add `dma_force_xt` override flag, modify `dma_xt8237_active()` 
- `src/device/inboard386.c:708` - Call `dma_set_force_xt(1)` on init

**What it does:**
Channel 0 of the DMA controller is dedicated to DRAM refresh on genuine XT hardware. 86Box incorrectly gates this behavior on `is286` (CPU classification), so with a 386+ CPU, refresh doesn't fire. The Inboard is XT-class hardware regardless of CPU, so we force XT-mode DMA behavior.

**Why it matters:**
DRAM refresh is essential - without it, memory contents become corrupted. The BIOS has a POST test for this; if it fails, boot hangs. With this fix, refresh works and the test passes.

**Line References:**
- `src/dma.c:107`: `static int dma_force_xt = 0;`
- `src/dma.c:110`: `dma_set_force_xt()` function
- `src/dma.c:118`: Check in `dma_xt8237_active()`
- `src/device/inboard386.c:708`: Initialization call

## Supporting Mechanisms (Less Critical, But Validated)

These were extensively tested and found to have limited effect on the `101`/`1801` sequence, but are kept as they're architecturally sound and represent correct modeling of accelerator-card behavior:

### `io_waitstates` (I/O Port Cycle Padding)
- **Location**: `src/cpu/cpu.h:644`, `src/cpu/x86_ops_io.h` (all IN/OUT opcodes)
- **Function**: `inboard386_apply_io_waitstates()` (src/device/inboard386.c:225)
- **Effect**: Scales I/O instruction timing by CPU-to-ISA-bus-speed ratio
- **Status**: Validated active and correctly scaled, but insufficient alone to fix timing issues

### `cpu_rom_prefetch_cycles` Scaling
- **Location**: `src/device/inboard386.c:370` - `inboard386_apply_rom_prefetch()`
- **Effect**: Overrides ROM code-fetch timing while shadow-caching is disabled
- **Status**: Validated active on correct CPU (IBM486BL), but timing math doesn't match real hardware constraints

### `mem_timing` Overrides
- **Location**: `src/device/inboard386.c:139` - `inboard386_apply_mem_timing()`
- **Effect**: Scales all memory access cycle counts
- **Status**: Validated active, but insufficient alone

## Known Limitations

### Still-Open `1801` Issue
The boot sequence now reaches `1801` (Expansion I/O Unit test) as a **soft, recoverable error** (not a hard hang). This is progress—the BIOS is far enough along to run this test, which it wouldn't reach at all without the above fixes. The root cause was not fully determined in the original investigation; candidate theories remain documented in INBOARD_86BOX_PORT_PLAN.md.

### Why Not a Complete Fix?
The root-cause analysis determined that even with all cycle-padding mechanisms tuned perfectly, a genuine ~55ms absolute real-time hardware constraint (PIT channel 1 pre-existing countdown) remains unmatched by ratio-based per-instruction scaling. The pragmatic workaround (patched test ROM with adjusted CX budget) was suggested but not implemented.

## Testing & Validation

**Boot Progress with All Fixes Applied:**
- ✅ POST starts normally
- ✅ PIC IMR test passes (no `E35D` hard hang)
- ✅ IRQ0 delivery test passes (interrupt fires in time)
- ✅ DMA refresh test passes (DRAM check completes)
- ✅ "640 KB OK" message prints (RAM count completes)
- ✅ `1801` error displays as soft (recoverable) error, not hard hang
- ❌ Cannot proceed past `1801` without F1 keypress (input not currently automated)

**Build Status:**
- ✅ All source code in place in `86box_full/src/`
- ✅ All initialization calls in `src/device/inboard386.c`
- ✅ Pre-compiled working binary exists: `C:\Users\lycet\OneDrive\Desktop\Claude_stuff\86box_inboard_test\86Box.exe`
- ❌ Current MSVC build environment is broken (missing C runtime headers)
- ⏳ MinGW build environment used previously - setup procedure documented in INBOARD_86BOX_PORT_PLAN.md

## For Future Sessions

1. **Build Environment**: The working binary already incorporates all fixes. To rebuild from source, use the MinGW/MSYS2 setup documented in INBOARD_86BOX_PORT_PLAN.md §2026-07-24, not the current broken MSVC configuration.

2. **Testing**: Use the pre-built 86Box.exe with test VMs in `C:\Users\lycet\OneDrive\Desktop\Claude_stuff\86box_inboard_test\vm_*` directories to validate that these fixes continue to work correctly.

3. **Integration**: When this codebase is upstreamed to mainline 86Box or used as a fork:
   - All device-level initialization in `inboard386.c` is self-contained
   - All core CPU/DMA/PIC modifications are minimal and isolated
   - No changes to existing 86Box device APIs or common code paths
   - Can be cleanly merged via patch files if needed

4. **Further Investigation**: If timing issues reappear in future work:
   - Live-tracing technique proven effective: targeted `fprintf` in `386_dynarec.c`'s `exec386()` with address/content filtering
   - Ring-buffer technique for multi-step failure sequences: `ring_cs[4096]`/`ring_pc[4096]` with one-shot dump on marker address
   - Live ROM dump technique: `mem_readb_phys()` loop, saved to file, disassembled with capstone
   - These are more effective than static analysis for timing issues

## References

- **Root cause analysis**: `COMrade_Latest\XT_5160_rework_claude\INBOARD_86BOX_PORT_PLAN.md` - Full session logs with disassembly, testing matrix, and mechanisms explored
- **Source code documentation**: `src/device/inboard386.c` - Extensive comments throughout explaining each timing mechanism
- **Hardware specifications**: `EXACT_HARDWARE_SPECS.md` (memory file), `HARDWARE_CONFIGURATION.md` (repo)

## Commit Status

All changes are committed to git in `86Box-Inboard` repository. This document serves as a permanent reference for why each fix was necessary and how they work together.
