/* M8CURRD.EXE - the cursor as it is in video memory: moves the pointer to
 * (400, 300), waits, and reads the 32x32 block there straight from the card's
 * ports, past the driver (which lifts the cursor for any read through GDI).
 * Prints # for white (FFh), o for black (00h), ? for the cursor's key colour
 * (01h: a keyed copy that let it through), . for anything else.
 * Build: wcl386 -q -bt=nt -l=nt -ox -fe=M8CURRD.EXE M8CURRD.C user32.lib
 */
#include <windows.h>
#include <stdio.h>
#include <conio.h>

#define X0  400
#define Y0  300

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

int main( void )
{
    WORD    px[16];
    long    n;
    int     r;
    int     c;
    BYTE    b;

    SetCursorPos( X0, Y0 );
    Sleep( 500 );
    wait_idle();
    outpw( 0xCEEE, 0x2211 );            /* DP_CONFIG: plain 8514 */
    outpw( 0xBEE8, 0xA000 );            /* PIX_CNTL */
    outpw( 0xAEE8, 0x00FF );            /* RD_MASK */
    outpw( 0xBAE8, 0x0047 );            /* FRGD_MIX: host, replace */
    outpw( 0x86E8, X0 );
    outpw( 0x82E8, Y0 );
    outpw( 0x96E8, 31 );
    outpw( 0xBEE8, 31 );
    outpw( 0x9AE8, 0x53B0 );            /* host read */
    for( n = 0; n < 200000L && !(inpw( 0x9AE8 ) & 0x0100); ++n ) {
    }
    if( n == 200000L ) {
        printf( "no read data\n" );
        outpw( 0x42E8, 0x900F );
        outpw( 0x42E8, 0x500F );
        return( 1 );
    }
    for( r = 0; r < 32; ++r ) {
        for( c = 0; c < 16; ++c ) {
            px[c] = inpw( 0xE2E8 );
        }
        for( c = 0; c < 32; ++c ) {
            b = (BYTE)(c & 1 ? px[c / 2] >> 8 : px[c / 2]);
            putchar( b == 0xFF ? '#' : b == 0x00 ? 'o' : b == 0x01 ? '?' : '.' );
        }
        putchar( '\n' );
    }
    return( 0 );
}
