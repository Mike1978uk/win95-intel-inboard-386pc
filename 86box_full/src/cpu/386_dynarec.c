#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#if defined(__APPLE__) && defined(__aarch64__)
#    include <pthread.h>
#endif
#include <wchar.h>
#include <math.h>
#ifndef INFINITY
#    define INFINITY (__builtin_inff())
#endif

#define HAVE_STDARG_H
#include <86box/86box.h>
#include "cpu.h"
#include "x86.h"
#include "x86_ops.h"
#include "x86seg_common.h"
#include "x86seg.h"
#include "x87_sf.h"
#include "x87.h"
#include <86box/io.h>
#include <86box/mem.h>
#include <86box/nmi.h>
#include <86box/pic.h>
#include <86box/timer.h>
#include <86box/fdd.h>
#include <86box/fdc.h>
#include <86box/machine.h>
#include <86box/plat_fallthrough.h>
#include <86box/plat_unused.h>
#include <86box/gdbstub.h>
#include <86box/keyboard.h>
#ifdef USE_DYNAREC
#    include "codegen.h"
#    ifdef USE_NEW_DYNAREC
#        include "codegen_backend.h"
#    endif
#endif

#ifdef IS_DYNAREC
#    undef IS_DYNAREC
#endif

#include "386_common.h"

#if defined(__APPLE__) && defined(__aarch64__)
#    include <pthread.h>
#endif

#define CPU_BLOCK_END() cpu_block_end = 1

int cpu_force_interpreter   = 0;
int cpu_override_dynarec    = 0;
int inrecomp                = 0;
int cpu_block_end           = 0;
int cpu_end_block_after_ins = 0;

/* [vmmhang] armed from pic.c's IMR-00->AC hook, see the trace site below in exec386(). */
int vmmhang_post_count = 0;

#if defined(__aarch64__) || defined(_M_ARM64)
/* ARM64-only epoch: monotonically advances on dirty-list transitions so
   per-block retry state can distinguish dense bursts from stale retries. */
static uint32_t dynarec_s03e_dirty_epoch             = 0;
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
/* ARM64-only policy: require repeated BYTE_MASK dirty-list hits before
   NO_IMMEDIATES promotion to avoid premature slow-immediate escalation. */
/* Tuning: raise threshold to 3 consecutive dirty-list retries so transient
   churn is more likely to recover via retry-decay before forcing NO_IMMEDIATES. */
#    define DYNAREC_S03B_NO_IMM_THRESHOLD 3
/* Tuning: retry bursts must stay temporally dense; large gaps reset burst
   accumulation instead of carrying stale debt into later promotions. */
#    define DYNAREC_S03E_BURST_GAP_MAX 64
#endif

#ifdef ENABLE_386_DYNAREC_LOG
int x386_dynarec_do_log = ENABLE_386_DYNAREC_LOG;

void
x386_dynarec_log(const char *fmt, ...)
{
    va_list ap;

    if (x386_dynarec_do_log) {
        va_start(ap, fmt);
        pclog_ex(fmt, ap);
        va_end(ap);
    }
}
#else
#    define x386_dynarec_log(fmt, ...)
#endif

static __inline void
fetch_ea_32_long(uint32_t rmdat)
{
    eal_r = eal_w = NULL;
    easeg         = cpu_state.ea_seg->base;
    if (cpu_rm == 4) {
        uint8_t sib = rmdat >> 8;

        switch (cpu_mod) {
            case 0:
                cpu_state.eaaddr = cpu_state.regs[sib & 7].l;
                cpu_state.pc++;
                break;
            case 1:
                cpu_state.pc++;
                cpu_state.eaaddr = ((uint32_t) (int8_t) getbyte()) + cpu_state.regs[sib & 7].l;
                break;
            case 2:
                cpu_state.eaaddr = (fastreadl(cs + cpu_state.pc + 1)) + cpu_state.regs[sib & 7].l;
                cpu_state.pc += 5;
                break;
        }
        /*SIB byte present*/
        if ((sib & 7) == 5 && !cpu_mod)
            cpu_state.eaaddr = getlong();
        else if ((sib & 6) == 4 && !cpu_state.ssegs) {
            easeg            = ss;
            cpu_state.ea_seg = &cpu_state.seg_ss;
        }
        if (((sib >> 3) & 7) != 4)
            cpu_state.eaaddr += cpu_state.regs[(sib >> 3) & 7].l << (sib >> 6);
    } else {
        cpu_state.eaaddr = cpu_state.regs[cpu_rm].l;
        if (cpu_mod) {
            if (cpu_rm == 5 && !cpu_state.ssegs) {
                easeg            = ss;
                cpu_state.ea_seg = &cpu_state.seg_ss;
            }
            if (cpu_mod == 1) {
                cpu_state.eaaddr += ((uint32_t) (int8_t) (rmdat >> 8));
                cpu_state.pc++;
            } else {
                cpu_state.eaaddr += getlong();
            }
        } else if (cpu_rm == 5) {
            cpu_state.eaaddr = getlong();
        }
    }
    if (easeg != 0xFFFFFFFF && ((easeg + cpu_state.eaaddr) & 0xFFF) <= 0xFFC) {
        uint32_t addr = easeg + cpu_state.eaaddr;
        if (readlookup2[addr >> 12] != (uintptr_t) -1)
            eal_r = (uint32_t *) (readlookup2[addr >> 12] + addr);
        if (writelookup2[addr >> 12] != (uintptr_t) -1)
            eal_w = (uint32_t *) (writelookup2[addr >> 12] + addr);
    }
}

static __inline void
fetch_ea_16_long(uint32_t rmdat)
{
    eal_r = eal_w = NULL;
    easeg         = cpu_state.ea_seg->base;
    if (!cpu_mod && cpu_rm == 6) {
        cpu_state.eaaddr = getword();
    } else {
        switch (cpu_mod) {
            case 0:
                cpu_state.eaaddr = 0;
                break;
            case 1:
                cpu_state.eaaddr = (uint16_t) (int8_t) (rmdat >> 8);
                cpu_state.pc++;
                break;
            case 2:
                cpu_state.eaaddr = getword();
                break;
        }
        cpu_state.eaaddr += (*mod1add[0][cpu_rm]) + (*mod1add[1][cpu_rm]);
        if (mod1seg[cpu_rm] == &ss && !cpu_state.ssegs) {
            easeg            = ss;
            cpu_state.ea_seg = &cpu_state.seg_ss;
        }
        cpu_state.eaaddr &= 0xFFFF;
    }
    if (easeg != 0xFFFFFFFF && ((easeg + cpu_state.eaaddr) & 0xFFF) <= 0xFFC) {
        uint32_t addr = easeg + cpu_state.eaaddr;
        if (readlookup2[addr >> 12] != (uintptr_t) -1)
            eal_r = (uint32_t *) (readlookup2[addr >> 12] + addr);
        if (writelookup2[addr >> 12] != (uintptr_t) -1)
            eal_w = (uint32_t *) (writelookup2[addr >> 12] + addr);
    }
}

#define fetch_ea_16(rmdat)       \
    cpu_state.pc++;              \
    cpu_mod = (rmdat >> 6) & 3;  \
    cpu_reg = (rmdat >> 3) & 7;  \
    cpu_rm  = rmdat & 7;         \
    if (cpu_mod != 3) {          \
        fetch_ea_16_long(rmdat); \
        if (cpu_state.abrt)      \
            return 1;            \
    }
#define fetch_ea_32(rmdat)       \
    cpu_state.pc++;              \
    cpu_mod = (rmdat >> 6) & 3;  \
    cpu_reg = (rmdat >> 3) & 7;  \
    cpu_rm  = rmdat & 7;         \
    if (cpu_mod != 3) {          \
        fetch_ea_32_long(rmdat); \
    }                            \
    if (cpu_state.abrt)          \
    return 1

#include "x86_flags.h"

#define PREFETCH_RUN(instr_cycles, bytes, modrm, reads, reads_l, writes, writes_l, ea32)      \
    do {                                                                                      \
        if (cpu_prefetch_cycles)                                                              \
            prefetch_run(instr_cycles, bytes, modrm, reads, reads_l, writes, writes_l, ea32); \
    } while (0)

#define PREFETCH_PREFIX()        \
    do {                         \
        if (cpu_prefetch_cycles) \
            prefetch_prefixes++; \
    } while (0)
#define PREFETCH_FLUSH() prefetch_flush()

#define OP_TABLE(name)   ops_##name
#if 0
#    define CLOCK_CYCLES(c)               \
        {                                 \
            if (fpu_cycles > 0) {         \
                fpu_cycles -= (c);        \
                if (fpu_cycles < 0) {     \
                    cycles += fpu_cycles; \
                }                         \
            } else {                      \
                cycles -= (c);            \
            }                             \
        }
#    define CLOCK_CYCLES_FPU(c)   cycles -= (c)
#    define CONCURRENCY_CYCLES(c) fpu_cycles = (c)
#else
#    define CLOCK_CYCLES(c)     cycles -= (c)
#    define CLOCK_CYCLES_FPU(c) cycles -= (c)
#    define CONCURRENCY_CYCLES(c)
#endif
#define CLOCK_CYCLES_ALWAYS(c) cycles -= (c)

#include "386_ops.h"

#ifdef USE_DEBUG_REGS_486
#    define CACHE_ON() (!(cr0 & (1 << 30)) && !(cpu_state.flags & T_FLAG) && !(dr[7] & 0xFF))
#else
#    define CACHE_ON() (!(cr0 & (1 << 30)) && !(cpu_state.flags & T_FLAG))
#endif

#ifdef USE_DYNAREC
int32_t         cycles_main = 0;
static int32_t  cycles_old  = 0;
static uint64_t tsc_old     = 0;

#    ifdef USE_ACYCS
int32_t acycs = 0;
#    endif

int
codegen_mmx_enter(void)
{
    MMX_ENTER();
    return 0;
}

int
codegen_femms(void)
{
    if (!cpu_has_feature(CPU_FEATURE_MMX)) {
        x86illegal();
        return 1;
    }
    if (cr0 & 0xc) {
        x86_int(7);
        return 1;
    }

    x87_emms();
    return 0;
}

int
codegen_fp_enter(void)
{
    FP_ENTER();
    return 0;
}

void
update_tsc(void)
{
    int      cycdiff;
    uint64_t delta;

    cycdiff = cycles_old - cycles;
#    ifdef USE_ACYCS
    if (inrecomp)
        cycdiff += acycs;
#    endif

    delta = tsc - tsc_old;
    if (delta > 0) {
        /* TSC has changed, this means interim timer processing has happened,
           see how much we still need to add. */
        cycdiff -= delta;
    }

    if (cycdiff > 0)
        tsc += cycdiff;

    if (cycdiff > 0) {
        if (TIMER_VAL_LESS_THAN_VAL(timer_target, (uint64_t) tsc))
            timer_process();
    }
}

static __inline void
exec386_dynarec_int(void)
{
    cpu_block_end = 0;
    x86_was_reset = 0;

    if (trap == 2) {
        /* Handle the T bit in the new TSS first. */
        CPU_BLOCK_END();
        goto block_ended;
    }

    while (!cpu_block_end) {
#    ifndef USE_NEW_DYNAREC
        oldcs  = CS;
        oldcpl = CPL;
#    endif
        cpu_state.oldpc = cpu_state.pc;
        cpu_state.op32  = use32;

        cpu_state.ea_seg = &cpu_state.seg_ds;
        cpu_state.ssegs  = 0;

        fetchdat = fastreadl_fetch(cs + cpu_state.pc);
#    ifdef ENABLE_386_DYNAREC_LOG
        if (in_smm)
            x386_dynarec_log("[%04X:%08X] fetchdat = %08X\n", CS, cpu_state.pc, fetchdat);
#    endif

        if (!cpu_state.abrt) {
            opcode = fetchdat & 0xFF;
            fetchdat >>= 8;

#    ifdef USE_DEBUG_REGS_486
            trap = (trap & ~1) | (!!(cpu_state.flags & T_FLAG));
#    else
            trap = cpu_state.flags & T_FLAG;
#    endif

            cpu_state.pc++;
#    ifdef USE_DEBUG_REGS_486
            cpu_state.eflags &= ~(RF_FLAG);
#    endif
            x86_opcodes[(opcode | cpu_state.op32) & 0x3ff](fetchdat);
        }

#    ifndef USE_NEW_DYNAREC
        if (!use32)
            cpu_state.pc &= 0xffff;
#    endif

#    ifdef USE_DEBUG_REGS_486
        if (!cpu_state.abrt) {
            if (!rf_flag_no_clear) {
                cpu_state.eflags &= ~RF_FLAG;
            }

            rf_flag_no_clear = 0;
        }
#    endif

        if (((cs + cpu_state.pc) >> 12) != pccache)
            CPU_BLOCK_END();

        if (cpu_end_block_after_ins) {
            cpu_end_block_after_ins--;
            if (!cpu_end_block_after_ins)
                CPU_BLOCK_END();
        }

        if (cpu_init)
            CPU_BLOCK_END();

        if (cpu_state.abrt)
            CPU_BLOCK_END();
        if (smi_line)
            CPU_BLOCK_END();
        else if (new_ne)
            CPU_BLOCK_END();
        else if (trap)
            CPU_BLOCK_END();
        else if (nmi && nmi_enable && nmi_mask)
            CPU_BLOCK_END();
        else if ((cpu_state.flags & I_FLAG) && pic.int_pending && !cpu_end_block_after_ins)
            CPU_BLOCK_END();
    }

block_ended:
    if (!cpu_state.abrt && !new_ne && trap) {
        if (trap & 2) dr[6] |= 0x8000;
        if (trap & 1) dr[6] |= 0x4000;
        if (trap & 16) dr[6] |= 0x2000;

        trap = 0;
#    ifndef USE_NEW_DYNAREC
        oldcs = CS;
#    endif
        cpu_state.oldpc = cpu_state.pc;
        x86_int(1);
    }

    cpu_end_block_after_ins = 0;
}

