/* M8DDB.EXE - DirectDraw timed the same way under any display driver, so
 * ours and ATI's (or Windows' software DirectDraw behind it) can be set side by
 * side, at 8 or 16 bpp, whatever the desktop is.
 *
 * Each test draws through DirectDraw onto the primary surface, inside a
 * topmost window, for a second in ten slices; the rate is the median slice's.
 * A fixed sequence of the same operation then draws into a cleared area, read
 * back through a lock and hashed: equal hashes mean equal pixels. A test the
 * driver or DirectDraw refuses prints its error instead of a rate.
 *
 *   M8DDB <label>      appends to C:\M8DDB.TXT and exits (about 15 s)
 *   M8DDB fs16 <label>   the same file: 640x480 at 16 bpp, full screen (as
 *                      dxdiag's third DirectDraw test), then the desktop back
 *   M8DDB modex <label>  the same file: 320x200 at 8 bpp, full screen, with
 *                      DDSCL_ALLOWMODEX (DirectDraw's own Mode X or a driver
 *                      mode), then the desktop back
 *
 * tools/m8_bench_compare.py sets two labels' results side by side.
 * Build: wcl386 -q -bt=nt -l=nt_win -ox -fe=M8DDB.EXE M8DDB.C advapi32.lib
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <io.h>
#include <ddraw.h>

#define WIN_W       640
#define WIN_H       480
#define RUN_MS      1000
#define SLICES      10
#define HASH_X      400         /* the area the fixed sequences draw into */
#define HASH_Y      240
#define HASH_W      200
#define HASH_H      200
#define TEST_COUNT  10

typedef HRESULT (WINAPI *DDCREATE)( GUID FAR *, LPDIRECTDRAW FAR *, IUnknown FAR * );
typedef HRESULT (*OP)( int i );

static HWND                 wnd;
static LPDIRECTDRAW         dd;
static LPDIRECTDRAWSURFACE  prim;
static LPDIRECTDRAWSURFACE  vid_s;      /* 64x64, video memory (NULL if none) */
static LPDIRECTDRAWSURFACE  vid_l;      /* 256x256, video memory */
static LPDIRECTDRAWSURFACE  sys_s;      /* 64x64, system memory */
static LPDIRECTDRAWSURFACE  sys_l;      /* 256x256, system memory */
static int                  bpp;        /* bytes a pixel on the primary */
static DWORD                pixmask;    /* all bits of a pixel */
static LARGE_INTEGER        freq;
static unsigned long        rnd = 12345;
static FILE                 *out;
static int                  test_no;

static int r( int n )
{
    rnd = rnd * 1103515245UL + 12345UL;
    return (int)((rnd >> 16) % (unsigned long)n);
}

static DWORD colour( void )
{
    return ((DWORD)r( 65536 ) | ((DWORD)r( 65536 ) << 16)) & pixmask;
}

static double now_ms( void )
{
    LARGE_INTEGER t;

    QueryPerformanceCounter( &t );
    return (double)t.QuadPart * 1000.0 / (double)freq.QuadPart;
}

static void flush_out( void )
{
    fflush( out );
    _commit( fileno( out ) );
}

static HRESULT fill( LPDIRECTDRAWSURFACE s, int x, int y, int w, int h, DWORD c )
{
    DDBLTFX fx;
    RECT    rc;

    memset( &fx, 0, sizeof( fx ) );
    fx.dwSize = sizeof( fx );
    fx.dwFillColor = c;
    SetRect( &rc, x, y, x + w, y + h );
    return( s->lpVtbl->Blt( s, &rc, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx ) );
}

static void clear( void )
{
    fill( prim, 0, 0, WIN_W, WIN_H, pixmask );
}

