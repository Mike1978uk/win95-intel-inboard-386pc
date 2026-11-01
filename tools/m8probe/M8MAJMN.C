/* M8MAJMN.EXE - do MAJ_AXIS_PCNT and MIN_AXIS_PCNT survive each engine
 * operation the driver uses? For each case: set MAJ = 6, MIN = 3, run the
 * operation elsewhere on the screen, read MAJ back at 96EE, then fill a
 * rectangle writing only CUR_X, CUR_Y and CMD, and read it back from the card.
 * Survived = a 7x4 rectangle. The driver may skip unchanged MAJ/MIN writes
 * only after the operations that print "kept".
 * Leaves the engine as the driver's init does; reboot after (the driver's
 * register cache does not know the probe ran).
 * Build: wcl386 -q -bt=nt -l=nt -ox -fe=M8MAJMN.EXE M8MAJMN.C user32.lib
 */
#include <windows.h>
#include <stdio.h>
#include <conio.h>

#define X0      16
#define Y0      16
#define XOP     200             /* where each case's own operation draws */
#define MAJ     6
#define MIN     3
#define MARK    0x5A

static int wait_idle( void )
{
    long    n;

    for( n = 0; n < 200000L; ++n ) {
        if( inpw( 0x9AEE ) == 0 && !(inpw( 0x9AE8 ) & 0x0200) ) {
            return( 1 );
        }
    }
    printf( "engine not idle: GP_STAT %04x EXT_FIFO %04x\n", inpw( 0x9AE8 ), inpw( 0x9AEE ) );
    outpw( 0x42E8, 0x900F );
    outpw( 0x42E8, 0x500F );
    return( 0 );
}

static void plain( void )
{
    outpw( 0xCEEE, 0x2211 );            /* DP_CONFIG: plain 8514 */
    outpw( 0xBEE8, 0xA000 );            /* PIX_CNTL */
    outpw( 0xAAE8, 0x00FF );            /* WRT_MASK */
    outpw( 0xAEE8, 0x00FF );            /* RD_MASK */
    outpw( 0xDAEE, 0x0800 );            /* scissors as the driver leaves them */
    outpw( 0xDEEE, 0x0800 );
    outpw( 0xE2EE, 0x07FF );
    outpw( 0xE6EE, 0x07FF );
    outpw( 0xBAE8, 0x0027 );            /* FRGD_MIX: colour, replace */
}

static void fill( int x, int y, int w, int h, int colour )
{
    wait_idle();
    outpw( 0xA6E8, colour );
    outpw( 0x86E8, x );
    outpw( 0x82E8, y );
    outpw( 0x96E8, w - 1 );
    outpw( 0xBEE8, h - 1 );
    outpw( 0x9AE8, 0x40F3 );
}

/* 32x16 at (X0, Y0) by a host read, two pixels a word. */
static int read_block( BYTE *px )
{
    long    n;
    int     i;
    WORD    v;

    wait_idle();
    outpw( 0xBAE8, 0x0047 );            /* host data path for the read */
    outpw( 0x86E8, X0 );
    outpw( 0x82E8, Y0 );
    outpw( 0x96E8, 31 );
    outpw( 0xBEE8, 15 );
    outpw( 0x9AE8, 0x53B0 );            /* host read */
    for( n = 0; n < 200000L && !(inpw( 0x9AE8 ) & 0x0100); ++n ) {
    }
    if( n == 200000L ) {
        printf( "no read data\n" );
        outpw( 0x42E8, 0x900F );
        outpw( 0x42E8, 0x500F );
        return( 0 );
    }
    for( i = 0; i < 32 * 16; i += 2 ) {
        v = inpw( 0xE2E8 );
        px[i] = (BYTE)v;
        px[i + 1] = (BYTE)(v >> 8);
    }
    outpw( 0xBAE8, 0x0027 );
    return( 1 );
}

