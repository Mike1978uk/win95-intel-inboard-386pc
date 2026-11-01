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

/* Milestone 2: what GDI drew is what a lock reads, and what is written
 * through a lock is what GDI then reads. The rectangle lock starts at an odd
 * x, so the pixel left of it must stay as GDI left it. */
static void round_trip( LPDIRECTDRAWSURFACE prim )
{
    HDC             dc = GetDC( NULL );
    DDSURFACEDESC   sd;
    RECT            rc;
    HRESULT         r;
    BYTE            *p;
    int             x;
    int             y;
    int             bad = 0;

    PatBlt( dc, 100, 100, 64, 32, BLACKNESS );
    GdiFlush();
    memset( &sd, 0, sizeof( sd ) );
    sd.dwSize = sizeof( sd );
    r = prim->lpVtbl->Lock( prim, NULL, &sd, DDLOCK_WAIT, NULL );
    printf( "lock rw %08lx\n", (unsigned long)r );
    if( r == DD_OK ) {
        p = (BYTE *)sd.lpSurface;
        for( y = 100; y < 132; ++y ) {
            for( x = 100; x < 164; ++x ) {
                bad += p[y * sd.lPitch + x] != 0;
            }
        }
        printf( "read: %d of 2048 not black\n", bad );
        for( y = 100; y < 132; ++y ) {
            memset( p + y * sd.lPitch + 100, 0xFF, 64 );
        }
        r = prim->lpVtbl->Unlock( prim, NULL );
        printf( "unlock %08lx  gdi(100,100) %06lx  gdi(163,131) %06lx\n", (unsigned long)r,
                (unsigned long)GetPixel( dc, 100, 100 ), (unsigned long)GetPixel( dc, 163, 131 ) );
    }
    PatBlt( dc, 200, 100, 40, 16, BLACKNESS );
    GdiFlush();
    SetRect( &rc, 201, 100, 233, 116 );
    memset( &sd, 0, sizeof( sd ) );
    sd.dwSize = sizeof( sd );
    r = prim->lpVtbl->Lock( prim, &rc, &sd, DDLOCK_WAIT, NULL );
    printf( "lock rect %08lx\n", (unsigned long)r );
    if( r == DD_OK ) {
        p = (BYTE *)sd.lpSurface;
        for( y = 0; y < 16; ++y ) {
            memset( p + y * sd.lPitch, 0xFF, 32 );
        }
        r = prim->lpVtbl->Unlock( prim, sd.lpSurface );
        printf( "unlock %08lx  gdi(200,100) %06lx (black)  gdi(201,100) %06lx  gdi(232,115) %06lx"
                "  gdi(233,115) %06lx (black)\n", (unsigned long)r,
                (unsigned long)GetPixel( dc, 200, 100 ), (unsigned long)GetPixel( dc, 201, 100 ),
                (unsigned long)GetPixel( dc, 232, 115 ), (unsigned long)GetPixel( dc, 233, 115 ) );
    }
    ReleaseDC( NULL, dc );
    InvalidateRect( NULL, NULL, TRUE );
}

/* Milestone 4: a surface in the card's off-screen rows. Its left half is
 * written white through a lock and read back; Blt fills its top left black
 * and copies it to the screen, where GDI reads it. */
