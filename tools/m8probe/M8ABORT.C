/* M8ABORT.EXE - what the transfer abort ATIM8's ScanLR ends with (seg09:0194:
 * SUBSYS_CNTL 900Fh, 500Fh) leaves of the engine set-up. ATIM8 rewrites only
 * DP_CONFIG and ALU_FG_FN after it; the driver rewrites everything.
 * Row 5, x 0..31, is painted 33h. Then a set-up that shows in the pixels:
 * WRT_MASK 0Fh, scissors x 4..11, FRGD_COLOR 55h. Run A draws x 0..15 with
 * it. Run B starts a row-long SCAN_TO_X read, reads one word, aborts, then
 * draws the same span writing only DP_CONFIG, ALU_FG_FN and the coordinates.
 * Each run reads x 0..15 back (8 words, two pixels each) after restoring the full set-up and GE_PITCH.
 * A and B equal = the abort keeps WRT_MASK, the scissors, FRGD_COLOR and
 * GE_PITCH. Expected A: 33 x4, 35 x8, 33 x4. Windows repaints
 * the row.
 * Run C: a read set up in full, a row-long read aborted after one word, then
 * a second read writing only CUR_Y, CUR_X and SCAN_TO_X; 3333h x8 = the
 * abort keeps DP_CONFIG and ALU_FG_FN as well.
 * Build: wcl386 -q -bt=nt -l=nt -ox -fe=M8ABORT.EXE M8ABORT.C
 */
#include <windows.h>
#include <stdio.h>
#include <conio.h>

#define ROW     5

static int wait_idle( void )
{
    long    n;

    for( n = 0; n < 200000L; ++n ) {
        if( inpw( 0x9AEE ) == 0 && !(inpw( 0x9AE8 ) & 0x0200) ) {
            return( 1 );
        }
    }
    printf( "engine not idle: GP_STAT %04x EXT_FIFO %04x\n", inpw( 0x9AE8 ), inpw( 0x9AEE ) );
    return( 0 );
}

static int wait_data( void )
{
    long    n;

    for( n = 0; n < 200000L; ++n ) {
        if( inpw( 0x9AE8 ) & 0x0100 ) {
            return( 1 );
        }
    }
    printf( "no data ready: GP_STAT %04x\n", inpw( 0x9AE8 ) );
    return( 0 );
}

static void abort_xfer( void )
{
    outpw( 0x42E8, 0x900F );
    outpw( 0x42E8, 0x500F );
}

static void full_setup( void )
{
    wait_idle();
    outpw( 0x76EE, 0x0080 );            /* GE_PITCH */
    outpw( 0x6EEE, 0x0000 );            /* GE_OFFSET */
    outpw( 0x72EE, 0x0000 );
    outpw( 0xAAE8, 0x00FF );            /* WRT_MASK */
    outpw( 0xAEE8, 0x00FF );            /* RD_MASK */
    outpw( 0xBEE8, 0xA000 );            /* PIX_CNTL */
    outpw( 0xDAEE, 0x0800 );            /* EXT_SCISSOR_L..B: the whole range */
    outpw( 0xDEEE, 0x0800 );
    outpw( 0xE2EE, 0x07FF );
    outpw( 0xE6EE, 0x07FF );
}

static void span( int x0, int x1 )
{
    outpw( 0xCEEE, 0x2211 );
    outpw( 0xBAEE, 0x0007 );
    outpw( 0x82E8, ROW );
    outpw( 0x86E8, x0 );
    outpw( 0xCAEE, x1 );
    wait_idle();
}

static void paint( void )
{
    full_setup();
    outpw( 0xA6E8, 0x33 );              /* FRGD_COLOR */
    span( 0, 32 );
}

static void test_setup( void )
{
    wait_idle();
    outpw( 0xAAE8, 0x000F );
    outpw( 0xDAEE, 4 );
    outpw( 0xE2EE, 11 );
    outpw( 0xA6E8, 0x55 );
}

static void read_back( const char *name, WORD *w )
{
    int i;

    full_setup();
    outpw( 0xCEEE, 0x5210 );
    outpw( 0xBAEE, 0x0007 );
    outpw( 0x82E8, ROW );
    outpw( 0x86E8, 0 );
    outpw( 0xCAEE, 16 );
    printf( "%s:", name );
    if( wait_data() ) {
        for( i = 0; i < 16; ++i ) {
            w[i] = inpw( 0xE2E8 );
            printf( " %04x", w[i] );
            if( i == 7 ) {
                break;
            }
        }
    }
    printf( "  SUBSYS_STAT %04x\n", inpw( 0x42E8 ) );
    abort_xfer();
    wait_idle();
}

int main( void )
{
    WORD    a[16], b[16];
    int     i, same = 1;

    paint();
    test_setup();
    span( 0, 16 );
    read_back( "A", a );

    paint();
    test_setup();
    outpw( 0xCEEE, 0x5210 );
    outpw( 0xBAEE, 0x0007 );
    outpw( 0x82E8, ROW );
    outpw( 0x86E8, 0 );
    outpw( 0xCAEE, 800 );
    if( wait_data() ) {
        printf( "B scan word %04x\n", inpw( 0xE2E8 ) );
    }
    abort_xfer();
    span( 0, 16 );
    read_back( "B", b );

    for( i = 0; i < 8; ++i ) {
        if( a[i] != b[i] ) {
            same = 0;
        }
    }
    printf( "%s\n", same ? "same: the abort keeps the set-up" : "DIFFER: the abort changes the set-up" );

    paint();
    full_setup();
    outpw( 0xCEEE, 0x5210 );
    outpw( 0xBAEE, 0x0007 );
    outpw( 0x82E8, ROW );
    outpw( 0x86E8, 0 );
    outpw( 0xCAEE, 800 );
    if( wait_data() ) {
        inpw( 0xE2E8 );
    }
    abort_xfer();
    outpw( 0x82E8, ROW );
    outpw( 0x86E8, 0 );
    outpw( 0xCAEE, 16 );
    printf( "C:" );
    same = 1;
    if( wait_data() ) {
        for( i = 0; i < 8; ++i ) {
            WORD v = inpw( 0xE2E8 );

            printf( " %04x", v );
            same &= v == 0x3333;
        }
    } else {
        same = 0;
    }
    printf( "  SUBSYS_STAT %04x\n%s\n", inpw( 0x42E8 ),
            same ? "same: the abort keeps DP_CONFIG" : "DIFFER: DP_CONFIG lost" );
    abort_xfer();
    wait_idle();
    full_setup();
    outpw( 0xCEEE, 0x2211 );
    return( 0 );
}
