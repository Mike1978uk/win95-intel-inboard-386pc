/* CFUDIAG.C - RATOC REX-CFU1 (SL811HS USB host) diagnostic for Windows 95/98.
 *
 * Win32 port of the DOS probe CFUPROBE.C.  Two access paths:
 *   1. CFU1.VXD (preferred): loaded dynamically from the EXE's directory,
 *      does everything in ring 0 (the only path that reaches real hardware
 *      on Win98 - see below) and provides the IRQ test.
 *   2. /RAW: direct ring-3 port I/O and a pointer to the attribute window
 *      at linear seg<<4.  WARNING: on Windows 98 with an active PCMCIA
 *      stack these accesses are silently virtualized per-VM and do NOT
 *      reach the hardware (readbacks echo your own writes).  /RAW is only
 *      trustworthy on configurations where nothing owns the socket
 *      controller.  No IRQ test in this mode.
 *
 * The card cannot be configured by the OS PCMCIA stack: its CIS CONFIG
 * tuple points at attribute 0xF8 but the real config register is at 0xFC
 * (see PROBE-NOTES.md).  Both paths below poke 0xFC directly.
 *
 * Build:  wcc386 -bt=nt -zq -ox CFUDIAG.C
 *         wlink system nt option quiet name CFUDIAG.EXE file CFUDIAG.obj
 *                library advapi32.lib
 *
 * Usage:  CFUDIAG [/RAW] [/SOCKET n] [/BASE hex] [/PCIC hex] [/WIN hex]
 *                 [/DETECT] [/ENUM] [/SOF] [/SWAP] [/IRQ n] [/OFF]
 *                 [/IDS] [/LIVE] [/PCIC]
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <setjmp.h>
#include "cfu1.h"

/* SL811HS registers */
#define SL_CTL1    0x05
#define SL_INTENA  0x06
#define SL_INTSTAT 0x0D
#define SL_HWREV   0x0E
#define SL_CTL2    0x0F
#define INT_SOF    0x10

static HANDLE   hVxd = INVALID_HANDLE_VALUE;
static int      raw = 0;
static unsigned iobase = 0x260, pcic = 0x3E0, attrseg = 0xD000;
static unsigned socketno = 0, sockoff = 0;
static jmp_buf  fault_jmp;

/*---------------------------------------------------------------- raw ----*/
static void dly(unsigned n) { while (n--) inp(0x80); }
static void pw(unsigned char r, unsigned char v)
    { outp(pcic, r + sockoff); outp(pcic + 1, v); }
static unsigned char pr(unsigned char r)
    { outp(pcic, r + sockoff); return (unsigned char)inp(pcic + 1); }

static LONG WINAPI fault_filter(EXCEPTION_POINTERS *ep)
{
    (void)ep;
    longjmp(fault_jmp, 1);
    return EXCEPTION_CONTINUE_SEARCH;   /* not reached */
}

static volatile unsigned char *attrwin(void)
    { return (volatile unsigned char *)((unsigned long)attrseg << 4); }

/*------------------------------------------------------------- access ----*/
static int vxd_ioctl(DWORD code, void *in, DWORD cbin, void *out, DWORD cbout)
{
    DWORD ret = 0;
    if (!DeviceIoControl(hVxd, code, in, cbin, out, cbout, &ret, NULL))
        return -1;
    return 0;
}

static unsigned char slr(unsigned char r)
{
    if (raw) { outp(iobase, r); return (unsigned char)inp(iobase + 1); }
    else {
        DWORD v = 0;
        vxd_ioctl(CFU_RDREG, &r, 1, &v, 4);
        return (unsigned char)v;
    }
}

static void slw(unsigned char r, unsigned char v)
{
    if (raw) { outp(iobase, r); outp(iobase + 1, v); }
    else {
        unsigned char b[2];
        b[0] = r; b[1] = v;
        vxd_ioctl(CFU_WRREG, b, 2, NULL, 0);
    }
}

/* burst data-port access (autoincrement) is only available in raw mode;
 * via the VxD we re-address every byte, which is slower but equivalent */
static void slw_seq(unsigned char start, const unsigned char *p, int n)
{
    int i;
    if (raw) {
        outp(iobase, start);
        for (i = 0; i < n; i++) outp(iobase + 1, p[i]);
    } else
        for (i = 0; i < n; i++) slw((unsigned char)(start + i), p[i]);
}

static void slr_seq(unsigned char start, unsigned char *p, int n)
{
    int i;
    if (raw) {
        outp(iobase, start);
        for (i = 0; i < n; i++) p[i] = (unsigned char)inp(iobase + 1);
    } else
        for (i = 0; i < n; i++) p[i] = slr((unsigned char)(start + i));
}

