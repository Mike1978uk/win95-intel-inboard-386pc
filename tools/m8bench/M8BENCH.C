/* M8BENCH.EXE - every kind of drawing a Windows 95 display driver does, timed the same
 * way under any driver, so ours and ATI's ATIM8.DRV can be set side by side.
 *
 * Each test runs for a fixed second and counts operations (QueryPerformanceCounter: the
 * PIT, 0.84 us). A fixed sequence of the same operation then draws into a cleared area,
 * which is read back and hashed: equal hashes under two drivers mean equal pixels.
 *
 *   M8BENCH <label>      appends to C:\M8BENCH.TXT and exits (about 45 s)
 *
 * tools/m8_bench_compare.py sets two labels' results side by side.
 * Build: wcl386 -q -bt=nt -l=nt_win -ox -fe=M8BENCH.EXE M8BENCH.C advapi32.lib
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <io.h>
#include <conio.h>

#define WIN_W       640
#define WIN_H       480
#define RUN_MS      1000
#define HASH_X      400         /* the area the fixed sequences draw into */
#define HASH_Y      240
#define HASH_W      200
#define HASH_H      200

static HWND     wnd;
static HDC      dc;
static HDC      mem;            /* compatible memory DC for the bitmap tests */
static HBITMAP  bmp_card;       /* 71x96, a card-sized picture */
static HBITMAP  bmp_card2;
static HBITMAP  bmp_icon;       /* 32x32 */
static HBITMAP  bmp_large;      /* 300x200 */
static HBITMAP  bmp_mono;       /* 400x50, 1 bpp: the marquee's kind */
static BYTE     dib_bits[100 * 100];
static struct { BITMAPINFOHEADER h; RGBQUAD pal[256]; } dib;
static LARGE_INTEGER freq;
static unsigned long rnd = 12345;
static FILE     *out;
static int      run_ms = RUN_MS;

/* Our driver's profile build (drivers/mach8_display/src/m8.h, -Prof): time spent
 * inside the driver per entry point, read around each test. Keep in step with
 * M8_PF_TAB there. Other drivers do not answer the escape. */
#define M8_ESC_PROF 0x4D38
#define M8_PF_N     10
#pragma pack( push, 2 )
typedef struct {
    DWORD   calls;
    DWORD   ticks;
    DWORD   long_calls;
    DWORD   long_ticks;
} M8_PF_ENT;

typedef struct {
    WORD        version;
    WORD        n;
    WORD        stamp_ticks;
    WORD        hits;
    M8_PF_ENT   e[M8_PF_N];
    WORD        checked;
    WORD        bad;
} M8_PF_TAB;
#pragma pack( pop )

static int      prof;

static void prof_read( M8_PF_TAB *t, WORD clear )
{
    memset( t, 0, sizeof( *t ) );
    ExtEscape( dc, M8_ESC_PROF, sizeof( clear ), (LPCSTR)&clear, sizeof( *t ), (LPSTR)t );
}

/* PIT ticks to microseconds. */
#define PF_US( ticks )  ((ticks) * 1000000.0 / 1193182.0)

static int r( int n )
{
    rnd = rnd * 1103515245UL + 12345UL;
    return (int)((rnd >> 16) % (unsigned long)n);
}

static double now_ms( void )
{
    LARGE_INTEGER t;

    QueryPerformanceCounter( &t );
    return (double)t.QuadPart * 1000.0 / (double)freq.QuadPart;
}

/* One test: op(i) called with i = 0, 1, 2... for RUN_MS. */
typedef void (*OP)( int i );

static void clear( void )
{
    PatBlt( dc, 0, 0, WIN_W, WIN_H, WHITENESS );
}