/* A surface w x h in the given memory, its pixels a fixed pattern. */
static LPDIRECTDRAWSURFACE surface( int w, int h, DWORD where, int seed )
{
    DDSURFACEDESC       sd;
    LPDIRECTDRAWSURFACE s;
    BYTE                *p;
    int                 x;
    int                 y;
    DWORD               c;

    memset( &sd, 0, sizeof( sd ) );
    sd.dwSize = sizeof( sd );
    sd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    sd.dwWidth = w;
    sd.dwHeight = h;
    sd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | where;
    if( dd->lpVtbl->CreateSurface( dd, &sd, &s, NULL ) != DD_OK ) {
        return( NULL );
    }
    memset( &sd, 0, sizeof( sd ) );
    sd.dwSize = sizeof( sd );
    if( s->lpVtbl->Lock( s, NULL, &sd, DDLOCK_WAIT, NULL ) == DD_OK ) {
        p = (BYTE *)sd.lpSurface;
        for( y = 0; y < h; ++y ) {
            for( x = 0; x < w; ++x ) {
                /* Rings and stripes; 0 at the corners for the colour key. */
                c = (DWORD)(((x ^ y) + seed) * 2654435761UL) >> 8;
                if( (x < 4 && y < 4) || (x * x + y * y) % 97 < 9 ) {
                    c = 0;
                }
                c &= pixmask;
                memcpy( p + y * sd.lPitch + x * bpp, &c, bpp );
            }
        }
        s->lpVtbl->Unlock( s, NULL );
    }
    return( s );
}

/* The same area through GDI, as 24-bit RGB: for a primary DirectDraw will
 * not lock. Hashes from the two readers do not compare with each other. */
static unsigned long hash_gdi( void )
{
    static BYTE         bits[HASH_W * HASH_H * 3];
    BITMAPINFOHEADER    bi;
    HDC                 dc = GetDC( NULL );
    HDC                 m = CreateCompatibleDC( dc );
    HBITMAP             b = CreateCompatibleBitmap( dc, HASH_W, HASH_H );
    HBITMAP             old = SelectObject( m, b );
    unsigned long       h = 2166136261UL;
    int                 i;

    BitBlt( m, 0, 0, HASH_W, HASH_H, dc, HASH_X, HASH_Y, SRCCOPY );
    SelectObject( m, old );
    memset( &bi, 0, sizeof( bi ) );
    bi.biSize = sizeof( bi );
    bi.biWidth = HASH_W;
    bi.biHeight = HASH_H;
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    GetDIBits( m, b, 0, HASH_H, bits, (BITMAPINFO *)&bi, DIB_RGB_COLORS );
    for( i = 0; i < HASH_W * HASH_H * 3; ++i ) {
        h = (h ^ bits[i]) * 16777619UL;
    }
    DeleteObject( b );
    DeleteDC( m );
    ReleaseDC( NULL, dc );
    return( h | 1 );        /* odd: read through GDI */
}

/* FNV-1a over the hash area, read through a lock of the primary. */
static unsigned long hash_area( void )
{
    DDSURFACEDESC   sd;
    RECT            rc;
    unsigned long   h = 2166136261UL;
    BYTE            *p;
    int             x;
    int             y;

    SetRect( &rc, HASH_X, HASH_Y, HASH_X + HASH_W, HASH_Y + HASH_H );
    memset( &sd, 0, sizeof( sd ) );
    sd.dwSize = sizeof( sd );
    if( prim->lpVtbl->Lock( prim, &rc, &sd, DDLOCK_WAIT | DDLOCK_READONLY, NULL ) != DD_OK ) {
        return( hash_gdi() );
    }
    for( y = 0; y < HASH_H; ++y ) {
        p = (BYTE *)sd.lpSurface + y * sd.lPitch;
        for( x = 0; x < HASH_W * bpp; ++x ) {
            h = (h ^ p[x]) * 16777619UL;
        }
    }
    prim->lpVtbl->Unlock( prim, sd.lpSurface );
    return( h & ~1UL );     /* even: read through a lock */
}

/* Our 16 bpp driver only: does the card show what is in its frame buffer
 * (ExtEscape 0x4D3B, as FB16CHK)? Empty under any other driver or depth. */
static const char *card_check( void )
{
    static char buf[48];
    DWORD       o[5];
    HDC         dc = GetDC( NULL );
    int         rc = ExtEscape( dc, 0x4D3B, 0, NULL, sizeof( o ), (LPSTR)o );

    ReleaseDC( NULL, dc );
    if( rc <= 0 ) {
        return( "" );
    }
    sprintf( buf, "  card %lu", o[0] );
    return( buf );
}

static int cmp_double( const void *a, const void *b )
{
    double x = *(const double *)a;
    double y = *(const double *)b;

    return( x < y ? -1 : x > y );
}

