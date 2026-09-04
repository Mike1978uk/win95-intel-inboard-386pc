/* CHUBT.C - hub interrogation over CFU_CTRL (ring 3, no reboot).
 * Why does the Apple hub STALL/fail every port-directed request?
 * Prints the RAW CFU_CRSP.err for each step: low byte = stage (1=SETUP,
 * 2=DATA, 3=STATUS), byte1 = SL811 packet status (bit7 STALL, bit6 NAK,
 * bit2 TIMEOUT, bit1 ERROR).
 */
#include <windows.h>
#include <stdio.h>
#include "cfu1.h"

static HANDLE hVxd;
static unsigned char g_mps0 = 8;

static int vxd_ioctl(DWORD code, void *in, DWORD cbin, void *out, DWORD cbout)
{
    DWORD ret = 0;
    if (!DeviceIoControl(hVxd, code, in, cbin, out, cbout, &ret, NULL))
        return -1;
    return 0;
}

/* control transfer; returns got, prints raw err on failure; rerr = raw */
static long ctl(unsigned char addr, unsigned char rt, unsigned char req,
                unsigned short val, unsigned short idx,
                unsigned char *data, unsigned len, unsigned long *rerr)
{
    CFU_CREQ q;
    static CFU_CRSP r;
    memset(&q, 0, sizeof q);
    q.devaddr = addr; q.mps = g_mps0; q.wlen = (unsigned short)len;
    q.setup[0] = rt;  q.setup[1] = req;
    q.setup[2] = (unsigned char)val; q.setup[3] = (unsigned char)(val >> 8);
    q.setup[4] = (unsigned char)idx; q.setup[5] = (unsigned char)(idx >> 8);
    q.setup[6] = (unsigned char)len; q.setup[7] = (unsigned char)(len >> 8);
    memset(&r, 0, sizeof r);
    if (vxd_ioctl(CFU_CTRL, &q, sizeof q, &r, sizeof r) != 0) {
        if (rerr) *rerr = 0xFFFFFFFFUL;
        return -1;
    }
    if (rerr) *rerr = r.err;
    if (r.err) return -1;
    if (data && r.got) memcpy(data, r.data, r.got > len ? len : r.got);
    return (long)r.got;
}

static void perr(const char *what, long got, unsigned long err)
{
    printf("  %-34s ", what);
    if (got >= 0) { printf("OK got=%ld\n", got); return; }
    printf("FAIL err=%08lX stage=%lu status=%02lX", err, err & 0xFF,
           (err >> 8) & 0xFF);
    if ((err >> 8) & 0x80) printf(" STALL");
    if ((err >> 8) & 0x40) printf(" NAK");
    if ((err >> 8) & 0x04) printf(" TIMEOUT");
    if ((err >> 8) & 0x02) printf(" ERROR");
    printf("\n");
}

static unsigned char rdreg(unsigned char reg)
{
    unsigned long v = 0xEE;                 /* VxD returns a DWORD */
    vxd_ioctl(CFU_RDREG, &reg, 1, &v, 4);
    return (unsigned char)v;
}

static void regs(void)
{
    unsigned char f1, f2;
    printf("  CTL1=%02X INTSTAT=%02X", rdreg(5), rdreg(0x0D));
    f1 = rdreg(0x0F); Sleep(5); f2 = rdreg(0x0F);
    printf(" frame=%02X->%02X %s\n", f1, f2,
           (f1 != f2) ? "(SOF RUNNING)" : "(SOF DEAD?)");
}

