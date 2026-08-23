/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Implement I/O ports and their operations.
 *
 * Authors: Sarah Walker, <https://pcem-emulator.co.uk/>
 *          Miran Grca, <mgrca8@gmail.com>
 *          Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *          Copyright 2008-2019 Sarah Walker.
 *          Copyright 2016-2025 Miran Grca.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#define HAVE_STDARG_H
#include <86box/86box.h>
#include <86box/io.h>
#include <86box/timer.h>
#include "cpu.h"
#include "x86.h"
#include <86box/m_amstrad.h>
#include <86box/pci.h>

/* [io-trace gate] 2026-08-23: every "[tag]" investigation trace in this file sits in inb()/outb(),
   the hottest functions in the emulator, and each one does an fflush() to disk per hit with caps as
   high as 20000. Together they were emitting ~45,000 lines (3.5 MB) in a couple of minutes of boot,
   which is a large part of why this build runs far slower than a clean one - and it was visible to
   the user as the machine taking minutes to reach video.

   The investigations they belong to are all closed (the OSR1 protected-mode keyboard work shipped
   as the custom VKD.VXD; the A20 and PIC2/RTC questions are answered). Per skill Technique 21, a
   hook with no remaining diagnostic value and a real per-boot cost is a liability - but rather than
   delete them outright they are gated here, so any of them can be brought back instantly without a
   rebuild:

       set INBOARD_IO_TRACE=1     (Windows)
       INBOARD_IO_TRACE=1 ./86Box (POSIX)

   Default is OFF, which costs one predictable branch on an already-cached int. */
static int
io_dbg_on(void)
{
    static int on = -1;
    if (on < 0)
        on = (getenv("INBOARD_IO_TRACE") != NULL);
    return on;
}


#define NPORTS 65536 /* PC/AT supports 64K ports */

typedef struct _io_ {
    uint8_t (*inb)(uint16_t port, void *priv);
    uint16_t (*inw)(uint16_t port, void *priv);
    uint32_t (*inl)(uint16_t port, void *priv);

    void (*outb)(uint16_t port, uint8_t val, void *priv);
    void (*outw)(uint16_t port, uint16_t val, void *priv);
    void (*outl)(uint16_t port, uint32_t val, void *priv);

    void *priv;

    struct _io_ *prev, *next;
} io_t;

typedef struct io_trap_s {
    uint8_t   enable;
    uint16_t  base;
    uint16_t  size;
    void    (*func)(uint16_t size, uint16_t port, uint8_t write, uint8_t val, void *priv);
    void     *priv;
} io_trap_t;

uint8_t initialized = 0;
io_t   *io[NPORTS];
io_t   *io_last[NPORTS];

#ifdef ENABLE_IO_LOG
uint8_t io_do_log = ENABLE_IO_LOG;

static void
io_log(const char *fmt, ...)
{
    va_list ap;

    if (io_do_log) {
        va_start(ap, fmt);
        pclog_ex(fmt, ap);
        va_end(ap);
    }
}
#else
#    define io_log(fmt, ...)
#endif

void
io_init(void)
{
    io_t *p;
    io_t *q;

    if (!initialized) {
        for (uint32_t c = 0; c < NPORTS; c++)
            io[c] = io_last[c] = NULL;
        initialized = 1;
    }

    for (uint32_t c = 0; c < NPORTS; c++) {
        if (io_last[c]) {
            /* Port c has at least one handler. */
            p = io_last[c];
            /* After this loop, p will have the pointer to the first handler. */
            while (p) {
                q = p->prev;
                free(p);
                p = q;
            }
            p = NULL;
        }

        /* io[c] should be NULL. */
        io[c] = io_last[c] = NULL;
    }
}

void
io_sethandler_common(uint16_t base, uint16_t size,
                     uint8_t (*inb)(uint16_t port, void *priv),
                     uint16_t (*inw)(uint16_t port, void *priv),
                     uint32_t (*inl)(uint16_t port, void *priv),
                     void (*outb)(uint16_t port, uint8_t val, void *priv),
                     void (*outw)(uint16_t port, uint16_t val, void *priv),
                     void (*outl)(uint16_t port, uint32_t val, void *priv),
                     void *priv, uint8_t step)
{
    io_t *p;
    io_t *q = NULL;

    for (uint32_t c = 0; c < size; c += step) {
        p = io_last[base + c];
        q = (io_t *) calloc(1, sizeof(io_t));
        if (p) {
            p->next = q;
            q->prev = p;
        } else {
            io[base + c] = q;
            q->prev      = NULL;
        }

        q->inb = inb;
        q->inw = inw;
        q->inl = inl;

        q->outb = outb;
        q->outw = outw;
        q->outl = outl;

        q->priv = priv;
        q->next = NULL;

        io_last[base + c] = q;

        q = NULL;
    }
}