static void run( const char *name, OP op, int fixed, int needs )
{
    double  t0;
    double  t;
    double  rate[SLICES];
    double  ts = 0.0;
    double  med;
    long    n = 0;
    long    ns = 0;
    long    least = 0x7FFFFFFFL;
    int     k = 0;
    int     i;
    HRESULT rc;
    char    title[48];

    ++test_no;
    sprintf( title, "M8DDB %d/%d %s", test_no, TEST_COUNT, name );
    SetWindowText( wnd, title );
    fprintf( out, "started %d/%d %s\n", test_no, TEST_COUNT, name );
    flush_out();
    Sleep( 300 );
    if( !needs ) {
        fprintf( out, "%-16s unsupported: no surface  #%d\n", name, test_no );
        flush_out();
        return;
    }
    clear();
    rnd = 12345;
    rc = op( 0 );
    if( rc != DD_OK ) {
        fprintf( out, "%-16s unsupported: %08lx  #%d\n", name, (unsigned long)rc, test_no );
        flush_out();
        return;
    }
    t0 = now_ms();
    do {
        op( (int)n );
        ++n;
        t = now_ms() - t0;
        if( k < SLICES && t >= (double)RUN_MS * (k + 1) / SLICES ) {
            rate[k] = (n - ns) * 1000.0 / (t - ts);
            if( n - ns < least ) {
                least = n - ns;
            }
            ts = t;
            ns = n;
            ++k;
        }
    } while( t < RUN_MS );
    if( k == SLICES && least >= 4 ) {
        qsort( rate, SLICES, sizeof( rate[0] ), cmp_double );
        med = (rate[SLICES / 2 - 1] + rate[SLICES / 2]) / 2.0;
    } else {
        med = n * 1000.0 / t;
    }
    clear();
    rnd = 777;
    for( i = 0; i < fixed; ++i ) {
        op( i );
    }
    fprintf( out, "%-16s %10.2f ops/s %8.3f ms/op  hash %08lx  #%d  pad 0000  med %.2f%s\n", name,
             n * 1000.0 / t, t / n, hash_area(), test_no, med, card_check() );
    flush_out();
}

/* ---------------------------------------------------------------- the tests */

static HRESULT t_fill_small( int i )
{
    return( fill( prim, HASH_X + r( 168 ), HASH_Y + r( 168 ), 32, 32, colour() ) );
}

static HRESULT t_fill_large( int i )
{
    return( fill( prim, HASH_X - 200 + r( 8 ), HASH_Y - 100 + r( 8 ), 392, 292, colour() ) );
}

static HRESULT blt( LPDIRECTDRAWSURFACE s, int w, int h, int x, int y, DWORD flags )
{
    RECT    src;
    RECT    dst;

    SetRect( &src, 0, 0, w, h );
    SetRect( &dst, x, y, x + w, y + h );
    return( prim->lpVtbl->Blt( prim, &dst, s, &src, DDBLT_WAIT | flags, NULL ) );
}

static HRESULT t_vid_small( int i )
{
    return( blt( vid_s, 64, 64, HASH_X + r( 136 ), HASH_Y + r( 136 ), 0 ) );
}

static HRESULT t_vid_large( int i )
{
    return( blt( vid_l, 256, 256, HASH_X - 56 + r( 8 ), HASH_Y - 56 + r( 8 ), 0 ) );
}

static HRESULT t_sys_small( int i )
{
    return( blt( sys_s, 64, 64, HASH_X + r( 136 ), HASH_Y + r( 136 ), 0 ) );
}

static HRESULT t_sys_large( int i )
{
    return( blt( sys_l, 256, 256, HASH_X - 56 + r( 8 ), HASH_Y - 56 + r( 8 ), 0 ) );
}

/* A sprite: the source's colour 0 left as it was. */
static HRESULT t_sprite( int i )
{
    return( blt( vid_s, 64, 64, HASH_X + r( 136 ), HASH_Y + r( 136 ), DDBLT_KEYSRC ) );
}

/* A scroll: the screen copied onto itself, 8 rows up. */
static HRESULT t_scroll( int i )
{
    RECT    src;
    RECT    dst;

    if( i % 16 == 0 ) {
        fill( prim, HASH_X, HASH_Y + 192, 200, 8, colour() );
    }
    SetRect( &src, HASH_X, HASH_Y + 8, HASH_X + 200, HASH_Y + 200 );
    SetRect( &dst, HASH_X, HASH_Y, HASH_X + 200, HASH_Y + 192 );
    return( prim->lpVtbl->Blt( prim, &dst, prim, &src, DDBLT_WAIT, NULL ) );
}

