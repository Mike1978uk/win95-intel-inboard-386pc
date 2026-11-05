/* M8D3DS.EXE - prints the Direct3D counters of the driver running, to prove what a
 * program such as dxdiag sent it: run it before and after, and read the difference.
 * Reads M8DD32.DLL's export table where the DLL is based; it is not a module of this
 * process, and loading it here hangs. Appends to C:\M8D3DS.TXT.
 *
 *   M8D3DS <label>
 *
 * Build: wcl386 -q -bt=nt -l=nt_win -ox -i=<watcom>h -i=<watcom>h\nt -fe=M8D3DS.EXE M8D3DS.C
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>

#define M8_DLL_BASE 0xB0C00000UL

static const DWORD *find_stat( void )
{
    const BYTE  *b = (const BYTE *)M8_DLL_BASE;
    const BYTE  *pe;
    const BYTE  *ex;
    const DWORD *names;
    const WORD  *ords;
    const DWORD *funcs;
    DWORD       i;

    if( IsBadReadPtr( b, 0x400 ) ) {
        return( NULL );
    }
    pe = b + *(const DWORD *)(b + 0x3C);
    ex = b + *(const DWORD *)(pe + 0x78);
    names = (const DWORD *)(b + *(const DWORD *)(ex + 32));
    ords = (const WORD *)(b + *(const DWORD *)(ex + 36));
    funcs = (const DWORD *)(b + *(const DWORD *)(ex + 28));
    for( i = 0; i < *(const DWORD *)(ex + 24); ++i ) {
        if( strcmp( (const char *)(b + names[i]), "m8d3d_stat" ) == 0 ) {
            return( (const DWORD *)(b + funcs[ords[i]]) );
        }
    }
    return( NULL );
}

int PASCAL WinMain( HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show )
{
    FILE        *out = fopen( "C:\\M8D3DS.TXT", "a" );
    const DWORD *s = find_stat();

    if( out == NULL ) {
        return( 1 );
    }
    if( s == NULL ) {
        fprintf( out, "%s: driver DLL not mapped here\n", cmd );
    } else {
        fprintf( out, "%s: ctx %lu/%lu scene %lu state %lu prim-calls %lu prims %lu drawn %lu culled %lu outside %lu refused %lu flips %lu\n",
                 cmd, s[0], s[1], s[2], s[3], s[4], s[5], s[8], s[9], s[10], s[11], s[15] );
    }
    fclose( out );
    return( 0 );
}