/*------------------------------------------------------------- enable ----*/
static int enable_raw(void)
{
    volatile unsigned char *aw;
    unsigned start, woff;
    int i;

    if ((pr(0x01) & 0x0C) != 0x0C) { printf("no card in socket %u\n", socketno); return 0; }
    pw(0x02, 0x95); dly(50000);
    pw(0x03, 0x40); dly(20000);
    for (i = 0; i < 100 && !(pr(0x01) & 0x20); i++) dly(1000);
    if (!(pr(0x01) & 0x20)) { printf("card never READY (status %02X)\n", pr(0x01)); return 0; }

    start = attrseg >> 8;
    woff  = ((0u - start) & 0x3FFF) | 0x4000;
    pw(0x10, start & 0xFF); pw(0x11, (start >> 8) & 0x3F);
    pw(0x12, (start + 3) & 0xFF); pw(0x13, ((start + 3) >> 8) & 0x3F);
    pw(0x14, woff & 0xFF); pw(0x15, (woff >> 8) & 0xFF);
    pw(0x06, pr(0x06) | 0x01); dly(2000);

    aw = attrwin();
    if (setjmp(fault_jmp)) {
        printf("FAULT touching attribute window at linear %05lX -- use the VxD path\n",
               (unsigned long)attrseg << 4);
        return 0;
    }
    aw[0x1F8] = 0x02;                   /* REAL COR: attribute 0xFC */
    dly(2000);
    printf("COR (attr 0xFC) readback: %02X (expect 42)\n", aw[0x1F8]);

    pw(0x03, 0x60);
    pw(0x08, iobase & 0xFF); pw(0x09, (iobase >> 8) & 0xFF);
    pw(0x0A, (iobase + 7) & 0xFF); pw(0x0B, ((iobase + 7) >> 8) & 0xFF);
    pw(0x07, 0x00);
    pw(0x06, 0x41);
    dly(1000);
    return 1;
}

/* The OS PCMCIA layer reacts to the power-on (card-insertion event) by
 * re-reading the CIS with its own window programming, racing our enable.
 * A retry once the dust settles wins: repeat ENABLE until the COR reads
 * back as the card's characteristic (index | 0x40). */
static int enable_vxd(void)
{
    CFU_SETUP  s;
    CFU_RESULT r;
    int tries;
    memset(&s, 0, sizeof s);
    s.iobase  = (unsigned short)iobase;
    s.pcic    = (unsigned short)pcic;
    s.socket  = (unsigned char)socketno;
    s.attrseg = (unsigned short)attrseg;
    for (tries = 0; tries < 4; tries++) {
        memset(&r, 0, sizeof r);
        if (vxd_ioctl(CFU_ENABLE, &s, sizeof s, &r, sizeof r) != 0) {
            printf("CFU_ENABLE ioctl failed (GetLastError %lu)\n", GetLastError());
            return 0;
        }
        if (r.err == 0 && r.corrb == 0x42) break;
        Sleep(800);                     /* let PCCARD finish its CIS pass */
    }
    if (r.err) {
        static const char *msg[] = { "?", "no card detected", "card never READY",
                                     "MapPhysToLinear failed" };
        printf("enable failed: %s (err %lu, PCIC status %02X)\n",
               r.err <= 3 ? msg[r.err] : "?", r.err, r.pcicstat);
        return 0;
    }
    printf("COR (attr 0xFC) readback: %02X (expect 42)%s\n", r.corrb,
           tries ? " [after retry]" : "");
    printf("PCIC status: %02X\n", r.pcicstat);
    if (r.corrb != 0x42)
        printf("WARNING: COR not confirmed - OS PCMCIA layer may be interfering\n");
    return r.corrb == 0x42;
}

/*--------------------------------------------------------------- diag ----*/
static void identify(void)
{
    unsigned char hw = slr(SL_HWREV);
    printf("SL811 hwrev: %02X (%s)\n", hw,
           (hw >> 4) == 1 ? "SL811HS v1.2" :
           (hw >> 4) == 2 ? "SL811HS v1.5" : "UNKNOWN - check base/socket");
}

static void regdump(void)
{
    unsigned char r[16];
    int i;
    slr_seq(0, r, 16);
    printf("SL811 regs 00-0F:");
    for (i = 0; i < 16; i++) printf(" %02X", r[i]);
    printf("\n");
}

static int ramtest(void)
{
    unsigned char wr[240], rd[240];
    int i, bad = 0, pass;
    for (pass = 0; pass < 2; pass++) {
        unsigned char x = pass ? 0xAA : 0x55;
        for (i = 0; i < 240; i++) wr[i] = (unsigned char)((0x10 + i) ^ x);
        slw_seq(0x10, wr, 240);
        slr_seq(0x10, rd, 240);
        for (i = 0; i < 240; i++) if (rd[i] != wr[i]) bad++;
    }
    printf("buffer RAM test (0x10-0xFF, 2 passes): %s (%d bad)\n",
           bad ? "FAIL" : "PASS", bad);
    return bad == 0;
}