#if defined(__linux__) && !defined(__clang__) && defined(USE_NEW_DYNAREC)
static inline void __attribute__((optimize("O2")))
#else
static __inline void
#endif
exec386_dynarec_dyn(void)
{
    uint32_t start_pc  = 0;
    uint32_t phys_addr = get_phys(cs + cpu_state.pc);
    int      hash      = HASH(phys_addr);
#    ifdef USE_NEW_DYNAREC
    codeblock_t *block = &codeblock[codeblock_hash[hash]];
#    else
    codeblock_t *block = codeblock_hash[hash];
#    endif
    int valid_block = 0;

#    ifdef USE_NEW_DYNAREC
    if (!cpu_state.abrt)
#    else
    if (block && !cpu_state.abrt)
#    endif
    {
        page_t *page = &pages[phys_addr >> 12];

        /* Block must match current CS, PC, code segment size,
           and physical address. The physical address check will
           also catch any page faults at this stage */
        valid_block = (block->pc == cs + cpu_state.pc) && (block->_cs == cs) && (block->phys == phys_addr) && !((block->status ^ cpu_cur_status) & CPU_STATUS_FLAGS) && ((block->status & cpu_cur_status & CPU_STATUS_MASK) == (cpu_cur_status & CPU_STATUS_MASK));
        if (!valid_block) {
            uint64_t mask = (uint64_t) 1 << ((phys_addr >> PAGE_MASK_SHIFT) & PAGE_MASK_MASK);
#    ifdef USE_NEW_DYNAREC
            int      byte_offset = (phys_addr >> PAGE_BYTE_MASK_SHIFT) & PAGE_BYTE_MASK_OFFSET_MASK;
            uint64_t byte_mask   = 1ULL << (phys_addr & PAGE_BYTE_MASK_MASK);

            if ((page->code_present_mask & mask) ||
                ((page->mem != page_ff) && (page->byte_code_present_mask[byte_offset] & byte_mask)))
#    else
            if (page->code_present_mask[(phys_addr >> PAGE_MASK_INDEX_SHIFT) & PAGE_MASK_INDEX_MASK] & mask)
#    endif
            {
                /* Walk page tree to see if we find the correct block */
                codeblock_t *new_block = codeblock_tree_find(phys_addr, cs);
                if (new_block) {
                    valid_block = (new_block->pc == cs + cpu_state.pc) && (new_block->_cs == cs) && (new_block->phys == phys_addr) && !((new_block->status ^ cpu_cur_status) & CPU_STATUS_FLAGS) && ((new_block->status & cpu_cur_status & CPU_STATUS_MASK) == (cpu_cur_status & CPU_STATUS_MASK));
                    if (valid_block) {
                        block = new_block;
#    ifdef USE_NEW_DYNAREC
                        codeblock_hash[hash] = get_block_nr(block);
#    endif
                    }
                }
            }
        }

        if (valid_block && (block->page_mask & *block->dirty_mask)) {
#    ifdef USE_NEW_DYNAREC
            codegen_check_flush(page, page->dirty_mask, phys_addr);
            if (block->valid && (block->flags & CODEBLOCK_IN_DIRTY_LIST))
                block->flags &= ~CODEBLOCK_WAS_RECOMPILED;
            else
#    else
            codegen_check_flush(page, page->dirty_mask[(phys_addr >> 10) & 3], phys_addr);
            page->dirty_mask[(phys_addr >> 10) & 3] = 0;
#    endif
            if (!block->valid)
                valid_block = 0;
        }
        if (valid_block && block->page_mask2) {
            /* We don't want the second page to cause a page
               fault at this stage - that would break any
               code crossing a page boundary where the first
               page is present but the second isn't. Instead
               allow the first page to be interpreted and for
               the page fault to occur when the page boundary
               is actually crossed.*/
#    ifdef USE_NEW_DYNAREC
            uint32_t phys_addr_2 = get_phys_noabrt(block->pc + ((block->flags & CODEBLOCK_BYTE_MASK) ? 0x40 : 0x400));
#    else
            uint32_t phys_addr_2 = get_phys_noabrt(block->endpc);
#    endif
            page_t *page_2 = &pages[phys_addr_2 >> 12];

            if ((block->phys_2 ^ phys_addr_2) & ~0xfff)
                valid_block = 0;
            else if (block->page_mask2 & *block->dirty_mask2) {
#    ifdef USE_NEW_DYNAREC
                codegen_check_flush(page_2, page_2->dirty_mask, phys_addr_2);
                if (block->valid && (block->flags & CODEBLOCK_IN_DIRTY_LIST))
                    block->flags &= ~CODEBLOCK_WAS_RECOMPILED;
                else
#    else
                codegen_check_flush(page_2, page_2->dirty_mask[(phys_addr_2 >> 10) & 3], phys_addr_2);
                page_2->dirty_mask[(phys_addr_2 >> 10) & 3] = 0;
#    endif
                if (!block->valid)
                    valid_block = 0;
            }
        }
#    ifdef USE_NEW_DYNAREC
        /* ARM64-only: if a BYTE_MASK block executes stably outside the dirty
           list, clear stale retry debt so a distant future dirty hit does not
           trigger premature NO_IMMEDIATES promotion. */
#        if defined(__aarch64__) || defined(_M_ARM64)
        if (valid_block && !(block->flags & CODEBLOCK_IN_DIRTY_LIST) && (block->flags & CODEBLOCK_BYTE_MASK)
            && !(block->flags & CODEBLOCK_NO_IMMEDIATES) && block->dirty_list_recompile_hits) {
            block->dirty_list_recompile_hits = 0;
            block->dirty_list_last_epoch     = 0;
        }
#        endif

        if (valid_block && (block->flags & CODEBLOCK_IN_DIRTY_LIST)) {
            const int had_byte_mask     = !!(block->flags & CODEBLOCK_BYTE_MASK);
            const int had_no_immediates = !!(block->flags & CODEBLOCK_NO_IMMEDIATES);
#if defined(__aarch64__) || defined(_M_ARM64)
            const uint16_t last_epoch_before = block->dirty_list_last_epoch;
#endif
            block->flags &= ~CODEBLOCK_WAS_RECOMPILED;
            if (had_byte_mask) {
                if (!had_no_immediates) {
#if defined(__aarch64__) || defined(_M_ARM64)
                    /* ARM64-only: wait for repeated dirty-list BYTE_MASK
                       hits before NO_IMMEDIATES promotion. */
                    /* Require retries to occur in a dense burst window;
                       stale widely-spaced retries are reset. */
                    dynarec_s03e_dirty_epoch++;
                    {
                        const uint16_t cur_epoch = (uint16_t) dynarec_s03e_dirty_epoch;

                        if (last_epoch_before != 0) {
                            const uint16_t epoch_gap = (uint16_t) (cur_epoch - last_epoch_before);
                            if (epoch_gap > DYNAREC_S03E_BURST_GAP_MAX) {
                                block->dirty_list_recompile_hits = 0;
                            }
                        }
                        block->dirty_list_last_epoch = cur_epoch;
                    }
                    block->dirty_list_recompile_hits++;
                    if (block->dirty_list_recompile_hits >= DYNAREC_S03B_NO_IMM_THRESHOLD) {
                        block->flags |= CODEBLOCK_NO_IMMEDIATES;
                        block->dirty_list_last_epoch = 0;
                    }
#else
                    block->flags |= CODEBLOCK_NO_IMMEDIATES;
#endif
                }
            } else {
#if defined(__aarch64__) || defined(_M_ARM64)
                block->dirty_list_recompile_hits = 0;
                block->dirty_list_last_epoch     = 0;
#endif
                block->flags |= CODEBLOCK_BYTE_MASK;
            }
        }
        if (valid_block && (block->flags & CODEBLOCK_WAS_RECOMPILED) && (block->flags & CODEBLOCK_STATIC_TOP) && block->TOP != (cpu_state.TOP & 7))
#    else
        if (valid_block && block->was_recompiled && (block->flags & CODEBLOCK_STATIC_TOP) && block->TOP != cpu_state.TOP)
#    endif
        {
            /* FPU top-of-stack does not match the value this block was compiled
               with, re-compile using dynamic top-of-stack*/
#    ifdef USE_NEW_DYNAREC
            block->flags &= ~(CODEBLOCK_STATIC_TOP | CODEBLOCK_WAS_RECOMPILED);
#    else
            block->flags &= ~CODEBLOCK_STATIC_TOP;
            block->was_recompiled = 0;
#    endif
        }
    }

#    ifdef USE_NEW_DYNAREC
    if (valid_block && (block->flags & CODEBLOCK_WAS_RECOMPILED))
#    else
    if (valid_block && block->was_recompiled)
#    endif
    {
        void (*code)(void) = (void *) &block->data[BLOCK_START];

#    ifndef USE_NEW_DYNAREC
        codeblock_hash[hash] = block;
#    endif
        inrecomp = 1;
        code();
#    ifdef USE_ACYCS
        acycs = 0;
#    endif
        inrecomp = 0;

#    ifndef USE_NEW_DYNAREC
        if (!use32)
            cpu_state.pc &= 0xffff;
#    endif
    } else if (valid_block && !cpu_state.abrt) {
#    ifdef USE_NEW_DYNAREC
        start_pc                 = cs + cpu_state.pc;
        const int max_block_size = (block->flags & CODEBLOCK_BYTE_MASK) ? ((128 - 25) - (start_pc & 0x3f)) : 1000;
#    else
        start_pc = cpu_state.pc;
#    endif

        cpu_block_end = 0;
        x86_was_reset = 0;

#    if defined(__APPLE__) && defined(__aarch64__)
        if (__builtin_available(macOS 11.0, *)) {
            pthread_jit_write_protect_np(0);
        }
#    endif
        codegen_block_start_recompile(block);
        codegen_in_recompile = 1;

        while (!cpu_block_end) {
#    ifndef USE_NEW_DYNAREC
            oldcs  = CS;
            oldcpl = CPL;
#    endif
            cpu_state.oldpc = cpu_state.pc;
            cpu_state.op32  = use32;

            cpu_state.ea_seg = &cpu_state.seg_ds;
            cpu_state.ssegs  = 0;

            fetchdat = fastreadl_fetch(cs + cpu_state.pc);
#    ifdef ENABLE_386_DYNAREC_LOG
            if (in_smm)
                x386_dynarec_log("[%04X:%08X] fetchdat = %08X\n", CS, cpu_state.pc, fetchdat);
#    endif

            if (!cpu_state.abrt) {
                opcode = fetchdat & 0xFF;
                fetchdat >>= 8;

                cpu_state.pc++;

                codegen_generate_call(opcode, x86_opcodes[(opcode | cpu_state.op32) & 0x3ff], fetchdat, cpu_state.pc, cpu_state.pc - 1);

                x86_opcodes[(opcode | cpu_state.op32) & 0x3ff](fetchdat);

                if (x86_was_reset)
                    break;
            }

#    ifndef USE_NEW_DYNAREC
            if (!use32)
                cpu_state.pc &= 0xffff;
#    endif

                /* Cap source code at 4000 bytes per block; this
                   will prevent any block from spanning more than
                   2 pages. In practice this limit will never be
                   hit, as host block size is only 2kB*/
#    ifdef USE_NEW_DYNAREC
            if (((cs + cpu_state.pc) - start_pc) >= max_block_size)
#    else
            if ((cpu_state.pc - start_pc) > 1000)
#    endif
                CPU_BLOCK_END();

            if (cpu_init)
                CPU_BLOCK_END();

            if (new_ne)
                CPU_BLOCK_END();
            if ((cpu_state.flags & T_FLAG) || (trap == 2))
                CPU_BLOCK_END();
            if (smi_line)
                CPU_BLOCK_END();
            if (nmi && nmi_enable && nmi_mask)
                CPU_BLOCK_END();
            if ((cpu_state.flags & I_FLAG) && pic.int_pending && !cpu_end_block_after_ins)
                CPU_BLOCK_END();

            if (cpu_end_block_after_ins) {
                cpu_end_block_after_ins--;
                if (!cpu_end_block_after_ins)
                    CPU_BLOCK_END();
            }

            if (cpu_state.abrt) {
                if (!(cpu_state.abrt & ABRT_EXPECTED))
                    codegen_block_remove();
                CPU_BLOCK_END();
            }
        }

        cpu_end_block_after_ins = 0;

        if ((!cpu_state.abrt || (cpu_state.abrt & ABRT_EXPECTED)) && !new_ne && !x86_was_reset)
            codegen_block_end_recompile(block);

        if (x86_was_reset)
            codegen_reset();

        codegen_in_recompile = 0;
#    if defined(__APPLE__) && defined(__aarch64__)
        if (__builtin_available(macOS 11.0, *)) {
            pthread_jit_write_protect_np(1);
        }
#    endif
    } else if (!cpu_state.abrt) {
        /* Mark block but do not recompile */
#    ifdef USE_NEW_DYNAREC
        start_pc                 = cs + cpu_state.pc;
        const int max_block_size = (block->flags & CODEBLOCK_BYTE_MASK) ? ((128 - 25) - (start_pc & 0x3f)) : 1000;
#    else
        start_pc = cpu_state.pc;
#    endif

        cpu_block_end = 0;
        x86_was_reset = 0;

        codegen_block_init(phys_addr);

        while (!cpu_block_end) {
#    ifndef USE_NEW_DYNAREC
            oldcs  = CS;
            oldcpl = CPL;
#    endif
            cpu_state.oldpc = cpu_state.pc;
            cpu_state.op32  = use32;

            cpu_state.ea_seg = &cpu_state.seg_ds;
            cpu_state.ssegs  = 0;

            codegen_endpc = (cs + cpu_state.pc) + 8;
            fetchdat      = fastreadl_fetch(cs + cpu_state.pc);

#    ifdef ENABLE_386_DYNAREC_LOG
            if (in_smm)
                x386_dynarec_log("[%04X:%08X] fetchdat = %08X\n", CS, cpu_state.pc, fetchdat);
#    endif

            if (!cpu_state.abrt) {
                opcode = fetchdat & 0xFF;
                fetchdat >>= 8;

                cpu_state.pc++;

                x86_opcodes[(opcode | cpu_state.op32) & 0x3ff](fetchdat);

                if (x86_was_reset)
                    break;
            }

#    ifndef USE_NEW_DYNAREC
            if (!use32)
                cpu_state.pc &= 0xffff;
#    endif

                /* Cap source code at 4000 bytes per block; this
                   will prevent any block from spanning more than
                   2 pages. In practice this limit will never be
                   hit, as host block size is only 2kB */
#    ifdef USE_NEW_DYNAREC
            if (((cs + cpu_state.pc) - start_pc) >= max_block_size)
#    else
            if ((cpu_state.pc - start_pc) > 1000)
#    endif
                CPU_BLOCK_END();

            if (cpu_init)
                CPU_BLOCK_END();

            if (new_ne)
                CPU_BLOCK_END();
            if (cpu_state.flags & T_FLAG)
                CPU_BLOCK_END();
            if (smi_line)
                CPU_BLOCK_END();
            if (nmi && nmi_enable && nmi_mask)
                CPU_BLOCK_END();
            if ((cpu_state.flags & I_FLAG) && pic.int_pending && !cpu_end_block_after_ins)
                CPU_BLOCK_END();

            if (cpu_end_block_after_ins) {
                cpu_end_block_after_ins--;
                if (!cpu_end_block_after_ins)
                    CPU_BLOCK_END();
            }

            if (cpu_state.abrt) {
                if (!(cpu_state.abrt & ABRT_EXPECTED))
                    codegen_block_remove();
                CPU_BLOCK_END();
            }
        }

        cpu_end_block_after_ins = 0;

        if ((!cpu_state.abrt || (cpu_state.abrt & ABRT_EXPECTED)) && !new_ne && !x86_was_reset)
            codegen_block_end();

        if (x86_was_reset)
            codegen_reset();
    }
#    ifdef USE_NEW_DYNAREC
    else
        cpu_state.oldpc = cpu_state.pc;
#    endif

}

void
exec386_dynarec(int32_t cycs)
{
    int      vector;
    int      tempi;
    int32_t  cycdiff;
    int32_t  oldcyc;
    int32_t  oldcyc2;
    uint64_t oldtsc;
    uint64_t delta;

    int32_t cyc_period = cycs / (force_10ms ? 2000 : 200); /*5us*/

#    ifdef USE_ACYCS
    acycs = 0;
#    endif
    cycles_main += cycs;
    while (cycles_main > 0) {
        int32_t cycles_start;

        cycles += cyc_period;
        cycles_start = cycles;

        while (cycles > 0) {
#    ifndef USE_NEW_DYNAREC
            oldcs           = CS;
            cpu_state.oldpc = cpu_state.pc;
            oldcpl          = CPL;
            cpu_state.op32  = use32;

            cycdiff = 0;
#    endif
            oldcyc = oldcyc2 = cycles;
            cycles_old       = cycles;
            oldtsc           = tsc;
            tsc_old          = tsc;
            if (cpu_force_interpreter || cpu_override_dynarec ||  (!CACHE_ON())) /*Interpret block*/
            {
                exec386_dynarec_int();
            } else {
                exec386_dynarec_dyn();
            }

            if (cpu_init) {
                cpu_init = 0;
                resetx86();
            }

            if (cpu_state.abrt) {
                flags_rebuild();
                tempi          = cpu_state.abrt & ABRT_MASK;
                cpu_state.abrt = 0;
                x86_doabrt(tempi);
                if (cpu_state.abrt) {
                    cpu_state.abrt = 0;
                    cpu_state.pc   = cpu_state.oldpc;
#    ifndef USE_NEW_DYNAREC
                    CS = oldcs;
#    endif
                    pmodeint(8, 0);
                    if (cpu_state.abrt) {
                        cpu_state.abrt = 0;
                        softresetx86();
                        cpu_set_edx();
#    ifdef ENABLE_386_DYNAREC_LOG
                        x386_dynarec_log("Triple fault - reset\n");
#    endif
                    }
                }
            }

            if (new_ne) {
#    ifndef USE_NEW_DYNAREC
                oldcs = CS;
#    endif
                cpu_state.oldpc = cpu_state.pc;
                new_ne = 0;
                x86_int(16);
            }

            if (smi_line)
                enter_smm_check(0);
            else if (nmi && nmi_enable && nmi_mask) {
#    ifndef USE_NEW_DYNAREC
                oldcs = CS;
#    endif
                cpu_state.oldpc = cpu_state.pc;
                x86_int(2);
                nmi_enable = 0;
#    ifdef OLD_NMI_BEHAVIOR
                if (nmi_auto_clear) {
                    nmi_auto_clear = 0;
                    nmi            = 0;
                }
#    else
                nmi = 0;
#    endif
            } else if ((cpu_state.flags & I_FLAG) && pic.int_pending) {
                vector = picinterrupt();
                if (vector != -1) {
#    ifndef USE_NEW_DYNAREC
                    oldcs = CS;
#    endif
                    cpu_state.oldpc = cpu_state.pc;
                    x86_int(vector);
                }
            }

            cycdiff = oldcyc - cycles;
            delta   = tsc - oldtsc;
            if (delta > 0) {
                /* TSC has changed, this means interim timer processing has happened,
                   see how much we still need to add. */
                cycdiff -= delta;
                if (cycdiff > 0)
                    tsc += cycdiff;
            } else {
                /* TSC has not changed. */
                tsc += cycdiff;
            }

            if (cycdiff > 0) {
                if (TIMER_VAL_LESS_THAN_VAL(timer_target, (uint64_t) tsc))
                    timer_process();
            }

#    ifdef USE_GDBSTUB
            if (gdbstub_instruction())
                return;
#    endif
        }

        cycles_main -= (cycles_start - cycles);
    }
}
#endif

/* Intel Inboard 386/PC POST fix-ups.

   These are address-gated corrections to this specific 1986 IBM XT BIOS's own POST
   self-tests and to the ATI Mach8 option ROM's self-test, needed because the Inboard's
   accelerated CPU breaks blind, instruction-counted delay loops those routines were
   calibrated against on a genuine 4.77 MHz 8088.

   Kept in ONE function called from BOTH interpreter loops (exec386() here and
   exec386_2386() in 386.c). cpu.c's cpu_set() routes 386DX/386SX-class CPUs to
   exec386_2386() and 486BL/486DLC-class ones to exec386(); when these fix-ups lived only
   in exec386(), selecting a plain 386DX/386SX - the CPU this card was actually sold to
   pair with - meant none of them ran at all, and POST hung in the Mach8 option ROM's PIT
   delay loop before even reaching the RAM count.

   Both call sites are gated on inboard386_present (set only while the card's device is
   instantiated), so on every other machine this costs one predictable branch per
   instruction and nothing here can run. That gate is what makes it safe: the individual
   fix-ups below are address-gated to this BIOS's and this option ROM's own code, but the
   segment-scoped ones (CS==0xC000, CS==0x0EAF) would otherwise be reachable by unrelated
   guests. */
void
inboard_post_fixups(void)
{
    /* Mach8 option-ROM self-test speed fix (2026-07-26, see
       INBOARD_86BOX_PORT_PLAN.md). `io_waitstates`/`reg_op_waitstates`
       (inboard386.c) exist to make the *system BIOS's* own blind, instruction-
       counted delay loops - calibrated against real 4.77MHz-ISA-bus timing -
       take roughly the same real wall-clock time regardless of the Inboard's
       configured accelerator speed. The Mach8 option ROM's own self-test is a
       different case entirely: once its PIT-readback delay loop is fixed (the
       C000:7B37 fix below) to resolve on the guest's own terms, its remaining
       delays are governed by genuine, correctly-real-time-paced PIT ticks, not
       blind instruction counts - so it needs no compensation at all, and
       applying the same inflation this project needs elsewhere in POST to the
       option ROM's own hundreds of individual I/O operations is exactly what
       was stretching a real-hardware-instant self-test into 65-100+ real
       seconds (confirmed by the user's own real hardware: banner shows
       immediately, no visible delay). Scoped to CS==0xC000 only - restores the
       real values the instant execution leaves the option ROM's own segment,
       so every other POST-timing fix elsewhere in this project (all tuned
       against the real, uncompensated io_waitstates/reg_op_waitstates values)
       is completely unaffected. */
    {
        static int c000_ws_saved        = 0;
        static int saved_io_ws          = 0;
        static int saved_regop_ws       = 0;
        static int saved_prefetch       = 0;
        static int saved_mem_prefetch   = 0;
        static int saved_rom_prefetch   = 0;
        static int saved_cycles_read    = 0;
        static int saved_cycles_read_l  = 0;
        static int saved_cycles_write   = 0;
        static int saved_cycles_write_l = 0;
        static int saved_isa_cycles     = 0;
        if (CS == 0xC000) {
            if (!c000_ws_saved) {
                c000_ws_saved           = 1;
                saved_io_ws             = io_waitstates;
                saved_regop_ws          = reg_op_waitstates;
                saved_prefetch          = cpu_prefetch_cycles;
                saved_mem_prefetch      = cpu_mem_prefetch_cycles;
                saved_rom_prefetch      = cpu_rom_prefetch_cycles;
                saved_cycles_read       = cpu_cycles_read;
                saved_cycles_read_l     = cpu_cycles_read_l;
                saved_cycles_write      = cpu_cycles_write;
                saved_cycles_write_l    = cpu_cycles_write_l;
                saved_isa_cycles        = isa_cycles;
                io_waitstates           = 0;
                reg_op_waitstates       = 0;
                cpu_prefetch_cycles     = 1;
                cpu_mem_prefetch_cycles = 1;
                cpu_rom_prefetch_cycles = 1;
                cpu_cycles_read         = 1;
                cpu_cycles_read_l       = 1;
                cpu_cycles_write        = 1;
                cpu_cycles_write_l      = 1;
                isa_cycles              = 1;
            }
        } else if (c000_ws_saved) {
            c000_ws_saved           = 0;
            io_waitstates           = saved_io_ws;
            reg_op_waitstates       = saved_regop_ws;
            cpu_prefetch_cycles     = saved_prefetch;
            cpu_mem_prefetch_cycles = saved_mem_prefetch;
            cpu_rom_prefetch_cycles = saved_rom_prefetch;
            cpu_cycles_read         = saved_cycles_read;
            cpu_cycles_read_l       = saved_cycles_read_l;
            cpu_cycles_write        = saved_cycles_write;
            cpu_cycles_write_l      = saved_cycles_write_l;
            isa_cycles              = saved_isa_cycles;
        }
    }

    /* Mach8/ATI Graphics Ultra option-ROM PIT-readback delay-loop fix (2026-07-26,
       omitted from PR #7626 - 386_dynarec.c was not part of that submission's file
       list). The option ROM's own self-test does a real PIT-elapsed-ticks busy-wait
       (OUT 43h,0 / IN 40h / IN 40h / SUB / NEG / CMP / JBE) which desyncs from this
       project's CPU-speed/waitstate timing overrides and never resolves on its own.
       Zero blast radius: only touches CS=C000 (the option ROM's own segment) at this
       exact loop's compare instruction, forces the elapsed-ticks register past the
       loop's own target so the guest's own CMP/JBE resolves and exits on its own
       terms - the same as a real, unaccelerated system's PIT eventually ticking past
       the target. 0x7B37/0x7B23 are two previously-encountered ROM revisions; 0x7B16
       is a third, found via live CS:PC tracing against this clone's own ROM copy. */
    if ((CS == 0xC000) && ((cpu_state.pc == 0x7B37) || (cpu_state.pc == 0x7B23) ||
                            (cpu_state.pc == 0x7B16)) && (AX <= BX)) {
        AX = (uint16_t) (BX + 1);
    }

    /* Intel Inboard 386/PC follow-up POST self-test fixes (2026-07-26), omitted from
       the original PR #7626 - 386_dynarec.c was not part of that submission's file list.
       The base PIC-IMR/DMA-refresh timing fix (dma_force_xt/force_xt_imr_timing) makes
       the first of three back-to-back BIOS self-tests pass, but two more chained ones
       were found to still intermittently fail on real timing:
       1. F000:E362-E3AC: the BIOS's own IRQ0-delivery and "no spurious interrupt" checks
          can be contaminated by a genuine, unrelated IRQ1 (keyboard controller's own
          power-on self-test byte) landing during this narrow window, before this BIOS
          ever unmasks interrupts at all - only IRQ1 is suppressed while IRQ0 is verified,
          then both are suppressed during the immediately-following negative check.
       2. F000:E507: the DMA channel-0 (DRAM refresh) status flag is a read-and-clear
          register: something else reads port 8 between the last real refresh cycle and
          this check, consuming the flag before the BIOS's own AND/JNE gets to see it,
          even though refresh itself (PIT channel 1 -> DREQ0) is working correctly. Forces
          the bit the guest's own check consumes, rather than the read that clears it.
       Address-gated to this exact 1986 XT BIOS's own self-test byte ranges - inert on any
       other BIOS content or machine, the same technique already used by this file's
       existing Mach8-specific timing fixes. This self-test's own "test passed" exit can
       land on any of three adjacent addresses (E38E/E3AD/E3AE) depending on a data-
       dependent micro-branch a few instructions earlier; only E3AE proceeds into the
       negative-test phase (2026-08-22 correction - the original port only recognized
       E3AE/E38E, missing E3AD, which left IRQ1 suppressed for the rest of execution
       whenever this exact ROM took that path). */
    {
        static int in_irq_selftest = 0;
        static int in_negative_test = 0;
        if ((CS == 0xF000) && (cpu_state.pc == 0xE362) && !in_irq_selftest) {
            in_irq_selftest = 1;
        }
        /* Safety net: the explicit exit addresses below are reached via a data-dependent
           micro-branch, so the exact one taken varies with POST timing (e.g. whether a
           large video option ROM ran first). If execution leaves this self-test's own
           address range by any path we didn't enumerate, disarm here rather than leave
           IRQ1/IRQ0 suppressed for the rest of the session - a stuck gate silently breaks
           the keyboard for the whole run (observed as a POST keyboard error requiring F1).
           This can only ever shorten suppression, never extend it. */
        if ((in_irq_selftest || in_negative_test) &&
            ((CS != 0xF000) || (cpu_state.pc < 0xE362) || (cpu_state.pc > 0xE3C6))) {
            in_irq_selftest  = 0;
            in_negative_test = 0;
        }
        if (in_irq_selftest) {
            picintc(2); /* IRQ1 (keyboard) only - bit 1. IRQ0 (bit 0) untouched. */
            if ((CS == 0xF000) && ((cpu_state.pc == 0xE3AE) || (cpu_state.pc == 0xE38E)
                                    || (cpu_state.pc == 0xE3AD))) {
                in_irq_selftest  = 0;
                in_negative_test = (cpu_state.pc == 0xE3AE);
            }
        }
        if (in_negative_test) {
            picintc(1);
            picintc(2); /* both IRQ0 and IRQ1 - this test wants total silence. */
            if ((CS == 0xF000) && ((cpu_state.pc == 0xE3C6) || (cpu_state.pc == 0xE38E))) {
                in_negative_test = 0;
            }
        }
    }
    if ((CS == 0xF000) && (cpu_state.pc == 0xE507))
        AL |= 0x01;

    /* Segment-650B/INT-68h wild-jump fix (2026-08-04, Windows 95 boot). VMM32's real-mode
       VxD-loader startup code (segment 0EAF) executes `INT 68h` (a private multiplex-style
       call, AH=function selector) whose IVT vector (offset 0x68*4=0x1A0) is never
       initialized by anything earlier in boot, so the CPU walks off into the raw IVT
       table as code until it coincidentally hits a real CALL FAR into segment 650B -
       legitimate code, but reached with completely bogus calling context, causing erratic
       wild jumping. Pre-initializes the vector to point at an IRET, so INT 68h becomes a
       harmless no-op instead.

       2026-08-23: point the vector at the BIOS's own IRET at F000:FF53, rather than - as
       before - writing a 0xCF stub into IVT slot 0xF0's vector-table entry (physical
       0x3C0) and pointing there. Suggested by Michal Necasek on PR #7749, and verified
       here: the byte at file offset 0x7F53 of the U18/F800 chip is 0xCF (IRET) in both
       1986 ROM revisions (09MAY86 and 10JAN86), which are the only ones this machine
       accepts. Strictly better - no injected code at all, and no assumption that INT 0F0h
       is unused this early in boot.

       Fires the moment CS first becomes 0x0EAF (empirically confirmed reliable trigger -
       firing earlier, e.g. at the very first instruction of boot, gets clobbered by
       ordinary BIOS POST/DOS kernel low-memory init before INT 68h is ever reached). */
    {
        static int patchint68_done = 0;
        if (!patchint68_done && (CS == 0x0EAF)) {
            patchint68_done = 1;
            /* Point INT 68h at the BIOS's own IRET at F000:FF53 (verified 0xCF in both
               1986 ROM revisions). No stub is injected. */
            mem_writeb_phys(0x1A0, 0x53); /* INT 68h vector offset lo  = 0xFF53 */
            mem_writeb_phys(0x1A1, 0xFF); /* INT 68h vector offset hi */
            mem_writeb_phys(0x1A2, 0x00); /* INT 68h vector segment lo = 0xF000 */
            mem_writeb_phys(0x1A3, 0xF0); /* INT 68h vector segment hi */
        }
    }

}

