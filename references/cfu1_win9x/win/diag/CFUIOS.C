/* CFUIOS.C - Stage 2b probe: register CFU1.VXD with the Win9x I/O
 * Supervisor (IOS) and report which async event packets (AEPs) arrive.
 * The VxD's AER rejects everything for now; this only proves the linkage.
 * Build: wcc386 -bt=nt -zq CFUIOS.C ; wlink sys nt op q n CFUIOS.EXE f CFUIOS.obj
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
    case 0: return "AEP_INITIALIZE";
    case 2: return "AEP_BOOT_COMPLETE";
    case 3: return "AEP_CONFIG_DCB";
    case 4: return "AEP_UNCONFIG_DCB";
    case 6: return "AEP_DEVICE_INQUIRY";
    default: return "AEP_?";
    }
}
static void dumpstat(void)
{
    DWORD s[20]; int i;
    memset(s, 0, sizeof s);
    if (ioc(CFU_IOSSTAT, NULL, 0, s, sizeof s) != 0) { printf("IOSSTAT failed\n"); return; }
    printf("  total AEPs: %lu   last func: %lu (%s)   reg_result: %lu   DDB: %08lX\n",
           s[0], s[1], aepname((int)s[1]), s[2], s[3]);
    for (i = 0; i < 16; i++)
        if (s[4 + i]) printf("    func %2d (%-18s): %lu\n", i, aepname(i), s[4+i]);
}
int main(void)
{
    DWORD res = 0;
    char path[] = "\\\\.\\CFU1.VXD";
    printf("CFUIOS - IOS registration probe\n");
    hVxd = CreateFile(path, 0, 0, NULL, 0, FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (hVxd == INVALID_HANDLE_VALUE) { printf("cannot open CFU1.VXD (%lu)\n", GetLastError()); return 1; }

    printf("before register:\n"); dumpstat();
    printf("calling IOS_Register...\n");
    if (ioc(CFU_IOSREG, NULL, 0, &res, 4) != 0) { printf("IOSREG ioctl failed (%lu)\n", GetLastError()); CloseHandle(hVxd); return 1; }
    printf("IOS_Register returned reg_result = %lu "
           "(1=REMAIN_RESIDENT 2=MINIMIZE 0/other=rejected)\n", res);
    Sleep(200);
    printf("after register:\n"); dumpstat();
    CloseHandle(hVxd);
    return 0;
}
