/* M8CURVE: what a curve costs on the Mach8's 2D engine, one flat colour, 640x480.
 *
 * Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
 *
 * The same circle five ways, radius 30, 60 and 120, fastest of REPS x PASSES:
 *   rows      one rectangle command a row: what GDI's span list comes to
 *   scan      one row at a time by SCAN_TO_X: GDI's span list as the driver sends it
 *   chords    outline as NCHORD straight lines, end to end
 *   tangents  outline as NCHORD lines tangent to the circle, each as long as the circle is wide:
 *             the string-art way, where the curve is what the lines leave behind
 *   poly      chord edges XORed into a plane, then one polygon fill of the box
 * The parabola of a string-art figure is timed as well, NCHORD lines between two axes. Each arm's
 * pixels stay on screen for HOLD_MS, for the owner's eye and the agent's screenshot.
 * Writes C:\M8CURVE.TXT.
 *
 * Build: wcl386 -q -bt=nt -l=nt_win -ox -i=<watcom>h -i=<watcom>h\nt -i=<watcom>h\nt\directx
 *        "-fe=M8CURVE.EXE" M8CURVE.C ddraw.lib
 */

#define INITGUID
#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <unistd.h>
#include <math.h>
#include <ddraw.h>

#define SCR_W           640
#define SCR_H           480
#define SCRATCH_X       640
#define SCRATCH_Y       512
#define NCHORD          32
#define NTANGENT        32
#define PASSES          20
#define REPS            3
#define HOLD_MS         800

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
#define LINEDRAW_INDEX  0x9AEE
#define LINEDRAW_OPT    0xA2EE
#define DEST_X_START    0xA6EE
#define DEST_X_END      0xAAEE
#define DEST_Y_END      0xAEEE
#define SRC_X_START     0xB2EE
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
#define MIX_XOR         5
#define MIX_REPLACE     7
#define OPT_LAST_OFF    4
#define OPT_POLY        2
#define ESC_RESYNC      0x4D3E
#define PLANE           0x0001
#define SUBSYS_CNTL     0x42E8
#define SUBSYS_STAT     0x42E8
#define WAIT_TRIES      20000L

static FILE     *out;
static double   qpf;
static int      lost;
static const char *where = "start";
static int      at_rep;
static int      at_pass;
static int      seen_ext;
static int      seen_stat;
static int      seen_sub;
static int      rad;
static int      cx;
static int      cy;
static short    half[256];                  /* half-width of each row from the centre */
static short    chord[NCHORD + 1][2];
static short    tang[NTANGENT][4];
static short    para[NCHORD + 1][4];

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

/* Longer than any command here takes: a wait that runs out means the engine has stopped, and one
 * row at a time would otherwise spin for hours with the driver stuck behind it. */
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

/* x*x + y*y = r*r walked by differences: no square root, one addition a row. */
static void make_rows( int r )
{
    long    x = r;
    long    y;
    long    d = 0;

    for( y = 0; y <= r; ++y ) {
        if( y > 0 ) {
            d += 2 * y - 1;
        }
        while( d > 0 ) {
            d -= 2 * x - 1;
            --x;
        }
        half[y] = (short)x;
    }
}

static void make_lines( int r )
{
    int     i;
    double  a;

    for( i = 0; i <= NCHORD; ++i ) {
        a = 6.2831853 * i / NCHORD;
        chord[i][0] = (short)(cx + r * cos( a ) + 0.5);
        chord[i][1] = (short)(cy + r * sin( a ) + 0.5);
    }
    for( i = 0; i < NTANGENT; ++i ) {
        double  c;
        double  s;

        a = 6.2831853 * i / NTANGENT;
        c = cos( a );
        s = sin( a );
        tang[i][0] = (short)(cx + r * c - r * s + 0.5);
        tang[i][1] = (short)(cy + r * s + r * c + 0.5);
        tang[i][2] = (short)(cx + r * c + r * s + 0.5);
        tang[i][3] = (short)(cy + r * s - r * c + 0.5);
    }
    /* Two axes meeting at the corner: each line joins a point on one to a point on the other. */
    for( i = 0; i <= NCHORD; ++i ) {
        para[i][0] = (short)(cx - r);
        para[i][1] = (short)(cy - r + 2 * r * i / NCHORD);
        para[i][2] = (short)(cx - r + 2 * r * i / NCHORD);
        para[i][3] = (short)(cy + r);
    }
}

