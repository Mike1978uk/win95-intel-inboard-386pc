/* FB16CHK: asks the 16 bpp driver to compare the card with its frame buffer
 * (ExtEscape 0x4D3B) and prints how many pixels differ and where. 0 means
 * the card shows exactly what GDI drew, dithered. */

#include <windows.h>
#include <stdio.h>

int main( void )
{
    HDC     dc = GetDC( NULL );
    DWORD   out[5];
    int     rc;

    rc = ExtEscape( dc, 0x4D3B, 0, NULL, sizeof( out ), (LPSTR)out );
    ReleaseDC( NULL, dc );
    if( rc <= 0 ) {
        printf( "escape failed %d (not the 16 bpp driver?)\n", rc );
        return( 1 );
    }
    if( out[0] == 0 ) {
        printf( "card = frame buffer\n" );
    } else {
        printf( "%lu pixels differ, in %lu,%lu - %lu,%lu\n", out[0], out[1], out[2], out[3],
                out[4] );
    }
    return( 0 );
}
