/* CFUPROBE.C - RATOC REX-CFU1 "USB HOST CF+ Card" probe/enabler (IBM PC110).
 *
 * The card is a ScanLogic/Cypress SL811HS USB 1.1 host controller behind a
 * trivial PCMCIA I/O interface: 8-bit, 8 ports, addr reg at base+0, data reg
 * at base+1 (SL811 internal address space: 0x00-0x0F control, 0x10-0xFF RAM).
 *
 * CIS (verified 2026-07-11 on real hardware, socket 0):
 *   MANFID C015/0001, VERS_1 "RATOC / USB HOST CF+ Card"
 *   CFTABLE: I/O entries 2/3/5/6 = 8 ports @ 0x260/0x280/0x2A0/0x2C0, any IRQ
 *   CONFIG tuple claims COR at attribute 0xF8 -- THAT IS A LIE.
 *   *** The real COR is at attribute offset 0xFC. 0xF8 reads FF, never
 *   latches, and does not configure the card. 0xFC powers up as 0x40
 *   (bit6 stuck high) and gates I/O decode. Any standards-following host
 *   stack fails on this card; a driver must poke 0xFC itself. ***
 *
 * Usage: CFUPROBE            enable + identify + regdump + RAM test + status
 *        CFUPROBE /SOF       ...then start host mode + SOF generation
 *        CFUPROBE /SWAP      with /SOF: set CTL2 DSWAP (D+/D- swap) bit
 *        CFUPROBE /IRQ n     ...then route card IRQ to n and count SOF ints
 *        CFUPROBE /OFF       power the socket down and exit
 *
 * Build: C:\WATCOM\BLD.BAT CFUPROBE
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <dos.h>

#define PCIC   0x3E0
#define IOB    0x260                    /* config entry 2 */
#define A_ADDR (IOB + 0)
#define A_DATA (IOB + 1)
#define ATTRSEG 0xD000
#define COR_OFF 0x01F8                  /* window byte offset of attr 0xFC */

/* SL811HS registers (host mode) */
#define SL_CTL1    0x05                 /* bit0 SOF_ENA, bits3-4 force, bit5 LS */
#define SL_INTENA  0x06
#define SL_INTSTAT 0x0D                 /* write 1s to clear */
#define SL_HWREV   0x0E                 /* read; write = SOF counter low */
#define SL_CTL2    0x0F                 /* bits0-5 SOF hi, bit6 DSWAP, bit7 HOST */
#define INT_SOF    0x10
#define INT_INSRMV 0x20

static void pw(unsigned char i, unsigned char v){ outp(PCIC, i); outp(PCIC + 1, v); }
static unsigned char pr(unsigned char i){ outp(PCIC, i); return (unsigned char)inp(PCIC + 1); }
static void dly(unsigned n){ while (n--) inp(0x80); }        /* ~1us each */

static unsigned char slr(unsigned char r){ outp(A_ADDR, r); return (unsigned char)inp(A_DATA); }
static void slw(unsigned char r, unsigned char v){ outp(A_ADDR, r); outp(A_DATA, v); }

static volatile unsigned long irqcount;
static void (__interrupt __far *oldvec)();
static int irqline = -1;

static void __interrupt __far sofisr(void)
{
    outp(A_ADDR, SL_INTSTAT); outp(A_DATA, 0xFF);            /* ack SL811 */
    irqcount++;
    if (irqline >= 8) outp(0xA0, 0x20);
    outp(0x20, 0x20);                                        /* EOI */
}

