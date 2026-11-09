/* M8FEEDT: does M8FEED.VXD load and hand out its queue? No engine writes.
 *
 * Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
 *
 * Opens \\.\M8FEED.VXD as M8DD32.DLL does, calls INIT with a dummy hot byte block, reads the
 * queue header, calls STOP, closes. Writes C:\M8FEEDT.TXT.
 *
 * Build: wcl386 -q -bt=nt -l=nt_win -ox -i=<watcom>h -i=<watcom>h\nt "-fe=M8FEEDT.EXE" M8FEEDT.C
 */

#include <windows.h>
#include <stdio.h>

static BYTE hot[64];

int PASCAL WinMain( HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show )
{
    FILE    *o = fopen( "C:\\M8FEEDT.TXT", "a" );
    HANDLE  h;
    DWORD   in = (DWORD)hot;
    DWORD   q = 0;
    DWORD   head = 0;
    DWORD   got = 0;
    BOOL    r;

    (void)inst;
    (void)prev;
    (void)show;
    if( o == NULL ) {
        return( 1 );
    }
    fprintf( o, "== M8FEEDT %s\n", cmd );
    h = CreateFileA( "\\\\.\\M8FEED.VXD", 0, 0, NULL, 0, FILE_FLAG_DELETE_ON_CLOSE, NULL );
    fprintf( o, "open %s error %lu\n", h == INVALID_HANDLE_VALUE ? "failed" : "ok", (unsigned long)GetLastError() );
    if( h == INVALID_HANDLE_VALUE ) {
        h = CreateFileA( "\\\\.\\C:\\WINDOWS\\SYSTEM\\M8FEED.VXD", 0, 0, NULL, 0, FILE_FLAG_DELETE_ON_CLOSE, NULL );
        fprintf( o, "open by path %s error %lu\n", h == INVALID_HANDLE_VALUE ? "failed" : "ok",
                 (unsigned long)GetLastError() );
    }
    if( h != INVALID_HANDLE_VALUE ) {
        r = DeviceIoControl( h, 1, &in, 4, &q, 4, &got, NULL );
        fprintf( o, "init %d error %lu queue %08lx got %lu\n", r, (unsigned long)GetLastError(), (unsigned long)q,
                 (unsigned long)got );
        if( q != 0 && !IsBadReadPtr( (void *)q, 64 ) ) {
            DWORD *p = (DWORD *)q;

            fprintf( o, "header %lu %lu %lu %lu %lu %lu %lu %lu\n", p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7] );
            fprintf( o, "writable %d\n", !IsBadWritePtr( p + 16, 4 ) );
        } else {
            fprintf( o, "queue not readable from ring 3\n" );
        }
        r = DeviceIoControl( h, 3, NULL, 0, &head, 4, &got, NULL );
        fprintf( o, "stop %d head %lu\n", r, (unsigned long)head );
        CloseHandle( h );
    }
    fprintf( o, "done\n\n" );
    fclose( o );
    return( 0 );
}
