/* M8DDMODE.EXE - the DirectDraw calls a full-screen 8 bpp program makes, each result
 * printed: create, exclusive mode, SetDisplayMode, a primary with one back buffer, a
 * palette on it, a fill, a flip, then back. Run twice in a row, it shows which call a
 * second 8 bpp program fails on.
 *   M8DDMODE [width height bpp]        default 640 480 8
 * Exit code 0 when every call returned DD_OK.
 * Build: wcl386 -q -bt=nt -l=nt -ox -fe=M8DDMODE.EXE M8DDMODE.C
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <ddraw.h>

typedef HRESULT (WINAPI *DDCREATE)( GUID *, LPDIRECTDRAW *, IUnknown * );

static int fails;

static void show( const char *what, HRESULT r )
{
    printf( "%-22s %08lx%s\n", what, (unsigned long)r, r == DD_OK ? "" : "  FAILED" );
    if( r != DD_OK ) {
        ++fails;
    }
}

static LRESULT CALLBACK wndproc( HWND h, UINT m, WPARAM w, LPARAM l )
{
    return( DefWindowProc( h, m, w, l ) );
}

int main( int argc, char **argv )
{
    int                     w = argc > 3 ? atoi( argv[1] ) : 640;
    int                     ht = argc > 3 ? atoi( argv[2] ) : 480;
    int                     bpp = argc > 3 ? atoi( argv[3] ) : 8;
    HMODULE                 lib = LoadLibrary( "DDRAW.DLL" );
    DDCREATE                create = lib ? (DDCREATE)GetProcAddress( lib, "DirectDrawCreate" ) : NULL;
    LPDIRECTDRAW            dd = NULL;
    LPDIRECTDRAWSURFACE     prim = NULL;
    LPDIRECTDRAWSURFACE     back = NULL;
    LPDIRECTDRAWPALETTE     pal = NULL;
    DDSURFACEDESC           sd;
    DDSCAPS                 caps;
    DDBLTFX                 fx;
    PALETTEENTRY            pe[256];
    WNDCLASS                wc;
    HWND                    wnd;
    HDC                     dc;
    int                     i;

    memset( &wc, 0, sizeof( wc ) );
    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandle( NULL );
    wc.lpszClassName = "M8DDMODE";
    RegisterClass( &wc );
    wnd = CreateWindowEx( WS_EX_TOPMOST, "M8DDMODE", "M8DDMODE", WS_POPUP | WS_VISIBLE, 0, 0,
                          GetSystemMetrics( SM_CXSCREEN ), GetSystemMetrics( SM_CYSCREEN ),
                          NULL, NULL, wc.hInstance, NULL );
    dc = GetDC( NULL );
    printf( "GDI before: %dx%d %d bpp\n", GetDeviceCaps( dc, HORZRES ), GetDeviceCaps( dc, VERTRES ),
            GetDeviceCaps( dc, BITSPIXEL ) );
    ReleaseDC( NULL, dc );
    if( create == NULL ) {
        printf( "no DirectDrawCreate\n" );
        return( 2 );
    }
    show( "DirectDrawCreate", create( NULL, &dd, NULL ) );
    if( dd == NULL ) {
        return( 1 );
    }
    show( "SetCooperativeLevel", IDirectDraw_SetCooperativeLevel( dd, wnd, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN ) );
    show( "SetDisplayMode", IDirectDraw_SetDisplayMode( dd, w, ht, bpp ) );
    dc = GetDC( NULL );
    printf( "GDI in mode: %dx%d %d bpp\n", GetDeviceCaps( dc, HORZRES ), GetDeviceCaps( dc, VERTRES ),
            GetDeviceCaps( dc, BITSPIXEL ) );
    ReleaseDC( NULL, dc );
    memset( &sd, 0, sizeof( sd ) );
    sd.dwSize = sizeof( sd );
    sd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
    sd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX;
    sd.dwBackBufferCount = 1;
    show( "CreateSurface flip", IDirectDraw_CreateSurface( dd, &sd, &prim, NULL ) );
    if( prim == NULL ) {
        sd.dwFlags = DDSD_CAPS;
        sd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
        show( "CreateSurface prim", IDirectDraw_CreateSurface( dd, &sd, &prim, NULL ) );
    } else {
        caps.dwCaps = DDSCAPS_BACKBUFFER;
        show( "GetAttachedSurface", IDirectDrawSurface_GetAttachedSurface( prim, &caps, &back ) );
    }
    if( prim != NULL ) {
        memset( &sd, 0, sizeof( sd ) );
        sd.dwSize = sizeof( sd );
        show( "GetSurfaceDesc", IDirectDrawSurface_GetSurfaceDesc( prim, &sd ) );
        printf( "primary: %lux%lu %lu bpp caps %08lx\n", sd.dwWidth, sd.dwHeight,
                sd.ddpfPixelFormat.dwRGBBitCount, sd.ddsCaps.dwCaps );
        if( bpp == 8 ) {
            for( i = 0; i < 256; ++i ) {
                pe[i].peRed = (BYTE)i;
                pe[i].peGreen = (BYTE)(255 - i);
                pe[i].peBlue = (BYTE)(i * 7);
                pe[i].peFlags = 0;
            }
            show( "CreatePalette", IDirectDraw_CreatePalette( dd, DDPCAPS_8BIT | DDPCAPS_ALLOW256, pe, &pal, NULL ) );
            if( pal != NULL ) {
                show( "SetPalette", IDirectDrawSurface_SetPalette( prim, pal ) );
                show( "SetEntries", IDirectDrawPalette_SetEntries( pal, 0, 0, 256, pe ) );
            }
        }
        memset( &fx, 0, sizeof( fx ) );
        fx.dwSize = sizeof( fx );
        fx.dwFillColor = 0x80;
        show( "Blt fill", IDirectDrawSurface_Blt( back != NULL ? back : prim, NULL, NULL, NULL,
                                                  DDBLT_COLORFILL | DDBLT_WAIT, &fx ) );
        if( back != NULL ) {
            show( "Flip", IDirectDrawSurface_Flip( prim, NULL, DDFLIP_WAIT ) );
        }
        Sleep( 2000 );
    }
    if( pal != NULL ) {
        IDirectDrawPalette_Release( pal );
    }
    if( prim != NULL ) {
        IDirectDrawSurface_Release( prim );
    }
    show( "RestoreDisplayMode", IDirectDraw_RestoreDisplayMode( dd ) );
    show( "SetCooperativeLevel N", IDirectDraw_SetCooperativeLevel( dd, wnd, DDSCL_NORMAL ) );
    IDirectDraw_Release( dd );
    dc = GetDC( NULL );
    printf( "GDI after: %dx%d %d bpp\n", GetDeviceCaps( dc, HORZRES ), GetDeviceCaps( dc, VERTRES ),
            GetDeviceCaps( dc, BITSPIXEL ) );
    ReleaseDC( NULL, dc );
    DestroyWindow( wnd );
    printf( "%d calls failed\n", fails );
    return( fails ? 1 : 0 );
}
