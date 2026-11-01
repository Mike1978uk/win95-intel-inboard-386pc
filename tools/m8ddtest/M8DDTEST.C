/* M8DDTEST.EXE - does DirectDraw see the Mach8 driver's HAL? Creates the
 * DirectDraw object on the primary display and prints the hardware and
 * emulation caps. With no HAL, DDRAW reports DDCAPS_NOHARDWARE.
 * Build: wcl386 -q -bt=nt -l=nt -ox -fe=M8DDTEST.EXE M8DDTEST.C
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <ddraw.h>

typedef HRESULT (WINAPI *DDCREATE)( GUID FAR *, LPDIRECTDRAW FAR *, IUnknown FAR * );

int main( void )
{
    HMODULE         lib;
    DDCREATE        create;
    LPDIRECTDRAW    dd = NULL;
    DDCAPS          hw;
    DDCAPS          hel;
    HRESULT         r;

    lib = LoadLibrary( "DDRAW.DLL" );
    if( lib == NULL ) {
        printf( "no DDRAW.DLL\n" );
        return( 1 );
    }
    create = (DDCREATE)GetProcAddress( lib, "DirectDrawCreate" );
    if( create == NULL ) {
        printf( "no DirectDrawCreate\n" );
        return( 1 );
    }
    r = create( NULL, &dd, NULL );
    printf( "DirectDrawCreate %08lx\n", (unsigned long)r );
    if( r != DD_OK ) {
        return( 1 );
    }
    memset( &hw, 0, sizeof( hw ) );
    memset( &hel, 0, sizeof( hel ) );
    hw.dwSize = sizeof( hw );
    hel.dwSize = sizeof( hel );
    r = dd->lpVtbl->GetCaps( dd, &hw, &hel );
    printf( "GetCaps %08lx\n", (unsigned long)r );
    printf( "hw  caps %08lx caps2 %08lx pal %08lx scaps %08lx vidmem %lu  %s\n",
            hw.dwCaps, hw.dwCaps2, hw.dwPalCaps, hw.ddsCaps.dwCaps, hw.dwVidMemTotal,
            (hw.dwCaps & DDCAPS_NOHARDWARE) ? "NO HAL" : "HAL" );
    printf( "hel caps %08lx caps2 %08lx pal %08lx scaps %08lx\n",
            hel.dwCaps, hel.dwCaps2, hel.dwPalCaps, hel.ddsCaps.dwCaps );
    dd->lpVtbl->Release( dd );
    return( 0 );
}
