/* M8P0.EXE - the numbers that order docs/mach8_frame_rate_plan.md, Phase 0:
 *   R  RAM: write, read and fingerprint 600 KB (the 16 bpp frame). Is the
 *      64 ms unlock the read itself, or the hash?
 *   O  raw OUT cost: 14 FRGD_COLOR writes then one FIFO wait, and one IN of
 *      EXT_FIFO_STATUS alone. DOS measured 7.64 us a word OUT; M8SPAN's loop
 *      costs ~10 us a write. The gap is our loop or Windows.
 *   H  which CRTC the 28800 line counter (A8h/A9h) follows while the 8514
 *      side drives the monitor: its wraps a second against DISP_STATUS
 *      VERT_SYNC edges, and LINE_SYNC toggles counted between two edges.
 *   V  the V_BLANK latch (SUBSYS_STATUS bit 0) looked at once a millisecond:
 *      frames caught without polling.
 *   D  DISP_STATUS VERT_SYNC alone in a tight loop: time in each state, so
 *      the short one is the pulse whatever its polarity.
 *   W  intervals between V_BLANK latch hits, looked at in a tight loop.
 *   S  "M8P0 s": CRT_OFFSET written mid-frame (owner watches, 5 s). Rows
 *      600-1023 are filled with bars; each frame the offset is set to 0 at
 *      VERT_SYNC and to row 624 halfway down (by LINE_SYNC count). A steady
 *      split = the offset takes effect within the frame (raster splits work);
 *      all bars = it latches once a frame; flicker = neither cleanly.
 * Interrupts stay on in Windows: best of three for timings. Drawing goes
 * behind the driver, so it is asked to set the card up again (ExtEscape
 * 0x4D3E) at the end.
 * Build: wcl386 -q -bt=nt -l=nt -ox -fe=M8P0.EXE M8P0.C
 */
#include <windows.h>
#include <stdio.h>
#include <conio.h>

#define FRAME   (640L * 480 * 2)

static LARGE_INTEGER    freq;
static DWORD            *ram;

static double now_us( void )
{
    LARGE_INTEGER t;

    QueryPerformanceCounter( &t );
    return( (double)t.QuadPart * 1e6 / (double)freq.QuadPart );
}

static void wait_empty( void )
{
    long n;

    for( n = 0; n < 200000L && inpw( 0x9AEE ) != 0; ++n ) {
    }
}

static double best3( double (*f)( void ) )
{
    double  b = 1e30;
    double  t;
    int     k;

    for( k = 0; k < 3; ++k ) {
        t = f();
        if( t < b ) {
            b = t;
        }
    }
    return( b );
}

/* ---- R ---- */

static double r_write( void )
{
    double  t0 = now_us();
    DWORD   *p = ram;
    DWORD   n = FRAME / 4;

    _asm {
        mov     edi, p
        mov     ecx, n
        mov     eax, 12345678h
        cld
        rep     stosd
    }
    return( now_us() - t0 );
}

static double r_read( void )
{
    double  t0 = now_us();
    DWORD   *p = ram;
    DWORD   n = FRAME / 4;

    _asm {
        mov     esi, p
        mov     ecx, n
        cld
        rep     lodsd
    }
    return( now_us() - t0 );
}

/* The driver's fingerprint: Jenkins one-at-a-time, one step per dword. */
static double r_hash( void )
{
    double  t0 = now_us();
    DWORD   *p = ram;
    DWORD   n = FRAME / 4;

    _asm {
        mov     esi, p
        mov     ecx, n
        xor     ebx, ebx
    hl:
        mov     eax, [esi]
        add     esi, 4
        add     ebx, eax
        mov     eax, ebx
        shl     eax, 10
        add     ebx, eax
        mov     eax, ebx
        shr     eax, 6
        xor     ebx, eax
        dec     ecx
        jnz     hl
    }
    return( now_us() - t0 );
}

/* ---- O ---- */

