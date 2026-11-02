/* M8SPAN.EXE - what the Mach8 engine costs per span and per line vertex, the
 * numbers that rank the 3D approaches in docs/mach8_3d_research.md:
 *   A  one span per row (CUR_Y, CUR_X, SCAN_TO_X) at widths 8, 32, 128, 320
 *   B  ten spans along one row (FRGD_COLOR, SCAN_TO_X each)
 *   C  a LINEDRAW polyline, 20-pixel segments (two writes a vertex)
 * Each arm draws 2000 operations into x 100..500, y 100..400, best of three
 * timings (QueryPerformanceCounter, 0.84 us; interrupts stay on in Windows,
 * so the best run is the least disturbed). The FIFO is checked empty once per
 * 16 writes at most. Then the driver is asked to set the card up again
 * (ExtEscape 0x4D3E), as the drawing went behind its back.
 * Build: wcl386 -q -bt=nt -l=nt -ox -fe=M8SPAN.EXE M8SPAN.C
 */
#include <windows.h>
#include <stdio.h>
#include <conio.h>

#define N       2000
#define X0      100
#define Y0      100
#define ROWS    300

static LARGE_INTEGER    freq;
static int              queued;         /* writes since the FIFO was last seen empty */

static double now_us( void )
{
    LARGE_INTEGER t;

    QueryPerformanceCounter( &t );
    return( (double)t.QuadPart * 1e6 / (double)freq.QuadPart );
}

static int wait_idle( void )
{
    long n;

    for( n = 0; n < 200000L; ++n ) {
        if( inpw( 0x9AEE ) == 0 && !(inpw( 0x9AE8 ) & 0x0200) ) {
            queued = 0;
            return( 1 );
        }
    }
    printf( "engine not idle: GP_STAT %04x EXT_FIFO %04x\n", inpw( 0x9AE8 ), inpw( 0x9AEE ) );
    return( 0 );
}

/* Room for k more writes: the 16-entry FIFO is waited on only when needed. */
static void room( int k )
{
    if( queued + k > 16 ) {
        while( inpw( 0x9AEE ) != 0 ) {
        }
        queued = 0;
    }
    queued += k;
}

static void setup( void )
{
    wait_idle();
    outpw( 0xAAE8, 0x00FF );            /* WRT_MASK */
    outpw( 0xAEE8, 0x00FF );            /* RD_MASK */
    outpw( 0xBEE8, 0xA000 );            /* PIX_CNTL */
    outpw( 0xDAEE, 0x0800 );            /* EXT_SCISSOR_L..B: the whole range */
    outpw( 0xDEEE, 0x0800 );
    outpw( 0xE2EE, 0x07FF );
    outpw( 0xE6EE, 0x07FF );
    outpw( 0xCEEE, 0x2211 );            /* DP_CONFIG: FRGD_COLOR source */
    outpw( 0xBAEE, 0x0007 );            /* ALU_FG_FN: replace */
    outpw( 0xA6E8, 0x0010 );            /* FRGD_COLOR */
    wait_idle();
}

static double arm_a( int w )
{
    double  t0;
    int     i;
    int     y;

    setup();
    t0 = now_us();
    for( i = 0; i < N; ++i ) {
        y = Y0 + i % ROWS;
        room( 3 );
        outpw( 0x82E8, y );             /* CUR_Y */
        outpw( 0x86E8, X0 );            /* CUR_X */
        outpw( 0xCAEE, X0 + w );        /* SCAN_TO_X */
    }
    wait_idle();
    return( (now_us() - t0) / N );
}

static double arm_b( void )
{
    double  t0;
    int     i;
    int     k;
    int     y;

    setup();
    t0 = now_us();
    for( i = 0; i < N / 10; ++i ) {
        y = Y0 + i % ROWS;
        room( 2 );
        outpw( 0x82E8, y );
        outpw( 0x86E8, X0 );
        for( k = 0; k < 10; ++k ) {
            room( 2 );
            outpw( 0xA6E8, 0x10 + k );          /* FRGD_COLOR */
            outpw( 0xCAEE, X0 + 30 * (k + 1) ); /* SCAN_TO_X */
        }
    }
    wait_idle();
    return( (now_us() - t0) / N );
}

static double arm_c( void )
{
    double  t0;
    int     i;

    setup();
    t0 = now_us();
    room( 4 );
    outpw( 0xA2EE, 0x0004 );            /* LINEDRAW_OPT: LAST_PEL_OFF */
    outpw( 0x9AEE, 0 );                 /* LINEDRAW_INDEX: move to */
    outpw( 0xFEEE, X0 );
    outpw( 0xFEEE, Y0 );
    for( i = 1; i < N; ++i ) {
        room( 2 );
        outpw( 0xFEEE, X0 + (i & 1) * 20 + (i / 2) % 300 );
        outpw( 0xFEEE, Y0 + (i * 7) % ROWS );
    }
    wait_idle();
    return( (now_us() - t0) / N );
}

static double best3( double (*f)( void ) )
{
    double  b = 1e30;
    double  t;
    int     k;

    for( k = 0; k < 3; ++k ) {
        t = f();
        if( t < b ) {
            b = t;
        }
    }
    return( b );
}

static int a_w;
static double arm_a_w( void )
{
    return( arm_a( a_w ) );
}

int main( void )
{
    static const int widths[] = { 8, 32, 128, 320 };
    HDC     dc;
    int     i;

    QueryPerformanceFrequency( &freq );
    for( i = 0; i < 4; ++i ) {
        a_w = widths[i];
        printf( "A span per row, width %3d   %7.2f us/span\n", widths[i], best3( arm_a_w ) );
    }
    printf( "B ten spans along a row      %7.2f us/span\n", best3( arm_b ) );
    printf( "C polyline, 20 px segments   %7.2f us/vertex\n", best3( arm_c ) );
    dc = GetDC( NULL );
    printf( "resync %d\n", ExtEscape( dc, 0x4D3E, 0, NULL, 0, NULL ) );
    ReleaseDC( NULL, dc );
    return( 0 );
}
