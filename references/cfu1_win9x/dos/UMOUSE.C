/* UMOUSE.C - DOS INT 33h mouse driver for a USB mouse on the RATOC
 * REX-CFU1 (SL811HS USB host CF+ card).  Looks like MOUSE.COM to DOS
 * apps and games; the motion comes from a USB boot-protocol mouse
 * plugged into the card.
 *
 * How: enables the card (COR at attribute 0xFC - the CIS lies), resets
 * the bus, enumerates the mouse to address 1, puts it in boot protocol,
 * then goes resident.  A prescaled SL811 SOF interrupt (1 kHz / 8 =
 * 125 Hz) polls interrupt-IN endpoint 1 and feeds an INT 33h state
 * machine: position, mickeys, buttons, ranges, text-mode cursor, user
 * event callback.  /POLL uses INT 1Ch (18 Hz) instead of an IRQ.
 *
 * Usage: UMOUSE [/SOCKET n] [/IRQ n] [/POLL] [/EP n] [/WIN hex] [/U]
 *   defaults: socket 0, IRQ 7, endpoint 1, attr window segment D000
 *
 * Services: 0000 reset, 0001/0002 show/hide (text mode), 0003 status,
 * 0004 set pos, 0005/0006 press/release data, 0007/0008 ranges,
 * 000B mickeys, 000C user handler, 000F ratio, 0015-0017 save/restore,
 * 0021 soft reset, 0024 version (type 4 "PS/2", our IRQ).
 *
 * Build: wcc -ms -bt=dos -zq -os UMOUSE.C && wlink system dos file UMOUSE.obj
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <dos.h>
#include <i86.h>

/* ------------------------------------------------- hardware ------- */
#define PCIC        0x3E0
#define IOB         0x260
#define A_ADDR      (IOB + 0)
#define A_DATA      (IOB + 1)

#define SL_CTL1     0x05
#define SL_INTENA   0x06
#define SL_INTSTAT  0x0D
#define SL_SOFLOW   0x0E
#define SL_CTL2     0x0F
#define INT_SOF     0x10
#define INT_DONE_A  0x01

#define EPA_CTL     0x00
#define EPA_ADDR    0x01
#define EPA_LEN     0x02
#define EPA_PIDEP   0x03
#define EPA_CNT     0x04

#define PID_SETUP   0xD0
#define PID_IN      0x90
#define PID_OUT     0x10

#define HC_ARM      0x01
#define HC_ENABLE   0x02
#define HC_OUT      0x04
#define HC_DATA1    0x40

static int sock = 0;                    /* PCIC socket (bank * 0x40)   */
static unsigned attrseg = 0xD000;
static int irqline = 7;
static int use_poll = 0;                /* INT 1Ch instead of IRQ      */
static int mouse_ep = 1;
static unsigned char dev_ls = 0;        /* low-speed device            */
static unsigned char mps0 = 8;

static void pw(unsigned char i, unsigned char v)
{ outp(PCIC, (unsigned char)(i + sock * 0x40)); outp(PCIC + 1, v); }
static unsigned char pr(unsigned char i)
{ outp(PCIC, (unsigned char)(i + sock * 0x40)); return (unsigned char)inp(PCIC + 1); }
static void dly(unsigned n){ while (n--) inp(0x80); }   /* ~1us each */

static unsigned char slr(unsigned char r)
{ outp(A_ADDR, r); return (unsigned char)inp(A_DATA); }
static void slw(unsigned char r, unsigned char v)
{ outp(A_ADDR, r); outp(A_DATA, v); }

static void buf_write(unsigned char a, const unsigned char *p, int n)
{ int i; outp(A_ADDR, a); for (i = 0; i < n; i++) outp(A_DATA, p[i]); }
static void buf_read(unsigned char a, unsigned char *p, int n)
{ int i; outp(A_ADDR, a); for (i = 0; i < n; i++) p[i] = (unsigned char)inp(A_DATA); }

/* one EP A transaction; returns packet status or -1 on timeout.
 * 'spin' is the DONE-wait budget in ~1us units (ISR uses a short one). */
static int xact(unsigned char pidep, unsigned char devaddr,
                unsigned char bufaddr, unsigned char len,
                unsigned char ctl, unsigned spin)
{
    unsigned tmo;
    slw(EPA_ADDR, bufaddr);
    slw(EPA_LEN, len);
    slw(EPA_PIDEP, pidep);
    slw(EPA_CNT, devaddr);
    slw(SL_INTSTAT, 0xFF);
    slw(EPA_CTL, ctl);
    for (tmo = 0; tmo < spin; tmo++) {
        if (slr(SL_INTSTAT) & INT_DONE_A) return slr(EPA_PIDEP);
        inp(0x80);
    }
    return -1;
}

