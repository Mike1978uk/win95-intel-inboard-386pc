/* CFUWT.C - ring-0 BOT write/read loopback: WRITE(10) a known pattern to a
 * scratch LBA in the MBR gap (unused by the FS), READ(10) back, compare.
 * Isolates the BOT OUT data path from VFAT/IOS entirely. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "cfu1.h"

static HANDLE hV;
static unsigned char ib[532], ob[532];

static int bot(int wr, unsigned long lba, unsigned char *data)
{
    DWORD r = 0;
    memset(ib, 0, sizeof ib);
    ib[0] = 10;                 /* cdblen */
    ib[1] = wr ? 0 : 1;         /* dir: 0=OUT 1=IN */
    ib[2] = 0x00; ib[3] = 0x02; /* datalen 512 LE */
    ib[4] = wr ? 0x2A : 0x28;   /* WRITE(10)/READ(10) */
    ib[6] = (unsigned char)(lba >> 24);
    ib[7] = (unsigned char)(lba >> 16);
    ib[8] = (unsigned char)(lba >> 8);
    ib[9] = (unsigned char)lba;
    ib[12] = 1;                 /* 1 sector */
    if (wr) memcpy(ib + 20, data, 512);
    memset(ob, 0xEE, sizeof ob);
    if (!DeviceIoControl(hV, CFU_MSCCMD, ib, wr ? 532 : 20, ob, 516, &r, NULL))
        return -1;
    if (!wr) memcpy(data, ob + 4, 512);
    return (int)*(DWORD *)ob;   /* csw status */
}

int main(void)
{
    static unsigned char w[512], rd[512];
    unsigned long lba = 2000;   /* MBR gap: unused, safe scratch */
    int i, bad = 0, st;
    char p[] = "\\\\.\\CFU1.VXD";
    hV = CreateFile(p, 0, 0, NULL, 0, 0, NULL);
    if (hV == INVALID_HANDLE_VALUE) { printf("no vxd\n"); return 1; }
    for (i = 0; i < 512; i++) w[i] = (unsigned char)(i ^ (i >> 3));
    st = bot(1, lba, w);
    printf("WRITE(10) lba=%lu csw=%d\n", lba, st);
    st = bot(0, lba, rd);
    printf("READ(10)  lba=%lu csw=%d\n", lba, st);
    for (i = 0; i < 512; i++) if (rd[i] != w[i]) bad++;
    printf("compare: %d/512 bytes wrong\n", bad);
    if (bad) {
        printf("first diffs (ofs: wrote != read):\n");
        for (i = 0; i < 512 && bad; i++)
            if (rd[i] != w[i]) {
                printf("  %3d: %02X != %02X\n", i, w[i], rd[i]);
                if (--bad < 512 - 40) break;    /* cap output */
            }
        printf("read[0..31]: ");
        for (i = 0; i < 32; i++) printf("%02X", rd[i]);
        printf("\n");
    } else printf("*** BOT WRITE PATH IS BYTE-EXACT ***\n");
    CloseHandle(hV);
    return 0;
}