void
io_removehandler_common(uint16_t base, uint16_t size,
                        uint8_t (*inb)(uint16_t port, void *priv),
                        uint16_t (*inw)(uint16_t port, void *priv),
                        uint32_t (*inl)(uint16_t port, void *priv),
                        void (*outb)(uint16_t port, uint8_t val, void *priv),
                        void (*outw)(uint16_t port, uint16_t val, void *priv),
                        void (*outl)(uint16_t port, uint32_t val, void *priv),
                        void *priv, uint8_t step)
{
    io_t *p;
    io_t *q;

    for (uint32_t c = 0; c < size; c += step) {
        p = io[base + c];
        if (!p)
            continue;
        while (p) {
            q = p->next;
            if ((p->inb == inb) && (p->inw == inw) && (p->inl == inl) && (p->outb == outb) && (p->outw == outw) && (p->outl == outl) && (p->priv == priv)) {
                if (p->prev)
                    p->prev->next = p->next;
                else
                    io[base + c] = p->next;
                if (p->next)
                    p->next->prev = p->prev;
                else
                    io_last[base + c] = p->prev;
                free(p);
                p = NULL;
                break;
            }
            p = q;
        }
    }
}

void
io_handler_common(uint8_t set, uint16_t base, uint16_t size,
                  uint8_t (*inb)(uint16_t port, void *priv),
                  uint16_t (*inw)(uint16_t port, void *priv),
                  uint32_t (*inl)(uint16_t port, void *priv),
                  void (*outb)(uint16_t port, uint8_t val, void *priv),
                  void (*outw)(uint16_t port, uint16_t val, void *priv),
                  void (*outl)(uint16_t port, uint32_t val, void *priv),
                  void *priv, uint8_t step)
{
    if (set)
        io_sethandler_common(base, size, inb, inw, inl, outb, outw, outl, priv, step);
    else
        io_removehandler_common(base, size, inb, inw, inl, outb, outw, outl, priv, step);
}

void
io_sethandler(uint16_t base, uint16_t size,
              uint8_t (*inb)(uint16_t port, void *priv),
              uint16_t (*inw)(uint16_t port, void *priv),
              uint32_t (*inl)(uint16_t port, void *priv),
              void (*outb)(uint16_t port, uint8_t val, void *priv),
              void (*outw)(uint16_t port, uint16_t val, void *priv),
              void (*outl)(uint16_t port, uint32_t val, void *priv),
              void *priv)
{
    io_sethandler_common(base, size, inb, inw, inl, outb, outw, outl, priv, 1);
}

void
io_removehandler(uint16_t base, uint16_t size,
                 uint8_t (*inb)(uint16_t port, void *priv),
                 uint16_t (*inw)(uint16_t port, void *priv),
                 uint32_t (*inl)(uint16_t port, void *priv),
                 void (*outb)(uint16_t port, uint8_t val, void *priv),
                 void (*outw)(uint16_t port, uint16_t val, void *priv),
                 void (*outl)(uint16_t port, uint32_t val, void *priv),
                 void *priv)
{
    io_removehandler_common(base, size, inb, inw, inl, outb, outw, outl, priv, 1);
}

void
io_handler(uint8_t set, uint16_t base, uint16_t size,
           uint8_t (*inb)(uint16_t port, void *priv),
           uint16_t (*inw)(uint16_t port, void *priv),
           uint32_t (*inl)(uint16_t port, void *priv),
           void (*outb)(uint16_t port, uint8_t val, void *priv),
           void (*outw)(uint16_t port, uint16_t val, void *priv),
           void (*outl)(uint16_t port, uint32_t val, void *priv),
           void *priv)
{
    io_handler_common(set, base, size, inb, inw, inl, outb, outw, outl, priv, 1);
}

void
io_sethandler_interleaved(uint16_t base, uint16_t size,
                          uint8_t (*inb)(uint16_t port, void *priv),
                          uint16_t (*inw)(uint16_t port, void *priv),
                          uint32_t (*inl)(uint16_t port, void *priv),
                          void (*outb)(uint16_t port, uint8_t val, void *priv),
                          void (*outw)(uint16_t port, uint16_t val, void *priv),
                          void (*outl)(uint16_t port, uint32_t val, void *priv),
                          void *priv)
{
    io_sethandler_common(base, size, inb, inw, inl, outb, outw, outl, priv, 2);
}

void
io_removehandler_interleaved(uint16_t base, uint16_t size,
                             uint8_t (*inb)(uint16_t port, void *priv),
                             uint16_t (*inw)(uint16_t port, void *priv),
                             uint32_t (*inl)(uint16_t port, void *priv),
                             void (*outb)(uint16_t port, uint8_t val, void *priv),
                             void (*outw)(uint16_t port, uint16_t val, void *priv),
                             void (*outl)(uint16_t port, uint32_t val, void *priv),
                             void *priv)
{
    io_removehandler_common(base, size, inb, inw, inl, outb, outw, outl, priv, 2);
}

