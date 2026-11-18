/* DBGRUN.EXE - run a program as its debugger and log what it tells a debugger.
   Windows 95 has no system-wide debug output monitor; a program's OutputDebugString text,
   its DLL loads and its exceptions reach only the process debugging it.
   Each line carries milliseconds since start, so the last line before a stall names the step.

     DBGRUN [/Llogfile] [/Ddir] program [args]     default log C:\DBGRUN.LOG, dir = current

   Build: wcl386 -q -bt=nt -l=nt dbgrun.c                                                  */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

static FILE *lg;
static DWORD t0;

static void out(const char *fmt, ...)
{
    va_list va;
    fprintf(lg, "%7lu ", GetTickCount() - t0);
    va_start(va, fmt);
    vfprintf(lg, fmt, va);
    va_end(va);
    fputc('\n', lg);
    fflush(lg);   /* a stall or reset must not lose the tail */
}

static void dbgstr(HANDLE ph, OUTPUT_DEBUG_STRING_INFO *s)
{
    char b[512];
    DWORD n = s->nDebugStringLength;
    DWORD got = 0;
    if (n >= sizeof b) n = sizeof b - 1;
    if (s->fUnicode || !ReadProcessMemory(ph, s->lpDebugStringData, b, n, &got)) {
        out("ODS (unreadable)");
        return;
    }
    b[got] = 0;
    while (got && (b[got - 1] == '\n' || b[got - 1] == '\r')) b[--got] = 0;
    out("ODS %s", b);
}

int main(int argc, char **argv)
{
    const char *logname = "C:\\DBGRUN.LOG";
    const char *dir = NULL;
    char cmd[1024] = "";
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    DEBUG_EVENT de;
    HANDLE ph = NULL;
    int i = 1, first_bp = 1;

    for (; i < argc && argv[i][0] == '/'; i++) {
        if (argv[i][1] == 'L' || argv[i][1] == 'l') logname = argv[i] + 2;
        else if (argv[i][1] == 'D' || argv[i][1] == 'd') dir = argv[i] + 2;
    }
    if (i >= argc) {
        printf("DBGRUN [/Llog] [/Ddir] program [args]\n");
        return 1;
    }
    for (; i < argc; i++) {
        if (cmd[0]) strcat(cmd, " ");
        strncat(cmd, argv[i], sizeof cmd - strlen(cmd) - 2);
    }
    lg = fopen(logname, "a");
    if (!lg) return 2;
    t0 = GetTickCount();
    out("== DBGRUN %s (dir %s)", cmd, dir ? dir : ".");

    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    if (!CreateProcess(NULL, cmd, NULL, NULL, FALSE, DEBUG_ONLY_THIS_PROCESS,
                       NULL, dir, &si, &pi)) {
        out("CreateProcess failed %lu", GetLastError());
        return 3;
    }
    ph = pi.hProcess;
    for (;;) {
        DWORD cont = DBG_CONTINUE;
        if (!WaitForDebugEvent(&de, INFINITE)) break;
        switch (de.dwDebugEventCode) {
        case OUTPUT_DEBUG_STRING_EVENT:
            dbgstr(ph, &de.u.DebugString);
            break;
        case EXCEPTION_DEBUG_EVENT: {
            EXCEPTION_RECORD *r = &de.u.Exception.ExceptionRecord;
            /* The loader's own breakpoint arrives once at start; anything else is the program's. */
            if (r->ExceptionCode == EXCEPTION_BREAKPOINT && first_bp) {
                first_bp = 0;
                break;
            }
            out("EXCEPTION %08lX at %08lX %s thread %lu", r->ExceptionCode,
                (DWORD)r->ExceptionAddress, de.u.Exception.dwFirstChance ? "first" : "LAST",
                de.dwThreadId);
            cont = DBG_EXCEPTION_NOT_HANDLED;
            break;
        }
        case CREATE_THREAD_DEBUG_EVENT:
            out("thread %lu start %08lX", de.dwThreadId, (DWORD)de.u.CreateThread.lpStartAddress);
            break;
        case EXIT_THREAD_DEBUG_EVENT:
            out("thread %lu exit %lu", de.dwThreadId, de.u.ExitThread.dwExitCode);
            break;
        case LOAD_DLL_DEBUG_EVENT:
            out("dll at %08lX", (DWORD)de.u.LoadDll.lpBaseOfDll);
            if (de.u.LoadDll.hFile) CloseHandle(de.u.LoadDll.hFile);
            break;
        case CREATE_PROCESS_DEBUG_EVENT:
            out("process start, image at %08lX", (DWORD)de.u.CreateProcessInfo.lpBaseOfImage);
            if (de.u.CreateProcessInfo.hFile) CloseHandle(de.u.CreateProcessInfo.hFile);
            break;
        case EXIT_PROCESS_DEBUG_EVENT:
            out("process exit %lu", de.u.ExitProcess.dwExitCode);
            ContinueDebugEvent(de.dwProcessId, de.dwThreadId, DBG_CONTINUE);
            fclose(lg);
            return 0;
        }
        ContinueDebugEvent(de.dwProcessId, de.dwThreadId, cont);
    }
    out("WaitForDebugEvent failed %lu", GetLastError());
    fclose(lg);
    return 4;
}