static int enable_card(void)
{
    unsigned char v;
    int i;
    if ((pr(0x01) & 0x0C) != 0x0C) { printf("no card in socket 0\n"); return 0; }
    pw(0x02, 0x95); dly(30000);                              /* Vcc on, 5V */
    pw(0x03, 0x40); dly(20000);                              /* deassert RESET */
    for (i = 0; i < 100; i++) { if (pr(0x01) & 0x20) break; dly(1000); }
    if (!(pr(0x01) & 0x20)) { printf("card never went READY (status %02X)\n", pr(0x01)); return 0; }

    /* attribute window 0 at ATTRSEG, card offset 0, REG space */
    pw(0x10, 0xD0); pw(0x11, 0x00);
    pw(0x12, 0xD3); pw(0x13, 0x00);
    pw(0x14, 0x30); pw(0x15, 0x7F);                          /* offset | attr */
    pw(0x06, pr(0x06) | 0x01); dly(1000);

    *(unsigned char __far *)MK_FP(ATTRSEG, COR_OFF) = 0x02;  /* REAL COR: attr 0xFC */
    dly(2000);
    v = *(unsigned char __far *)MK_FP(ATTRSEG, COR_OFF);
    printf("COR (attr 0xFC) readback: %02X (expect 42)\n", v);

    pw(0x03, 0x60);                                          /* I/O card mode */
    pw(0x08, IOB & 0xFF); pw(0x09, IOB >> 8);                /* I/O win 0 */
    pw(0x0A, (IOB + 7) & 0xFF); pw(0x0B, (IOB + 7) >> 8);
    pw(0x07, 0x00);                                          /* 8-bit timing */
    pw(0x06, 0x41);                                          /* attr win + I/O win 0 */
    dly(1000);
    return 1;
}

static void power_down(void)
{
    pw(0x06, 0x00); pw(0x03, 0x00); pw(0x02, 0x00);
    printf("socket 0 powered down.\n");
}

static void regdump(void)
{
    int r;
    printf("SL811 regs 00-0F:");
    for (r = 0; r < 16; r++) printf(" %02X", slr((unsigned char)r));
    printf("\n");
}

static int ramtest(void)
{
    int a, bad = 0;
    outp(A_ADDR, 0x10);
    for (a = 0x10; a <= 0xFF; a++) outp(A_DATA, (a ^ 0x55) & 0xFF);
    outp(A_ADDR, 0x10);
    for (a = 0x10; a <= 0xFF; a++)
        if (inp(A_DATA) != ((a ^ 0x55) & 0xFF)) bad++;
    outp(A_ADDR, 0x10);
    for (a = 0x10; a <= 0xFF; a++) outp(A_DATA, (a ^ 0xAA) & 0xFF);
    outp(A_ADDR, 0x10);
    for (a = 0x10; a <= 0xFF; a++)
        if (inp(A_DATA) != ((a ^ 0xAA) & 0xFF)) bad++;
    printf("buffer RAM test (0x10-0xFF, 2 passes): %s (%d bad)\n", bad ? "FAIL" : "PASS", bad);
    return bad == 0;
}

static void mirrors(void)
{
    unsigned char a, b, c;
    outp(IOB + 2, SL_HWREV); a = (unsigned char)inp(IOB + 3);
    outp(IOB + 4, SL_HWREV); b = (unsigned char)inp(IOB + 5);
    outp(IOB + 6, SL_HWREV); c = (unsigned char)inp(IOB + 7);
    printf("hwrev via +2/+3:%02X +4/+5:%02X +6/+7:%02X (mirror decode check)\n", a, b, c);
}

static void linestate(const char *tag)
{
    unsigned char s1, s2;
    s1 = slr(SL_INTSTAT);
    slw(SL_INTSTAT, 0xFF);                                   /* clear all */
    dly(3000);                                               /* ~3ms */
    s2 = slr(SL_INTSTAT);
    printf("INTSTAT %s: before-clear %02X, 3ms after clear %02X\n", tag, s1, s2);
}

static void start_sof(int swap)
{
    slw(SL_INTENA, 0x00);
    slw(SL_HWREV, 0xE0);                                     /* SOF counter low */
    slw(SL_CTL2, (unsigned char)(0x80 | (swap ? 0x40 : 0x00) | 0x2E)); /* HOST + SOF hi */
    /* SL811 quirk (per Linux sl811-hcd): SOF generation must be kick-
     * started by arming EP A with a zero-length SOF packet; CTL1 last. */
    slw(0x02, 0x00);                                         /* EP A buf length 0 */
    outp(A_DATA, 0x50);                                      /* +1 (autoinc): PID=SOF ep0 */
    outp(A_DATA, 0x00);                                      /* +1: device address 0 */
    slw(0x00, 0x01);                                         /* EP A hostctl = ARM */
    slw(SL_CTL1, 0x01);                                      /* SOF_ENA */
    slw(SL_INTSTAT, 0xFF);
    /* datasheet 5.3.5: INTENA bit4 enables the SOF *timer* itself, not
     * just its interrupt -- without it the counter never runs */
    slw(SL_INTENA, INT_SOF);
    printf("host mode + SOF started (CTL1=%02X CTL2=%02X)%s\n",
           slr(SL_CTL1), slr(SL_CTL2), swap ? " [DSWAP]" : "");
}

