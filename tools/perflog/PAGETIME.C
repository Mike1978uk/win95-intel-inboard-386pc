/*
 * PAGETIME - time how long programs take to open and to come back.
 *
 *   PAGETIME label [ZIP]
 *
 * RAMBASE waits fixed times between steps, so its wall time cannot show a
 * speed-up; this is the clock for an A/B. It opens Paint Shop Pro, WordPad,
 * Notepad and Paint one after another, each as soon as the last is ready,
 * then switches back to each in turn twice: with all four open, the
 * switches are where pages come back from the swap file. With ZIP given and
 * them still open, WinZip zips C:\SBPRO and unzips it again, real work
 * competing for the same RAM. A third switch pass follows. Then it closes
 * them. One line per step is appended to C:\PAGETIME.TXT:
 *
 *   label,open,WordPad,4120       milliseconds
 *   label,open,total,23510
 *
 * "Ready" is the program's own window: visible, answering a sent message,
 * and with nothing left to paint. WaitForInputIdle alone is not enough,
 * because Paint Shop Pro is a 16-bit program. Paint is started as
 * MSPAINT.EXE, since PBRUSH.EXE only launches it and exits.
 *
 * WinZip here is the evaluation copy, which shows a licence dialog on every
 * start; PAGETIME presses its "I Agree" button, as the run must be hands
 * off. The time to that dialog is logged as its own step (zipnag), since
 * it is not paging work. The press does not always land, which stops a run
 * for a person to click, so the WinZip steps are off unless asked for.
 *
 * Every wait gives up (WinZip after 10 minutes, the rest after 2) and the
 * step is logged as -1, so a program that never appears cannot leave a
 * run stuck.
 *
 * Built without a C runtime (build.sh), like PERFLOG.
 */

typedef unsigned long DWORD;
typedef void *HANDLE;
typedef void *HWND;
typedef int BOOL;
typedef long LPARAM;
typedef void *HKEY;
typedef long LONG;

#define WINAPI __attribute__((dllimport, stdcall))
#define CALLBACK __attribute__((stdcall))
#define WM_NULL 0x0000
#define WM_CLOSE 0x0010
#define SMTO_NORMAL 0
#define RDW_INVALIDATE 0x0001
#define RDW_ERASE 0x0004
#define RDW_ALLCHILDREN 0x0080
#define RDW_UPDATENOW 0x0100
#define RDW_FRAME 0x0400
#define LIMIT_MS 120000
#define ZIP_LIMIT_MS 600000
#define BM_CLICK 0x00F5
#define HKEY_DYN_DATA ((HKEY)0x80000006)
#define KEY_READ 0x20019

typedef struct {
    DWORD cb;
    char *lpReserved, *lpDesktop, *lpTitle;
    DWORD dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars;
    DWORD dwFillAttribute, dwFlags;
    unsigned short wShowWindow, cbReserved2;
    void *lpReserved2;
    HANDLE hStdInput, hStdOutput, hStdError;
} STARTUPINFOA;

typedef struct {
    HANDLE hProcess, hThread;
    DWORD dwProcessId, dwThreadId;
} PROCESS_INFORMATION;

typedef BOOL (CALLBACK *WNDENUMPROC)(HWND, LPARAM);

WINAPI void   ExitProcess(unsigned);
WINAPI void   Sleep(DWORD);
WINAPI DWORD  GetTickCount(void);
WINAPI char  *GetCommandLineA(void);
WINAPI void   GetLocalTime(void *);
WINAPI LONG   RegOpenKeyExA(HKEY, const char *, DWORD, DWORD, HKEY *);
WINAPI LONG   RegQueryValueExA(HKEY, const char *, DWORD *, DWORD *, void *, DWORD *);
WINAPI LONG   RegCloseKey(HKEY);
WINAPI HANDLE GetStdHandle(DWORD);
WINAPI int    WriteFile(HANDLE, const void *, DWORD, DWORD *, void *);
WINAPI HANDLE CreateFileA(const char *, DWORD, DWORD, void *, DWORD, DWORD, HANDLE);
WINAPI DWORD  SetFilePointer(HANDLE, long, long *, DWORD);
WINAPI BOOL   CloseHandle(HANDLE);
WINAPI DWORD  WaitForSingleObject(HANDLE, DWORD);
WINAPI BOOL   DeleteFileA(const char *);
WINAPI BOOL   CreateProcessA(const char *, char *, void *, void *, BOOL, DWORD,
                             void *, const char *, STARTUPINFOA *, PROCESS_INFORMATION *);
