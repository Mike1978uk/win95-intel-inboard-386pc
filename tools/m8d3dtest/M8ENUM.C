/* M8ENUM: what DirectX reports for this card, at the desktop's mode and at 640x480x16.
 *
 * Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
 *
 * A program chooses its device from this list, usually before it sets a mode. Appends to
 * C:\M8ENUM.TXT, under the label on the command line.
 *
 * Build: wcl386 -q -bt=nt -l=nt_win -ox -i=<watcom>h -i=<watcom>h\nt -i=<watcom>h\nt\directx
 *        "-fe=M8ENUM.EXE" M8ENUM.C ddraw.lib
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <ddraw.h>
#include <d3d.h>

static const GUID iid_d3d2 = { 0x6aae1ec1, 0x662a, 0x11d0, { 0x88, 0x9d, 0x00, 0xaa, 0x00, 0xbb, 0xb7, 0x6a } };

static FILE *out;

static HRESULT WINAPI on_device( GUID *g, LPSTR desc, LPSTR name, LPD3DDEVICEDESC hal, LPD3DDEVICEDESC hel, LPVOID ctx )
{
    (void)desc;
    (void)ctx;
    fprintf( out, "  device %-24s %08lx  hal flags %08lx", name, g != NULL ? (unsigned long)g->Data1 : 0UL,
             (unsigned long)hal->dwFlags );
    if( hal->dwFlags != 0 ) {
        fprintf( out, " render %08lx z %08lx devcaps %08lx raster %08lx zcmp %08lx tex %08lx",
                 (unsigned long)hal->dwDeviceRenderBitDepth, (unsigned long)hal->dwDeviceZBufferBitDepth,
                 (unsigned long)hal->dwDevCaps, (unsigned long)hal->dpcTriCaps.dwRasterCaps,
                 (unsigned long)hal->dpcTriCaps.dwZCmpCaps, (unsigned long)hal->dpcTriCaps.dwTextureCaps );
    } else {
        fprintf( out, " hel render %08lx z %08lx", (unsigned long)hel->dwDeviceRenderBitDepth,
                 (unsigned long)hel->dwDeviceZBufferBitDepth );
    }
    fprintf( out, "\n" );
    fflush( out );
    return( D3DENUMRET_OK );
}

static HRESULT WINAPI on_zformat( LPDDPIXELFORMAT f, LPVOID ctx )
{
    (void)ctx;
    fprintf( out, "    z format flags %08lx bits %lu\n", (unsigned long)f->dwFlags, (unsigned long)f->dwRGBBitCount );
    return( D3DENUMRET_OK );
}

static void report( LPDIRECTDRAW dd, const char *where )
{
    DDCAPS          hw;
    DDCAPS          he;
    LPDIRECT3D2     d3d = NULL;
    HRESULT         hr;
    DDSURFACEDESC   sd;

    memset( &hw, 0, sizeof( hw ) );
    hw.dwSize = sizeof( hw );
    memset( &he, 0, sizeof( he ) );
    he.dwSize = sizeof( he );
    memset( &sd, 0, sizeof( sd ) );
    sd.dwSize = sizeof( sd );
    dd->lpVtbl->GetDisplayMode( dd, &sd );
    fprintf( out, "%s: mode %lux%lux%lu\n", where, (unsigned long)sd.dwWidth, (unsigned long)sd.dwHeight,
             (unsigned long)sd.ddpfPixelFormat.dwRGBBitCount );
    hr = dd->lpVtbl->GetCaps( dd, &hw, &he );
    fprintf( out, "  getcaps %08lx  hw caps %08lx caps2 %08lx surfaces %08lx zdepths %08lx  hel caps2 %08lx\n",
             (unsigned long)hr, (unsigned long)hw.dwCaps, (unsigned long)hw.dwCaps2,
             (unsigned long)hw.ddsCaps.dwCaps, (unsigned long)hw.dwZBufferBitDepths, (unsigned long)he.dwCaps2 );
    hr = dd->lpVtbl->QueryInterface( dd, &iid_d3d2, (LPVOID *)&d3d );
    fprintf( out, "  d3d2 %08lx\n", (unsigned long)hr );
    fflush( out );
    if( hr == DD_OK ) {
        hr = d3d->lpVtbl->EnumDevices( d3d, on_device, NULL );
        fprintf( out, "  enum %08lx\n", (unsigned long)hr );
        d3d->lpVtbl->Release( d3d );
    }
    fflush( out );
    (void)on_zformat;
}

static LRESULT CALLBACK wndproc( HWND w, UINT m, WPARAM a, LPARAM b )
{
    return( DefWindowProc( w, m, a, b ) );
}

int PASCAL WinMain( HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show )
{
    WNDCLASS        wc;
    HWND            wnd;
    LPDIRECTDRAW    dd;
    HRESULT         hr;

    (void)prev;
    (void)show;
    out = fopen( "C:\\M8ENUM.TXT", "a" );
    if( out == NULL ) {
        return( 1 );
    }
    fprintf( out, "== M8ENUM %s\n", cmd );
    hr = DirectDrawCreate( NULL, &dd, NULL );
    fprintf( out, "ddraw %08lx\n", (unsigned long)hr );
    fflush( out );
    if( hr != DD_OK ) {
        fclose( out );
        return( 1 );
    }
    report( dd, "desktop" );
    memset( &wc, 0, sizeof( wc ) );
    wc.lpfnWndProc = wndproc;
    wc.hInstance = inst;
    wc.lpszClassName = "M8ENUM";
    RegisterClass( &wc );
    wnd = CreateWindowEx( WS_EX_TOPMOST, "M8ENUM", "M8ENUM", WS_POPUP | WS_VISIBLE, 0, 0, 640, 480, NULL, NULL, inst, NULL );
    dd->lpVtbl->SetCooperativeLevel( dd, wnd, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN );
    hr = dd->lpVtbl->SetDisplayMode( dd, 640, 480, 16 );
    fprintf( out, "setmode %08lx\n", (unsigned long)hr );
    if( hr == DD_OK ) {
        report( dd, "640x480x16" );
    }
    dd->lpVtbl->RestoreDisplayMode( dd );
    dd->lpVtbl->SetCooperativeLevel( dd, wnd, DDSCL_NORMAL );
    dd->lpVtbl->Release( dd );
    fprintf( out, "\n" );
    fclose( out );
    DestroyWindow( wnd );
    return( 0 );
}
