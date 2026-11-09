/* M8LATCH: can engine writes share one low byte on the 8-bit bus? Card only, 640x480.
 *
 * Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
 *
 * On this bus a word OUT is two byte cycles; the card holds the low byte in one latch for all
 * ports and the high (odd) byte commits the word (docs/captures/2026-10-08_m8byte/README.md).
 * If the latch keeps its byte after a commit, a run of writes with the same low byte costs one
 * low OUT and then one high OUT each. Each test sets registers with lone bytes, draws, and the
 * screen is searched for where the ink landed:
 *   t1  low 40h to CUR_X, high 01h to CUR_X+1                     span at x 140h, row 100
 *   t2  low 50h once; high 00h to CUR_Y+1, high 01h to CUR_X+1   span at x 150h, row 50h: the latch survives a commit
 *   t3  low 60h; high 00h to CUR_Y+1; word FRGD_COLOR 0023h; high 01h to CUR_X+1
 *                                                                x 160h: a word between leaves it; 123h: the word loads it
 *   t4  LINEDRAW with every coordinate's low byte 80h: low once, four highs: 180h,80h -> 80h,80h
 *                                                                a line on row 80h from x 80h to 180h
 * Then time: 4,096 words to FRGD_COLOR against one low byte and 4,096 high bytes, FIFO room
 * read every 16. Writes C:\M8LATCH.TXT.
 *
 * Build: wcl386 -q -bt=nt -l=nt_win -ox -i=<watcom>h -i=<watcom>h\nt -i=<watcom>h\nt\directx
 *        "-fe=M8LATCH.EXE" M8LATCH.C ddraw.lib
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
#define INK             0x23
#define WAIT_TRIES      20000L
#define SEARCH_H        200
#define TIME_N          4096

#define GP_STAT         0x9AE8
#define CMD             0x9AE8
#define EXT_FIFO        0x9AEE
#define CUR_Y           0x82E8
#define CUR_X           0x86E8
#define MAJ_AXIS_PCNT   0x96E8
#define FRGD_COLOR      0xA6E8
#define WRT_MASK        0xAAE8
#define RD_MASK         0xAEE8
#define FRGD_MIX        0xBAE8
#define MULTIFUNC_CNTL  0xBEE8
#define PIX_TRANS       0xE2E8
#define SUBSYS_CNTL     0x42E8
#define LINEDRAW_INDEX  0x9AEE
#define LINEDRAW_OPT    0xA2EE
#define ALU_FG_FN       0xBAEE
#define DP_CONFIG       0xCEEE
#define SCISSOR_L       0xDAEE
#define SCISSOR_T       0xDEEE
#define SCISSOR_R       0xE2EE
#define SCISSOR_B       0xE6EE
#define LINEDRAW        0xFEEE

#define DP_PLAIN        0x2211
#define MIX_REPLACE     7
#define ESC_RESYNC      0x4D3E

static FILE     *out;
static double   qpf;
static int      lost;
static BYTE     row[SCR_W];

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
    ++lost;
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
    ++lost;
}

static void rect( int x, int y, int w, int h, int colour )
{
    room( 10 );
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

/* Never read PIX_TRANS without DATA_READY: that locks the FIFO. */
static void read_row( int y )
{
    long    w;
    long    t;

    memset( row, 0, sizeof( row ) );
    wait_idle();
    if( lost ) {
        return;
    }
    outpw( FRGD_MIX, 0x0047 );
    outpw( CUR_X, 0 );
    outpw( CUR_Y, y );
    outpw( MAJ_AXIS_PCNT, SCR_W - 1 );
    outpw( MULTIFUNC_CNTL, 0 );
    wait_idle();
    outpw( CMD, 0x53B0 );
    for( w = 0; w < SCR_W / 2; ++w ) {
        WORD    v;

        for( t = 0; t < WAIT_TRIES && !(inpw( GP_STAT ) & 0x0100); ++t ) {
        }
        if( t == WAIT_TRIES ) {
            ++lost;
            return;
        }
        v = inpw( PIX_TRANS );
        row[2 * w] = (BYTE)v;
        row[2 * w + 1] = (BYTE)(v >> 8);
    }
    wait_idle();
    outpw( FRGD_MIX, 0x0027 );
}

/* Where the ink is, rows 0 to SEARCH_H - 1: the box and the pixel count. */
static void find( const char *name )
{
    int     l = SCR_W;
    int     r = -1;
    int     t = -1;
    int     b = -1;
    long    n = 0;
    int     x;
    int     y;

    for( y = 0; y < SEARCH_H && !lost; ++y ) {
        read_row( y );
        for( x = 0; x < SCR_W; ++x ) {
            if( row[x] == INK ) {
                l = x < l ? x : l;
                r = x > r ? x : r;
                t = t < 0 ? y : t;
                b = y;
                ++n;
            }
        }
    }
    if( n == 0 ) {
        fprintf( out, "%-3s no ink in rows 0-%d\n", name, SEARCH_H - 1 );
    } else {
        fprintf( out, "%-3s ink x %03xh-%03xh rows %03xh-%03xh, %ld px\n", name, l, r, t, b, n );
    }
    log_commit();
}