static void linestate(const char *tag)
{
    unsigned char s1, s2;
    s1 = slr(SL_INTSTAT);
    slw(SL_INTSTAT, 0xFF);
    Sleep(3);
    s2 = slr(SL_INTSTAT);
    printf("INTSTAT %s: before-clear %02X, 3ms after clear %02X\n", tag, s1, s2);
}

static void detect(void)
{
    unsigned char s;
    slw(SL_CTL1, 0x00);
    slw(SL_INTSTAT, 0xFF);
    Sleep(5);
    s = slr(SL_INTSTAT);
    printf("detect: INTSTAT=%02X -> %s\n", s,
           (s & 0x40) ? "NO DEVICE (bus SE0)" :
           (s & 0x80) ? "FULL-SPEED device attached (D+ high)"
                      : "LOW-SPEED device attached (D- high)");
}

static void start_sof(int swap)
{
    slw(SL_INTENA, 0x00);
    slw(SL_HWREV, 0xE0);                 /* SOF counter low  */
    slw(SL_CTL2, (unsigned char)(0x80 | (swap ? 0x40 : 0) | 0x2E));
    /* SL811 kickstart: arm EP A once with a zero-length SOF packet */
    slw(0x02, 0x00);
    slw(0x03, 0x50);                     /* PID=SOF, ep 0    */
    slw(0x04, 0x00);                     /* device address 0 */
    slw(0x00, 0x01);                     /* ARM              */
    slw(SL_CTL1, 0x01);                  /* SOF_ENA          */
    slw(SL_INTSTAT, 0xFF);
    slw(SL_INTENA, INT_SOF);             /* bit4 also GATES the SOF timer */
    printf("host mode + SOF started (CTL1=%02X)%s\n",
           slr(SL_CTL1), swap ? " [DSWAP]" : "");
}

/*------------------------------------------ USB transactions (polled) ----*/
/* EP A: ctl=0 addr=1 len=2 pidep=3(w)/status(r) devaddr=4(w)/count(r) */
static int xact(unsigned char pidep, unsigned char bufaddr,
                unsigned char len, unsigned char ctl)
{
    DWORD t0;
    slw(0x01, bufaddr);
    slw(0x02, len);
    slw(0x03, pidep);
    slw(0x04, 0x00);                    /* device address 0 */
    slw(SL_INTSTAT, 0xFF);
    slw(0x00, ctl);
    t0 = GetTickCount();
    while (!(slr(SL_INTSTAT) & 0x01))
        if (GetTickCount() - t0 > 100) return -1;
    return slr(0x03);                   /* packet status */
}

static int xact_retry(unsigned char pidep, unsigned char bufaddr,
                      unsigned char len, unsigned char ctl, int tries)
{
    int st = -1;
    while (tries--) {
        st = xact(pidep, bufaddr, len, ctl);
        if (st < 0 || !(st & 0x40)) return st;      /* not NAK */
        Sleep(2);
    }
    return st;
}

static const char *ststr(int st)
{
    static char b[64];
    if (st < 0) return "TIMEOUT";
    b[0] = 0;
    if (st & 0x01) strcat(b, "ACK ");
    if (st & 0x02) strcat(b, "ERR ");
    if (st & 0x04) strcat(b, "TMOUT ");
    if (st & 0x08) strcat(b, "SEQ ");
    if (st & 0x20) strcat(b, "OVF ");
    if (st & 0x40) strcat(b, "NAK ");
    if (st & 0x80) strcat(b, "STALL ");
    return b;
}

/* bus reset, host mode at the given speed, SOF kickstart */
static void usb_host_init(int lowspeed)
{
    slw(SL_INTENA, 0x00);
    slw(SL_CTL1, 0x08);                 /* force SE0: USB reset */
    Sleep(30);
    slw(SL_CTL1, 0x00);
    Sleep(5);
    slw(SL_HWREV, 0xE0);                /* SOF low */
    slw(SL_CTL2, (unsigned char)(0x80 | 0x2E | (lowspeed ? 0x40 : 0)));
    slw(0x02, 0x00);                    /* SOF kickstart: EP A len 0 */
    slw(0x03, 0x50);                    /* PID=SOF ep0 */
    slw(0x04, 0x00);
    slw(0x00, 0x01);                    /* ARM */
    slw(SL_CTL1, (unsigned char)(0x01 | (lowspeed ? 0x20 : 0)));
    slw(SL_INTSTAT, 0xFF);
    slw(SL_INTENA, INT_SOF);            /* bit4 gates the SOF timer */
    Sleep(120);                         /* post-reset recovery time */
}

