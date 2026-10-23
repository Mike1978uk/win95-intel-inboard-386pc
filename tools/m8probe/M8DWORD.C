/* M8DWORD.EXE - whether a 32-bit OUT to PIX_TRANS (E2E8h) feeds a host blit
 * with both of its words. The Inboard splits it into four byte cycles,
 * E2E8h-E2EBh; E2EAh is no register in the guide, but the DAC mask is at
 * 02EAh, so the mask is read before and after and put back. A: a 16x1 blit
 * fed 8 words with 16-bit OUTs. B: the same fed 4 dwords. Each is read back
 * with a SCAN_TO_X read. If B = A and the mask is unchanged, C times 5000
 * dwords against 10000 words into a 100x100 blit. Row 0 is repainted by
 * Windows.
 * Build: wcl386 -q -bt=nt -l=nt -ox -fe=M8DWORD.EXE M8DWORD.C
 */
#include <windows.h>
#include <stdio.h>
#include <conio.h>

static WORD buf[100 * 100 / 2];

void rep_outsw( WORD port, const void *src, DWORD count );
#pragma aux rep_outsw = "rep outsw" parm [dx] [esi] [ecx] modify [esi ecx];
void rep_outsd( WORD port, const void *src, DWORD count );
#pragma aux rep_outsd = "rep outsd" parm [dx] [esi] [ecx] modify [esi ecx];

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

static void blit( int w, int h )
{
    wait_idle();
    outpw( 0xAAE8, 0x00FF );
    outpw( 0xAEE8, 0x00FF );
    outpw( 0xBEE8, 0xA000 );
    outpw( 0xDAEE, 0x0800 );
    outpw( 0xDEEE, 0x0800 );
    outpw( 0xE2EE, 0x07FF );
    outpw( 0xE6EE, 0x07FF );
    outpw( 0xCEEE, 0x2211 );
    outpw( 0xBAE8, 0x0047 );            /* FRGD_MIX: host data, replace */
    outpw( 0x86E8, 0 );
    outpw( 0x82E8, 0 );
    outpw( 0x96E8, w - 1 );
    outpw( 0xBEE8, 0x0000 | (h - 1) );
    outpw( 0x9AE8, 0x53B1 );            /* CMD: host write, two pixels a word */
}

static void read_row( const char *name, WORD *w )
{
    long    n;
    int     i;

    wait_idle();
    outpw( 0xCEEE, 0x5210 );
    outpw( 0xBAEE, 0x0007 );
    outpw( 0x82E8, 0 );
    outpw( 0x86E8, 0 );
    outpw( 0xCAEE, 16 );
    for( n = 0; n < 200000L && !(inpw( 0x9AE8 ) & 0x0100); ++n ) {
    }
    printf( "%s:", name );
    for( i = 0; i < 8; ++i ) {
        w[i] = n < 200000L ? inpw( 0xE2E8 ) : 0xDEAD;
        printf( " %04x", w[i] );
    }
    printf( "\n" );
    outpw( 0x42E8, 0x900F );
    outpw( 0x42E8, 0x500F );
    wait_idle();
    outpw( 0xCEEE, 0x2211 );
}

int main( void )
{
    WORD            a[8], b[8];
    DWORD           d[4];
    int             i, same = 1;
    BYTE            mask0, mask1;
    LARGE_INTEGER   f, t0, t1;
    double          ws, ds;

    for( i = 0; i < 8; ++i ) {
        buf[i] = (WORD)(0x1010 + i * 0x0202 + 0x0100);
    }
    mask0 = (BYTE)inp( 0x02EA );

    blit( 16, 1 );
    for( i = 0; i < 8; ++i ) {
        outpw( 0xE2E8, buf[i] );
    }
    read_row( "A words ", a );

    blit( 16, 1 );
    for( i = 0; i < 4; ++i ) {
        d[i] = (DWORD)buf[2 * i] | ((DWORD)buf[2 * i + 1] << 16);
        outpd( 0xE2E8, d[i] );
    }
    if( !wait_idle() ) {
        /* Half the words reached the blit: it still waits for host data. */
        outpw( 0x42E8, 0x900F );
        outpw( 0x42E8, 0x500F );
    }
    mask1 = (BYTE)inp( 0x02EA );
    outp( 0x02EA, mask0 );
    read_row( "B dwords", b );

    for( i = 0; i < 8; ++i ) {
        same &= a[i] == b[i];
    }
    printf( "DAC mask before %02x after dwords %02x (restored)\n", mask0, mask1 );
    printf( "%s\n", same && mask0 == mask1 ? "same: dwords feed the blit" : "DIFFER: dwords do not feed the blit" );
    if( same && mask0 == mask1 ) {
        QueryPerformanceFrequency( &f );
        QueryPerformanceCounter( &t0 );
        for( i = 0; i < 10; ++i ) {
            blit( 100, 100 );
            rep_outsw( 0xE2E8, buf, 5000 );
        }
        wait_idle();
        QueryPerformanceCounter( &t1 );
        ws = (double)(t1.QuadPart - t0.QuadPart) * 1000.0 / f.QuadPart / 10;
        QueryPerformanceCounter( &t0 );
        for( i = 0; i < 10; ++i ) {
            blit( 100, 100 );
            rep_outsd( 0xE2E8, buf, 2500 );
        }
        wait_idle();
        QueryPerformanceCounter( &t1 );
        ds = (double)(t1.QuadPart - t0.QuadPart) * 1000.0 / f.QuadPart / 10;
        printf( "C 100x100: words %.2f ms (%.3f us/B), dwords %.2f ms (%.3f us/B)\n",
                ws, ws * 1000.0 / 10000, ds, ds * 1000.0 / 10000 );
        printf( "GP_STAT %04x EXT_FIFO %04x DAC mask %02x\n", inpw( 0x9AE8 ), inpw( 0x9AEE ),
                inp( 0x02EA ) );
    }
    InvalidateRect( NULL, NULL, TRUE );
    return( 0 );
}