void
io_handler_interleaved(uint8_t set, uint16_t base, uint16_t size,
                       uint8_t (*inb)(uint16_t port, void *priv),
                       uint16_t (*inw)(uint16_t port, void *priv),
                       uint32_t (*inl)(uint16_t port, void *priv),
                       void (*outb)(uint16_t port, uint8_t val, void *priv),
                       void (*outw)(uint16_t port, uint16_t val, void *priv),
                       void (*outl)(uint16_t port, uint32_t val, void *priv),
                       void *priv)
{
    io_handler_common(set, base, size, inb, inw, inl, outb, outw, outl, priv, 2);
}

#ifdef USE_DEBUG_REGS_486
extern int trap;
/* Set trap for I/O address breakpoints. */
void
io_debug_check_addr(uint16_t port)
{
    uint8_t set_trap = 0;

    if (!(dr[7] & 0xFF))
        return;
    
    if (!(cr4 & 0x8))
        return; /* No I/O debug trap. */

    for (uint8_t i = 0; i < 4; i++) {
        uint16_t dr_addr = dr[i] & 0xFFFF;
        uint8_t  breakpoint_enabled = !!(dr[7] & (0x3 << (2 * i)));
        int len_type_pair = ((dr[7] >> 16) & (0xF << (4 * i))) >> (4 * i);
        if (!breakpoint_enabled)
            continue;
        if ((len_type_pair & 3) != 2)
            continue;
        
        switch ((len_type_pair >> 2) & 3) {
            case 0x00:
                if (dr_addr == port) {
                    set_trap = 1;
                    dr[6] |= (1 << i);
                }
                break;
            case 0x01:
                if ((dr_addr & ~1) == port || ((dr_addr & ~1) + 1) == (port + 1)) {
                    set_trap = 1;
                    dr[6] |= (1 << i);
                }
                break;
            case 0x03:
                dr_addr &= ~3;
                if (port >= dr_addr && port < (dr_addr + 4)) {
                    set_trap = 1;
                    dr[6] |= (1 << i);
                }
                break;
        }
    }
    if (set_trap)
        trap |= 4;
}
#endif