/* A program drawing into the screen itself: lock, write, unlock. */
static HRESULT lock_write( int x, int y, int w, int h )
{
    DDSURFACEDESC   sd;
    RECT            rc;
    HRESULT         res;
    DWORD           c = colour();
    BYTE            *p;
    int             j;
    int             k;

    SetRect( &rc, x, y, x + w, y + h );
    memset( &sd, 0, sizeof( sd ) );
    sd.dwSize = sizeof( sd );
    res = prim->lpVtbl->Lock( prim, &rc, &sd, DDLOCK_WAIT | DDLOCK_WRITEONLY, NULL );
    if( res != DD_OK ) {
        return( res );
    }
    for( j = 0; j < h; ++j ) {
        p = (BYTE *)sd.lpSurface + j * sd.lPitch;
        for( k = 0; k < w; ++k ) {
            memcpy( p + k * bpp, &c, bpp );
            c = (c * 5 + 1) & pixmask;
        }
    }
    return( prim->lpVtbl->Unlock( prim, sd.lpSurface ) );
}

static HRESULT t_lock_small( int i )
{
    return( lock_write( HASH_X + r( 136 ), HASH_Y + r( 136 ), 64, 64 ) );
}

static HRESULT t_lock_large( int i )
{
    return( lock_write( HASH_X - 200 + r( 8 ), HASH_Y - 100 + r( 8 ), 392, 292 ) );
}

/* ---------------------------------------------------------------- Mode X */

static int  mx_w = 320;
static int  mx_h = 200;
static int  mx_bpp = 8;

static HRESULT t_mx_fill( int i )
{
    return( fill( prim, 0, 0, mx_w, mx_h, colour() ) );
}

/* A software renderer's frame: every byte of the screen written through a lock. */
static HRESULT t_mx_frame( int i )
{
    DDSURFACEDESC   sd;
    HRESULT         res;
    BYTE            *p;
    BYTE            c = (BYTE)r( 256 );
    int             y;
    int             x;

    memset( &sd, 0, sizeof( sd ) );
    sd.dwSize = sizeof( sd );
    res = prim->lpVtbl->Lock( prim, NULL, &sd, DDLOCK_WAIT | DDLOCK_WRITEONLY, NULL );
    if( res != DD_OK ) {
        return( res );
    }
    for( y = 0; y < mx_h; ++y ) {
        p = (BYTE *)sd.lpSurface + y * sd.lPitch;
        if( bpp == 2 ) {
            for( x = 0; x < mx_w; ++x ) {
                ((WORD *)p)[x] = (WORD)((c << 8) + x * 33 + y * 2049);
            }
        } else {
            for( x = 0; x < mx_w; ++x ) {
                p[x] = (BYTE)(c + x + y);
            }
        }
    }
    return( prim->lpVtbl->Unlock( prim, sd.lpSurface ) );
}

/* dxdiag's full-screen test as DirectDraw emulates its flip: every pixel of
 * the screen written through one lock, all black but a 32x32 white box that
 * moves 4 pixels a frame. */
static HRESULT t_mx_box( int i )
{
    DDSURFACEDESC   sd;
    HRESULT         res;
    BYTE            *p;
    int             bx = 16 + (i * 4) % (mx_w - 64);
    int             by = 16 + (i * 3) % (mx_h - 64);
    int             y;

    memset( &sd, 0, sizeof( sd ) );
    sd.dwSize = sizeof( sd );
    res = prim->lpVtbl->Lock( prim, NULL, &sd, DDLOCK_WAIT | DDLOCK_WRITEONLY, NULL );
    if( res != DD_OK ) {
        return( res );
    }
    for( y = 0; y < mx_h; ++y ) {
        p = (BYTE *)sd.lpSurface + y * sd.lPitch;
        memset( p, 0, mx_w * bpp );
        if( y >= by && y < by + 32 ) {
            memset( p + bx * bpp, 0xFF, 32 * bpp );
        }
    }
    return( prim->lpVtbl->Unlock( prim, sd.lpSurface ) );
}

/* mx_box's frame with nothing moving: what finding no change costs. */
static HRESULT t_mx_same( int i )
{
    return( t_mx_box( 0 ) );
}