static void op( int c )
{
    WORD    w[4];
    int     i;

    wait_idle();
    switch( c ) {
    case 1:                             /* rectangle fill, MAJ/MIN as set */
        outpw( 0xA6E8, 0x11 );
        outpw( 0x86E8, XOP );
        outpw( 0x82E8, Y0 );
        outpw( 0x9AE8, 0x40F3 );
        break;
    case 2:                             /* screen-to-screen blit */
        outpw( 0xBAE8, 0x0067 );
        outpw( 0x86E8, XOP );
        outpw( 0x82E8, Y0 );
        outpw( 0x8EE8, XOP + 40 );
        outpw( 0x8AE8, Y0 );
        outpw( 0x9AE8, 0xC0F3 );
        wait_idle();
        outpw( 0xBAE8, 0x0027 );
        break;
    case 3:                             /* LINEDRAW, two segments */
        outpw( 0xA6E8, 0x22 );
        outpw( 0xA2EE, 0x0004 );
        outpw( 0x9AEE, 0 );
        outpw( 0xFEEE, XOP );
        outpw( 0xFEEE, Y0 + 8 );
        outpw( 0xFEEE, XOP + 60 );
        outpw( 0xFEEE, Y0 + 20 );
        outpw( 0xFEEE, XOP + 90 );
        outpw( 0xFEEE, Y0 + 8 );
        break;
    case 4:                             /* SCAN_TO_X draw */
        outpw( 0xA6E8, 0x33 );
        outpw( 0x82E8, Y0 + 24 );
        outpw( 0x86E8, XOP );
        outpw( 0xCAEE, XOP + 80 );
        break;
    case 5:                             /* SCAN_TO_X read, 8 pixels */
        outpw( 0xCEEE, 0x5210 );
        outpw( 0x82E8, Y0 );
        outpw( 0x86E8, XOP );
        outpw( 0xCAEE, XOP + 8 );
        for( i = 0; i < 4; ++i ) {
            long n;
            for( n = 0; n < 200000L && !(inpw( 0x9AE8 ) & 0x0100); ++n ) {
            }
            w[i] = inpw( 0xE2E8 );
        }
        wait_idle();
        outpw( 0xCEEE, 0x2211 );
        outpw( 0xBEE8, 0xA000 );
        outpw( 0xBAE8, 0x0027 );
        break;
    }
    (void)w;
}

static const char *name[] = {
    "nothing", "rect fill", "blit", "LINEDRAW", "SCAN_TO_X draw", "SCAN_TO_X read"
};

int main( void )
{
    static BYTE px[32 * 16];
    int     c;
    int     w;
    int     h;
    WORD    rb;

    SetCursorPos( 780, 580 );
    Sleep( 300 );
    if( !wait_idle() ) {
        return( 1 );
    }
    for( c = 0; c < 6; ++c ) {
        plain();
        fill( X0, Y0, 32, 16, 0x00 );
        wait_idle();
        outpw( 0x96E8, MAJ );
        outpw( 0xBEE8, MIN );           /* MULTIFUNC index 0: MIN_AXIS_PCNT */
        op( c );
        wait_idle();
        rb = inpw( 0x96EE );
        outpw( 0xA6E8, MARK );
        outpw( 0x86E8, X0 );
        outpw( 0x82E8, Y0 );
        outpw( 0x9AE8, 0x40F3 );        /* no MAJ, no MIN */
        if( !read_block( px ) ) {
            return( 1 );
        }
        for( w = 0; w < 32 && px[w] == MARK; ++w ) {
        }
        for( h = 0; h < 16 && px[h * 32] == MARK; ++h ) {
        }
        printf( "%-15s 96EE %04x  fill %2dx%-2d  %s\n", name[c], rb, w, h,
                w == MAJ + 1 && h == MIN + 1 ? "kept" : "CHANGED" );
    }
    plain();
    wait_idle();
    InvalidateRect( NULL, NULL, TRUE );
    return( 0 );
}
