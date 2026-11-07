/* M8TEXR: a texture carried by rows and lines, card timing and pixels, 640x480.
 *
 * Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
 *
 * SCAN_TO_X and LINEDRAW under DP_CONFIG 6211h step along a source, one source pixel per
 * destination pixel. One triangle, 32 and 100 px, REPS x PASSES, fastest kept:
 *   flat      edges, polygon fill: the control
 *   tatlas    edges XORed into plane 7 of the texture, 6213h fill, edges XORed out: shift only
 *   rows      a span a row with its own source start, source row following: shift
 *   shear     as rows with the source start moving 1 px a row: what a turned face needs
 *   flatrows  a span a row in one colour: what the source costs
 *   lines     a source row to each screen line, 45 degrees: a turned texture
 * Then one draw each, pixels logged: which source pixel lands where on a diagonal, a steep line,
 * a line drawn right to left and upwards, and SRC_Y_DIR 0 past the end of a source row.
 * Writes C:\M8TEXR.TXT.
 *
 * Build: wcl386 -q -bt=nt -l=nt_win -ox -i=<watcom>h -i=<watcom>h\nt -i=<watcom>h\nt\directx
 *        "-fe=M8TEXR.EXE" M8TEXR.C ddraw.lib
 */

#define INITGUID
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <unistd.h>
#include <ddraw.h>

#define SCR_W           640
#define SCR_H           480
#define ATLAS_X         640
#define ATLAS_Y         0
#define ATLAS_W         128
#define RAMP_Y          200
#define RAMP_W          64
#define SCRATCH_X       640
#define SCRATCH_Y       512
#define TX              100
#define TY              100
#define BG              0x41
#define INK             0x23
#define PASSES          20
#define REPS            3
#define READ_W          200
#define WAIT_TRIES      20000L

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
#define SUBSYS_CNTL     0x42E8
#define SUBSYS_STAT     0x42E8
#define LINEDRAW_INDEX  0x9AEE
#define LINEDRAW_OPT    0xA2EE
#define DEST_X_START    0xA6EE
#define DEST_X_END      0xAAEE
#define DEST_Y_END      0xAEEE
#define SRC_X_START     0xB2EE
#define ALU_BG_FN       0xB6EE
#define ALU_FG_FN       0xBAEE
#define SRC_X_END       0xBEEE
#define SRC_Y_DIR       0xC2EE
#define SCAN_TO_X       0xCAEE
#define DP_CONFIG       0xCEEE
#define SCISSOR_L       0xDAEE
#define SCISSOR_T       0xDEEE
#define SCISSOR_R       0xE2EE
#define SCISSOR_B       0xE6EE
#define LINEDRAW        0xFEEE

#define DP_PLAIN        0x2211
#define DP_FILL_FLAT    0x2213
#define DP_COPY         0x6211
#define DP_FILL_VRAM    0x6213
#define MIX_XOR         5
#define MIX_REPLACE     7
#define MIX_AVG         0x17
#define MIX_ADD         0x1B
#define OPT_LAST_OFF    4
#define OPT_POLY        2
#define ESC_RESYNC      0x4D3E

static FILE     *out;
static double   qpf;
static int      lost;
static const char *where = "start";
static int      seen_ext;
static int      seen_stat;
static int      seen_sub;
#define PLANE           0x0080
static short    tri[6];
static int      box_l;
static int      box_t;
static int      box_r;
static int      box_b;
static BYTE     row[READ_W + 2];
static BYTE     row2[READ_W + 2];
static short    rlo[256];
static short    rhi[256];

/* To disk, not just the cache: a hang that needs the power switch must still say how far it got. */
static void log_commit( void )
{
    fflush( out );
    fsync( fileno( out ) );
}

static double now( void )
{
    LARGE_INTEGER t;

    QueryPerformanceCounter( &t );
    return( (double)t.LowPart + 4294967296.0 * (double)t.HighPart );
}

