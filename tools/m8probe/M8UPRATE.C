/* M8UPRATE.EXE - what a 100x100 8bpp host upload costs on the card, the
 * transfer SetDIBitsToDevice makes (ATIM8 2.98 us a byte in the bench, ours
 * 3.9). A: the blit fed a rep outsw per row, as both drivers feed it. B: one
 * rep outsw for the whole rectangle. C: the same 5000 words to 0278h, which
 * nothing decodes: the bus floor. Each 20 times, timed with
 * QueryPerformanceCounter. The rectangle is drawn at the screen's top left;
 * the desktop is repainted at the end.
 * Build: wcl386 -q -bt=nt -l=nt -ox -fe=M8UPRATE.EXE M8UPRATE.C
 */
#include <windows.h>
#include <stdio.h>
#include <conio.h>

#define W       100
#define H       100
#define REPS    20

static WORD buf[W * H / 2];

void rep_outsw( WORD port, const WORD *src, DWORD count );
#pragma aux rep_outsw = "rep outsw" parm [dx] [esi] [ecx] modify [esi ecx];

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

static void blit_start( void )
{
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
    outpw( 0x96E8, W - 1 );             /* MAJ_AXIS_PCNT */
    outpw( 0xBEE8, 0x0000 | (H - 1) );  /* MIN_AXIS_PCNT */
    outpw( 0x9AE8, 0x53B1 );            /* CMD: host write, two pixels a word */
}

static double ms( LARGE_INTEGER a, LARGE_INTEGER b, LARGE_INTEGER f )
{
    return( (double)(b.QuadPart - a.QuadPart) * 1000.0 / (double)f.QuadPart );
}

int main( void )
{
    LARGE_INTEGER   f, t0, t1;
    int             r, y;
    double          a, b, c;

    for( r = 0; r < W * H / 2; ++r ) {
        buf[r] = (WORD)(((r * 2) % 200) | (((r * 2 + 1) % 200) << 8));
    }
    QueryPerformanceFrequency( &f );

    QueryPerformanceCounter( &t0 );
    for( r = 0; r < REPS; ++r ) {
        blit_start();
        for( y = 0; y < H; ++y ) {
            rep_outsw( 0xE2E8, buf + y * (W / 2), W / 2 );
        }
    }
    wait_idle();
    QueryPerformanceCounter( &t1 );
    a = ms( t0, t1, f ) / REPS;

    QueryPerformanceCounter( &t0 );
    for( r = 0; r < REPS; ++r ) {
        blit_start();
        rep_outsw( 0xE2E8, buf, W * H / 2 );
    }
    wait_idle();
    QueryPerformanceCounter( &t1 );
    b = ms( t0, t1, f ) / REPS;

    QueryPerformanceCounter( &t0 );
    for( r = 0; r < REPS; ++r ) {
        rep_outsw( 0x0278, buf, W * H / 2 );
    }
    QueryPerformanceCounter( &t1 );
    c = ms( t0, t1, f ) / REPS;

    printf( "A per-row rep outsw   %7.2f ms  %5.3f us/B\n", a, a * 1000.0 / (W * H) );
    printf( "B one rep outsw       %7.2f ms  %5.3f us/B\n", b, b * 1000.0 / (W * H) );
    printf( "C 0278h, no device    %7.2f ms  %5.3f us/B\n", c, c * 1000.0 / (W * H) );
    printf( "GP_STAT %04x EXT_FIFO %04x\n", inpw( 0x9AE8 ), inpw( 0x9AEE ) );
    InvalidateRect( NULL, NULL, TRUE );
    return( 0 );
}