static int xact_retry(unsigned char pidep, unsigned char devaddr,
                      unsigned char bufaddr, unsigned char len,
                      unsigned char ctl, int tries)
{
    int st = -1;
    while (tries--) {
        st = xact(pidep, devaddr, bufaddr, len, ctl, 50000);
        if (st < 0 || !(st & 0x40)) return st;      /* not a NAK */
        dly(2000);
    }
    return st;
}

/* control transfer, no data or IN data; returns bytes got or -1 */
static int control(unsigned char devaddr, const unsigned char *setup,
                   unsigned char *data, int len)
{
    int st, got = 0, chunk, tog = 1;
    buf_write(0x10, setup, 8);
    st = xact_retry(PID_SETUP, devaddr, 0x10, 8, HC_OUT | HC_ENABLE | HC_ARM, 50);
    if (st < 0 || !(st & 0x01)) return -1;
    while (got < len) {
        chunk = len - got; if (chunk > mps0) chunk = mps0;
        st = xact_retry(PID_IN, devaddr, 0x10, (unsigned char)chunk,
                        (unsigned char)((tog ? HC_DATA1 : 0) | HC_ENABLE | HC_ARM), 200);
        if (st < 0 || !(st & 0x01)) return -1;
        chunk -= slr(EPA_CNT);                       /* actually received */
        buf_read(0x10, data + got, chunk);
        got += chunk; tog ^= 1;
        if (chunk < mps0) break;                     /* short packet */
    }
    if (len && data) {                               /* STATUS out */
        st = xact_retry(PID_OUT, devaddr, 0x10, 0,
                        HC_DATA1 | HC_OUT | HC_ENABLE | HC_ARM, 50);
    } else {                                         /* STATUS in */
        st = xact_retry(PID_IN, devaddr, 0x10, 0,
                        HC_DATA1 | HC_ENABLE | HC_ARM, 50);
    }
    if (st < 0 || !(st & 0x01)) return -1;
    return got;
}

/* ------------------------------------------------- INT 33h state -- */
static volatile int  pos_x = 320, pos_y = 100;
static volatile int  min_x = 0, max_x = 639, min_y = 0, max_y = 199;
static volatile int  mick_x = 0, mick_y = 0;        /* since last 000B */
static volatile unsigned char buttons = 0;
static volatile unsigned press_cnt[3], rel_cnt[3];
static volatile int  press_x[3], press_y[3], rel_x[3], rel_y[3];
static volatile int  show = -1;                     /* visible when 0  */
static int ratio_x = 8, ratio_y = 16;               /* mickeys/8 px    */
void (__far *user_proc)() = 0;                      /* non-static: the  */
volatile unsigned user_mask = 0;                    /* aux pragma needs */
                                                    /* to name it       */

/* text-mode cursor: invert the attribute at the cell under pos */
static volatile int cur_cell = -1;                  /* offset drawn at */
static volatile unsigned char cur_attr = 0;

static void cursor_undraw(void)
{
    unsigned char __far *vp;
    if (cur_cell < 0) return;
    vp = (unsigned char __far *)MK_FP(0xB800, cur_cell * 2 + 1);
    *vp = cur_attr;
    cur_cell = -1;
}

static void cursor_draw(void)
{
    unsigned char __far *vp;
    unsigned char mode = *(unsigned char __far *)MK_FP(0x40, 0x49);
    int cell;
    if (show != 0) return;
    if (mode != 2 && mode != 3 && mode != 7) return; /* text modes only */
    cell = (pos_y / 8) * 80 + (pos_x / 8);
    if (cell == cur_cell) return;
    cursor_undraw();
    vp = (unsigned char __far *)MK_FP(0xB800, cell * 2 + 1);
    cur_attr = *vp;
    *vp = (unsigned char)(((cur_attr << 4) | (cur_attr >> 4)));
    cur_cell = cell;
}

/* call the user event handler with the classic register interface:
 * AX=event mask, BX=buttons, CX/DX=pos, SI/DI=raw mickeys */
void call_user(unsigned ax_, unsigned bx_, unsigned cx_,
               unsigned dx_, int si_, int di_);