#define O_REPS  500

static double o_out( void )
{
    double  t0;
    int     i;

    wait_empty();
    t0 = now_us();
    for( i = 0; i < O_REPS; ++i ) {
        _asm {
            mov     dx, 0A6E8h
            mov     ax, 10h
            out     dx, ax
            out     dx, ax
            out     dx, ax
            out     dx, ax
            out     dx, ax
            out     dx, ax
            out     dx, ax
            out     dx, ax
            out     dx, ax
            out     dx, ax
            out     dx, ax
            out     dx, ax
            out     dx, ax
            out     dx, ax
            mov     dx, 9AEEh
        ow:
            in      ax, dx
            test    ax, ax
            jnz     ow
        }
    }
    return( (now_us() - t0) / O_REPS );
}

/* The same 14 writes from memory: a loop of single OUTs (as the drivers'
 * queues and DirectDraw's transfer loops do) and one rep outsw. */
static WORD o_words[14] = { 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29 };

static double o_loop( void )
{
    double  t0;
    int     i;
    int     k;

    wait_empty();
    t0 = now_us();
    for( i = 0; i < O_REPS; ++i ) {
        for( k = 0; k < 14; ++k ) {
            outpw( 0xA6E8, o_words[k] );
        }
        wait_empty();
    }
    return( (now_us() - t0) / O_REPS );
}

static double o_rep( void )
{
    double  t0;
    int     i;
    WORD    *w = o_words;

    wait_empty();
    t0 = now_us();
    for( i = 0; i < O_REPS; ++i ) {
        _asm {
            mov     esi, w
            mov     ecx, 14
            mov     dx, 0A6E8h
            cld
            rep     outsw
        }
        wait_empty();
    }
    return( (now_us() - t0) / O_REPS );
}

static double o_in( void )
{
    double  t0;
    int     i;

    wait_empty();
    t0 = now_us();
    for( i = 0; i < O_REPS; ++i ) {
        _asm {
            mov     dx, 9AEEh
            in      ax, dx
            in      ax, dx
            in      ax, dx
            in      ax, dx
            in      ax, dx
            in      ax, dx
            in      ax, dx
            in      ax, dx
            in      ax, dx
            in      ax, dx
        }
    }
    return( (now_us() - t0) / (O_REPS * 10) );
}

/* ---- H ---- */

static int a9( void )
{
    outp( 0x1CE, 0xA9 );
    return( inp( 0x1CF ) );
}

static int vsync( void )
{
    return( (inpw( 0x02E8 ) & 2) != 0 );
}

static void arm_h( void )
{
    double  t0;
    double  t;
    long    wraps = 0;
    long    edges = 0;
    long    lines[8];
    int     nl = 0;
    int     lo = 255;
    int     hi = 0;
    int     last = a9();
    int     v = vsync();
    int     lv = inpw( 0x02E8 ) & 4;
    int     c;
    int     w;
    int     k;
    long    n = 0;
    long    toggles = 0;
    int     counting = 0;

    t0 = now_us();
    do {
        c = a9();
        if( c < lo ) lo = c;
        if( c > hi ) hi = c;
        if( c < last - 32 ) {
            ++wraps;
        }
        last = c;
        w = inpw( 0x02E8 );
        if( ((w & 2) != 0) != v ) {
            v = !v;
            if( v ) {
                ++edges;
                if( counting && nl < 8 ) {
                    lines[nl++] = toggles;
                }
                counting = 1;
                toggles = 0;
            }
        }
        if( (w & 4) != lv ) {
            lv = w & 4;
            ++toggles;
        }
        if( (++n & 255) == 0 ) {
            t = now_us();
        }
    } while( n < 256 || t - t0 < 1e6 );
    printf( "H A9h: %ld wraps/s, range %02X-%02X; VERT_SYNC: %ld rising edges/s; %ld samples\n",
            wraps, lo, hi, edges, n );
    printf( "H LINE_SYNC toggles per frame:" );
    for( k = 0; k < nl; ++k ) {
        printf( " %ld", lines[k] );
    }
    printf( "\n" );
}