uint8_t
inb(uint16_t port)
{
    uint8_t ret = 0xff;
    io_t   *p;
    io_t   *q;
    uint8_t found  = 0;
#ifdef ENABLE_IO_LOG
    uint8_t qfound = 0;
#endif

    io_port = port;

#ifdef USE_DEBUG_REGS_486
    io_debug_check_addr(port);
#endif

    if ((pci_flags & FLAG_CONFIG_IO_ON) && (port >= pci_base) && (port < (pci_base + pci_size))) {
        ret = pci_read(port, NULL);
        found = 1;
#ifdef ENABLE_IO_LOG
        qfound = 1;
#endif
    } else if ((pci_flags & FLAG_CONFIG_DEV0_IO_ON) && (port >= 0xc000) && (port < 0xc100)) {
        ret = pci_read(port, NULL);
        found = 1;
#ifdef ENABLE_IO_LOG
        qfound = 1;
#endif
    } else {
        p = io[port];
        while (p) {
            q = p->next;
            if (p->inb) {
                ret &= p->inb(port, p->priv);
                found |= 1;
#ifdef ENABLE_IO_LOG
                qfound++;
#endif
            }
            p = q;
        }
    }

    if (amstrad_latch & 0x80000000) {
        if (port & 0x80)
            amstrad_latch = AMSTRAD_NOLATCH | 0x80000000;
        else if (port & 0x4000)
            amstrad_latch = AMSTRAD_SW10 | 0x80000000;
        else
            amstrad_latch = AMSTRAD_SW9 | 0x80000000;
    }

    if (!found)
        cycles -= io_delay;

    /* TriGem 486-BIOS MHz output. */
#if 0
    if (port == 0x1ed)
        ret = 0xfe;
#endif

    io_log("[%04X:%08X] (%i, %i, %04i) in b(%04X) = %02X\n", CS, cpu_state.pc, in_smm, found, qfound, port, ret);

    /* 2026-07-27: capped generic port-IO trace (INBOARD_86BOX_PORT_PLAN.md, "Sound Blaster
       digitized-sound DMA completion hangs..."). Every targeted-device trace so far (DSP busy
       status, DSP IRQ-ack, DMA1 status) shows the guest getting a clean "ready"/acknowledged
       response, yet the D1/D3 Speaker On/Off loop never advances - so this logs every guest
       port access in the PIT/PC-speaker/SB-base/OPL ranges to find what it's actually polling.
       Remove once root-caused. */
    {
        static int hits = 0;
        if ((hits < 400) &&
            (((port >= 0x40) && (port <= 0x43)) || (port == 0x61) ||
             ((port >= 0x220) && (port <= 0x22f)) || ((port >= 0x388) && (port <= 0x38b)))) {
            hits++;
            fprintf(stderr, "[iotrace] #%d IN  port=%04X ret=%02X CS:PC=%04X:%08X\n",
                    hits, port, ret, CS, cpu_state.pc);
            fflush(stderr);
        }
    }

    /* [a20trace] 2026-08-02: user hypothesis - the CONFIG.SYS-stage stall (see
       memory/win95_emulator_repro_2026_08_02.md) might involve Windows 95's own
       protected/V86-mode A20 handling using a command sequence this Inboard XT board
       doesn't recognize (unlike HIMEM.SYS's real-mode A20 auto-detect, which is already
       carefully matched to this hardware's port 0x60/0x64 behavior - see
       inboard386_write_60/inboard386_read_64 above). Trace all IN/OUT on ports 0x60
       (Inboard's own A20 gate, 0xDD/0xDF), 0x64 (hardwired 0x00 read), and 0x92 (PS/2-style
       "fast A20", not implemented on this XT-era board at all - if anything reads/writes it,
       that's itself informative) to see whether any A20-related I/O happens during the stall,
       not just during early CONFIG.SYS loading. Uncapped-ish (2000) since a comparison
       against known-legitimate early HIMEM.SYS activity is part of the point. */
    {
        static int a20_hits = 0;
        if (io_dbg_on() && (a20_hits < 2000) && ((port == 0x60) || (port == 0x64) || (port == 0x92))) {
            a20_hits++;
            fprintf(stderr, "[a20trace] #%d IN  port=%04X ret=%02X CS:PC=%04X:%08X\n",
                    a20_hits, port, ret, CS, cpu_state.pc);
            fflush(stderr);
        }
    }

    /* [kbdporttrace2] 2026-08-04: OSR1 protected-mode-keyboard investigation - [a20trace] above
       caps at 2000 hits and that cap was already exhausted by ~t+61s (legitimate INBRDPC.SYS
       driver-load-time A20 dance), making it silently blind to everything after - including
       whatever happens once Windows reaches protected mode and a real GUI dialog. This is a
       FRESH, separately-counted, much higher-capped trace for the exact same ports (60/64) so
       late-boot port I/O (or its absence) can be seen with real evidence instead of a false
       "silence" from an already-saturated counter. */
    {
        static int kbdport2_hits = 0;
        if (io_dbg_on() && (kbdport2_hits < 20000) && ((port == 0x60) || (port == 0x64))) {
            kbdport2_hits++;
            fprintf(stderr, "[kbdporttrace2] #%d IN  port=%04X ret=%02X CS:PC=%04X:%08X\n",
                    kbdport2_hits, port, ret, CS, cpu_state.pc);
            fflush(stderr);
        }
    }

    /* [pic2rtctrace] 2026-08-02: user hypothesis - this machine (ibmxt_inboard386, m_xt.c)
       genuinely has no second/slave 8259 PIC (pic2_init() is only called from m_at_common.c,
       m_ps1.c, m_ps2_isa.c, m_ps2_mca.c, and m_xt_xi8088.c - confirmed by grep, never from
       m_xt.c), so IRQ8-15 don't exist on this hardware at all, and IRQ8 specifically is the
       AT's RTC periodic-interrupt line. If Windows 95's VMM32/IOS/VFAT init (the exact stage
       this stall is inside) probes the CMOS RTC (ports 0x70/0x71) or the slave PIC (0xA0/0xA1)
       as part of deciding whether "32-bit disk access"/a fast timer source is available, and
       gets stuck waiting for a handshake this genuinely-absent hardware can never provide
       (rather than gracefully falling back), that would be a real, structural gap - not a
       config bug, an actual missing-feature gap needing a fix (or a documented hardware
       limitation) - exactly the same shape as the A20 readback discovery above. Uncapped-ish
       (2000 each way) so the whole boot timeline, not just the stall window, is visible for
       comparison. */
    {
        static int pic2rtc_hits = 0;
        if (io_dbg_on() && (pic2rtc_hits < 2000) && ((port == 0xA0) || (port == 0xA1) || (port == 0x70) || (port == 0x71))) {
            pic2rtc_hits++;
            fprintf(stderr, "[pic2rtctrace] #%d IN  port=%04X ret=%02X CS:PC=%04X:%08X\n",
                    pic2rtc_hits, port, ret, CS, cpu_state.pc);
            fflush(stderr);
        }
    }

    /* [seg0206porttrace] 2026-08-04: OSR1 XT+Inboard boot with genuinely-patched OSR1
       VPICD/VDMAD/VKD stalls in segment 0206 (INBRDPC.SYS resident code, same region as OSR2's
       020B loop), alternating with F000 BIOS calls but with no software-interrupt activity after
       ~t+150s per [intcalltally]. Trace every port this segment touches directly (no port-range
       filter - unlike the [iotrace]/[a20trace]/[pic2rtctrace] hooks above, we don't know the port
       yet) to find what it's actually polling, same technique that found port 0x670 in the OSR2
       020B case. Remove once root-caused. */
    {
        static int seg0206_hits = 0;
        if (io_dbg_on() && (seg0206_hits < 500) && (CS == 0x0206)) {
            seg0206_hits++;
            fprintf(stderr, "[seg0206porttrace] #%d IN  port=%04X ret=%02X CS:PC=%04X:%08X\n",
                    seg0206_hits, port, ret, CS, cpu_state.pc);
            fflush(stderr);
        }
    }

    return ret;
}