#pragma aux call_user =                             \
    "call dword ptr user_proc"                      \
    parm [ax] [bx] [cx] [dx] [si] [di]              \
    modify [ax bx cx dx si di es];

unsigned getsp(void);
#pragma aux getsp = "mov ax, sp" value [ax];

/* one mouse poll: IN transaction on the interrupt endpoint; parse a
 * boot report if one arrived.  Runs at interrupt time - short budget. */
static volatile unsigned char in_tog = 0;
static void mouse_poll(void)
{
    unsigned char rep[8];
    unsigned ev = 0;
    int st, got, dx, dy, i;
    unsigned char nb, chg;

    st = xact((unsigned char)(PID_IN | mouse_ep), 1, 0x10, 8,
              (unsigned char)((in_tog ? HC_DATA1 : 0) | HC_ENABLE | HC_ARM),
              2000);
    if (st == 0x09) return;                         /* ACK|SEQ: duplicate -
                                                     * drop, keep toggle */
    if (st != 0x01) return;                         /* ONLY a clean ACK may
                                                     * touch state: anything
                                                     * else risks parsing the
                                                     * stale chip buffer (a
                                                     * leftover SETUP reads
                                                     * as left-click + dx=11
                                                     * = the phantom drift) */
    in_tog ^= 1;
    got = 8 - slr(EPA_CNT);
    if (got < 3 || got > 8) return;                 /* stale count guard */
    buf_read(0x10, rep, got);

    nb = (unsigned char)(rep[0] & 7);
    dx = (signed char)rep[1];
    dy = (signed char)rep[2];

    if (dx || dy) {
        static int remx = 0, remy = 0;              /* carry the division
                                                     * remainder or small
                                                     * motions vanish */
        int n;
        mick_x += dx; mick_y += dy;
        n = dx * 8 + remx; pos_x += n / ratio_x; remx = n % ratio_x;
        n = dy * 8 + remy; pos_y += n / ratio_y; remy = n % ratio_y;
        if (pos_x < min_x) pos_x = min_x;
        if (pos_x > max_x) pos_x = max_x;
        if (pos_y < min_y) pos_y = min_y;
        if (pos_y > max_y) pos_y = max_y;
        cursor_draw();
        ev |= 0x01;
    }
    chg = (unsigned char)(nb ^ buttons);
    for (i = 0; i < 3; i++) {
        unsigned char bit = (unsigned char)(1 << i);
        if (!(chg & bit)) continue;
        if (nb & bit) {
            press_cnt[i]++; press_x[i] = pos_x; press_y[i] = pos_y;
            ev |= 2 << (i * 2);                     /* 2/8/32 press    */
        } else {
            rel_cnt[i]++; rel_x[i] = pos_x; rel_y[i] = pos_y;
            ev |= 4 << (i * 2);                     /* 4/16/64 release */
        }
    }
    buttons = nb;
    if (user_proc && (ev & user_mask))
        call_user(ev & user_mask, buttons,
                  (unsigned)pos_x, (unsigned)pos_y, dx, dy);
}

/* ------------------------------------------------- ISRs ----------- */
static void (__interrupt __far *old_irq)();
static void (__interrupt __far *old_1c)();
static void (__interrupt __far *old_33)();
static volatile unsigned char sof_div = 0;

static void __interrupt __far irq_isr(void)
{
    outp(A_ADDR, SL_INTSTAT); outp(A_DATA, 0xFF);   /* ack SL811 */
    if (++sof_div >= 8) {                           /* 1 kHz -> 125 Hz */
        sof_div = 0;
        mouse_poll();
    }
    if (irqline >= 8) outp(0xA0, 0x20);
    outp(0x20, 0x20);
}

static void __interrupt __far tick_isr(void)
{
    mouse_poll();
    _chain_intr(old_1c);
}

static void soft_reset(void)
{
    int i;
    cursor_undraw();
    show = -1;
    pos_x = 320; pos_y = 100;
    min_x = 0; max_x = 639; min_y = 0; max_y = 199;
    mick_x = mick_y = 0;
    ratio_x = 8; ratio_y = 16;
    user_proc = 0; user_mask = 0;
    for (i = 0; i < 3; i++) {
        press_cnt[i] = rel_cnt[i] = 0;
        press_x[i] = press_y[i] = rel_x[i] = rel_y[i] = 0;
    }
}

