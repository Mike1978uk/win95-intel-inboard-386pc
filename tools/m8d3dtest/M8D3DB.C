/* M8D3DB.EXE - flat triangles a second through Direct3D, our HAL against Microsoft's
 * RGB software renderer, on one scene. A frame is a clear, a batch of triangles in a
 * 256x128 target, and a copy to the screen, so a software frame pays for its pixels
 * crossing the bus and a HAL frame does not. Results append to C:\M8D3DB.TXT.
 *
 *   M8D3DB <label> [seconds per test, default 3]
 *
 * At 8 bpp the HAL needs the patched D3DIM.DLL beside this program.
 * Build: wcl386 -q -bt=nt -l=nt_win -ox -i=<watcom>h -i=<watcom>h\nt -i=<watcom>h\nt\directx
 *        -fe=M8D3DB.EXE M8D3DB.C ddraw.lib
 */

#define INITGUID
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ddraw.h>
#include <d3d.h>

/* d3d.h's GUIDs, here: Watcom's headers declare them but no library has them. */
static const GUID m8_iid_d3d2 = { 0x6aae1ec1, 0x662a, 0x11d0, { 0x88, 0x9d, 0x00, 0xaa, 0x00, 0xbb, 0xb7, 0x6a } };
static const GUID m8_iid_hal = { 0x84e63de0, 0x46aa, 0x11cf, { 0x81, 0x6f, 0x00, 0x00, 0xc0, 0x20, 0x15, 0x6e } };
static const GUID m8_iid_rgb = { 0xa4665c60, 0x2673, 0x11cf, { 0xa3, 0x1a, 0x00, 0xaa, 0x00, 0xb9, 0x33, 0x56 } };

#define TARGET_W    256
#define TARGET_H    128
#define MAX_TRIS    64

static FILE                 *out;
static LPDIRECTDRAW         dd;
static LPDIRECT3D2          d3d;
static LPDIRECTDRAWSURFACE  primary;
static LPDIRECTDRAWPALETTE  palette;
static D3DTLVERTEX          tri[MAX_TRIS * 3];
static double               qpf;

static double now( void )
{
    LARGE_INTEGER t;

    QueryPerformanceCounter( &t );
    return( (double)t.LowPart + 4294967296.0 * (double)t.HighPart );
}

/* `n` clockwise triangles of `size` pixels, spread over the target, each its own grey. */
static void scene( int n, int size )
{
    int i;
    int j;
    int x;
    int y;

    memset( tri, 0, sizeof( tri ) );
    for( i = 0; i < n; ++i ) {
        x = (i * 37) % (TARGET_W - size - 1);
        y = (i * 23) % (TARGET_H - size - 1);
        tri[3 * i].sx = (float)x;
        tri[3 * i].sy = (float)y;
        tri[3 * i + 1].sx = (float)(x + size);
        tri[3 * i + 1].sy = (float)(y + size);
        tri[3 * i + 2].sx = (float)x;
        tri[3 * i + 2].sy = (float)(y + size);
        for( j = 0; j < 3; ++j ) {
            tri[3 * i + j].sz = 0.5f;
            tri[3 * i + j].rhw = 1.0f;
            tri[3 * i + j].color = RGBA_MAKE( 64 + i * 3, 64 + i * 3, 64 + i * 3, 255 );
        }
    }
}