/* A lock of the whole screen and its unlock, nothing written: the fixed cost. */
static HRESULT t_mx_lock( int i )
{
    DDSURFACEDESC   sd;
    HRESULT         res;

    memset( &sd, 0, sizeof( sd ) );
    sd.dwSize = sizeof( sd );
    res = prim->lpVtbl->Lock( prim, NULL, &sd, DDLOCK_WAIT, NULL );
    if( res != DD_OK ) {
        return( res );
    }
    return( prim->lpVtbl->Unlock( prim, sd.lpSurface ) );
}

/* FNV-1a over the whole Mode X screen, read through a lock. */
static unsigned long mx_hash( void )
{
    DDSURFACEDESC   sd;
    unsigned long   h = 2166136261UL;
    BYTE            *p;
    int             x;
    int             y;

    memset( &sd, 0, sizeof( sd ) );
    sd.dwSize = sizeof( sd );
    if( prim->lpVtbl->Lock( prim, NULL, &sd, DDLOCK_WAIT | DDLOCK_READONLY, NULL ) != DD_OK ) {
        return( 0 );
    }
    for( y = 0; y < mx_h; ++y ) {
        p = (BYTE *)sd.lpSurface + y * sd.lPitch;
        for( x = 0; x < mx_w * bpp; ++x ) {
            h = (h ^ p[x]) * 16777619UL;
        }
    }
    prim->lpVtbl->Unlock( prim, sd.lpSurface );
    return( h );
}

static void mx_run( const char *name, OP op, int fixed )
{
    double  t0;
    double  t;
    long    n = 0;
    int     i;
    HRESULT rc;

    ++test_no;
    fprintf( out, "started %d %s\n", test_no, name );
    flush_out();
    rnd = 12345;
    rc = op( 0 );
    if( rc != DD_OK ) {
        fprintf( out, "%-16s unsupported: %08lx  #%d\n", name, (unsigned long)rc, test_no );
        flush_out();
        return;
    }
    t0 = now_ms();
    do {
        op( (int)n );
        ++n;
        t = now_ms() - t0;
    } while( t < RUN_MS );
    rnd = 777;
    for( i = 0; i < fixed; ++i ) {
        op( i );
    }
    fprintf( out, "%-16s %10.2f ops/s %8.3f ms/op  hash %08lx  #%d  pad 0000  med %.2f%s\n", name,
             n * 1000.0 / t, t / n, mx_hash(), test_no, n * 1000.0 / t, card_check() );
    flush_out();
}

/* Every mode DirectDraw offers, with Mode X flagged. */
static HRESULT WINAPI mx_mode( LPDDSURFACEDESC d, LPVOID ctx )
{
    fprintf( out, "mode %lux%lux%lu%s\n", d->dwWidth, d->dwHeight,
             d->ddpfPixelFormat.dwRGBBitCount,
             (d->ddsCaps.dwCaps & DDSCAPS_MODEX) ? " modex" : "" );
    return( DDENUMRET_OK );
}