void
outb(uint16_t port, uint8_t val)
{
    io_t   *p;
    io_t   *q;
    uint8_t found  = 0;
#ifdef ENABLE_IO_LOG
    uint8_t qfound = 0;
#endif

    io_port = port;
    io_val  = val;

#ifdef USE_DEBUG_REGS_486
    io_debug_check_addr(port);
#endif

    if ((pci_flags & FLAG_CONFIG_IO_ON) && (port >= pci_base) && (port < (pci_base + pci_size))) {
        pci_write(port, val, NULL);
        found = 1;
#ifdef ENABLE_IO_LOG
        qfound = 1;
#endif
    } else if ((pci_flags & FLAG_CONFIG_DEV0_IO_ON) && (port >= 0xc000) && (port < 0xc100)) {
        pci_write(port, val, NULL);
        found = 1;
#ifdef ENABLE_IO_LOG
        qfound = 1;
#endif
    } else {
        p = io[port];
        while (p) {
            q = p->next;
            if (p->outb) {
                p->outb(port, val, p->priv);
                found |= 1;
#ifdef ENABLE_IO_LOG
                qfound++;
#endif
            }
            p = q;
        }
    }

    if (!found || (port == 0x84)) {
        cycles -= io_delay;
#ifdef USE_DYNAREC
        if (cpu_use_dynarec && ((port == 0x84) || (port == 0xeb) || (port == 0xed)))
            update_tsc();
#endif
    }

    io_log("[%04X:%08X] (%i, %i, %04i) outb(%04X, %02X)\n", CS, cpu_state.pc, in_smm, found, qfound, port, val);

    /* 2026-07-27: capped generic port-IO trace, OUT side - see matching comment in inb() above.
       INBOARD_86BOX_PORT_PLAN.md. Remove once root-caused. */
    {
        static int hits = 0;
        if ((hits < 400) &&
            (((port >= 0x40) && (port <= 0x43)) || (port == 0x61) ||
             ((port >= 0x220) && (port <= 0x22f)) || ((port >= 0x388) && (port <= 0x38b)))) {
            hits++;
            fprintf(stderr, "[iotrace] #%d OUT port=%04X val=%02X CS:PC=%04X:%08X\n",
                    hits, port, val, CS, cpu_state.pc);
            fflush(stderr);
        }
    }

    /* [a20trace] OUT side - see matching comment in inb() above. */
    {
        static int a20_hits = 0;
        if (io_dbg_on() && (a20_hits < 2000) && ((port == 0x60) || (port == 0x64) || (port == 0x92))) {
            a20_hits++;
            fprintf(stderr, "[a20trace] #%d OUT port=%04X val=%02X CS:PC=%04X:%08X\n",
                    a20_hits, port, val, CS, cpu_state.pc);
            fflush(stderr);
        }
    }

    /* [kbdporttrace2] OUT side - see matching comment in inb() above. */
    {
        static int kbdport2_hits = 0;
        if (io_dbg_on() && (kbdport2_hits < 20000) && ((port == 0x60) || (port == 0x64))) {
            kbdport2_hits++;
            fprintf(stderr, "[kbdporttrace2] #%d OUT port=%04X val=%02X CS:PC=%04X:%08X\n",
                    kbdport2_hits, port, val, CS, cpu_state.pc);
            fflush(stderr);
        }
    }

    /* [p61acktrace] 2026-08-04: OSR1 protected-mode-keyboard investigation - reference code
       (FastDoom's I_KeyboardISR_XT, a real-world-validated working XT/Inboard keyboard ISR) does
       THREE things per keystroke, not just "read port 60h": read port 60h, then toggle port 61h
       bit 7 (0x80) - read pb, OR in 0x80, write back, then write the original value back - to
       "clear the strobe"/acknowledge the XT keyboard hardware, THEN send the PIC EOI. This
       project's own kbc_xt.c (kbd_write(), port 0x61 case) requires exactly this: `if (val &
       0x80) { kbd->pa=0; kbd->blocked=0; picintc(2); }` - blocked only clears on a port 61h write
       with bit 7 SET. If Windows' protected-mode keyboard handling never performs this specific
       XT-only acknowledgment (plausible if it's written assuming AT hardware, where this quirk
       doesn't exist), `blocked` could get stuck at 1 forever after the first key, silently
       dropping every subsequent one regardless of the VKD.VXD/KEYBOARD.DRV port-64h fixes already
       applied and confirmed active. Uncapped-ish, high cap, so the actual dialog-freeze window is
       fully visible (the old [iotrace]'s port 0x61 coverage caps at 400 hits, exhausted in the
       first second of boot). */
    {
        static int p61_hits = 0;
        if (io_dbg_on() && p61_hits < 20000) {
            p61_hits++;
            fprintf(stderr, "[p61acktrace] #%d OUT port=0061 val=%02X (bit7/0x80 %s) CS:PC=%04X:%08X\n",
                    p61_hits, val, (val & 0x80) ? "SET-ack" : "clear", CS, cpu_state.pc);
            fflush(stderr);
        }
    }

    /* [pic2rtctrace] OUT side - see matching comment in inb() above (slave-PIC/CMOS-RTC
       missing-IRQ8 hypothesis). */
    {
        static int pic2rtc_hits = 0;
        if (io_dbg_on() && (pic2rtc_hits < 2000) && ((port == 0xA0) || (port == 0xA1) || (port == 0x70) || (port == 0x71))) {
            pic2rtc_hits++;
            fprintf(stderr, "[pic2rtctrace] #%d OUT port=%04X val=%02X CS:PC=%04X:%08X\n",
                    pic2rtc_hits, port, val, CS, cpu_state.pc);
            fflush(stderr);
        }
    }

    /* [seg0206porttrace] OUT side - see matching comment in inb() above. */
    {
        static int seg0206_hits = 0;
        if (io_dbg_on() && (seg0206_hits < 500) && (CS == 0x0206)) {
            seg0206_hits++;
            fprintf(stderr, "[seg0206porttrace] #%d OUT port=%04X val=%02X CS:PC=%04X:%08X\n",
                    seg0206_hits, port, val, CS, cpu_state.pc);
            fflush(stderr);
        }
    }

    return;
}