/* Longer than any command here takes: a wait that runs out means the engine has stopped. */
static void wait_lost( void )
{
    if( lost++ == 0 ) {
        seen_ext = inpw( EXT_FIFO );
        seen_stat = inpw( GP_STAT );
        seen_sub = inpw( SUBSYS_STAT );
    }
}

static void wait_idle( void )
{
    long n;

    if( lost ) {
        return;
    }
    for( n = 0; n < WAIT_TRIES; ++n ) {
        if( inpw( EXT_FIFO ) == 0 && !(inpw( GP_STAT ) & 0x0200) ) {
            return;
        }
    }
    wait_lost();
}

/* A full FIFO can lock the card. */
static void room( int n )
{
    long i;

    if( lost ) {
        return;
    }
    for( i = 0; i < WAIT_TRIES; ++i ) {
        if( !(inpw( EXT_FIFO ) & (0x8000 >> n)) ) {
            return;
        }
    }
    wait_lost();
}

static void rect_mix( int x, int y, int w, int h, int colour, int mix, int mask )
{
    room( 12 );
    outpw( WRT_MASK, mask );
    outpw( RD_MASK, 0x00FF );
    outpw( DP_CONFIG, DP_PLAIN );
    outpw( FRGD_MIX, 0x0020 | mix );
    outpw( FRGD_COLOR, colour );
    outpw( CUR_X, x );
    outpw( CUR_Y, y );
    outpw( MAJ_AXIS_PCNT, w - 1 );
    outpw( MULTIFUNC_CNTL, h - 1 );
    outpw( CMD, 0x40F3 );
}

static void rect( int x, int y, int w, int h, int colour )
{
    rect_mix( x, y, w, h, colour, MIX_REPLACE, 0x00FF );
}

/* Each edge top to bottom without its last row, so every row holds a pair; XOR, so an apex row's
 * two pixels cancel. Drawn twice in the same place, the plane is as it was. */
static void edges( int ox, int oy )
{
    int i;

    room( 5 );
    outpw( WRT_MASK, PLANE );
    outpw( FRGD_COLOR, 0x00FF );
    outpw( DP_CONFIG, DP_PLAIN );
    outpw( ALU_FG_FN, MIX_XOR );
    outpw( LINEDRAW_OPT, OPT_POLY | OPT_LAST_OFF );
    for( i = 0; i < 3; ++i ) {
        int a = 2 * i;
        int b = 2 * ((i + 1) % 3);

        if( tri[a + 1] == tri[b + 1] ) {
            continue;
        }
        if( tri[a + 1] > tri[b + 1] ) {
            int t = a;

            a = b;
            b = t;
        }
        room( 5 );
        outpw( LINEDRAW_INDEX, 0 );
        outpw( LINEDRAW, ox + tri[a] );
        outpw( LINEDRAW, oy + tri[a + 1] );
        outpw( LINEDRAW, ox + tri[b] );
        outpw( LINEDRAW, oy + tri[b + 1] );
    }
}

/* The source's corner (sx, sy) stands for the box's; rows strictly between top and bottom. */
static void fill( int sx, int sy, int dp, int mix, int colour )
{
    room( 15 );
    outpw( WRT_MASK, 0x00FF );
    outpw( RD_MASK, PLANE );
    outpw( DP_CONFIG, dp );
    outpw( ALU_FG_FN, mix );
    outpw( FRGD_COLOR, colour );
    outpw( SRC_X, sx );
    outpw( SRC_Y, sy + 1 );
    outpw( SRC_X_START, sx );
    outpw( SRC_X_END, sx + box_r - box_l + 1 );
    outpw( SRC_Y_DIR, 1 );
    outpw( CUR_X, box_l );
    outpw( DEST_X_START, box_l );
    outpw( DEST_X_END, box_r + 1 );
    outpw( CUR_Y, box_t + 1 );
    outpw( DEST_Y_END, box_b );
    room( 2 );
    outpw( RD_MASK, 0x00FF );
    outpw( ALU_FG_FN, MIX_REPLACE );
}