/* FNV-1a over the hash area, read back as 8-bit indices. */
static unsigned long hash_area( void )
{
    static BYTE         bits[HASH_W * HASH_H];
    struct { BITMAPINFOHEADER h; RGBQUAD pal[256]; } bi;
    HDC                 m = CreateCompatibleDC( dc );
    HBITMAP             b = CreateCompatibleBitmap( dc, HASH_W, HASH_H );
    HBITMAP             old = SelectObject( m, b );
    unsigned long       h = 2166136261UL;
    int                 i;

    BitBlt( m, 0, 0, HASH_W, HASH_H, dc, HASH_X, HASH_Y, SRCCOPY );
    SelectObject( m, old );
    memset( &bi, 0, sizeof( bi ) );
    bi.h.biSize = sizeof( BITMAPINFOHEADER );
    bi.h.biWidth = HASH_W;
    bi.h.biHeight = HASH_H;
    bi.h.biPlanes = 1;
    bi.h.biBitCount = 8;
    GetDIBits( m, b, 0, HASH_H, bits, (BITMAPINFO *)&bi, DIB_PAL_COLORS );
    for( i = 0; i < HASH_W * HASH_H; ++i ) {
        h = (h ^ bits[i]) * 16777619UL;
    }
    DeleteObject( b );
    DeleteDC( m );
    return h;
}

static HBITMAP picture( int w, int h, int seed );
static HBITMAP save_a;
static HBITMAP save_b;

/* The bitmaps the tests change, made again: the fixed sequence must not see
 * what the timed loop - a different length under each driver - left in them. */
static void reset_pictures( void )
{
    HBITMAP o;

    DeleteObject( bmp_card2 );
    bmp_card2 = picture( 71, 96, 5 );
    o = SelectObject( mem, save_a );
    PatBlt( mem, 0, 0, 71, 96, WHITENESS );
    SelectObject( mem, save_b );
    PatBlt( mem, 0, 0, 71, 96, WHITENESS );
    SelectObject( mem, o );
}

/* Tests are numbered so two runs can be checked against each other by eye: the
 * number and name show on the taskbar button, and a "started" line goes to the
 * file first, so a fault leaves the test that caused it on record. */
#define TEST_COUNT  35
static int  test_no;

/* To the disk, not only the cache: a hang on the real card ends in a power-off,
 * and the line naming the test must survive it. */
static void flush_out( void )
{
    fflush( out );
    _commit( fileno( out ) );
}

static void run( const char *name, OP op, int fixed )
{
    double  t0;
    double  t;
    long    n = 0;
    int     i;
    char    title[48];
    M8_PF_TAB pf;

    ++test_no;
    sprintf( title, "M8BENCH %d/%d %s", test_no, TEST_COUNT, name );
    SetWindowText( wnd, title );
    fprintf( out, "started %d/%d %s\n", test_no, TEST_COUNT, name );
    flush_out();
    /* Let the taskbar repaint before the timing starts. */
    Sleep( 300 );

    clear();
    rnd = 12345;
    if( prof ) {
        prof_read( &pf, 1 );
    }
    t0 = now_ms();
    do {
        op( (int)n );
        ++n;
        t = now_ms() - t0;
    } while( t < run_ms );
    if( prof ) {
        double  us = 0.0;
        double  calls = 0.0;
        double  lcalls = 0.0;

        GdiFlush();
        prof_read( &pf, 0 );
        for( i = 0; i < M8_PF_N; ++i ) {
            us += PF_US( (double)pf.e[i].ticks + pf.e[i].long_ticks );
            calls += pf.e[i].calls;
            lcalls += pf.e[i].long_calls;
        }
        /* The timer's own cost, once per timed call, is taken off. */
        us -= (calls + lcalls) * PF_US( (double)pf.stamp_ticks );
        fprintf( out, "prof %-16s drv_us/op %8.1f  calls/op %6.2f  long %ld  hits %u  checked %u"
                 "  bad %u  stamp_us %.1f", name, us / n, (calls + lcalls) / n, (long)lcalls,
                 pf.hits, pf.checked, pf.bad, PF_US( (double)pf.stamp_ticks ) );
        for( i = 0; i < M8_PF_N; ++i ) {
            if( pf.e[i].calls + pf.e[i].long_calls != 0 ) {
                fprintf( out, "  e%d %lu/%.1f", i, pf.e[i].calls + pf.e[i].long_calls,
                         PF_US( (double)pf.e[i].ticks + pf.e[i].long_ticks ) /
                         (pf.e[i].calls + pf.e[i].long_calls) );
            }
        }
        fprintf( out, "\n" );
    }
    /* The fixed sequence, for the hash: the same calls under every driver. */
    reset_pictures();
    clear();
    rnd = 777;
    for( i = 0; i < fixed; ++i ) {
        op( i );
    }
    /* SCRATCH_PAD_1: a diagnostic driver build leaves its last engine reset
     * there (Dosr: routine o, wait s, count r); anything else is noise. */
    fprintf( out, "%-16s %10.1f ops/s %8.3f ms/op  hash %08lx  #%d  pad %04x\n", name,
             n * 1000.0 / t, t / n, hash_area(), test_no, inpw( 0x56EE ) );
    flush_out();
}

