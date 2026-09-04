/* WREBOOT.C - restart Windows 9x so the VxD reloads.  Detach from the
 * console (COMrade's DOS box) before forcing the reboot, so we aren't the
 * thing blocking shutdown; EWX_FORCE to skip the "close DOS program" prompt.
 */
#include <windows.h>
#include <stdio.h>
int main(int argc, char **argv)
{
    UINT flags = EWX_REBOOT | EWX_FORCE;
    if (argc > 1 && !stricmp(argv[1], "/NOFORCE")) flags = EWX_REBOOT;
    printf("rebooting (flags %u); detaching console...\n", flags);
    Sleep(700);
    FreeConsole();                 /* let go of COMrade's DOS box */
    ExitWindowsEx(flags, 0);
    return 0;
}
