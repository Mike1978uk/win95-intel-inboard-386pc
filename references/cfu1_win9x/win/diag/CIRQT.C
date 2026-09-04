/* CIRQT.C - safe one-shot IRQ delivery test.  Usage: CIRQT <irq>
 * The VxD takes exclusive chip ownership, hooks + steers the IRQ, counts
 * SOF interrupts (~1000/s expected if delivery works) for ~1s, then parks
 * and unhooks.  See win/ASYNC-ENGINE.md stage 1. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "cfu1.h"

int main(int argc, char **argv)
{
    HANDLE h;
    DWORD n = 0, r = 0;
    unsigned char irq;
    char p[] = "\\\\.\\CFU1.VXD";
    if (argc < 2) { printf("usage: CIRQT <irq>\n"); return 1; }
    irq = (unsigned char)atoi(argv[1]);
    h = CreateFile(p, 0, 0, NULL, 0, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { printf("no vxd\n"); return 1; }
    if (!DeviceIoControl(h, CFU_IRQTEST, &irq, 1, &n, 4, &r, NULL))
        printf("IRQTEST ioctl failed (%lu)\n", GetLastError());
    else if (n == 0xFFFFFFFFUL)
        printf("IRQ%u: test could not run (hook failed or chip busy)\n", irq);
    else
        printf("IRQ%u: %lu interrupts in ~1s (expect ~1000 if delivery works)\n",
               irq, n);
    CloseHandle(h);
    return 0;
}