/* ---------------------------------------------------------------- the tests */

static const char line_mixed[] = "The quick brown fox jumps over the lazy dog 0123456789";
static const char line_same[]  = "mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm";

static void t_text_sys( int i )
{
    SelectObject( dc, GetStockObject( SYSTEM_FONT ) );
    SetBkMode( dc, OPAQUE );
    TextOut( dc, 4 + (i % 3) * 200, HASH_Y + (i % 12) * 16, line_mixed, 20 );
}

static void t_text_same( int i )
{
    SelectObject( dc, GetStockObject( SYSTEM_FONT ) );
    SetBkMode( dc, OPAQUE );
    TextOut( dc, HASH_X, HASH_Y + (i % 12) * 16, line_same, 16 );
}

static void t_text_gui( int i )
{
    SelectObject( dc, GetStockObject( ANSI_VAR_FONT ) );
    SetBkMode( dc, TRANSPARENT );
    TextOut( dc, HASH_X, HASH_Y + (i % 14) * 14, line_mixed, 24 );
}

static void t_text_varied( int i )
{
    char s[32];

    SelectObject( dc, GetStockObject( ANSI_VAR_FONT ) );
    SetBkMode( dc, OPAQUE );
    wsprintf( s, "Item %d of %d", i, i * 7 );
    TextOut( dc, HASH_X, HASH_Y + (i % 14) * 14, s, lstrlen( s ) );
}

static void t_fill_small( int i )
{
    HBRUSH b = CreateSolidBrush( PALETTEINDEX( i & 15 ) );
    HBRUSH o = SelectObject( dc, b );

    PatBlt( dc, HASH_X + r( 184 ), HASH_Y + r( 184 ), 16, 16, PATCOPY );
    SelectObject( dc, o );
    DeleteObject( b );
}

static void t_fill_large( int i )
{
    SelectObject( dc, GetStockObject( i & 1 ? GRAY_BRUSH : LTGRAY_BRUSH ) );
    PatBlt( dc, 0, 0, 600, 440, PATCOPY );
}

static void t_fill_hatch( int i )
{
    HBRUSH b = CreateHatchBrush( HS_DIAGCROSS, RGB( 0, 0, 128 ) );
    HBRUSH o = SelectObject( dc, b );

    PatBlt( dc, HASH_X, HASH_Y + (i % 4) * 20, 200, 100, PATCOPY );
    SelectObject( dc, o );
    DeleteObject( b );
}

static void t_fill_xor( int i )
{
    SelectObject( dc, GetStockObject( GRAY_BRUSH ) );
    PatBlt( dc, HASH_X + (i % 5) * 10, HASH_Y + (i % 9) * 20, 100, 20, PATINVERT );
}

static void t_dstinvert( int i )
{
    PatBlt( dc, HASH_X + (i % 3), HASH_Y, 150, 100, DSTINVERT );
}

static void t_blit_small( int i )
{
    BitBlt( dc, HASH_X + r( 160 ), HASH_Y + r( 160 ), 32, 32, dc, r( 300 ), r( 200 ), SRCCOPY );
}

static void t_blit_large( int i )
{
    BitBlt( dc, (i & 1) * 20, (i & 2) * 10, 300, 200, dc, 300, 240, SRCCOPY );
}

