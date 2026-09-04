/* CFUENUM.C - first real USB transaction on the RATOC REX-CFU1 (SL811HS).
 * Assumes the card is already enabled at 0x260 (run CFUPROBE first).
 *
 * Does: bus reset -> speed detect -> low/full-speed host setup -> SOF/keep-
 * alive start -> GET_DESCRIPTOR(device) control transfer to address 0 ->
 * prints the descriptor.  Per SL811HS datasheet + Linux sl811-hcd flow.
 *
 * Build: C:\WATCOM\BLD.BAT CFUENUM
 */
#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <dos.h>

#define IOB    0x260
#define A_ADDR (IOB + 0)
#define A_DATA (IOB + 1)

#define SL_CTL1    0x05
#define SL_INTENA  0x06
#define SL_INTSTAT 0x0D
#define SL_SOFLOW  0x0E
#define SL_CTL2    0x0F
#define INT_DONE_A 0x01
#define INT_SOF    0x10

/* EP A transfer registers */
#define EPA_CTL    0x00
#define EPA_ADDR   0x01
#define EPA_LEN    0x02
#define EPA_PIDEP  0x03        /* write: PID<<4|ep, read: packet status */
#define EPA_CNT    0x04        /* write: device address, read: remaining */

#define PID_SETUP  0xD0
#define PID_IN     0x90
#define PID_OUT    0x10

/* host ctl bits: ARM|ENABLE|dir|AFTERSOF|TOGGLE */
#define HC_ARM     0x01
#define HC_ENABLE  0x02
#define HC_OUT     0x04
#define HC_SOFSYNC 0x20  /* ERRATUM: full-speed only, hangs on low-speed */
#define HC_DATA1   0x40

static unsigned char slr(unsigned char r){ outp(A_ADDR, r); return (unsigned char)inp(A_DATA); }
static void slw(unsigned char r, unsigned char v){ outp(A_ADDR, r); outp(A_DATA, v); }
static void dly(unsigned n){ while (n--) inp(0x80); }          /* ~1us */

static void buf_write(unsigned char a, const unsigned char *p, int n)
{
    int i;
    outp(A_ADDR, a);
    for (i = 0; i < n; i++) outp(A_DATA, p[i]);
}

static void buf_read(unsigned char a, unsigned char *p, int n)
{
    int i;
    outp(A_ADDR, a);
    for (i = 0; i < n; i++) p[i] = (unsigned char)inp(A_DATA);
}

/* run one EP A transaction, wait for DONE, return packet status (reg 3) */
static int xact(unsigned char pidep, unsigned char devaddr,
                unsigned char bufaddr, unsigned char len, unsigned char ctl)
{
    unsigned tmo;
    slw(EPA_ADDR, bufaddr);
    slw(EPA_LEN, len);
    slw(EPA_PIDEP, pidep);
    slw(EPA_CNT, devaddr);
    slw(SL_INTSTAT, 0xFF);
    slw(EPA_CTL, ctl);
    for (tmo = 0; tmo < 50000; tmo++) {                        /* ~50ms+ */
        if (slr(SL_INTSTAT) & INT_DONE_A) break;
        dly(1);
    }
    if (!(slr(SL_INTSTAT) & INT_DONE_A)) return -1;            /* timeout */
    return slr(EPA_PIDEP);                                     /* status */
}

