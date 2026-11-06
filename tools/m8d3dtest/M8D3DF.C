/* M8D3DF.EXE - Direct3D at 16 bpp on stock DirectX, the same tests every run: a
 * full-screen 640x480 flip chain, a black clear, flat triangles on the back buffer,
 * Flip. A card before each test says which it is (n/total) and what to expect. At the
 * end of a test one frame is locked, which puts the triangles in RAM, and its lit
 * pixels counted. Results append to C:\M8D3DF.TXT, with the time inside Flip from the
 * driver's counters.
 *
 *   M8D3DF <label> [seconds per test, default 3] [norm|high|rt] [hal|again|grab]
 *
 * high is HIGH_PRIORITY_CLASS, which a game could use; rt holds off everything but
 * interrupts, and starves sound and network. hal leaves out the slow software tests.
 * Spin lines show what other threads take with no DirectX call being made.
 *
 * grab: the HAL tests, and at the end of each both pages as the card holds them, every second
 * pixel, with the palette, in C:\M8D3DF.GRB: the bench's lit count comes from RAM and never sees
 * the card. About 10 s a test. tools/m8d3dtest/m8grab_png.py makes pictures of it.
 *
 * Build: wcl386 -q -bt=nt -l=nt_win -ox -i=<watcom>h -i=<watcom>h\nt -i=<watcom>h\nt\directx
 *        -fe=M8D3DF.EXE M8D3DF.C ddraw.lib
 */

#define INITGUID
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <ddraw.h>
#include <d3d.h>

static const GUID m8_iid_d3d2 = { 0x6aae1ec1, 0x662a, 0x11d0, { 0x88, 0x9d, 0x00, 0xaa, 0x00, 0xbb, 0xb7, 0x6a } };
static const GUID m8_iid_hal = { 0x84e63de0, 0x46aa, 0x11cf, { 0x81, 0x6f, 0x00, 0x00, 0xc0, 0x20, 0x15, 0x6e } };
static const GUID m8_iid_rgb = { 0xa4665c60, 0x2673, 0x11cf, { 0xa3, 0x1a, 0x00, 0xaa, 0x00, 0xb9, 0x33, 0x56 } };

#define SCR_W       640
#define SCR_H       480
#define MAX_TRIS    64
#define M8_DLL_BASE 0xB0C00000UL
#define SPIN_SECONDS 2.0
#define SPIN_GAP_MS 0.5                 /* more than the clock read and the timer tick */
#define SLOW_FRAME_MS 16.67

enum { MOVING = 1, ONE_COLOUR = 2, TURNING = 4, CORNERS = 8 };

typedef struct {
    const char  *name;
    const char  *what;
    const GUID  *device;
    int         n;
    int         size;
    int         flags;
} TEST;

static const TEST tests[] = {
    { "hal_tiny",   "16 triangles, 8 px, moving",       &m8_iid_hal, 16,   8, MOVING },
    { "hal_small",  "16 triangles, 32 px, moving",      &m8_iid_hal, 16,  32, MOVING },
    { "hal_large",  "16 triangles, 100 px, moving",     &m8_iid_hal, 16, 100, MOVING },
    { "hal_static", "16 triangles, 8 px, not moving",   &m8_iid_hal, 16,   8, 0 },
    { "hal_colour", "16 triangles, 8 px, one colour",   &m8_iid_hal, 16,   8, MOVING | ONE_COLOUR },
    { "hal_many",   "64 triangles, 8 px, moving",       &m8_iid_hal, 64,   8, MOVING },
    { "hal_turn32", "16 triangles, 32 px, turning",     &m8_iid_hal, 16,  32, MOVING | TURNING },
    { "hal_turn100", "16 triangles, 100 px, turning",   &m8_iid_hal, 16, 100, MOVING | TURNING },
    { "hal_grad",   "16 triangles, 100 px, 3 colours",  &m8_iid_hal, 16, 100, MOVING | CORNERS },
    { "rgb_tiny",  "software: 16 triangles, 8 px",     &m8_iid_rgb, 16,   8, MOVING },
    { "rgb_small",  "software: 16 triangles, 32 px",    &m8_iid_rgb, 16,  32, MOVING },
    { "rgb_large",  "software: 16 triangles, 100 px",   &m8_iid_rgb, 16, 100, MOVING }
};
#define TEST_COUNT  (sizeof( tests ) / sizeof( tests[0] ))
#define HAL_TESTS   9

