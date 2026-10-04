/* TXTBENCH.EXE - time text, on-card blits and fills through the display driver (#41).
 *
 * On the Mach8 a blit or a fill is one command to the card, while text the driver has not
 * cached crosses the 8-bit bus as pixels. Timing the three side by side, at Large and Small
 * Fonts, shows how much of Windows' drawing time is text and whether the font size matters.
 *
 *   TXTBENCH <label>     appends one line to C:\TXTBENCH.TXT and exits
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>

#define W_WIDTH   600
#define W_HEIGHT  400
#define REPS_TEXT 40
#define REPS_BLIT 100
#define REPS_FILL 100

static const char line_mixed[] = "The quick brown fox jumps over the lazy dog 0123456789 ABCDEFG";
static const char line_same[]  = "mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm";

static DWORD time_text(HDC dc, const char *s, HFONT font)
{
    DWORD t0;
    int i, y, len = (int) strlen(s);
    TEXTMETRIC tm;
    HFONT old = (HFONT) SelectObject(dc, font);

    GetTextMetrics(dc, &tm);
    GdiFlush();
    t0 = GetTickCount();
    for (i = 0; i < REPS_TEXT; i++) {
        for (y = 0; y + tm.tmHeight <= W_HEIGHT; y += tm.tmHeight)
            ExtTextOut(dc, 0, y, ETO_OPAQUE, NULL, s, len, NULL);
    }
    GdiFlush();
    t0 = GetTickCount() - t0;
    SelectObject(dc, old);
    return t0;
}

static DWORD time_blit(HDC dc)
{
    DWORD t0;
    int i;

    GdiFlush();
    t0 = GetTickCount();
    for (i = 0; i < REPS_BLIT; i++)
        BitBlt(dc, (i & 1) * 300, 200, 300, 200, dc, ((i + 1) & 1) * 300, 0, SRCCOPY);
    GdiFlush();
    return GetTickCount() - t0;
}

static DWORD time_fill(HDC dc)
{
    DWORD t0;
    int i;

    GdiFlush();
    t0 = GetTickCount();
    for (i = 0; i < REPS_FILL; i++)
        PatBlt(dc, 0, 0, W_WIDTH, W_HEIGHT, (i & 1) ? WHITENESS : BLACKNESS);
    GdiFlush();
    return GetTickCount() - t0;
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
    WNDCLASS wc;
    HWND wnd;
    HDC dc;
    FILE *f;
    DWORD sys_mixed, sys_same, fix_mixed, blit, fill;
    int dpi, cx, cy, bpp;

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc   = DefWindowProc;
    wc.hInstance     = inst;
    wc.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = "TxtBench";
    RegisterClass(&wc);
    wnd = CreateWindow("TxtBench", "TXTBENCH", WS_POPUP | WS_VISIBLE, 0, 0, W_WIDTH, W_HEIGHT,
                       NULL, NULL, inst, NULL);
    UpdateWindow(wnd);
    dc = GetDC(wnd);

    sys_mixed = time_text(dc, line_mixed, (HFONT) GetStockObject(SYSTEM_FONT));
    sys_same  = time_text(dc, line_same, (HFONT) GetStockObject(SYSTEM_FONT));
    fix_mixed = time_text(dc, line_mixed, (HFONT) GetStockObject(ANSI_FIXED_FONT));
    blit      = time_blit(dc);
    fill      = time_fill(dc);

    dpi = GetDeviceCaps(dc, LOGPIXELSY);
    cx  = GetDeviceCaps(dc, HORZRES);
    cy  = GetDeviceCaps(dc, VERTRES);
    bpp = GetDeviceCaps(dc, BITSPIXEL) * GetDeviceCaps(dc, PLANES);
    ReleaseDC(wnd, dc);
    DestroyWindow(wnd);

    f = fopen("C:\\TXTBENCH.TXT", "a");
    if (f) {
        fprintf(f, "%s %dx%dx%d dpi=%d text_sys=%lu text_sys_same=%lu text_fixed=%lu blit=%lu fill=%lu ms\n",
                cmd[0] ? cmd : "-", cx, cy, bpp, dpi, sys_mixed, sys_same, fix_mixed, blit, fill);
        fclose(f);
    }
    return 0;
}
