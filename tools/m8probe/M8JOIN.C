/* M8JOIN.EXE - does the polygon fill take a patch of two triangles as one outline?
 *
 * The joined path draws a patch's boundary into one scratch plane and fills it once. On the bed its
 * fills came out empty while the one-triangle fill works. One quad of two triangles of one colour,
 * five ways; each is read back at the two triangles' middles, beside the shared edge, and outside.
 *   1 per triangle, as the driver did: scratch at 640, 640, the box's corner on the scratch's
 *   2 one fill, the quad's 4 boundary edges, same scratch place
 *   3 one fill, all 6 edges XORed (the shared one cancels), same scratch place
 *   4 the joined path's left half: scratch at 640 + x, y
 *   5 the joined path's right half's rows: scratch at 640 + x, 512 + y
 * A case passes when all three inside points read the colour and the outside point reads 0.
 *
 * Log: C:\M8JOIN.TXT. About 8 s on screen, each case held 1 s.
 *
 * A Win32 console on Windows 95 is a DOS VM, so this is a window program.
 *
 * Build: wcl386 -q -bt=nt -l=nt_win -ox -i=<watcom>h -i=<watcom>h\nt -i=<watcom>h\nt\directx
 *        -fe=M8JOIN.EXE M8JOIN.C ddraw.lib
 */

#define INITGUID
#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <ddraw.h>

#define SCR_W           640
#define SCR_H           480

#define GP_STAT         0x9AE8
#define CMD             0x9AE8
#define EXT_FIFO        0x9AEE
#define CUR_Y           0x82E8
#define CUR_X           0x86E8
#define SRC_Y           0x8AE8
#define SRC_X           0x8EE8
#define MAJ_AXIS_PCNT   0x96E8
#define FRGD_COLOR      0xA6E8
#define WRT_MASK        0xAAE8
#define RD_MASK         0xAEE8
#define FRGD_MIX        0xBAE8
#define MULTIFUNC_CNTL  0xBEE8
#define PIX_TRANS       0xE2E8
#define LINEDRAW_INDEX  0x9AEE
#define LINEDRAW_OPT    0xA2EE
#define DEST_X_START    0xA6EE
#define DEST_X_END      0xAAEE
#define DEST_Y_END      0xAEEE
#define SRC_X_START     0xB2EE
#define ALU_FG_FN       0xBAEE
#define SRC_X_END       0xBEEE
#define SRC_Y_DIR       0xC2EE
#define DP_CONFIG       0xCEEE
#define SCISSOR_L       0xDAEE
#define SCISSOR_T       0xDEEE
#define SCISSOR_R       0xE2EE
#define SCISSOR_B       0xE6EE
#define LINEDRAW        0xFEEE

#define DP_PLAIN        0x2211
#define DP_FILL_FLAT    0x2213
#define MIX_XOR         5
#define MIX_REPLACE     7
#define OPT_LAST_OFF    4
#define OPT_POLY        2
#define ESC_RESYNC      0x4D3E
#define COLOUR          0x00C3
#define PLANE           0x0001

static FILE     *out;
static int      lost;

/* Two triangles sharing the edge (260,90)-(120,300); the quad lies within x 40-300. */
static const short tri_a[6] = { 60, 80, 260, 90, 120, 300 };
static const short tri_b[6] = { 260, 90, 120, 300, 290, 280 };
static const short quad[8] = { 60, 80, 260, 90, 290, 280, 120, 300 };

static void wait_idle( void )
{
    long n;

    for( n = 0; n < 400000L; ++n ) {
        if( inpw( EXT_FIFO ) == 0 && !(inpw( GP_STAT ) & 0x0200) ) {
            return;
        }
    }
    ++lost;
}

static void room( int n )
{
    long i;

    for( i = 0; i < 400000L; ++i ) {
        if( !(inpw( EXT_FIFO ) & (0x8000 >> n)) ) {
            return;
        }
    }
    ++lost;
}

static void engine_setup( void )
{
    wait_idle();
    outpw( WRT_MASK, 0x00FF );
    outpw( RD_MASK, 0x00FF );
    outpw( MULTIFUNC_CNTL, 0xA000 );
    outpw( SCISSOR_L, 0x0800 );
    outpw( SCISSOR_T, 0x0800 );
    outpw( SCISSOR_R, 0x07FF );
    outpw( SCISSOR_B, 0x07FF );
    outpw( FRGD_MIX, 0x0027 );
    wait_idle();
}

static void rect( int x, int y, int w, int h, int colour )
{
    room( 12 );
    outpw( WRT_MASK, 0x00FF );
    outpw( RD_MASK, 0x00FF );
    outpw( DP_CONFIG, DP_PLAIN );
    outpw( FRGD_MIX, 0x0027 );
    outpw( FRGD_COLOR, colour );
    outpw( CUR_X, x );
    outpw( CUR_Y, y );
    outpw( MAJ_AXIS_PCNT, w - 1 );
    outpw( MULTIFUNC_CNTL, h - 1 );
    outpw( CMD, 0x40F3 );
}