static FILE                 *out;
static LPDIRECTDRAW         dd;
static LPDIRECT3D2          d3d;
static LPDIRECTDRAWSURFACE  prim;
static LPDIRECTDRAWSURFACE  back;
static D3DTLVERTEX          tri[MAX_TRIS * 3];
static double               qpf;
static const DWORD          *stat;
static int                  grab;
static FILE                 *grab_out;

#define G_STAT      0x9AE8
#define G_EXT_FIFO  0x9AEE
#define G_FRGD_MIX  0xBAE8
#define G_WRT_MASK  0xAAE8
#define G_RD_MASK   0xAEE8
#define G_DP_CONFIG 0xCEEE
#define G_DP_PLAIN  0x2211
#define G_CUR_X     0x86E8
#define G_CUR_Y     0x82E8
#define G_MAJ       0x96E8
#define G_MIN       0xBEE8
#define G_PIX_TRANS 0xE2E8
#define G_PAGE1_Y   512
#define G_STEP      2

static void g_idle( void )
{
    long n;

    for( n = 0; n < 200000L; ++n ) {
        if( inpw( G_EXT_FIFO ) == 0 && !(inpw( G_STAT ) & 0x0200) ) {
            return;
        }
    }
}

static int g_pixel( int x, int y )
{
    long    n;
    int     v;

    g_idle();
    outpw( G_CUR_X, x );
    outpw( G_CUR_Y, y );
    outpw( G_MAJ, 1 );
    outpw( G_MIN, 0 );
    g_idle();
    outpw( 0x9AE8, 0x53B0 );
    for( n = 0; n < 200000L && !(inpw( G_STAT ) & 0x0100); ++n ) {
    }
    v = inpw( G_PIX_TRANS ) & 0xFF;
    g_idle();
    return( v );
}

/* Behind the driver's back: it sets the engine up afresh for each frame. */
static void g_dump( const char *name )
{
    char    head[16];
    int     page;
    int     x;
    int     y;
    int     i;

    if( grab_out == NULL ) {
        return;
    }
    g_idle();
    outpw( G_WRT_MASK, 0x00FF );
    outpw( G_RD_MASK, 0x00FF );
    outpw( G_DP_CONFIG, G_DP_PLAIN );
    outpw( G_FRGD_MIX, 0x0047 );
    memset( head, 0, sizeof( head ) );
    strncpy( head, name, sizeof( head ) - 1 );
    fwrite( head, 1, sizeof( head ), grab_out );
    outp( 0x2EB, 0 );
    for( i = 0; i < 768; ++i ) {
        fputc( inp( 0x2ED ), grab_out );
    }
    for( page = 0; page < 2; ++page ) {
        for( y = 0; y < SCR_H; y += G_STEP ) {
            for( x = 0; x < SCR_W; x += G_STEP ) {
                fputc( g_pixel( x, y + page * G_PAGE1_Y ), grab_out );
            }
        }
    }
    g_idle();
    outpw( G_FRGD_MIX, 0x0027 );
    fflush( grab_out );
}

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

/* The time this thread did not run: a tight loop on the clock, its long gaps taken. */
static void spin( const char *where )
{
    double          t0 = now();
    double          a = t0;
    double          b;
    double          gap;
    double          lost = 0.0;
    double          worst = 0.0;
    double          gap_ticks = SPIN_GAP_MS * qpf / 1000.0;
    unsigned long   gaps = 0;

    do {
        b = now();
        gap = b - a;
        if( gap > gap_ticks ) {
            lost += gap;
            ++gaps;
            if( gap > worst ) {
                worst = gap;
            }
        }
        a = b;
    } while( (b - t0) / qpf < SPIN_SECONDS );
    fprintf( out, "spin %-11s lost %5.1f %%  %5lu gaps  worst %6.2f ms\n", where, 100.0 * lost / (b - t0), gaps,
             1000.0 * worst / qpf );
    fflush( out );
}

