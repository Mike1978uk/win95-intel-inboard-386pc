/* M8D3DT.EXE - does Direct3D take our 8 bpp HAL (plan P6/D1)? Lists the
 * Direct3D devices, creates the HAL device on an off-screen 3D surface and
 * draws one flat triangle; every step's HRESULT to C:\M8D3DT.TXT.
 *
 *   M8D3DT [label]
 *
 * DirectX 5 interfaces (IDirect3D2), which DirectX 5-7 runtimes all serve.
 * Build: wcl386 -q -bt=nt -l=nt_win -ox -i=<watcom>h
tdirectx -fe=M8D3DT.EXE M8D3DT.C ddraw.lib
 */

#define INITGUID
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <ddraw.h>
#include <d3d.h>

/* d3d.h's GUIDs, here: Watcom's headers declare them but no library has them. */
static const GUID m8_iid_d3d2 = { 0x6aae1ec1, 0x662a, 0x11d0, { 0x88, 0x9d, 0x00, 0xaa, 0x00, 0xbb, 0xb7, 0x6a } };
static const GUID m8_iid_hal = { 0x84e63de0, 0x46aa, 0x11cf, { 0x81, 0x6f, 0x00, 0x00, 0xc0, 0x20, 0x15, 0x6e } };

static FILE *out;

static void say( const char *what, HRESULT hr )
{
    fprintf( out, "%-16s %08lx\n", what, (unsigned long)hr );
    fflush( out );
}

static HRESULT WINAPI device( LPGUID guid, LPSTR desc, LPSTR name, LPD3DDEVICEDESC hw,
                              LPD3DDEVICEDESC sw, LPVOID ctx )
{
    fprintf( out, "device %-24s hw model %lu depth %08lx shade %08lx  %s\n", name,
             hw->dcmColorModel, hw->dwDeviceRenderBitDepth, hw->dpcTriCaps.dwShadeCaps,
             IsEqualGUID( guid, &m8_iid_hal ) ? "HAL" : "" );
    fflush( out );
    return( D3DENUMRET_OK );
}