static void offscreen( LPDIRECTDRAW dd )
{
    HDC                 dc;
    DDSURFACEDESC       sd;
    LPDIRECTDRAWSURFACE off;
    LPDIRECTDRAWSURFACE prim;
    DDBLTFX             fx;
    RECT                rc;
    RECT                dst;
    HRESULT             r;
    BYTE                *p;
    int                 x;
    int                 y;
    int                 bad = 0;

    memset( &sd, 0, sizeof( sd ) );
    sd.dwSize = sizeof( sd );
    sd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    sd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY;
    sd.dwWidth = 64;
    sd.dwHeight = 32;
    r = dd->lpVtbl->CreateSurface( dd, &sd, &off, NULL );
    printf( "off CreateSurface %08lx\n", (unsigned long)r );
    if( r != DD_OK ) {
        return;
    }
    memset( &sd, 0, sizeof( sd ) );
    sd.dwSize = sizeof( sd );
    sd.dwFlags = DDSD_CAPS;
    sd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    r = dd->lpVtbl->CreateSurface( dd, &sd, &prim, NULL );
    if( r != DD_OK ) {
        off->lpVtbl->Release( off );
        return;
    }
    memset( &sd, 0, sizeof( sd ) );
    sd.dwSize = sizeof( sd );
    r = off->lpVtbl->Lock( off, NULL, &sd, DDLOCK_WAIT, NULL );
    printf( "off lock %08lx  surface %08lx  pitch %ld  %lux%lu  caps %08lx\n", (unsigned long)r,
            (unsigned long)sd.lpSurface, sd.lPitch, sd.dwWidth, sd.dwHeight,
            sd.ddsCaps.dwCaps );
    if( r == DD_OK ) {
        p = (BYTE *)sd.lpSurface;
        for( y = 0; y < 32; ++y ) {
            for( x = 0; x < 64; ++x ) {
                p[y * sd.lPitch + x] = x < 32 ? 0xFF : 0x00;
            }
        }
        r = off->lpVtbl->Unlock( off, NULL );
        printf( "off unlock %08lx\n", (unsigned long)r );
        /* The lock's memory is the driver's shadow: scribbled on, the next
         * lock must bring the pixels back from the card. */
        for( y = 0; y < 32; ++y ) {
            memset( p + y * sd.lPitch, 0x55, 64 );
        }
        memset( &sd, 0, sizeof( sd ) );
        sd.dwSize = sizeof( sd );
        r = off->lpVtbl->Lock( off, NULL, &sd, DDLOCK_WAIT | DDLOCK_READONLY, NULL );
        if( r == DD_OK ) {
            p = (BYTE *)sd.lpSurface;
            for( y = 0; y < 32; ++y ) {
                for( x = 0; x < 64; ++x ) {
                    bad += p[y * sd.lPitch + x] != (x < 32 ? 0xFF : 0x00);
                }
            }
            off->lpVtbl->Unlock( off, NULL );
        }
        printf( "off read back %08lx: %d of 2048 wrong\n", (unsigned long)r, bad );
    }
    memset( &fx, 0, sizeof( fx ) );
    fx.dwSize = sizeof( fx );
    fx.dwFillColor = 0;
    SetRect( &rc, 0, 0, 16, 8 );
    r = off->lpVtbl->Blt( off, &rc, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx );
    printf( "off fill %08lx\n", (unsigned long)r );
    SetRect( &rc, 0, 0, 64, 32 );
    SetRect( &dst, 301, 200, 365, 232 );
    r = prim->lpVtbl->Blt( prim, &dst, off, &rc, DDBLT_WAIT, NULL );
    dc = GetDC( NULL );
    printf( "off to screen %08lx  (301,200) %06lx black  (317,200) %06lx white  (301,208) %06lx white"
            "  (332,231) %06lx white  (333,200) %06lx black\n", (unsigned long)r,
            (unsigned long)GetPixel( dc, 301, 200 ), (unsigned long)GetPixel( dc, 317, 200 ),
            (unsigned long)GetPixel( dc, 301, 208 ), (unsigned long)GetPixel( dc, 332, 231 ),
            (unsigned long)GetPixel( dc, 333, 200 ) );
    ReleaseDC( NULL, dc );
    prim->lpVtbl->Release( prim );
    off->lpVtbl->Release( off );
    InvalidateRect( NULL, NULL, TRUE );
}

int main( void )
{
    HMODULE         lib;
    DDCREATE        create;
    LPDIRECTDRAW    dd = NULL;
    DDCAPS          hw;
    DDCAPS          hel;
    DDSURFACEDESC   sd;
    LPDIRECTDRAWSURFACE prim;
    HRESULT         r;

    /* Unbuffered: a hang then shows the last step that finished. */
    setvbuf( stdout, NULL, _IONBF, 0 );
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

    /* The primary: where a lock points. dxdiag's colour fill to the primary
     * faulted (DDERR_EXCEPTION), so print the address before touching it. */
    r = dd->lpVtbl->SetCooperativeLevel( dd, NULL, DDSCL_NORMAL );
    printf( "SetCooperativeLevel %08lx\n", (unsigned long)r );
    memset( &sd, 0, sizeof( sd ) );
    sd.dwSize = sizeof( sd );
    sd.dwFlags = DDSD_CAPS;
    sd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    r = dd->lpVtbl->CreateSurface( dd, &sd, &prim, NULL );
    printf( "CreateSurface %08lx\n", (unsigned long)r );
    if( r == DD_OK ) {
        memset( &sd, 0, sizeof( sd ) );
        sd.dwSize = sizeof( sd );
        r = prim->lpVtbl->Lock( prim, NULL, &sd, DDLOCK_WAIT | DDLOCK_READONLY, NULL );
        printf( "Lock %08lx  surface %08lx  pitch %ld  %lux%lu\n", (unsigned long)r,
                (unsigned long)sd.lpSurface, sd.lPitch, sd.dwWidth, sd.dwHeight );
        if( r == DD_OK ) {
            prim->lpVtbl->Unlock( prim, NULL );
        }
        round_trip( prim );
        prim->lpVtbl->Release( prim );
    }
    offscreen( dd );
    dd->lpVtbl->Release( dd );
    return( 0 );
}