static void copy_to( int sx, int sy, int send, int dx, int dy, int w, int h )
{
    room( 14 );
    outpw( WRT_MASK, 0x00FF );
    outpw( RD_MASK, 0x00FF );
    outpw( DP_CONFIG, DP_COPY );
    outpw( ALU_FG_FN, MIX_REPLACE );
    outpw( SRC_X, sx );
    outpw( SRC_Y, sy );
    outpw( SRC_X_START, sx );
    outpw( SRC_X_END, send );
    outpw( SRC_Y_DIR, 1 );
    outpw( CUR_X, dx );
    outpw( DEST_X_START, dx );
    outpw( DEST_X_END, dx + w );
    outpw( CUR_Y, dy );
    outpw( DEST_Y_END, dy + h );
}

/* Never read PIX_TRANS without DATA_READY: that locks the FIFO. */
static void read_row( int x, int y, int n, BYTE *p )
{
    long    w;
    long    t;

    memset( p, 0, n );
    wait_idle();
    if( lost ) {
        return;
    }
    outpw( RD_MASK, 0x00FF );
    outpw( FRGD_MIX, 0x0047 );
    outpw( CUR_X, x );
    outpw( CUR_Y, y );
    outpw( MAJ_AXIS_PCNT, n - 1 );
    outpw( MULTIFUNC_CNTL, 0 );
    wait_idle();
    outpw( CMD, 0x53B0 );
    for( w = 0; w < (n + 1) / 2; ++w ) {
        WORD    v;

        for( t = 0; t < WAIT_TRIES && !(inpw( GP_STAT ) & 0x0100); ++t ) {
        }
        if( t == WAIT_TRIES ) {
            wait_lost();
            return;
        }
        v = inpw( PIX_TRANS );
        p[2 * w] = (BYTE)v;
        p[2 * w + 1] = (BYTE)(v >> 8);
    }
    wait_idle();
    outpw( FRGD_MIX, 0x0027 );
}

static void log_row( const char *name, const BYTE *p, int n )
{
    int i;

    fprintf( out, "  %-9s", name );
    for( i = 0; i < n; ++i ) {
        fprintf( out, " %02x", p[i] );
    }
    fprintf( out, "\n" );
    log_commit();
}

/* The triangle's middle row, from just before the first pixel that is not background. */
static void log_shape( const char *name, int bg )
{
    int i;
    int y = (box_t + box_b) / 2;

    read_row( box_l - 4, y, READ_W, row );
    for( i = 0; i < READ_W - 28 && row[i] == bg; ++i ) {
    }
    i = i >= 2 ? i - 2 : 0;
    log_row( name, row + i, 28 );
}

static void make_tri( int s )
{
    tri[0] = TX;
    tri[1] = TY;
    tri[2] = TX + s;
    tri[3] = TY + s / 4;
    tri[4] = TX + s / 3;
    tri[5] = TY + s;
    box_l = TX;
    box_t = TY;
    box_r = TX + s;
    box_b = TY + s;
}

static void draw_flat( void )
{
    rect( SCRATCH_X, SCRATCH_Y, box_r - box_l + 2, box_b - box_t + 2, 0 );
    edges( SCRATCH_X - box_l, SCRATCH_Y - box_t );
    fill( SCRATCH_X, SCRATCH_Y, DP_FILL_FLAT, MIX_REPLACE, INK );
}

static void draw_tatlas( void )
{
    edges( ATLAS_X - box_l, ATLAS_Y - box_t );
    fill( ATLAS_X, ATLAS_Y, DP_FILL_VRAM, MIX_REPLACE, 0 );
    edges( ATLAS_X - box_l, ATLAS_Y - box_t );
}