/* The driver's counters, by reading its export table where the DLL is based: it is
 * not a module of this process, and loading it here hangs. */
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

/* Clockwise triangles of `size` pixels, in fixed places plus the frame's drift. */
static void scene( const TEST *t, int frame )
{
    int i;
    int j;
    int x;
    int y;
    int f = (t->flags & MOVING) ? frame : 0;

    for( i = 0; i < t->n; ++i ) {
        x = (i * 83 + f * 3) % (SCR_W - t->size - 1);
        y = (i * 47 + f * 2) % (SCR_H - t->size - 1);
        tri[3 * i].sx = (float)x;
        tri[3 * i].sy = (float)y;
        tri[3 * i + 1].sx = (float)(x + t->size);
        tri[3 * i + 1].sy = (float)(y + t->size);
        tri[3 * i + 2].sx = (float)x;
        tri[3 * i + 2].sy = (float)(y + t->size);
        /* Shapes that keep changing, so a shape cache gets few repeats: one corner of each triangle
         * walks to and fro, on a long cycle at the 100 px test and the shorter one 32 px allows. The
         * corners stay inside the box the fixed triangles use, and the winding stays clockwise. */
        if( t->flags & TURNING ) {
            int k = (i * 37 + frame * 3) % 512;
            int d = (k < 256 ? k : 511 - k) * t->size / 512;

            tri[3 * i].sx = (float)(x + d);
            tri[3 * i + 1].sy = (float)(y + t->size - d);
        }
        for( j = 0; j < 3; ++j ) {
            tri[3 * i + j].sz = 0.5f;
            tri[3 * i + j].rhw = 1.0f;
            tri[3 * i + j].color = (t->flags & ONE_COLOUR) ? RGBA_MAKE( 255, 192, 0, 255 ) :
                                   RGBA_MAKE( i * 16 & 255, 255 - i * 16 & 255, i * 40 & 255, 255 );
        }
        if( t->flags & CORNERS ) {
            tri[3 * i].color = RGBA_MAKE( 255, i * 16 & 255, 0, 255 );
            tri[3 * i + 1].color = RGBA_MAKE( 0, 255, i * 16 & 255, 255 );
            tri[3 * i + 2].color = RGBA_MAKE( i * 16 & 255, 0, 255, 255 );
        }
    }
}

static void title( int no, const TEST *t )
{
    HDC     dc;
    HFONT   f;
    HFONT   of;
    char    s[64];
    DDBLTFX fx;
    int     h = SCR_H / 16;

    memset( &fx, 0, sizeof( fx ) );
    fx.dwSize = sizeof( fx );
    back->lpVtbl->Blt( back, NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx );
    prim->lpVtbl->Flip( prim, NULL, DDFLIP_WAIT );
    if( back->lpVtbl->Blt( back, NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx ) != DD_OK ||
        back->lpVtbl->GetDC( back, &dc ) != DD_OK ) {
        return;
    }
    f = CreateFont( h, 0, 0, 0, FW_BOLD, 0, 0, 0, ANSI_CHARSET, 0, 0, 0, 0, "Arial" );
    of = SelectObject( dc, f );
    SetBkMode( dc, TRANSPARENT );
    SetTextColor( dc, RGB( 255, 255, 0 ) );
    sprintf( s, "%d/%d  %s", no, (int)TEST_COUNT, t->name );
    TextOut( dc, h, SCR_H / 2 - 2 * h, s, strlen( s ) );
    TextOut( dc, h, SCR_H / 2, t->what, strlen( t->what ) );
    SelectObject( dc, of );
    DeleteObject( f );
    back->lpVtbl->ReleaseDC( back, dc );
    prim->lpVtbl->Flip( prim, NULL, DDFLIP_WAIT );
    Sleep( 1500 );
}