int PASCAL WinMain( HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show )
{
    LPDIRECTDRAW            dd;
    LPDIRECT3D2             d3d;
    LPDIRECTDRAWSURFACE     target;
    LPDIRECTDRAWSURFACE     primary;
    DDBLTFX                 fx;
    LPDIRECTDRAWPALETTE     palette;
    PALETTEENTRY            pal[256];
    unsigned long           stat_addr;
    DWORD                   *stat;
    LPDIRECT3DDEVICE2       dev;
    LPDIRECT3DVIEWPORT2     vp;
    DDSURFACEDESC           sd;
    D3DVIEWPORT2            v;
    D3DTLVERTEX             tri[3];
    HRESULT                 hr;
    int                     i;

    out = fopen( "C:\\M8D3DT.TXT", "a" );
    if( out == NULL ) {
        return( 1 );
    }
    fprintf( out, "== M8D3DT %s\n", cmd );
    hr = DirectDrawCreate( NULL, &dd, NULL );
    say( "ddraw", hr );
    if( hr != DD_OK ) {
        return( 1 );
    }
    dd->lpVtbl->SetCooperativeLevel( dd, NULL, DDSCL_NORMAL );
    hr = dd->lpVtbl->QueryInterface( dd, &m8_iid_d3d2, (LPVOID *)&d3d );
    say( "d3d2", hr );
    if( hr == DD_OK ) {
        d3d->lpVtbl->EnumDevices( d3d, device, NULL );
        memset( &sd, 0, sizeof( sd ) );
        sd.dwSize = sizeof( sd );
        sd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
        sd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_3DDEVICE | DDSCAPS_VIDEOMEMORY;
        sd.dwWidth = 256;
        sd.dwHeight = 128;
        hr = dd->lpVtbl->CreateSurface( dd, &sd, &target, NULL );
        say( "target", hr );
        if( hr == DD_OK ) {
            /* An 8 bpp render target must have a palette, or CreateDevice
             * returns DDERR_NOPALETTEATTACHED. */
            memset( pal, 0, sizeof( pal ) );
            for( i = 0; i < 256; ++i ) {
                pal[i].peRed = pal[i].peGreen = pal[i].peBlue = (BYTE)i;
            }
            hr = dd->lpVtbl->CreatePalette( dd, DDPCAPS_8BIT | DDPCAPS_ALLOW256, pal, &palette, NULL );
            say( "palette", hr );
            if( hr == DD_OK ) {
                say( "setpalette", target->lpVtbl->SetPalette( target, palette ) );
            }
            memset( &sd, 0, sizeof( sd ) );
            sd.dwSize = sizeof( sd );
            say( "getdesc", target->lpVtbl->GetSurfaceDesc( target, &sd ) );
            fprintf( out, "target caps %08lx pf flags %08lx bits %lu pitch %ld %lux%lu\n",
                     sd.ddsCaps.dwCaps, sd.ddpfPixelFormat.dwFlags, sd.ddpfPixelFormat.dwRGBBitCount,
                     sd.lPitch, sd.dwWidth, sd.dwHeight );
            fflush( out );
            memset( &fx, 0, sizeof( fx ) );
            fx.dwSize = sizeof( fx );
            fx.dwFillColor = 0;
            say( "clear", target->lpVtbl->Blt( target, NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx ) );
            hr = d3d->lpVtbl->CreateDevice( d3d, &m8_iid_hal, target, &dev );
            say( "createdevice", hr );
            /* The DLL is not a module of this process, and loading it here hangs.
             * Its sections are shared at a fixed base, so its counters can be read
             * by address, given on the command line as the second word. */
            stat = NULL;
            if( sscanf( cmd, "%*s %lx", &stat_addr ) == 1 &&
                !IsBadReadPtr( (void *)stat_addr, 8 * sizeof( DWORD ) ) ) {
                stat = (DWORD *)stat_addr;
            }
            if( stat != NULL ) {
                fprintf( out, "hal ctx %lu/%lu scene %lu state %lu prim %lu (%lu) dds %08lx z %08lx\n",
                         stat[0], stat[1], stat[2], stat[3], stat[4], stat[5], stat[6], stat[7] );
                fflush( out );
            } else {
                fprintf( out, "hal stat not found\n" );
            }
            if( hr == DD_OK ) {
                hr = d3d->lpVtbl->CreateViewport( d3d, &vp, NULL );
                say( "viewport", hr );
                if( hr == DD_OK ) {
                    memset( &v, 0, sizeof( v ) );
                    v.dwSize = sizeof( v );
                    v.dwWidth = 256;
                    v.dwHeight = 128;
                    v.dvClipX = -1.0f;
                    v.dvClipY = 1.0f;
                    v.dvClipWidth = 2.0f;
                    v.dvClipHeight = 2.0f;
                    v.dvMaxZ = 1.0f;
                    say( "addviewport", dev->lpVtbl->AddViewport( dev, vp ) );
                    say( "setviewport", vp->lpVtbl->SetViewport2( vp, &v ) );
                    say( "current", dev->lpVtbl->SetCurrentViewport( dev, vp ) );
                }
                memset( tri, 0, sizeof( tri ) );
                for( i = 0; i < 3; ++i ) {
                    tri[i].sz = 0.5f;
                    tri[i].rhw = 1.0f;
                    tri[i].color = RGBA_MAKE( 255, 255, 255, 255 );
                }
                tri[0].sx = 128.0f; tri[0].sy = 10.0f;
                tri[1].sx = 240.0f; tri[1].sy = 118.0f;
                tri[2].sx = 16.0f;  tri[2].sy = 118.0f;
                say( "beginscene", dev->lpVtbl->BeginScene( dev ) );
                say( "draw", dev->lpVtbl->DrawPrimitive( dev, D3DPT_TRIANGLELIST, D3DVT_TLVERTEX,
                                                         tri, 3, D3DDP_WAIT ) );
                say( "endscene", dev->lpVtbl->EndScene( dev ) );
                if( stat != NULL ) {
                    fprintf( out, "after draw: scene %lu state %lu prim %lu (%lu) drawn %lu culled %lu outside %lu refused %lu\n",
                             stat[2], stat[3], stat[4], stat[5], stat[8], stat[9], stat[10], stat[11] );
                }
                /* The target as the card holds it, one character per 8x8 cell's
                 * centre pixel; then on the screen, so it can be seen. */
                memset( &sd, 0, sizeof( sd ) );
                sd.dwSize = sizeof( sd );
                hr = target->lpVtbl->Lock( target, NULL, &sd, DDLOCK_WAIT | DDLOCK_READONLY, NULL );
                say( "lock", hr );
                if( hr == DD_OK ) {
                    int x;
                    int y;
                    unsigned long lit = 0;

                    for( y = 0; y < 128; ++y ) {
                        for( x = 0; x < 256; ++x ) {
                            lit += ((BYTE *)sd.lpSurface)[y * sd.lPitch + x] != 0;
                        }
                    }
                    fprintf( out, "pixels lit %lu of 32768\n", lit );
                    for( y = 4; y < 128; y += 8 ) {
                        for( x = 4; x < 256; x += 8 ) {
                            fputc( ((BYTE *)sd.lpSurface)[y * sd.lPitch + x] ? '#' : '.', out );
                        }
                        fputc( '\n', out );
                    }
                    target->lpVtbl->Unlock( target, NULL );
                }
                memset( &sd, 0, sizeof( sd ) );
                sd.dwSize = sizeof( sd );
                sd.dwFlags = DDSD_CAPS;
                sd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
                if( dd->lpVtbl->CreateSurface( dd, &sd, &primary, NULL ) == DD_OK ) {
                    RECT to;

                    SetRect( &to, 40, 40, 296, 168 );
                    say( "show", primary->lpVtbl->Blt( primary, &to, target, NULL, DDBLT_WAIT, NULL ) );
                    Sleep( 5000 );
                    primary->lpVtbl->Release( primary );
                }
                dev->lpVtbl->Release( dev );
            }
            target->lpVtbl->Release( target );
        }
        d3d->lpVtbl->Release( d3d );
    }
    dd->lpVtbl->Release( dd );
    fprintf( out, "\n" );
    fclose( out );
    return( 0 );
}