/* Worked out before the first OUT: any port access empties the L1. */
static void make_rows( void )
{
    int y;
    int i;

    for( y = box_t; y <= box_b; ++y ) {
        int lo = 32767;
        int hi = -32768;

        for( i = 0; i < 3; ++i ) {
            int ax = tri[2 * i];
            int ay = tri[2 * i + 1];
            int bx = tri[2 * ((i + 1) % 3)];
            int by = tri[2 * ((i + 1) % 3) + 1];
            int x;

            if( (y < ay && y < by) || (y > ay && y > by) || ay == by ) {
                continue;
            }
            x = ax + (long)(bx - ax) * (y - ay) / (by - ay);
            if( x < lo ) lo = x;
            if( x > hi ) hi = x;
        }
        rlo[y - box_t] = (short)lo;
        rhi[y - box_t] = (short)hi;
    }
}

static void source_setup( void )
{
    room( 7 );
    outpw( WRT_MASK, 0x00FF );
    outpw( RD_MASK, 0x00FF );
    outpw( ALU_FG_FN, MIX_REPLACE );
    outpw( DP_CONFIG, DP_COPY );
    outpw( SRC_X_START, ATLAS_X );
    outpw( SRC_X_END, ATLAS_X + 384 );
    outpw( SRC_Y_DIR, 1 );
}

static void draw_rows( void )
{
    int n = box_b - box_t;
    int i;

    source_setup();
    for( i = 0; i < n; ++i ) {
        room( 5 );
        outpw( CUR_Y, box_t + i );
        outpw( CUR_X, rlo[i] );
        outpw( SRC_X, ATLAS_X + rlo[i] - box_l );
        outpw( SRC_Y, ATLAS_Y + i );
        outpw( SCAN_TO_X, rhi[i] + 1 );
    }
}

static void draw_shear( void )
{
    int n = box_b - box_t;
    int i;

    source_setup();
    for( i = 0; i < n; ++i ) {
        room( 5 );
        outpw( CUR_Y, box_t + i );
        outpw( CUR_X, rlo[i] );
        outpw( SRC_X, ATLAS_X + rlo[i] - box_l + i / 2 );
        outpw( SRC_Y, ATLAS_Y + i );
        outpw( SCAN_TO_X, rhi[i] + 1 );
    }
}

static void draw_flatrows( void )
{
    int n = box_b - box_t;
    int i;

    room( 5 );
    outpw( WRT_MASK, 0x00FF );
    outpw( RD_MASK, 0x00FF );
    outpw( ALU_FG_FN, MIX_REPLACE );
    outpw( DP_CONFIG, DP_PLAIN );
    outpw( FRGD_COLOR, INK );
    for( i = 0; i < n; ++i ) {
        room( 3 );
        outpw( CUR_Y, box_t + i );
        outpw( CUR_X, rlo[i] );
        outpw( SCAN_TO_X, rhi[i] + 1 );
    }
}

/* One source row a screen line, 45 degrees across the box. */
static void draw_lines( void )
{
    int n = box_b - box_t;
    int i;

    source_setup();
    room( 1 );
    outpw( LINEDRAW_OPT, 0 );
    for( i = 0; i < n; ++i ) {
        room( 7 );
        outpw( SRC_X, ATLAS_X );
        outpw( SRC_Y, ATLAS_Y + i );
        outpw( LINEDRAW_INDEX, 0 );
        outpw( LINEDRAW, box_l );
        outpw( LINEDRAW, box_t + i );
        outpw( LINEDRAW, box_l + (n - i) );
        outpw( LINEDRAW, box_t + n );
    }
}

static void arm( const char *name, void (*draw)( void ), int s )
{
    double  best = 1e30;
    double  t0;
    double  t;
    int     rep;
    int     p;

    where = name;
    fprintf( out, "%s %d\n", name, s );
    log_commit();
    for( rep = 0; rep < REPS && !lost; ++rep ) {
        rect( 0, 0, SCR_W, SCR_H, BG );
        wait_idle();
        t0 = now();
        for( p = 0; p < PASSES && !lost; ++p ) {
            draw();
        }
        wait_idle();
        t = (now() - t0) * 1e6 / qpf / PASSES;
        if( t < best ) {
            best = t;
        }
    }
    if( lost ) {
        return;
    }
    fprintf( out, "%3d %-9s %9.1f us\n", s, name, best );
    log_shape( name, BG );
    room( 1 );
    outpw( DP_CONFIG, DP_PLAIN );
}