WINAPI DWORD  WaitForInputIdle(HANDLE, DWORD);
WINAPI BOOL   EnumWindows(WNDENUMPROC, LPARAM);
WINAPI BOOL   EnumChildWindows(HWND, WNDENUMPROC, LPARAM);
WINAPI int    GetWindowTextA(HWND, char *, int);
WINAPI BOOL   IsWindowVisible(HWND);
WINAPI BOOL   PostMessageA(HWND, unsigned, unsigned, LPARAM);
WINAPI long   SendMessageTimeoutA(HWND, unsigned, unsigned, LPARAM, unsigned, unsigned, DWORD *);
WINAPI BOOL   SetForegroundWindow(HWND);
WINAPI BOOL   RedrawWindow(HWND, const void *, void *, unsigned);
WINAPI BOOL   GetUpdateRect(HWND, void *, BOOL);
__attribute__((dllimport)) int __cdecl wsprintfA(char *, const char *, ...);
typedef struct { unsigned short y, mo, dow, d, h, mi, s, ms; } SYSTEMTIME;

/* The title text identifies the window; "- Paint" does not match "Paint Shop Pro". */
static struct { const char *name, *cmd, *title; } prog[] = {
    { "PSP",     "C:\\PSP\\PSP.EXE", "Paint Shop Pro" },
    { "WordPad", "\"C:\\Program Files\\Accessories\\WORDPAD.EXE\"", "WordPad" },
    { "Notepad", "C:\\WINDOWS\\NOTEPAD.EXE C:\\WINDOWS\\WIN.INI", "Notepad" },
    { "Paint",   "\"C:\\Program Files\\Accessories\\MSPAINT.EXE\"", "- Paint" },
};
#define NPROG (sizeof prog / sizeof prog[0])

static HANDLE out, logf;
static char label[32];
static const char *want;
static HWND hit;

static char lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

static int len(const char *s)
{
    int n = 0;
    while (s[n])
        n++;
    return n;
}

static BOOL CALLBACK visit(HWND h, LPARAM lp)
{
    char title[128];
    const char *t, *w, *p;

    (void)lp;
    if (!IsWindowVisible(h) || GetWindowTextA(h, title, sizeof title) <= 0)
        return 1;
    for (p = title; *p; p++) {
        for (t = p, w = want; *w && lower(*t) == lower(*w); t++, w++)
            ;
        if (*w == 0) {
            hit = h;
            return 0;
        }
    }
    return 1;
}

static HWND find(const char *title)
{
    want = title;
    hit = 0;
    EnumWindows(visit, 0);
    return hit;
}

/* The window's thread has run its message loop and painted everything. */
static int settle(HWND h, DWORD t0)
{
    DWORD r;

    if (!SendMessageTimeoutA(h, WM_NULL, 0, 0, SMTO_NORMAL, LIMIT_MS, &r))
        return 0;
    while (GetUpdateRect(h, 0, 0)) {
        if (GetTickCount() - t0 > LIMIT_MS)
            return 0;
        Sleep(10);
    }
    return 1;
}

static void put(const char *phase, const char *name, long ms)
{
    char b[96];
    DWORD w;

    wsprintfA(b, "%s,%s,%s,%ld\r\n", label, phase, name, ms);
    WriteFile(out, b, len(b), &w, 0);
    WriteFile(logf, b, len(b), &w, 0);
}