static int modex( const char *label )
{
    DDSURFACEDESC   sd;
    PALETTEENTRY    pe[256];
    LPDIRECTDRAWPALETTE pal;
    HRESULT         res;
    int             i;

    fprintf( out, "== %s  fullscreen  %dx%dx%d  %d ms  directdraw\n", label, mx_w, mx_h, mx_bpp,
             RUN_MS );
    res = dd->lpVtbl->SetCooperativeLevel( dd, wnd, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN |
                                           DDSCL_ALLOWMODEX );
    fprintf( out, "coop %08lx\n", (unsigned long)res );
    dd->lpVtbl->EnumDisplayModes( dd, 0, NULL, NULL, mx_mode );
    flush_out();
    res = dd->lpVtbl->SetDisplayMode( dd, mx_w, mx_h, mx_bpp );
    fprintf( out, "setmode %08lx\n", (unsigned long)res );
    flush_out();
    if( res != DD_OK ) {
        return( 1 );
    }
    memset( &sd, 0, sizeof( sd ) );
    sd.dwSize = sizeof( sd );
    sd.dwFlags = DDSD_CAPS;
    sd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    res = dd->lpVtbl->CreateSurface( dd, &sd, &prim, NULL );
    fprintf( out, "primary %08lx\n", (unsigned long)res );
    flush_out();
    if( res == DD_OK ) {
        memset( &sd, 0, sizeof( sd ) );
        sd.dwSize = sizeof( sd );
        fprintf( out, "getdesc\n" );
        flush_out();
        res = prim->lpVtbl->GetSurfaceDesc( prim, &sd );
        fprintf( out, "surface %08lx caps %08lx  pitch %ld  %lux%lu  mem %08lx\n", (unsigned long)res,
                 sd.ddsCaps.dwCaps, sd.lPitch, sd.dwWidth, sd.dwHeight, (unsigned long)sd.lpSurface );
        flush_out();
        for( i = 0; i < 256; ++i ) {
            pe[i].peRed = (BYTE)i;
            pe[i].peGreen = (BYTE)(255 - i);
            pe[i].peBlue = (BYTE)(i * 3);
            pe[i].peFlags = 0;
        }
        /* A label starting "nopal" leaves the palette alone, to separate a
         * palette fault from a drawing one. */
        res = strncmp( label, "nopal", 5 ) == 0 ? DDERR_GENERIC :
              dd->lpVtbl->CreatePalette( dd, DDPCAPS_8BIT | DDPCAPS_ALLOW256, pe, &pal, NULL );
        fprintf( out, "palette %08lx\n", (unsigned long)res );
        flush_out();
        if( res == DD_OK ) {
            res = prim->lpVtbl->SetPalette( prim, pal );
            fprintf( out, "setpalette %08lx\n", (unsigned long)res );
            flush_out();
        }
        bpp = mx_bpp / 8;
        pixmask = bpp == 2 ? 0xFFFF : 0xFF;
        mx_run( "mx_fill", t_mx_fill, 4 );
        mx_run( "mx_frame", t_mx_frame, 2 );
        mx_run( "mx_box", t_mx_box, 4 );
        mx_run( "mx_same", t_mx_same, 2 );
        mx_run( "mx_lock", t_mx_lock, 2 );
        prim->lpVtbl->Release( prim );
    }
    res = dd->lpVtbl->RestoreDisplayMode( dd );
    fprintf( out, "restore %08lx\n", (unsigned long)res );
    dd->lpVtbl->SetCooperativeLevel( dd, wnd, DDSCL_NORMAL );
    fprintf( out, "\n" );
    flush_out();
    return( 0 );
}