/* An 8 x 8 grid of 7-bit colours; two rows where every pixel differs. */
static void make_atlas( void )
{
    int x;
    int y;

    for( y = 0; y < ATLAS_W / 8; ++y ) {
        for( x = 0; x < ATLAS_W / 8; ++x ) {
            rect( ATLAS_X + 8 * x, ATLAS_Y + 8 * y, 8, 8, 0x10 + ((3 * x + 5 * y) & 0x3F) );
        }
    }
    for( x = 0; x < RAMP_W; ++x ) {
        rect( ATLAS_X + x, RAMP_Y - 1, 1, 1, 0x80 + x );
        rect( ATLAS_X + x, RAMP_Y, 1, 1, 0x40 + x );
        rect( ATLAS_X + x, RAMP_Y + 1, 1, 1, 0xC0 + x );
    }
    wait_idle();
}

/* A line under test, then each listed screen point's pixel. */
static void line_test( const char *name, int x0, int y0, int x1, int y1, int dir, int send )
{
    int     n = (x1 > x0 ? x1 - x0 : x0 - x1) > (y1 > y0 ? y1 - y0 : y0 - y1) ?
                (x1 > x0 ? x1 - x0 : x0 - x1) : (y1 > y0 ? y1 - y0 : y0 - y1);
    int     i;

    where = name;
    fprintf( out, "%s: (%d,%d) to (%d,%d) dir %d\n", name, x0, y0, x1, y1, dir );
    log_commit();
    rect( 0, 0, SCR_W, SCR_H, BG );
    room( 14 );
    outpw( WRT_MASK, 0x00FF );
    outpw( RD_MASK, 0x00FF );
    outpw( ALU_FG_FN, MIX_REPLACE );
    outpw( DP_CONFIG, DP_COPY );
    outpw( SRC_X_START, ATLAS_X );
    outpw( SRC_X_END, send );
    outpw( SRC_Y_DIR, dir );
    outpw( SRC_X, ATLAS_X );
    outpw( SRC_Y, RAMP_Y );
    outpw( LINEDRAW_OPT, 0 );
    outpw( LINEDRAW_INDEX, 0 );
    outpw( LINEDRAW, x0 );
    outpw( LINEDRAW, y0 );
    outpw( LINEDRAW, x1 );
    room( 1 );
    outpw( LINEDRAW, y1 );
    wait_idle();
    for( i = 0; i <= n && i < READ_W && !lost; ++i ) {
        read_row( x0 + (long)(x1 - x0) * i / n, y0 + (long)(y1 - y0) * i / n, 2, row2 );
        row[i] = row2[0];
    }
    room( 2 );
    outpw( DP_CONFIG, DP_PLAIN );
    outpw( SRC_Y_DIR, 1 );
    log_row( name, row, n + 1 > 32 ? 32 : n + 1 );
}

static LRESULT CALLBACK wndproc( HWND w, UINT m, WPARAM a, LPARAM b )
{
    return( DefWindowProc( w, m, a, b ) );
}