static long open_one(unsigned i)
{
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    char cmd[128];
    DWORD t0;
    HWND h;
    unsigned k;

    for (k = 0; k < sizeof si; k++)
        ((char *)&si)[k] = 0;
    si.cb = sizeof si;
    for (k = 0; prog[i].cmd[k]; k++)
        cmd[k] = prog[i].cmd[k];
    cmd[k] = 0;

    t0 = GetTickCount();
    if (!CreateProcessA(0, cmd, 0, 0, 0, 0, 0, 0, &si, &pi))
        return -1;
    WaitForInputIdle(pi.hProcess, LIMIT_MS);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    while (!(h = find(prog[i].title))) {
        if (GetTickCount() - t0 > LIMIT_MS)
            return -1;
        Sleep(50);
    }
    if (!settle(h, t0))
        return -1;
    return (long)(GetTickCount() - t0);
}

static long switch_one(unsigned i)
{
    DWORD t0;
    HWND h = find(prog[i].title);

    if (!h)
        return -1;
    t0 = GetTickCount();
    SetForegroundWindow(h);
    RedrawWindow(h, 0, 0, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME
                 | RDW_ALLCHILDREN | RDW_UPDATENOW);
    if (!settle(h, t0))
        return -1;
    return (long)(GetTickCount() - t0);
}

#define WINZIP "\"C:\\Program Files\\WinZip\\WINZIP32.EXE\" -min "

/* The licence dialog's button; its text carries the accelerator ampersand. */
static BOOL CALLBACK agree_child(HWND h, LPARAM lp)
{
    char t[16];
    static const char want_text[] = "I &Agree";
    int k;

    if (GetWindowTextA(h, t, sizeof t) != sizeof want_text - 1)
        return 1;
    for (k = 0; want_text[k] && t[k] == want_text[k]; k++)
        ;
    if (want_text[k])
        return 1;
    *(HWND *)lp = h;
    return 0;
}

static BOOL CALLBACK agree_top(HWND h, LPARAM lp)
{
    if (IsWindowVisible(h))
        EnumChildWindows(h, agree_child, lp);
    return *(HWND *)lp == 0;
}

/* Run one WinZip command to completion, pressing "I Agree" when it asks. */
static void winzip(const char *step, const char *args)
{
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    char cmd[160], nag[16];
    const char *p;
    DWORD t0;
    HWND btn;
    long nag_ms = -1, ms = -1;
    unsigned k;

    for (k = 0; k < sizeof si; k++)
        ((char *)&si)[k] = 0;
    si.cb = sizeof si;
    k = 0;
    for (p = WINZIP; *p; p++)
        cmd[k++] = *p;
    for (p = args; *p; p++)
        cmd[k++] = *p;
    cmd[k] = 0;

    t0 = GetTickCount();
    if (CreateProcessA(0, cmd, 0, 0, 0, 0, 0, 0, &si, &pi)) {
        CloseHandle(pi.hThread);
        while (WaitForSingleObject(pi.hProcess, 100) != 0) {
            if (GetTickCount() - t0 > ZIP_LIMIT_MS)
                break;
            if (nag_ms < 0) {
                btn = 0;
                EnumWindows(agree_top, (LPARAM)&btn);
                if (btn) {
                    nag_ms = (long)(GetTickCount() - t0);
                    PostMessageA(btn, BM_CLICK, 0, 0);
                }
            }
        }
        if (GetTickCount() - t0 <= ZIP_LIMIT_MS)
            ms = (long)(GetTickCount() - t0);
        CloseHandle(pi.hProcess);
    }
    wsprintfA(nag, "%snag", step);
    put("zip", nag, nag_ms);
    put("zip", step, ms);
}

/* SHADRAM's state this boot, from its own counters: -S<pages added> if it
 * loaded, -N if not. With the start time it lets one appended log hold both
 * arms of an A/B without renaming. */
