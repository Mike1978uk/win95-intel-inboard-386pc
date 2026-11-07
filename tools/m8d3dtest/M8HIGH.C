/* M8HIGH: start a program at high priority, so the remote agent cannot take half the CPU from it.
 *
 * Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
 *
 * Usage: M8HIGH <directory> <program and arguments>. Starts it in that directory and returns at once.
 *
 * Build: wcl386 -q -bt=nt -l=nt_win -ox -i=<watcom>h -i=<watcom>h\nt "-fe=M8HIGH.EXE" M8HIGH.C
 */

#include <windows.h>
#include <string.h>

int PASCAL WinMain( HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show )
{
    STARTUPINFO         si;
    PROCESS_INFORMATION pi;
    char                dir[MAX_PATH];
    char                *space;

    (void)inst;
    (void)prev;
    (void)show;
    lstrcpyn( dir, cmd, sizeof( dir ) );
    space = strchr( dir, ' ' );
    if( space == NULL ) {
        return( 1 );
    }
    *space = '\0';
    memset( &si, 0, sizeof( si ) );
    si.cb = sizeof( si );
    if( !CreateProcess( NULL, space + 1, NULL, NULL, FALSE, HIGH_PRIORITY_CLASS, NULL, dir, &si, &pi ) ) {
        return( 2 );
    }
    CloseHandle( pi.hThread );
    CloseHandle( pi.hProcess );
    return( 0 );
}