/* GET_DESCRIPTOR(device, 8) to address 0; returns 1 on success */
static int try_get_descriptor(unsigned char *buf)
{
    static const unsigned char st8[8] =
        { 0x80, 0x06, 0x00, 0x01, 0x00, 0x00, 0x08, 0x00 };
    int st;
    slw_seq(0x10, st8, 8);
    st = xact_retry(0xD0, 0x10, 8, 0x07, 20);       /* SETUP, DATA0, OUT */
    printf("  SETUP : status %02X (%s)\n", st, ststr(st));
    if (st < 0 || !(st & 0x01)) return 0;
    st = xact_retry(0x90, 0x10, 8, 0x43, 50);       /* IN, DATA1 */
    printf("  IN    : status %02X (%s)\n", st, ststr(st));
    if (st < 0 || !(st & 0x01)) return 0;
    slr_seq(0x10, buf, 8);
    st = xact_retry(0x10, 0x10, 0, 0x47, 20);       /* OUT len 0, DATA1 */
    printf("  STATUS: status %02X (%s)\n", st, ststr(st));
    return 1;
}

static void usb_enum(void)
{
    unsigned char buf[8], s;
    int i, speed;

    slw(SL_CTL1, 0x00);
    slw(SL_INTSTAT, 0xFF);
    Sleep(5);
    s = slr(SL_INTSTAT);
    if (s & 0x40) { printf("enum: no device attached (INTSTAT=%02X)\n", s); return; }

    /* INTSTAT bit7 speed detect is unreliable on this hardware: probe.
     * Try full-speed first, then low-speed (LSPD + DSWAP). */
    for (speed = 0; speed < 2; speed++) {
        printf("enum: trying %s-speed...\n", speed ? "LOW" : "FULL");
        usb_host_init(speed);
        if (try_get_descriptor(buf)) {
            printf("device descriptor (first 8):");
            for (i = 0; i < 8; i++) printf(" %02X", buf[i]);
            printf("\n");
            if (buf[1] == 0x01)
                printf("bLength=%u bcdUSB=%X.%02X class=%02X maxpacket0=%u [%s-SPEED]\n",
                       buf[0], buf[3], buf[2], buf[4], buf[7],
                       speed ? "LOW" : "FULL");
            printf("*** USB TRANSACTION ENGINE WORKS ON WINDOWS 98 ***\n");
            return;
        }
    }
    printf("enum failed at both speeds; INTSTAT=%02X\n", slr(SL_INTSTAT));
}

/*--------------------------- ring-0 engine: enumeration + HID demo -------*/
static int ctrl_xfer(unsigned char addr, unsigned char mps,
                     const unsigned char *setup, void *out, unsigned wlen,
                     unsigned *got)
{
    CFU_CREQ q;
    static CFU_CRSP r;
    memset(&q, 0, sizeof q);
    q.devaddr = addr;
    q.mps = mps;
    q.wlen = (unsigned short)wlen;
    memcpy(q.setup, setup, 8);
    memset(&r, 0, sizeof r);
    if (vxd_ioctl(CFU_CTRL, &q, sizeof q, &r, sizeof r) != 0) {
        printf("  CFU_CTRL ioctl failed (%lu)\n", GetLastError());
        return 0;
    }
    if (r.err) {
        static const char *st[] = { "?", "SETUP", "DATA", "STATUS" };
        printf("  control transfer failed at %s (SL811 status %02lX)\n",
               st[r.err & 3], (r.err >> 8) & 0xFF);
        return 0;
    }
    if (got) *got = r.got;
    if (out && r.got) memcpy(out, r.data, r.got > wlen ? wlen : r.got);
    return 1;
}

static int usb_init_ring0(int lowspeed)
{
    unsigned char ls = (unsigned char)lowspeed;
    DWORD st = 0;
    if (vxd_ioctl(CFU_USBINIT, &ls, 1, &st, 4) != 0) return -1;
    return (int)st;
}

