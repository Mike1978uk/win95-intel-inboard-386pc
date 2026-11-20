/* M8FINE.EXE [ms] [log] [all] - asks the Mach8 display driver, every ms milliseconds
   (default 250), to send the still parts of the 16 bpp screen again with the fine (R2)
   dither instead of the 4x4 matrix (escape 4D42h, fb16.c m8_fb16_refine). The driver does
   nothing until the screen has been still for ~0.4 s, and a call sends at most ~25 ms of
   pixels. Once a whole pass finds nothing to do it asks once a second. Exits at once when
   the screen is not 16 bpp. With "log", appends each call that did something to
   C:\M8FINE.TXT; with "all", every call and the escape's result. For the StartUp group.
   Build: wcl386 -q -bt=nt -l=nt_win -ox -fe=M8FINE.EXE M8FINE.C */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ESC_FB16FINE    0x4D42

int PASCAL WinMain( HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show )
{
    DWORD   out[4];
    int     ms = atoi( cmd ) > 0 ? atoi( cmd ) : 250;
    int     log = strstr( cmd, "log" ) != NULL;
    int     all = strstr( cmd, "all" ) != NULL;
    HDC     dc = GetDC( NULL );
    FILE    *o;
    int     r;

    (void)inst; (void)prev; (void)show;
    for( ;; ) {
        memset( out, 0, sizeof( out ) );
        r = ExtEscape( dc, ESC_FB16FINE, 0, NULL, sizeof( out ), (LPSTR)out );
        if( ( all || ( log && ( out[0] != 0 || out[1] != 0 ) ) ) &&
            ( o = fopen( "C:\\M8FINE.TXT", "a" ) ) != NULL ) {
            fprintf( o, "%lu ms: escape %d, sent %lu px, %lu pieces fine, %lu rows%s\n", GetTickCount(),
                     r, out[0], out[1], out[2], out[3] ? ", pass done" : "" );
            fclose( o );
        }
        if( r <= 0 ) {
            break;
        }
        Sleep( out[3] ? 1000 : ms );
    }
    ReleaseDC( NULL, dc );
    return( 0 );
}
