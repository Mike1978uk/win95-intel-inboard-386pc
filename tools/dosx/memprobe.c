/* What a 32-bit DOS extender finds on the 5160 under INBRDPC.SYS (P3.2 groundwork).
   The 1986 BIOS has no INT 15h AH=88h and HIMEM reports 0 KB XMS, so the extender may run
   in conventional memory only; this prints what it actually got.
   Build: wcl386 -bt=dos -l=dos32a memprobe.c   (or -l=pmodew)                          */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <i86.h>

static void dpmi_version(void)
{
    union REGS r;
    memset(&r, 0, sizeof r);
    r.w.ax = 0x0400;
    int386(0x31, &r, &r);
    if (r.w.cflag) { printf("DPMI 0400h: failed\n"); return; }
    printf("DPMI version %u.%02u  flags %04Xh  cpu %u86  PIC bases %02Xh/%02Xh\n",
           r.h.ah, r.h.al, r.w.bx, r.h.cl, r.h.dh, r.h.dl);
}

static void dpmi_free(void)
{
    static unsigned long info[12];
    union REGS r;
    struct SREGS s;
    memset(&r, 0, sizeof r);
    segread(&s);
    memset(info, 0xff, sizeof info);
    r.w.ax = 0x0500;
    r.x.edi = (unsigned long)info;
    int386x(0x31, &r, &r, &s);
    if (r.w.cflag) { printf("DPMI 0500h: failed\n"); return; }
    printf("largest free block %lu KB", info[0] / 1024);
    if (info[6] != 0xffffffffUL) printf("  total %lu KB", info[6] * 4);
    if (info[4] != 0xffffffffUL) printf("  free pages %lu KB", info[4] * 4);
    printf("\n");
}

int main(void)
{
    unsigned long total = 0;
    unsigned n = 0;
    void *p, *first = NULL;

    printf("memprobe: 32-bit protected mode reached\n");
    dpmi_version();
    dpmi_free();
    /* 64 KB chunks, kept, until malloc fails: what a program can really use. */
    while ((p = malloc(65536)) != NULL) {
        memset(p, 0x5a, 65536);
        if (!first) first = p;
        total += 64;
        n++;
        if (total >= 16384) break;
    }
    printf("malloc 64 KB chunks: %u (%lu KB), each written\n", n, total);
    printf("first chunk at %08lXh in the flat space\n", (unsigned long)first);
    return 0;
}
