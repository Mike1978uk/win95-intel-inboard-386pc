/* CFUDRV.C - register CFU1.VXD with IOS, keep the VxD resident (no
 * DELETE_ON_CLOSE), and watch the AEP sequence + logical-drive bitmap for a
 * few seconds to see whether IOS drives DEVICE_INQUIRY/CONFIG_DCB and assigns
 * a drive letter to our USB disk.
 * Build: wcc386 -bt=nt -zq CFUDRV.C ; wlink sys nt op q n CFUDRV.EXE f CFUDRV.obj
 */
#include <windows.h>
#include <stdio.h>
#include "cfu1.h"

static HANDLE hVxd;
static int ioc(DWORD code, void *in, DWORD ci, void *out, DWORD co)
{
    DWORD r = 0;
    return DeviceIoControl(hVxd, code, in, ci, out, co, &r, NULL) ? 0 : -1;
}
static const char *aepname(int f)
{
    switch (f) {
    case 0: return "INITIALIZE";
    case 2: return "BOOT_COMPLETE";
    case 3: return "CONFIG_DCB";
    case 4: return "UNCONFIG_DCB";
    case 6: return "DEVICE_INQUIRY";
    default: return "?";
    }
}
static void drvstr(DWORD mask, char *buf)
{
    int i, n = 0;
    for (i = 0; i < 26; i++)
        if (mask & (1UL << i)) buf[n++] = (char)('A' + i);
    buf[n] = 0;
}
int main(void)
{
    DWORD res = 0, s[20];
    DWORD d0, d1;
    char b0[32], b1[32];
    char path[] = "\\\\.\\CFU1.VXD";
    int k;

    printf("CFUDRV - register + watch for drive letter\n");
    hVxd = CreateFile(path, 0, 0, NULL, 0, 0, NULL);   /* no DELETE_ON_CLOSE */
    if (hVxd == INVALID_HANDLE_VALUE) {
        printf("cannot open CFU1.VXD (%lu)\n", GetLastError());
        return 1;
    }
    d0 = GetLogicalDrives();
    drvstr(d0, b0);
    printf("drives before: %s\n", b0);

    if (ioc(CFU_IOSREG, NULL, 0, &res, 4) != 0)
        printf("IOSREG ioctl failed (%lu)\n", GetLastError());
    printf("IOS_Register reg_result = %lu\n", res);

    {
    DWORD r[53];
    int pass;
    for (pass = 0; pass < 2; pass++) {
    if (pass == 1) {
        DWORD bc = 0;
        printf("-- creating logical volume DCB + ISP_ASSOCIATE_DCB --\n");
        if (ioc(CFU_IOSVOL, NULL, 0, &bc, 4) != 0)
            printf("IOSVOL failed (%lu)\n", GetLastError());
        else
            printf("volume: create_result=%u assoc_result=%u\n",
                   (unsigned)(bc >> 16), (unsigned)(bc & 0xFFFF));
    }
    for (k = 1; k <= 8; k++) {
        int i;
        Sleep(1000);
        memset(s, 0, sizeof s);
        ioc(CFU_IOSSTAT, NULL, 0, s, sizeof s);
        memset(r, 0, sizeof r);
        ioc(CFU_IORSTAT, NULL, 0, r, sizeof r);
        d1 = GetLogicalDrives();
        drvstr(d1, b1);
        printf("t+%2ds AEPs=%lu last=%s inq=%lu cfg=%lu isp=%lu IORs=%lu drives=%s\n",
               k, s[0], aepname((int)s[1]), s[4 + 6], s[4 + 3], r[2], r[0], b1);
        if (r[0]) {
            printf("      IOR funcs:");
            for (i = 0; i < 48; i++)
                if (r[5 + i]) printf(" %02X:%lu", i, r[5 + i]);
            printf("  (last=%02lX)\n", r[1]);
        }
        if (d1 != d0) { printf("*** DRIVE BITMAP CHANGED: %s -> %s ***\n", b0, b1); d0 = d1; }
    }
    }
    printf("cfg DCB=%08lX inq DCB=%08lX\n", r[3], r[4]);
    if (r[3]) {
        DWORD pk[2];
        static unsigned char dcb[192];
        int i, j;
        pk[0] = r[3];
        pk[1] = 192;
        if (ioc(CFU_PEEK, pk, 8, dcb, 192) == 0) {
            printf("DCB dump (192 bytes):\n");
            for (i = 0; i < 192; i += 16) {
                printf("  +%02X:", i);
                for (j = 0; j < 16; j++) printf(" %02X", dcb[i + j]);
                printf("\n");
            }
        } else printf("PEEK failed\n");
    }
    }
    CloseHandle(hVxd);
    printf("done (VxD left resident if IOS held it)\n");
    return 0;
}
