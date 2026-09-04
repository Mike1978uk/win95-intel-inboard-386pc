/* CFUKBD.C - USB keyboard on the RATOC REX-CFU1 -> Windows 98 keystrokes.
 *
 * Uses CFU1.VXD's ring-0 engine: enumerates the keyboard, selects boot
 * protocol, and starts the HID poll loop in keyboard mode.  The VxD diffs
 * each 8-byte boot report, translates HID usages to PC scan codes, and
 * injects them with VKD_Force_Keys (with typematic repeat).  Movement of
 * keystrokes into Windows happens entirely at ring 0, so this program can
 * exit and the keyboard keeps working.
 *
 * Usage:  CFUKBD /TEST        VKD self-test: type "hi" with no USB at all
 *         CFUKBD [/EP n]       enumerate + arm the keyboard (persists)
 *         CFUKBD /STOP         stop the keyboard feed
 *
 * Build:  wcc386 -bt=nt -zq -ox CFUKBD.C
 *         wlink system nt op q name CFUKBD.EXE file CFUKBD.obj lib user32
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cfu1.h"

static HANDLE hVxd = INVALID_HANDLE_VALUE;

static int vxd_ioctl(DWORD code, void *in, DWORD cbin, void *out, DWORD cbout)
{
    DWORD ret = 0;
    if (!DeviceIoControl(hVxd, code, in, cbin, out, cbout, &ret, NULL))
        return -1;
    return 0;
}

static HANDLE load_vxd(void)
{
    char exe[MAX_PATH], path[MAX_PATH + 8];
    char *p;
    HANDLE h;
    GetModuleFileName(NULL, exe, sizeof exe);
    p = strrchr(exe, '\\');
    if (p) *(p + 1) = 0;
    sprintf(path, "\\\\.\\%sCFU1.VXD", exe);
    h = CreateFile(path, 0, 0, NULL, 0, FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (h == INVALID_HANDLE_VALUE)
        h = CreateFile("\\\\.\\CFU1.VXD", 0, 0, NULL, 0,
                       FILE_FLAG_DELETE_ON_CLOSE, NULL);
    return h;
}

static int ctrl_xfer(unsigned char addr, unsigned char mps,
                     const unsigned char *setup, void *out, unsigned wlen,
                     unsigned *got)
{
    CFU_CREQ q;
    static CFU_CRSP r;
    memset(&q, 0, sizeof q);
    q.devaddr = addr;
    q.mps = mps;
    q.wlen = (unsigned short)wlen;
    memcpy(q.setup, setup, 8);
    memset(&r, 0, sizeof r);
    if (vxd_ioctl(CFU_CTRL, &q, sizeof q, &r, sizeof r) != 0) return 0;
    if (r.err) return 0;
    if (got) *got = r.got;
    if (out && r.got) memcpy(out, r.data, r.got > wlen ? wlen : r.got);
    return 1;
}

int main(int argc, char **argv)
{
    static const unsigned char gd8[8] = {0x80,6,0,1,0,0,8,0};
    static const unsigned char sa[8]  = {0x00,5,1,0,0,0,0,0};
    static const unsigned char sc[8]  = {0x00,9,1,0,0,0,0,0};
    static const unsigned char sp[8]  = {0x21,0x0B,0,0,0,0,0,0};  /* boot */
    /* VKD self-test: make/break for H then I (scan set 1: H=23h I=17h) */
    static const unsigned char hi[4]  = {0x23, 0xA3, 0x17, 0x97};
    unsigned char d[16], mps, ks[4];
    unsigned got = 0, i;
    int ep = 1, speed, test = 0, stop = 0, kstat = 0;

    for (i = 1; i < (unsigned)argc; i++) {
        if (!stricmp(argv[i], "/TEST")) test = 1;
        else if (!stricmp(argv[i], "/STOP")) stop = 1;
        else if (!stricmp(argv[i], "/KSTAT")) kstat = 1;
        else if (!stricmp(argv[i], "/EP") && i + 1 < (unsigned)argc)
            ep = atoi(argv[++i]);
    }

    printf("CFUKBD - USB keyboard on the REX-CFU1 -> Windows keystrokes\n");
    hVxd = load_vxd();
    if (hVxd == INVALID_HANDLE_VALUE) {
        printf("cannot load CFU1.VXD (%lu)\n", GetLastError());
        return 1;
    }

    if (test) {
        printf("VKD self-test: injecting 'hi' in 3 seconds - click into an\n"
               "editor / the DOS prompt so you can see it land...\n");
        Sleep(3000);
        vxd_ioctl(CFU_FORCEKEY, (void *)hi, sizeof hi, NULL, 0);
        printf("sent. (this used VKD_Force_Keys only - no USB involved)\n");
        CloseHandle(hVxd);
        return 0;
    }

    if (stop) {
        vxd_ioctl(CFU_HIDSTOP, NULL, 0, NULL, 0);
        printf("keyboard feed stopped.\n");
        CloseHandle(hVxd);
        return 0;
    }

    if (kstat) {
        DWORD c[6];
        memset(c, 0, sizeof c);
        if (vxd_ioctl(CFU_KBDSTAT, NULL, 0, c, sizeof c) != 0) {
            printf("KBDSTAT failed (%lu)\n", GetLastError());
            CloseHandle(hVxd);
            return 1;
        }
        printf("keyboard pipeline counters:\n");
        printf("  reports processed : %lu\n", c[0]);
        printf("  scan bytes enqueued: %lu\n", c[1]);
        printf("  drains scheduled  : %lu\n", c[2]);
        printf("  drain events fired: %lu\n", c[3]);
        printf("  bytes -> VKD      : %lu\n", c[4]);
        printf("  hid_mode          : %lu (2 = keyboard)\n", c[5]);
        CloseHandle(hVxd);
        return 0;
    }

    for (speed = 0; speed < 2; speed++) {
        unsigned char ls = (unsigned char)speed;
        vxd_ioctl(CFU_USBINIT, &ls, 1, NULL, 0);
        if (ctrl_xfer(0, 8, gd8, d, 8, &got) && got >= 8) break;
    }
    if (speed == 2) {
        printf("no USB device answers - is the keyboard plugged into the card?\n");
        CloseHandle(hVxd);
        return 1;
    }
    mps = d[7] ? d[7] : 8;
    printf("device: VID/PID pending, ep0 mps %u, %s-speed\n", mps,
           speed ? "low" : "full");
    if (!ctrl_xfer(0, mps, sa, NULL, 0, NULL)) { printf("SET_ADDRESS failed\n"); return 1; }
    Sleep(20);
    if (!ctrl_xfer(1, mps, sc, NULL, 0, NULL)) { printf("SET_CONFIGURATION failed\n"); return 1; }
    if (ctrl_xfer(1, mps, sp, NULL, 0, NULL))
        printf("boot protocol selected\n");
    else
        printf("device kept report protocol (boot report assumed anyway)\n");

    ks[0] = (unsigned char)ep;
    ks[1] = 1;
    ks[2] = 8;
    ks[3] = 8;
    if (vxd_ioctl(CFU_KBDSTART, ks, 4, NULL, 0) != 0) {
        printf("KBDSTART failed (%lu)\n", GetLastError());
        CloseHandle(hVxd);
        return 1;
    }
    printf("keyboard is LIVE - type on the USB keyboard and it reaches\n"
           "Windows (ring-0 VKD injection).  This program can exit now;\n"
           "the keyboard keeps working.  Stop it with: CFUKBD /STOP\n");
    CloseHandle(hVxd);
    return 0;
}
