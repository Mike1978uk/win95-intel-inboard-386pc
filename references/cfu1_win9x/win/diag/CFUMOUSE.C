/* CFUMOUSE.C - drive the Windows 98 pointer with a USB mouse attached to
 * the RATOC REX-CFU1's USB port.
 *
 * Uses CFU1.VXD's ring-0 engine: enumerates the device (address 1,
 * configuration 1, boot protocol if the device accepts it), starts the
 * in-VxD HID poll loop (CFU_HIDSTART), then drains reports and feeds
 * them to Windows with mouse_event() - movement and all three buttons.
 *
 * Usage:  CFUMOUSE [seconds] [/EP n]     (default 60s, endpoint 1)
 * Stop early with ESC.
 *
 * Build:  wcc386 -bt=nt -zq -ox CFUMOUSE.C
 *         wlink system nt option quiet name CFUMOUSE.EXE file CFUMOUSE.obj
 *                library user32.lib
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
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
    static const unsigned char gd8[8]  = {0x80,6,0,1,0,0,8,0};
    static const unsigned char sa[8]   = {0x00,5,1,0,0,0,0,0};
    static const unsigned char sc[8]   = {0x00,9,1,0,0,0,0,0};
    static const unsigned char sp[8]   = {0x21,0x0B,0,0,0,0,0,0}; /* boot */
    unsigned char d[16], mps, hs[5];
    unsigned char rbuf[4 + 32 * 9];
    unsigned got = 0, i, o;
    unsigned prevbtn = 0;
    unsigned long nrep = 0, nmove = 0;
    int seconds = 60, ep = 1, speed, vmouse = 0, stop = 0, auto_ = 0, noauto = 0;
    DWORD t0, st;

    for (i = 1; i < (unsigned)argc; i++) {
        if (!stricmp(argv[i], "/EP") && i + 1 < (unsigned)argc)
            ep = atoi(argv[++i]);
        else if (!stricmp(argv[i], "/VMOUSE")) vmouse = 1;
        else if (!stricmp(argv[i], "/STOP")) stop = 1;
        else if (!stricmp(argv[i], "/AUTO")) auto_ = 1;
        else if (!stricmp(argv[i], "/NOAUTO")) noauto = 1;
        else seconds = atoi(argv[i]);
    }
    if (seconds <= 0) seconds = 60;

    printf("CFUMOUSE - USB mouse on the REX-CFU1 -> Windows pointer\n");
    hVxd = load_vxd();
    if (hVxd == INVALID_HANDLE_VALUE) {
        printf("cannot load CFU1.VXD (%lu)\n", GetLastError());
        return 1;
    }

    if (stop) {
        vxd_ioctl(CFU_HIDSTOP, NULL, 0, NULL, 0);
        printf("ring-0 pointer feed stopped.\n");
        CloseHandle(hVxd);
        return 0;
    }

    if (noauto) {
        unsigned char off = 0;
        vxd_ioctl(CFU_AUTOMOUSE, &off, 1, NULL, 0);
        printf("auto-mouse supervisor disabled.\n");
        CloseHandle(hVxd);
        return 0;
    }

    if (auto_) {
        unsigned char on = 1;
        static const char *sname[] = {
            "idle (watching for a mouse)", "bus reset", "get descriptor",
            "set address", "set config", "set protocol / arm", "?", "?",
            "?", "RUNNING (mouse live)" };
        DWORD s;
        int k;
        if (vxd_ioctl(CFU_AUTOMOUSE, &on, 1, NULL, 0) != 0) {
            printf("AUTOMOUSE failed (%lu)\n", GetLastError());
            CloseHandle(hVxd);
            return 1;
        }
        printf("auto-mouse supervisor ENABLED - plug a USB mouse into the\n"
               "card anytime and it becomes a working mouse, no commands.\n"
               "This survives app exit and reboots (armed from CONFIG_START).\n");
        for (k = 0; k < 12; k++) {      /* show it enumerate, if present now */
            s = 0;
            vxd_ioctl(CFU_AUTOSTAT, NULL, 0, &s, 4);
            printf("  state: %s%s\n", sname[(s >> 8) & 15],
                   (s & 2) ? "  [ARMED]" : "");
            if (s & 2) break;
            Sleep(400);
        }
        printf("(disable with CFUMOUSE /NOAUTO)\n");
        CloseHandle(hVxd);
        return 0;
    }

    /* enumerate: probe speed, address 1, configured, boot protocol */
    for (speed = 0; speed < 2; speed++) {
        unsigned char ls = (unsigned char)speed;
        st = 0;
        vxd_ioctl(CFU_USBINIT, &ls, 1, &st, 4);
        if (ctrl_xfer(0, 8, gd8, d, 8, &got) && got >= 8) break;
    }
    if (speed == 2) {
        printf("no USB device answers - is the mouse plugged into the card?\n");
        CloseHandle(hVxd);
        return 1;
    }
    mps = d[7] ? d[7] : 8;
    if (!ctrl_xfer(0, mps, sa, NULL, 0, NULL)) { printf("SET_ADDRESS failed\n"); return 1; }
    Sleep(20);
    if (!ctrl_xfer(1, mps, sc, NULL, 0, NULL)) { printf("SET_CONFIGURATION failed\n"); return 1; }
    if (ctrl_xfer(1, mps, sp, NULL, 0, NULL))
        printf("boot protocol selected\n");
    else
        printf("device kept report protocol (that's fine)\n");

    hs[0] = (unsigned char)ep;          /* endpoint       */
    hs[1] = 1;                          /* device address */
    hs[2] = 8;                          /* mps            */
    hs[3] = 8;                          /* poll interval ms */
    hs[4] = (unsigned char)(vmouse ? 1 : 0);   /* mode: VMOUSE post */
    if (vxd_ioctl(CFU_HIDSTART, hs, 5, NULL, 0) != 0) {
        printf("HIDSTART failed (%lu)\n", GetLastError());
        CloseHandle(hVxd);
        return 1;
    }

    if (vmouse) {
        printf("ring-0 pointer feed is LIVE (VMOUSE injection in the VxD).\n"
               "This program can exit - the mouse keeps working.\n"
               "Stop it later with: CFUMOUSE /STOP\n");
        CloseHandle(hVxd);
        return 0;
    }

    printf("driving the pointer for %ds - move the USB mouse! (ESC stops)\n",
           seconds);
    t0 = GetTickCount();
    while (GetTickCount() - t0 < (DWORD)seconds * 1000) {
        DWORD n;
        int quit = 0;
        while (kbhit())                 /* drain the console queue too, so */
            if (getch() == 27) quit = 1;/* keys don't replay at the prompt */
        if (quit) break;
        Sleep(15);
        memset(rbuf, 0, sizeof rbuf);
        if (vxd_ioctl(CFU_HIDREAD, NULL, 0, rbuf, sizeof rbuf) != 0) break;
        n = *(DWORD *)rbuf;
        for (i = 0, o = 4; i < n; i++, o += 9) {
            unsigned char len = rbuf[o];
            unsigned char *r = rbuf + o + 1;
            int dx = 0, dy = 0;
            unsigned btn = 0;
            if (len >= 8) {             /* 16-bit report protocol */
                btn = r[0];
                dx = (short)(r[2] | (r[3] << 8));
                dy = (short)(r[4] | (r[5] << 8));
            } else if (len >= 3) {      /* boot protocol */
                btn = r[0];
                dx = (signed char)r[1];
                dy = (signed char)r[2];
            } else
                continue;
            nrep++;
            if (dx || dy) {
                mouse_event(MOUSEEVENTF_MOVE, dx, dy, 0, 0);
                nmove++;
            }
            if ((btn ^ prevbtn) & 1)
                mouse_event((btn & 1) ? MOUSEEVENTF_LEFTDOWN
                                      : MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            if ((btn ^ prevbtn) & 2)
                mouse_event((btn & 2) ? MOUSEEVENTF_RIGHTDOWN
                                      : MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
            if ((btn ^ prevbtn) & 4)
                mouse_event((btn & 4) ? MOUSEEVENTF_MIDDLEDOWN
                                      : MOUSEEVENTF_MIDDLEUP, 0, 0, 0, 0);
            prevbtn = btn;
        }
    }

    vxd_ioctl(CFU_HIDSTOP, NULL, 0, NULL, 0);
    printf("%lu report(s), %lu movement(s) injected\n", nrep, nmove);
    CloseHandle(hVxd);
    return 0;
}