static void usb_enum2(void)
{
    static const unsigned char gd_dev[8] = {0x80,6,0,1,0,0,18,0};
    static const unsigned char gd_dev8[8] = {0x80,6,0,1,0,0,8,0};
    static const unsigned char gd_cfg9[8] = {0x80,6,0,2,0,0,9,0};
    unsigned char sa[8] = {0x00,5,1,0,0,0,0,0};      /* SET_ADDRESS 1 */
    unsigned char sc[8] = {0x00,9,1,0,0,0,0,0};      /* SET_CONFIGURATION 1 */
    unsigned char gd_cfg[8] = {0x80,6,0,2,0,0,0,0};
    unsigned char d[256], mps = 8;
    unsigned got = 0, total, i;
    int speed, st;

    for (speed = 0; speed < 2; speed++) {
        printf("enum2: bus reset, %s-speed host init...\n",
               speed ? "LOW" : "FULL");
        st = usb_init_ring0(speed);
        printf("  INTSTAT after init: %02X\n", st);
        if (!ctrl_xfer(0, 8, gd_dev8, d, 8, &got)) continue;
        break;
    }
    if (speed == 2) { printf("enum2: no response at either speed\n"); return; }
    mps = d[7] ? d[7] : 8;
    printf("  ep0 max packet: %u\n", mps);

    if (!ctrl_xfer(0, mps, sa, NULL, 0, NULL)) return;
    Sleep(20);
    printf("  SET_ADDRESS 1: ok\n");

    if (!ctrl_xfer(1, mps, gd_dev, d, 18, &got)) return;
    printf("  device descriptor (%u):", got);
    for (i = 0; i < got && i < 18; i++) printf(" %02X", d[i]);
    printf("\n");
    if (got >= 14)
        printf("  >>> VID=%02X%02X PID=%02X%02X bcdDevice=%02X.%02X usb=%X.%02X class=%02X\n",
               d[9], d[8], d[11], d[10], d[13], d[12], d[3], d[2], d[4]);

    if (!ctrl_xfer(1, mps, gd_cfg9, d, 9, &got) || got < 9) return;
    total = d[2] | (d[3] << 8);
    if (total > sizeof d) total = sizeof d;
    printf("  config descriptor: wTotalLength=%u, %u interface(s)\n",
           total, d[4]);
    gd_cfg[6] = (unsigned char)total;
    gd_cfg[7] = (unsigned char)(total >> 8);
    if (!ctrl_xfer(1, mps, gd_cfg, d, total, &got)) return;
    for (i = 0; i + 1 < got; i += d[i]) {
        if (d[i] == 0) break;
        if (d[i + 1] == 4)              /* interface descriptor */
            printf("  interface %u: class %02X subclass %02X proto %02X%s\n",
                   d[i + 2], d[i + 5], d[i + 6], d[i + 7],
                   d[i + 5] == 3 ? "  <-- HID" : "");
        if (d[i + 1] == 5)              /* endpoint descriptor */
            printf("  endpoint %02X: attr %02X mps %u interval %ums\n",
                   d[i + 2], d[i + 3], d[i + 4] | (d[i + 5] << 8), d[i + 6]);
    }

    if (!ctrl_xfer(1, mps, sc, NULL, 0, NULL)) return;
    printf("  SET_CONFIGURATION 1: ok\n");
    printf("*** FULL ENUMERATION COMPLETE (device at address 1, configured) ***\n");
}

/* poll a HID interrupt-IN endpoint and print reports as they arrive */
static void hid_poll(int ep, int seconds)
{
    CFU_XREQ q;
    static CFU_XRSP r;
    DWORD t0 = GetTickCount(), n = 0;
    unsigned toggle = 0, i;
    printf("polling interrupt IN ep %d for %ds (move the mouse / press keys!)...\n",
           ep, seconds);
    while (GetTickCount() - t0 < (DWORD)seconds * 1000) {
        memset(&q, 0, sizeof q);
        q.pidep = (unsigned char)(0x90 | (ep & 0x0F));
        q.devaddr = 1;
        q.ctl = (unsigned char)(0x03 | (toggle ? 0x40 : 0));
        q.len = 8;
        memset(&r, 0, sizeof r);
        if (vxd_ioctl(CFU_XACT, &q, sizeof q, &r, sizeof r) != 0) break;
        if (r.status != 0xFFFFFFFFUL && (r.status & 1) && r.got) {
            printf("  report %3lu:", ++n);
            for (i = 0; i < r.got; i++) printf(" %02X", r.data[i]);
            printf("\n");
            toggle ^= 1;
        }
        Sleep(10);
    }
    printf("%lu report(s) in %ds\n", n, seconds);
}

static unsigned char pcic_peek(unsigned char reg)
{
    DWORD v = 0;
    vxd_ioctl(CFU_PCICRD, &reg, 1, &v, 4);
    return (unsigned char)v;
}

static void irqtest(int irq)
{
    unsigned char b = (unsigned char)irq;
    unsigned char steer0, steer1;
    DWORD n = 0;
    if (raw) { printf("IRQ test needs the VxD (omit /RAW)\n"); return; }
    if (vxd_ioctl(CFU_IRQHOOK, &b, 1, NULL, 0) != 0) {
        printf("IRQ hook failed (GetLastError %lu)\n", GetLastError());
        return;
    }
    steer0 = pcic_peek(0x03);
    Sleep(1000);
    steer1 = pcic_peek(0x03);
    vxd_ioctl(CFU_IRQCOUNT, NULL, 0, &n, 4);
    printf("IRQ%d SOF interrupt test: %lu ints in ~1s (expect ~1000)\n", irq, n);
    printf("  PCIC id=%02X intctl %02X -> %02X (want %02X), ifstat=%02X (bit5=0 means IREQ# active), INTSTAT=%02X\n",
           pcic_peek(0x00), steer0, steer1, 0x60 | irq,
           pcic_peek(0x01), slr(SL_INTSTAT));
    vxd_ioctl(CFU_IRQUNHOOK, NULL, 0, NULL, 0);
}

