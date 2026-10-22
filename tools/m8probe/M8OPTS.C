/* M8OPTS.EXE - two ATIM8 register values our driver does not use, timed on the
 * card (port census, docs/atim8_port_census.md):
 *  - LINEDRAW_OPT (A2EEh): we write 0004h; ATIM8's polylines write 070Ch
 *    (seg08:01e8), adding bit 3 (not in the guide), BOUNDS_RESET and a clip
 *    mode. 2000 lines with each value, timed, and one line read back.
 *  - A pixel set: ours is a 1x1 fill (FRGD_MIX, FRGD_COLOR, CUR_X, CUR_Y,
 *    MAJ_AXIS, MULTIFUNC, CMD); ATIM8's (seg07:0066) is DP_CONFIG 4215h, the
 *    ALU functions, CUR_Y, CUR_X, SCAN_TO_X and the colour in the high byte of
 *    one PIX_TRANS write. 2000 pixels each way, timed, and read back.
 * Draws in a 64x64 patch at the screen's top-left; Windows repaints it.
 *   M8OPTS d   draws one polyline with each LINEDRAW_OPT value and prints it
 * Build: wcl386 -q -bt=nt -l=nt -ox -fe=M8OPTS.EXE M8OPTS.C
 */
#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <string.h>

#define N   2000

static LARGE_INTEGER freq;

static double now_ms( void )
{
    LARGE_INTEGER t;

    QueryPerformanceCounter( &t );
    return( t.QuadPart * 1000.0 / freq.QuadPart );
}

static void wait_idle( void )
{
    long    n;

    for( n = 0; n < 400000L; ++n ) {
        if( inpw( 0x9AEE ) == 0 && !(inpw( 0x9AE8 ) & 0x0200) ) {
            return;
        }
    }
    printf( "engine not idle: GP_STAT %04x EXT_FIFO %04x\n", inpw( 0x9AE8 ), inpw( 0x9AEE ) );
}

/* Room for n writes: the top n bits of EXT_FIFO_STATUS clear. */
static void room( int n )
{
    WORD mask = (WORD)~(0xFFFFu >> n);

    while( inpw( 0x9AEE ) & mask ) {
    }
}

static void plain( void )
{
    wait_idle();
    outpw( 0xAAE8, 0x00FF );            /* WRT_MASK */
    outpw( 0xAEE8, 0x00FF );            /* RD_MASK */
    outpw( 0xBEE8, 0xA000 );            /* PIX_CNTL */
    outpw( 0xDAEE, 0x0800 );            /* EXT_SCISSOR_L..B: the whole range */
    outpw( 0xDEEE, 0x0800 );
    outpw( 0xE2EE, 0x07FF );
    outpw( 0xE6EE, 0x07FF );
    outpw( 0xCEEE, 0x2211 );
}

/* Lines from (0,0) to (63, i & 63) through LINEDRAW, as the driver draws them. */
static double lines( WORD opt )
{
    double  t0;
    int     i;

    plain();
    outpw( 0xBAE8, 0x0027 );            /* FRGD_MIX: FRGD_COLOR, replace */
    outpw( 0xA6E8, 0x0F );
    t0 = now_ms();
    for( i = 0; i < N; ++i ) {
        room( 5 );
        outpw( 0xA2EE, opt );           /* LINEDRAW_OPT */
        outpw( 0x9AEE, 0 );             /* LINEDRAW_INDEX: next is a move */
        outpw( 0xFEEE, 0 );
        outpw( 0xFEEE, 0 );
        room( 2 );
        outpw( 0xFEEE, 63 );
        outpw( 0xFEEE, i & 63 );
    }
    wait_idle();
    return( now_ms() - t0 );
}

static double set_ours( void )
{
    double  t0;
    int     i;

    plain();
    t0 = now_ms();
    for( i = 0; i < N; ++i ) {
        room( 7 );
        outpw( 0xBAE8, 0x0027 );        /* FRGD_MIX */
        outpw( 0xA6E8, i & 0xFF );      /* FRGD_COLOR */
        outpw( 0x86E8, i & 63 );        /* CUR_X */
        outpw( 0x82E8, (i >> 6) & 63 ); /* CUR_Y */
        outpw( 0x96E8, 0 );             /* MAJ_AXIS_PCNT */
        outpw( 0xBEE8, 0x0000 );        /* MULTIFUNC: MIN_AXIS 0 */
        outpw( 0x9AE8, 0x40F3 );        /* CMD: fill */
    }
    wait_idle();
    return( now_ms() - t0 );
}

