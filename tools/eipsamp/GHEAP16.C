/*
 * GHEAP16 - the 16-bit global heap's code segments, so EIPSAMP's 16-bit samples can be named.
 *
 * Writes C:\GHEAP16.TXT, one line a code segment: selector, linear base, size, segment number
 * (1-based, as in the module's NE table) and owning module. A sample's CS is matched by
 * selector; its IP is an offset into that segment of the module's file.
 *
 * Win16, so Toolhelp sees the same heap the sampled program's 16-bit calls run in. EIPSAMP
 * runs it while the sampled program is still up: discardable segments can move once it ends.
 */
#include <windows.h>
#include <toolhelp.h>

int PASCAL WinMain( HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show )
{
    GLOBALENTRY ge;
    MODULEENTRY me;
    HFILE       f;
    char        b[128];
    int         n;

    (void)inst; (void)prev; (void)cmd; (void)show;
    f = _lcreat( "C:\\GHEAP16.TXT", 0 );
    if( f == HFILE_ERROR ) {
        return( 1 );
    }
    ge.dwSize = sizeof( ge );
    me.dwSize = sizeof( me );
    if( GlobalFirst( &ge, GLOBAL_ALL ) ) {
        do {
            if( ge.wType == GT_CODE ) {
                if( !ModuleFindHandle( &me, (HMODULE)ge.hOwner ) ) {
                    me.szModule[0] = '?';
                    me.szModule[1] = 0;
                }
                n = wsprintf( b, "G %04x %08lx %08lx %u %s\r\n",
                              GlobalHandleToSel( ge.hBlock ), ge.dwAddress, ge.dwBlockSize,
                              ge.wData, (LPSTR)me.szModule );
                _lwrite( f, b, n );
            }
            ge.dwSize = sizeof( ge );
        } while( GlobalNext( &ge, GLOBAL_ALL ) );
    }
    _lclose( f );
    return( 0 );
}
