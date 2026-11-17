/* M8EPOCH.EXE seconds - samples the display driver's GDI counters (escape 4D41h, the
   ones M8DD32's 8 bpp Lock uses to decide whether its RAM copy still matches the card)
   every 100 ms and writes to C:\M8EPOCH.TXT how often each one moved, and whether the
   escape ever failed. A failed escape or a counter moving every frame forces Lock to
   read the card back. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#define ESC_STATS   0x4D41
#define N           20

static const char *names[N] = { "bitblt", "output", "pixel", "strblt", "exttextout", "scanlr",
    "stretchblt", "stretchdib", "dibtodev", "fastborder", "updatecol", "fill_hw", "copy_hw",
    "readback", "text_hw", "text_fail", "run_fill", "covered", "line_hw", "cur_hide" };

int PASCAL WinMain( HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show )
{
    DWORD   a[N], b[N], moved[N], total[N];
    long    samples = 0, failed = 0;
    int     secs = atoi( cmd ), i, ok, have = 0;
    DWORD   end;
    FILE    *o;

    (void)inst; (void)prev; (void)show;
    if( secs <= 0 ) secs = 30;
    memset( moved, 0, sizeof( moved ) );
    memset( total, 0, sizeof( total ) );
    end = GetTickCount() + (DWORD)secs * 1000;
    while( GetTickCount() < end ) {
        HDC dc = GetDC( NULL );

        ok = dc != NULL && ExtEscape( dc, ESC_STATS, 0, NULL, sizeof( b ), (LPSTR)b ) > 0;
        if( dc != NULL ) ReleaseDC( NULL, dc );
        ++samples;
        if( !ok ) {
            ++failed;
        } else {
            if( have ) {
                for( i = 0; i < N; ++i ) {
                    if( b[i] != a[i] ) { ++moved[i]; total[i] += b[i] - a[i]; }
                }
            }
            memcpy( a, b, sizeof( a ) );
            have = 1;
        }
        Sleep( 100 );
    }
    o = fopen( "C:\\M8EPOCH.TXT", "a" );
    if( o == NULL ) return( 1 );
    fprintf( o, "== M8EPOCH %d s: samples %ld, escape failed %ld\n", secs, samples, failed );
    for( i = 0; i < N; ++i ) {
        if( moved[i] ) fprintf( o, "  %-10s moved in %ld samples, +%lu\n", names[i], (long)moved[i], (unsigned long)total[i] );
    }
    fclose( o );
    return( 0 );
}