int PASCAL WinMain( HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show )
{
    WNDCLASS        wc;
    SYSTEMTIME      st;
    HMODULE         lib;
    DDCREATE        create;
    DDSURFACEDESC   sd;
    DDCOLORKEY      key;
    HRESULT         res;
    HDC             dc;
    char            drv[64] = "?";
    int             i;

    QueryPerformanceFrequency( &freq );
    memset( &wc, 0, sizeof( wc ) );
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = inst;
    wc.hbrBackground = (HBRUSH)GetStockObject( WHITE_BRUSH );
    wc.lpszClassName = "M8DDBench";
    RegisterClass( &wc );
    wnd = CreateWindowEx( WS_EX_TOPMOST, "M8DDBench", "M8DDBENCH", WS_POPUP | WS_VISIBLE, 0, 0,
                          WIN_W, WIN_H, NULL, NULL, inst, NULL );
    UpdateWindow( wnd );
    out = fopen( "C:\\M8DDB.TXT", "a" );
    if( out == NULL ) {
        return 1;
    }
    GetLocalTime( &st );
    {
        HKEY    k;
        DWORD   n = sizeof( drv );

        if( RegOpenKeyEx( HKEY_LOCAL_MACHINE,
                          "System\\CurrentControlSet\\Services\\Class\\Display\\0000\\DEFAULT",
                          0, KEY_READ, &k ) == ERROR_SUCCESS ) {
            RegQueryValueEx( k, "drv", NULL, NULL, (LPBYTE)drv, &n );
            RegCloseKey( k );
        }
    }
    dc = GetDC( wnd );
    fprintf( out, "== %s  driver %s  %dx%dx%d  %04d-%02d-%02d %02d:%02d  %d ms  directdraw\n",
             cmd[0] ? cmd : "-", drv, GetDeviceCaps( dc, HORZRES ), GetDeviceCaps( dc, VERTRES ),
             GetDeviceCaps( dc, BITSPIXEL ), st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute,
             RUN_MS );
    ReleaseDC( wnd, dc );
    flush_out();

    lib = LoadLibrary( "DDRAW.DLL" );
    create = lib == NULL ? NULL : (DDCREATE)GetProcAddress( lib, "DirectDrawCreate" );
    res = create == NULL ? DDERR_GENERIC : create( NULL, &dd, NULL );
    if( res != DD_OK ) {
        fprintf( out, "no DirectDraw: %08lx\n\n", (unsigned long)res );
        fclose( out );
        return 1;
    }
    {
        DDCAPS  hw;
        DDCAPS  hel;

        memset( &hw, 0, sizeof( hw ) );
        memset( &hel, 0, sizeof( hel ) );
        hw.dwSize = sizeof( hw );
        hel.dwSize = sizeof( hel );
        dd->lpVtbl->GetCaps( dd, &hw, &hel );
        fprintf( out, "caps  hw %08lx  ckey %08lx  vidmem %lu  %s\n", hw.dwCaps, hw.dwCKeyCaps,
                 hw.dwVidMemTotal, (hw.dwCaps & DDCAPS_NOHARDWARE) ? "NO HAL" : "HAL" );
    }
    if( strncmp( cmd, "fs16 ", 5 ) == 0 ) {
        mx_w = 640;
        mx_h = 480;
        mx_bpp = 16;
        i = modex( cmd + 5 );
        fclose( out );
        dd->lpVtbl->Release( dd );
        DestroyWindow( wnd );
        return( i );
    }
    if( strncmp( cmd, "modex ", 6 ) == 0 ) {
        i = modex( cmd + 6 );
        fclose( out );
        dd->lpVtbl->Release( dd );
        DestroyWindow( wnd );
        return( i );
    }
    dd->lpVtbl->SetCooperativeLevel( dd, wnd, DDSCL_NORMAL );
    memset( &sd, 0, sizeof( sd ) );
    sd.dwSize = sizeof( sd );
    sd.dwFlags = DDSD_CAPS;
    sd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    res = dd->lpVtbl->CreateSurface( dd, &sd, &prim, NULL );
    if( res != DD_OK ) {
        fprintf( out, "no primary: %08lx\n\n", (unsigned long)res );
        fclose( out );
        return 1;
    }
    memset( &sd, 0, sizeof( sd ) );
    sd.dwSize = sizeof( sd );
    prim->lpVtbl->GetSurfaceDesc( prim, &sd );
    bpp = (int)(sd.ddpfPixelFormat.dwRGBBitCount / 8);
    pixmask = bpp >= 4 ? 0xFFFFFFFFUL : (1UL << (bpp * 8)) - 1;

    vid_s = surface( 64, 64, DDSCAPS_VIDEOMEMORY, 1 );
    vid_l = surface( 256, 256, DDSCAPS_VIDEOMEMORY, 2 );
    sys_s = surface( 64, 64, DDSCAPS_SYSTEMMEMORY, 1 );
    sys_l = surface( 256, 256, DDSCAPS_SYSTEMMEMORY, 2 );
    if( vid_s != NULL ) {
        key.dwColorSpaceLowValue = key.dwColorSpaceHighValue = 0;
        vid_s->lpVtbl->SetColorKey( vid_s, DDCKEY_SRCBLT, &key );
    }
    fprintf( out, "surfaces  vid %s/%s  sys %s/%s  %d bytes a pixel\n", vid_s ? "yes" : "no",
             vid_l ? "yes" : "no", sys_s ? "yes" : "no", sys_l ? "yes" : "no", bpp );
    flush_out();

    run( "dd_fill_small", t_fill_small, 40, 1 );
    run( "dd_fill_large", t_fill_large, 3, 1 );
    run( "dd_vid_small", t_vid_small, 30, vid_s != NULL );
    run( "dd_vid_large", t_vid_large, 3, vid_l != NULL );
    run( "dd_sys_small", t_sys_small, 30, sys_s != NULL );
    run( "dd_sys_large", t_sys_large, 3, sys_l != NULL );
    run( "dd_sprite", t_sprite, 30, vid_s != NULL );
    run( "dd_scroll", t_scroll, 24, 1 );
    run( "dd_lock_small", t_lock_small, 20, 1 );
    run( "dd_lock_large", t_lock_large, 2, 1 );

    fprintf( out, "\n" );
    fclose( out );
    if( vid_s ) vid_s->lpVtbl->Release( vid_s );
    if( vid_l ) vid_l->lpVtbl->Release( vid_l );
    if( sys_s ) sys_s->lpVtbl->Release( sys_s );
    if( sys_l ) sys_l->lpVtbl->Release( sys_l );
    prim->lpVtbl->Release( prim );
    dd->lpVtbl->Release( dd );
    DestroyWindow( wnd );
    return 0;
}