static void shadram_tag(char *s)
{
    HKEY k;
    DWORD v = 0, sz = sizeof v, type;
    const char *name = "SHADRAM\\PagesAdded";
    int found = 0;
    if (RegOpenKeyExA(HKEY_DYN_DATA, "PerfStats\\StartStat", 0, KEY_READ, &k) == 0) {
        found = RegQueryValueExA(k, name, 0, &type, &v, &sz) == 0;
        RegCloseKey(k);
    }
    if (found && RegOpenKeyExA(HKEY_DYN_DATA, "PerfStats\\StatData", 0, KEY_READ, &k) == 0) {
        sz = sizeof v;
        RegQueryValueExA(k, name, 0, &type, &v, &sz);
        RegCloseKey(k);
    }
    if (found)
        wsprintfA(s, "-S%lu", v);
    else
        wsprintfA(s, "-N");
}

static void pass(const char *phase, long (*step)(unsigned))
{
    long ms, total = 0;
    unsigned i;

    for (i = 0; i < NPROG; i++) {
        ms = step(i);
        put(phase, prog[i].name, ms);
        if (ms < 0 || total < 0)
            total = -1;
        else
            total += ms;
    }
    put(phase, "total", total);
}

void start(void)
{
    char *cl = GetCommandLineA(), *p;
    unsigned i, k;
    int zip = 0;
    SYSTEMTIME st;
    DWORD w;

    /* Skip the program name, quoted or not. */
    if (*cl == '"') {
        for (cl++; *cl && *cl != '"'; cl++)
            ;
        if (*cl)
            cl++;
    } else {
        while (*cl && *cl != ' ')
            cl++;
    }
    while (*cl == ' ')
        cl++;
    for (p = cl; *p && *p != ' ' && *p != '\r' && *p != '\n'; p++)
        ;
    if (*p == ' ') {
        *p++ = 0;
        while (*p == ' ')
            p++;
        zip = (p[0] | 32) == 'z' && (p[1] | 32) == 'i' && (p[2] | 32) == 'p';
    } else {
        *p = 0;
    }
    out = GetStdHandle((DWORD)-11);
    if (*cl == 0) {
        WriteFile(out, "Usage: PAGETIME label\r\n", 23, &w, 0);
        ExitProcess(2);
    }
    /* The start time makes every run's label unique in the appended log. */
    for (k = 0; k < 15 && cl[k]; k++)
        label[k] = cl[k];
    shadram_tag(label + k);
    while (label[k])
        k++;
    GetLocalTime(&st);
    wsprintfA(label + k, "-%02u%02u%02u", st.h, st.mi, st.s);
    logf = CreateFileA("C:\\PAGETIME.TXT", 0x40000000, 1, 0, 4 /* OPEN_ALWAYS */, 0x80, 0);
    if (logf == (HANDLE)-1)
        ExitProcess(1);
    SetFilePointer(logf, 0, 0, 2 /* FILE_END */);

    pass("open", open_one);
    pass("switch1", switch_one);
    pass("switch2", switch_one);
    if (zip) {
        DeleteFileA("C:\\TEMP\\PT.ZIP");
        winzip("zip", "-a -r -p C:\\TEMP\\PT.ZIP C:\\SBPRO\\*.*");
        winzip("unzip", "-e -o C:\\TEMP\\PT.ZIP C:\\TEMP\\PTX");
    }
    pass("switch3", switch_one);

    for (i = 0; i < NPROG; i++) {
        HWND h = find(prog[i].title);
        if (h)
            PostMessageA(h, WM_CLOSE, 0, 0);
    }
    for (k = 0; k < 600; k++) {
        for (i = 0; i < NPROG && !find(prog[i].title); i++)
            ;
        if (i == NPROG)
            break;
        Sleep(100);
    }
    CloseHandle(logf);
    ExitProcess(0);
}