static void t_scroll( int i )
{
    ScrollDC( dc, 0, -16, NULL, NULL, NULL, NULL );
}

static void t_bmp( HBITMAP b, int w, int h, int x, int y )
{
    HBITMAP o = SelectObject( mem, b );

    BitBlt( dc, x, y, w, h, mem, 0, 0, SRCCOPY );
    SelectObject( mem, o );
}

static void t_bmp_icon( int i )
{
    t_bmp( bmp_icon, 32, 32, HASH_X + (i % 6) * 32, HASH_Y + (i % 5) * 32 );
}

static void t_bmp_card( int i )
{
    t_bmp( bmp_card, 71, 96, HASH_X + (i % 2) * 71, HASH_Y + (i % 2) * 50 );
}

/* A different picture every call: what a cache cannot help. */
static void t_bmp_changing( int i )
{
    HBITMAP o = SelectObject( mem, bmp_card2 );

    SetPixel( mem, i % 71, (i / 71) % 96, PALETTEINDEX( i & 15 ) );
    BitBlt( dc, HASH_X, HASH_Y, 71, 96, mem, 0, 0, SRCCOPY );
    SelectObject( mem, o );
}

static void t_bmp_large( int i )
{
    t_bmp( bmp_large, 300, 200, (i & 1) * 40, 20 );
}

static void t_dib( int i )
{
    SetDIBitsToDevice( dc, HASH_X + (i % 2) * 50, HASH_Y + (i % 2) * 50, 100, 100, 0, 0, 0, 100,
                       dib_bits, (BITMAPINFO *)&dib, DIB_RGB_COLORS );
}

static void t_stretch_dib( int i )
{
    StretchDIBits( dc, HASH_X, HASH_Y, 200, 200, 0, 0, 100, 100, dib_bits, (BITMAPINFO *)&dib,
                   DIB_RGB_COLORS, SRCCOPY );
}

static void t_stretchblt( int i )
{
    HBITMAP o = SelectObject( mem, bmp_card );

    StretchBlt( dc, HASH_X, HASH_Y, 142, 192, mem, 0, 0, 71, 96, SRCCOPY );
    SelectObject( mem, o );
}

static void t_mono( int i )
{
    HBITMAP o = SelectObject( mem, bmp_mono );

    SetTextColor( dc, RGB( 255, 255, 0 ) );
    SetBkColor( dc, RGB( 0, 0, 128 ) );
    BitBlt( dc, 0, HASH_Y + (i % 4) * 10, 400, 50, mem, (i % 40), 0, SRCCOPY );
    SelectObject( mem, o );
    SetTextColor( dc, RGB( 0, 0, 0 ) );
    SetBkColor( dc, RGB( 255, 255, 255 ) );
}

static void t_icon( int i )
{
    DrawIcon( dc, HASH_X + (i % 5) * 36, HASH_Y + (i % 5) * 36, LoadIcon( NULL, IDI_APPLICATION ) );
}

static void t_scr2mem( int i )
{
    HBITMAP o = SelectObject( mem, bmp_card2 );

    BitBlt( mem, 0, 0, 71, 96, dc, HASH_X + (i % 50), HASH_Y, SRCCOPY );
    SelectObject( mem, o );
    if( i < 4 ) {
        t_bmp( bmp_card2, 71, 96, HASH_X + i * 20, HASH_Y + 100 );
    }
}

static void t_mem2mem( int i )
{
    HDC     m2 = CreateCompatibleDC( dc );
    HBITMAP o2 = SelectObject( m2, bmp_card );
    HBITMAP o = SelectObject( mem, bmp_card2 );

    BitBlt( mem, i % 30, 0, 40, 96, m2, 0, 0, SRCCOPY );
    SelectObject( mem, o );
    SelectObject( m2, o2 );
    DeleteDC( m2 );
    if( i < 4 ) {
        t_bmp( bmp_card2, 71, 96, HASH_X + i * 20, HASH_Y );
    }
}

/* Solitaire's drag frame: save the new area, patch the overlap between the two saves,
 * put the card into the old save, restore the old area, draw the card. */

