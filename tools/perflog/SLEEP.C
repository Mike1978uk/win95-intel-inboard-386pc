/*
 * SLEEP - wait N seconds without using the CPU.
 *
 *   SLEEP seconds
 *
 * RAMBASE.BAT needs pauses between steps. CHOICE /T polls the keyboard and
 * keeps its DOS box at 100% CPU, which competes with the workload being
 * measured; Sleep() blocks the thread instead.
 *
 * Built without a C runtime (build.sh), like PERFLOG.
 */

typedef unsigned long DWORD;

#define WINAPI __attribute__((dllimport, stdcall))

WINAPI void  ExitProcess(unsigned);
WINAPI void  Sleep(DWORD);
WINAPI char *GetCommandLineA(void);

void start(void)
{
    char *cl = GetCommandLineA();
    DWORD n = 0;

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
    while (*cl >= '0' && *cl <= '9')
        n = n * 10 + (DWORD)(*cl++ - '0');

    Sleep(n * 1000);
    ExitProcess(n ? 0 : 1);
}
