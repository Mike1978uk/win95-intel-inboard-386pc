/* M8STAT.EXE - read the Mach8 engine's status registers and print them, for a
 * card that has stopped drawing. Reads only, except `drain`.
 *   M8STAT [count]     count samples, 10 ms apart (default 5)
 *   M8STAT drain       a command waiting for host data: send zero words to
 *                      PIX_TRANS until GP_STAT is no longer busy, and say how many
 * Build: wcl386 -q -bt=nt -l=nt -ox -fe=M8STAT.EXE M8STAT.C
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>

static void show( void )
{
    /* GP_STAT 9AE8: bit 9 busy, bit 8 data ready; EXT_FIFO_STATUS 9AEE: one bit
     * per occupied entry; EXT_GE_STATUS 62EE: bit 13 GE active; SUBSYS_STAT 42E8. */
    printf( "GP_STAT %04x  EXT_FIFO %04x  EXT_GE %04x  SUBSYS %04x\n",
            inpw( 0x9AE8 ), inpw( 0x9AEE ), inpw( 0x62EE ), inpw( 0x42E8 ) );
}

int main( int argc, char **argv )
{
    int     n = argc > 1 ? atoi( argv[1] ) : 5;
    int     i;
    long    words;

    if( argc > 1 && strcmp( argv[1], "drain" ) == 0 ) {
        show();
        for( words = 0; words < 600000L && (inpw( 0x9AE8 ) & 0x0200); ++words ) {
            outpw( 0xE2E8, 0 );
        }
        printf( "%ld words sent\n", words );
        show();
        return 0;
    }
    for( i = 0; i < n; ++i ) {
        show();
        Sleep( 10 );
    }
    return 0;
}