static void t_drag( int i )
{
    HDC     ma = CreateCompatibleDC( dc );
    HDC     mb = CreateCompatibleDC( dc );
    HBITMAP oa = SelectObject( ma, (i & 1) ? save_b : save_a );
    HBITMAP ob = SelectObject( mb, (i & 1) ? save_a : save_b );
    HBITMAP oc = SelectObject( mem, bmp_card );
    int     x = 100 + (i % 200);
    int     y = 100 + (i % 120);

    BitBlt( mb, 0, 0, 71, 96, dc, x + 2, y + 1, SRCCOPY );
    BitBlt( mb, 0, 0, 69, 95, ma, 2, 1, SRCCOPY );
    BitBlt( ma, 2, 1, 69, 95, mem, 0, 0, SRCCOPY );
    BitBlt( dc, x, y, 71, 96, ma, 0, 0, SRCCOPY );
    BitBlt( dc, x + 2, y + 1, 71, 96, mem, 0, 0, SRCCOPY );
    SelectObject( mem, oc );
    SelectObject( mb, ob );
    SelectObject( ma, oa );
    DeleteDC( mb );
    DeleteDC( ma );
}

static void t_lines( int i )
{
    MoveToEx( dc, HASH_X + r( 200 ), HASH_Y + r( 200 ), NULL );
    LineTo( dc, HASH_X + r( 200 ), HASH_Y + r( 200 ) );
}

static void t_lines_hv( int i )
{
    int y = HASH_Y + r( 200 );

    MoveToEx( dc, HASH_X, y, NULL );
    LineTo( dc, HASH_X + 200, y );
    MoveToEx( dc, HASH_X + r( 200 ), HASH_Y, NULL );
    LineTo( dc, HASH_X + r( 200 ), HASH_Y + 200 );
}

static void t_lines_dash( int i )
{
    HPEN p = CreatePen( PS_DASH, 1, RGB( 0, 0, 0 ) );
    HPEN o = SelectObject( dc, p );

    MoveToEx( dc, HASH_X + r( 200 ), HASH_Y + r( 200 ), NULL );
    LineTo( dc, HASH_X + r( 200 ), HASH_Y + r( 200 ) );
    SelectObject( dc, o );
    DeleteObject( p );
}

static void t_rubber( int i )
{
    int o = SetROP2( dc, R2_NOT );

    SelectObject( dc, GetStockObject( NULL_BRUSH ) );
    Rectangle( dc, HASH_X + (i % 20), HASH_Y + (i % 20), HASH_X + 150, HASH_Y + 120 );
    SetROP2( dc, o );
}

static void t_setpixel( int i )
{
    SetPixel( dc, HASH_X + r( 200 ), HASH_Y + r( 200 ), PALETTEINDEX( i & 15 ) );
}

static void t_getpixel( int i )
{
    GetPixel( dc, HASH_X + r( 200 ), HASH_Y + r( 200 ) );
}

static void t_ellipse( int i )
{
    SelectObject( dc, GetStockObject( i & 1 ? GRAY_BRUSH : WHITE_BRUSH ) );
    Ellipse( dc, HASH_X + r( 150 ), HASH_Y + r( 150 ), HASH_X + 200 - r( 20 ), HASH_Y + 200 - r( 20 ) );
}

static void t_polygon( int i )
{
    POINT pt[6];
    int   k;

    for( k = 0; k < 6; ++k ) {
        pt[k].x = HASH_X + r( 200 );
        pt[k].y = HASH_Y + r( 200 );
    }
    SelectObject( dc, GetStockObject( i & 1 ? DKGRAY_BRUSH : LTGRAY_BRUSH ) );
    Polygon( dc, pt, 6 );
}

static void t_rect( int i )
{
    SelectObject( dc, GetStockObject( LTGRAY_BRUSH ) );
    Rectangle( dc, HASH_X + r( 100 ), HASH_Y + r( 100 ), HASH_X + 100 + r( 100 ),
               HASH_Y + 100 + r( 100 ) );
}

