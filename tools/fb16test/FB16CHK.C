/* FB16CHK: asks the 16 bpp driver to compare the card with its frame buffer
 * (ExtEscape 0x4D3B) and appends to C:\FB16CHK.TXT how many pixels differ and
 * where, and how many times a send was entered during another. 0 differ means
 * the card shows exactly what GDI drew, dithered.
 *
 *   FB16CHK [label]
 *
 * A window program: a Win32 console on Windows 95 is a DOS VM, and this must run
 * from a list with the agent off.
 *
 * Build: wcl386 -q -bt=nt -l=nt_win -ox -i=<watcom>h -i=<watcom>h\nt "-fe=FB16CHK.EXE" FB16CHK.C
 */

#include <windows.h>
#include <stdio.h>

int PASCAL WinMain( HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show )
{
    HDC     dc = GetDC( NULL );
    DWORD   out[6];
    FILE    *f;
    int     rc;

    memset( out, 0, sizeof( out ) );
    rc = ExtEscape( dc, 0x4D3B, 0, NULL, sizeof( out ), (LPSTR)out );
    ReleaseDC( NULL, dc );
    f = fopen( "C:\\FB16CHK.TXT", "a" );
    if( f == NULL ) {
        return( 1 );
    }
    if( rc <= 0 ) {
        fprintf( f, "%s  escape failed %d (not the 16 bpp driver?)\n", cmd, rc );
    } else {
        fprintf( f, "%s  %lu pixels differ, in %lu,%lu - %lu,%lu; sends entered during a send %lu\n", cmd,
                 out[0], out[1], out[2], out[3], out[4], out[5] );
    }
    fclose( f );
    return( rc <= 0 );
}