int main(int argc, char **argv)
{
    unsigned long e; long g; int p, i;
    unsigned char b[64];
    char path[] = "\\\\.\\CFU1.VXD";

    hVxd = CreateFile(path, 0, 0, NULL, 0, 0, NULL);
    if (hVxd == INVALID_HANDLE_VALUE) { printf("no vxd\n"); return 1; }

    regs();

    /* watch bus presence oscillate: 40 samples, 100ms apart, INTSTAT
     * cleared before each read so the latched bits are fresh.
     * bit7 = D+ (live FS presence); frame reg advancing = SOF alive. */
    if (argc > 1 && !stricmp(argv[1], "/WATCH")) {
        unsigned char is, f1, f2, wr[2];
        for (i = 0; i < 40; i++) {
            wr[0] = 0x0D; wr[1] = 0xFF;         /* clear INTSTAT latches */
            vxd_ioctl(CFU_WRREG, wr, 2, NULL, 0);
            Sleep(3);
            is = rdreg(0x0D);
            f1 = rdreg(0x0F); Sleep(2); f2 = rdreg(0x0F);
            printf("  +%4dms INTSTAT=%02X D+=%u %s SOF=%s\n", i * 100, is,
                   (is >> 7) & 1,
                   (is & 0x80) ? "PRESENT" : "absent ",
                   (f1 != f2) ? "run" : "dead");
            Sleep(95);
        }
        CloseHandle(hVxd);
        return 0;
    }

    /* full clean re-enumeration with the supervisor stopped: shows exactly
     * where the hub stops answering.  CHUBT /REENUM */
    if (argc > 1 && !stricmp(argv[1], "/REENUM")) {
        unsigned char z = 0, cv = 1;
        unsigned long is = 0;
        vxd_ioctl(CFU_AUTOMOUSE, &z, 1, NULL, 0);   /* stop supervisor */
        Sleep(300);
        printf("  supervisor stopped; USBINIT (bus reset + SOF)...\n");
        vxd_ioctl(CFU_USBINIT, &z, 1, &is, 4);
        printf("  USBINIT INTSTAT=%08lX\n", is);
        regs();
        Sleep(100);
        g_mps0 = 8;
        g = ctl(0, 0x80, 6, 0x0100, 0, b, 8, &e);
        perr("GET_DESCRIPTOR(dev,8) @0", g, e);
        if (g >= 8 && b[7]) g_mps0 = b[7];
        g = ctl(0, 0x00, 5, 1, 0, NULL, 0, &e);
        perr("SET_ADDRESS(1)", g, e);
        Sleep(20);
        g = ctl(1, 0x80, 6, 0x0100, 0, b, 8, &e);
        perr("GET_DESCRIPTOR(dev,8) @1", g, e);
        g = ctl(1, 0x80, 6, 0x0200, 0, b, 9, &e);
        perr("GET_DESCRIPTOR(config,9)", g, e);
        if (g >= 6) { cv = b[5]; printf("    -> bConfigurationValue=%u\n", cv); }
        g = ctl(1, 0x00, 9, cv, 0, NULL, 0, &e);
        perr("SET_CONFIGURATION", g, e);
        g = ctl(1, 0x80, 8, 0, 0, b, 1, &e);
        perr("GET_CONFIGURATION", g, e);
        if (g > 0) printf("    -> config=%u\n", b[0]);
        g = ctl(1, 0xA0, 6, 0x2900, 0, b, 12, &e);
        perr("GET_DESCRIPTOR(hub,12)", g, e);
        if (g >= 5) {
            printf("    -> nports=%u wHubChars=%02X%02X pwrswitch=%s\n",
                   b[2], b[4], b[3],
                   (b[3] & 2) ? "NONE (always on)" :
                   ((b[3] & 1) ? "individual" : "ganged"));
        }
        /* the key experiment: port status BEFORE any PORT_POWER */
        for (p = 1; p <= 3; p++) {
            char lbl[40];
            sprintf(lbl, "GET_STATUS(port %d) pre-power", p);
            g = ctl(1, 0xA3, 0, 0, p, b, 4, &e);
            perr(lbl, g, e);
            if (g >= 2) printf("    -> status=%02X%02X%s%s\n", b[1], b[0],
                               (b[0] & 1) ? " CONNECTED" : "",
                               (b[0] & 0x10) ? "" : "");
        }
        g = ctl(1, 0xA0, 0, 0, 0, b, 4, &e);    /* hub-level status */
        perr("GET_STATUS(hub)", g, e);
        if (g >= 4) printf("    -> hubstatus=%02X%02X change=%02X%02X\n",
                           b[1], b[0], b[3], b[2]);
        if (argc > 2 && !stricmp(argv[2], "/NOPOWER")) {
            printf("  (skipping PORT_POWER on request)\n");
            regs();
            printf("  (supervisor left STOPPED; CFUMOUSE /AUTO to restart)\n");
            CloseHandle(hVxd);
            return 0;
        }
        /* power ONE port (argv[2] = port number, default 1) and watch it
         * die (or live) with an immediate 20x50ms status loop */
        p = 1;
        if (argc > 2) p = atoi(argv[2]) ? atoi(argv[2]) : 1;
        printf("  PORT_POWER(%d) then status loop:\n", p);
        g = ctl(1, 0x23, 3, 8, p, NULL, 0, &e);
        perr("SET_FEATURE(PORT_POWER)", g, e);
        for (i = 0; i < 20; i++) {
            g = ctl(1, 0xA3, 0, 0, p, b, 4, &e);
            if (g >= 2) {
                printf("    +%3dms status=%02X%02X change=%02X%02X%s\n",
                       i * 50, b[1], b[0], b[3], b[2],
                       (b[0] & 1) ? " CONNECTED" : "");
                if (b[0] & 1) break;
            } else {
                printf("    +%3dms DEAD (err=%08lX)\n", i * 50, e);
                break;
            }
            Sleep(50);
        }
        /* is the hub gone, or did it reboot back to address 0? */
        Sleep(500);
        g = ctl(1, 0x80, 6, 0x0100, 0, b, 8, &e);
        perr("GET_DESCRIPTOR(dev,8) @1 after", g, e);
        g = ctl(0, 0x80, 6, 0x0100, 0, b, 8, &e);
        perr("GET_DESCRIPTOR(dev,8) @0 after", g, e);
        regs();
        printf("  (supervisor left STOPPED; CFUMOUSE /AUTO to restart)\n");
        CloseHandle(hVxd);
        return 0;
    }

    /* 0: who answers where?  (control-path address sweep) */
    {
        static unsigned char sweep[4] = { 0, 1, 9, 12 };
        char lbl[40];
        int k;
        for (k = 0; k < 4; k++) {
            sprintf(lbl, "GET_DESCRIPTOR(dev,8) @%u", sweep[k]);
            g = ctl(sweep[k], 0x80, 6, 0x0100, 0, b, 8, &e);
            perr(lbl, g, e);
            if (g >= 8)
                printf("    -> cls=%02X mps0=%u\n", b[4], b[7]);
        }
    }

    /* 1: device-level sanity at addr 1 */
    g = ctl(1, 0x80, 6, 0x0100, 0, b, 8, &e);
    perr("GET_DESCRIPTOR(device,8)", g, e);

    g = ctl(1, 0x80, 8, 0, 0, b, 1, &e);            /* GET_CONFIGURATION */
    perr("GET_CONFIGURATION", g, e);
    if (g > 0) printf("    -> current config = %u %s\n", b[0],
                      b[0] ? "(CONFIGURED)" : "(NOT CONFIGURED!)");

    g = ctl(1, 0x80, 6, 0x0200, 0, b, 9, &e);       /* config desc header */
    perr("GET_DESCRIPTOR(config,9)", g, e);
    if (g >= 6) printf("    -> bConfigurationValue = %u\n", b[5]);

    g = ctl(1, 0x80, 0, 0, 0, b, 2, &e);            /* GET_STATUS device */
    perr("GET_STATUS(device)", g, e);

    g = ctl(1, 0xA0, 6, 0x2900, 0, b, 8, &e);       /* hub descriptor */
    perr("GET_DESCRIPTOR(hub)", g, e);

    /* 2: the failing shapes */
    g = ctl(1, 0xA3, 0, 0, 1, b, 4, &e);            /* GET_STATUS port 1 */
    perr("GET_STATUS(port 1)", g, e);
    g = ctl(1, 0x23, 3, 8, 1, NULL, 0, &e);         /* PORT_POWER port 1 */
    perr("SET_FEATURE(PORT_POWER,1)", g, e);

    /* 3: optional re-configure with the descriptor's own config value,
     * then retry the port ops:  CHUBT /FIX */
    if (argc > 1 && !stricmp(argv[1], "/FIX")) {
        unsigned char cv = 1;
        if (ctl(1, 0x80, 6, 0x0200, 0, b, 9, &e) >= 6) cv = b[5];
        g = ctl(1, 0x00, 9, cv, 0, NULL, 0, &e);
        printf("  SET_CONFIGURATION(%u):\n", cv);
        perr("SET_CONFIGURATION", g, e);
        Sleep(50);
        g = ctl(1, 0x80, 8, 0, 0, b, 1, &e);
        perr("GET_CONFIGURATION after", g, e);
        if (g > 0) printf("    -> current config = %u\n", b[0]);
        for (p = 1; p <= 3; p++) {
            ctl(1, 0x23, 3, 8, p, NULL, 0, &e);     /* PORT_POWER */
        }
        Sleep(300);
        for (p = 1; p <= 3; p++) {
            char lbl[32];
            sprintf(lbl, "GET_STATUS(port %d)", p);
            g = ctl(1, 0xA3, 0, 0, p, b, 4, &e);
            perr(lbl, g, e);
            if (g >= 2) {
                printf("    -> status=%02X%02X", b[1], b[0]);
                if (b[0] & 1) printf(" CONNECTED");
                printf("\n");
            }
        }
    }
    CloseHandle(hVxd);
    return 0;
}
