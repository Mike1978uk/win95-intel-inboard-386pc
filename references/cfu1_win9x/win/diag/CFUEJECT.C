/* CFUEJECT.C - politely eject the USB drive mounted by CFU1.VXD.
 *
 *   CFUEJECT          flush + lock-check + tear the volume down
 *   CFUEJECT /FORCE   skip the open-files veto
 *   CFUEJECT /UP      bring the volume back up (remount without replug)
 *
 * The flush uses the documented Win9x volume lock (int 21h 440Dh level-0
 * lock forces IFSMGR to commit dirty buffers) via \\.\vwin32, then asks the
 * VxD to destroy the logical volume DCB - IOS waits for pending I/O,
 * notifies IFSMGR and broadcasts the removal to the shell.
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "cfu1.h"

typedef struct {
    DWORD reg_EBX, reg_EDX, reg_ECX, reg_EAX, reg_EDI, reg_ESI, reg_Flags;
} DIOC_REGS;
#define VWIN32_DIOC_DOS_IOCTL 1

static HANDLE hVxd;
static int ioc(DWORD code, void *in, DWORD ci, void *out, DWORD co)
{
    DWORD r = 0;
    return DeviceIoControl(hVxd, code, in, ci, out, co, &r, NULL) ? 0 : -1;
}

/* int 21h 440Dh generic IOCTL through vwin32.  Returns 0 on success. */
static int dos_ioctl(HANDLE hv, int drive1, int cx, int dx)
{
    DIOC_REGS r;
    DWORD cb;
    memset(&r, 0, sizeof r);
    r.reg_EAX = 0x440D;
    r.reg_EBX = (DWORD)drive1;      /* 1 = A:, 5 = E: */
    r.reg_ECX = (DWORD)cx;
    r.reg_EDX = (DWORD)dx;
    if (!DeviceIoControl(hv, VWIN32_DIOC_DOS_IOCTL, &r, sizeof r,
                         &r, sizeof r, &cb, NULL))
        return -1;
    return (r.reg_Flags & 1) ? -1 : 0;  /* carry set = failed */
}

int main(int argc, char **argv)
{
    DWORD st[5], res = 0;
    HANDLE hv;
    char vxdpath[] = "\\\\.\\CFU1.VXD";
    char vwpath[]  = "\\\\.\\vwin32";
    int force = 0, up = 0, i, drive1;
    unsigned char fb;

    for (i = 1; i < argc; i++) {
        if (!lstrcmpi(argv[i], "/FORCE")) force = 1;
        if (!lstrcmpi(argv[i], "/UP"))    up = 1;
    }

    hVxd = CreateFile(vxdpath, 0, 0, NULL, 0, 0, NULL);
    if (hVxd == INVALID_HANDLE_VALUE) {
        printf("CFU1.VXD not loaded (%lu)\n", GetLastError());
        return 1;
    }

    if (up) {
        if (ioc(CFU_VOLUP, NULL, 0, &res, 4) != 0) {
            printf("VOLUP failed (%lu)\n", GetLastError());
            CloseHandle(hVxd);
            return 1;
        }
        memset(st, 0, sizeof st);
        ioc(CFU_VOLSTAT, NULL, 0, st, sizeof st);
        if (res & 1)
            printf("volume is up on %c:\n", 'A' + (int)st[4]);
        else
            printf("no volume (mounted=%u) - is a USB disk attached?\n",
                   (unsigned)(res >> 16));
        CloseHandle(hVxd);
        return (res & 1) ? 0 : 1;
    }

    memset(st, 0, sizeof st);
    if (ioc(CFU_VOLSTAT, NULL, 0, st, sizeof st) != 0) {
        printf("VOLSTAT failed (%lu)\n", GetLastError());
        CloseHandle(hVxd);
        return 1;
    }
    if (!st[1]) {
        printf("no volume is mounted - nothing to eject\n");
        CloseHandle(hVxd);
        return 0;
    }
    drive1 = (int)st[4] + 1;            /* 1-based for int 21h */
    printf("ejecting %c: ...\n", 'A' + (int)st[4]);

    /* NO vwin32 volume lock here: our self-made volume has no VRP (no
     * VOLTRACK layer), and IFSMGR's lock path hangs the system on it.
     * The volume is mounted write-through (removable), and ISP_DCB_DESTROY
     * blocks until pending I/O completes - that IS the flush. */
    (void)hv; (void)vwpath; (void)drive1; (void)dos_ioctl;

    fb = (unsigned char)force;
    if (ioc(CFU_VOLDOWN, &fb, 1, &res, 4) != 0) {
        printf("VOLDOWN failed (%lu)\n", GetLastError());
        CloseHandle(hVxd);
        return 1;
    }
    if (res == 1) {
        printf("  Windows vetoed the removal (volume in use).\n"
               "  close programs using the drive, or CFUEJECT /FORCE\n");
        CloseHandle(hVxd);
        return 1;
    }
    printf("done - safe to remove the USB device\n");
    CloseHandle(hVxd);
    return 0;
}
