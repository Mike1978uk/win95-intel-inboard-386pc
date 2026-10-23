/* M8APERT.EXE - whether the Mach8's video memory shows through the VGA
 * windows as the card stands (ATI's Win 3.1 docs speak of an aperture in the
 * VGA area, ATIDESK/MACH8.WRI:140). Writes a 256-byte ramp (00h..FFh) into
 * row 0 of the Mach8's memory with a host blit, then searches A0000-BFFFFh
 * for 16 bytes of it, and prints the first bytes of each window. Row 0 is
 * repainted by Windows.
 * Build: wcl386 -q -bt=nt -l=nt -ox -fe=M8APERT.EXE M8APERT.C
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

static void ramp( void )
{
    int i;

    wait_idle();
    outpw( 0xAAE8, 0x00FF );            /* WRT_MASK */
    outpw( 0xAEE8, 0x00FF );            /* RD_MASK */
    outpw( 0xBEE8, 0xA000 );            /* PIX_CNTL */
    outpw( 0xDAEE, 0x0800 );            /* EXT_SCISSOR_L..B: the whole range */
    outpw( 0xDEEE, 0x0800 );
    outpw( 0xE2EE, 0x07FF );
    outpw( 0xE6EE, 0x07FF );
    outpw( 0xCEEE, 0x2211 );            /* DP_CONFIG: 8514 path */
    outpw( 0xBAE8, 0x0047 );            /* FRGD_MIX: host data, replace */
    outpw( 0x86E8, 0 );                 /* CUR_X */
    outpw( 0x82E8, 0 );                 /* CUR_Y */
    outpw( 0x96E8, 255 );               /* MAJ_AXIS_PCNT: 256 wide */
    outpw( 0xBEE8, 0x0000 );            /* MIN_AXIS_PCNT: 1 row */
    outpw( 0x9AE8, 0x53B1 );            /* CMD: host write, two pixels a word */
    for( i = 0; i < 256; i += 2 ) {
        outpw( 0xE2E8, (WORD)(i | ((i + 1) << 8)) );
    }
    wait_idle();
}

int main( void )
{
    const BYTE  *base = (const BYTE *)0xA0000;
    DWORD       len = 0x20000;
    DWORD       i;
    int         k;
    int         found = 0;

    ramp();
    if( IsBadReadPtr( base, len ) ) {
        printf( "A0000-BFFFF not readable from this process\n" );
        return( 1 );
    }
    for( i = 0; i + 16 <= len; ++i ) {
        for( k = 0; k < 16 && base[i + k] == (BYTE)(base[i] + k); ++k ) {
        }
        if( k == 16 ) {
            printf( "ramp-like run at %05lx: starts %02x\n", 0xA0000 + i, base[i] );
            if( ++found == 8 ) {
                break;
            }
            i += 15;
        }
    }
    if( found == 0 ) {
        printf( "no 16-byte ramp anywhere in A0000-BFFFF\n" );
    }
    for( i = 0; i < len; i += 0x8000 ) {
        printf( "%05lx:", 0xA0000 + i );
        for( k = 0; k < 16; ++k ) {
            printf( " %02x", base[i + k] );
        }
        printf( "\n" );
    }
    InvalidateRect( NULL, NULL, TRUE );
    return( 0 );
}
