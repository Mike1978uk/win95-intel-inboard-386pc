/* FB16MON: what the 16 bpp driver sends to the card, once a second, while
 * another program runs (ExtEscape 0x4D3A: access calls, sends, pixels sent,
 * engine copies and one-colour sends). Appends to C:\FB16MON.TXT.
 *   FB16MON <seconds> <label>
 * At about 1.9 us a pixel on the 5160's bus, pixels/s shows how much of a
 * second the sends take there.
 * Build: wcl386 -q -bt=nt -l=nt -ox -fe=FB16MON.EXE FB16MON.C
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

static int stat( DWORD *v )
{
    HDC dc = GetDC( NULL );
    int rc = ExtEscape( dc, 0x4D3A, 0, NULL, 4 * sizeof( DWORD ), (LPSTR)v );

    ReleaseDC( NULL, dc );
    return( rc > 0 );
}

int main( int argc, char **argv )
{
    DWORD   a[4];
    DWORD   b[4];
    DWORD   t;
    int     secs = argc > 1 ? atoi( argv[1] ) : 30;
    int     i;
    FILE    *f = fopen( "C:\\FB16MON.TXT", "a" );

    if( f == NULL || !stat( a ) ) {
        printf( "no 16 bpp driver\n" );
        return( 1 );
    }
    fprintf( f, "== %s  %d s\n", argc > 2 ? argv[2] : "-", secs );
    t = GetTickCount();
    for( i = 0; i < secs; ++i ) {
        Sleep( 1000 );
        stat( b );
        fprintf( f, "%3d  access %6lu  sends %6lu  pixels %8lu  copies/solid %5lu  (%lu ms)\n", i + 1,
                 b[0] - a[0], b[1] - a[1], b[2] - a[2], b[3] - a[3], GetTickCount() - t );
        fflush( f );
        a[0] = b[0];
        a[1] = b[1];
        a[2] = b[2];
        a[3] = b[3];
        t = GetTickCount();
    }
    fclose( f );
    return( 0 );
}
