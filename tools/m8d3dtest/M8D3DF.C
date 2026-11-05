/* M8D3DF.EXE - Direct3D at 16 bpp on stock DirectX: a full-screen flip chain, flat
 * triangles on the back buffer, by our HAL and by Microsoft's RGB software renderer.
 * Each test shows moving triangles and reports frames a second; at the end the back
 * buffer is locked, which puts the triangles in RAM, and its lit pixels are counted.
 * Results append to C:\M8D3DF.TXT.
 *
 *   M8D3DF <label> [seconds per test, default 3]
 *
 * Build: wcl386 -q -bt=nt -l=nt_win -ox -i=<watcom>h -i=<watcom>h\nt -i=<watcom>h\nt\directx
 *        -fe=M8D3DF.EXE M8D3DF.C ddraw.lib
 */

#define INITGUID
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <ddraw.h>
#include <d3d.h>

static const GUID m8_iid_d3d2 = { 0x6aae1ec1, 0x662a, 0x11d0, { 0x88, 0x9d, 0x00, 0xaa, 0x00, 0xbb, 0xb7, 0x6a } };
static const GUID m8_iid_hal = { 0x84e63de0, 0x46aa, 0x11cf, { 0x81, 0x6f, 0x00, 0x00, 0xc0, 0x20, 0x15, 0x6e } };
static const GUID m8_iid_rgb = { 0xa4665c60, 0x2673, 0x11cf, { 0xa3, 0x1a, 0x00, 0xaa, 0x00, 0xb9, 0x33, 0x56 } };

#define SCR_W       640
#define SCR_H       480
#define MAX_TRIS    64

static FILE                 *out;
static LPDIRECTDRAW         dd;
static LPDIRECT3D2          d3d;
static LPDIRECTDRAWSURFACE  prim;
static LPDIRECTDRAWSURFACE  back;
static D3DTLVERTEX          tri[MAX_TRIS * 3];
static double               qpf;

static double now( void )
{
    LARGE_INTEGER t;

    QueryPerformanceCounter( &t );
    return( (double)t.LowPart + 4294967296.0 * (double)t.HighPart );
}

static void say( const char *what, HRESULT hr )
{
    fprintf( out, "%-14s %08lx\n", what, (unsigned long)hr );
    fflush( out );
}

/* Clockwise triangles of `size` pixels, drifting by `frame`, each its own colour. */
static void scene( int n, int size, int frame )
{
    int i;
    int j;
    int x;
    int y;

    for( i = 0; i < n; ++i ) {
        x = (i * 83 + frame * 3) % (SCR_W - size - 1);
        y = (i * 47 + frame * 2) % (SCR_H - size - 1);
        tri[3 * i].sx = (float)x;
        tri[3 * i].sy = (float)y;
        tri[3 * i + 1].sx = (float)(x + size);
        tri[3 * i + 1].sy = (float)(y + size);
        tri[3 * i + 2].sx = (float)x;
        tri[3 * i + 2].sy = (float)(y + size);
        for( j = 0; j < 3; ++j ) {
            tri[3 * i + j].sz = 0.5f;
            tri[3 * i + j].rhw = 1.0f;
            tri[3 * i + j].color = RGBA_MAKE( i * 16 & 255, 255 - i * 16 & 255, i * 40 & 255, 255 );
        }
    }
}

