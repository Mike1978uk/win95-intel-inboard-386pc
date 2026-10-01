/*
 * WINCLOSE - wait for a window to go, or close windows by title.
 *
 *   WINCLOSE WAIT title   wait until no window is titled exactly "title"
 *   WINCLOSE text         close every window whose title contains "text"
 *
 * Both ignore case. RAMBASE.BAT uses WAIT so it can tell when PERFLOG has
 * finished, then closes the programs it opened, so the next run starts from
 * an empty desktop. A batch file has no other way to reach a Windows program.
 *
 * WAIT gives up after 10 minutes and exits 1, so a missing PERFLOG window
 * cannot leave the batch stuck.
 *
 * Built without a C runtime (build.sh), like PERFLOG.
 */

typedef unsigned long DWORD;
typedef void *HWND;
typedef int BOOL;
typedef long LPARAM;

#define WINAPI __attribute__((dllimport, stdcall))
#define CALLBACK __attribute__((stdcall))
#define WM_CLOSE 0x0010

typedef BOOL (CALLBACK *WNDENUMPROC)(HWND, LPARAM);

WINAPI void  ExitProcess(unsigned);
WINAPI void  Sleep(DWORD);
WINAPI char *GetCommandLineA(void);
WINAPI BOOL  EnumWindows(WNDENUMPROC, LPARAM);
WINAPI int   GetWindowTextA(HWND, char *, int);
WINAPI BOOL  IsWindowVisible(HWND);
WINAPI BOOL  PostMessageA(HWND, unsigned, unsigned, LPARAM);

static char *want;
static int exact, found;

static char lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

static int matches(const char *title)
{
    const char *t, *w;

    if (exact) {
        for (t = title, w = want; *t && lower(*t) == lower(*w); t++, w++)
            ;
        return *t == 0 && *w == 0;
    }
    for (; *title; title++) {
        for (t = title, w = want; *w && lower(*t) == lower(*w); t++, w++)
            ;
        if (*w == 0)
            return 1;
    }
    return 0;
}

static BOOL CALLBACK visit(HWND h, LPARAM lp)
{
    char title[128];

    (void)lp;
    if (!IsWindowVisible(h) || GetWindowTextA(h, title, sizeof title) <= 0)
        return 1;
    if (matches(title)) {
        found++;
        if (!exact)
            PostMessageA(h, WM_CLOSE, 0, 0);
    }
    return 1;
}

static char *next_word(char *s)
{
    while (*s && *s != ' ')
        s++;
    if (*s)
        *s++ = 0;
    while (*s == ' ')
        s++;
    return s;
}

void start(void)
{
    char *cl = GetCommandLineA();
    char *arg;
    int i;

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
    if (*cl == 0)
        ExitProcess(2);
    for (arg = cl; *arg; arg++)
        ;
    while (arg > cl && (arg[-1] == ' ' || arg[-1] == '\r' || arg[-1] == '\n'))
        *--arg = 0;

    arg = cl;
    if (lower(arg[0]) == 'w' && lower(arg[1]) == 'a' && lower(arg[2]) == 'i'
            && lower(arg[3]) == 't' && arg[4] == ' ') {
        want = next_word(arg);
        exact = 1;
        for (i = 0; i < 600; i++) {
            found = 0;
            EnumWindows(visit, 0);
            if (!found)
                ExitProcess(0);
            Sleep(1000);
        }
        ExitProcess(1);
    }

    want = arg;
    EnumWindows(visit, 0);
    ExitProcess(found ? 0 : 1);
}
