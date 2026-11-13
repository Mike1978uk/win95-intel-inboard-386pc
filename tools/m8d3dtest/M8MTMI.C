/* M8MTMI: Monster Truck Madness's Direct3D start-up, call for call, with every result logged.
 *
 * Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
 *
 * The sequence is MONSTERX.EXE's (0x494880 full screen, 0x495260 device choice, 0x4954c0 device,
 * 0x495740 textures): DirectDraw 1 and Direct3D 1 interfaces, the HAL device taken from the back
 * buffer, at least 96 textures of 32x32 in video memory. Appends to C:\M8MTMI.TXT. The first
 * failing call is the thing the game is missing.
 *
 * Build: wcl386 -q -bt=nt -l=nt_win -ox -i=<watcom>h -i=<watcom>h\nt -i=<watcom>h\nt\directx
 *        "-fe=M8MTMI.EXE" M8MTMI.C ddraw.lib
 */

#define INITGUID
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <ddraw.h>
#include <d3d.h>

static const GUID iid_d3d = { 0x3bba0080, 0x2421, 0x11cf, { 0xa3, 0x1a, 0x00, 0xaa, 0x00, 0xb9, 0x33, 0x56 } };
static const GUID iid_tex = { 0x2cdcd9e0, 0x25a0, 0x11cf, { 0xa3, 0x1a, 0x00, 0xaa, 0x00, 0xb9, 0x33, 0x56 } };
static const GUID iid_hal = { 0x84e63de0, 0x46aa, 0x11cf, { 0x81, 0x6f, 0x00, 0x00, 0xc0, 0x20, 0x15, 0x6e } };

static FILE             *out;
static DDSURFACEDESC    texfmt;
static int              texfmt_ok;
static int              hal_seen;

static HRESULT say( const char *what, HRESULT hr )
{
    fprintf( out, "%-28s %08lx%s\n", what, (unsigned long)hr, hr == DD_OK ? "" : "  <-- FAILS" );
    fflush( out );
    return( hr );
}

static HRESULT WINAPI dev_cb( LPGUID guid, LPSTR desc, LPSTR name, LPD3DDEVICEDESC hw, LPD3DDEVICEDESC sw, LPVOID ctx )
{
    (void)desc;
    (void)sw;
    (void)ctx;
    fprintf( out, "device %-22s flags %08lx devcaps %08lx shade %08lx depth %08lx texcaps %08lx\n", name,
             hw->dwFlags, hw->dwDevCaps, hw->dpcTriCaps.dwShadeCaps, hw->dwDeviceRenderBitDepth,
             hw->dpcTriCaps.dwTextureCaps );
    if( IsEqualGUID( guid, &iid_hal ) ) {
        hal_seen = 1;
    }
    return( D3DENUMRET_OK );
}

/* As the game: the first 16-bit format, taken over by one with alpha 8000h. */
static HRESULT WINAPI fmt_cb( LPDDSURFACEDESC d, LPVOID ctx )
{
    (void)ctx;
    fprintf( out, "texture format bits %lu masks %04lx %04lx %04lx %04lx\n", d->ddpfPixelFormat.dwRGBBitCount,
             d->ddpfPixelFormat.dwRBitMask, d->ddpfPixelFormat.dwGBitMask, d->ddpfPixelFormat.dwBBitMask,
             d->ddpfPixelFormat.dwRGBAlphaBitMask );
    if( d->ddpfPixelFormat.dwRGBBitCount == 16 && (!texfmt_ok || d->ddpfPixelFormat.dwRGBAlphaBitMask == 0x8000) ) {
        texfmt = *d;
        texfmt_ok = 1;
    }
    return( DDENUMRET_OK );
}

static LRESULT CALLBACK wndproc( HWND w, UINT m, WPARAM a, LPARAM b )
{
    return( DefWindowProc( w, m, a, b ) );
}