static void test( const char *name, const GUID *device, DWORD memory, int n, int size, int seconds )
{
    LPDIRECTDRAWSURFACE     target = NULL;
    LPDIRECT3DDEVICE2       dev = NULL;
    LPDIRECT3DVIEWPORT2     vp = NULL;
    DDSURFACEDESC           sd;
    D3DVIEWPORT2            v;
    DDBLTFX                 fx;
    RECT                    to;
    HRESULT                 hr;
    double                  t0;
    double                  t;
    unsigned long           frames = 0;
    unsigned long           bad = 0;

    memset( &sd, 0, sizeof( sd ) );
    sd.dwSize = sizeof( sd );
    sd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    sd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_3DDEVICE | memory;
    sd.dwWidth = TARGET_W;
    sd.dwHeight = TARGET_H;
    hr = dd->lpVtbl->CreateSurface( dd, &sd, &target, NULL );
    if( hr != DD_OK ) {
        fprintf( out, "%-4s %3d x %3d px  no target %08lx\n", name, n, size, (unsigned long)hr );
        return;
    }
    target->lpVtbl->SetPalette( target, palette );
    hr = d3d->lpVtbl->CreateDevice( d3d, (GUID *)device, target, &dev );
    if( hr != DD_OK ) {
        fprintf( out, "%-4s %3d x %3d px  no device %08lx\n", name, n, size, (unsigned long)hr );
        target->lpVtbl->Release( target );
        return;
    }
    d3d->lpVtbl->CreateViewport( d3d, &vp, NULL );
    memset( &v, 0, sizeof( v ) );
    v.dwSize = sizeof( v );
    v.dwWidth = TARGET_W;
    v.dwHeight = TARGET_H;
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

    scene( n, size );
    memset( &fx, 0, sizeof( fx ) );
    fx.dwSize = sizeof( fx );
    SetRect( &to, 40, 40, 40 + TARGET_W, 40 + TARGET_H );
    t0 = now();
    do {
        if( target->lpVtbl->Blt( target, NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx ) != DD_OK ) {
            ++bad;
        }
        if( dev->lpVtbl->BeginScene( dev ) != D3D_OK ||
            dev->lpVtbl->DrawPrimitive( dev, D3DPT_TRIANGLELIST, D3DVT_TLVERTEX, tri, 3 * n, D3DDP_WAIT ) != D3D_OK ||
            dev->lpVtbl->EndScene( dev ) != D3D_OK ) {
            ++bad;
        }
        if( primary->lpVtbl->Blt( primary, &to, target, NULL, DDBLT_WAIT, NULL ) != DD_OK ) {
            ++bad;
        }
        ++frames;
        t = (now() - t0) / qpf;
    } while( t < (double)seconds );
    fprintf( out, "%-4s %3d x %3d px  %8.1f fps %9.0f tri/s  %8.2f ms/frame  bad %lu\n", name, n, size,
             frames / t, frames * (double)n / t, 1000.0 * t / frames, bad );
    fflush( out );
    vp->lpVtbl->Release( vp );
    dev->lpVtbl->Release( dev );
    target->lpVtbl->Release( target );
}

int PASCAL WinMain( HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show )
{
    static const int    sizes[] = { 8, 32, 100 };
    PALETTEENTRY        pal[256];
    DDSURFACEDESC       sd;
    LARGE_INTEGER       f;
    char                label[32];
    int                 seconds = 3;
    int                 i;

    out = fopen( "C:\\M8D3DB.TXT", "a" );
    if( out == NULL ) {
        return( 1 );
    }
    label[0] = '\0';
    sscanf( cmd, "%31s %d", label, &seconds );
    QueryPerformanceFrequency( &f );
    qpf = (double)f.LowPart;
    fprintf( out, "== M8D3DB %s\n", label );
    if( DirectDrawCreate( NULL, &dd, NULL ) != DD_OK ) {
        fprintf( out, "no ddraw\n" );
        return( 1 );
    }
    dd->lpVtbl->SetCooperativeLevel( dd, NULL, DDSCL_NORMAL );
    if( dd->lpVtbl->QueryInterface( dd, &m8_iid_d3d2, (LPVOID *)&d3d ) != DD_OK ) {
        fprintf( out, "no d3d2\n" );
        return( 1 );
    }
    memset( pal, 0, sizeof( pal ) );
    for( i = 0; i < 256; ++i ) {
        pal[i].peRed = pal[i].peGreen = pal[i].peBlue = (BYTE)i;
    }
    dd->lpVtbl->CreatePalette( dd, DDPCAPS_8BIT | DDPCAPS_ALLOW256, pal, &palette, NULL );
    memset( &sd, 0, sizeof( sd ) );
    sd.dwSize = sizeof( sd );
    sd.dwFlags = DDSD_CAPS;
    sd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    if( dd->lpVtbl->CreateSurface( dd, &sd, &primary, NULL ) != DD_OK ) {
        fprintf( out, "no primary\n" );
        return( 1 );
    }
    for( i = 0; i < 3; ++i ) {
        test( "HAL", &m8_iid_hal, DDSCAPS_VIDEOMEMORY, 16, sizes[i], seconds );
    }
    for( i = 0; i < 3; ++i ) {
        test( "RGB", &m8_iid_rgb, DDSCAPS_SYSTEMMEMORY, 16, sizes[i], seconds );
    }
    fprintf( out, "\n" );
    fclose( out );
    primary->lpVtbl->Release( primary );
    palette->lpVtbl->Release( palette );
    d3d->lpVtbl->Release( d3d );
    dd->lpVtbl->Release( dd );
    return( 0 );
}
