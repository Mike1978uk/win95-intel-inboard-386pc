/*
 * TIMERRES - what does a Windows 95 timer tick cost on this machine, and
 * does timeBeginPeriod(1) shorten the waits of drivers that park a request
 * on SCSIPORT's timer (T130.MPD polled, SD120PPD.MPD)?
 *
 *   TIMERRES        measure at the default rate, at 1 ms, then default again
 *   TIMERRES HOLD   request 1 ms and keep it until Enter is pressed
 *
 * Results go to the console and are appended to C:\TIMERRES.TXT.
 *
 * Built without a C runtime (build.sh): wsprintfA has no floating point, so
 * every figure is an integer in microseconds. Timing uses
 * QueryPerformanceCounter, which Windows 95 derives from the 8254.
 */

typedef unsigned long DWORD;
typedef void *HANDLE;
typedef struct { DWORD lo, hi; } LI;

#define STDCALL __attribute__((stdcall))
#define WINAPI __attribute__((dllimport, stdcall))

WINAPI void   ExitProcess(unsigned);
WINAPI HANDLE GetStdHandle(DWORD);
WINAPI int    WriteFile(HANDLE, const void *, DWORD, DWORD *, void *);
WINAPI int    ReadFile(HANDLE, void *, DWORD, DWORD *, void *);
WINAPI void   Sleep(DWORD);
WINAPI int    QueryPerformanceCounter(LI *);
WINAPI int    QueryPerformanceFrequency(LI *);
WINAPI DWORD  GetTickCount(void);
WINAPI char  *GetCommandLineA(void);
WINAPI HANDLE CreateFileA(const char *, DWORD, DWORD, void *, DWORD, DWORD, HANDLE);
WINAPI DWORD  SetFilePointer(HANDLE, long, long *, DWORD);
WINAPI int    CloseHandle(HANDLE);
WINAPI int    MulDiv(int, int, int);
WINAPI DWORD  timeGetTime(void);
WINAPI DWORD  timeBeginPeriod(DWORD);
WINAPI DWORD  timeEndPeriod(DWORD);
__attribute__((dllimport)) int __cdecl wsprintfA(char *, const char *, ...);

#define STD_INPUT_HANDLE  ((DWORD)-10)
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define INVALID_HANDLE    ((HANDLE)-1)

static HANDLE out, logf;
static DWORD freq;

static void say(const char *s)
{
    DWORD n = 0, w;
    while (s[n])
        n++;
    WriteFile(out, s, n, &w, 0);
    if (logf != INVALID_HANDLE)
        WriteFile(logf, s, n, &w, 0);
}

static DWORD now(void)
{
    LI t;
    QueryPerformanceCounter(&t);
    return t.lo;
}

/* Intervals here are a few seconds at most, so the low word and a 32-bit
 * MulDiv suffice and no 64-bit arithmetic helper is needed. */
static DWORD us(DWORD ticks)
{
    return (DWORD)MulDiv((int)ticks, 1000000, (int)freq);
}

/* Sleep(1) returns on the first timer tick after 1 ms, so its duration is
 * the period the scheduler, and SCSIPORT's timer callbacks, actually run at. */
static void sleep_period(char *b)
{
    DWORD i, t, d, lo = 0xFFFFFFFF, hi = 0, sum = 0;
    for (i = 0; i < 50; i++) {
        t = now();
        Sleep(1);
        d = us(now() - t);
        sum += d;
        if (d < lo) lo = d;
        if (d > hi) hi = d;
    }
    wsprintfA(b, "  Sleep(1)       mean %lu us  min %lu  max %lu\r\n", sum / 50, lo, hi);
    say(b);
}

static void clock_step(char *b, const char *name, DWORD (*clk)(void))
{
    DWORD i, a, c, lo = 0xFFFFFFFF, sum = 0;
    for (i = 0; i < 20; i++) {
        a = clk();
        while ((c = clk()) == a)
            ;
        a = c;
        while ((c = clk()) == a)
            ;
        sum += c - a;
        if (c - a < lo) lo = c - a;
    }
    wsprintfA(b, "  %-14s step mean %lu ms  min %lu\r\n", name, sum / 20, lo);
    say(b);
}

static DWORD tgt(void) { return timeGetTime(); }
static DWORD gtc(void) { return GetTickCount(); }

/* A register-only loop. The clock is read once per 65536 passes because
 * reading it is itself port I/O; what this loop loses between two settings
 * is CPU time taken by timer interrupts. */
static DWORD cpu_rate(void)
{
    volatile DWORD sink = 0;
    (void)sink;
    DWORD t = now(), limit = freq * 2, passes = 0, i;
    do {
        for (i = 0; i < 65536; i++)
            sink += i;
        passes++;
    } while (now() - t < limit);
    return (DWORD)MulDiv((int)passes, 65536 / 2, 1000);   /* k passes/s */
}

static void measure(const char *label)
{
    char b[160];
    DWORD k;
    wsprintfA(b, "%s\r\n", label);
    say(b);
    sleep_period(b);
    clock_step(b, "timeGetTime", tgt);
    clock_step(b, "GetTickCount", gtc);
    for (k = 0; k < 3; k++) {
        wsprintfA(b, "  CPU loop       %lu k passes/s\r\n", cpu_rate());
        say(b);
    }
}

void start(void)
{
    char b[160], *cl = GetCommandLineA();
    int hold = 0;
    LI f;
    DWORD r;

    for (; *cl; cl++)
        if ((cl[0] | 0x20) == 'h' && (cl[1] | 0x20) == 'o' &&
            (cl[2] | 0x20) == 'l' && (cl[3] | 0x20) == 'd')
            hold = 1;

    out = GetStdHandle(STD_OUTPUT_HANDLE);
    logf = CreateFileA("C:\\TIMERRES.TXT", 0x40000000, 1, 0, 4 /* OPEN_ALWAYS */, 0x80, 0);
    if (logf != INVALID_HANDLE)
        SetFilePointer(logf, 0, 0, 2 /* FILE_END */);

    QueryPerformanceFrequency(&f);
    freq = f.lo;
    wsprintfA(b, "TIMERRES %s  QPC %lu Hz\r\n", hold ? "HOLD" : "", freq);
    say(b);
    if (freq == 0) {
        say("no performance counter\r\n");
        ExitProcess(1);
    }

    if (hold) {
        r = timeBeginPeriod(1);
        wsprintfA(b, "timeBeginPeriod(1) = %lu (0 = granted)\r\n", r);
        say(b);
        sleep_period(b);
        say("holding 1 ms - run the copy in another MS-DOS Prompt, then press Enter\r\n");
        ReadFile(GetStdHandle(STD_INPUT_HANDLE), b, sizeof b, &r, 0);
        timeEndPeriod(1);
        say("released\r\n");
    } else {
        measure("default rate");
        r = timeBeginPeriod(1);
        wsprintfA(b, "timeBeginPeriod(1) = %lu (0 = granted)\r\n", r);
        say(b);
        measure("1 ms requested");
        timeEndPeriod(1);
        measure("default rate again");
    }
    say("done\r\n\r\n");
    if (logf != INVALID_HANDLE)
        CloseHandle(logf);
    ExitProcess(0);
}
