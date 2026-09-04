/* CFUSOF.C - minimal SL811 SOF starter for an ALREADY-ENABLED REX-CFU1.
 * No PCIC access at all (safe inside a Win9x DOS box where the socket
 * controller is virtualized but card I/O is not).  Runs the whole SOF
 * start sequence at native port-I/O speed to rule out the millisecond
 * gaps the ring-3 ioctl path inserts between register writes.
 * Usage: CFUSOF [iobase-hex]   (default 280)
 * Build: C:\WATCOM\BLD.BAT CFUSOF
 */
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>

static unsigned iob = 0x280;
static unsigned char slr(unsigned char r){ outp(iob, r); return (unsigned char)inp(iob+1); }
static void slw(unsigned char r, unsigned char v){ outp(iob, r); outp(iob+1, v); }
static void dly(unsigned n){ while (n--) inp(0x80); }

int main(int argc, char **argv)
{
    unsigned char hw, s1, s2, f1, f2;
    if (argc > 1) iob = (unsigned)strtoul(argv[1], NULL, 16);
    hw = slr(0x0E);
    printf("CFUSOF @%03X: hwrev %02X\n", iob, hw);
    if ((hw >> 4) != 2 && (hw >> 4) != 1) { printf("no SL811 here\n"); return 1; }

    slw(0x06, 0x00);            /* INTENA off       */
    slw(0x0E, 0xE0);            /* SOF counter low  */
    slw(0x0F, 0xAE);            /* CTL2: host+SOFhi */
    slw(0x02, 0x00);            /* EP A len 0       */
    outp(iob+1, 0x50);          /* autoinc: PID=SOF */
    outp(iob+1, 0x00);          /*          devaddr */
    slw(0x00, 0x01);            /* ARM              */
    slw(0x05, 0x01);            /* CTL1: SOF_ENA    */
    slw(0x0D, 0xFF);            /* clear INTSTAT    */
    slw(0x06, 0x10);            /* INTENA: SOF timer gate */
    dly(10000);                 /* ~10ms */
    s1 = slr(0x0D);
    f1 = slr(0x0F);
    dly(3000);
    s2 = slr(0x0D);
    f2 = slr(0x0F);
    printf("INTSTAT %02X -> %02X   frame timer %02X -> %02X\n", s1, s2, f1, f2);
    printf("EP A ctl now: %02X (ARM auto-clears if the engine ran)\n", slr(0x00));
    printf("%s\n", (s1 & 0x10) ? "*** SOF RUNNING ***" : "SOF dead");
    slw(0x06, 0x00);
    slw(0x0D, 0xFF);
    return 0;
}