/* IRQ delivery test using the 1-kHz SOF timer interrupt (start_sof first) */
static void irqtest(int irq)
{
    unsigned char pic = (unsigned char)(irq < 8 ? 0x21 : 0xA1);
    unsigned char bit = (unsigned char)(1 << (irq & 7));
    unsigned char oldmask;
    unsigned long t0;
    volatile unsigned long __far *ticks =
        (volatile unsigned long __far *)MK_FP(0x40, 0x6C);

    irqline = irq;
    oldvec = _dos_getvect((unsigned char)(irq < 8 ? 8 + irq : 0x70 + irq - 8));
    _dos_setvect((unsigned char)(irq < 8 ? 8 + irq : 0x70 + irq - 8), sofisr);
    pw(0x03, (unsigned char)(0x60 | irq));                   /* steer card IRQ */
    oldmask = (unsigned char)inp(pic);
    outp(pic, oldmask & ~bit);

    irqcount = 0;
    slw(SL_INTSTAT, 0xFF);
    slw(SL_INTENA, INT_SOF);
    t0 = *ticks;
    while (*ticks - t0 < 18UL) ;                             /* ~1s */
    slw(SL_INTENA, 0x00);
    slw(SL_INTSTAT, 0xFF);

    outp(pic, oldmask);
    _dos_setvect((unsigned char)(irq < 8 ? 8 + irq : 0x70 + irq - 8), oldvec);
    pw(0x03, 0x60);
    printf("IRQ%d SOF interrupt test: %lu ints in ~1s (expect ~1000)\n", irq, irqcount);
}

/* Datasheet 5.3.7: INTSTAT bit6 = 1 device absent / 0 present (suspend off);
 * bit7 = live D+ (high = full-speed device, low = low-speed) */
static void detect(void)
{
    unsigned char s;
    slw(SL_CTL1, 0x00);
    slw(SL_INTSTAT, 0xFF);
    dly(5000);
    s = slr(SL_INTSTAT);
    printf("detect: INTSTAT=%02X -> %s\n", s,
           (s & 0x40) ? "NO DEVICE (bus SE0)" :
           (s & 0x80) ? "FULL-SPEED device attached (D+ high)"
                      : "LOW-SPEED device attached (D- high)");
}

int main(int argc, char **argv)
{
    int i, dosof = 0, swap = 0, irq = -1, dooff = 0, dodet = 0;
    unsigned char hw;

    for (i = 1; i < argc; i++) {
        if (!stricmp(argv[i], "/SOF")) dosof = 1;
        else if (!stricmp(argv[i], "/SWAP")) swap = 1;
        else if (!stricmp(argv[i], "/OFF")) dooff = 1;
        else if (!stricmp(argv[i], "/DETECT")) dodet = 1;
        else if (!stricmp(argv[i], "/IRQ") && i + 1 < argc) irq = atoi(argv[++i]);
        else { printf("usage: CFUPROBE [/SOF] [/SWAP] [/DETECT] [/IRQ n] [/OFF]\n"); return 1; }
    }
    printf("CFUPROBE - RATOC REX-CFU1 / SL811HS probe (I/O 0x%03X)\n", IOB);
    if (dooff) { power_down(); return 0; }
    if (!enable_card()) return 1;

    hw = slr(SL_HWREV);
    printf("SL811 hwrev: %02X (%s)\n", hw,
           (hw >> 4) == 1 ? "SL811HS v1.2" : (hw >> 4) == 2 ? "SL811HS v1.5" : "UNKNOWN");
    regdump();
    ramtest();
    mirrors();
    linestate("idle");
    if (dodet) detect();
    if (dosof || swap) { start_sof(swap); linestate("sof"); }
    if (irq >= 0) { if (!(dosof || swap)) start_sof(swap); irqtest(irq); }
    printf("card left enabled at 0x%03X.\n", IOB);
    return 0;
}
