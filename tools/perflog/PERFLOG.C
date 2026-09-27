/*
 * PERFLOG - log Windows 95's own performance counters (the ones System
 * Monitor shows: page-ins, swapfile in use, free memory, disk cache size)
 * to a file, with no clicking, so a paging workload can run unattended.
 *
 *   PERFLOG label seconds [interval]     default interval 2 s
 *
 * Run it with START so it samples while the workload runs:
 *   START PERFLOG A 300
 *
 * Every counter Windows offers is started and logged, so nothing depends on
 * guessing a counter name. Rows go to C:\PERFLOG.CSV: the header names the
 * counters, the first two columns are the label and milliseconds since start.
 * Rows are buffered and written once a minute and at the end, so the log
 * itself adds little disk traffic to the paging it is measuring.
 *
 * Built without a C runtime (build.sh), like TIMERRES.
 */

typedef unsigned long DWORD;
typedef long LONG;
typedef void *HANDLE;
typedef void *HKEY;

#define WINAPI __attribute__((dllimport, stdcall))

WINAPI void   ExitProcess(unsigned);
WINAPI HANDLE GetStdHandle(DWORD);
WINAPI int    WriteFile(HANDLE, const void *, DWORD, DWORD *, void *);
WINAPI void   Sleep(DWORD);
WINAPI DWORD  GetTickCount(void);
WINAPI char  *GetCommandLineA(void);
WINAPI HANDLE CreateFileA(const char *, DWORD, DWORD, void *, DWORD, DWORD, HANDLE);
WINAPI DWORD  SetFilePointer(HANDLE, long, long *, DWORD);
WINAPI int    CloseHandle(HANDLE);
WINAPI LONG   RegOpenKeyExA(HKEY, const char *, DWORD, DWORD, HKEY *);
WINAPI LONG   RegEnumValueA(HKEY, DWORD, char *, DWORD *, DWORD *, DWORD *, void *, DWORD *);
WINAPI LONG   RegQueryValueExA(HKEY, const char *, DWORD *, DWORD *, void *, DWORD *);
WINAPI LONG   RegCloseKey(HKEY);
__attribute__((dllimport)) int __cdecl wsprintfA(char *, const char *, ...);

#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define INVALID_HANDLE    ((HANDLE)-1)
#define HKEY_DYN_DATA     ((HKEY)0x80000006)
#define KEY_READ          0x20019

#define MAXSTAT 160
#define NAMELEN 64
#define BUFSZ   32768

static char names[MAXSTAT][NAMELEN];
static int nstat;
static char buf[BUFSZ];
static DWORD used;
static HANDLE out, logf;

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

static void flush(void)
{
    DWORD w;
    if (used && logf != INVALID_HANDLE)
        WriteFile(logf, buf, used, &w, 0);
    used = 0;
}

static void put(const char *s)
{
    DWORD n = len(s);
    if (used + n > BUFSZ)
        flush();
    while (*s)
        buf[used++] = *s++;
}

/* Reading a counter's name under StartStat is what tells Windows to start
 * collecting it; StatData then returns its value, and StopStat ends it. */
static void touch_all(const char *key)
{
    HKEY k;
    DWORD v, sz, type;
    int i;
    if (RegOpenKeyExA(HKEY_DYN_DATA, key, 0, KEY_READ, &k) != 0)
        return;
    for (i = 0; i < nstat; i++) {
        sz = sizeof v;
        RegQueryValueExA(k, names[i], 0, &type, &v, &sz);
    }
    RegCloseKey(k);
}

static int list_stats(void)
{
    HKEY k;
    DWORD i, nl, dl, type;
    unsigned char d[16];
    if (RegOpenKeyExA(HKEY_DYN_DATA, "PerfStats\\StartStat", 0, KEY_READ, &k) != 0)
        return 0;
    for (i = 0; nstat < MAXSTAT; i++) {
        nl = NAMELEN;
        dl = sizeof d;
        if (RegEnumValueA(k, i, names[nstat], &nl, 0, &type, d, &dl) != 0)
            break;
        nstat++;
    }
    RegCloseKey(k);
    return nstat;
}

static void sample(HKEY k, const char *label, DWORD ms)
{
    char b[48];
    DWORD v, sz, type;
    int i;
    wsprintfA(b, "%s,%lu", label, ms);
    put(b);
    for (i = 0; i < nstat; i++) {
        v = 0;
        sz = sizeof v;
        if (RegQueryValueExA(k, names[i], 0, &type, &v, &sz) == 0)
            wsprintfA(b, ",%lu", v);
        else
            wsprintfA(b, ",");
        put(b);
    }
    put("\r\n");
}

static DWORD number(char **p)
{
    DWORD n = 0;
    while (**p == ' ')
        (*p)++;
    while (**p >= '0' && **p <= '9')
        n = n * 10 + (DWORD)(*(*p)++ - '0');
    return n;
}

void start(void)
{
    char label[16], b[96], *cl = GetCommandLineA();
    DWORD secs, every, t0, next, now, row = 0;
    HKEY data;
    int i;

    out = GetStdHandle(STD_OUTPUT_HANDLE);

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
    for (i = 0; i < 15 && *cl && *cl != ' '; i++)
        label[i] = *cl++;
    label[i] = 0;
    secs = number(&cl);
    every = number(&cl);
    if (!every)
        every = 2;
    if (!label[0] || !secs) {
        say("Usage: PERFLOG label seconds [interval]   e.g. START PERFLOG A 300\r\n");
        ExitProcess(1);
    }

    if (!list_stats()) {
        say("no counters under HKEY_DYN_DATA\\PerfStats\\StartStat\r\n");
        ExitProcess(1);
    }
    touch_all("PerfStats\\StartStat");
    if (RegOpenKeyExA(HKEY_DYN_DATA, "PerfStats\\StatData", 0, KEY_READ, &data) != 0) {
        say("cannot open PerfStats\\StatData\r\n");
        ExitProcess(1);
    }

    logf = CreateFileA("C:\\PERFLOG.CSV", 0x40000000, 1, 0, 4 /* OPEN_ALWAYS */, 0x80, 0);
    if (logf != INVALID_HANDLE)
        SetFilePointer(logf, 0, 0, 2 /* FILE_END */);

    put("label,ms");
    for (i = 0; i < nstat; i++) {
        put(",");
        put(names[i]);
    }
    put("\r\n");

    wsprintfA(b, "PERFLOG %s: %d counters, every %lu s for %lu s\r\n",
              label, nstat, every, secs);
    say(b);

    t0 = GetTickCount();
    next = t0;
    for (;;) {
        now = GetTickCount();
        sample(data, label, now - t0);
        if (++row % (60 / every + 1) == 0)
            flush();
        if (now - t0 >= secs * 1000)
            break;
        next += every * 1000;
        now = GetTickCount();
        if (next > now)
            Sleep(next - now);
    }

    flush();
    RegCloseKey(data);
    touch_all("PerfStats\\StopStat");
    if (logf != INVALID_HANDLE)
        CloseHandle(logf);
    say("PERFLOG done\r\n");
    ExitProcess(0);
}