/* ---- V ---- */

/* SUBSYS_STATUS bit 0 latches vertical blank until SUBSYS_CNTL bit 0 clears
 * it (guide 8-19). Checked once per ~1 ms only: if the count still equals the
 * refresh, a frame is caught without a tight loop. Interrupt enables (bits
 * 8-11) and the reset field (bits 12-15) are left at 0. */
static void arm_v( void )
{
    double  t0;
    double  t;
    double  next;
    long    hits = 0;
    long    looks = 0;

    outpw( 0x42E8, 0x0001 );
    t0 = now_us();
    next = t0;
    do {
        t = now_us();
        if( t >= next ) {
            next += 1000.0;
            ++looks;
            if( inpw( 0x42E8 ) & 1 ) {
                ++hits;
                outpw( 0x42E8, 0x0001 );
            }
        }
    } while( t - t0 < 1e6 );
    printf( "V V_BLANK latch: %ld frames in 1 s from %ld looks\n", hits, looks );
}

/* ---- D ---- */

/* DISP_STATUS alone in a tight loop for 1 s: the time spent in each state of
 * VERT_SYNC (bit 1), so the short state is the pulse whatever its polarity,
 * and the number of pulses a second is the refresh. */
static void arm_d( void )
{
    double  t0 = now_us();
    double  t = t0;
    double  edge = t0;
    double  len[2] = { 0.0, 0.0 };
    long    cnt[2] = { 0, 0 };
    double  shortest[2] = { 1e9, 1e9 };
    long    n = 0;
    int     v = (inpw( 0x02E8 ) & 2) != 0;
    int     w;

    do {
        w = (inpw( 0x02E8 ) & 2) != 0;
        ++n;
        if( w != v ) {
            t = now_us();
            len[v] += t - edge;
            ++cnt[v];
            if( t - edge < shortest[v] ) {
                shortest[v] = t - edge;
            }
            edge = t;
            v = w;
        } else if( (n & 63) == 0 ) {
            t = now_us();
        }
    } while( t - t0 < 1e6 );
    printf( "D VERT_SYNC=0: %ld spans, mean %.0f us, min %.0f | =1: %ld spans, mean %.0f us, min %.0f | %ld reads\n",
            cnt[0], cnt[0] ? len[0] / cnt[0] : 0.0, shortest[0],
            cnt[1], cnt[1] ? len[1] / cnt[1] : 0.0, shortest[1], n );
}

/* ---- W ---- */

/* The V_BLANK latch looked at in a tight loop, cleared on each hit: the
 * intervals between hits, to see what sets it. */
static void arm_w( void )
{
    double  hit[33];
    double  t0 = now_us();
    double  t = t0;
    int     k = 0;
    int     i;

    outpw( 0x42E8, 0x0001 );
    while( k < 33 && t - t0 < 2e6 ) {
        if( inpw( 0x42E8 ) & 1 ) {
            t = now_us();
            hit[k++] = t;
            outpw( 0x42E8, 0x0001 );
        } else {
            t = now_us();
        }
    }
    printf( "W V_BLANK hits, intervals us:" );
    for( i = 1; i < k; ++i ) {
        printf( " %.0f", hit[i] - hit[i - 1] );
    }
    printf( "\n" );
}

/* ---- S ---- */

static void offset( long px )
{
    long o = px >> 2;

    outpw( 0x2EEE, (WORD)((o >> 16) & 0xF) );     /* CRT_OFFSET_HI */
    outpw( 0x2AEE, (WORD)(o & 0xFFFF) );          /* CRT_OFFSET_LO */
}