int PASCAL WinMain( HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show )
{
    WNDCLASS                wc;
    HWND                    wnd;
    LPDIRECTDRAW            dd = NULL;
    LPDIRECT3D              d3d = NULL;
    LPDIRECTDRAWSURFACE     prim = NULL;
    LPDIRECTDRAWSURFACE     back = NULL;
    LPDIRECT3DDEVICE        dev = NULL;
    LPDIRECT3DVIEWPORT      vp = NULL;
    LPDIRECTDRAWSURFACE     stage = NULL;
    LPDIRECT3DTEXTURE       stage_t = NULL;
    static LPDIRECTDRAWSURFACE  ts[128];
    static LPDIRECT3DTEXTURE    tt[128];
    LPDIRECT3DEXECUTEBUFFER eb = NULL;
    D3DEXECUTEBUFFERDESC    ebd;
    D3DDEVICEDESC           hw;
    D3DDEVICEDESC           sw;
    DDSURFACEDESC           sd;
    DDSCAPS                 caps;
    D3DVIEWPORT             v;
    D3DTEXTUREHANDLE        th;
    HRESULT                 hr;
    int                     n;
    int                     made = 0;

    (void)prev;
    (void)show;
    out = fopen( "C:\\M8MTMI.TXT", "a" );
    if( out == NULL ) {
        return( 1 );
    }
    fprintf( out, "== M8MTMI %s\n", cmd );
    memset( &wc, 0, sizeof( wc ) );
    wc.lpfnWndProc = wndproc;
    wc.hInstance = inst;
    wc.lpszClassName = "M8MTMI";
    RegisterClass( &wc );
    wnd = CreateWindowEx( WS_EX_TOPMOST, "M8MTMI", "M8MTMI", WS_POPUP | WS_VISIBLE, 0, 0, 640, 480, NULL, NULL, inst, NULL );

    if( say( "DirectDrawCreate", DirectDrawCreate( NULL, &dd, NULL ) ) != DD_OK ) {
        goto done;
    }
    if( say( "SetCooperativeLevel 11h", dd->lpVtbl->SetCooperativeLevel( dd, wnd, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN ) ) != DD_OK ||
        say( "SetDisplayMode 640x480x16", dd->lpVtbl->SetDisplayMode( dd, 640, 480, 16 ) ) != DD_OK ) {
        goto done;
    }
    memset( &sd, 0, sizeof( sd ) );
    sd.dwSize = sizeof( sd );
    sd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
    sd.dwBackBufferCount = 1;
    sd.ddsCaps.dwCaps = 0x6218;     /* PRIMARY | FLIP | COMPLEX | 3DDEVICE | VIDEOMEMORY */
    if( say( "CreateSurface primary 6218h", dd->lpVtbl->CreateSurface( dd, &sd, &prim, NULL ) ) != DD_OK ) {
        goto done;
    }
    caps.dwCaps = DDSCAPS_BACKBUFFER;
    if( say( "GetAttachedSurface back", prim->lpVtbl->GetAttachedSurface( prim, &caps, &back ) ) != DD_OK ) {
        goto done;
    }
    if( say( "QI IDirect3D", dd->lpVtbl->QueryInterface( dd, &iid_d3d, (LPVOID *)&d3d ) ) != DD_OK ) {
        goto done;
    }
    say( "EnumDevices", d3d->lpVtbl->EnumDevices( d3d, dev_cb, NULL ) );
    fprintf( out, "HAL listed: %d\n", hal_seen );
    if( say( "QI HAL device from back", back->lpVtbl->QueryInterface( back, &iid_hal, (LPVOID *)&dev ) ) != DD_OK ) {
        goto done;
    }
    memset( &hw, 0, sizeof( hw ) );
    memset( &sw, 0, sizeof( sw ) );
    hw.dwSize = sizeof( hw );
    sw.dwSize = sizeof( sw );
    say( "Device GetCaps", dev->lpVtbl->GetCaps( dev, &hw, &sw ) );
    fprintf( out, "  hw devcaps %08lx maxbuffer %lu maxvertex %lu\n", hw.dwDevCaps, hw.dwMaxBufferSize, hw.dwMaxVertexCount );
    say( "EnumTextureFormats", dev->lpVtbl->EnumTextureFormats( dev, fmt_cb, NULL ) );
    if( !texfmt_ok ) {
        fprintf( out, "no 16-bit texture format  <-- FAILS\n" );
        goto done;
    }
    if( say( "CreateViewport", d3d->lpVtbl->CreateViewport( d3d, &vp, NULL ) ) != DD_OK ||
        say( "AddViewport", dev->lpVtbl->AddViewport( dev, vp ) ) != DD_OK ) {
        goto done;
    }
    memset( &v, 0, sizeof( v ) );
    v.dwSize = sizeof( v );
    v.dwWidth = 640;
    v.dwHeight = 480;
    v.dvScaleX = 320.0f;
    v.dvScaleY = 240.0f;
    v.dvMaxX = 1.0f;
    v.dvMaxY = 1.0f;
    if( say( "SetViewport", vp->lpVtbl->SetViewport( vp, &v ) ) != DD_OK ) {
        goto done;
    }
    /* The staging texture in system memory. */
    sd = texfmt;
    sd.dwSize = sizeof( sd );
    sd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
    sd.dwWidth = 32;
    sd.dwHeight = 32;
    sd.ddsCaps.dwCaps = DDSCAPS_TEXTURE | DDSCAPS_SYSTEMMEMORY;
    if( say( "CreateSurface stage 1800h", dd->lpVtbl->CreateSurface( dd, &sd, &stage, NULL ) ) != DD_OK ||
        say( "QI texture (stage)", stage->lpVtbl->QueryInterface( stage, &iid_tex, (LPVOID *)&stage_t ) ) != DD_OK ) {
        goto done;
    }
    /* The game's textures: it needs 96. */
    for( n = 0; n < 128; ++n ) {
        sd.ddsCaps.dwCaps = 0x4005000;  /* ALLOCONLOAD | VIDEOMEMORY | TEXTURE */
        hr = dd->lpVtbl->CreateSurface( dd, &sd, &ts[n], NULL );
        if( hr != DD_OK ) {
            if( n < 2 || n >= 96 ) {
                say( "CreateSurface texture 4005000h", hr );
            }
            break;
        }
        hr = ts[n]->lpVtbl->QueryInterface( ts[n], &iid_tex, (LPVOID *)&tt[n] );
        if( hr != DD_OK ) {
            say( "QI texture", hr );
            ts[n]->lpVtbl->Release( ts[n] );
            break;
        }
        hr = tt[n]->lpVtbl->GetHandle( tt[n], dev, &th );
        if( hr != DD_OK ) {
            say( "Texture GetHandle", hr );
            tt[n]->lpVtbl->Release( tt[n] );
            ts[n]->lpVtbl->Release( ts[n] );
            break;
        }
        if( n == 0 ) {
            say( "Texture Load (first)", tt[n]->lpVtbl->Load( tt[n], stage_t ) );
        }
        ++made;
    }
    fprintf( out, "textures made %d (game needs 96)%s\n", made, made < 96 ? "  <-- FAILS" : "" );
    memset( &ebd, 0, sizeof( ebd ) );
    ebd.dwSize = sizeof( ebd );
    ebd.dwFlags = D3DDEB_BUFSIZE;
    ebd.dwBufferSize = hw.dwMaxBufferSize != 0 && hw.dwMaxBufferSize < 0x14000 + 0x800 * 32 ? hw.dwMaxBufferSize : 0x14000 + 0x800 * 32;
    say( "CreateExecuteBuffer", dev->lpVtbl->CreateExecuteBuffer( dev, &ebd, &eb, NULL ) );

done:
    if( eb != NULL ) {
        eb->lpVtbl->Release( eb );
    }
    for( n = 0; n < made; ++n ) {
        tt[n]->lpVtbl->Release( tt[n] );
        ts[n]->lpVtbl->Release( ts[n] );
    }
    if( stage_t != NULL ) {
        stage_t->lpVtbl->Release( stage_t );
    }
    if( stage != NULL ) {
        stage->lpVtbl->Release( stage );
    }
    if( vp != NULL ) {
        vp->lpVtbl->Release( vp );
    }
    if( dev != NULL ) {
        dev->lpVtbl->Release( dev );
    }
    if( d3d != NULL ) {
        d3d->lpVtbl->Release( d3d );
    }
    if( prim != NULL ) {
        prim->lpVtbl->Release( prim );
    }
    if( dd != NULL ) {
        dd->lpVtbl->RestoreDisplayMode( dd );
        dd->lpVtbl->SetCooperativeLevel( dd, wnd, DDSCL_NORMAL );
        dd->lpVtbl->Release( dd );
    }
    fprintf( out, "\n" );
    fclose( out );
    DestroyWindow( wnd );
    return( 0 );
}