static int read_pixel( int x, int y )
{
    int v;
    long n;

    wait_idle();
    outpw( WRT_MASK, 0x00FF );
    outpw( RD_MASK, 0x00FF );
    outpw( DP_CONFIG, DP_PLAIN );
    outpw( FRGD_MIX, 0x0047 );
    outpw( CUR_X, x );
    outpw( CUR_Y, y );
    outpw( MAJ_AXIS_PCNT, 1 );
    outpw( MULTIFUNC_CNTL, 0 );
    wait_idle();
    outpw( CMD, 0x53B0 );
    for( n = 0; n < 200000L && !(inpw( GP_STAT ) & 0x0100); ++n ) {
    }
    v = inpw( PIX_TRANS ) & 0xFF;
    wait_idle();
    outpw( FRGD_MIX, 0x0027 );
    return( v );
}

/* One edge, top to bottom without its last row, XOR into the plane, at scratch ox + x, oy + y. */
static void edge( int ox, int oy, int xa, int ya, int xb, int yb )
{
    if( ya == yb ) {
        return;
    }
    if( ya > yb ) {
        int t = xa;

        xa = xb;
        xb = t;
        t = ya;
        ya = yb;
        yb = t;
    }
    room( 5 );
    outpw( LINEDRAW_INDEX, 0 );
    outpw( LINEDRAW, ox + xa );
    outpw( LINEDRAW, oy + ya );
    outpw( LINEDRAW, ox + xb );
    outpw( LINEDRAW, oy + yb );
}

static void edges_begin( void )
{
    room( 6 );
    outpw( WRT_MASK, PLANE );
    outpw( RD_MASK, 0x00FF );
    outpw( FRGD_COLOR, 0x00FF );
    outpw( DP_CONFIG, DP_PLAIN );
    outpw( ALU_FG_FN, MIX_XOR );
    outpw( LINEDRAW_OPT, OPT_POLY | OPT_LAST_OFF );
}

/* A closed figure's edges, n corners. */
static void figure( int ox, int oy, const short *v, int n )
{
    int i;

    for( i = 0; i < n; ++i ) {
        int j = (i + 1) % n;

        edge( ox, oy, v[2 * i], v[2 * i + 1], v[2 * j], v[2 * j + 1] );
    }
}

/* The polygon fill of box l, t .. r, b from the scratch, whose (sx, sy) is the box's (l, t). */
static void fill( int l, int t, int r, int b, int sx, int sy )
{
    room( 15 );
    outpw( WRT_MASK, 0x00FF );
    outpw( RD_MASK, PLANE );
    outpw( DP_CONFIG, DP_FILL_FLAT );
    outpw( ALU_FG_FN, MIX_REPLACE );
    outpw( FRGD_COLOR, COLOUR );
    outpw( SRC_X, sx );
    outpw( SRC_Y, sy + 1 );
    outpw( SRC_X_START, sx );
    outpw( SRC_X_END, sx + r - l + 1 );
    outpw( SRC_Y_DIR, 1 );
    outpw( CUR_X, l );
    outpw( DEST_X_START, l );
    outpw( DEST_X_END, r + 1 );
    outpw( CUR_Y, t + 1 );
    outpw( DEST_Y_END, b );
    room( 1 );
    outpw( RD_MASK, 0x00FF );
}

static void outline( const short *v, int n )
{
    int i;

    room( 6 );
    outpw( WRT_MASK, 0x00FF );
    outpw( FRGD_COLOR, COLOUR );
    outpw( DP_CONFIG, DP_PLAIN );
    outpw( ALU_FG_FN, MIX_REPLACE );
    outpw( LINEDRAW_OPT, 0 );
    for( i = 0; i < n; ++i ) {
        int j = (i + 1) % n;

        room( 5 );
        outpw( LINEDRAW_INDEX, 0 );
        outpw( LINEDRAW, v[2 * i] );
        outpw( LINEDRAW, v[2 * i + 1] );
        outpw( LINEDRAW, v[2 * j] );
        outpw( LINEDRAW, v[2 * j + 1] );
    }
}

static void box( const short *v, int n, int *l, int *t, int *r, int *b )
{
    int i;

    *l = *r = v[0];
    *t = *b = v[1];
    for( i = 1; i < n; ++i ) {
        if( v[2 * i] < *l ) *l = v[2 * i];
        if( v[2 * i] > *r ) *r = v[2 * i];
        if( v[2 * i + 1] < *t ) *t = v[2 * i + 1];
        if( v[2 * i + 1] > *b ) *b = v[2 * i + 1];
    }
}

static void clear_all( void )
{
    rect( 0, 0, SCR_W, SCR_H, 0 );
    rect( 640, 0, 352, 1024, 0 );
    wait_idle();
}