uint16_t
inw(uint16_t port)
{
    io_t    *p;
    io_t    *q;
    uint16_t ret    = 0xffff;
    uint8_t  found  = 0;
#ifdef ENABLE_IO_LOG
    uint8_t  qfound = 0;
#endif
    uint8_t  ret8[2];

    io_port = port;

#ifdef USE_DEBUG_REGS_486
    io_debug_check_addr(port);
#endif

    if ((pci_flags & FLAG_CONFIG_IO_ON) && (port >= pci_base) && (port < (pci_base + pci_size))) {
        ret = pci_readw(port, NULL);
        found = 2;
#ifdef ENABLE_IO_LOG
        qfound = 1;
#endif
    } else if ((pci_flags & FLAG_CONFIG_DEV0_IO_ON) && (port >= 0xc000) && (port < 0xc100)) {
        ret = pci_readw(port, NULL);
        found = 2;
#ifdef ENABLE_IO_LOG
        qfound = 1;
#endif
    } else {
        p = io[port];
        while (p) {
            q = p->next;
            if (p->inw) {
                ret &= p->inw(port, p->priv);
                found |= 2;
#ifdef ENABLE_IO_LOG
                qfound++;
#endif
            }
            p = q;
        }

        ret8[0] = ret & 0xff;
        ret8[1] = (ret >> 8) & 0xff;
        for (uint8_t i = 0; i < 2; i++) {
            p = io[(port + i) & 0xffff];
            while (p) {
                q = p->next;
                if (p->inb && !p->inw) {
                    ret8[i] &= p->inb(port + i, p->priv);
                    found |= 1;
#ifdef ENABLE_IO_LOG
                    qfound++;
#endif
                }
                p = q;
            }
        }
        ret = (ret8[1] << 8) | ret8[0];
    }

    if (amstrad_latch & 0x80000000) {
        if (port & 0x80)
            amstrad_latch = AMSTRAD_NOLATCH | 0x80000000;
        else if (port & 0x4000)
            amstrad_latch = AMSTRAD_SW10 | 0x80000000;
        else
            amstrad_latch = AMSTRAD_SW9 | 0x80000000;
    }

    if (!found)
        cycles -= io_delay;

    io_log("[%04X:%08X] (%i, %i, %04i) in w(%04X) = %04X\n", CS, cpu_state.pc, in_smm, found, qfound, port, ret);

    return ret;
}