/* retry a transaction while the device NAKs (busy) */
static int xact_retry(unsigned char pidep, unsigned char devaddr,
                      unsigned char bufaddr, unsigned char len,
                      unsigned char ctl, int tries)
{
    int st;
    while (tries--) {
        st = xact(pidep, devaddr, bufaddr, len, ctl);
        if (st < 0 || !(st & 0x40)) return st;                 /* not NAK */
        dly(2000);
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

int main(void)
{
    unsigned char st8[8] = { 0x80, 0x06, 0x00, 0x01, 0x00, 0x00, 0x08, 0x00 };
    unsigned char buf[8];
    unsigned char s, hw, ctl2, lowspeed;
    int st, i, got;

    printf("CFUENUM - SL811HS USB enumeration probe (I/O 0x%03X)\n", IOB);
    hw = slr(0x0E);
    if ((hw >> 4) != 2 && (hw >> 4) != 1) {
        printf("SL811 not responding (hwrev %02X) - run CFUPROBE first\n", hw);
        return 1;
    }

    /* is anything attached? */
    slw(SL_CTL1, 0x00);
    slw(SL_INTENA, 0x00);
    slw(SL_INTSTAT, 0xFF);
    dly(5000);
    s = slr(SL_INTSTAT);
    if (s & 0x40) { printf("no device attached (INTSTAT=%02X)\n", s); return 1; }
    lowspeed = !(s & 0x80);
    printf("device present, %s-speed\n", lowspeed ? "LOW" : "FULL");

    /* USB bus reset: SE0 for ~30ms */
    printf("bus reset... ");
    slw(SL_CTL1, 0x08);
    dly(30000);
    slw(SL_CTL1, 0x00);
    dly(3000);
    slw(SL_INTSTAT, 0xFF);
    dly(5000);
    s = slr(SL_INTSTAT);
    if (s & 0x40) { printf("device vanished after reset (INTSTAT=%02X)\n", s); return 1; }
    lowspeed = !(s & 0x80);
    printf("done, still %s-speed\n", lowspeed ? "LOW" : "FULL");

    /* host mode, speed config, SOF/keep-alive + kickstart */
    ctl2 = (unsigned char)(0x80 | 0x2E | (lowspeed ? 0x40 : 0x00)); /* HOST|SOFhi|DSWAP */
    slw(SL_SOFLOW, 0xE0);
    slw(SL_CTL2, ctl2);
    slw(EPA_LEN, 0);
    outp(A_DATA, 0x50);                 /* autoinc: PIDEP = SOF ep0 */
    outp(A_DATA, 0x00);                 /* devaddr 0 */
    slw(EPA_CTL, HC_ARM);
    slw(SL_CTL1, (unsigned char)(0x01 | (lowspeed ? 0x20 : 0x00))); /* SOF_ENA|LSPD */
    slw(SL_INTSTAT, 0xFF);
    slw(SL_INTENA, INT_SOF);            /* bit4 gates the SOF timer */
    dly(20000);
    printf("host mode: CTL1=%02X CTL2=%02X INTSTAT=%02X\n",
           slr(SL_CTL1), ctl2, slr(SL_INTSTAT));
    dly(60000);                         /* let the device recover from reset */

    /* --- control transfer: GET_DESCRIPTOR(device), 8 bytes, addr 0 --- */
    buf_write(0x10, st8, 8);
    st = xact_retry(PID_SETUP, 0, 0x10, 8,
                    HC_OUT | HC_ENABLE | HC_ARM, 20);
    printf("SETUP : status %02X (%s)\n", st, ststr(st));
    if (st < 0 || !(st & 0x01)) goto fail;

    got = 0;
    st = xact_retry(PID_IN, 0, 0x10, 8,
                    HC_DATA1 | HC_ENABLE | HC_ARM, 50);
    printf("IN    : status %02X (%s)\n", st, ststr(st));
    if (st < 0 || !(st & 0x01)) goto fail;
    got = 8 - slr(EPA_CNT);
    buf_read(0x10, buf, 8);

    st = xact_retry(PID_OUT, 0, 0x10, 0,
                    HC_DATA1 | HC_OUT | HC_ENABLE | HC_ARM, 20);
    printf("STATUS: status %02X (%s)\n", st, ststr(st));

    printf("\ndevice descriptor (first %d bytes):", got);
    for (i = 0; i < 8; i++) printf(" %02X", buf[i]);
    printf("\n");
    if (buf[1] == 0x01)
        printf("bLength=%u bcdUSB=%X.%02X class=%02X maxpacket0=%u\n",
               buf[0], buf[3], buf[2], buf[4], buf[7]);
    printf("\n*** USB TRANSACTION ENGINE WORKS ***\n");
    return 0;

fail:
    printf("transfer failed; INTSTAT=%02X\n", slr(SL_INTSTAT));
    return 1;
}