static void t_flood( int i )
{
    HBRUSH b = CreateSolidBrush( PALETTEINDEX( 1 + (i & 7) ) );
    HBRUSH o = SelectObject( dc, b );

    if( i == 0 ) {
        SelectObject( dc, GetStockObject( NULL_BRUSH ) );
        Rectangle( dc, HASH_X + 20, HASH_Y + 20, HASH_X + 180, HASH_Y + 180 );
        SelectObject( dc, b );
    }
    ExtFloodFill( dc, HASH_X + 100, HASH_Y + 100, GetPixel( dc, HASH_X + 100, HASH_Y + 100 ),
                  FLOODFILLSURFACE );
    SelectObject( dc, o );
    DeleteObject( b );
}

static void t_cursor( int i )
{
    SetCursorPos( 50 + (i * 7) % 500, 50 + (i * 3) % 350 );
}

/* ------------------------------------------------------------------- setup */

static HBITMAP picture( int w, int h, int seed )
{
    HBITMAP b = CreateCompatibleBitmap( dc, w, h );
    HBITMAP o = SelectObject( mem, b );
    HBRUSH  br;
    int     k;

    PatBlt( mem, 0, 0, w, h, WHITENESS );
    for( k = 0; k < 12; ++k ) {
        br = CreateSolidBrush( PALETTEINDEX( (seed + k) & 15 ) );
        SelectObject( mem, br );
        Ellipse( mem, (k * 13) % w, (k * 7) % h, (k * 13) % w + w / 3, (k * 7) % h + h / 3 );
        SelectObject( mem, GetStockObject( WHITE_BRUSH ) );
        DeleteObject( br );
    }
    SetBkMode( mem, TRANSPARENT );
    TextOut( mem, 2, 2, "K", 1 );
    SelectObject( mem, o );
    return b;
}

static void setup( void )
{
    static WORD mono_bits[50 * 26];
    int         k;

    mem = CreateCompatibleDC( dc );
    bmp_card = picture( 71, 96, 1 );
    bmp_card2 = picture( 71, 96, 5 );
    bmp_icon = picture( 32, 32, 9 );
    bmp_large = picture( 300, 200, 3 );
    save_a = CreateCompatibleBitmap( dc, 71, 96 );
    save_b = CreateCompatibleBitmap( dc, 71, 96 );
    for( k = 0; k < 50 * 26; ++k ) {
        mono_bits[k] = (WORD)(0x0F0F ^ (k * 0x2493));
    }
    bmp_mono = CreateBitmap( 400, 50, 1, 1, mono_bits );
    memset( &dib, 0, sizeof( dib ) );
    dib.h.biSize = sizeof( BITMAPINFOHEADER );
    dib.h.biWidth = 100;
    dib.h.biHeight = 100;
    dib.h.biPlanes = 1;
    dib.h.biBitCount = 8;
    dib.h.biClrUsed = 256;
    for( k = 0; k < 256; ++k ) {
        dib.pal[k].rgbRed = (BYTE)k;
        dib.pal[k].rgbGreen = (BYTE)(255 - k);
        dib.pal[k].rgbBlue = (BYTE)((k * 3) & 255);
    }
    for( k = 0; k < 100 * 100; ++k ) {
        dib_bits[k] = (BYTE)((k % 100) * 2 + (k / 100) % 7);
    }
}

static const char *driver_name( void )
{
    static char name[64] = "?";
    HKEY        k;
    DWORD       n = sizeof( name );

    if( RegOpenKeyEx( HKEY_LOCAL_MACHINE,
                      "System\\CurrentControlSet\\Services\\Class\\Display\\0000\\DEFAULT", 0,
                      KEY_READ, &k ) == ERROR_SUCCESS ) {
        RegQueryValueEx( k, "drv", NULL, NULL, (BYTE *)name, &n );
        RegCloseKey( k );
    }
    return name;
}

