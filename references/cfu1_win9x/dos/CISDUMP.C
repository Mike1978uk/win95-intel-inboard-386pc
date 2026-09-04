/* CISDUMP.C - dump a PCMCIA card's CIS via the Intel 82365 PCIC.
 * Scans sockets 0 and 1 (PCIC at 0x3E0), powers each, maps a window onto the
 * card's ATTRIBUTE memory, walks the CIS tuples and prints them (hex + decoded
 * VERS_1 strings, MANFID, FUNCID). For identifying a card and planning enabler
 * support (e.g. the Ratoc REX-5572 vs REX-5571). Read-only: powers each socket
 * back down after dumping.
 * Build: C:\WATCOM\BLD.BAT CISDUMP
 */
#include <stdio.h>
#include <conio.h>
#include <dos.h>

#define PCIC 0x3E0
static unsigned sockoff;

static void wr(unsigned char i, unsigned char v){ outp(PCIC, i + sockoff); outp(PCIC + 1, v); }
static unsigned char rd(unsigned char i){ outp(PCIC, i + sockoff); return (unsigned char)inp(PCIC + 1); }
static void dly(unsigned n){ while (n--) inp(0x80); }              /* ~1us each */
/* attribute memory: CIS byte i lives at window offset i*2 */
static unsigned char cisb(unsigned seg, unsigned i){ return *(unsigned char __far *)MK_FP(seg, i * 2); }

static int mapwin(unsigned seg)
{
    unsigned start, stop, woff;
    if ((rd(0x01) & 0x0C) != 0x0C) return 0;            /* card-detect: not present */
    wr(0x02, 0x95); dly(20000);                         /* power on, 5V */
    wr(0x03, 0x40); dly(10000);                         /* memory mode, reset released */
    start = seg >> 8; stop = (seg >> 8) + 3;
    woff  = ((unsigned)(0 - (seg >> 8)) & 0x3FFF) | 0x4000;   /* 0x4000 = attribute space */
    wr(0x10, start & 0xFF); wr(0x11, (start >> 8) & 0x3F);
    wr(0x12, stop  & 0xFF); wr(0x13, (stop  >> 8) & 0x3F);
    wr(0x14, woff  & 0xFF); wr(0x15, (woff  >> 8) & 0xFF);
    wr(0x06, rd(0x06) | 0x01);
    dly(20000);
    return 1;
}

static void dumpcis(unsigned seg)
{
    int off = 0, i, code, link, guard = 0;
    for (;;) {
        code = cisb(seg, off);
        if (code == 0xFF) { printf("  TPL FF (end)\n"); break; }
        if (code == 0x00) { off++; if (++guard > 512) break; continue; }
        link = cisb(seg, off + 1);
        printf("  TPL %02X len %2d:", code, link);
        for (i = 0; i < link && i < 24; i++) printf(" %02X", cisb(seg, off + 2 + i));
        printf("\n");
        if (code == 0x15) {                             /* CISTPL_VERS_1 */
            printf("    VERS_1 v%u.%u: ", cisb(seg, off + 2), cisb(seg, off + 3));
            for (i = 4; i < link; i++) { unsigned char c = cisb(seg, off + 2 + i); putchar(c ? c : '|'); }
            printf("\n");
        }
        if (code == 0x20)                               /* CISTPL_MANFID */
            printf("    MANFID: %02X%02X / %02X%02X\n",
                   cisb(seg, off + 3), cisb(seg, off + 2), cisb(seg, off + 5), cisb(seg, off + 4));
        if (code == 0x21)                               /* CISTPL_FUNCID */
            printf("    FUNCID: %02X\n", cisb(seg, off + 2));
        if (link == 0xFF) break;
        off += link + 2;
        if (off > 0x400) break;
    }
}

int main(void)
{
    unsigned sock, seg = 0xD000;
    printf("CISDUMP - REX card CIS reader\n");
    for (sock = 0; sock < 2; sock++) {
        sockoff = sock * 0x40;
        printf("=== Socket %u (0x3E0, bank 0x%02X) ===\n", sock, sockoff);
        if (!mapwin(seg)) { printf("  (no card present)\n"); continue; }
        dumpcis(seg);
        wr(0x06, 0x00); wr(0x03, 0x00); wr(0x02, 0x00);  /* power down */
    }
    return 0;
}