static void fill( int x, int y, int w, int h, int c )
{
    wait_empty();
    outpw( 0xA6E8, c );                 /* FRGD_COLOR */
    outpw( 0x86E8, x );                 /* CUR_X */
    outpw( 0x82E8, y );                 /* CUR_Y */
    outpw( 0x96E8, w - 1 );             /* MAJ_AXIS_PCNT */
    outpw( 0xBEE8, h - 1 );             /* MIN_AXIS_PCNT */
    outpw( 0x9AE8, 0x40F3 );            /* CMD: rectangle fill */
}

static void arm_s( void )
{
    double  t0;
    int     y;
    int     v;
    long    half;
    long    tog;
    int     lv;
    int     w;

    wait_empty();
    outpw( 0xAAE8, 0x00FF );            /* WRT_MASK */
    outpw( 0xBEE8, 0xA000 );            /* PIX_CNTL */
    outpw( 0xDAEE, 0x0800 );            /* EXT_SCISSOR_L..B: the whole range */
    outpw( 0xDEEE, 0x0800 );
    outpw( 0xE2EE, 0x07FF );
    outpw( 0xE6EE, 0x07FF );
    outpw( 0xCEEE, 0x2211 );            /* DP_CONFIG: FRGD_COLOR source */
    outpw( 0xBAEE, 0x0007 );            /* ALU_FG_FN: replace */
    for( y = 600; y < 1024; y += 32 ) {
        fill( 0, y, 1024, 16, 0x0C );   /* bars */
        fill( 0, y + 16, 1024, 16, 0x0F );
    }
    wait_empty();
    half = 0;
    t0 = now_us();
    do {
        /* Wait for the start of vertical sync (either polarity: an edge). */
        v = vsync();
        while( vsync() == v ) {
        }
        if( !vsync() ) {
            while( !vsync() ) {
            }
        }
        offset( 0 );
        /* Halfway: LINE_SYNC toggles counted from the sync. */
        lv = inpw( 0x02E8 ) & 4;
        for( tog = 0; tog < 300; ) {
            w = inpw( 0x02E8 ) & 4;
            if( w != lv ) {
                lv = w;
                ++tog;
            }
        }
        offset( 1024L * 624 );
        ++half;
    } while( now_us() - t0 < 5e6 );
    offset( 0 );
    printf( "S %ld frames split at 300 line toggles\n", half );
}

int main( int argc, char **argv )
{
    HDC     dc;

    QueryPerformanceFrequency( &freq );
    SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL );
    ram = VirtualAlloc( NULL, FRAME, MEM_COMMIT, PAGE_READWRITE );
    if( ram == NULL ) {
        printf( "no memory\n" );
        return( 1 );
    }
    r_write();
    printf( "R write 600 KB (rep stosd)    %8.0f us  %5.2f MB/s\n", best3( r_write ),
            FRAME / best3( r_write ) );
    printf( "R read  600 KB (rep lodsd)    %8.0f us  %5.2f MB/s\n", best3( r_read ),
            FRAME / best3( r_read ) );
    printf( "R one-at-a-time hash 600 KB   %8.0f us\n", best3( r_hash ) );
    printf( "O 14 OUTs + 1 FIFO wait       %8.2f us  (%.2f us/OUT if the wait is one IN)\n",
            best3( o_out ), (best3( o_out ) - best3( o_in )) / 14 );
    printf( "O one IN (EXT_FIFO_STATUS)    %8.2f us\n", best3( o_in ) );
    printf( "O 14 from memory, C loop      %8.2f us\n", best3( o_loop ) );
    printf( "O 14 from memory, rep outsw   %8.2f us\n", best3( o_rep ) );
    arm_h();
    arm_v();
    arm_d();
    arm_w();
    if( argc > 1 && argv[1][0] == 's' ) {
        arm_s();
    }
    dc = GetDC( NULL );
    printf( "resync %d\n", ExtEscape( dc, 0x4D3E, 0, NULL, 0, NULL ) );
    ReleaseDC( NULL, dc );
    return( 0 );
}