void
outw(uint16_t port, uint16_t val)
{
    io_t   *p;
    io_t   *q;
    uint8_t found  = 0;
#ifdef ENABLE_IO_LOG
    uint8_t qfound = 0;
#endif

    io_port = port;
    io_val  = val;

#ifdef USE_DEBUG_REGS_486
    io_debug_check_addr(port);
#endif

    if ((pci_flags & FLAG_CONFIG_IO_ON) && (port >= pci_base) && (port < (pci_base + pci_size))) {
        pci_writew(port, val, NULL);
        found = 2;
#ifdef ENABLE_IO_LOG
        qfound = 1;
#endif
    } else if ((pci_flags & FLAG_CONFIG_DEV0_IO_ON) && (port >= 0xc000) && (port < 0xc100)) {
        pci_writew(port, val, NULL);
        found = 2;
#ifdef ENABLE_IO_LOG
        qfound = 1;
#endif
    } else {
        p = io[port];
        while (p) {
            q = p->next;
            if (p->outw) {
                p->outw(port, val, p->priv);
                found |= 2;
#ifdef ENABLE_IO_LOG
                qfound++;
#endif
            }
            p = q;
        }

        for (uint8_t i = 0; i < 2; i++) {
            p = io[(port + i) & 0xffff];
            while (p) {
                q = p->next;
                if (p->outb && !p->outw) {
                    p->outb(port + i, val >> (i << 3), p->priv);
                    found |= 1;
#ifdef ENABLE_IO_LOG
                    qfound++;
#endif
                }
                p = q;
            }
        }
    }

    if (!found) {
        cycles -= io_delay;
#ifdef USE_DYNAREC
        if (cpu_use_dynarec && ((port == 0xeb) || (port == 0xed)))
            update_tsc();
#endif
    }

    io_log("[%04X:%08X] (%i, %i, %04i) outw(%04X, %04X)\n", CS, cpu_state.pc, in_smm, found, qfound, port, val);

    return;
}

uint32_t
inl(uint16_t port)
{
    io_t    *p;
    io_t    *q;
    uint32_t ret = 0xffffffff;
    uint16_t ret16[2];
    uint8_t  ret8[4];
    uint8_t  found  = 0;
#ifdef ENABLE_IO_LOG
    uint8_t  qfound = 0;
#endif

    io_port = port;

#ifdef USE_DEBUG_REGS_486
    io_debug_check_addr(port);
#endif

    if ((pci_flags & FLAG_CONFIG_IO_ON) && (port >= pci_base) && (port < (pci_base + pci_size))) {
        ret = pci_readl(port, NULL);
        found = 4;
#ifdef ENABLE_IO_LOG
        qfound = 1;
#endif
    } else if ((pci_flags & FLAG_CONFIG_DEV0_IO_ON) && (port >= 0xc000) && (port < 0xc100)) {
        ret = pci_readl(port, NULL);
        found = 4;
#ifdef ENABLE_IO_LOG
        qfound = 1;
#endif
    } else {
        p = io[port];
        while (p) {
            q = p->next;
            if (p->inl) {
                ret &= p->inl(port, p->priv);
                found |= 4;
#ifdef ENABLE_IO_LOG
                qfound++;
#endif
            }
            p = q;
        }

        ret16[0] = ret & 0xffff;
        ret16[1] = (ret >> 16) & 0xffff;
        p        = io[port & 0xffff];
        while (p) {
            q = p->next;
            if (p->inw && !p->inl) {
                ret16[0] &= p->inw(port, p->priv);
                found |= 2;
#ifdef ENABLE_IO_LOG
                qfound++;
#endif
            }
            p = q;
        }

        p = io[(port + 2) & 0xffff];
        while (p) {
            q = p->next;
            if (p->inw && !p->inl) {
                ret16[1] &= p->inw(port + 2, p->priv);
                found |= 2;
#ifdef ENABLE_IO_LOG
                qfound++;
#endif
            }
            p = q;
        }
        ret = (ret16[1] << 16) | ret16[0];

        ret8[0] = ret & 0xff;
        ret8[1] = (ret >> 8) & 0xff;
        ret8[2] = (ret >> 16) & 0xff;
        ret8[3] = (ret >> 24) & 0xff;
        for (uint8_t i = 0; i < 4; i++) {
            p = io[(port + i) & 0xffff];
            while (p) {
                q = p->next;
                if (p->inb && !p->inw && !p->inl) {
                    ret8[i] &= p->inb(port + i, p->priv);
                    found |= 1;
#ifdef ENABLE_IO_LOG
                    qfound++;
#endif
                }
                p = q;
            }
        }
        ret = (ret8[3] << 24) | (ret8[2] << 16) | (ret8[1] << 8) | ret8[0];
    }

    if (amstrad_latch & 0x80000000) {
        if (port & 0x80)
            amstrad_latch = AMSTRAD_NOLATCH | 0x80000000;
        else if (port & 0x4000)
            amstrad_latch = AMSTRAD_SW10 | 0x80000000;
        else
            amstrad_latch = AMSTRAD_SW9 | 0x80000000;
    }

    if (!found)
        cycles -= io_delay;

    io_log("[%04X:%08X] (%i, %i, %04i) in l(%04X) = %08X\n", CS, cpu_state.pc, in_smm, found, qfound, port, ret);

    return ret;
}