typedef struct {                                    /* funcs 15-17 */
    int px, py, mx0, mx1, my0, my1, sh;
    void (__far *up)(); unsigned um;
} SAVESTATE;

static void __interrupt __far int33_isr(union INTPACK r)
{
    int b;
    switch (r.w.ax) {
    case 0x0000:                                    /* reset */
        soft_reset();
        r.w.ax = 0xFFFF; r.w.bx = 3;
        break;
    case 0x0001:                                    /* show cursor */
        if (show < 0) show++;
        cursor_draw();
        break;
    case 0x0002:                                    /* hide cursor */
        show--;
        cursor_undraw();
        break;
    case 0x0003:                                    /* status */
        r.w.bx = buttons; r.w.cx = (unsigned)pos_x; r.w.dx = (unsigned)pos_y;
        break;
    case 0x0004:                                    /* set position */
        cursor_undraw();
        pos_x = (int)r.w.cx; pos_y = (int)r.w.dx;
        if (pos_x < min_x) pos_x = min_x;
        if (pos_x > max_x) pos_x = max_x;
        if (pos_y < min_y) pos_y = min_y;
        if (pos_y > max_y) pos_y = max_y;
        cursor_draw();
        break;
    case 0x0005:                                    /* press data */
        b = r.w.bx & 3; if (b > 2) b = 2;
        r.w.ax = buttons; r.w.bx = press_cnt[b]; press_cnt[b] = 0;
        r.w.cx = (unsigned)press_x[b]; r.w.dx = (unsigned)press_y[b];
        break;
    case 0x0006:                                    /* release data */
        b = r.w.bx & 3; if (b > 2) b = 2;
        r.w.ax = buttons; r.w.bx = rel_cnt[b]; rel_cnt[b] = 0;
        r.w.cx = (unsigned)rel_x[b]; r.w.dx = (unsigned)rel_y[b];
        break;
    case 0x0007:                                    /* X range */
        min_x = (int)r.w.cx; max_x = (int)r.w.dx;
        if (min_x > max_x) { b = min_x; min_x = max_x; max_x = b; }
        break;
    case 0x0008:                                    /* Y range */
        min_y = (int)r.w.cx; max_y = (int)r.w.dx;
        if (min_y > max_y) { b = min_y; min_y = max_y; max_y = b; }
        break;
    case 0x000B:                                    /* mickeys */
        r.w.cx = (unsigned)mick_x; r.w.dx = (unsigned)mick_y;
        mick_x = mick_y = 0;
        break;
    case 0x000C:                                    /* user handler */
        user_mask = r.w.cx;
        user_proc = (void (__far *)())MK_FP(r.w.es, r.w.dx);
        if (!user_mask) user_proc = 0;
        break;
    case 0x000F:                                    /* ratio */
        if (r.w.cx) ratio_x = (int)r.w.cx;
        if (r.w.dx) ratio_y = (int)r.w.dx;
        break;
    case 0x0015:                                    /* save-state size */
        r.w.bx = sizeof(SAVESTATE);
        break;
    case 0x0016: {                                  /* save state */
        SAVESTATE __far *s = (SAVESTATE __far *)MK_FP(r.w.es, r.w.dx);
        s->px = pos_x; s->py = pos_y;
        s->mx0 = min_x; s->mx1 = max_x; s->my0 = min_y; s->my1 = max_y;
        s->sh = show; s->up = user_proc; s->um = user_mask;
        break; }
    case 0x0017: {                                  /* restore state */
        SAVESTATE __far *s = (SAVESTATE __far *)MK_FP(r.w.es, r.w.dx);
        cursor_undraw();
        pos_x = s->px; pos_y = s->py;
        min_x = s->mx0; max_x = s->mx1; min_y = s->my0; max_y = s->my1;
        show = s->sh; user_proc = s->up; user_mask = s->um;
        cursor_draw();
        break; }
    case 0x0021:                                    /* software reset */
        soft_reset();
        r.w.ax = 0xFFFF; r.w.bx = 3;
        break;
    case 0x0024:                                    /* version/type */
        r.w.bx = 0x0800 + 0x14;                     /* say 8.20 */
        r.h.ch = 4;                                 /* "PS/2" */
        r.h.cl = (unsigned char)(use_poll ? 0 : irqline);
        break;
    default:
        break;                                      /* unimplemented: iret */
    }
}