static void line_setup( int colour, int wrt, int mix, int opt )
{
    room( 5 );
    outpw( WRT_MASK, wrt );
    outpw( FRGD_COLOR, colour );
    outpw( DP_CONFIG, DP_PLAIN );
    outpw( ALU_FG_FN, mix );
    outpw( LINEDRAW_OPT, opt );
}

static void line( int x0, int y0, int x1, int y1 )
{
    room( 5 );
    outpw( LINEDRAW_INDEX, 0 );
    outpw( LINEDRAW, x0 );
    outpw( LINEDRAW, y0 );
    outpw( LINEDRAW, x1 );
    outpw( LINEDRAW, y1 );
}

static void draw_rows( int colour )
{
    int y;
    int w;

    room( 6 );
    outpw( WRT_MASK, 0x00FF );
    outpw( RD_MASK, 0x00FF );
    outpw( DP_CONFIG, DP_PLAIN );
    outpw( FRGD_MIX, 0x0027 );
    outpw( FRGD_COLOR, colour );
    outpw( MULTIFUNC_CNTL, 0 );
    for( y = -rad; y <= rad; ++y ) {
        w = half[y < 0 ? -y : y];
        room( 4 );
        outpw( CUR_X, cx - w );
        outpw( CUR_Y, cy + y );
        outpw( MAJ_AXIS_PCNT, 2 * w );
        outpw( CMD, 0x40F3 );
    }
}

static void draw_scan( int colour )
{
    int y;
    int w;

    room( 7 );
    outpw( WRT_MASK, 0x00FF );
    outpw( RD_MASK, 0x00FF );
    outpw( DP_CONFIG, DP_PLAIN );
    outpw( ALU_FG_FN, MIX_REPLACE );
    outpw( FRGD_COLOR, colour );
    for( y = -rad; y <= rad; ++y ) {
        w = half[y < 0 ? -y : y];
        room( 3 );
        outpw( CUR_Y, cy + y );
        outpw( CUR_X, cx - w );
        outpw( SCAN_TO_X, cx + w + 1 );
    }
}

static void draw_chords( int colour )
{
    int i;

    line_setup( colour, 0x00FF, MIX_REPLACE, OPT_LAST_OFF );
    for( i = 0; i < NCHORD; ++i ) {
        line( chord[i][0], chord[i][1], chord[i + 1][0], chord[i + 1][1] );
    }
}

static void draw_tangents( int colour )
{
    int i;

    line_setup( colour, 0x00FF, MIX_REPLACE, 0 );
    for( i = 0; i < NTANGENT; ++i ) {
        line( tang[i][0], tang[i][1], tang[i][2], tang[i][3] );
    }
}

static void draw_parabola( int colour )
{
    int i;

    line_setup( colour, 0x00FF, MIX_REPLACE, 0 );
    for( i = 0; i <= NCHORD; ++i ) {
        line( para[i][0], para[i][1], para[i][2], para[i][3] );
    }
}

/* Edges drawn top to bottom without their last row, so each row holds a pair: rows inside fill. */
static void draw_poly( int colour )
{
    int i;
    int ox = SCRATCH_X - (cx - rad);
    int oy = SCRATCH_Y - (cy - rad);

    rect( SCRATCH_X, SCRATCH_Y, 2 * rad + 2, 2 * rad + 2, 0 );
    line_setup( 0x00FF, PLANE, MIX_XOR, OPT_POLY | OPT_LAST_OFF );
    for( i = 0; i < NCHORD; ++i ) {
        int a = i;
        int b = i + 1;

        if( chord[a][1] == chord[b][1] ) {
            continue;
        }
        if( chord[a][1] > chord[b][1] ) {
            int t = a;

            a = b;
            b = t;
        }
        line( ox + chord[a][0], oy + chord[a][1], ox + chord[b][0], oy + chord[b][1] );
    }
    room( 15 );
    outpw( WRT_MASK, 0x00FF );
    outpw( RD_MASK, PLANE );
    outpw( DP_CONFIG, DP_FILL_FLAT );
    outpw( ALU_FG_FN, MIX_REPLACE );
    outpw( FRGD_COLOR, colour );
    outpw( SRC_X, SCRATCH_X );
    outpw( SRC_Y, SCRATCH_Y + 1 );
    outpw( SRC_X_START, SCRATCH_X );
    outpw( SRC_X_END, SCRATCH_X + 2 * rad + 1 );
    outpw( SRC_Y_DIR, 1 );
    outpw( CUR_X, cx - rad );
    outpw( DEST_X_START, cx - rad );
    outpw( DEST_X_END, cx + rad + 1 );
    outpw( CUR_Y, cy - rad + 1 );
    outpw( DEST_Y_END, cy + rad );
    room( 1 );
    outpw( RD_MASK, 0x00FF );
}

