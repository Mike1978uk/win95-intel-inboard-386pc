/*
 * EIPSAMP - where a Windows 95 program's main thread spends its time, by sampling its
 * instruction pointer (worklist D14 a: D3DIM's share of a Direct3D frame).
 *
 *   EIPSAMP seconds interval_ms command line...
 *
 * Starts the program itself (so it holds the main thread's handle; Windows 95 has no
 * OpenThread), then every interval suspends that thread, reads CS:EIP and resumes it, for the
 * given seconds or until the program ends. Writes C:\EIPSAMP.TXT: the program's modules
 * (Toolhelp, taken every few seconds while it runs, the last one kept), then one line a sample.
 * A 16-bit CS (the display driver, USER, GDI) is written as is; tools/eipsamp/eipsum.py sums
 * samples by module, and by export within a module.
 *
 * Time a thread spends in a VxD shows at the ring-3 instruction it entered from. The program
 * is not closed at the end: closing a full-screen DirectX program is the caller's (WINCLOSE).
 *
 * Built without a C runtime (build.sh), like PERFLOG.
 */

typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef long LONG;
typedef void *HANDLE;

#define WINAPI __attribute__((dllimport, stdcall))

typedef struct {
    DWORD cb;
    char *res, *desk, *title;
    DWORD x, y, xs, ys, xc, yc, fill, flags;
    WORD show, res2;
    void *res3;
    HANDLE in, out, err;
} STARTUPINFOA;
typedef struct { HANDLE proc, thread; DWORD pid, tid; } PROCESS_INFORMATION;

/* The i386 CONTEXT as Windows 95 has it (no extended registers): 204 bytes. */
typedef struct {
    DWORD flags;
    DWORD dr[6];
    unsigned char fsave[112];
    DWORD gs, fs, es, ds, edi, esi, ebx, edx, ecx, eax, ebp, eip, cs, eflags, esp, ss;
} CONTEXT;
#define CONTEXT_CONTROL 0x00010001

typedef struct {
    DWORD size, id, pid, glbl, proc;
    unsigned char *base;
    DWORD bsize;
    HANDLE hmod;
    char name[256];
    char path[260];
} MODULEENTRY32;
#define TH32CS_SNAPMODULE 8

WINAPI void   ExitProcess(unsigned);
WINAPI HANDLE GetStdHandle(DWORD);
WINAPI int    WriteFile(HANDLE, const void *, DWORD, DWORD *, void *);
WINAPI void   Sleep(DWORD);
WINAPI DWORD  GetTickCount(void);
WINAPI char  *GetCommandLineA(void);
WINAPI HANDLE CreateFileA(const char *, DWORD, DWORD, void *, DWORD, DWORD, HANDLE);
WINAPI int    CloseHandle(HANDLE);
WINAPI int    CreateProcessA(const char *, char *, void *, void *, int, DWORD, void *,
                             const char *, STARTUPINFOA *, PROCESS_INFORMATION *);
WINAPI DWORD  WaitForSingleObject(HANDLE, DWORD);
WINAPI DWORD  SuspendThread(HANDLE);
WINAPI DWORD  ResumeThread(HANDLE);
WINAPI int    GetThreadContext(HANDLE, CONTEXT *);
WINAPI HANDLE GetCurrentThread(void);
WINAPI int    SetThreadPriority(HANDLE, int);
WINAPI HANDLE CreateToolhelp32Snapshot(DWORD, DWORD);
WINAPI int    Module32First(HANDLE, MODULEENTRY32 *);
WINAPI int    Module32Next(HANDLE, MODULEENTRY32 *);
WINAPI unsigned timeBeginPeriod(unsigned);
WINAPI unsigned timeEndPeriod(unsigned);
__attribute__((dllimport)) int __cdecl wsprintfA(char *, const char *, ...);

#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define INVALID_HANDLE    ((HANDLE)-1)
#define WAIT_TIMEOUT      0x102
#define THREAD_PRIORITY_TIME_CRITICAL 15

#define MAXSAMP 65536
#define MAXMOD  64
#define SNAP_MS 3000

/* Samples go to memory and the file is written once at the end, so the sampler adds no disk
 * traffic to what it measures. */
static DWORD  s_eip[MAXSAMP];
static WORD   s_cs[MAXSAMP];
static DWORD  nsamp;
static MODULEENTRY32 mods[MAXMOD];
static int    nmods;
static HANDLE out;

static DWORD len(const char *s)
{
    DWORD n = 0;
    while (s[n])
        n++;
    return n;
}