/* ------------------------------------------------- bring-up ------- */
static int enable_card(void)
{
    unsigned char v;
    int i;
    if ((pr(0x01) & 0x0C) != 0x0C) { printf("no card in socket %d\n", sock); return 0; }
    pw(0x02, 0x95); dly(30000);
    pw(0x03, 0x40); dly(20000);
    for (i = 0; i < 100; i++) { if (pr(0x01) & 0x20) break; dly(1000); }
    if (!(pr(0x01) & 0x20)) { printf("card never went READY\n"); return 0; }
    {   /* attribute window: host attrseg<<4, 16KB, card offset 0, REG */
        unsigned pg = attrseg >> 8;                 /* addr bits 19:12 */
        unsigned off = (0x4000 - pg) & 0x3FFF;      /* card 0 - host   */
        pw(0x10, (unsigned char)pg);      pw(0x11, 0x00);
        pw(0x12, (unsigned char)(pg + 3)); pw(0x13, 0x00);
        pw(0x14, (unsigned char)off);
        pw(0x15, (unsigned char)(((off >> 8) & 0x3F) | 0x40));
    }
    pw(0x06, (unsigned char)(pr(0x06) | 0x01)); dly(1000);
    *(unsigned char __far *)MK_FP(attrseg, 0x01F8) = 0x02;   /* COR @ 0xFC */
    dly(2000);
    v = *(unsigned char __far *)MK_FP(attrseg, 0x01F8);
    if ((v & 0x02) != 0x02) { printf("COR write did not latch (%02X)\n", v); return 0; }
    pw(0x03, 0x60);
    pw(0x08, IOB & 0xFF); pw(0x09, IOB >> 8);
    pw(0x0A, (IOB + 7) & 0xFF); pw(0x0B, (IOB + 7) >> 8);
    pw(0x07, 0x00);
    pw(0x06, 0x41);
    dly(1000);
    return 1;
}

static int usb_bringup(void)
{
    static unsigned char gd8[8]  = { 0x80,0x06,0x00,0x01,0x00,0x00,0x08,0x00 };
    static unsigned char sa1[8]  = { 0x00,0x05,0x01,0x00,0x00,0x00,0x00,0x00 };
    static unsigned char sc1[8]  = { 0x00,0x09,0x01,0x00,0x00,0x00,0x00,0x00 };
    static unsigned char sp0[8]  = { 0x21,0x0B,0x00,0x00,0x00,0x00,0x00,0x00 };
    unsigned char buf[8], s, hw;
    int r;

    hw = slr(SL_SOFLOW);
    if ((hw >> 4) != 1 && (hw >> 4) != 2) { printf("SL811 not responding\n"); return 0; }

    slw(SL_CTL1, 0x00); slw(SL_INTENA, 0x00); slw(SL_INTSTAT, 0xFF);
    dly(5000);
    s = slr(SL_INTSTAT);
    if (s & 0x40) { printf("no USB device attached (plug the mouse in)\n"); return 0; }
    dev_ls = (unsigned char)(!(s & 0x80));

    slw(SL_CTL1, 0x08); dly(30000); slw(SL_CTL1, 0x00); dly(3000);
    slw(SL_INTSTAT, 0xFF); dly(5000);
    s = slr(SL_INTSTAT);
    if (s & 0x40) { printf("device vanished after bus reset\n"); return 0; }
    dev_ls = (unsigned char)(!(s & 0x80));

    slw(SL_SOFLOW, 0xE0);
    slw(SL_CTL2, (unsigned char)(0x80 | 0x2E | (dev_ls ? 0x40 : 0x00)));
    slw(EPA_LEN, 0);
    outp(A_DATA, 0x50);
    outp(A_DATA, 0x00);
    slw(EPA_CTL, HC_ARM);
    slw(SL_CTL1, (unsigned char)(0x01 | (dev_ls ? 0x20 : 0x00)));
    slw(SL_INTSTAT, 0xFF);
    slw(SL_INTENA, INT_SOF);
    dly(60000);                                     /* reset recovery */

    r = control(0, gd8, buf, 8);
    if (r < 8) { printf("GET_DESCRIPTOR failed\n"); return 0; }
    mps0 = buf[7] ? buf[7] : 8;
    if (control(0, sa1, 0, 0) < 0) { printf("SET_ADDRESS failed\n"); return 0; }
    dly(20000);
    if (control(1, sc1, 0, 0) < 0) { printf("SET_CONFIGURATION failed\n"); return 0; }
    control(1, sp0, 0, 0);                          /* boot protocol; may STALL */
    printf("%s-speed mouse at address 1, ep %d, mps0=%u\n",
           dev_ls ? "low" : "full", mouse_ep, mps0);
    return 1;
}