static void test( int no, const TEST *t, int seconds )
{
    LPDIRECT3DDEVICE2       dev = NULL;
    LPDIRECT3DVIEWPORT2     vp = NULL;
    D3DVIEWPORT2            v;
    DDBLTFX                 fx;
    DDSURFACEDESC           sd;
    HRESULT                 hr;
    double                  t0;
    double                  el;
    double                  a;
    double                  b;
    double                  part[4];            /* our scene, clear, scene calls, Flip */
    unsigned long           frames = 0;
    unsigned long           bad = 0;
    unsigned long           lit = 0;
    unsigned long           slow = 0;
    double                  frame0;
    double                  worst = 0.0;
    DWORD                   s[12];
    int                     x;
    int                     y;
    int                     i;

    title( no, t );
    hr = d3d->lpVtbl->CreateDevice( d3d, (GUID *)t->device, back, &dev );
    if( hr != DD_OK ) {
        fprintf( out, "%d/%d %-10s no device %08lx\n", no, (int)TEST_COUNT, t->name, (unsigned long)hr );
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
    dev->lpVtbl->SetRenderState( dev, D3DRENDERSTATE_SHADEMODE, (t->flags & CORNERS) ? D3DSHADE_GOURAUD : D3DSHADE_FLAT );
    dev->lpVtbl->SetRenderState( dev, D3DRENDERSTATE_CULLMODE, D3DCULL_NONE );
    memset( tri, 0, sizeof( tri ) );
    memset( &fx, 0, sizeof( fx ) );
    fx.dwSize = sizeof( fx );
    memset( s, 0, sizeof( s ) );
    if( stat != NULL ) {
        for( i = 0; i < 12; ++i ) {
            s[i] = stat[12 + i];
        }
    }
    for( i = 0; i < 4; ++i ) {
        part[i] = 0.0;
    }
    t0 = now();
    a = t0;
    do {
        frame0 = a;
        scene( t, (int)frames );
        b = now();
        part[0] += b - a;
        if( back->lpVtbl->Blt( back, NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx ) != DD_OK ) {
            ++bad;
        }
        a = now();
        part[1] += a - b;
        if( dev->lpVtbl->BeginScene( dev ) != D3D_OK ||
            dev->lpVtbl->DrawPrimitive( dev, D3DPT_TRIANGLELIST, D3DVT_TLVERTEX, tri, 3 * t->n, D3DDP_WAIT ) != D3D_OK ||
            dev->lpVtbl->EndScene( dev ) != D3D_OK ) {
            ++bad;
        }
        b = now();
        part[2] += b - a;
        if( prim->lpVtbl->Flip( prim, NULL, DDFLIP_WAIT ) != DD_OK ) {
            ++bad;
        }
        a = now();
        part[3] += a - b;
        ++frames;
        if( (a - frame0) * 1000.0 / qpf > SLOW_FRAME_MS ) {
            ++slow;
        }
        if( a - frame0 > worst ) {
            worst = a - frame0;
        }
        el = (a - t0) / qpf;
    } while( el < (double)seconds );
    if( stat != NULL ) {
        for( i = 0; i < 12; ++i ) {
            s[i] = stat[12 + i] - s[i];
        }
    }
    if( grab ) {
        g_dump( t->name );
    }
    back->lpVtbl->Blt( back, NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx );
    dev->lpVtbl->BeginScene( dev );
    dev->lpVtbl->DrawPrimitive( dev, D3DPT_TRIANGLELIST, D3DVT_TLVERTEX, tri, 3 * t->n, D3DDP_WAIT );
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
    fprintf( out, "%d/%d %-10s %7.1f fps %6.2f ms/frame  scene %5.2f clear %5.2f d3d %6.2f flipcall %6.2f", no, (int)TEST_COUNT, t->name,
             frames / el, 1000.0 * el / frames, 1000.0 * part[0] / qpf / frames, 1000.0 * part[1] / qpf / frames,
             1000.0 * part[2] / qpf / frames, 1000.0 * part[3] / qpf / frames );
    if( s[3] != 0 ) {
        fprintf( out, "  flip %6.2f (16-bit %6.2f, engine %6.2f)", 1000.0 * s[0] / qpf / s[3], 1000.0 * s[1] / qpf / s[3],
                 1000.0 * s[2] / qpf / s[3] );
        fprintf( out, "  page wait %5.2f  blt %5.2f calls %lu described %lu", 1000.0 * s[7] / qpf / s[3], 1000.0 * s[4] / qpf / s[3],
                 (unsigned long)s[5], (unsigned long)s[6] );
        fprintf( out, "  masks %lu built %lu  stamps %lu built %lu", (unsigned long)s[8], (unsigned long)s[9],
                 (unsigned long)s[10], (unsigned long)s[11] );
    }
    fprintf( out, "  bad %lu  lit %lu  over16.7ms %lu/%lu  worst %.1f\n", bad, lit, slow, frames, 1000.0 * worst / qpf );
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
    WNDCLASS            wc;
    HWND                wnd;
    DDSURFACEDESC       sd;
    DDSCAPS             caps;
    LARGE_INTEGER       f;
    char                label[32];
    char                prio[8];
    char                which[8];
    int                 count = (int)TEST_COUNT;
    int                 seconds = 3;
    int                 i;
    HRESULT             hr;

    out = fopen( "C:\\M8D3DF.TXT", "a" );
    if( out == NULL ) {
        return( 1 );
    }
    label[0] = '\0';
    strcpy( prio, "norm" );
    which[0] = '\0';
    sscanf( cmd, "%31s %d %7s %7s", label, &seconds, prio, which );
    QueryPerformanceFrequency( &f );
    qpf = (double)f.LowPart;
    if( strcmp( prio, "high" ) == 0 ) {
        SetPriorityClass( GetCurrentProcess(), HIGH_PRIORITY_CLASS );
    } else if( strcmp( prio, "rt" ) == 0 ) {
        SetPriorityClass( GetCurrentProcess(), REALTIME_PRIORITY_CLASS );
        SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL );
    }
    if( strcmp( which, "hal" ) == 0 || strcmp( which, "again" ) == 0 || strcmp( which, "grab" ) == 0 ) {
        count = HAL_TESTS;
    }
    if( strcmp( which, "grab" ) == 0 ) {
        grab = 1;
        grab_out = fopen( "C:\\M8D3DF.GRB", "wb" );
    }
    fprintf( out, "== M8D3DF %s  640x480x16  %d s a test  priority %s\n", label, seconds, prio );
    spin( "desktop" );
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
    stat = find_stat();
    fprintf( out, "counters %s\n", stat != NULL ? "found" : "not found" );
    say( "coop", dd->lpVtbl->SetCooperativeLevel( dd, wnd, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN ) );
    say( "setmode", dd->lpVtbl->SetDisplayMode( dd, SCR_W, SCR_H, 16 ) );
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
            spin( "fullscreen" );
            for( i = 0; i < count; ++i ) {
                test( i + 1, &tests[i], seconds );
            }
            /* The first test also carries the mode set's paging: the same test last shows
             * how much of its time is its own. */
            if( strcmp( which, "again" ) == 0 ) {
                test( 1, &tests[0], seconds );
            }
            spin( "after" );
            prim->lpVtbl->Release( prim );
        }
        d3d->lpVtbl->Release( d3d );
    }
    dd->lpVtbl->RestoreDisplayMode( dd );
    dd->lpVtbl->SetCooperativeLevel( dd, wnd, DDSCL_NORMAL );
    dd->lpVtbl->Release( dd );
    if( grab_out != NULL ) {
        fclose( grab_out );
    }
    fprintf( out, "\n" );
    fclose( out );
    DestroyWindow( wnd );
    return( 0 );
}