static void test( const char *name, const GUID *device, int n, int size, int seconds )
{
    LPDIRECT3DDEVICE2       dev = NULL;
    LPDIRECT3DVIEWPORT2     vp = NULL;
    D3DVIEWPORT2            v;
    DDBLTFX                 fx;
    DDSURFACEDESC           sd;
    HRESULT                 hr;
    double                  t0;
    double                  t;
    unsigned long           frames = 0;
    unsigned long           bad = 0;
    unsigned long           lit = 0;
    int                     x;
    int                     y;

    hr = d3d->lpVtbl->CreateDevice( d3d, (GUID *)device, back, &dev );
    if( hr != DD_OK ) {
        fprintf( out, "%-4s %3d x %3d px  no device %08lx\n", name, n, size, (unsigned long)hr );
        fflush( out );
        return;
    }
    d3d->lpVtbl->CreateViewport( d3d, &vp, NULL );
    memset( &v, 0, sizeof( v ) );
    v.dwSize = sizeof( v );
    v.dwWidth = SCR_W;
    v.dwHeight = SCR_H;
    v.dvClipX = -1.0f;
    v.dvClipY = 1.0f;
    v.dvClipWidth = 2.0f;
    v.dvClipHeight = 2.0f;
    v.dvMaxZ = 1.0f;
    dev->lpVtbl->AddViewport( dev, vp );
    vp->lpVtbl->SetViewport2( vp, &v );
    dev->lpVtbl->SetCurrentViewport( dev, vp );
    dev->lpVtbl->SetRenderState( dev, D3DRENDERSTATE_SHADEMODE, D3DSHADE_FLAT );
    dev->lpVtbl->SetRenderState( dev, D3DRENDERSTATE_CULLMODE, D3DCULL_NONE );
    memset( tri, 0, sizeof( tri ) );
    memset( &fx, 0, sizeof( fx ) );
    fx.dwSize = sizeof( fx );
    t0 = now();
    do {
        scene( n, size, (int)frames );
        if( back->lpVtbl->Blt( back, NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx ) != DD_OK ) {
            ++bad;
        }
        if( dev->lpVtbl->BeginScene( dev ) != D3D_OK ||
            dev->lpVtbl->DrawPrimitive( dev, D3DPT_TRIANGLELIST, D3DVT_TLVERTEX, tri, 3 * n, D3DDP_WAIT ) != D3D_OK ||
            dev->lpVtbl->EndScene( dev ) != D3D_OK ) {
            ++bad;
        }
        if( prim->lpVtbl->Flip( prim, NULL, DDFLIP_WAIT ) != DD_OK ) {
            ++bad;
        }
        ++frames;
        t = (now() - t0) / qpf;
    } while( t < (double)seconds );
    /* One more frame drawn and read back through RAM. */
    back->lpVtbl->Blt( back, NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx );
    dev->lpVtbl->BeginScene( dev );
    dev->lpVtbl->DrawPrimitive( dev, D3DPT_TRIANGLELIST, D3DVT_TLVERTEX, tri, 3 * n, D3DDP_WAIT );
    dev->lpVtbl->EndScene( dev );
    memset( &sd, 0, sizeof( sd ) );
    sd.dwSize = sizeof( sd );
    if( back->lpVtbl->Lock( back, NULL, &sd, DDLOCK_WAIT | DDLOCK_READONLY, NULL ) == DD_OK ) {
        for( y = 0; y < SCR_H; ++y ) {
            for( x = 0; x < SCR_W; ++x ) {
                lit += *(WORD *)((BYTE *)sd.lpSurface + y * sd.lPitch + 2 * x) != 0;
            }
        }
        back->lpVtbl->Unlock( back, NULL );
    }
    fprintf( out, "%-4s %3d x %3d px  %8.1f fps %9.0f tri/s  %8.2f ms/frame  bad %lu  lit %lu\n", name, n, size,
             frames / t, frames * (double)n / t, 1000.0 * t / frames, bad, lit );
    fflush( out );
    vp->lpVtbl->Release( vp );
    dev->lpVtbl->Release( dev );
}

static LRESULT CALLBACK wndproc( HWND w, UINT m, WPARAM a, LPARAM b )
{
    return( DefWindowProc( w, m, a, b ) );
}

int PASCAL WinMain( HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show )
{
    static const int    sizes[] = { 8, 32, 100 };
    WNDCLASS            wc;
    HWND                wnd;
    DDSURFACEDESC       sd;
    DDSCAPS             caps;
    LARGE_INTEGER       f;
    char                label[32];
    int                 seconds = 3;
    int                 i;
    HRESULT             hr;

    out = fopen( "C:\\M8D3DF.TXT", "a" );
    if( out == NULL ) {
        return( 1 );
    }
    label[0] = '\0';
    sscanf( cmd, "%31s %d", label, &seconds );
    QueryPerformanceFrequency( &f );
    qpf = (double)f.LowPart;
    fprintf( out, "== M8D3DF %s\n", label );
    memset( &wc, 0, sizeof( wc ) );
    wc.lpfnWndProc = wndproc;
    wc.hInstance = inst;
    wc.lpszClassName = "M8D3DF";
    RegisterClass( &wc );
    wnd = CreateWindowEx( WS_EX_TOPMOST, "M8D3DF", "M8D3DF", WS_POPUP | WS_VISIBLE, 0, 0, SCR_W, SCR_H,
                          NULL, NULL, inst, NULL );
    hr = DirectDrawCreate( NULL, &dd, NULL );
    say( "ddraw", hr );
    if( hr != DD_OK ) {
        return( 1 );
    }
    say( "coop", dd->lpVtbl->SetCooperativeLevel( dd, wnd, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN ) );
    hr = dd->lpVtbl->SetDisplayMode( dd, SCR_W, SCR_H, 16 );
    say( "setmode", hr );
    hr = dd->lpVtbl->QueryInterface( dd, &m8_iid_d3d2, (LPVOID *)&d3d );
    say( "d3d2", hr );
    if( hr == DD_OK ) {
        memset( &sd, 0, sizeof( sd ) );
        sd.dwSize = sizeof( sd );
        sd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
        sd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX | DDSCAPS_3DDEVICE;
        sd.dwBackBufferCount = 1;
        hr = dd->lpVtbl->CreateSurface( dd, &sd, &prim, NULL );
        say( "flipchain", hr );
        if( hr == DD_OK ) {
            caps.dwCaps = DDSCAPS_BACKBUFFER;
            say( "attached", prim->lpVtbl->GetAttachedSurface( prim, &caps, &back ) );
            for( i = 0; i < 3; ++i ) {
                test( "HAL", &m8_iid_hal, 16, sizes[i], seconds );
            }
            for( i = 0; i < 3; ++i ) {
                test( "RGB", &m8_iid_rgb, 16, sizes[i], seconds );
            }
            prim->lpVtbl->Release( prim );
        }
        d3d->lpVtbl->Release( d3d );
    }
    dd->lpVtbl->RestoreDisplayMode( dd );
    dd->lpVtbl->SetCooperativeLevel( dd, wnd, DDSCL_NORMAL );
    dd->lpVtbl->Release( dd );
    fprintf( out, "\n" );
    fclose( out );
    DestroyWindow( wnd );
    return( 0 );
}