static double set_atim8( void )
{
    double  t0;
    int     i;

    plain();
    t0 = now_ms();
    for( i = 0; i < N; ++i ) {
        room( 7 );
        outpw( 0xCEEE, 0x4215 );        /* DP_CONFIG: PIX_TRANS, write */
        outpw( 0xBAEE, 0x0007 );        /* ALU_FG_FN: replace */
        outpw( 0xB6EE, 0x0001 );        /* ALU_BG_FN */
        outpw( 0x82E8, (i >> 6) & 63 );
        outpw( 0x86E8, i & 63 );
        outpw( 0xCAEE, (i & 63) + 1 );  /* SCAN_TO_X */
        outpw( 0xE2E8, (WORD)((i & 0xFF) << 8) );
    }
    wait_idle();
    return( now_ms() - t0 );
}

/* The 64x24 top-left patch: # for colour 0Fh, . for anything else. */
static void dump( void )
{
    WORD    px[32];
    long    n;
    int     r;
    int     c;

    plain();
    outpw( 0xBAE8, 0x0047 );
    outpw( 0x86E8, 0 );
    outpw( 0x82E8, 0 );
    outpw( 0x96E8, 63 );
    outpw( 0xBEE8, 23 );
    outpw( 0x9AE8, 0x53B0 );
    for( n = 0; n < 200000L && !(inpw( 0x9AE8 ) & 0x0100); ++n ) {
    }
    for( r = 0; r < 24; ++r ) {
        printf( "    " );
        for( c = 0; c < 32; ++c ) {
            px[c] = inpw( 0xE2E8 );
        }
        for( c = 0; c < 64; ++c ) {
            putchar( (BYTE)(c & 1 ? px[c / 2] >> 8 : px[c / 2]) == 0x0F ? '#' : '.' );
        }
        putchar( '\n' );
    }
}

static void clear( void )
{
    plain();
    outpw( 0xBAE8, 0x0027 );
    outpw( 0xA6E8, 0x00 );
    outpw( 0x86E8, 0 );
    outpw( 0x82E8, 0 );
    outpw( 0x96E8, 63 );
    outpw( 0xBEE8, 63 );
    outpw( 0x9AE8, 0x40F3 );
    wait_idle();
}

/* 64 pixels of row y as a checksum, read back through the 8514 host read. */
static DWORD row_sum( int y )
{
    DWORD   s = 0;
    long    n;
    int     i;

    plain();
    outpw( 0xBAE8, 0x0047 );            /* FRGD_MIX: host */
    outpw( 0x86E8, 0 );
    outpw( 0x82E8, y );
    outpw( 0x96E8, 63 );
    outpw( 0xBEE8, 0 );
    outpw( 0x9AE8, 0x53B0 );
    for( n = 0; n < 200000L && !(inpw( 0x9AE8 ) & 0x0100); ++n ) {
    }
    for( i = 0; i < 32; ++i ) {
        s = s * 31 + inpw( 0xE2E8 );
    }
    return( s );
}

int main( void )
{
    double  t;

    QueryPerformanceFrequency( &freq );
    if( GetCommandLine() != NULL && strstr( GetCommandLine(), " d" ) != NULL ) {
        static const WORD opts[] = { 0x0004, 0x000C, 0x070C, 0x0008 };
        int k;

        for( k = 0; k < 4; ++k ) {
            clear();
            plain();
            outpw( 0xBAE8, 0x0027 );
            outpw( 0xA6E8, 0x0F );
            outpw( 0xA2EE, opts[k] );
            outpw( 0x9AEE, 0 );
            outpw( 0xFEEE, 2 );
            outpw( 0xFEEE, 2 );
            outpw( 0xFEEE, 60 );
            outpw( 0xFEEE, 20 );
            outpw( 0xFEEE, 10 );
            outpw( 0xFEEE, 22 );
            wait_idle();
            printf( "LINEDRAW_OPT %04x, polyline (2,2)-(60,20)-(10,22):\n", opts[k] );
            dump();
        }
        outpw( 0xA2EE, 0x0004 );
        return( 0 );
    }
    t = lines( 0x0004 );
    printf( "lines LINEDRAW_OPT 0004: %7.2f ms for %d, row 20 %08lx\n", t, N, row_sum( 20 ) );
    t = lines( 0x000C );
    printf( "lines LINEDRAW_OPT 000C: %7.2f ms for %d, row 20 %08lx\n", t, N, row_sum( 20 ) );
    t = lines( 0x070C );
    printf( "lines LINEDRAW_OPT 070C: %7.2f ms for %d, row 20 %08lx\n", t, N, row_sum( 20 ) );
    t = set_ours();
    printf( "pixel set, 1x1 fill:     %7.2f ms for %d, row 5 %08lx\n", t, N, row_sum( 5 ) );
    t = set_atim8();
    printf( "pixel set, ATIM8 4215:   %7.2f ms for %d, row 5 %08lx\n", t, N, row_sum( 5 ) );
    outpw( 0xCEEE, 0x2211 );
    outpw( 0xA2EE, 0x0004 );
    return( 0 );
}