static void clear( void )
{
    rect( 0, 0, SCR_W, SEARCH_H, 0 );
    wait_idle();
}

/* A 4 x 1 span in INK at CUR_X and CUR_Y as left by the lone bytes before it. */
static void span_setup( void )
{
    room( 8 );
    outpw( FRGD_COLOR, INK );
    outpw( CUR_Y, 100 );
    outpw( MAJ_AXIS_PCNT, 3 );
    outpw( MULTIFUNC_CNTL, 0 );
}

static void t1( void )
{
    clear();
    span_setup();
    wait_idle();
    outp( CUR_X, 0x40 );
    outp( CUR_X + 1, 0x01 );
    outpw( CMD, 0x40F3 );
    find( "t1" );
}

static void t2( void )
{
    clear();
    span_setup();
    wait_idle();
    outp( CUR_X, 0x50 );
    outp( CUR_Y + 1, 0x00 );
    outp( CUR_X + 1, 0x01 );
    outpw( CMD, 0x40F3 );
    find( "t2" );
}

static void t3( void )
{
    clear();
    span_setup();
    outpw( FRGD_COLOR, 0x00FF );
    wait_idle();
    outp( CUR_X, 0x60 );
    outp( CUR_Y + 1, 0x00 );
    outpw( FRGD_COLOR, INK );
    outp( CUR_X + 1, 0x01 );
    outpw( CMD, 0x40F3 );
    find( "t3" );
}

static void t4( void )
{
    clear();
    room( 6 );
    outpw( FRGD_COLOR, INK );
    outpw( DP_CONFIG, DP_PLAIN );
    outpw( ALU_FG_FN, MIX_REPLACE );
    outpw( LINEDRAW_OPT, 0 );
    outpw( LINEDRAW_INDEX, 0 );
    wait_idle();
    outp( LINEDRAW, 0x80 );
    outp( LINEDRAW + 1, 0x01 );
    outp( LINEDRAW + 1, 0x00 );
    outp( LINEDRAW + 1, 0x00 );
    outp( LINEDRAW + 1, 0x00 );
    find( "t4" );
}

/* us a write: words, then a low byte once and high bytes, FIFO room read every 16. */
static void timing( void )
{
    double  a;
    double  b;
    int     i;
    int     k;

    wait_idle();
    a = now();
    for( i = 0; i < TIME_N && !lost; i += 16 ) {
        room( 16 );
        for( k = 0; k < 16; ++k ) {
            outpw( FRGD_COLOR, 0x0011 );
        }
    }
    wait_idle();
    b = now();
    fprintf( out, "words      %.3f us a write\n", (b - a) * 1e6 / qpf / TIME_N );
    a = now();
    outp( FRGD_COLOR, 0x11 );
    for( i = 0; i < TIME_N && !lost; i += 16 ) {
        room( 16 );
        for( k = 0; k < 16; ++k ) {
            outp( FRGD_COLOR + 1, 0x00 );
        }
    }
    wait_idle();
    b = now();
    fprintf( out, "high bytes %.3f us a write\n", (b - a) * 1e6 / qpf / TIME_N );
    a = now();
    for( i = 0; i < TIME_N && !lost; i += 16 ) {
        room( 16 );
    }
    b = now();
    fprintf( out, "room reads alone %.3f us a 16\n", (b - a) * 1e6 / qpf / (TIME_N / 16) );
    log_commit();
}

static LRESULT CALLBACK wndproc( HWND w, UINT m, WPARAM a, LPARAM b )
{
    return( DefWindowProc( w, m, a, b ) );
}

int PASCAL WinMain( HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show )
{
    WNDCLASS            wc;
    LPDIRECTDRAW        dd;
    HWND                wnd;
    HRESULT             hr;
    HDC                 dc;
    LARGE_INTEGER       f;

    (void)prev;
    (void)show;
    out = fopen( "C:\\M8LATCH.TXT", "a" );
    if( out == NULL ) {
        return( 1 );
    }
    QueryPerformanceFrequency( &f );
    qpf = (double)f.LowPart + 4294967296.0 * (double)f.HighPart;
    memset( &wc, 0, sizeof( wc ) );
    wc.lpfnWndProc = wndproc;
    wc.hInstance = inst;
    wc.lpszClassName = "M8LATCH";
    RegisterClass( &wc );
    wnd = CreateWindowEx( WS_EX_TOPMOST, "M8LATCH", "M8LATCH", WS_POPUP | WS_VISIBLE, 0, 0, SCR_W, SCR_H,
                          NULL, NULL, inst, NULL );
    hr = DirectDrawCreate( NULL, &dd, NULL );
    fprintf( out, "== M8LATCH %s  ddraw %08lx\n", cmd, (unsigned long)hr );
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

    t1();
    t2();
    t3();
    t4();
    if( !lost ) {
        timing();
    }
    fprintf( out, "waits that ran out: %d\n", lost );
    if( lost ) {
        /* The engine ignores writes until it is reset. */
        outpw( SUBSYS_CNTL, 0x900F );
        outpw( SUBSYS_CNTL, 0x500F );
        lost = 0;
        wait_idle();
    }
    clear();
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