void
outl(uint16_t port, uint32_t val)
{
    io_t   *p;
    io_t   *q;
    uint8_t found  = 0;
#ifdef ENABLE_IO_LOG
    uint8_t qfound = 0;
#endif

    io_port = port;
    io_val  = val;

#ifdef USE_DEBUG_REGS_486
    io_debug_check_addr(port);
#endif

    if ((pci_flags & FLAG_CONFIG_IO_ON) && (port >= pci_base) && (port < (pci_base + pci_size))) {
        pci_writel(port, val, NULL);
        found = 4;
#ifdef ENABLE_IO_LOG
        qfound = 1;
#endif
    } else if ((pci_flags & FLAG_CONFIG_DEV0_IO_ON) && (port >= 0xc000) && (port < 0xc100)) {
        pci_writel(port, val, NULL);
        found = 4;
#ifdef ENABLE_IO_LOG
        qfound = 1;
#endif
    } else {
        p = io[port];
        if (p) {
            while (p) {
                q = p->next;
                if (p->outl) {
                    p->outl(port, val, p->priv);
                    found |= 4;
#ifdef ENABLE_IO_LOG
                    qfound++;
#endif
                }
                p = q;
            }
        }

        for (uint8_t i = 0; i < 4; i += 2) {
            p = io[(port + i) & 0xffff];
            while (p) {
                q = p->next;
                if (p->outw && !p->outl) {
                    p->outw(port + i, val >> (i << 3), p->priv);
                    found |= 2;
#ifdef ENABLE_IO_LOG
                    qfound++;
#endif
                }
                p = q;
            }
        }

        for (uint8_t i = 0; i < 4; i++) {
            p = io[(port + i) & 0xffff];
            while (p) {
                q = p->next;
                if (p->outb && !p->outw && !p->outl) {
                    p->outb(port + i, val >> (i << 3), p->priv);
                    found |= 1;
#ifdef ENABLE_IO_LOG
                    qfound++;
#endif
                }
                p = q;
            }
        }
    }

    if (!found) {
        cycles -= io_delay;
#ifdef USE_DYNAREC
        if (cpu_use_dynarec && ((port == 0xeb) || (port == 0xed)))
            update_tsc();
#endif
    }

    io_log("[%04X:%08X] (%i, %i, %04i) outl(%04X, %08X)\n", CS, cpu_state.pc, in_smm, found, qfound, port, val);

    return;
}

static uint8_t
io_trap_readb(uint16_t port, void *priv)
{
    io_trap_t *trap = (io_trap_t *) priv;
    trap->func(1, port, 0, 0, trap->priv);
    return 0xff;
}

static uint16_t
io_trap_readw(uint16_t port, void *priv)
{
    io_trap_t *trap = (io_trap_t *) priv;
    trap->func(2, port, 0, 0, trap->priv);
    return 0xffff;
}

static uint32_t
io_trap_readl(uint16_t port, void *priv)
{
    io_trap_t *trap = (io_trap_t *) priv;
    trap->func(4, port, 0, 0, trap->priv);
    return 0xffffffff;
}

static void
io_trap_writeb(uint16_t port, uint8_t val, void *priv)
{
    io_trap_t *trap = (io_trap_t *) priv;
    trap->func(1, port, 1, val, trap->priv);
}

static void
io_trap_writew(uint16_t port, uint16_t val, void *priv)
{
    io_trap_t *trap = (io_trap_t *) priv;
    trap->func(2, port, 1, val, trap->priv);
}

static void
io_trap_writel(uint16_t port, uint32_t val, void *priv)
{
    io_trap_t *trap = (io_trap_t *) priv;
    trap->func(4, port, 1, val, trap->priv);
}

void *
io_trap_add(void (*func)(uint16_t size, uint16_t port, uint8_t write, uint8_t val, void *priv),
            void *priv)
{
    /* Instantiate new I/O trap. */
    io_trap_t *trap = (io_trap_t *) calloc(1, sizeof(io_trap_t));
    trap->enable    = 0;
    trap->base = trap->size = 0;
    trap->func              = func;
    trap->priv              = priv;

    return trap;
}

void
io_trap_remap(void *handle, uint8_t enable, uint16_t port, uint16_t size)
{
    io_trap_t *trap = (io_trap_t *) handle;
    if (!trap)
        return;

    io_log("I/O: Remapping trap from %04X-%04X (enable %d) to %04X-%04X (enable %d)\n",
           trap->base, trap->base + trap->size - 1, trap->enable, port, port + size - 1, enable);

    /* Remove old I/O mapping. */
    if (trap->enable && trap->size) {
        io_removehandler(trap->base, trap->size,
                         io_trap_readb, io_trap_readw, io_trap_readl,
                         io_trap_writeb, io_trap_writew, io_trap_writel,
                         trap);
    }

    /* Set trap enable flag, base address and size. */
    trap->enable = !!enable;
    trap->base   = port;
    trap->size   = size;

    /* Add new I/O mapping. */
    if (trap->enable && trap->size) {
        io_sethandler(trap->base, trap->size,
                      io_trap_readb, io_trap_readw, io_trap_readl,
                      io_trap_writeb, io_trap_writew, io_trap_writel,
                      trap);
    }
}

void
io_trap_remove(void *handle)
{
    io_trap_t *trap = (io_trap_t *) handle;
    if (!trap)
        return;

    /* Unmap I/O trap before freeing it. */
    io_trap_remap(trap, 0, 0, 0);

    free(trap);
}