/* /DUMP: foreground raw-report dump until a key is pressed.
 * /NOTOG: never advance the IN toggle (test: does the SL811 track it
 * itself?  If SEQ vanishes here, manual toggling double-flips). */
static int no_tog = 0;
static void dump_reports(void)
{
    unsigned char rep[8];
    int st, got, i, n = 0;
    printf("raw reports (move the mouse; any key quits)%s:\n",
           no_tog ? " [no-toggle]" : "");
    while (!kbhit()) {
        st = xact((unsigned char)(PID_IN | mouse_ep), 1, 0x10, 8,
                  (unsigned char)((in_tog ? HC_DATA1 : 0) | HC_ENABLE | HC_ARM),
                  20000);
        if (st < 0 || !(st & 0x01)) { dly(8000); continue; }
        if (!no_tog) in_tog ^= 1;
        got = 8 - slr(EPA_CNT);
        if (got < 1) continue;
        if (got > 8) got = 8;
        buf_read(0x10, rep, got);
        printf("st=%02X got=%d:", st, got);
        for (i = 0; i < got; i++) printf(" %02X", rep[i]);
        printf("\n");
        if (++n >= 200) { printf("(200 reports; stopping)\n"); break; }
        dly(8000);
    }
    if (kbhit()) getch();
}

/* ------------------------------------------------- install -------- */
int main(int argc, char **argv)
{
    int i, uninst = 0, vec, dump = 0;
    for (i = 1; i < argc; i++) {
        if (!stricmp(argv[i], "/POLL")) use_poll = 1;
        else if (!stricmp(argv[i], "/DUMP")) dump = 1;
        else if (!stricmp(argv[i], "/NOTOG")) no_tog = 1;
        else if (!stricmp(argv[i], "/U")) uninst = 1;
        else if (!stricmp(argv[i], "/SOCKET") && i + 1 < argc) sock = atoi(argv[++i]);
        else if (!stricmp(argv[i], "/IRQ") && i + 1 < argc) irqline = atoi(argv[++i]);
        else if (!stricmp(argv[i], "/EP") && i + 1 < argc) mouse_ep = atoi(argv[++i]);
        else if (!stricmp(argv[i], "/WIN") && i + 1 < argc) attrseg = (unsigned)strtoul(argv[++i], 0, 16);
        else { printf("usage: UMOUSE [/SOCKET n] [/IRQ n] [/POLL] [/EP n] [/WIN hex] [/U]\n"); return 1; }
    }
    printf("UMOUSE - USB mouse INT 33h driver (REX-CFU1, I/O 0x%03X)\n", IOB);
    if (uninst) { printf("uninstall: reboot instead (v1 keeps it simple)\n"); return 1; }

    if (!enable_card()) return 1;
    if (!usb_bringup()) return 1;
    if (dump) { dump_reports(); return 0; }

    soft_reset();
    old_33 = _dos_getvect(0x33);
    _dos_setvect(0x33, int33_isr);

    if (use_poll) {
        old_1c = _dos_getvect(0x1C);
        _dos_setvect(0x1C, tick_isr);
        printf("polling on INT 1Ch (18 Hz).  Resident.\n");
    } else {
        unsigned char pic = (unsigned char)(irqline < 8 ? 0x21 : 0xA1);
        unsigned char bit = (unsigned char)(1 << (irqline & 7));
        vec = irqline < 8 ? 8 + irqline : 0x70 + irqline - 8;
        old_irq = _dos_getvect((unsigned char)vec);
        _dos_setvect((unsigned char)vec, irq_isr);
        pw(0x03, (unsigned char)(0x60 | irqline));  /* steer card IRQ */
        outp(pic, (unsigned char)(inp(pic) & ~bit));
        slw(SL_INTSTAT, 0xFF);
        printf("SOF IRQ%d, mouse polled at 125 Hz.  Resident.\n", irqline);
    }

    {   /* keep PSP..top-of-DGROUP resident (small model: SS = DGROUP,
         * stack at the top, so SS:SP is just below the segment end) */
        struct SREGS sr;
        segread(&sr);
        _dos_keep(0, (unsigned)(sr.ss - _psp) + ((getsp() + 256 + 15) >> 4));
    }
    return 0;
}