/* Scratch rows set in the plane over the quad's box at (sx, sy) for (l, t), counted per row. */
static void check( int n, const char *name, int sx, int sy )
{
    int a = read_pixel( 147, 157 );
    int b = read_pixel( 223, 223 );
    int e = read_pixel( 192, 200 );
    int o = read_pixel( 20, 400 );
    int ok = a == COLOUR && b == COLOUR && e == COLOUR && o == 0;

    fprintf( out, "%d %-34s a %3d  b %3d  edge %3d  outside %3d  %s\n", n, name, a, b, e, o, ok ? "pass" : "FAIL" );
    (void)sx;
    (void)sy;
    fflush( out );
    Sleep( 1000 );
}

static void one_tri( const short *v )
{
    int l;
    int t;
    int r;
    int b;

    box( v, 3, &l, &t, &r, &b );
    edges_begin();
    figure( 640 - l, 640 - t, v, 3 );
    fill( l, t, r, b, 640, 640 );
    outline( v, 3 );
    wait_idle();
    rect( 640, 512, 352, 480, 0 );
}

static void run( void )
{
    int l;
    int t;
    int r;
    int b;

    box( quad, 4, &l, &t, &r, &b );

    /* The read-back itself: a plain rectangle, and a line on its own. */
    clear_all();
    rect( 140, 150, 100, 80, COLOUR );
    outline( quad, 4 );
    wait_idle();
    fprintf( out, "0 plain rectangle reads %d (want %d), outline at 60,80 reads %d\n",
             read_pixel( 147, 157 ), COLOUR, read_pixel( 60, 80 ) );
    fflush( out );
    Sleep( 1000 );

    clear_all();
    one_tri( tri_a );
    one_tri( tri_b );
    check( 1, "per triangle", 0, 0 );

    clear_all();
    edges_begin();
    figure( 640 - l, 640 - t, quad, 4 );
    fill( l, t, r, b, 640, 640 );
    outline( quad, 4 );
    check( 2, "quad outline, scratch 640,640", 640, 640 );

    clear_all();
    edges_begin();
    figure( 640 - l, 640 - t, tri_a, 3 );
    figure( 640 - l, 640 - t, tri_b, 3 );
    fill( l, t, r, b, 640, 640 );
    outline( quad, 4 );
    check( 3, "6 edges XOR, scratch 640,640", 640, 640 );

    clear_all();
    edges_begin();
    figure( 640, 0, quad, 4 );
    fill( l, t, r, b, 640 + l, t );
    outline( quad, 4 );
    check( 4, "quad, scratch 640+x, y", 640 + l, t );

    clear_all();
    edges_begin();
    figure( 640, 512, quad, 4 );
    fill( l, t, r, b, 640 + l, 512 + t );
    outline( quad, 4 );
    check( 5, "quad, scratch 640+x, 512+y", 640 + l, 512 + t );
}

static LRESULT CALLBACK wndproc( HWND w, UINT m, WPARAM wp, LPARAM lp )
{
    return( DefWindowProc( w, m, wp, lp ) );
}

int PASCAL WinMain( HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show_cmd )
{
    WNDCLASS        wc;
    HWND            wnd;
    LPDIRECTDRAW    dd;
    HRESULT         hr;
    HDC             dc;

    (void)prev;
    (void)cmd;
    (void)show_cmd;
    out = fopen( "C:\\M8JOIN.TXT", "a" );
    if( out == NULL ) {
        return( 1 );
    }
    memset( &wc, 0, sizeof( wc ) );
    wc.lpfnWndProc = wndproc;
    wc.hInstance = inst;
    wc.lpszClassName = "M8JOIN";
    RegisterClass( &wc );
    wnd = CreateWindowEx( WS_EX_TOPMOST, "M8JOIN", "M8JOIN", WS_POPUP | WS_VISIBLE, 0, 0, SCR_W, SCR_H,
                          NULL, NULL, inst, NULL );
    hr = DirectDrawCreate( NULL, &dd, NULL );
    fprintf( out, "== M8JOIN  ddraw %08lx\n", (unsigned long)hr );
    fflush( out );
    if( hr != DD_OK ) {
        return( 1 );
    }
    dd->lpVtbl->SetCooperativeLevel( dd, wnd, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN );
    hr = dd->lpVtbl->SetDisplayMode( dd, SCR_W, SCR_H, 16 );
    fprintf( out, "setmode %08lx\n", (unsigned long)hr );
    fflush( out );
    if( hr != DD_OK ) {
        return( 1 );
    }
    ShowCursor( FALSE );
    Sleep( 1000 );

    engine_setup();
    run();
    fprintf( out, "waits that ran out: %d\n", lost );

    clear_all();
    ShowCursor( TRUE );
    dd->lpVtbl->RestoreDisplayMode( dd );
    dd->lpVtbl->SetCooperativeLevel( dd, wnd, DDSCL_NORMAL );
    dd->lpVtbl->Release( dd );
    dc = GetDC( NULL );
    if( dc != NULL ) {
        ExtEscape( dc, ESC_RESYNC, 0, NULL, 0, NULL );
        ReleaseDC( NULL, dc );
    }
    fprintf( out, "done\n\n" );
    fclose( out );
    DestroyWindow( wnd );
    return( 0 );
}