int PASCAL WinMain( HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show )
{
    static const int    sizes[2] = { 32, 100 };
    WNDCLASS            wc;
    LPDIRECTDRAW        dd;
    HWND                wnd;
    HRESULT             hr;
    HDC                 dc;
    LARGE_INTEGER       f;
    int                 i;

    (void)prev;
    (void)show;
    out = fopen( "C:\\M8TEXR.TXT", "a" );
    if( out == NULL ) {
        return( 1 );
    }
    QueryPerformanceFrequency( &f );
    qpf = (double)f.LowPart + 4294967296.0 * (double)f.HighPart;
    memset( &wc, 0, sizeof( wc ) );
    wc.lpfnWndProc = wndproc;
    wc.hInstance = inst;
    wc.lpszClassName = "M8TEXR";
    RegisterClass( &wc );
    wnd = CreateWindowEx( WS_EX_TOPMOST, "M8TEXR", "M8TEXR", WS_POPUP | WS_VISIBLE, 0, 0, SCR_W, SCR_H,
                          NULL, NULL, inst, NULL );
    hr = DirectDrawCreate( NULL, &dd, NULL );
    fprintf( out, "== M8TEXR %s  ddraw %08lx\n", cmd, (unsigned long)hr );
    log_commit();
    if( hr != DD_OK ) {
        return( 1 );
    }
    dd->lpVtbl->SetCooperativeLevel( dd, wnd, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN );
    hr = dd->lpVtbl->SetDisplayMode( dd, SCR_W, SCR_H, 16 );
    fprintf( out, "setmode %08lx\n", (unsigned long)hr );
    log_commit();
    if( hr != DD_OK ) {
        return( 1 );
    }
    ShowCursor( FALSE );
    Sleep( 1000 );

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
    make_atlas();

    for( i = 0; i < 2 && !lost; ++i ) {
        make_tri( sizes[i] );
        make_rows();
        arm( "flat", draw_flat, sizes[i] );
        arm( "tatlas", draw_tatlas, sizes[i] );
        arm( "rows", draw_rows, sizes[i] );
        arm( "shear", draw_shear, sizes[i] );
        arm( "flatrows", draw_flatrows, sizes[i] );
        arm( "lines", draw_lines, sizes[i] );
    }
    if( !lost ) line_test( "across", TX, TY, TX + 31, TY, 1, ATLAS_X + 384 );
    if( !lost ) line_test( "diagonal", TX, TY, TX + 31, TY + 31, 1, ATLAS_X + 384 );
    if( !lost ) line_test( "shallow", TX, TY, TX + 31, TY + 8, 1, ATLAS_X + 384 );
    if( !lost ) line_test( "steep", TX, TY, TX + 8, TY + 31, 1, ATLAS_X + 384 );
    if( !lost ) line_test( "down", TX, TY, TX, TY + 31, 1, ATLAS_X + 384 );
    if( !lost ) line_test( "reversed", TX + 31, TY + 31, TX, TY, 1, ATLAS_X + 384 );
    if( !lost ) line_test( "wrapdown", TX, TY, TX + 31, TY, 1, ATLAS_X + 16 );
    if( !lost ) line_test( "wrapup", TX, TY, TX + 31, TY, 0, ATLAS_X + 16 );
    fprintf( out, "waits that ran out: %d\n", lost );
    if( lost ) {
        fprintf( out, "stopped: %s  ext_fifo %04x gp_stat %04x subsys_stat %04x\n",
                 where, seen_ext, seen_stat, seen_sub );
        log_commit();
        /* The engine ignores writes until it is reset; the set-up goes with it. */
        outpw( SUBSYS_CNTL, 0x900F );
        outpw( SUBSYS_CNTL, 0x500F );
        lost = 0;
        wait_idle();
        fprintf( out, "after reset: ext_fifo %04x gp_stat %04x subsys_stat %04x\n",
                 inpw( EXT_FIFO ), inpw( GP_STAT ), inpw( SUBSYS_STAT ) );
        log_commit();
    }

    room( 4 );
    outpw( DP_CONFIG, DP_PLAIN );
    outpw( ALU_FG_FN, MIX_REPLACE );
    outpw( LINEDRAW_OPT, 0 );
    rect( 0, 0, SCR_W, SCR_H, 0 );
    rect( ATLAS_X, ATLAS_Y, ATLAS_W, RAMP_Y + 2, 0 );
    rect( SCRATCH_X, SCRATCH_Y, 352, 256, 0 );
    wait_idle();
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
