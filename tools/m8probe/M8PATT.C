/* M8PATT.EXE - what PATT_LENGTH (D2EE) bits 8, 13 and 14 do on the Mach8. The
 * Mach32 guide calls bits 14:8 reserved (9-60); ATIM8 writes 4107h and 6107h
 * for its one-blit diagonal hatches (seg16:3dd6, 3e45). Each case fills a
 * 16x8 patch at the screen's top-left with a linear mono pattern, ATIM8's
 * register order, reads it back and prints it: # foreground, . background.
 * The patch is drawn over the desktop; Windows repaints it.
 *   M8PATT [x y]       patch position (default 0 0)
 *   M8PATT s           SCAN_TO_X cases instead
 * Build: wcl386 -q -bt=nt -l=nt -ox -fe=M8PATT.EXE M8PATT.C
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>

#define W   16
#define H   8
#define FG  0x01
#define BG  0x02

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

static void fill( int x, int y, WORD len, WORD data, WORD index )
{
    wait_idle();
    outpw( 0xAAE8, 0x00FF );            /* WRT_MASK */
    outpw( 0xAEE8, 0x00FF );            /* RD_MASK */
    outpw( 0xBEE8, 0xA000 );            /* PIX_CNTL */
    outpw( 0xDAEE, 0x0800 );            /* EXT_SCISSOR_L..B: the whole range */
    outpw( 0xDEEE, 0x0800 );
    outpw( 0xE2EE, 0x07FF );
    outpw( 0xE6EE, 0x07FF );
    wait_idle();
    outpw( 0xCEEE, 0x2231 );            /* DP_CONFIG: mono pattern picks the colour */
    outpw( 0xBAEE, 0x0007 );            /* ALU_FG_FN: replace */
    outpw( 0xB6EE, 0x0007 );            /* ALU_BG_FN: replace */
    outpw( 0xA6E8, FG );
    outpw( 0xA2E8, BG );
    outpw( 0xD2EE, len );
    outpw( 0x82EE, 0x0010 );
    outpw( 0x8EEE, data );
    outpw( 0xD6EE, index );
    outpw( 0x86E8, x );                 /* CUR_X */
    outpw( 0xA6EE, x );                 /* DEST_X_START */
    outpw( 0xAAEE, x + W );             /* DEST_X_END */
    outpw( 0x82E8, y );                 /* CUR_Y */
    outpw( 0xAEEE, y + H );             /* DEST_Y_END: starts the blit */
    wait_idle();
}

static void show( int x, int y )
{
    WORD    px[W / 2];
    long    n;
    int     r;
    int     c;
    BYTE    b;

    wait_idle();
    outpw( 0xCEEE, 0x2211 );            /* DP_CONFIG: plain 8514 */
    outpw( 0xBAE8, 0x0047 );            /* FRGD_MIX: host, replace */
    outpw( 0x86E8, x );
    outpw( 0x82E8, y );
    outpw( 0x96E8, W - 1 );
    outpw( 0xBEE8, H - 1 );
    outpw( 0x9AE8, 0x53B0 );            /* host read */
    for( n = 0; n < 200000L && !(inpw( 0x9AE8 ) & 0x0100); ++n ) {
    }
    if( n == 200000L ) {
        printf( "no read data\n" );
        outpw( 0x42E8, 0x900F );
        outpw( 0x42E8, 0x500F );
        return;
    }
    for( r = 0; r < H; ++r ) {
        printf( "    " );
        for( c = 0; c < W / 2; ++c ) {
            px[c] = inpw( 0xE2E8 );
        }
        for( c = 0; c < W; ++c ) {
            b = (BYTE)(c & 1 ? px[c / 2] >> 8 : px[c / 2]);
            putchar( b == FG ? '#' : b == BG ? '.' : '?' );
        }
        putchar( '\n' );
    }
}

static void run( int x, int y, WORD len, WORD data, WORD index )
{
    printf( "D2EE=%04x data=%04x index=%u\n", len, data, index );
    fill( x, y, len, data, index );
    show( x, y );
}

/* SCAN_TO_X (CAEE, guide 9-61): one write draws from CUR_X to the given X on
 * CUR_Y, through the blit set-up. Pattern: a row byte per row, as ATIM8's
 * scanline routine (seg08:0dcd). Solid: two spans a row, which shows whether
 * the fill flag of polygon scan conversion turns every second write into a
 * move. */
static void scan_pattern( int x, int y )
{
    int r;

    printf( "SCAN_TO_X pattern, row byte 80h >> row\n" );
    wait_idle();
    outpw( 0xCEEE, 0x2231 );
    outpw( 0xBAEE, 0x0007 );
    outpw( 0xB6EE, 0x0007 );
    outpw( 0xA6E8, FG );
    outpw( 0xA2E8, BG );
    outpw( 0xD2EE, 0x0007 );
    for( r = 0; r < H; ++r ) {
        wait_idle();
        outpw( 0x82E8, y + r );
        outpw( 0x82EE, 0x0010 );
        outpw( 0x8EEE, 0x80 >> r );
        outpw( 0x86E8, x );
        outpw( 0xD6EE, x & 7 );
        outpw( 0xCAEE, x + W );
    }
    show( x, y );
}

static void scan_solid( int x, int y )
{
    int r;

    printf( "SCAN_TO_X solid, spans 0-4 and 8-12 each row, background first\n" );
    fill( x, y, 0x0007, 0x0000, 0 );
    wait_idle();
    outpw( 0xCEEE, 0x2211 );
    outpw( 0xBAEE, 0x0007 );
    outpw( 0xA6E8, FG );
    for( r = 0; r < H; ++r ) {
        wait_idle();
        outpw( 0x82E8, y + r );
        outpw( 0x86E8, x );
        outpw( 0xCAEE, x + 5 );
        outpw( 0x86E8, x + 8 );
        outpw( 0xCAEE, x + 13 );
    }
    show( x, y );
}

int main( int argc, char **argv )
{
    static const WORD lens[] = { 0x0007, 0x4107, 0x6107, 0x0107, 0x4007, 0x2007, 0x2107 };
    int     x = argc > 2 ? atoi( argv[1] ) : 0;
    int     y = argc > 2 ? atoi( argv[2] ) : 0;
    int     i;

    if( argc > 1 && argv[1][0] == 's' ) {
        scan_pattern( 0, 0 );
        scan_solid( 0, 0 );
        outpw( 0xCEEE, 0x2211 );
        return( 0 );
    }
    /* Bit order and start: one bit set, each end of the word. */
    run( x, y, 0x0007, 0x0001, 0 );
    run( x, y, 0x0007, 0x0080, 0 );
    run( x, y, 0x0007, 0x0100, 0 );
    run( x, y, 0x0007, 0x0001, 3 );
    for( i = 1; i < sizeof( lens ) / sizeof( lens[0] ); ++i ) {
        run( x, y, lens[i], 0x0001, 0 );
    }
    run( x, y, 0x4107, 0x0001, 3 );
    run( x, y, 0x6107, 0x0001, 3 );
    run( x, y, 0x411F, 0x0001, 0 );
    outpw( 0xCEEE, 0x2211 );
    return( 0 );
}