/* print the PnP hardware IDs Win9x generated for inserted PC Cards, so we
 * can bind CFU1.INF to the exact ID (the CRC part is otherwise unknowable) */
static void list_pcmcia_ids(void)
{
    HKEY hk;
    char name[128];
    DWORD i = 0, cb;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Enum\\PCMCIA", 0, KEY_READ, &hk)
            != ERROR_SUCCESS) {
        printf("no HKLM\\Enum\\PCMCIA key (no PC Card ever enumerated?)\n");
        return;
    }
    printf("PC Card PnP IDs known to this system:\n");
    for (;;) {
        cb = sizeof name;
        if (RegEnumKeyEx(hk, i++, name, &cb, NULL, NULL, NULL, NULL)
                != ERROR_SUCCESS) break;
        printf("  PCMCIA\\%s\n", name);
    }
    RegCloseKey(hk);
}

/* dump live devnodes from the Configuration Manager (HKEY_DYN_DATA) so we
 * can see which PC Cards Windows thinks are inserted right now */
static void list_live_devnodes(void)
{
    HKEY hk, hsub;
    char name[64], key[96], hwkey[256];
    DWORD i = 0, cb, cb2, type, problem;
    if (RegOpenKeyEx(HKEY_DYN_DATA, "Config Manager\\Enum", 0, KEY_READ, &hk)
            != ERROR_SUCCESS) {
        printf("cannot open HKEY_DYN_DATA Config Manager (not Win9x?)\n");
        return;
    }
    printf("live devnodes (Config Manager):\n");
    for (;;) {
        cb = sizeof name;
        if (RegEnumKeyEx(hk, i++, name, &cb, NULL, NULL, NULL, NULL)
                != ERROR_SUCCESS) break;
        sprintf(key, "Config Manager\\Enum\\%s", name);
        if (RegOpenKeyEx(HKEY_DYN_DATA, key, 0, KEY_READ, &hsub)
                != ERROR_SUCCESS) continue;
        cb2 = sizeof hwkey; hwkey[0] = 0;
        RegQueryValueEx(hsub, "HardWareKey", NULL, &type,
                        (BYTE *)hwkey, &cb2);
        cb2 = sizeof problem; problem = 0;
        RegQueryValueEx(hsub, "Problem", NULL, &type,
                        (BYTE *)&problem, &cb2);
        RegCloseKey(hsub);
        if (problem) printf("  %s  [PROBLEM %lu]\n", hwkey, problem);
        else printf("  %s\n", hwkey);
    }
    RegCloseKey(hk);
}

/* show what the PnP path (CONFIGMG -> CFU1.VXD config handler) captured;
 * returns nonzero if the card is already enabled by PnP (skip re-enable) */
static int show_conf(void)
{
    CFU_CONF c;
    memset(&c, 0xEE, sizeof c);
    if (vxd_ioctl(CFU_GETCONF, NULL, 0, &c, sizeof c) != 0) {
        printf("GETCONF failed (old VxD loaded? GetLastError %lu)\n",
               GetLastError());
        return 0;
    }
    printf("PnP state: devnode %s, started %s, COR fix %s\n",
           (c.flags & 1) ? "SEEN" : "never seen",
           (c.flags & 2) ? "YES" : "no",
           (c.flags & 4) ? "APPLIED" : "no");
    printf("  Windows-assigned I/O base: %03X   IRQ: %u   socket: %u   COR rb: %02X\n",
           c.iobase, c.irq, c.socket, c.corrb);
    if ((c.flags & 4) && c.iobase) {
        iobase = c.iobase;
        return 1;
    }
    return 0;
}

/* delete the card's registry enum branch so the next insertion enumerates
 * fresh and picks up a newly-installed INF (Win9x RegDeleteKey is
 * recursive) */
static void clean_id(void)
{
    LONG rc = RegDeleteKey(HKEY_LOCAL_MACHINE,
                    "Enum\\PCMCIA\\RATOC-USB_HOST_CF+_CARD-57DD");
    printf("delete HKLM\\Enum\\PCMCIA\\RATOC-USB_HOST_CF+_CARD-57DD: %s (%ld)\n",
           rc == ERROR_SUCCESS ? "OK" : "failed", rc);
    printf("eject and reinsert the card to re-enumerate.\n");
}

static void pcic_dump(void)
{
    int r;
    printf("PCIC socket-%u regs 00-3F:\n", socketno);
    for (r = 0; r < 0x40; r++) {
        if ((r & 15) == 0) printf("  %02X:", r);
        printf(" %02X", pcic_peek((unsigned char)r));
        if ((r & 15) == 15) printf("\n");
    }
}