void
exec386(int32_t cycs)
{
    int      vector;
    int      tempi;
    int32_t  cycdiff;
    int32_t  oldcyc;
    int32_t  cycle_period;
    int32_t  ins_cycles;
    uint32_t addr;

    cycles += cycs;

    while (cycles > 0) {
        cycle_period = (timer_target - (uint64_t) tsc) + 1;

        x86_was_reset = 0;
        cycdiff       = 0;
        oldcyc        = cycles;
        while (cycdiff < cycle_period) {
#ifdef USE_DEBUG_REGS_486
            int ins_fetch_fault = 0;
#endif
            ins_cycles = cycles;

#ifndef USE_NEW_DYNAREC
            oldcs  = CS;
            oldcpl = CPL;
#endif
            cpu_state.oldpc = cpu_state.pc;
            cpu_state.op32  = use32;

            /* [reset40trace] 2026-08-22 diagnostic, temporary - see matching hook in the upstream-
               clone build. Raw (unconditional, non-deduped) dump of the first 40 instructions since
               CPU reset, to find the exact byte-level cause of the AX divergence at F000:E080.
               Remove once root-caused. */
            {
                static int reset40_count = 0;
                if (reset40_count < 40) {
                    reset40_count++;
                    FILE *rf = fopen("live_reset40_LOCAL.txt", "a");
                    if (rf) {
                        fprintf(rf, "#%d CS:PC=%04X:%08X AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X flags=%04X\n",
                                reset40_count, CS, cpu_state.pc, AX, BX, CX, DX, SI, DI, cpu_state.flags);
                        fclose(rf);
                    }
                }
                {
                    static int td_dumped = 0;
                    if (!td_dumped) {
                        td_dumped = 1;
                        FILE *tf = fopen("live_speedcheck_LOCAL.txt", "w");
                        if (tf) {
                            fprintf(tf, "cpu_busspeed=%.3f cpu_waitstates=%d cpu_multi=%d "
                                        "cpu_dmulti=%.3f is386=%d cpu_16bitbus=%d\n",
                                    cpu_busspeed, cpu_waitstates, cpu_multi,
                                    cpu_dmulti, is386, cpu_16bitbus);
                            fclose(tf);
                        }
                    }
                }
                /* [postpacing] 2026-08-22 diagnostic, temporary - see matching hook in the
                   upstream-clone build. Remove once root-caused. */
                {
                    static uint64_t pp_instr = 0;
                    static time_t   pp_t0    = 0;
                    static int      pp_done  = 0;
                    if (pp_t0 == 0)
                        pp_t0 = time(NULL);
                    pp_instr++;
                    if (!pp_done && ((time(NULL) - pp_t0) >= 12)) {
                        pp_done = 1;
                        FILE *pf = fopen("live_postpacing_LOCAL.txt", "w");
                        if (pf) {
                            fprintf(pf, "reached E69F: instr=%llu real_secs=%ld\n"
                                        "io_waitstates=%d reg_op_waitstates=%d\n"
                                        "cpu_prefetch_cycles=%d cpu_mem_prefetch_cycles=%d cpu_rom_prefetch_cycles=%d\n"
                                        "cpu_cycles_read=%d cpu_cycles_read_l=%d cpu_cycles_write=%d cpu_cycles_write_l=%d\n"
                                        "isa_cycles=%d cpu_waitstates=%d\n",
                                    (unsigned long long) pp_instr, (long) (time(NULL) - pp_t0),
                                    io_waitstates, reg_op_waitstates,
                                    cpu_prefetch_cycles, cpu_mem_prefetch_cycles, cpu_rom_prefetch_cycles,
                                    cpu_cycles_read, cpu_cycles_read_l, cpu_cycles_write, cpu_cycles_write_l,
                                    isa_cycles, cpu_waitstates);
                            fclose(pf);
                        }
                    }
                }
                if ((CS == 0xF000) && (cpu_state.pc == 0xE07E)) {
                    static int dumped_e07e_live = 0;
                    if (!dumped_e07e_live) {
                        dumped_e07e_live = 1;
                        FILE *bf = fopen("live_e07e_bytes_LOCAL.txt", "w");
                        if (bf) {
                            for (int i = 0; i < 8; i++)
                                fprintf(bf, "%02X ", mem_readb_phys(0xF0000u + 0xE07Eu + i));
                            fclose(bf);
                        }
                    }
                }
            }

            /* Mach8 option-ROM self-test speed fix (2026-07-26, see
               INBOARD_86BOX_PORT_PLAN.md). `io_waitstates`/`reg_op_waitstates`
               (inboard386.c) exist to make the *system BIOS's* own blind, instruction-
               counted delay loops - calibrated against real 4.77MHz-ISA-bus timing -
               take roughly the same real wall-clock time regardless of the Inboard's
               configured accelerator speed. The Mach8 option ROM's own self-test is a
               different case entirely: once its PIT-readback delay loop is fixed (the
               C000:7B37 fix below) to resolve on the guest's own terms, its remaining
               delays are governed by genuine, correctly-real-time-paced PIT ticks, not
               blind instruction counts - so it needs no compensation at all, and
               applying the same inflation this project needs elsewhere in POST to the
               option ROM's own hundreds of individual I/O operations is exactly what
               was stretching a real-hardware-instant self-test into 65-100+ real
               seconds (confirmed by the user's own real hardware: banner shows
               immediately, no visible delay). Scoped to CS==0xC000 only - restores the
               real values the instant execution leaves the option ROM's own segment,
               so every other POST-timing fix elsewhere in this project (all tuned
               against the real, uncompensated io_waitstates/reg_op_waitstates values)
               is completely unaffected. */
            /* The block that used to live here (save/zero/restore of io_waitstates &c while
               CS==0xC000) is now part of the shared inboard_post_fixups() above, so the plain
               386DX/386SX interpreter loop gets it too - see that function. Upstream: PR #7749. */
            if (inboard386_present)
                inboard_post_fixups();

            /* Ring buffer of the last N (CS,PC) pairs, recorded on EVERY instruction
               regardless of address - used to reconstruct the path INTO the shared
               F000:E354 error trap when it's reached from somewhere other than the
               two already-understood PIC-IMR / IRQ0-delivery tests at E32A-E3A0. */
            {
                /* [rawring] 2026-08-02: the main ring buffer below dedupes consecutive
                   identical (CS,PC) - correct for "which distinct addresses ran" but it means
                   a hand-decoded backward walk from its "last recorded predecessor" can miss
                   real intermediate steps if a repeated address briefly recurs, or if a hand
                   decode of the stored bytes has any misalignment - see
                   xt_650B_root_cause_null_far_call_2026_08_02.md's follow-up: patching the
                   pointer that hand-decode identified as the culprit did NOT stop the
                   segment-650B stall, meaning that decode doesn't actually match live execution.
                   This is a small, UNCONDITIONAL (no dedup at all) per-instruction ring - every
                   single instruction cycle appends here, guaranteeing the true immediate
                   sequence is captured with zero ambiguity. Small (4096 entries) since it's
                   meant only for a short backward look right before a specific trigger, not
                   general-purpose history. */
                static uint32_t rawring_cs[4096];
                static uint32_t rawring_pc[4096];
                static int      rawring_pos = 0;

                /* [live0619] 2026-08-02: capstone cross-check found bytes at 0048:0619 (00 EC,
                   captured via the ring_op snapshot mechanism) can only decode as a 2-byte
                   instruction, yet the raw undeduped ring proves the real execution step there
                   was 3 bytes - a genuine, unexplained contradiction. Test the self-modifying-
                   code hypothesis directly: read the bytes FRESH, live, at the exact moment
                   execution reaches this PC (before this instruction executes), every time this
                   address is visited (not one-shot) - if these ever differ from "00 EC ...", the
                   memory changed between captures. If they're always identical, the anomaly is
                   in the tracing/PC-bookkeeping itself, not the guest code. */
                if ((CS == 0x0048) && (cpu_state.pc == 0x0619)) {
                    static int live0619_hits = 0;
                    if (live0619_hits < 10) {
                        live0619_hits++;
                        uint32_t base0619 = ((uint32_t) 0x0048) << 4;
                        fprintf(stderr, "[live0619] #%d live bytes at 0048:0619 = ", live0619_hits);
                        for (int bi = 0; bi < 8; bi++)
                            fprintf(stderr, "%02X ", readmemb(base0619, 0x0619 + bi));
                        fprintf(stderr, "\n");
                        fflush(stderr);
                    }
                }

                rawring_cs[rawring_pos] = CS;
                rawring_pc[rawring_pos] = cpu_state.pc;
                rawring_pos             = (rawring_pos + 1) % 4096;
                if ((CS == 0x0000) && (cpu_state.pc == 0x0000)) {
                    static int rawring_dump_hits = 0;
                    /* only dump the FIRST time we see two consecutive raw entries confirming
                       genuine fresh arrival (not every single cycle spent sitting at 0:0
                       decoding IVT garbage, which would flood this) */
                    int prev_idx = (rawring_pos - 2 + 4096) % 4096;
                    if ((rawring_dump_hits < 3) && !((rawring_cs[prev_idx] == 0x0000) && (rawring_pc[prev_idx] == 0x0000))) {
                        rawring_dump_hits++;
                        fprintf(stderr, "[rawring] last 60 RAW (undeduped) instruction steps before 0000:0000:\n");
                        for (int back = 60; back >= 1; back--) {
                            int idx = ((rawring_pos - back) % 4096 + 4096) % 4096;
                            fprintf(stderr, "  -%02d CS:PC=%04X:%04X\n", back, rawring_cs[idx], rawring_pc[idx]);
                        }
                        fflush(stderr);
                    }
                }
            }
            {
                static uint32_t ring_cs[1048576];
                static uint32_t ring_pc[1048576];
                /* [ringbytes] 2026-08-01: real-time single-step capture - the actual opcode
                   bytes fetched AT THE MOMENT each ring entry is recorded, not a later
                   post-hoc memory dump. Needed because a post-hoc dump of segment 0048 showed
                   what looked like data/zero-fill where the ring buffer's own CS:PC history
                   proved real execution had just happened - strongly suggesting that memory
                   region gets overwritten (reused as a scratch/data buffer) shortly after
                   being executed as code, making any dump taken even slightly later unreliable
                   for this specific investigation. This captures the true, tamper-proof
                   ground truth via readmemb() (correct through paging) at the exact live
                   instant, immune to whatever happens to that memory afterward. */
                static uint8_t  ring_op[1048576][16];
                static int      ring_pos      = 0;
                static int      seen_e3a0     = 0;
                static int      dumped_second = 0;
                static uint32_t ring_last_cs  = 0xFFFFFFFF;
                static uint32_t ring_last_pc  = 0xFFFFFFFF;

                /* Collapse consecutive duplicate (CS,PC) entries - without this, an idle
                   wait/poll loop (e.g. INT 16h keyboard polling) burns through the whole
                   1048576-entry ring in a handful of real-time microseconds, leaving nothing
                   of the actually-interesting code path that ran just before it. This
                   makes the ring buffer's effective time-depth vastly larger for any
                   trigger that fires after even a brief idle spell (found the hard way
                   2026-07-25 chasing the INBRDPC.SYS "BAD" chip marker - the plain ring
                   buffer was 100% consumed by the E842-E84D keyboard-poll loop). */
                /* Diagnostic (2026-07-26, 1986-ROM investigation): F000:E0AB is a classic
                   early-POST 8088 FLAGS/register self-test HLT trap - every conditional
                   jump in the preceding block (E060-E0A9, LAHF/SAHF/STC/CLC/segment-register-
                   chain checks) lands here on ANY unexpected result. Reaching it this early
                   (before the IVT is even set up) matches "black screen, doesn't boot"
                   exactly. Capture which specific check failed by logging the previous
                   unique (CS,PC) - i.e. which conditional jump was actually taken - right
                   before it gets overwritten by the ring-buffer update below. */
                if ((CS == 0xF000) && (cpu_state.pc == 0xE0AB)) {
                    static int e0ab_dumped = 0;
                    if (!e0ab_dumped) {
                        e0ab_dumped = 1;
                        fprintf(stderr, "[e0abtrap] HLT-trap reached; previous unique CS:PC = %04X:%04X (AX=%04X BX=%04X CX=%04X DX=%04X DS=%04X flags=%04X)\n",
                                ring_last_cs, ring_last_pc, AX, BX, CX, DX, ds, cpu_state.flags);
                        fflush(stderr);
                    }
                }

                /* None of the 3 statically-found (E8/E9/EB-opcode-scanned) callers of the
                   shared F000:E387 print+halt routine (E385, E3AC, E511) are being reached
                   this time (confirmed: [e507fix] never fires) - something else, likely an
                   indirect/computed jump my static byte scan can't see, calls in. Trap the
                   TRUE immediate predecessor directly at E387's own entry, same technique as
                   [e0abtrap] above (log ring_last_* before this instruction's own ring-buffer
                   write happens), rather than trusting the full ring dump's wraparound-prone
                   ordering. */
                if ((CS == 0xF000) && (cpu_state.pc == 0xE387)) {
                    static int e387_dumped = 0;
                    if (!e387_dumped) {
                        e387_dumped = 1;
                        fprintf(stderr, "[e387trap] entered shared print+halt; previous unique CS:PC = %04X:%04X\n",
                                ring_last_cs, ring_last_pc);
                        fflush(stderr);
                    }
                }

                /* Companion: log DS:BX right at entry to the F8C8 ROM-checksum routine
                   (mov cx,0 / xor al,al / add al,[bx] / inc bx / loop $-3 / or al,al / ret -
                   sums 65536 bytes starting at [DS:BX], returns with ZF set iff the sum is 0)
                   so we know exactly which physical memory range is being checksummed and can
                   cross-check it against how the two 1986 ROM files actually get mapped. */
                if ((CS == 0xF000) && (cpu_state.pc == 0xF8C8)) {
                    static int f8c8_dumped = 0;
                    if (!f8c8_dumped) {
                        f8c8_dumped = 1;
                        fprintf(stderr, "[f8c8entry] checksum start DS:BX=%04X:%04X (physical=%05X)\n",
                                ds, BX, (((uint32_t) ds) << 4) + BX);
                        fflush(stderr);
                    }
                }

                /* [vmmhang] 2026-07-31 SB Pro digitized-sound hang, VMM-level half: real-hardware
                   comparison via COMrade already pinned the fault to a Windows VxD context that
                   permanently stops being scheduled right after the master-PIC IMR write that
                   masks IRQ5 (`00 -> AC`) - a flat ring-0 selector (0028), so this is protected-
                   mode VMM32 code, not BIOS/real-mode. Technique 11 (dma.c/pic.c/snd_sb_dsp.c)
                   already proved zero further DMA/DSP-port I/O happens anywhere after that
                   trigger, ruling out a hardware-register bug - the answer has to be visible in
                   the pure CS:PC execution path from here, not in device state.
                   Armed from `pic.c` (`vmmhang_post_count`), the exact same call site as the
                   existing `sbprov2_hang_trace_armed` flag - NOT by re-matching the CS:PC value
                   `picimr5` logs (0028:80051271): that value is captured from inside the OUT
                   instruction's own handler, after `cpu_state.pc` has already advanced past the
                   opcode byte, so it's a post-fetch snapshot, not a value guaranteed to reappear
                   at this loop's pre-fetch top-of-loop check (confirmed empirically 2026-07-31:
                   an exact-CS:PC-match version of this hook never fired even though `picimr5`
                   logged the address every time). Arming from the device write itself sidesteps
                   that ambiguity entirely. Traces the next 500 DISTINCT (CS,PC) values reached
                   from there (same consecutive-dedup approach as the ring buffer just below,
                   since a genuine wait/spin primitive would otherwise flood the log with one
                   repeating address and hide whatever ran on the way in). A short, non-growing
                   set of unique addresses in the post-trace confirms a real spin/wait loop at the
                   emulator level (not a true CPU halt); a long, non-repeating trace before it
                   settles shows the actual code path taken first. */
                {
                    static uint32_t vmmhang_last_cs     = 0xFFFFFFFF;
                    static uint32_t vmmhang_last_pc     = 0xFFFFFFFF;

                    if (vmmhang_post_count > 0
                        && ((CS != vmmhang_last_cs) || (cpu_state.pc != vmmhang_last_pc))) {
                        vmmhang_last_cs = CS;
                        vmmhang_last_pc = cpu_state.pc;
                        vmmhang_post_count--;
                        fprintf(stderr, "[vmmhangpost] #%03d CS:PC=%04X:%08X EAX=%08X ECX=%08X ESP=%08X EBP=%08X flags=%08X\n",
                                500 - vmmhang_post_count, CS, cpu_state.pc, EAX, ECX, ESP, EBP, cpu_state.flags);
                    }
                }

                /* [int6entry] 2026-08-01: identifies the actual caller of INBRDPC.SYS's INT 06h
                   request 0x050F handler (disassembly-confirmed entry point 020B:0603 - see
                   memory/win95_emulator_repro_2026_08_01.md for the full disassembly). INT 06h
                   is normally the CPU's own Invalid-Opcode exception, not a software API - so
                   dispatch here isn't a `call` instruction our own ring buffer would show as a
                   normal caller; the ring buffer's "previous unique CS:PC"
                   (ring_last_cs/ring_last_pc, read BEFORE this instruction updates it, same
                   ordering [e0abtrap] already relies on) is the literal instruction that
                   triggered the interrupt - either a genuine `INT 6` opcode or whatever
                   invalid/reserved opcode the CPU faulted on. Also dumps CR0 and the full 32-bit
                   EFLAGS (cpu_state.flags is only the low 16 bits - the VM flag, bit 17, lives in
                   cpu_state.eflags's bit 1) to settle whether this is plain real mode or a V86
                   context set up by a protected-mode VxD. Gated on elapsed real time (same
                   pattern as the retired [seg0048trap] hook) so it catches the actual stall
                   instance, not any earlier, unrelated use of this same handler entry point
                   during ordinary boot. One-shot; remove once the caller is identified. */
                /* [ah87caller] 2026-08-04: INBRDPC.SYS's own AH=87h Extended-Memory-Block-Move
                   handler entry point (CS:PC=0206:044E, confirmed via full recursive-descent
                   disassembly - this is the very first instruction, before the handler's own
                   "pushf", so the stack still holds exactly the CPU-pushed INT-return frame:
                   [SP+0]=caller IP, [SP+2]=caller CS, [SP+4]=caller FLAGS). The handler redoes a
                   full 32KB self-test on every single call (its own completion flag at [0x2B7] is
                   never set anywhere in the file), and this segment has been observed cycling for
                   many minutes during the OSR1 XT+Inboard patched-VxD boot test - capture who's
                   actually calling it repeatedly (HIMEM.SYS? EMM386? something else?) rather than
                   guessing. Capped, not one-shot, to see if the caller is consistent. */
                {
                    static int ah87caller_hits = 0;
                    if ((ah87caller_hits < 30) && (CS == 0x0206) && (cpu_state.pc == 0x044e)) {
                        ah87caller_hits++;
                        uint32_t stack_base   = ((uint32_t) SS) << 4;
                        uint16_t caller_ip    = readmemb(stack_base, SP + 0) | (readmemb(stack_base, SP + 1) << 8);
                        uint16_t caller_cs    = readmemb(stack_base, SP + 2) | (readmemb(stack_base, SP + 3) << 8);
                        fprintf(stderr,
                                "[ah87caller] #%d called from CS:IP=%04X:%04X (AX=%04X BX=%04X CX=%04X DX=%04X)\n",
                                ah87caller_hits, caller_cs, caller_ip, AX, BX, CX, DX);
                        fflush(stderr);
                    }
                }

                {
                    static int    int6entry_fired = 0;
                    static time_t int6entry_t0    = 0;

                    if (int6entry_t0 == 0)
                        int6entry_t0 = time(NULL);

                    if (!int6entry_fired && CS == 0x020B && cpu_state.pc == 0x0603
                        && (time(NULL) - int6entry_t0) >= 150) {
                        int6entry_fired = 1;
                        fprintf(stderr,
                                "[int6entry] t+%lds triggered by CS:PC=%04X:%04X -> handler 020B:%04X "
                                "| CR0=%08X EFLAGS=%04X%04X (VM=%s) | AX=%04X BX=%04X CX=%04X DX=%04X SP=%04X BP=%04X\n",
                                (long) (time(NULL) - int6entry_t0), ring_last_cs, ring_last_pc, cpu_state.pc,
                                cr0, cpu_state.eflags, cpu_state.flags,
                                (cpu_state.eflags & 0x0002) ? "SET (V86 mode)" : "clear (real/protected, not V86)",
                                AX, BX, CX, DX, SP, BP);

                        /* [ringhistory] 2026-08-01: x86_int6_trap (386_common.c, hooking
                           x86_int() directly) shows x86_int(6) is called only ~30 times total,
                           ALL in the first second of boot - never again during the later
                           "stall" - yet CS:PC repeatedly reaches 020B:0603 at t+180s+ anyway, so
                           it must be via ordinary control flow (jump/call), not a fresh
                           interrupt. ring_last_cs/ring_last_pc gives only the single immediately-
                           preceding address; dump the last 40 entries of the existing ring
                           buffer (already tracks up to 1048576 recent unique CS:PC pairs, see
                           ~line 1076) to see the actual call chain, not just one data point. */
                        fprintf(stderr, "[ringhistory] last 20 unique CS:PC before entering 020B:0603 (most recent first), "
                                        "with 16 live-captured opcode bytes each (real-time, not a post-hoc dump):\n");
                        for (int rh = 1; rh <= 20; rh++) {
                            int ridx = (ring_pos - rh + 1048576) % 1048576;
                            fprintf(stderr, "  [%2d] %04X:%04X  bytes=", rh, ring_cs[ridx], ring_pc[ridx]);
                            for (int rb = 0; rb < 16; rb++)
                                fprintf(stderr, "%02X ", ring_op[ridx][rb]);
                            fprintf(stderr, "\n");
                        }

                        /* [stackpeek] 2026-08-01: x86illegal_trap (386_common.c) never fires for
                           this trigger, and there's no CD 06 (explicit INT 6) byte pattern near
                           the ring-buffer-reported caller address either - so dispatch here is
                           NOT going through either the CPU's fault path or a deliberate `int 6`
                           software interrupt. That leaves a plain CALL treating this address as
                           an ordinary subroutine - which would mean an IRET-based return (3
                           words: IP,CS,FLAGS) is fundamentally the wrong mechanism (should be a
                           plain RETF, 2 words: IP,CS) and would desync the stack by one word.
                           Settle this empirically: dump SS:SP and the next 6 words on the stack.
                           A genuine INT/exception frame's 3rd word (the FLAGS slot) always has
                           bit 1 set (0x0002, architecturally always-1) and plausible IOPL/other
                           bits for V86 mode (~0x2xxxx range folded into the low word); a
                           CALL-frame's "3rd word" is just whatever the caller had on its stack
                           before the call - essentially arbitrary, very unlikely to coincidentally
                           satisfy the same bit-1-always-set pattern. */
                        {
                            uint32_t stack_base = ((uint32_t) SS) << 4;
                            fprintf(stderr, "[stackpeek] SS:SP=%04X:%04X stack words: ", SS, SP);
                            for (int w = 0; w < 6; w++) {
                                uint16_t lo = readmemb(stack_base, SP + w * 2);
                                uint16_t hi = readmemb(stack_base, SP + w * 2 + 1);
                                fprintf(stderr, "%04X ", (uint16_t) (lo | (hi << 8)));
                            }
                            fprintf(stderr, "\n");
                        }

                        /* Dump the full 64KB of the caller's segment (ring_last_cs, confirmed
                           0x0048 - the component that deliberately triggers this INT 06h/0x050F
                           protocol) via readmemb() rather than a raw physical-address guess,
                           since CR0.PG=1 here (paging active) - readmemb() goes through the
                           actual page-table translation (readlookup2), so this reflects what the
                           V86 CPU genuinely sees, not whatever happens to sit at a naively
                           computed physical address. V86-mode segment base = selector*16 (plain
                           real-mode-style addressing, no descriptor table involved even under
                           paging). Written to vm_win311/seg0048_dump.bin for offline disassembly
                           - not into the stderr log, 64KB is too large for that. */
                        {
                            uint32_t caller_base = ((uint32_t) ring_last_cs) << 4;
                            FILE    *df          = fopen("seg0048_dump.bin", "wb");
                            if (df) {
                                for (uint32_t off = 0; off < 0x10000; off++) {
                                    uint8_t b = readmemb(caller_base, off);
                                    fputc(b, df);
                                }
                                fclose(df);
                                fprintf(stderr, "[int6entry] dumped 64KB of segment %04X to seg0048_dump.bin\n", ring_last_cs);
                            }
                        }
                    }
                }

                /* [himemcaller] 2026-08-02: [segidtrace2] identified segment 0E77 as HIMEM.SYS
                   (Windows XMS Driver 3.95 strings, "ERROR: Unable to control A20 line!" etc).
                   [a20trace] showed continuous, repeated OUT 60,DF/DD + OUT 64,FF/D1 A20 toggling
                   through this segment for the whole session, not just at CONFIG.SYS load time -
                   HIMEM.SYS is the resident, shared XMS A20-control point every other component
                   (VMM/VKD included) is expected to call via its driver entry point rather than
                   touching hardware ports directly, so the real question isn't "is HIMEM's port
                   I/O correct" (already verified for this hardware, see inboard386.c's own comment
                   on the port-0x64-hardwired-0x00 trick) but "what keeps calling HIMEM's
                   enable/disable-A20 entry point back-to-back, forever." Catch this the same
                   reliable way Technique 1 already established for genuine CALL/JMP-based control
                   flow (not exception dispatch, so no [e0abtrap]-style caveat applies here): log
                   ring_last_cs/ring_last_pc (the previous unique CS:PC, read BEFORE this
                   instruction updates it) the moment CS transitions INTO 0x0E77 from anything else -
                   that's the literal calling address, whether it's real-mode code, a V86-reflected
                   protected-mode call, or something else entirely. */
                {
                    static int himem_hits = 0;
                    if ((himem_hits < 300) && (CS == 0x0E77) && (ring_last_cs != 0x0E77)) {
                        himem_hits++;
                        fprintf(stderr, "[himemcaller] #%d caller=%04X:%04X -> HIMEM 0E77:%04X AX=%04X BX=%04X\n",
                                himem_hits, ring_last_cs, ring_last_pc, cpu_state.pc, AX, BX);
                        fflush(stderr);
                    }
                }

                /* [seg0f86caller] 2026-08-02, Technique 22: [himemcaller] found 0F86:0B31 repeatedly
                   relaying an INT 2Fh AX=0x1213 request into HIMEM's chain, but disassembly showed
                   0x0B31 is just 0F86's own "not mine, chain onward" tail (its real interest is
                   AH=0x16, the Windows Init/Exit Broadcast) - 0F86 is a pass-through link, not the
                   true caller. Trace one hop further back the same reliable way: log the caller the
                   moment CS transitions INTO 0x0F86 from anything else. */
                {
                    static int seg0f86_hits = 0;
                    if ((seg0f86_hits < 300) && (CS == 0x0F86) && (ring_last_cs != 0x0F86)) {
                        seg0f86_hits++;
                        fprintf(stderr, "[seg0f86caller] #%d caller=%04X:%04X -> 0F86:%04X AX=%04X BX=%04X\n",
                                seg0f86_hits, ring_last_cs, ring_last_pc, cpu_state.pc, AX, BX);
                        fflush(stderr);
                    }
                }

                /* [seg650Bcaller] 2026-08-02: [seg650Btrace] proved segment 650B is executing
                   into completely zeroed memory (see memory
                   xt_650B_smoking_gun_zeroed_memory_2026_08_02.md) - the next question is what
                   jumped/called into it expecting real code to be there, and with what registers
                   (ESI/EDI/ECX in particular - if this is a copy/decompress loop that's supposed
                   to populate the destination buffer, those would be the source/dest/count regs
                   at the moment of the *original* entry into 650B, before whatever loop inside it
                   clobbers them). Same one-shot transition-into-segment technique as
                   [himemcaller]/[seg0f86caller]/[seg6517caller] above. */
                {
                    static int seg650b_caller_hits = 0;
                    if ((seg650b_caller_hits < 20) && (CS == 0x650B) && (ring_last_cs != 0x650B)) {
                        seg650b_caller_hits++;
                        fprintf(stderr,
                                "[seg650Bcaller] #%d caller=%04X:%04X -> 650B:%04X "
                                "EAX=%08X EBX=%08X ECX=%08X EDX=%08X ESI=%08X EDI=%08X "
                                "ESP=%08X EBP=%08X CR0=%08X\n",
                                seg650b_caller_hits, ring_last_cs, ring_last_pc, cpu_state.pc,
                                EAX, EBX, ECX, EDX, ESI, EDI, ESP, EBP, cpu_state.CR0);
                        fflush(stderr);

                        /* [seg650Bbackward] same event, one-shot: [seg650Bvector] proved
                           entry isn't via an IDT interrupt/trap gate, and the caller
                           (0000:0038) is too low/unlikely-looking to be genuine loader code
                           at face value - dump the last 40 distinct real-mode CS:PC steps
                           (with live-captured opcode bytes, per [ringbytes]) leading up to
                           the switch, to see the actual real-mode code path (disk read,
                           CR0 write, far jump) instead of just its final step. */
                        if (seg650b_caller_hits == 1) {
                            fprintf(stderr, "[seg650Bbackward] last 40 distinct steps before entering 650B:\n");
                            for (int back = 40; back >= 1; back--) {
                                int idx = ((ring_pos - back) % 1048576 + 1048576) % 1048576;
                                fprintf(stderr,
                                        "  -%02d CS:PC=%04X:%04X bytes=%02X %02X %02X %02X %02X %02X\n",
                                        back, ring_cs[idx], ring_pc[idx],
                                        ring_op[idx][0], ring_op[idx][1], ring_op[idx][2],
                                        ring_op[idx][3], ring_op[idx][4], ring_op[idx][5]);
                            }
                            fflush(stderr);
                        }
                    }
                }

                /* [segdump0EAF]/[segdumpFF03] 2026-08-04: OSR1 segment-650B/INT-68h investigation -
                   [seg650Bbackward] identified FF03 as the segment that CALLs into 0EAF, and 0EAF
                   as the segment whose code (at 0EAF:3067) executes the fateful `INT 68h` with an
                   uninitialized vector. Neither is a segment this project has identified before.
                   Dump both live (64KB each, physical/real-mode-flat, first time CS reaches them)
                   so they can be string-searched/disassembled offline to find out what component
                   owns this code and what's actually supposed to set up INT 68h's vector first. */
                {
                    static int dumped_0eaf = 0;
                    if (!dumped_0eaf && (CS == 0x0EAF)) {
                        dumped_0eaf = 1;
                        FILE *f = fopen("seg_0EAF_dump.bin", "wb");
                        if (f) {
                            uint32_t base = 0x0EAF0u;
                            for (uint32_t a = 0; a < 0x10000; a++) {
                                uint8_t b = mem_readb_phys(base + a);
                                fwrite(&b, 1, 1, f);
                            }
                            fclose(f);
                            fprintf(stderr, "[segdump0EAF] wrote seg_0EAF_dump.bin (base=%05X) at CS:PC=0EAF:%04X\n",
                                    base, cpu_state.pc);
                            fflush(stderr);
                        }
                    }
                    static int dumped_ff03 = 0;
                    if (!dumped_ff03 && (CS == 0xFF03)) {
                        dumped_ff03 = 1;
                        FILE *f = fopen("seg_FF03_dump.bin", "wb");
                        if (f) {
                            uint32_t base = 0x0FF030u;
                            for (uint32_t a = 0; a < 0x10000; a++) {
                                uint8_t b = mem_readb_phys(base + a);
                                fwrite(&b, 1, 1, f);
                            }
                            fclose(f);
                            fprintf(stderr, "[segdumpFF03] wrote seg_FF03_dump.bin (base=%06X) at CS:PC=FF03:%04X\n",
                                    base, cpu_state.pc);
                            fflush(stderr);
                        }
                    }
                }

                /* [segdump0325] 2026-08-04: Al Williams a20() correspondence investigation - live-
                   traced a real-mode loop at 0325:0471 executing OUT 64h,D1 / OUT 60h,val / OUT
                   64h,FF - a byte-for-byte match to Al Williams's own 1990 Dr. Dobb's Journal
                   errata code's generic-AT (non-Inboard) `a20()` fallback branch (see
                   docs/al_williams_inboard_a20_correspondence_2023.md). Identify which real file
                   this segment actually is (leading suspect: HIMEM.SYS, loaded via CONFIG.SYS
                   right after INBRDPC.SYS) via the same live-dump-then-string-search technique
                   already proven this session. One-shot, fires on first CS==0x0325 - real mode,
                   happens early in boot (no need to wait for the later GUI dialog). */
                {
                    static int dumped_0325 = 0;
                    if (!dumped_0325 && (CS == 0x0325)) {
                        dumped_0325 = 1;
                        FILE *f = fopen("seg_0325_dump.bin", "wb");
                        if (f) {
                            uint32_t base = 0x03250u;
                            for (uint32_t a = 0; a < 0x10000; a++) {
                                uint8_t b = mem_readb_phys(base + a);
                                fwrite(&b, 1, 1, f);
                            }
                            fclose(f);
                            fprintf(stderr, "[segdump0325] wrote seg_0325_dump.bin (base=%05X) at CS:PC=0325:%04X\n",
                                    base, cpu_state.pc);
                            fflush(stderr);
                        }
                    }
                }

                /* [patchint68] 2026-08-04: OSR1 segment-650B/INT-68h investigation - identified
                   segment 0EAF (via live seg_0EAF_dump.bin/seg_FF03_dump.bin string search) as
                   VMM32's own real-mode VxD-loader startup code (strings: "Loading Vxd = ",
                   "LoadSuccess = ", "LoadFailed  = ", the exact static VxD names this project
                   patches, "VMM32\*.VXD", SYSTEM.INI [386enh] device= lines). The already-captured
                   [seg650Bbackward] ring-buffer bytes decode cleanly as: 0EAF:3062 `MOV DI,0x1091`;
                   0EAF:3065 `MOV AH,0x4F`; 0EAF:3067 `INT 68h` - a private multiplex-style call
                   (AH=function selector, same shape as INT 2Fh) whose vector (IVT offset
                   0x68*4=0x1A0) is never initialized by anything in this boot before it's called,
                   so the CPU faithfully walks off into the raw IVT table as "code" until it
                   coincidentally hits a real CALL FAR (INT 0Eh's own vector bytes) into segment
                   650B - real, legitimate code, but reached with completely bogus calling context,
                   hence the erratic in-650B jumping. Rather than fully reverse the AH=4Fh protocol
                   (no source, no public documentation found), test the empirical mitigation the
                   OSR2 investigation already recommended for this exact bug SHAPE (see [patch0144]
                   below, planned but never implemented for the 0048 case): pre-initialize the
                   vector to a harmless one-byte IRET stub *before* anything can reach the call, so
                   INT 68h AH=4Fh becomes a no-op that returns immediately instead of a wild jump.
                   Stub lives at IVT slot 0xF0's own 4-byte vector-table entry (physical 0x3C0) -
                   INT 0F0h is unused this early in a real-mode DOS/Windows 95 boot on non-AT
                   hardware, so borrowing its 1 byte of table space as scratch code is safe; only
                   the byte at 0x3C0 is touched, vector 0xF0 itself is left as whatever it already
                   was (nothing here calls INT 0F0h).

                   2026-08-04 retry: firing this at the very first instruction of boot (before
                   POST) did NOT work - confirmed via a full rerun that the exact same
                   `caller=0000:0038 -> 650B:4500` chain still happened, meaning something between
                   boot and t+~140s (almost certainly ordinary BIOS POST and/or DOS kernel low-
                   memory init, which routinely clears/rewrites large chunks of the IVT) clobbered
                   the early write before INT 68h ever got a chance to use it. Move the trigger to
                   fire the moment CS first becomes 0x0EAF instead (same reliable, empirically-
                   confirmed-every-run trigger point [segdump0EAF] above already uses) - by then
                   POST and DOS init are long finished, and the `INT 68h` call is only a few
                   hundred instructions away (0EAF:1E14's CALL, a few thousand cycles later), far
                   too close for anything else to plausibly rewrite this specific vector again
                   first. One-shot per CS==0x0EAF entry. */
                /* Moved into the shared inboard_post_fixups() above (PR #7749). The one-shot
                   [patchint68] stderr line it used to print went with it. */

                /* [patch0144] 2026-08-02: root-caused (see memory
                   xt_650B_root_cause_null_far_call_2026_08_02.md) - some instruction in segment
                   0048 executes "CS: CALL FAR [0144]", reading a target CS:IP from linear
                   0048:0144, which is genuinely never initialized (confirmed zero). That null
                   call is what starts the whole wild-jump chain ending in the segment-650B
                   stall. The exact calling offset (hand-decoded as ~0664) didn't reproduce when
                   targeted directly - the ring-buffer predecessor evidence suggests that decode
                   had an error - so patch the *data*, not a guessed instruction address: the
                   moment CS first becomes 0x0048 (well before any code in the segment could run
                   far enough to reach the bad call), write a RETF stub into unused scratch space
                   and redirect the 0144 vector at it, so whenever the real call happens it reads
                   a harmless target instead of zero. */
                /* [skip0637] 2026-08-02: raw undeduped tracing (see
                   xt_650B_root_cause_null_far_call_2026_08_02.md) proved with certainty that
                   0048:0637 is the exact, single instruction that transfers control to
                   0000:0000 (zero intermediate steps) - but hand-decoding what it actually is
                   has been unreliable (a capstone cross-check found the static bytes there don't
                   even agree with the live execution step length one instruction earlier, at
                   0619). Rather than keep guessing the mechanism, try neutralizing the address
                   directly: the moment CS:PC reaches 0048:0637, skip past it without executing
                   whatever's actually encoded there, by forcing cpu_state.pc forward. Trying the
                   smallest plausible skip first (+2, past what a plain "OR r/m8,r8" alone would
                   consume) - if this doesn't let boot proceed, the next things to try are larger
                   skips or skipping to the nearby RET this session's capstone decode found
                   (approximately 0048:065B-0650, alignment uncertain). One-shot. */
                if (0 && (CS == 0x0048) && (cpu_state.pc == 0x0637)) {
                    /* gated off 2026-08-04: OSR2-content-specific fixed address, must not fire
                       against OSR1 content during the unmodified-boot fidelity check. */
                    static int skip0637_done = 0;
                    if (!skip0637_done) {
                        skip0637_done = 1;
                        fprintf(stderr, "[skip0637] reached 0048:0637 - skipping to 0048:0639 instead of executing\n");
                        fflush(stderr);
                        cpu_state.pc = 0x0639;
                    }
                }

                /* [nulljump] 2026-08-02: [seg650Bbackward] showed the CPU walking through raw
                   IVT bytes starting at CS:IP=0000:0000, decoding table data as instructions
                   until it coincidentally hit a CALL FAR whose operand bytes (borrowed from IVT
                   vectors 14/15) happened to read 650B:E100 - the real bug is whatever transfers
                   control to 0000:0000 in the first place, not segment 650B itself. Fire the
                   instant CS:PC becomes exactly 0000:0000, reporting the true immediate
                   predecessor (ring_last_cs/pc) plus a *fresh* read of bytes at that predecessor
                   address (not the stored ring_op window, to avoid any hand-alignment error) so
                   the actual transferring instruction (CALL/JMP/INT/IRET/etc) can be identified
                   unambiguously. */
                /* [vkdlivedump] 2026-08-04: OSR1 protected-mode-keyboard-fix investigation -
                   [kbdporttrace2] proved Windows' VKD.VXD polls port 64h's status byte at live
                   linear addresses 0xC0382EDB (poll loop) and issues AT 8042 commands from
                   0xC0383034/0xC004BE48 - but VKD.VXD's LE object table has all-zero relocation
                   base addresses (VMM assigns real linear bases dynamically at load time, not
                   baked into the file), so those addresses can't be mapped to file offsets via
                   header math alone. Dump real bytes AT these live addresses instead (paging-
                   aware via readmemb, correct for protected mode unlike mem_readb_phys) so they
                   can be byte-matched directly against VKD_INBOARD_v2.VXD to find the real file
                   offset - sidesteps the whole LE-relocation problem with ground truth. One-shot
                   each, 64 bytes captured (enough to be a unique match, short enough to read by
                   hand once found). */
                {
                    static int vkddump_a_done = 0, vkddump_b_done = 0, vkddump_c_done = 0;
                    if (!vkddump_a_done && (CS == 0x0028) && (cpu_state.pc == 0xC0382EDB)) {
                        vkddump_a_done = 1;
                        /* [vkdpatchverify] 2026-08-04: verify the IN AL,64h -> MOV AL,1 patch
                           (patch_vkd_kbdready.py, file offset 0x3e11) actually survived the
                           VMM32.VXD combine step - capture starting 4 bytes EARLIER than before
                           so the 2 bytes immediately preceding TEST AL,1 (the actual patched
                           instruction) are visible this time. */
                        fprintf(stderr, "[vkdpatchverify] bytes at 0xC0382ED7 (expect B0 01 if "
                                        "patched, E4 64 if not, right before A8 01 E1 FA C3)=");
                        for (int bi = 0; bi < 12; bi++)
                            fprintf(stderr, "%02X ", readmemb(0, 0xC0382ED7 + bi));
                        fprintf(stderr, "\n");
                        fflush(stderr);
                        fprintf(stderr, "[vkdlivedump] A (poll loop, 0xC0382EDB) bytes=");
                        for (int bi = 0; bi < 64; bi++)
                            fprintf(stderr, "%02X ", readmemb(0, 0xC0382EDB + bi));
                        fprintf(stderr, "\n");
                        fflush(stderr);
                    }
                    if (!vkddump_b_done && (CS == 0x0028) && (cpu_state.pc == 0xC0383034)) {
                        vkddump_b_done = 1;
                        fprintf(stderr, "[vkdlivedump] B (0xAD/0xAE site 1, 0xC0383034) bytes=");
                        for (int bi = 0; bi < 64; bi++)
                            fprintf(stderr, "%02X ", readmemb(0, 0xC0383034 + bi));
                        fprintf(stderr, "\n");
                        fflush(stderr);
                    }
                    if (!vkddump_c_done && (CS == 0x0028) && (cpu_state.pc == 0xC004BE48)) {
                        vkddump_c_done = 1;
                        fprintf(stderr, "[vkdlivedump] C (0xAD/0xAE site 2, 0xC004BE48) bytes=");
                        for (int bi = 0; bi < 64; bi++)
                            fprintf(stderr, "%02X ", readmemb(0, 0xC004BE48 + bi));
                        fprintf(stderr, "\n");
                        fflush(stderr);
                    }
                }

                if ((CS == 0x0000) && (cpu_state.pc == 0x0000) && !((ring_last_cs == 0x0000) && (ring_last_pc == 0x0000))) {
                    static int nulljump_hits = 0;
                    if (nulljump_hits < 10) {
                        nulljump_hits++;
                        uint32_t pred_base = ((uint32_t) ring_last_cs) << 4;
                        /* [nulljump2] 2026-08-02: the original 10-byte window wasn't long enough
                           to reach the actual control-transfer opcode - hand-decoding it showed
                           only ordinary OR/ADD/ROR instructions, no JMP/CALL/INT/IRET, within
                           those 10 bytes. Capture 48 bytes instead so the real transferring
                           instruction (wherever it actually starts) is visible. */
                        fprintf(stderr, "[nulljump2] predecessor=%04X:%04X bytes=", ring_last_cs, ring_last_pc);
                        for (int pi = 0; pi < 56; pi++)
                            fprintf(stderr, "%02X ", readmemb(pred_base, ring_last_pc + pi));
                        fprintf(stderr, "\n");
                        /* [nulljump3] hand-decode of [nulljump2]'s bytes reaches, at byte
                           index 45-47 (2E FF 1E), what looks like "CS: CALL FAR [disp16]" - an
                           indirect far call reading its target CS:IP from a data pointer
                           elsewhere in memory (not adjacent to the instruction bytes). If that
                           pointer table entry was never initialized, this would produce exactly
                           the observed 0000:0000 jump. disp16 is the 2 bytes right after the
                           ModRM (index 48-49) - read it, resolve the linear address it names
                           (CS-relative, per the 2E override), and dump the 4-byte far pointer
                           actually stored there to confirm or refute this directly. */
                        {
                            uint16_t disp16  = readmemb(pred_base, ring_last_pc + 48) |
                                                (readmemb(pred_base, ring_last_pc + 49) << 8);
                            uint32_t ptr_lin = pred_base + disp16;
                            uint16_t ptr_off = readmemb(ptr_lin, 0) | (readmemb(ptr_lin, 1) << 8);
                            uint16_t ptr_seg = readmemb(ptr_lin, 2) | (readmemb(ptr_lin, 3) << 8);
                            fprintf(stderr,
                                    "[nulljump3] disp16=%04X ptr_lin=%08X far_ptr_at_[CS:%04X]=%04X:%04X\n",
                                    disp16, ptr_lin, disp16, ptr_seg, ptr_off);
                        }
                        fflush(stderr);
                    }
                }

                if ((CS != ring_last_cs) || (cpu_state.pc != ring_last_pc)) {
                    ring_last_cs      = CS;
                    ring_last_pc      = cpu_state.pc;
                    ring_cs[ring_pos] = CS;
                    ring_pc[ring_pos] = cpu_state.pc;
                    {
                        uint32_t ring_base = ((uint32_t) CS) << 4;
                        for (int rb = 0; rb < 16; rb++)
                            ring_op[ring_pos][rb] = readmemb(ring_base, cpu_state.pc + rb);
                    }
                    ring_pos          = (ring_pos + 1) % 1048576;
                }

                /* [ringdump0206] 2026-08-04: exhaustive static search (every direct call/jmp/
                   short-jmp/near-cond-jmp/short-cond-jmp form) found ZERO instructions anywhere
                   in INBRDPC.SYS targeting file offset 0xAE1 (runtime 0206:06DE, the self-test-
                   and-report wrapper entry) - it must be reached via an indirect jump/call whose
                   target only exists at runtime. Dump the last 300 unique ring-buffer entries
                   once CS==0x0206 has been running a while, to see the exact real instruction
                   that transfers control there - ground truth, not reconstruction. One-shot. */
                {
                    static int    ringdump_fired = 0;
                    static time_t ringdump_t0    = 0;
                    if (ringdump_t0 == 0)
                        ringdump_t0 = time(NULL);
                    if (!ringdump_fired && (CS == 0x0206) && ((time(NULL) - ringdump_t0) >= 90)) {
                        ringdump_fired = 1;
                        fprintf(stderr, "[ringdump0206] dumping last 300 unique ring entries "
                                        "(oldest first):\n");
                        int start = (ring_pos - 300 + 1048576 * 2) % 1048576;
                        for (int i = 0; i < 300; i++) {
                            int idx = (start + i) % 1048576;
                            fprintf(stderr, "  [%3d] %04X:%04X  bytes=%02X %02X %02X %02X %02X %02X\n",
                                    i, ring_cs[idx], ring_pc[idx],
                                    ring_op[idx][0], ring_op[idx][1], ring_op[idx][2],
                                    ring_op[idx][3], ring_op[idx][4], ring_op[idx][5]);
                        }
                        fflush(stderr);
                    }
                }

                /* [seg0048trace2] 2026-08-02: the NEW CONFIG.SYS-stage stall that survived the
                   resident-size fix (see memory/win95_emulator_repro_2026_08_01.md, "NEXT SESSION
                   STARTS HERE" #1) - CS:PC confined to segment 0048 (offsets ~0x258-0x630, plus
                   occasional C000/F000 visits, matching the ATI option-ROM/BIOS V86 reflection
                   pattern already characterized) starting ~t+110s, screen frozen at "Setting time
                   and date / Starting Comrade". Unlike the fixed INT 06h/0x050F bug, [int6entry]/
                   [x86_int6_trap] both stay at 0 hits here - this does NOT go through INBRDPC.SYS's
                   covert INT 06h API at all, so it's a different mechanism, not a recurrence.
                   Trigger-armed (Technique 11/12 style): arm once real elapsed time hits 100s
                   (comfortably inside the stall window, confirmed live this session), then log the
                   next 400 DISTINCT (CS,PC) transitions with CS==0x0048, each with a full register
                   dump and the first 8 live opcode bytes - enough to see whether this is a genuine
                   bounded loop (few unique addresses, revisited) or slow-but-forward-moving work
                   (many unique addresses), and whether any INT/OUT/IN instructions appear.

                   Revised 2026-08-02, same session: the first cut (arm at 100s, any CS==0x0048
                   offset, 400-hit cap) exhausted its whole cap in ~3 real seconds on ordinary
                   CONFIG.SYS driver-loading code at offsets 0x1279+ - legitimate boot noise
                   reached before the actual stuck loop (confirmed by prior runs to live in the
                   narrower 0x258-0x680 range starting closer to t+110-120s), exactly the
                   cap-exhausted-before-the-interesting-window failure Technique 11 warns about.
                   Fix: restrict to the known offset band and raise the cap so it survives well
                   into the stall proper. */
                {
                    static time_t seg48_t0      = 0;
                    static int    seg48_armed   = 0;
                    static int    seg48_hits    = 0;
                    if (seg48_t0 == 0)
                        seg48_t0 = time(NULL);
                    if (!seg48_armed && (time(NULL) - seg48_t0) >= 100)
                        seg48_armed = 1;
                    if (seg48_armed && (seg48_hits < 4000) && (CS == 0x0048)
                        && (cpu_state.pc >= 0x0250) && (cpu_state.pc <= 0x0680)) {
                        seg48_hits++;
                        uint32_t base = ((uint32_t) CS) << 4;
                        fprintf(stderr,
                                "[seg0048trace2] #%d t+%llds PC=%04X AX=%04X BX=%04X CX=%04X DX=%04X "
                                "SI=%04X DI=%04X SP=%04X BP=%04X DS=%04X ES=%04X flags=%04X bytes=",
                                seg48_hits, (long long) (time(NULL) - seg48_t0), cpu_state.pc,
                                AX, BX, CX, DX, SI, DI, SP, BP, ds, es, cpu_state.flags);
                        for (int rb = 0; rb < 8; rb++)
                            fprintf(stderr, "%02X ", readmemb(base, cpu_state.pc + rb));
                        fprintf(stderr, "\n");
                        fflush(stderr);
                    }
                }

                /* [segidtrace2] 2026-08-02: after [seg0048trace2] confirmed the new CONFIG.SYS-
                   stage stall (segment 0048 does NOT go through INBRDPC.SYS's INT 06h API this
                   time), the modecheck heartbeat showed CS:PC cycling among a growing-then-
                   plateauing set of segments (F000/C000/D000/857D/024D/8DD2/020B/1BD3/0E77/FF33/
                   1832/8D66/191F/0048/1128 - stable for 90+s after t+204s, matching Technique 13's
                   "genuine bounded loop" signature). 191F and 1128 are new, never seen in
                   yesterday's stuck run - identify them the same way Technique 16 identified
                   segment 0048 (dump 64KB via readmemb, which goes through real page-table
                   translation, then extract ASCII strings offline) rather than guessing from the
                   segment number alone. One-shot per segment, first entry only. */
                {
                    static int dumped_191f = 0;
                    static int dumped_1128 = 0;
                    if (!dumped_191f && (CS == 0x191F)) {
                        dumped_191f = 1;
                        FILE *df = fopen("seg191F_dump.bin", "wb");
                        if (df) {
                            uint32_t base = ((uint32_t) CS) << 4;
                            for (uint32_t off = 0; off < 0x10000; off++)
                                fputc(readmemb(base, off), df);
                            fclose(df);
                            fprintf(stderr, "[segidtrace2] dumped 64KB of segment 191F to seg191F_dump.bin at PC=%04X\n", cpu_state.pc);
                            fflush(stderr);
                        }
                    }
                    if (!dumped_1128 && (CS == 0x1128)) {
                        dumped_1128 = 1;
                        FILE *df = fopen("seg1128_dump.bin", "wb");
                        if (df) {
                            uint32_t base = ((uint32_t) CS) << 4;
                            for (uint32_t off = 0; off < 0x10000; off++)
                                fputc(readmemb(base, off), df);
                            fclose(df);
                            fprintf(stderr, "[segidtrace2] dumped 64KB of segment 1128 to seg1128_dump.bin at PC=%04X\n", cpu_state.pc);
                            fflush(stderr);
                        }
                    }
                    /* 2026-08-02: [himemcaller] found segment 0F86 (offset 0x0B31) repeatedly
                       firing an INT 2Fh AX=0x1213 request that HIMEM.SYS's multiplex handler
                       (AH=0x43 check) doesn't even recognize as its own - identify what 0F86
                       actually is, since it - not HIMEM - is the component doing the repeating. */
                    static int dumped_0f86 = 0;
                    if (!dumped_0f86 && (CS == 0x0F86)) {
                        dumped_0f86 = 1;
                        FILE *df = fopen("seg0F86_dump.bin", "wb");
                        if (df) {
                            uint32_t base = ((uint32_t) CS) << 4;
                            for (uint32_t off = 0; off < 0x10000; off++)
                                fputc(readmemb(base, off), df);
                            fclose(df);
                            fprintf(stderr, "[segidtrace2] dumped 64KB of segment 0F86 to seg0F86_dump.bin at PC=%04X\n", cpu_state.pc);
                            fflush(stderr);
                        }
                    }
                    /* 2026-08-02: segment 0E77 is where [a20trace] caught the live, real-mode
                       OUT 60,DF/DD + OUT 64,FF/D1 toggle loop - identify it the same way, since
                       it's real-mode/V86 code (a plain segment:offset CS value), not VKD_INBOARD.VXD
                       (which is protected-mode, flat-selector, and cannot execute with CS=0E77). */
                    static int dumped_0e77 = 0;
                    if (!dumped_0e77 && (CS == 0x0E77)) {
                        dumped_0e77 = 1;
                        FILE *df = fopen("seg0E77_dump.bin", "wb");
                        if (df) {
                            uint32_t base = ((uint32_t) CS) << 4;
                            for (uint32_t off = 0; off < 0x10000; off++)
                                fputc(readmemb(base, off), df);
                            fclose(df);
                            fprintf(stderr, "[segidtrace2] dumped 64KB of segment 0E77 to seg0E77_dump.bin at PC=%04X\n", cpu_state.pc);
                            fflush(stderr);
                        }
                    }
                }

                /* [seg191Ftrace] 2026-08-02: [segidtrace2] identified segment 191F as containing
                   the live SYSTEM.INI [386Enh] text (device=*vxdname lines) - this is Windows 95's
                   own startup code reading its VxD device list. Revised same session: a read-only
                   pyfatfs check of VMM32.VXD's file size (688,825 bytes, matching the known
                   real-combine size) confirmed the combine already happened before this session
                   ever started - this is genuine POST-combine startup code, not combine prep.
                   CS:PC cycles between 0048 and 191F (plus BIOS/option-ROM
                   segments) with no new segments for 90+s - trace the actual code path inside 191F
                   itself (not range-restricted yet, since the interesting offset band here isn't
                   known the way 0048's was) to see whether it's walking forward through the
                   device= list or stuck re-reading the same one or two lines. Gated later (110s)
                   than [seg0048trace2] since this segment is reached slightly later in the cycle;
                   high cap since the working offset range is unknown. */
                {
                    static time_t seg191f_t0    = 0;
                    static int    seg191f_armed = 0;
                    static int    seg191f_hits  = 0;
                    if (seg191f_t0 == 0)
                        seg191f_t0 = time(NULL);
                    if (!seg191f_armed && (time(NULL) - seg191f_t0) >= 110)
                        seg191f_armed = 1;
                    if (seg191f_armed && (seg191f_hits < 500) && (CS == 0x191F)) {
                        seg191f_hits++;
                        uint32_t base = ((uint32_t) CS) << 4;
                        fprintf(stderr,
                                "[seg191Ftrace] #%d t+%llds PC=%04X AX=%04X BX=%04X CX=%04X DX=%04X "
                                "SI=%04X DI=%04X SP=%04X BP=%04X DS=%04X ES=%04X flags=%04X bytes=",
                                seg191f_hits, (long long) (time(NULL) - seg191f_t0), cpu_state.pc,
                                AX, BX, CX, DX, SI, DI, SP, BP, ds, es, cpu_state.flags);
                        for (int rb = 0; rb < 8; rb++)
                            fprintf(stderr, "%02X ", readmemb(base, cpu_state.pc + rb));
                        fprintf(stderr, "\n");
                        fflush(stderr);
                    }
                }

                /* [seg191Fseek] 2026-08-02: [seg191Ftrace] plus static disassembly found three live
                   INT 21h call sites inside segment 191F's hot loop: 0x1d4b (AH=08h, read keyboard
                   char without echo), 0x2e93/0x2ea6 (AH=42h LSEEK then AH=3Fh read-file, a
                   seek-then-read pair with a 32-bit file position in ECX:EDX), and 0x2f17 (a second
                   AH=42h LSEEK, followed by path/filename-parsing code). The seek+read pair is the
                   most likely candidate for "loading/decompressing a file piece by piece" (the
                   SHLD-based bitstream decoder at 0x4300+ probably unpacks whatever this reads) -
                   log the actual seek offset and requested byte count each time these fire, gated
                   the same 110s as [seg191Ftrace], to see directly whether the file position
                   genuinely advances each pass (real, if slow, progress) or resets to the same
                   value every time (a genuine restart-from-scratch retry bug). AH=08h logged too,
                   in case it's blocking on a keypress the user can't see (V86/graphics-mode
                   blindness) rather than a real spin. */
                {
                    static time_t seek_t0    = 0;
                    static int    seek_armed = 0;
                    static int    seek_hits  = 0;
                    if (seek_t0 == 0)
                        seek_t0 = time(NULL);
                    if (!seek_armed && (time(NULL) - seek_t0) >= 110)
                        seek_armed = 1;
                    if (seek_armed && (seek_hits < 900) && (CS == 0x191F)) {
                        if (cpu_state.pc == 0x2e93) {
                            seek_hits++;
                            fprintf(stderr, "[seg191Fseek] #%d t+%llds LSEEK@2e93 handle(BX)=%04X ECX:EDX=%08X:%08X AL(mode)=%02X\n",
                                    seek_hits, (long long) (time(NULL) - seek_t0), BX, ECX, EDX, AL);
                            fflush(stderr);
                        } else if (cpu_state.pc == 0x2ea6) {
                            seek_hits++;
                            fprintf(stderr, "[seg191Fseek] #%d t+%llds READ@2ea6 handle(BX)=%04X CX(bytes)=%04X DS:DX=%04X:%04X\n",
                                    seek_hits, (long long) (time(NULL) - seek_t0), BX, CX, ds, DX);
                            fflush(stderr);
                        } else if (cpu_state.pc == 0x2f17) {
                            seek_hits++;
                            fprintf(stderr, "[seg191Fseek] #%d t+%llds LSEEK@2f17 handle(BX)=%04X ECX:EDX=%08X:%08X AL(mode)=%02X\n",
                                    seek_hits, (long long) (time(NULL) - seek_t0), BX, ECX, EDX, AL);
                            fflush(stderr);
                        } else if (cpu_state.pc == 0x1d4b) {
                            seek_hits++;
                            fprintf(stderr, "[seg191Fseek] #%d t+%llds KBDWAIT@1d4b AH=08h (blocking read, no echo)\n",
                                    seek_hits, (long long) (time(NULL) - seek_t0));
                            fflush(stderr);
                        }
                    }
                }

                /* [seg191Freadcaller] 2026-08-02: exhaustive static scan (both 16-bit and 32-bit
                   relative CALL encodings, plus a brute-force per-byte-alignment capstone sweep)
                   found NO direct call anywhere in segment 191F targeting the seek+read helper at
                   0x2e82 - it must be reached via an indirect/computed call (a function-pointer
                   table, common in compiled C). Read the actual return address directly off the
                   stack instead of guessing further (Technique 1's "live evidence over static
                   guessing" principle) - for a near call, the 16-bit return offset sits at SS:SP
                   the instant execution reaches the call target, before anything is pushed/popped.
                   Also dump the next 3 stack words in case it's actually a far call (offset+segment)
                   instead. */
                {
                    static int rc_hits = 0;
                    if ((rc_hits < 100) && (CS == 0x191F) && (cpu_state.pc == 0x2e82)) {
                        rc_hits++;
                        uint32_t ssbase = ((uint32_t) ss) << 4;
                        uint16_t w0 = readmemb(ssbase, SP) | (readmemb(ssbase, SP + 1) << 8);
                        uint16_t w1 = readmemb(ssbase, SP + 2) | (readmemb(ssbase, SP + 3) << 8);
                        uint16_t w2 = readmemb(ssbase, SP + 4) | (readmemb(ssbase, SP + 5) << 8);
                        fprintf(stderr, "[seg191Freadcaller] #%d SS:SP=%04X:%04X stack[0..2]=%04X %04X %04X AX=%04X BX=%04X CX=%04X DX=%04X\n",
                                rc_hits, ss, SP, w0, w1, w2, AX, BX, CX, DX);
                        fflush(stderr);
                    }
                }

                /* [int1587] 2026-07-26 (shadow-RAM re-investigation): the two prior fix attempts
                   both guessed the location/nature of the "reference" bytes from static
                   disassembly of two DIFFERENT files (the system ROM dump and INBRDPC.SYS) and
                   never actually confirmed which one - if either - genuinely executes the
                   INT 15h AH=87h call live, nor what the real BIOS's own handler does with it.
                   Trap the ACTUAL execution generically (opcode bytes CD 15 with AH=87h in AX,
                   any CS) so the caller's true segment is known directly rather than assumed -
                   this alone will show whether the call originates from the system ROM's own
                   F000 segment or from wherever DOS loaded INBRDPC.SYS. Also dump the GDT
                   descriptor-pair structure at ES:SI (the standard AH=87h calling convention -
                   6 x 8-byte descriptors, entry 2 = source, entry 3 = destination) so the real
                   source/destination physical addresses are known, not guessed. A paired
                   one-shot watches the return point (CS:PC+2, since INT/IRET preserves CS and
                   restores IP right after the 2-byte CD 15) to see whether the real ROM's INT 15h
                   handler actually sets CF=1 (unsupported - correct AT-BIOS-absence behavior,
                   would make the whole check a no-op) or returns CF=0 (as if it succeeded without
                   doing anything, which is what would actually trigger the visible error). */
                {
                    static int      int87_hits          = 0;
                    static int      int87_watch_pending  = 0;
                    static uint32_t int87_watch_cs       = 0;
                    static uint32_t int87_watch_pc       = 0;
                    static int      int87_post_count     = 0;

                    if (int87_watch_pending && (CS == int87_watch_cs) && (cpu_state.pc == int87_watch_pc)) {
                        int87_watch_pending = 0;
                        fprintf(stderr, "[int1587] return: CS:PC=%04X:%04X flags=%04X CF=%d AX=%04X DSbase=%05X ESbase=%05X SI=%04X DI=%04X\n",
                                CS, cpu_state.pc, cpu_state.flags, (cpu_state.flags & C_FLAG) ? 1 : 0, AX, ds, es, SI, DI);
                        fprintf(stderr, "[int1587]   readback: FE05B=%02X %02X %02X   5FE05B=%02X %02X %02X\n",
                                mem_readb_phys(0xFE05B), mem_readb_phys(0xFE05C), mem_readb_phys(0xFE05D),
                                mem_readb_phys(0x5FE05B), mem_readb_phys(0x5FE05C), mem_readb_phys(0x5FE05D));
                        fflush(stderr);
                        int87_post_count = 80; /* Trace next 80 retired instructions after the return. */
                    }

                    if (int87_post_count > 0) {
                        uint32_t phys2  = (((uint32_t) CS) << 4) + cpu_state.pc;
                        uint8_t  opb0   = mem_readb_phys(phys2);
                        uint8_t  opb1   = mem_readb_phys(phys2 + 1);
                        int      is_cmps = (opb0 == 0xA6) || (opb0 == 0xA7)
                            || (((opb0 == 0xF2) || (opb0 == 0xF3)) && ((opb1 == 0xA6) || (opb1 == 0xA7)));
                        fprintf(stderr, "[int1587post] #%02d CS:PC=%04X:%04X op=%02X %02X DSbase=%05X SI=%04X ESbase=%05X DI=%04X CX=%04X flags=%04X%s\n",
                                81 - int87_post_count, CS, cpu_state.pc, opb0, opb1, ds, SI, es, DI, CX, cpu_state.flags,
                                is_cmps ? "  <-- CMPS" : "");
                        if (is_cmps) {
                            /* NOTE: ds/es (lowercase) are already-computed segment BASE addresses
                               (cpu.h: #define ds cpu_state.seg_ds.base) - do NOT shift by <<4 again,
                               unlike CS (cpu.h: #define CS cpu_state.seg_cs.seg, a raw selector). An
                               earlier version of this diagnostic wrongly shifted ds/es a second time,
                               producing a bogus destination physical address past the 1MB real-mode
                               limit that always read back as unmapped 0xFF - a self-inflicted false
                               reading, not a real finding. */
                            uint32_t src_phys = ((uint32_t) ds) + SI;
                            uint32_t dst_phys = ((uint32_t) es) + DI;
                            fprintf(stderr, "[int1587post]      CMPS operands: DSbase:SI=%05X:%04X (phys %05X, byte=%02X)  ESbase:DI=%05X:%04X (phys %05X, byte=%02X)  CX=%04X\n",
                                    ds, SI, src_phys, mem_readb_phys(src_phys), es, DI, dst_phys, mem_readb_phys(dst_phys), CX);
                        }
                        fflush(stderr);
                        int87_post_count--;
                    }

                    /* [ioscan] 2026-07-26: hunting the ~256K memory-accounting gap found via the
                       user's real-hardware photos - real INBRDPC.SYS reports "extended memory
                       detected: 4352k" (matching the Intel manual's documented "256K bytes of
                       extended memory" on the Inboard card itself, PLUS the 4096K/4MB piggyback -
                       256+4096=4352, exact match), while this project's emulator consistently
                       reports only 4096k (piggyback only, zero contribution from the onboard
                       card's own 256K). INBRDPC.SYS doesn't use the standard INT 15h AH=88h "get
                       extended memory size" call (checked: not present in the file at all) - it
                       must determine the total some other way, most likely a real I/O port read
                       on the Inboard card itself (this device already handles ports 0x60/0x64/
                       0xA0/0x670/0x674 - if there's a port for reported memory *size* specifically,
                       it isn't modeled). Trace all IN/OUT activity from CS in the typical low-
                       DOS-driver segment range (0x0100-0x0500, where INBRDPC.SYS itself loads in
                       this test config - excludes the already-separately-traced F000/C000 ROM
                       segments) to find it, capped tightly since this fires during ordinary driver
                       execution, not just one routine. */
                    {
                        static int ioscan_hits = 0;
                        if ((ioscan_hits < 80) && (CS >= 0x0100) && (CS <= 0x0500)) {
                            uint32_t phys3 = (((uint32_t) CS) << 4) + cpu_state.pc;
                            uint8_t  opb   = mem_readb_phys(phys3);
                            if ((opb == 0xE4) || (opb == 0xE5) || (opb == 0xEC) || (opb == 0xED)
                                || (opb == 0xE6) || (opb == 0xE7) || (opb == 0xEE) || (opb == 0xEF)) {
                                ioscan_hits++;
                                uint8_t imm = mem_readb_phys(phys3 + 1);
                                fprintf(stderr, "[ioscan] #%d CS:PC=%04X:%04X op=%02X imm8=%02X DX=%04X AX=%04X\n",
                                        ioscan_hits, CS, cpu_state.pc, opb, imm, DX, AX);
                                fflush(stderr);
                            }
                        }
                    }

                    /* [ramaddr] 2026-07-26: hunting why this project's emulated ATI self-test shows
                       the extended "RAM Addressing" diagnostic forever (an ever-incrementing counter,
                       matching a genuine non-terminating retry loop) while the user's real hardware
                       (confirmed running the byte-identical real ROM) only ever shows instant
                       "Testing.......Ok". Disassembled the real ROM's own pattern-verify loop
                       (0x76AE-0x76D2: writes a test pattern via port 0xBAE8 as a register-select,
                       then repeatedly reads back port 0xE2E8 comparing against the pattern (AAAA/
                       A5A5/5555 in turn) and separately polls the same port for a status bit to
                       clear (0x76C7-0x76D0, CX-bounded retry, structurally identical in shape to the
                       PIT-readback delay loop already fixed earlier this session for a different
                       routine) - trace the actual port values this project's emulator produces at
                       the read-back point (0x76C7, right before the CX-bounded compare-against-zero)
                       to see whether it's a genuine status-bit-never-clears problem, the same class
                       of bug as the already-fixed PIT-readback issue, or something else entirely. */
                    {
                        static int ramaddr_hits = 0;
                        if ((ramaddr_hits < 200) && (CS == 0xC000) && (cpu_state.pc == 0x76C7)) {
                            ramaddr_hits++;
                            fprintf(stderr, "[ramaddr] #%d CS:PC=%04X:%04X DX=%04X AX(pre-in)=%04X CX=%04X flags=%04X\n",
                                    ramaddr_hits, CS, cpu_state.pc, DX, AX, CX, cpu_state.flags);
                            fflush(stderr);
                        }
                    }

                    /* [ramaddr2] 2026-07-26, continued: [ramaddr] only covered the CX-bounded retry
                       loop (0x76C7), which the earlier trace proved is clean (24/24, no errors). Fresh
                       disassembly of the real ROM dump (XT_project/ATI_MACH8.bin) found the actual
                       error-bit source is upstream of that loop: 0x76AE/0x76BA are the two INITIAL
                       compares for each of the three AAAA/A5A5/5555 patterns (write via port 0xE2E8,
                       read back same port, XOR against the expected pattern in SI) - a mismatch here
                       calls 0x7618 to OR error bits into BL, and 0x7729's `test bx,0xff` is what
                       actually gates printing "RAM Addressing" (file offset 0x772F). Trace both compare
                       points plus the final gate to find exactly which pattern (if any) mismatches. */
                    {
                        static int ramaddr2_hits = 0;
                        if ((ramaddr2_hits < 60) && (CS == 0xC000) &&
                            ((cpu_state.pc == 0x76B1) || (cpu_state.pc == 0x76BD) || (cpu_state.pc == 0x7729))) {
                            ramaddr2_hits++;
                            fprintf(stderr, "[ramaddr2] #%d CS:PC=%04X:%04X AX(post-xor)=%04X SI=%04X BX=%04X CX=%04X DX=%04X flags=%04X\n",
                                    ramaddr2_hits, CS, cpu_state.pc, AX, SI, BX, CX, DX, cpu_state.flags);
                            fflush(stderr);
                        }
                    }

                    if ((int87_hits < 8) && !int87_watch_pending) {
                        uint32_t phys = (((uint32_t) CS) << 4) + cpu_state.pc;
                        if ((mem_readb_phys(phys) == 0xCD) && (mem_readb_phys(phys + 1) == 0x15) && ((AX >> 8) == 0x87)) {
                            uint32_t gdt_phys = ((uint32_t) es) + SI;
                            int87_hits++;
                            fprintf(stderr, "[int1587] call #%d: CS:PC=%04X:%04X AX=%04X BX=%04X CX=%04X DX=%04X DSbase=%05X ESbase=%05X SI=%04X DI=%04X GDTphys=%05X\n",
                                    int87_hits, CS, cpu_state.pc, AX, BX, CX, DX, ds, es, SI, DI, gdt_phys);
                            for (int gi = 0; gi < 48; gi += 8) {
                                fprintf(stderr, "[int1587]   desc[%d] (phys %05X): %02X %02X %02X %02X %02X %02X %02X %02X\n",
                                        gi / 8, gdt_phys + gi,
                                        mem_readb_phys(gdt_phys + gi + 0), mem_readb_phys(gdt_phys + gi + 1),
                                        mem_readb_phys(gdt_phys + gi + 2), mem_readb_phys(gdt_phys + gi + 3),
                                        mem_readb_phys(gdt_phys + gi + 4), mem_readb_phys(gdt_phys + gi + 5),
                                        mem_readb_phys(gdt_phys + gi + 6), mem_readb_phys(gdt_phys + gi + 7));
                            }
                            {
                                uint32_t src_base = mem_readb_phys(gdt_phys + 16 + 2)
                                    | (((uint32_t) mem_readb_phys(gdt_phys + 16 + 3)) << 8)
                                    | (((uint32_t) mem_readb_phys(gdt_phys + 16 + 4)) << 16);
                                uint32_t dst_base = mem_readb_phys(gdt_phys + 24 + 2)
                                    | (((uint32_t) mem_readb_phys(gdt_phys + 24 + 3)) << 8)
                                    | (((uint32_t) mem_readb_phys(gdt_phys + 24 + 4)) << 16);
                                fprintf(stderr, "[int1587]   decoded: src_base=%06X dst_base=%06X (CX=%04X words = %04X bytes)\n",
                                        src_base, dst_base, CX, CX * 2);
                            }
                            fflush(stderr);
                            int87_watch_pending = 1;
                            int87_watch_cs      = CS;
                            int87_watch_pc      = (uint32_t) ((cpu_state.pc + 2) & 0xFFFF);
                        }
                    }
                }

                if ((CS == 0xF000) && (cpu_state.pc == 0xE3A0))
                    seen_e3a0 = 1;

                if (seen_e3a0 && !dumped_second && (CS == 0xF000) && (cpu_state.pc == 0xE354)) {
                    dumped_second = 1;
                    fprintf(stderr, "[ring] *** reached E354 again after E3A0 - dumping last 1048576 (CS:PC) ***\n");
                    for (int i = 0; i < 1048576; i++) {
                        int idx = (ring_pos + i) % 1048576;
                        fprintf(stderr, "[ring] %04X:%04X\n", ring_cs[idx], ring_pc[idx]);
                    }
                    fflush(stderr);
                }

                /* [shadowfinal] 2026-07-26: real-hardware investigation via comrade proved the
                   user's actual system ROM chip is byte-identical to this project's bundled
                   ibm5160_050986 dump (confirmed by physically disabling shadow-for-reads on the
                   live machine via port 0x670 and reading the raw chip: F000:E05B = FA B4 D5,
                   matching exactly) - the earlier "different ROM revision" conclusion was wrong,
                   caused by reading through the ALREADY-CORRECTED live shadow copy (which read
                   EA F5 0B, matching INBRDPC.SYS's own hardcoded reference at file offset 0x2C6).
                   The user has never seen the "shadow RAM failed" message on real hardware, so the
                   real question isn't ROM content - it's whether the shadow copy gets correctly
                   populated to match the reference by the time boot completes here too, the same
                   way it does on real hardware. One-shot: the instant "C:\>" (COMRADE's own final
                   prompt) appears in video RAM, dump what F000:E05B currently reads (through the
                   live shadow mapping, same address/method used against the real machine). */
                {
                    static int shadowfinal_dumped = 0;
                    if (!shadowfinal_dumped) {
                        uint8_t sc0 = mem_readb_phys(0xB8000);
                        uint8_t sc1 = mem_readb_phys(0xB8002);
                        uint8_t sc2 = mem_readb_phys(0xB8004);
                        if ((sc0 == 'C') && (sc1 == ':') && (sc2 == '\\')) {
                            shadowfinal_dumped = 1;
                            fprintf(stderr, "[shadowfinal] C:\\> reached - F000:E05B = %02X %02X %02X (real hardware, shadow enabled, post-boot: EA F5 0B; real hardware, shadow disabled, raw chip: FA B4 D5)\n",
                                    mem_readb_phys(0xFE05B), mem_readb_phys(0xFE05C), mem_readb_phys(0xFE05D));
                            fflush(stderr);
                        }
                    }
                }

                /* One-shot: the instant "1801" (the still-unexplained false expansion-unit
                   POST error) appears in CGA text video RAM (B8000, standard 2-bytes-per-char
                   char+attribute layout, top-left of screen per the known screenshot), dump the
                   ring buffer of the last 1048576 executed (CS,PC) pairs - this gives the exact
                   code path that led to the write, without needing to guess which BIOS routine
                   is responsible from static disassembly alone. */
                {
                    static int dumped_1801 = 0;
                    if (!dumped_1801) {
                        uint8_t c0 = mem_readb_phys(0xB8000);
                        uint8_t c1 = mem_readb_phys(0xB8002);
                        uint8_t c2 = mem_readb_phys(0xB8004);
                        uint8_t c3 = mem_readb_phys(0xB8006);
                        if ((c0 == '1') && (c1 == '8') && (c2 == '0') && (c3 == '1')) {
                            dumped_1801 = 1;
                            fprintf(stderr, "[ring1801] *** \"1801\" now visible in video RAM at CS:PC=%04X:%04X - dumping last 1048576 (CS:PC) ***\n", CS, cpu_state.pc);
                            for (int i = 0; i < 1048576; i++) {
                                int idx = (ring_pos + i) % 1048576;
                                fprintf(stderr, "[ring1801] %04X:%04X\n", ring_cs[idx], ring_pc[idx]);
                            }
                            fflush(stderr);
                        }
                    }
                }

                /* One-shot: the instant "BAD" first appears anywhere in CGA text video RAM
                   (the Inboard 386/PC chip-diagnostics results screen marking a failed
                   position), dump the ring buffer of the last 1048576 executed (CS,PC) pairs.
                   Same technique that found the real "1801" test - used here because manual
                   address-guessing from static disassembly (multiple structurally-similar
                   LGDT/MOV-CR0 blocks, ~12 of them) missed the actual executed path twice in a
                   row. Gated to check only once/second (string-scanning 2000 bytes every single
                   instruction would be needlessly expensive). */
                {
                    static int dumped_bad     = 0;
                    static int bad_check_ctr  = 0;
                    if (!dumped_bad && (++bad_check_ctr >= 20000)) {
                        bad_check_ctr = 0;
                        for (int pos = 0; pos < 2000 - 3; pos++) {
                            uint32_t addr = 0xB8000u + (uint32_t) pos * 2u;
                            if ((mem_readb_phys(addr) == 'B') && (mem_readb_phys(addr + 2) == 'A') && (mem_readb_phys(addr + 4) == 'D')) {
                                dumped_bad = 1;
                                fprintf(stderr, "[ringbad] *** \"BAD\" now visible in video RAM at cell %d, CS:PC=%04X:%04X - dumping last 1048576 (CS:PC) ***\n", pos, CS, cpu_state.pc);
                                for (int i = 0; i < 1048576; i++) {
                                    int idx = (ring_pos + i) % 1048576;
                                    fprintf(stderr, "[ringbad] %04X:%04X\n", ring_cs[idx], ring_pc[idx]);
                                }
                                fflush(stderr);
                                break;
                            }
                        }
                    }
                }

                /* Same technique, targeting "Contact Intel" - the ROM BIOS shadow RAM
                   failure message ("The Inboard 386/PC's ROM BIOS shadow RAM failed. /
                   Contact Intel Customer Support..."). Two content-population fixes to
                   inboard386.c's shadow buffer (pre-populate at init, then a full
                   read/write-asymmetric rework matching UniPCemu's mapmemoryROM() model)
                   both left this message unchanged - so, same as the RAM-diagnostic
                   investigation, stop guessing from static disassembly and find the
                   actual executed check directly. */
                {
                    static int shadowfail_dumped = 0;
                    static int shadowfail_ctr    = 0;
                    if (!shadowfail_dumped && (++shadowfail_ctr >= 20000)) {
                        shadowfail_ctr = 0;
                        for (int pos = 0; pos < 2000 - 11; pos++) {
                            uint32_t addr = 0xB8000u + (uint32_t) pos * 2u;
                            if ((mem_readb_phys(addr) == 's') && (mem_readb_phys(addr + 2) == 'h')
                                && (mem_readb_phys(addr + 4) == 'a') && (mem_readb_phys(addr + 6) == 'd')
                                && (mem_readb_phys(addr + 8) == 'o') && (mem_readb_phys(addr + 10) == 'w')) {
                                shadowfail_dumped = 1;
                                fprintf(stderr, "[ringshadow] *** \"shadow\"(RAM failed) now visible in video RAM at cell %d, CS:PC=%04X:%04X - previous unique CS:PC=%04X:%04X - dumping last 1048576 (CS:PC) ***\n",
                                        pos, CS, cpu_state.pc, ring_last_cs, ring_last_pc);
                                for (int i = 0; i < 1048576; i++) {
                                    int idx = (ring_pos + i) % 1048576;
                                    fprintf(stderr, "[ringshadow] %04X:%04X\n", ring_cs[idx], ring_pc[idx]);
                                }
                                fflush(stderr);
                                break;
                            }
                        }
                    }
                }

                /* Pin down exactly which of the three checks in the E362-E385 PIC-IMR
                   self-test (1986 ROM addresses) fails when the Mach8 card is present, now
                   that the PIT fix has zero interrupt side effects - if this STILL fails,
                   it's the ATI ROM's own PIC manipulation leaving real, non-timing-related
                   state behind, not a backlogged-interrupt artifact. */
                if ((CS == 0xF000) && ((cpu_state.pc == 0xE368) || (cpu_state.pc == 0xE372) || (cpu_state.pc == 0xE380))) {
                    static int imr_trace_count = 0;
                    if (imr_trace_count < 30) {
                        imr_trace_count++;
                        fprintf(stderr, "[imrcheck] #%d PC=%04X AL=%02X byte[46B]=%02X\n",
                                imr_trace_count, cpu_state.pc, AL, mem_readb_phys(0x46B));
                        fflush(stderr);
                    }
                }

                /* Root cause of "101" (2026-07-26), confirmed via [imrcheck]: unrelated to
                   the PIT fix above (which by this point generates no interrupts at all) -
                   the keyboard controller's own universal, non-Mach8-specific self-test
                   completion byte (kbc_xt.c, sent ~1ms after machine start via a real,
                   TIMER_USEC-paced delay - fires on every single boot, on every machine)
                   raises IRQ1 while this BIOS still has interrupts masked this early in
                   POST, exactly the same way IRQ0 backlogs were shown to. F000:E362's own
                   test happens to be the *first* point in this specific BIOS's POST that
                   unmasks IRQs at all, so it's also the first point a real, waiting IRQ1
                   ever gets to land - onto the shared "unexpected interrupt" IVT stub this
                   self-test also depends on, contaminating its shared status byte before the
                   test's own IRQ0-specific logic gets a chance to run cleanly. IRQ0 needs to
                   go through untouched (the very next test, E38F-E3AC, depends on it) - only
                   suppress IRQ1 specifically, only across that first two-test window.
                   A third test immediately follows (F000:E3AE-E3C6): reloads channel 0 with
                   a large (0xFF) count and polls the SAME shared status byte for only 12
                   iterations expecting it to STAY clear - i.e. the inverse of the previous
                   test, verifying IRQ0 does NOT fire prematurely. Live-traced (2026-07-26):
                   this one gets a *genuine* IRQ0 within its tiny 12-iteration window - not a
                   backlog, a real one, because this project's waitstate throttling makes 12
                   loop iterations take disproportionately more real/guest PIT-tick time than
                   the "12 iterations = microseconds" assumption this test was written
                   against, letting the newly-armed counter legitimately reach terminal count
                   before the loop exhausts. Since this test *wants* no interrupt at all,
                   suppressing both IRQ0 and IRQ1 here is exactly correct, not a workaround. */
                /* Moved into the shared inboard_post_fixups() above (PR #7749), where it also
                   gained the E3AD exit address and the range-based safety net that disarms the
                   gate if execution leaves E362-E3C6 by any unenumerated path. The
                   [irq1suppress]/[irq01suppress] stderr lines went with it. */

                /* Third and (per static E8-scan of every caller of the shared F000:E387
                   print+halt routine) final self-test sharing that same failure path:
                   F000:E4D5-E511, a memory-refresh verification - reads the 8237 DMA
                   controller's status register (port 8, read-and-clear semantics per the
                   8237 datasheet) and requires bit 0 (channel 0/refresh reached terminal
                   count) to be set. Channel 0's refresh is driven by PIT channel 1, already
                   confirmed correctly programmed and ticking (this project's very first,
                   already-working channel-1 self-test, E0E1-E103, passes identically on
                   both CGA and Mach8). The failure here is the same shape as the first two:
                   something reads port 8 between the last real refresh cycle and this check,
                   consuming (clearing) the flag this read-and-clear register can only report
                   once. Same zero-side-effect fix as the C000:7B37 delay loop above - force
                   the bit the guest's own AND/JNE checks, rather than chase why the read
                   got consumed upstream. */
                /* Moved into the shared inboard_post_fixups() above (PR #7749); the [e507fix]
                   stderr line went with it. */

                /* Same technique, targeting the "101" POST error reappearing with the ATI
                   Mach8 card present (2026-07-26) - this project's earlier "101" fix
                   (PIC-IMR/IRQ0/DMA-refresh, 2026-07-24) already confirmed zero [ring1801]
                   triggers on the clean CGA baseline, so this is either the option ROM
                   leaving PIC/DMA state disturbed for that already-fixed check to
                   legitimately re-trip, or an entirely different check that also happens to
                   print "101" - find out which instead of guessing. */
                {
                    static int err101_dumped = 0;
                    static int err101_ctr    = 0;
                    if (!err101_dumped && (++err101_ctr >= 20000)) {
                        err101_ctr = 0;
                        for (int pos = 0; pos < 2000 - 3; pos++) {
                            uint32_t addr = 0xB8000u + (uint32_t) pos * 2u;
                            if ((mem_readb_phys(addr) == '1') && (mem_readb_phys(addr + 2) == '0') && (mem_readb_phys(addr + 4) == '1')) {
                                err101_dumped = 1;
                                fprintf(stderr, "[ring101] *** \"101\" now visible in video RAM at cell %d, CS:PC=%04X:%04X - dumping last 1048576 (CS:PC) ***\n", pos, CS, cpu_state.pc);
                                for (int i = 0; i < 1048576; i++) {
                                    int idx = (ring_pos + i) % 1048576;
                                    fprintf(stderr, "[ring101] %04X:%04X\n", ring_cs[idx], ring_pc[idx]);
                                }
                                fflush(stderr);
                                break;
                            }
                        }
                    }
                }

                /* One-shot, wall-clock-timed dump of the CGA text video buffer (B8000, 80x25,
                   2 bytes/cell: char, attribute) as plain ASCII - to check whether the garbled
                   "vertical stripes" seen on screen after the enable_5161 fix is a genuine CGA
                   rendering/timing artifact (real characters present, just mis-rendered) or an
                   actual data-level corruption (garbage characters really are in video RAM).
                   Triggered ~20 real seconds after process start, once, regardless of CS:PC. */
                {
                    static time_t t0        = 0;
                    static time_t last_dump = 0;
                    if (t0 == 0)
                        t0 = time(NULL);
                    time_t now = time(NULL);
                    if ((now - t0 >= 5) && (now != last_dump)) {
                        last_dump = now;
                        FILE *f = fopen("vram_dump.txt", "ab");
                        if (f) {
                            fprintf(f, "=== t+%lds ===\n", (long) (now - t0));
                            for (int row = 0; row < 25; row++) {
                                for (int col = 0; col < 80; col++) {
                                    uint32_t addr = 0xB8000u + (uint32_t) (row * 80 + col) * 2u;
                                    uint8_t  ch   = mem_readb_phys(addr);
                                    if (ch < 0x20 || ch > 0x7E)
                                        ch = '.';
                                    fputc(ch, f);
                                }
                                fputc('\n', f);
                            }
                            fclose(f);
                            fprintf(stderr, "[vramdump] appended snapshot t+%lds\n", (long) (now - t0));
                            fflush(stderr);
                        }
                        /* Companion check (2026-07-26, Mach8 investigation): the B8000 text
                           dump above went static for 5+ real minutes once the ATI BIOS's own
                           "RAM Addressing" self-test banner appeared, while the SDL window
                           simultaneously shrank from the CGA baseline's 969x644 down to
                           326x429 and rendered solid black - consistent with a real mode
                           switch out of B8000-compatible text mode (the B8000 probe going
                           blind), not necessarily a genuine CPU spin. Settle it directly:
                           log current CS:PC (proves whether execution is still moving at
                           all) plus a cheap running XOR checksum of the first 16KB of the
                           VGA graphics aperture (0xA0000) each tick (proves whether *anything*
                           is still being written there, i.e. real ongoing graphics-mode
                           activity vs a true idle/spin). */
                        {
                            uint32_t chk = 0;
                            for (uint32_t a = 0; a < 0x4000; a++)
                                chk ^= ((uint32_t) mem_readb_phys(0xA0000u + a)) << (a & 7);
                            fprintf(stderr, "[modecheck] t+%lds CS:PC=%04X:%04X A0000_xor16k=%08X ring_last=%04X:%04X oldpc=%04X\n",
                                    (long) (now - t0), CS, cpu_state.pc, chk, ring_last_cs, ring_last_pc, cpu_state.oldpc);
                            fflush(stderr);
                        }
                        /* [kbdbuf] 2026-08-02: end-of-session finding - typing directly into the
                           VM while it's in the stuck state produced a BIOS "keyboard buffer full"
                           beep (classic behavior when the 16-entry ring buffer at 0040:001E-003D
                           overflows because nothing drains it via INT 16h). Read the BIOS Data
                           Area's own head/tail pointers each second alongside [modecheck] - if
                           they're genuinely stuck at a fixed, full-buffer relationship
                           (tail+2==head, mod 0x1E offset range, per the standard IBM BDA layout)
                           for the whole stall window, that directly confirms "input piles up,
                           nothing calls INT 16h to read it" as opposed to a one-off/transient
                           beep. 0040:001A=buffer head (next char to read), 0040:001C=buffer tail
                           (next free slot) - real-mode BIOS data area, always identity-mapped
                           low memory, so mem_readb_phys is the right tool here (same as the
                           B8000 dump above), not readmemb. */
                        {
                            uint8_t head_lo = mem_readb_phys(0x41A);
                            uint8_t head_hi = mem_readb_phys(0x41B);
                            uint8_t tail_lo = mem_readb_phys(0x41C);
                            uint8_t tail_hi = mem_readb_phys(0x41D);
                            uint8_t start_lo = mem_readb_phys(0x480);
                            uint8_t start_hi = mem_readb_phys(0x481);
                            uint8_t end_lo   = mem_readb_phys(0x482);
                            uint8_t end_hi   = mem_readb_phys(0x483);
                            uint16_t head = head_lo | (head_hi << 8);
                            uint16_t tail = tail_lo | (tail_hi << 8);
                            uint16_t bufstart = start_lo | (start_hi << 8);
                            uint16_t bufend   = end_lo | (end_hi << 8);
                            fprintf(stderr, "[kbdbuf] t+%lds head(041A)=%04X tail(041C)=%04X bufstart(0480)=%04X bufend(0482)=%04X\n",
                                    (long) (now - t0), head, tail, bufstart, bufend);
                            fflush(stderr);
                        }
                        /* [irq1state] 2026-08-04: OSR1 Startup-Menu-freeze investigation - kbdbuf
                           above stays permanently EMPTY (head==tail==bufstart) through this whole
                           stall, and zero port 0x60/61/64 I/O of any kind occurs after the driver-
                           load-time A20 burst ends (~t+61s) - meaning no scancode, real OR injected
                           via inject_key.txt, is reaching the BIOS buffer at all. Leading theory:
                           IRQ1 is masked at the PIC (or IF=0 globally) at this point, so the 8042's
                           IRQ1 line never reaches the CPU to run the int09 ISR that would fill the
                           buffer. Log IMR/IRR/ISR/int_pending + the CPU's own IF flag once/second
                           starting at t+100s (comfortably before the freeze begins) to test this
                           directly - ground truth over more guessing. Capped so a fast/normal boot
                           doesn't get flooded. */
                        if ((now - t0) >= 100) {
                            static int irq1state_hits = 0;
                            if (irq1state_hits < 2000) {
                                irq1state_hits++;
                                fprintf(stderr, "[irq1state] t+%lds imr=%02X irr=%02X isr=%02X intp=%d IF=%d\n",
                                        (long) (now - t0), pic.imr, pic.irr, pic.isr, pic.int_pending,
                                        (cpu_state.flags & I_FLAG) ? 1 : 0);
                                fflush(stderr);
                            }
                        }
                    }
                }

                /* [seg650Btrace] 2026-08-02: live Technique 12 trace of segment 650B, the
                   confirmed real-hardware-matching "flashing cursor" stall (see memory
                   xt_650B_real_stall_confirmed_2026_08_02.md). Arms the first time CS becomes
                   0x650B, then logs every *distinct* CS:PC visited (Technique 1/12's
                   consecutive-dedup approach) with a full register dump, capped to avoid
                   flooding on a genuine tight spin. Remove once root-caused. */
                {
                    static int      seg650b_armed      = 0;
                    static uint32_t seg650b_last_logged = 0xFFFFFFFFu;
                    static int      seg650b_hits        = 0;
                    if (!seg650b_armed && CS == 0x650B)
                        seg650b_armed = 1;
                    if (seg650b_armed && CS == 0x650B && seg650b_hits < 2000) {
                        uint32_t here = ((uint32_t) CS << 16) | cpu_state.pc;
                        if (here != seg650b_last_logged) {
                            seg650b_last_logged = here;
                            /* CR0.PG is set (confirmed live) - CS is a protected-mode selector,
                               not a real-mode segment, so the linear address needs the resolved
                               descriptor base (cpu_state.seg_cs.base), not CS<<4 - and the actual
                               bytes must go through readmemb (page-table-aware), not
                               mem_readb_phys, per Technique 16's own caveat. */
                            uint32_t linaddr = cpu_state.seg_cs.base + cpu_state.pc;
                            uint8_t  b0 = readmemb(linaddr, 0);
                            uint8_t  b1 = readmemb(linaddr, 1);
                            uint8_t  b2 = readmemb(linaddr, 2);
                            uint8_t  b3 = readmemb(linaddr, 3);
                            FILE *f = fopen("seg650b_trace.txt", "a");
                            if (f) {
                                fprintf(f,
                                        "[seg650Btrace] #%d CS:PC=%04X:%04X lin=%08X bytes=%02X %02X %02X %02X "
                                        "EAX=%08X EBX=%08X ECX=%08X EDX=%08X ESI=%08X EDI=%08X "
                                        "ESP=%08X EBP=%08X CR0=%08X\n",
                                        seg650b_hits, CS, cpu_state.pc, linaddr, b0, b1, b2, b3,
                                        EAX, EBX, ECX, EDX, ESI, EDI, ESP, EBP,
                                        cpu_state.CR0);
                                fclose(f);
                            }
                            seg650b_hits++;
                        }
                    }
                }

                /* One-shot: dump the live INT 15h vector (0000:0054, 4 bytes: offset then
                   segment) once BIOS POST has had time to initialize the IVT, plus a live
                   ROM dump of the whole F000 segment at the same moment - so the actual
                   INT 15h handler entry point can be disassembled offline. Needed to
                   settle, with real evidence rather than a plausible guess, whether this
                   1982 ROM's INT 15h handler explicitly sets CF=1 for an unrecognized
                   AH (e.g. AH=0x87, the AT-era "copy extended memory" function INBRDPC.SYS
                   calls as part of its ROM-shadow verification - see
                   INBOARD_86BOX_PORT_PLAN.md 2026-07-26) or leaves flags untouched via a
                   bare IRET, which is the working theory for why that verification
                   spuriously "succeeds" without actually copying anything. */
                {
                    static int    int15_dumped = 0;
                    static time_t int15_t0     = 0;
                    if (int15_t0 == 0)
                        int15_t0 = time(NULL);
                    if (!int15_dumped && (time(NULL) - int15_t0 >= 15)) {
                        int15_dumped = 1;
                        uint16_t vec_off = mem_readw_phys(0x54);
                        uint16_t vec_seg = mem_readw_phys(0x56);
                        fprintf(stderr, "[int15vec] INT 15h vector = %04X:%04X\n", vec_seg, vec_off);
                        fflush(stderr);
                        FILE *f = fopen("bios_f000_dump_int15.bin", "wb");
                        if (f) {
                            for (uint32_t a = 0; a < 0x10000; a++) {
                                uint8_t b = mem_readb_phys(0xF0000u + a);
                                fwrite(&b, 1, 1, f);
                            }
                            fclose(f);
                            fprintf(stderr, "[int15vec] wrote bios_f000_dump_int15.bin\n");
                            fflush(stderr);
                        }
                    }
                }

                /* File-based keystroke injection channel - OS-level SendInput/SendKeys
                   were both confirmed (2026-07-24/25) to not reach 86Box's SDL keyboard
                   handling (a known limitation, not yet root-caused - possibly synthetic/
                   injected input being filtered). Since we have source access, drive
                   86Box's own internal keyboard_input() UI-layer entry point directly
                   instead: poll (at most once/second, cheap) for a small text file
                   ("inject_key.txt", containing a decimal XT scancode on one line),
                   inject it as a make+break pair, then delete the file so external
                   tooling can drop a new one whenever a keypress is needed. */
                {
                    /* [injectfix] 2026-08-04: root-caused why inject_key.txt keystrokes were
                       logged as "sent" but never reached the guest (confirmed via the OSR1
                       Startup-Menu-freeze investigation - zero port 60/61/64 I/O and a
                       permanently empty BIOS keyboard buffer followed every injection).
                       keyboard_input() in keyboard.c only calls the real key_process() (the
                       thing that actually drives the emulated 8042/scancode hardware) when
                       `override_capture || mouse_capture || !kbd_req_capture || (fullscreen
                       stuff)` - i.e. only once the VM window has "captured" keyboard focus.
                       Headless/scripted runs (no window click-to-capture ever happens) fail
                       this gate silently: recv_key_ui[] gets updated but key_process() never
                       runs. keyboard_toggle_override() (keyboard.c:142) flips the same static
                       override_capture flag the interactive "force capture" hotkey uses - call
                       it once, here, so file-based injection works the same headless as it does
                       interactively. */
                    static int override_capture_done = 0;
                    if (!override_capture_done) {
                        override_capture_done = 1;
                        keyboard_toggle_override();
                        fprintf(stderr, "[injectfix] called keyboard_toggle_override() once to "
                                        "bypass the capture-focus gate for headless key injection\n");
                        fflush(stderr);
                    }
                    static time_t last_key_check = 0;
                    time_t        key_now        = time(NULL);
                    if (key_now != last_key_check) {
                        last_key_check = key_now;
                        FILE *kf = fopen("inject_key.txt", "r");
                        if (kf) {
                            /* Optional leading 's' = hold Shift (0x2A) around this scancode, for
                               shifted characters (colon, etc.) the plain make+break-pair protocol
                               below can't otherwise reach. */
                            int  shifted = (fgetc(kf) == 's');
                            if (!shifted)
                                rewind(kf);
                            int scan = 0;
                            if (fscanf(kf, "%d", &scan) == 1 && scan > 0 && scan < 0x200) {
                                if (shifted)
                                    keyboard_input(1, 0x2A);
                                keyboard_input(1, (uint16_t) scan);
                                keyboard_input(0, (uint16_t) scan);
                                if (shifted)
                                    keyboard_input(0, 0x2A);
                                fprintf(stderr, "[keyinject] sent scancode %d (0x%02X)%s\n", scan, scan, shifted ? " [+shift]" : "");
                                fflush(stderr);
                            }
                            fclose(kf);
                            remove("inject_key.txt");
                        }
                    }
                }

                /* Find where INBRDPC.SYS actually runs, to dump+disassemble its real
                   extended-memory diagnostic routine (the driver-level "functional
                   extended memory: 0k / bad extended memory" report, per user priority
                   2026-07-25 - getting functional RAM above 1MB, ideally the full 5MB,
                   is essential for the Win95 attempts). Log each newly-seen CS segment
                   value once (cheap - a small linear array, capped), skipping the
                   already-fully-understood F000 BIOS segment. This is a one-shot map,
                   not a per-instruction trace, so it's safe to leave running the whole
                   session without flooding the log. */
                {
                    static uint16_t seen_cs[64];
                    static int      seen_count = 0;
                    if ((CS != 0xF000) && (seen_count < 64)) {
                        int found = 0;
                        for (int i = 0; i < seen_count; i++) {
                            if (seen_cs[i] == CS) {
                                found = 1;
                                break;
                            }
                        }
                        if (!found) {
                            seen_cs[seen_count++] = CS;
                            fprintf(stderr, "[segmap] new CS segment seen: %04X (at PC=%04X)\n", CS, cpu_state.pc);
                            fflush(stderr);
                        }
                    }
                }

                /* One-shot live dump of INBRDPC.SYS itself, the moment its strategy
                   routine is entered (CS:PC = 0247:04F8 confirmed live via [segmap]
                   above - matches the driver header's documented strategy_off=04F8
                   exactly, see INBOARD/inbrdpc_sys_disasm_notes.md). Dumps the whole
                   ~51200-byte region starting at the driver's load segment for offline
                   disassembly - same live-dump-beats-static-file technique already
                   proven for the BIOS ROM (see INBOARD_86BOX_PORT_PLAN.md), needed here
                   because DOS relocates/patches the driver at load time so the static
                   .SYS file on disk won't exactly match what's actually executing. */
                if ((CS == 0x0247) && (cpu_state.pc == 0x04F8)) {
                    static int dumped_inbrdpc = 0;
                    if (!dumped_inbrdpc) {
                        dumped_inbrdpc = 1;
                        FILE *f = fopen("inbrdpc_live_dump.bin", "wb");
                        if (f) {
                            uint32_t base = ((uint32_t) CS) << 4;
                            for (uint32_t a = 0; a < 51200; a++) {
                                uint8_t b = mem_readb_phys(base + a);
                                fwrite(&b, 1, 1, f);
                            }
                            fclose(f);
                            fprintf(stderr, "[inbrdpcdump] wrote inbrdpc_live_dump.bin from base %05X\n", base);
                            fflush(stderr);
                        }
                    }
                }

                /* Trace INBRDPC.SYS's real protected-mode extended-memory test loop
                   (found via live-dump disassembly this session, file offsets
                   ~9B75-9E5C: enters protected mode via a GDT descriptor, fills a 64KB
                   region with 0xFFFFFFFF/0x01010101/0x00000000 in turn, XOR-compares
                   each read-back against the expected pattern into EBP, and reports
                   "bad" if EBP is ever nonzero). These are the three "or ebp,eax"
                   compare points in the CX-counted per-region loop body (the initial,
                   pre-loop pass uses the same shape at 9C9F/9CC1/9CE3 but the repeating
                   loop that actually walks all of extended memory is 9DF5/9E17/9E39).
                   EIP alone is enough to identify these uniquely - CS is whatever
                   protected-mode selector is active, not F000/0247, so don't filter on
                   it. Logs only non-zero (mismatch) hits, capped, with the byte pattern
                   under test and the raw XOR-mismatch mask - which bits are wrong tells
                   us immediately whether this is a full-region dead read, a stuck-bit,
                   or something narrower (e.g. only the top byte of each dword, which
                   would point at a specific address-line/bank-select problem). */
                if ((cpu_state.pc == 0x9DF5) || (cpu_state.pc == 0x9E17) || (cpu_state.pc == 0x9E39)) {
                    static int ramtest_count = 0;
                    if (ramtest_count < 200 && (EAX != 0)) {
                        ramtest_count++;
                        const char *pattern = (cpu_state.pc == 0x9DF5) ? "FFFFFFFF" : (cpu_state.pc == 0x9E17) ? "01010101" : "00000000";
                        fprintf(stderr, "[ramtest] #%d MISMATCH pattern=%s EIP=%04X mismatch_mask=%08X EDI=%08X ESI=%08X EBX=%08X\n",
                                ramtest_count, pattern, cpu_state.pc, EAX, EDI, ESI, EBX);
                        fflush(stderr);
                    }
                }

                /* Also trace the outer loop's iteration counter/descriptor-base advance
                   (word ptr [0x2a6] += 0x40 each pass through 0xbf98, cx from [0x90b9])
                   so we can correlate a mismatch above with WHICH 64KB region/iteration
                   it happened in, not just that one occurred somewhere. */
                if (cpu_state.pc == 0x9BF1) {
                    static int iter_count = 0;
                    if (iter_count < 100) {
                        iter_count++;
                        uint16_t base_word = mem_readw_phys(0x2470 + 0x2a6);
                        fprintf(stderr, "[ramtest-iter] #%d before-advance word[0x2a6]=%04X\n", iter_count, base_word);
                        fflush(stderr);
                    }
                }

                /* Trace ES.base on EVERY block1 pattern-test iteration (not just the 2
                   that failed) at the "or ebp,ebp" check (9D29) - the instant right
                   after the 3-pattern fill/compare finishes for that iteration, so
                   ES/DS are still set to whatever the test actually used. Goal: see
                   exactly which iteration the descriptor transitions from the wrong
                   value (0x2470, INBRDPC.SYS's own segment - confirmed on iterations 1-2)
                   to the correct extended-memory base, to tell a genuine off-by-N
                   pipeline/ordering bug in the driver's own descriptor setup from
                   anything timing-related on our end. */
                if (cpu_state.pc == 0x9D29) {
                    static int esbase_count = 0;
                    if (esbase_count < 80) {
                        esbase_count++;
                        fprintf(stderr, "[esbase-iter] #%d ES.base=%08X ES.limit=%08X EBP=%08X\n",
                                esbase_count, es, cpu_state.seg_es.limit, EBP);
                        fflush(stderr);
                    }
                }

                /* A full-file search (2026-07-25) for "OR byte ptr [si],imm8" (opcode
                   80 0C xx - the exact "mark this chip bad" instruction) found 10 hits,
                   in 5 pairs (bit1=memory-pattern-fail, bit2=port-0x62-fail), at file
                   offsets 9D2E/9D42, 9E88/9E98, A02C/A040, A1C8/A1D8, A31B/A32B - one
                   pair per board-type test block (1M/2M/4M/etc, matching the 3+ hardcoded
                   diagnostics-table entries already known from inbrdpc_sys_disasm_notes.md).
                   The earlier single-address trace (just 9D2E/9D42, the first block) got
                   zero hits - wrong block for our actual 4MB-piggyback config. Trace all
                   10 at once instead of guessing again which one is live. */
                if ((cpu_state.pc == 0x9D2E) || (cpu_state.pc == 0x9E88) || (cpu_state.pc == 0xA02C) || (cpu_state.pc == 0xA1C8) || (cpu_state.pc == 0xA31B)) {
                    static int mark1_count = 0;
                    if (mark1_count < 50) {
                        mark1_count++;
                        uint16_t word2a6 = mem_readw_phys(0x2470 + 0x2a6);
                        uint16_t word90b9 = mem_readw_phys(0x2470 + 0x90b9);
                        fprintf(stderr, "[markbad1-mempattern] #%d hit at EIP=%04X EBP(mismatch-mask)=%08X ESI=%08X word[2a6]=%04X word[90b9]=%04X ES.base=%08X ES.limit=%08X DS.base=%08X\n",
                                mark1_count, cpu_state.pc, EBP, ESI, word2a6, word90b9, es, cpu_state.seg_es.limit, ds);
                        fflush(stderr);
                    }
                }
                if ((cpu_state.pc == 0x9D3D) || (cpu_state.pc == 0x9E93) || (cpu_state.pc == 0xA03B) || (cpu_state.pc == 0xA1D3) || (cpu_state.pc == 0xA326)) {
                    static int port62_count = 0;
                    if (port62_count < 200) {
                        port62_count++;
                        fprintf(stderr, "[port62] #%d EIP=%04X AL(from IN 0x62)=%02X bit0x40=%d\n",
                                port62_count, cpu_state.pc, AL, (AL & 0x40) ? 1 : 0);
                        fflush(stderr);
                    }
                }
                if ((cpu_state.pc == 0x9D42) || (cpu_state.pc == 0x9E98) || (cpu_state.pc == 0xA040) || (cpu_state.pc == 0xA1D8) || (cpu_state.pc == 0xA32B)) {
                    static int mark2_count = 0;
                    if (mark2_count < 50) {
                        mark2_count++;
                        fprintf(stderr, "[markbad2-port62] #%d hit at EIP=%04X (port-0x62 failure marker actually executed)\n", mark2_count, cpu_state.pc);
                        fflush(stderr);
                    }
                }

                /* [optionrom] one-shot 1,048,576-line ring-buffer dump (2026-07-26 Mach8
                   black-screen investigation) removed 2026-08-02: it fired unconditionally
                   8 real seconds after CS first became 0xC000 regardless of whether the CPU
                   was actually stuck (not gated on lack of progress, despite the comment),
                   so it fired on every normal boot through the Mach8 option ROM - not just a
                   hang. Writing >1M individual fprintf lines blocked exec386()'s interpreter
                   loop for a long, disk-speed-dependent stretch of real wall-clock time,
                   which looked exactly like a fresh boot hang (frozen window, no further POST
                   progress, modecheck heartbeat going silent) on 2026-08-02 - confirmed via
                   direct correlation: [modecheck] ticked normally every second through t+11s,
                   then went silent at the exact moment this dump's line count matched a
                   still-in-progress write. The bug this hook was diagnosing (the C000:7B37/
                   7B23 PIT-readback delay loop) was root-caused and fixed by the AX=BX+1
                   force below in the same session - this hook has had zero diagnostic value
                   since, and was actively harmful once left in. See Technique 21 in
                   inboard-hw-debug skill.

                   Follow-up (2026-07-26) to the [optionrom] finding above: live-disassembly
                   of the ATI Mach8 BIOS.BIN found the actual stuck point is a CX=0x10-bounded
                   loop at C000:3ACB-3ADF (an EEPROM/status-bit shift-in loop, part of a board-
                   ID detection routine at 397D), calling a delay helper at 3A4E that toggles a
                   clock bit via OUT to port 0x1CE/0x1CF with STI active during the delay. If
                   this loop is genuinely bounded, CX should count down 0x10..0x1 then the loop
                   exits - so log CX every time PC=0x3ACB is reached (top of the loop body,
                   right after "push ax"), capped, to see directly whether the countdown is
                   clean (proves something ABOVE this routine is retrying it forever) or
                   corrupted/non-monotonic (proves the STI-during-delay is letting an interrupt
                   clobber CX, e.g. via a stack/SS:SP mismatch). */
                if ((CS == 0xC000) && (cpu_state.pc == 0x3ACB)) {
                    static int cxtrace_count = 0;
                    if (cxtrace_count < 80) {
                        cxtrace_count++;
                        fprintf(stderr, "[cxtrace] #%d PC=3ACB CX=%04X BX=%04X SP=%04X SS=%04X\n",
                                cxtrace_count, CX, BX, ESP & 0xFFFF, ss);
                        fflush(stderr);
                    }
                }

                /* Follow-up (2026-07-26): live-disasm found the confirmed-stuck loop is
                   actually C000:7B15-7B39, a PIT-channel-0 direct-readback busy-wait
                   (latches counter 0 via OUT 43h,0 then reads two bytes from port 40h,
                   with no IRQ0 dependency) computing elapsed PIT ticks and looping until
                   BX ticks have passed. Given this whole project's history of CPU-speed/
                   waitstate timing overrides desyncing from real hardware behavior (the
                   already-fixed CGA "snow" bug was explicitly this same class of problem),
                   the live question is whether the PIT counter it reads is actually
                   decrementing at all under this config. Log DX (initial latched value,
                   set once at loop entry) and the live elapsed AX right at the CMP/branch
                   (0x7B37) every hit, capped - if DX-derived elapsed is frozen or barely
                   moving over many real seconds, the PIT itself isn't advancing properly
                   relative to instruction dispatch in this config; if it's climbing at a
                   sane rate just short of BX, it's a target-value/threshold problem instead. */
                if ((CS == 0xC000) && (cpu_state.pc == 0x7B37)) {
                    static int     pittrace_count = 0;
                    static time_t  pit_t0         = 0;
                    static uint16_t last_ax       = 0xFFFF;
                    static uint16_t last_bx       = 0xFFFF;
                    if (pit_t0 == 0)
                        pit_t0 = time(NULL);
                    /* Edge-triggered: only log when AX (elapsed ticks) or BX (target)
                       actually changes, so a legitimately-fast early phase (many quick
                       calls, AX pinned at 0 the whole time) doesn't burn a hit cap before
                       the real question - does AX ever stop changing during the later
                       stuck phase - can be observed. Log once/real-second regardless as a
                       heartbeat too, so a truly-frozen AX is visible as "still 0000" rather
                       than silence. */
                    static time_t last_heartbeat = 0;
                    time_t        pit_now        = time(NULL);
                    int           changed         = (AX != last_ax) || (BX != last_bx);
                    int           heartbeat       = (pit_now != last_heartbeat);
                    if ((changed || heartbeat) && (pittrace_count < 20000)) {
                        pittrace_count++;
                        last_ax        = AX;
                        last_bx        = BX;
                        last_heartbeat = pit_now;
                        fprintf(stderr, "[pittrace] #%d t+%llds AX(elapsed)=%04X BX(target)=%04X DX(initial_latch)=%04X %s\n",
                                pittrace_count, (long long) (pit_now - pit_t0), AX, BX, DX, changed ? "CHANGED" : "heartbeat");
                        fflush(stderr);
                    }
                }

                /* The actual fix (2026-07-26, replacing an earlier PIT-pre-arm attempt - see
                   INBOARD_86BOX_PORT_PLAN.md for why that approach was reverted): a real
                   channel-0 arm unfroze this delay loop but caused a cascade of *new*
                   failures elsewhere in this same BIOS's later POST (backlogged IRQ0/IRQ1
                   landing on unrelated interrupt self-tests deep in E362-E3AC, since this
                   whole boot takes 70+ real seconds - far longer than a Mode-3 counter's
                   ~55ms max period, guaranteeing a backlog on real, unaccelerated-hardware
                   timescales that would never accumulate). This version has zero blast
                   radius outside this one routine: it doesn't touch the real system PIT at
                   all, generates no interrupt, and only ever fires while CS=C000 (the video
                   option ROM's own segment) at this exact loop's compare instruction -
                   directly force the elapsed-ticks register past the loop's own target so
                   the *guest's own* CMP/JBE resolves normally and it exits on its own terms,
                   the same as it would once a real, unaccelerated system's PIT had
                   genuinely ticked past BX.

                   0x7B37 is the `11301115150` ROM (a different revision than the user's real
                   card - see INBOARD_86BOX_PORT_PLAN.md). Once the user's own dumped ROM
                   (`113-11504-002`) was installed, the identical delay loop (same
                   OUT-43h/IN-40h/IN-40h/SUB/NEG/CMP/JBE shape, confirmed via disassembly) was
                   found at a *different but structurally identical* address, 0x7B23 - the
                   real ROM's own board-ID/RAM-addressing self-test genuinely does exist too
                   (confirmed live: same "RAM Addressing" text, same frozen-PIT symptom), just
                   faster overall, matching the user's real-hardware description once this
                   loop is unstuck the same way. Both addresses handled so either ROM boots
                   correctly without needing to remember to switch this fix over. */
                /* Moved into the shared inboard_post_fixups() above (PR #7749), where a third
                   ROM-revision address (0x7B16) is handled alongside 0x7B37/0x7B23. */
            }

            if ((CS == 0xF000) && (cpu_state.pc >= 0xE320) && (cpu_state.pc <= 0xE3A0)) {
                static int diag_count = 0;
                static int diag_loop_iter = 0;
                static int diag_poll_iter = 0;
                int is_tight_loop = (cpu_state.pc == 0xE349) || (cpu_state.pc == 0xE34B);
                int is_poll_loop  = (cpu_state.pc == 0xE371) || (cpu_state.pc == 0xE378);
                if (is_tight_loop)
                    diag_loop_iter++;
                if (is_poll_loop)
                    diag_poll_iter++;
                if (diag_count < 4000 && (!is_tight_loop || (diag_loop_iter % 4000 == 0) || (diag_loop_iter < 3))) {
                    diag_count++;
                    fprintf(stderr, "[trace] #%d PC=%04X AL=%02X CX=%04X flags=%04X IF=%d loop_iter=%d poll_iter=%d imr=%02X irr=%02X isr=%02X intp=%d f46b=%02X\n",
                            diag_count, cpu_state.pc, AL, CX, cpu_state.flags,
                            (cpu_state.flags & I_FLAG) ? 1 : 0, diag_loop_iter, diag_poll_iter,
                            pic.imr, pic.irr, pic.isr, pic.int_pending,
                            mem_readb_phys(0x46Bu));
                    fflush(stderr);
                }
            }

            /* One-shot full BIOS ROM dump (F0000-FFFFF live memory, not the static file -
               see INBOARD_86BOX_PORT_PLAN.md for why those disagree) the first time execution
               reaches F000:E518, the confirmed success-path target right after the DMA
               channel-0 refresh check (Bug 3, now fixed) - i.e. the start of whatever POST
               code runs next, leading up to the still-unexplained "1801" expansion-unit
               error. Written once to bios_f000_dump_1801.bin for offline capstone disasm. */
            if ((CS == 0xF000) && (cpu_state.pc == 0xE518)) {
                static int dumped_e518 = 0;
                if (!dumped_e518) {
                    dumped_e518 = 1;
                    FILE *f = fopen("bios_f000_dump_1801.bin", "wb");
                    if (f) {
                        for (uint32_t a = 0xF0000; a <= 0xFFFFF; a++) {
                            uint8_t b = mem_readb_phys(a);
                            fwrite(&b, 1, 1, f);
                        }
                        fclose(f);
                        fprintf(stderr, "[dump] wrote bios_f000_dump_1801.bin at E518\n");
                        fflush(stderr);
                    }
                }
            }

            /* One-shot: F000:E3CE is "in al,0x60" immediately after the receiver-card/
               expansion-I/O-unit pulse sequence on port 0x61 (49h, C8h, 48h) and a settle
               delay - this IS the actual "1801" presence test (confirmed via live-dump
               disassembly this session, F000:E3A6-E3DB). Real hardware expects AL==0 here
               (no extender chassis attached); a non-zero readback is exactly what triggers
               the false "1801". Print AL right after the read fires, plus the XT keyboard
               controller's buffer/status state at that instant, to see whether a stale
               keyboard byte (e.g. the 0xAA self-test-pass BAT response) is the culprit. */
            if ((CS == 0xF000) && (cpu_state.pc == 0xE3D0)) {
                static int dumped_recv = 0;
                if (!dumped_recv) {
                    dumped_recv = 1;
                    fprintf(stderr, "[recv1801] AL after IN AL,60h = %02X (expect 0 for no receiver card)\n", AL);
                    fflush(stderr);
                }
            }

            /* Wide, one-shot trace of the entire E3A6-E3DE receiver-card test decision
               path (register state at each step), to see exactly which branch is taken
               and why. */
            if ((CS == 0xF000) && (cpu_state.pc >= 0xE3A6) && (cpu_state.pc <= 0xE3DE)) {
                static int diag3_count = 0;
                if (diag3_count < 60) {
                    diag3_count++;
                    fprintf(stderr, "[recvtrace] #%d PC=%04X AX=%04X BX=%04X CX=%04X flags=%04X\n",
                            diag3_count, cpu_state.pc, AX, BX, CX, cpu_state.flags);
                    fflush(stderr);
                }
            }

            /* This test (2026-07-25, later): the E3A6-E3DE test's own port-0x60 readback
               came back AL=0 (the "no receiver card" pass value) - the je at E3D2 should
               therefore SKIP the error-setting call at E3D4 entirely. So whatever actually
               sets "1801" must be a short stretch between E3DE (this test's exit point) and
               F067 (first INT10h teletype call reached). Trace that whole gap once. */
            if ((CS == 0xF000) && (cpu_state.pc >= 0xE3DE) && (cpu_state.pc <= 0xF067)) {
                static int diag4_count = 0;
                if (diag4_count < 400) {
                    diag4_count++;
                    fprintf(stderr, "[gaptrace] #%d PC=%04X AX=%04X BX=%04X CX=%04X DX=%04X flags=%04X\n",
                            diag4_count, cpu_state.pc, AX, BX, CX, DX, cpu_state.flags);
                    fflush(stderr);
                }
            }

            /* Direct, unconditional (up to a few hits each) checkpoints at every single
               address in the E3D2-E3DE branch decision, to settle definitively whether the
               JE at E3D2 is actually taken or not - the E3DE-window gaptrace above found
               ZERO hits below E66F, which is only possible if either the JE was NOT taken
               (falling to E3D4's error-setting call) and control never came back through
               E3DE the way the static disassembly implies, or CS was not F000 at some point
               in here. This settles it directly, one checkpoint at a time. */
            if (cpu_state.pc == 0xE3D2) {
                static int n = 0; if (n < 5) { n++; fprintf(stderr, "[cp] E3D2 (JE) reached, CS=%04X flags=%04X ZF=%d\n", CS, cpu_state.flags, (cpu_state.flags & Z_FLAG) ? 1 : 0); fflush(stderr); }
            }
            if (cpu_state.pc == 0xE3D4) {
                static int n = 0; if (n < 5) { n++; fprintf(stderr, "[cp] E3D4 (JE NOT taken - error call) reached, CS=%04X\n", CS); fflush(stderr); }
            }
            if (cpu_state.pc == 0xE3D7) {
                static int n = 0; if (n < 5) { n++; fprintf(stderr, "[cp] E3D7 (print EC4C) reached, CS=%04X\n", CS); fflush(stderr); }
            }
            if (cpu_state.pc == 0xE3DB) {
                static int n = 0; if (n < 5) { n++; fprintf(stderr, "[cp] E3DB (call FF9A9) reached, CS=%04X\n", CS); fflush(stderr); }
            }
            if (cpu_state.pc == 0xE3DE) {
                static int n = 0; if (n < 5) { n++; fprintf(stderr, "[cp] E3DE (JE taken target / reconverge) reached, CS=%04X\n", CS); fflush(stderr); }
            }

            /* NOTE (2026-07-25): the E500-E900 window was traced and fully disassembled this
               session - it's the option-ROM scan (C800-F600), LPT/COM port presence detection
               (0x3BC/0x378/0x278 loopback test, 0x3FA/0x2FA), and DIP-switch/equipment-word
               setup, ending in a genuine INT 19h bootstrap call at E66D. None of it prints
               "1801" - that must happen either before E4D0 or on a later re-entry (warm boot
               after a failed INT 19h). Removed the per-instruction trace here in favor of the
               screen-text-triggered ring buffer dump above, which finds the actual call site
               directly instead of requiring more manual disassembly windows. */

#ifndef USE_NEW_DYNAREC
            x86_was_reset = 0;
#endif

            cpu_state.ea_seg = &cpu_state.seg_ds;
            cpu_state.ssegs  = 0;

#ifdef USE_DEBUG_REGS_486
            if (is386)
                ins_fetch_fault = cpu_386_check_instruction_fault();

            /* Breakpoint fault has priority over other faults. */
            if ((cpu_state.abrt == 0) & ins_fetch_fault) {
                x86gen();
                ins_fetch_fault = 0;
                /* No instructions executed at this point. */
                goto block_ended;
            }
#endif

            fetchdat = fastreadl_fetch(cs + cpu_state.pc);

            if (!cpu_state.abrt) {
#ifdef ENABLE_386_LOG
                if (in_smm)
                    x386_dynarec_log("[%04X:%08X] %08X\n", CS, cpu_state.pc, fetchdat);
#endif
                opcode = fetchdat & 0xFF;
                fetchdat >>= 8;
#ifdef USE_DEBUG_REGS_486
                trap = (trap & ~1) | (!!(cpu_state.flags & T_FLAG));
#else
                trap = cpu_state.flags & T_FLAG;
#endif

                cpu_state.pc++;
#ifdef USE_DEBUG_REGS_486
                cpu_state.eflags &= ~(RF_FLAG);
#endif
                x86_opcodes[(opcode | cpu_state.op32) & 0x3ff](fetchdat);
                if (x86_was_reset)
                    break;
            }
#ifdef ENABLE_386_LOG
            else if (in_smm)
                x386_dynarec_log("[%04X:%08X] ABRT\n", CS, cpu_state.pc);
#endif

            if (cpu_flush_pending == 1)
                cpu_flush_pending++;
            else if (cpu_flush_pending == 2) {
                cpu_flush_pending = 0;
                flushmmucache_pc();
            }

#ifndef USE_NEW_DYNAREC
            if (!use32)
                cpu_state.pc &= 0xffff;
#endif

            if (cpu_end_block_after_ins)
                cpu_end_block_after_ins--;

#ifdef USE_DEBUG_REGS_486
block_ended:
#endif
            if (cpu_state.abrt) {
                uint8_t oop    = opcode;
                flags_rebuild();
                tempi          = cpu_state.abrt & ABRT_MASK;
                cpu_state.abrt = 0;
                x86_doabrt(tempi);
                if (cpu_state.abrt) {
                    pclog("Double fault - %02X\n", oop);
                    cpu_state.abrt = 0;
#ifndef USE_NEW_DYNAREC
                    CS = oldcs;
#endif
                    cpu_state.pc = cpu_state.oldpc;
                    x386_dynarec_log("Double fault\n");
                    pmodeint(8, 0);
                    if (cpu_state.abrt) {
                        cpu_state.abrt = 0;
                        softresetx86();
                        cpu_set_edx();
#ifdef ENABLE_386_LOG
                        x386_dynarec_log("Triple fault - reset\n");
#endif
                    }
                }

#ifdef USE_DEBUG_REGS_486
                if (is386 && !x86_was_reset  && ins_fetch_fault)
                    x86gen();
#endif
            } else if (new_ne) {
                flags_rebuild();

                new_ne = 0;
#ifndef USE_NEW_DYNAREC
                oldcs = CS;
#endif
                cpu_state.oldpc = cpu_state.pc;
                x86_int(16);
            } else if (trap) {
                flags_rebuild();
#ifdef USE_DEBUG_REGS_486
                if (trap & 2) dr[6] |= 0x8000;
                if (trap & 1) dr[6] |= 0x4000;
                if (trap & 16) dr[6] |= 0x2000;
#endif
                trap = 0;
#ifndef USE_NEW_DYNAREC
                oldcs = CS;
#endif
                cpu_state.oldpc = cpu_state.pc;
                x86_int(1);
            }

            if (smi_line)
                enter_smm_check(0);
            else if (nmi && nmi_enable && nmi_mask) {
#ifndef USE_NEW_DYNAREC
                oldcs = CS;
#endif
                cpu_state.oldpc = cpu_state.pc;
                x86_int(2);
                nmi_enable = 0;
#ifdef OLD_NMI_BEHAVIOR
                if (nmi_auto_clear) {
                    nmi_auto_clear = 0;
                    nmi            = 0;
                }
#else
                nmi = 0;
#endif
            } else if ((cpu_state.flags & I_FLAG) && pic.int_pending && !cpu_end_block_after_ins) {
                vector = picinterrupt();
                if (vector != -1) {
                    flags_rebuild();
                    if (msw & 1)
                        pmodeint(vector, 0);
                    else {
                        writememw(ss, (SP - 2) & 0xFFFF, cpu_state.flags);
                        writememw(ss, (SP - 4) & 0xFFFF, CS);
                        writememw(ss, (SP - 6) & 0xFFFF, cpu_state.pc);
                        SP -= 6;
                        addr = (vector << 2) + idt.base;
                        cpu_state.flags &= ~I_FLAG;
                        cpu_state.flags &= ~T_FLAG;
                        cpu_state.pc = readmemw(0, addr);
                        loadcs(readmemw(0, addr + 2));
                    }
                }
            }

            ins_cycles -= cycles;
            tsc += ins_cycles;

            cycdiff = oldcyc - cycles;

            if (timetolive) {
                timetolive--;
                if (!timetolive)
                    fatal("Life expired\n");
            }

            if (TIMER_VAL_LESS_THAN_VAL(timer_target, (uint64_t) tsc))
                timer_process();

#ifdef USE_GDBSTUB
            if (gdbstub_instruction())
                return;
#endif
        }
    }
}
