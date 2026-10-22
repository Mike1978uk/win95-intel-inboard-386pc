/* M8SCANR.EXE - what a SCAN_TO_X read returns on the Mach8, for a ScanLR done
 * on the card as ATIM8 does it (seg09:0109): DP_CONFIG 5210h or 4010h, CUR_X,
 * CUR_Y, SCAN_TO_X at the row's end, then one PIX_TRANS read a pixel. Paints
 * pixels 0..15 of row 0 with colours 10h..1Fh, reads them back to the right
 * from x 0 and to the left from x 15, each way with both DP_CONFIG values,
 * prints the words read, and aborts what is left as ATIM8 does. The row is
 * drawn over the desktop; Windows repaints it.
 * Build: wcl386 -q -bt=nt -l=nt -ox -fe=M8SCANR.EXE M8SCANR.C
 */
#include <windows.h>
#include <stdio.h>
#include <conio.h>

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

static void paint( void )
{
    int x;

    wait_idle();
    outpw( 0xAAE8, 0x00FF );            /* WRT_MASK */
    outpw( 0xAEE8, 0x00FF );            /* RD_MASK */
    outpw( 0xBEE8, 0xA000 );            /* PIX_CNTL */
    outpw( 0xDAEE, 0x0800 );            /* EXT_SCISSOR_L..B: the whole range */
    outpw( 0xDEEE, 0x0800 );
    outpw( 0xE2EE, 0x07FF );
    outpw( 0xE6EE, 0x07FF );
    outpw( 0xCEEE, 0x2211 );
    outpw( 0xBAEE, 0x0007 );
    for( x = 0; x < 16; ++x ) {
        wait_idle();
        outpw( 0xA6E8, 0x10 + x );
        outpw( 0x82E8, 0 );
        outpw( 0x86E8, x );
        outpw( 0xCAEE, x + 1 );
    }
    wait_idle();
}

static void scan( WORD dp, int left )
{
    long    n;
    int     i;
    int     cx = left ? 16 : 0;

    printf( "DP_CONFIG %04x %s:", dp, left ? "left from 15" : "right from 0" );
    wait_idle();
    outpw( 0xCEEE, dp );
    outpw( 0xBAEE, 0x0007 );
    outpw( 0x82E8, 0 );
    outpw( 0x86E8, cx );
    outpw( 0xCAEE, left ? 0 : 16 );
    for( n = 0; n < 200000L && !(inpw( 0x9AE8 ) & 0x0100); ++n ) {
    }
    if( n == 200000L ) {
        printf( " no data ready, GP_STAT %04x\n", inpw( 0x9AE8 ) );
    } else {
        for( i = 0; i < 16; ++i ) {
            printf( " %04x", inpw( 0xE2E8 ) );
        }
        printf( "\n" );
    }
    outpw( 0x42E8, 0x900F );
    outpw( 0x42E8, 0x500F );
    wait_idle();
}

int main( void )
{
    paint();
    scan( 0x5210, 0 );
    scan( 0x5210, 1 );
    paint();
    scan( 0x4010, 0 );
    scan( 0x4010, 1 );
    outpw( 0xCEEE, 0x2211 );
    return( 0 );
}