static void power_off(void)
{
    if (raw) { pw(0x06, 0); pw(0x03, 0); pw(0x02, 0); }
    else vxd_ioctl(CFU_DISABLE, NULL, 0, NULL, 0);
    printf("socket %u powered down.\n", socketno);
}

/*--------------------------------------------------------------- main ----*/
/* Win9x dynamic VxD loading is picky about the \\.\ path form; try the
 * known-good variants in turn and say which one worked. */
static HANDLE load_vxd(void)
{
    char exe[MAX_PATH], path[MAX_PATH + 8], sys[MAX_PATH];
    HANDLE h;
    int i;
    char *p;

    GetModuleFileName(NULL, exe, sizeof exe);
    p = strrchr(exe, '\\');
    if (p) *(p + 1) = 0;                     /* exe = EXE's directory\ */

    /* 1: full path, backslashes */
    sprintf(path, "\\\\.\\%sCFU1.VXD", exe);
    h = CreateFile(path, 0, 0, NULL, 0, FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (h != INVALID_HANDLE_VALUE) { printf("VxD loaded via full path\n"); return h; }
    printf("  load try 1 (%s): err %lu\n", path, GetLastError());

    /* 2: full path, forward slashes */
    sprintf(path, "\\\\.\\%sCFU1.VXD", exe);
    for (i = 4; path[i]; i++) if (path[i] == '\\') path[i] = '/';
    h = CreateFile(path, 0, 0, NULL, 0, FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (h != INVALID_HANDLE_VALUE) { printf("VxD loaded via fwd-slash path\n"); return h; }
    printf("  load try 2 (%s): err %lu\n", path, GetLastError());

    /* 3: bare name (loader searches cwd / windows\system) */
    h = CreateFile("\\\\.\\CFU1.VXD", 0, 0, NULL, 0, FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (h != INVALID_HANDLE_VALUE) { printf("VxD loaded via bare name\n"); return h; }
    printf("  load try 3 (\\\\.\\CFU1.VXD): err %lu\n", GetLastError());

    /* 4: copy beside KERNEL32 and load by name */
    GetSystemDirectory(sys, sizeof sys);
    strcat(sys, "\\CFU1.VXD");
    sprintf(path, "%sCFU1.VXD", exe);
    if (CopyFile(path, sys, FALSE)) {
        h = CreateFile("\\\\.\\CFU1.VXD", 0, 0, NULL, 0,
                       FILE_FLAG_DELETE_ON_CLOSE, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            printf("VxD loaded from %s\n", sys);
            return h;
        }
        printf("  load try 4 (%s): err %lu\n", sys, GetLastError());
    } else
        printf("  copy to %s failed: err %lu\n", sys, GetLastError());
    return INVALID_HANDLE_VALUE;
}

int main(int argc, char **argv)
{
    int i, dosof = 0, swap = 0, dodet = 0, dooff = 0, irq = -1, dodump = 0, doenum = 0, doconf = 0, pnp_up = 0, rdreg_arg = -1, wrreg_arg = -1, wrval_arg = 0, doenum2 = 0, hidep = -1;
    DWORD ver = 0;

    for (i = 1; i < argc; i++) {
        if (!stricmp(argv[i], "/RAW")) raw = 1;
        else if (!stricmp(argv[i], "/SOF")) dosof = 1;
        else if (!stricmp(argv[i], "/SWAP")) swap = 1;
        else if (!stricmp(argv[i], "/DETECT")) dodet = 1;
        else if (!stricmp(argv[i], "/OFF")) dooff = 1;
        else if (!stricmp(argv[i], "/IDS")) { list_pcmcia_ids(); return 0; }
        else if (!stricmp(argv[i], "/LIVE")) { list_live_devnodes(); return 0; }
        else if (!stricmp(argv[i], "/PCIC")) dodump = 1;
        else if (!stricmp(argv[i], "/ENUM")) doenum = 1;
        else if (!stricmp(argv[i], "/ENUM2")) doenum2 = 1;
        else if (!stricmp(argv[i], "/HID") && i + 1 < argc) hidep = atoi(argv[++i]);
        else if (!stricmp(argv[i], "/CONF")) doconf = 1;
        else if (!stricmp(argv[i], "/RD") && i + 1 < argc) { rdreg_arg = (int)strtoul(argv[++i], NULL, 16); }
        else if (!stricmp(argv[i], "/WR") && i + 2 < argc) { wrreg_arg = (int)strtoul(argv[++i], NULL, 16); wrval_arg = (int)strtoul(argv[++i], NULL, 16); }
        else if (!stricmp(argv[i], "/CLEANID")) { clean_id(); return 0; }
        else if (!stricmp(argv[i], "/IRQ") && i + 1 < argc) irq = atoi(argv[++i]);
        else if (!stricmp(argv[i], "/SOCKET") && i + 1 < argc) socketno = atoi(argv[++i]);
        else if (!stricmp(argv[i], "/BASE") && i + 1 < argc) iobase = strtoul(argv[++i], NULL, 16);
        else if (!stricmp(argv[i], "/PCIC") && i + 1 < argc) pcic = strtoul(argv[++i], NULL, 16);
        else if (!stricmp(argv[i], "/WIN") && i + 1 < argc) attrseg = strtoul(argv[++i], NULL, 16);
        else {
            printf("usage: CFUDIAG [/RAW] [/SOCKET n] [/BASE hex] [/PCIC hex] [/WIN hex]\n"
                   "               [/DETECT] [/SOF] [/SWAP] [/IRQ n] [/OFF] [/IDS]\n");
            return 1;
        }
    }
    sockoff = socketno * 0x40;

    printf("CFUDIAG - RATOC REX-CFU1 / SL811HS diag (socket %u, I/O 0x%03X)\n",
           socketno, iobase);

    if (!raw) {
        hVxd = load_vxd();
        if (hVxd == INVALID_HANDLE_VALUE) {
            printf("CFU1.VXD not loadable (GetLastError %lu) - falling back to /RAW\n",
                   GetLastError());
            raw = 1;
        } else {
            vxd_ioctl(CFU_GETVER, NULL, 0, &ver, 4);
            printf("CFU1.VXD loaded, version %lX.%02lX\n", ver >> 16, ver & 0xFFFF);
        }
    }
    if (raw)
        SetUnhandledExceptionFilter(fault_filter);

    if (doconf && !raw) {
        pnp_up = show_conf();
        /* /CONF alone inspects PnP state without touching the hardware */
        if (!(dosof || swap || dodet || doenum || doenum2 || hidep >= 0 || dodump || dooff || irq >= 0)) {
            CloseHandle(hVxd);
            return 0;
        }
    }

    if (dooff) {
        if (raw && (pr(0x01) & 0x0C) != 0x0C) printf("(no card)\n");
        power_off();
        return 0;
    }

    if (pnp_up)
        printf("using PnP-enabled card at 0x%03X (no re-enable)\n", iobase);
    else if (raw ? !enable_raw() : !enable_vxd()) return 1;

    /* quick register peek/poke path: /WR reg val and/or /RD reg, then out */
    if ((wrreg_arg >= 0 || rdreg_arg >= 0) &&
        !(dosof || swap || dodet || doenum || doenum2 || hidep >= 0 || dodump || irq >= 0)) {
        if (wrreg_arg >= 0) {
            slw((unsigned char)wrreg_arg, (unsigned char)wrval_arg);
            printf("wr [%02X] = %02X\n", wrreg_arg, wrval_arg);
        }
        if (rdreg_arg >= 0)
            printf("rd [%02X] = %02X (again: %02X)\n", rdreg_arg,
                   slr((unsigned char)rdreg_arg), slr((unsigned char)rdreg_arg));
        if (hVxd != INVALID_HANDLE_VALUE) CloseHandle(hVxd);
        return 0;
    }

    identify();
    regdump();
    ramtest();
    linestate("idle");
    if (dodump) pcic_dump();
    if (dodet) detect();
    if (doenum) { usb_enum(); slw(SL_INTENA, 0); slw(SL_INTSTAT, 0xFF); }
    if (doenum2) { usb_enum2(); }
    if (hidep >= 0) { hid_poll(hidep, 8); }
    if (doenum2 || hidep >= 0) { slw(SL_INTENA, 0); slw(SL_INTSTAT, 0xFF); }
    if (dosof || swap) { start_sof(swap); linestate("sof"); }
    if (irq >= 0) { if (!(dosof || swap)) start_sof(swap); irqtest(irq); }
    if (wrreg_arg >= 0) {
        slw((unsigned char)wrreg_arg, (unsigned char)wrval_arg);
        printf("wr [%02X] = %02X\n", wrreg_arg, wrval_arg);
    }
    if (rdreg_arg >= 0)
        printf("rd [%02X] = %02X (again: %02X)\n", rdreg_arg,
               slr((unsigned char)rdreg_arg), slr((unsigned char)rdreg_arg));
    if (dosof || swap || irq >= 0) {
        /* leaving INTENA set with no handler wedges IREQ# and makes the
         * OS PCMCIA layer shut the socket down - quiesce before exit */
        slw(SL_INTENA, 0x00);
        slw(SL_INTSTAT, 0xFF);
        printf("(SOF/interrupts quiesced before exit)\n");
    }
    printf("card left enabled at 0x%03X.\n", iobase);

    if (hVxd != INVALID_HANDLE_VALUE) CloseHandle(hVxd);
    return 0;
}
