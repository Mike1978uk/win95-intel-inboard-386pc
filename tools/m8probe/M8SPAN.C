/* M8SPAN.EXE - what the Mach8 engine costs per span and per line vertex, the
 * numbers that rank the 3D approaches in docs/mach8_3d_research.md:
 *   A  one span per row (CUR_Y, CUR_X, SCAN_TO_X) at widths 8, 32, 128, 320
 *   B  ten spans along one row (FRGD_COLOR, SCAN_TO_X each)
 *   C  a LINEDRAW polyline, 20-pixel segments (two writes a vertex)
 *   D  a filled triangle by the polygon fill: its outline drawn with
 *      LINEDRAW in POLY_MODE into off-screen rows (from y 700), then the
 *      extended blit with DP_CONFIG bit 1 fills it onto the screen (M8PF's
 *      sequence, tools/m8seq/M8PF.ASM); 20x20 and 200x200
 *   G  palette rotation: 64 DAC entries rewritten (one index, 192 bytes)
 *   E  a pan: CRT_OFFSET_LO/HI (4-byte units, 1024-pixel rows), two writes
 *   F  vertical blanks counted for a second (DISP_STAT bit 1), and a flip
 *      timed from the wait to the offset written
 * Arms A-C draw 2000 operations into x 100..500, y 100..400, best of three
 * timings (QueryPerformanceCounter, 0.84 us; interrupts stay on in Windows,
 * so the best run is the least disturbed). The FIFO is checked empty once per
 * 16 writes at most. Then the driver is asked to set the card up again
 * (ExtEscape 0x4D3E), as the drawing went behind its back; "M8SPAN k" leaves
 * the drawing on screen instead.
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

/* D: one triangle, bounding box w x h at (X0, Y0), outline at (0, 700). */
static void tri( int w, int h, int c )
{
    room( 9 );
    /* Clear the outline box. */
    outpw( 0xCEEE, 0x2211 );
    outpw( 0xA6E8, 0x0000 );
    outpw( 0x86E8, 0 );
    outpw( 0x82E8, 700 );
    outpw( 0x96E8, w - 1 );             /* MAJ_AXIS_PCNT */
    outpw( 0xBEE8, h - 1 );             /* MIN_AXIS_PCNT */
    outpw( 0x9AE8, 0x40F3 );            /* CMD: fill */
    outpw( 0xA6E8, 0x00FF );
    outpw( 0xA2EE, 0x0006 );            /* LINEDRAW_OPT: POLY_MODE, LAST_PEL_OFF */
    room( 9 );
    outpw( 0x9AEE, 0 );                 /* LINEDRAW_INDEX: move to */
    outpw( 0xFEEE, 0 );
    outpw( 0xFEEE, 700 );
    outpw( 0xFEEE, w - 1 );
    outpw( 0xFEEE, 700 + h / 2 );
    outpw( 0xFEEE, w / 3 );
    outpw( 0xFEEE, 700 + h - 1 );
    outpw( 0xFEEE, 0 );
    outpw( 0xFEEE, 700 );
    room( 14 );
    outpw( 0xAEE8, 0x00FF );            /* RD_MASK */
    outpw( 0xCEEE, 0x2213 );            /* DP_CONFIG: poly fill from the source */
    outpw( 0xBAEE, 0x0007 );
    outpw( 0xA6E8, c );
    outpw( 0x8EE8, 0 );                 /* source x, y */
    outpw( 0x8AE8, 700 );
    outpw( 0xB2EE, 0 );                 /* SRC_X_START, SRC_X_END, SRC_Y_DIR */
    outpw( 0xBEEE, w );
    outpw( 0xC2EE, 1 );
    outpw( 0x86E8, X0 );                /* destination */
    outpw( 0xA6EE, X0 );
    outpw( 0xAAEE, X0 + w );
    outpw( 0x82E8, Y0 );
    outpw( 0xAEEE, Y0 + h );            /* DEST_Y_END: starts the blit */
}

static int d_w;
static double arm_d( void )
{
    double  t0;
    int     i;

    setup();
    t0 = now_us();
    for( i = 0; i < 200; ++i ) {
        tri( d_w, d_w, 0x20 + (i & 15) );
    }
    wait_idle();
    outpw( 0xCEEE, 0x2211 );
    return( (now_us() - t0) / 200 );
}

/* G: 64 palette entries rotated, as a colour-cycling frame would. */
static double arm_g( void )
{
    double  t0;
    int     i;
    int     k;

    t0 = now_us();
    for( i = 0; i < 100; ++i ) {
        outp( 0x02EC, 0x40 );           /* DAC_W_INDEX */
        for( k = 0; k < 64; ++k ) {
            outp( 0x02ED, (k + i) & 63 );
            outp( 0x02ED, 63 - ((k + i) & 63) );
            outp( 0x02ED, ((k + i) * 3) & 63 );
        }
    }
    return( (now_us() - t0) / 100 );
}

static void offset( long px )
{
    long o = px >> 2;

    outpw( 0x2EEE, (WORD)((o >> 16) & 0xF ) );    /* CRT_OFFSET_HI */
    outpw( 0x2AEE, (WORD)(o & 0xFFFF) );          /* CRT_OFFSET_LO */
}

/* E: a pan, one row down each time. */
static double arm_e( void )
{
    double  t0;
    int     i;

    t0 = now_us();
    for( i = 0; i < 400; ++i ) {
        offset( 1024L * (i % 200) );
    }
    offset( 0 );
    return( (now_us() - t0) / 400 );
}

/* F: vertical syncs seen in a second (the refresh), and the time from one
 * starting to a flip written. The sync pulse is short, so its edge is polled
 * in a tight loop and the clock read once a frame (QueryPerformanceCounter is
 * slow on Windows 95). */
static void arm_f( void )
{
    double  t0;
    double  t;
    double  flip = 0.0;
    int     n = 0;
    long    k;

    t0 = now_us();
    do {
        for( k = 0; k < 200000L && (inpw( 0x02E8 ) & 2); ++k ) {
        }
        for( k = 0; k < 200000L && !(inpw( 0x02E8 ) & 2); ++k ) {
        }
        t = now_us();
        offset( (n & 1) ? 1024L * 400 : 0 );
        flip += now_us() - t;
        ++n;
    } while( t - t0 < 1e6 );
    offset( 0 );
    printf( "F vertical syncs in 1 s       %7d   (flip %.2f us)\n", n, flip / n );
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

int main( int argc, char **argv )
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
    d_w = 20;
    printf( "D polygon fill, 20x20         %7.2f us/triangle\n", best3( arm_d ) );
    d_w = 200;
    printf( "D polygon fill, 200x200       %7.2f us/triangle\n", best3( arm_d ) );
    printf( "G palette, 64 entries         %7.2f us/frame\n", best3( arm_g ) );
    printf( "E pan (CRT offset)            %7.2f us/pan\n", best3( arm_e ) );
    arm_f();
    if( argc > 1 && argv[1][0] == 'k' ) {
        printf( "kept on screen: no resync\n" );
        return( 0 );
    }
    dc = GetDC( NULL );
    printf( "resync %d\n", ExtEscape( dc, 0x4D3E, 0, NULL, 0, NULL ) );
    ReleaseDC( NULL, dc );
    return( 0 );
}