int WINAPI WinMain( HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show )
{
    WNDCLASS    wc;
    SYSTEMTIME  st;

    char        *sp;
    int         esc = M8_ESC_PROF;

    /* "<label> [ms per test]" */
    sp = strchr( cmd, ' ' );
    if( sp != NULL ) {
        *sp = '\0';
        if( atoi( sp + 1 ) > 0 ) {
            run_ms = atoi( sp + 1 );
        }
    }
    QueryPerformanceFrequency( &freq );
    memset( &wc, 0, sizeof( wc ) );
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = inst;
    wc.hbrBackground = (HBRUSH)GetStockObject( WHITE_BRUSH );
    wc.lpszClassName = "M8Bench";
    RegisterClass( &wc );
    wnd = CreateWindowEx( WS_EX_TOPMOST, "M8Bench", "M8BENCH", WS_POPUP | WS_VISIBLE, 0, 0,
                          WIN_W, WIN_H, NULL, NULL, inst, NULL );
    UpdateWindow( wnd );
    dc = GetDC( wnd );
    out = fopen( "C:\\M8BENCH.TXT", "a" );
    if( out == NULL ) {
        return 1;
    }
    GetLocalTime( &st );
    prof = ExtEscape( dc, QUERYESCSUPPORT, sizeof( esc ), (LPCSTR)&esc, 0, NULL ) > 0;
    fprintf( out, "== %s  driver %s  %dx%dx%d  %04d-%02d-%02d %02d:%02d  %d ms%s\n",
             cmd[0] ? cmd : "-", driver_name(), GetDeviceCaps( dc, HORZRES ),
             GetDeviceCaps( dc, VERTRES ), GetDeviceCaps( dc, BITSPIXEL ), st.wYear, st.wMonth,
             st.wDay, st.wHour, st.wMinute, run_ms, prof ? "  profiled" : "" );
    flush_out();
    setup();
    /* The cursor away from the window, except for its own test. */
    SetCursorPos( GetDeviceCaps( dc, HORZRES ) - 1, GetDeviceCaps( dc, VERTRES ) - 1 );

    run( "text_sys", t_text_sys, 24 );
    run( "text_same", t_text_same, 24 );
    run( "text_gui", t_text_gui, 28 );
    run( "text_varied", t_text_varied, 28 );
    run( "fill_small", t_fill_small, 40 );
    run( "fill_large", t_fill_large, 3 );
    run( "fill_hatch", t_fill_hatch, 4 );
    run( "fill_xor", t_fill_xor, 9 );
    run( "dstinvert", t_dstinvert, 3 );
    run( "blit_small", t_blit_small, 40 );
    run( "blit_large", t_blit_large, 4 );
    run( "scroll", t_scroll, 8 );
    run( "bmp_icon", t_bmp_icon, 30 );
    run( "bmp_card", t_bmp_card, 4 );
    run( "bmp_changing", t_bmp_changing, 20 );
    run( "bmp_large", t_bmp_large, 2 );
    run( "dib_8bpp", t_dib, 2 );
    run( "stretch_dib", t_stretch_dib, 1 );
    run( "stretchblt", t_stretchblt, 1 );
    run( "mono_bmp", t_mono, 4 );
    run( "draw_icon", t_icon, 5 );
    run( "scr_to_mem", t_scr2mem, 4 );
    run( "mem_to_mem", t_mem2mem, 4 );
    run( "drag_frame", t_drag, 10 );
    run( "lines", t_lines, 40 );
    run( "lines_hv", t_lines_hv, 20 );
    run( "lines_dash", t_lines_dash, 20 );
    run( "rubber_band", t_rubber, 7 );
    run( "setpixel", t_setpixel, 200 );
    run( "getpixel", t_getpixel, 50 );
    run( "polygon", t_polygon, 10 );
    run( "rectangle", t_rect, 10 );
    run( "floodfill", t_flood, 3 );
    run( "cursor_move", t_cursor, 20 );
    /* Last: it faulted in the DIB Engine under our driver's fallback (10-17). */
    run( "ellipse", t_ellipse, 10 );

    fprintf( out, "\n" );
    fclose( out );
    ReleaseDC( wnd, dc );
    DestroyWindow( wnd );
    return 0;
}