static void arm( const char *name, void (*draw)( int ), int colour )
{
    double  best = 1e30;
    double  t0;
    double  t;
    int     rep;
    int     p;

    where = name;
    for( rep = 0; rep < REPS && !lost; ++rep ) {
        at_rep = rep;
        rect( 0, 0, SCR_W, SCR_H, 0 );
        wait_idle();
        t0 = now();
        for( p = 0; p < PASSES && !lost; ++p ) {
            at_pass = p;
            draw( colour );
        }
        wait_idle();
        t = (now() - t0) * 1e6 / qpf / PASSES;
        fprintf( out, "r %3d  %s rep %d %.1f us\n", rad, name, rep, t );
        log_commit();
        if( t < best ) {
            best = t;
        }
    }
    if( lost ) {
        return;
    }
    fprintf( out, "r %3d  %-9s %9.1f us\n", rad, name, best );
    log_commit();
    Sleep( HOLD_MS );
}

static LRESULT CALLBACK wndproc( HWND w, UINT m, WPARAM a, LPARAM b )
{
    return( DefWindowProc( w, m, a, b ) );
}

int PASCAL WinMain( HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show )
{
    static const int    radii[3] = { 30, 60, 120 };
    WNDCLASS            wc;
    LPDIRECTDRAW        dd;
    HWND                wnd;
    HRESULT             hr;
    HDC                 dc;
    LARGE_INTEGER       f;
    int                 i;

    (void)prev;
    (void)show;
    out = fopen( "C:\\M8CURVE.TXT", "a" );
    if( out == NULL ) {
        return( 1 );
    }
    QueryPerformanceFrequency( &f );
    qpf = (double)f.LowPart + 4294967296.0 * (double)f.HighPart;
    memset( &wc, 0, sizeof( wc ) );
    wc.lpfnWndProc = wndproc;
    wc.hInstance = inst;
    wc.lpszClassName = "M8CURVE";
    RegisterClass( &wc );
    wnd = CreateWindowEx( WS_EX_TOPMOST, "M8CURVE", "M8CURVE", WS_POPUP | WS_VISIBLE, 0, 0, SCR_W, SCR_H,
                          NULL, NULL, inst, NULL );
    hr = DirectDrawCreate( NULL, &dd, NULL );
    fprintf( out, "== M8CURVE %s  ddraw %08lx\n", cmd, (unsigned long)hr );
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
    cx = SCR_W / 2;
    cy = SCR_H / 2;
    for( i = 0; i < 3 && !lost; ++i ) {
        rad = radii[i];
        make_rows( rad );
        make_lines( rad );
        arm( "rows", draw_rows, 0xFF );
        arm( "scan", draw_scan, 0xFF );
        arm( "chords", draw_chords, 0xFF );
        arm( "tangents", draw_tangents, 0xFF );
        arm( "parabola", draw_parabola, 0xFF );
        arm( "poly", draw_poly, 0xFF );
    }
    fprintf( out, "waits that ran out: %d\n", lost );
    if( lost ) {
        fprintf( out, "stopped: r %d %s rep %d pass %d  ext_fifo %04x gp_stat %04x subsys_stat %04x\n",
                 rad, where, at_rep, at_pass, seen_ext, seen_stat, seen_sub );
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

    rect( 0, 0, SCR_W, SCR_H, 0 );
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
