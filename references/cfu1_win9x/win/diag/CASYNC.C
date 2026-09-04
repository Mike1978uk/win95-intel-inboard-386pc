/* CASYNC.C - stage-A IRQ-completion mode control + stats.
 * CASYNC /ON | /OFF | (no arg = stats only)
 * Stats: irqcount (raw interrupts), windows completed by IRQ, window
 * timeouts, current mode.  With /ON, run disk/HID traffic and watch
 * ok climb: every USB transaction is now completed by an interrupt.
 */
#include <windows.h>
#include <stdio.h>
#include "cfu1.h"

int main(int argc, char **argv)
{
    static const char *stg[5] = { "none", "CBW-out", "data-out",
                                  "data-in", "CSW-in" };
    HANDLE h; DWORD r = 0, st[7]; unsigned char v;
    char p[] = "\\\\.\\CFU1.VXD";
    h = CreateFile(p, 0, 0, NULL, 0, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { printf("no vxd\n"); return 1; }
    if (argc > 1 && !stricmp(argv[1], "/ON")) {
        v = 1;
        if (DeviceIoControl(h, CFU_ASYNCA, &v, 1, NULL, 0, &r, NULL))
            printf("stage-A IRQ completion ON (hooked, PIC gated)\n");
        else printf("ASYNCA on FAILED (card enabled? IRQ assigned?)\n");
    } else if (argc > 1 && !stricmp(argv[1], "/OFF")) {
        v = 0;
        if (DeviceIoControl(h, CFU_ASYNCA, &v, 1, NULL, 0, &r, NULL))
            printf("stage-A OFF (polled engine, IRQ parked)\n");
        else printf("ASYNCA off FAILED\n");
    }
    if (DeviceIoControl(h, CFU_ASYNCST, NULL, 0, st, sizeof(st), &r, NULL)) {
        printf("irqcount=%lu  irq-completed=%lu  timeouts=%lu  mode=%s\n",
               st[0], st[1], st[2], st[3] ? "ASYNC-A" : "polled");
        printf("canary=%08lX %s\n", st[4],
               st[4] == 0xBEEFCAFEUL ? "(intact)" : "(SEGMENT-END CLOBBERED!)");
        if (st[5])
            printf("last BOT failure: stage %s, SL811 status %02lX%s%s%s%s%s\n",
                   st[5] < 5 ? stg[st[5]] : "?", st[6],
                   (st[6] == 0xFF) ? " (XACT TIMEOUT)" : "",
                   (st[6] == 0xFE) ? " (GARBAGE COUNT REG)" : "",
                   (st[6] < 0xFE && (st[6] & 0x80)) ? " STALL" : "",
                   (st[6] < 0xFE && (st[6] & 0x40)) ? " NAK" : "",
                   (st[6] < 0xFE && (st[6] & 0x04)) ? " TIMEOUT" : "");
    } else printf("ASYNCST ioctl failed (old VxD?)\n");
    CloseHandle(h);
    return 0;
}