static void say(const char *s)
{
    DWORD w;
    WriteFile(out, s, len(s), &w, 0);
}

/* Lines are gathered and written 32 KB at a time: a WriteFile a sample is slow here. */
static char   wbuf[32768];
static DWORD  wused;

static void wflush(HANDLE f)
{
    DWORD w;
    if (wused)
        WriteFile(f, wbuf, wused, &w, 0);
    wused = 0;
}

static void wr(HANDLE f, const char *s)
{
    DWORD n = len(s);
    if (wused + n > sizeof wbuf)
        wflush(f);
    while (*s)
        wbuf[wused++] = *s++;
}

static DWORD num(char **p)
{
    DWORD v = 0;
    while (**p == ' ')
        (*p)++;
    while (**p >= '0' && **p <= '9')
        v = v * 10 + (DWORD)(*(*p)++ - '0');
    return v;
}

static void snap_modules(DWORD pid)
{
    HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    MODULEENTRY32 m;
    int n = 0;

    if (h == INVALID_HANDLE)
        return;
    m.size = sizeof m;
    if (Module32First(h, &m)) {
        do {
            if (n < MAXMOD)
                mods[n++] = m;
            m.size = sizeof m;
        } while (Module32Next(h, &m));
    }
    CloseHandle(h);
    if (n)
        nmods = n;
}

void start(void)
{
    char *cl = GetCommandLineA();
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    CONTEXT c;
    DWORD secs, iv, t0, t_snap, t_end, failed = 0;
    char b[128];
    HANDLE f;
    DWORD i;
    unsigned char *z;

    out = GetStdHandle(STD_OUTPUT_HANDLE);
    /* Skip our own name (quoted or not), then seconds and interval; the rest is the program. */
    if (*cl == '"') {
        for (cl++; *cl && *cl != '"'; cl++)
            ;
        if (*cl)
            cl++;
    } else {
        while (*cl && *cl != ' ')
            cl++;
    }
    secs = num(&cl);
    iv = num(&cl);
    while (*cl == ' ')
        cl++;
    if (!secs || !iv || !*cl) {
        say("EIPSAMP seconds interval_ms command line...\r\n");
        ExitProcess(2);
    }
    z = (unsigned char *)&si;
    for (i = 0; i < sizeof si; i++)
        z[i] = 0;
    si.cb = sizeof si;
    if (!CreateProcessA(0, cl, 0, 0, 0, 0, 0, 0, &si, &pi)) {
        say("EIPSAMP: could not start the program\r\n");
        ExitProcess(1);
    }
    timeBeginPeriod(1);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    t0 = GetTickCount();
    t_end = t0 + secs * 1000;
    t_snap = t0 + SNAP_MS;
    while (WaitForSingleObject(pi.proc, iv) == WAIT_TIMEOUT && GetTickCount() < t_end) {
        if (SuspendThread(pi.thread) == (DWORD)-1) {
            failed++;
            continue;
        }
        c.flags = CONTEXT_CONTROL;
        if (GetThreadContext(pi.thread, &c) && nsamp < MAXSAMP) {
            s_eip[nsamp] = c.eip;
            s_cs[nsamp] = (WORD)c.cs;
            nsamp++;
        } else {
            failed++;
        }
        ResumeThread(pi.thread);
        if (GetTickCount() >= t_snap) {
            snap_modules(pi.pid);
            t_snap = GetTickCount() + SNAP_MS;
        }
    }
    timeEndPeriod(1);
    f = CreateFileA("C:\\EIPSAMP.TXT", 0x40000000, 0, 0, 2, 0x80, 0);
    if (f == INVALID_HANDLE) {
        say("EIPSAMP: cannot write C:\\EIPSAMP.TXT\r\n");
        ExitProcess(1);
    }
    wsprintfA(b, "EIPSAMP samples %lu failed %lu ms %lu interval %lu\r\n", nsamp, failed,
              GetTickCount() - t0, iv);
    wr(f, b);
    say(b);
    for (i = 0; i < (DWORD)nmods; i++) {
        wsprintfA(b, "M %08lx %08lx %s\r\n", (DWORD)mods[i].base, mods[i].bsize, mods[i].name);
        wr(f, b);
    }
    for (i = 0; i < nsamp; i++) {
        wsprintfA(b, "S %04x %08lx\r\n", (unsigned)s_cs[i], s_eip[i]);
        wr(f, b);
    }
    wflush(f);
    CloseHandle(f);
    CloseHandle(pi.thread);
    CloseHandle(pi.proc);
    ExitProcess(0);
}
