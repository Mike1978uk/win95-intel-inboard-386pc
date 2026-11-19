/* M8DTEST.EXE - exercises every m8dos call and measures triangles a second (P3.2, P3.3).
 *
 *   M8DTEST card [seconds]        a still test card: every primitive, clipping, overlaps
 *   M8DTEST bench [tris] [size] [frames] [nv]
 *                                 random flat triangles, tris a frame (default 50) whose box
 *                                 is about size pixels (default 40), frames (default 200)
 * The report is printed and appended to M8DTEST.TXT.
 *
 * Build: BUILD.BAT. GPL-3.0-or-later, as drivers/mach8_display.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <i86.h>
#include "m8dos.h"

/* The perf mailbox as an outside reader sees it: through the pointer at 40:F0. Read while the
 * card is open, printed with the report so the two can be compared. */
static M8PERF   seen;
static int      seen_ok;

static void mailbox_read( void )
{
#ifdef __386__
    unsigned long           p = *(volatile unsigned long *)0x4F0;
    const volatile M8PERF   *m = (const volatile M8PERF *)( ( p >> 16 ) * 16 + ( p & 0xFFFF ) );
#else
    const volatile M8PERF __far *m = *(M8PERF __far * volatile __far *)MK_FP( 0x40, 0xF0 );
#endif
    unsigned long           seq;
    int                     i;

    seen_ok = 0;
    if( m == 0 || m->sig[0] != 'M' || m->sig[1] != '8' || m->sig[2] != 'P' || m->sig[3] != 'F' ) {
        return;
    }
    do {
        seq = m->seq;
        for( i = 0; i < sizeof( seen ); ++i ) {
            ((unsigned char *)&seen)[i] = ((const volatile unsigned char *)m)[i];
        }
    } while( ( seq & 1 ) || m->seq != seq );
    seen_ok = 1;
}

static void palette( void )
{
    static unsigned char    rgb[256 * 3];
    int                     i;

    for( i = 0; i < 256; ++i ) {
        rgb[i * 3 + 0] = (unsigned char)((i & 0xE0) >> 2 | (i & 0xE0) >> 5);
        rgb[i * 3 + 1] = (unsigned char)((i & 0x1C) << 1 | (i & 0x1C) >> 2);
        rgb[i * 3 + 2] = (unsigned char)((i & 0x03) << 4 | (i & 0x03) << 2 | (i & 0x03));
    }
    m8_palette( 0, 256, rgb );
}

/* Colours in the 3-3-2 palette above. */
#define RED     0xE0
#define GREEN   0x1C
#define BLUE    0x03
#define WHITE   0xFF
#define GREY    0x92
#define YELLOW  0xFC

static void card( void )
{
    M8PT    oct[8];
    M8PT    big[4];
    int     i;

    m8_clear( 0 );
    /* Corners and edges: a 1-pixel frame must show on all four sides. */
    m8_rect( 0, 0, M8_W - 1, 0, WHITE );
    m8_rect( 0, M8_H - 1, M8_W - 1, M8_H - 1, WHITE );
    m8_rect( 0, 0, 0, M8_H - 1, WHITE );
    m8_rect( M8_W - 1, 0, M8_W - 1, M8_H - 1, WHITE );
    /* Painter's order: three overlapping triangles, each must cover the one before. */
    m8_tri( 40, 40, 200, 60, 80, 200, RED );
    m8_tri( 100, 60, 240, 120, 60, 160, GREEN );
    m8_tri( 120, 90, 200, 200, 40, 120, BLUE );
    /* Non-overlapping triangles: one batch. */
    for( i = 0; i < 6; ++i ) {
        m8_tri( 280 + i * 55, 40, 320 + i * 55, 40, 300 + i * 55, 120, 0x20 * (i + 1) | 0x0C );
    }
    /* Thin shapes: one-row and two-row triangles, a one-pixel-wide sliver. */
    m8_tri( 280, 140, 600, 140, 440, 140, YELLOW );
    m8_tri( 280, 150, 600, 151, 440, 151, YELLOW );
    m8_tri( 300, 160, 301, 230, 300, 230, YELLOW );
    /* An octagon, 8 corners. */
    for( i = 0; i < 8; ++i ) {
        static const short dx[8] = { 30, 70, 100, 100, 70, 30, 0, 0 };
        static const short dy[8] = { 0, 0, 30, 70, 100, 100, 70, 30 };

        oct[i].x = (short)(340 + dx[i]);
        oct[i].y = (short)(250 + dy[i]);
    }
    m8_poly( 8, oct, GREY );
    /* Trapezoids, as a span generator would hand them over. */
    m8_trap( 250, 330, 60L << 16, 140L << 16, -(1L << 15), 1L << 15, GREEN );
    m8_trap( 340, 380, 40L << 16, 200L << 16, 1L << 16, -(1L << 16), RED );
    /* Clipping: a polygon far past the bottom-right corner, lines through all four sides. */
    big[0].x = 560; big[0].y = 400;
    big[1].x = 900; big[1].y = 420;
    big[2].x = 880; big[2].y = 900;
    big[3].x = 540; big[3].y = 700;
    m8_poly( 4, big, BLUE );
    m8_line( -50, 240, 700, 260, WHITE );
    m8_line( 320, -50, 330, 600, WHITE );
    m8_line( 460, 260, 620, 380, YELLOW );
    m8_hline( 470, -10, 700, RED );
    /* A clip window: only the part inside 480..620 x 20..120 may show. */
    m8_clip( 480, 160, 620, 230 );
    m8_rect( 400, 100, 700, 300, GREY );
    m8_tri( 470, 150, 640, 170, 520, 260, RED );
    m8_clip( 0, 0, M8_W - 1, M8_H - 1 );
}

static unsigned long seed = 12345;

static int rnd( int n )
{
    seed = seed * 1103515245UL + 12345;
    return( (int)((seed >> 16) % (unsigned)n) );
}

static void report( const char *what )
{
    M8STATS s;
    double  secs;
    char    line[600];
    FILE    *f;

    m8_stats( &s );
    secs = s.ticks / 18.2065;
    if( secs <= 0 ) {
        secs = 1 / 18.2065;
    }
    sprintf( line,
        "M8DTEST %s: %lu frames in %lu ticks = %.2f s, %.2f fps\n"
        "  port writes %lu (%.1f a frame), status reads %lu\n"
        "  polygons by fill %lu, by spans %lu (%.1f a s), rects %lu, lines %lu\n"
        "  flip wait polls %lu, timeouts %lu\n",
        what, s.frames, s.ticks, secs, s.frames / secs,
        s.outs, s.frames ? (double)s.outs / s.frames : 0.0, s.ins,
        s.polys_fill, s.polys_span, (s.polys_fill + s.polys_span) / secs, s.rects, s.lines,
        s.flip_polls, s.flip_timeouts );
    if( seen_ok ) {
        sprintf( line + strlen( line ),
            "  mailbox: frames %lu, last %lu us, avg %lu us, %lu.%02lu fps, writes %lu, reads %lu, flags %lu\n",
            seen.frames, seen.last_us, seen.avg_us, seen.fps_x100 / 100, seen.fps_x100 % 100,
            seen.outs, seen.ins, seen.flags );
    } else {
        strcat( line, "  mailbox: not found at 40:F0\n" );
    }
    fputs( line, stdout );
    f = fopen( "M8DTEST.TXT", "a" );
    if( f != NULL ) {
        fputs( line, f );
        fclose( f );
    }
}

int main( int argc, char **argv )
{
    const char  *mode = argc > 1 ? argv[1] : "card";
    char        what[80];

    if( stricmp( mode, "card" ) == 0 ) {
        unsigned long   secs = argc > 2 ? atol( argv[2] ) : 10;
        unsigned long   t;

        if( m8_open() ) {
            return( 1 );
        }
        palette();
        m8_stats_reset();
        m8_frame_begin();               /* the same card on both pages */
        card();
        m8_frame_end( 1 );
        m8_frame_begin();
        card();
        m8_frame_end( 1 );
        t = m8_ticks();
        while( m8_ticks() - t < secs * 182 / 10 && !m8_key() ) {
        }
        mailbox_read();
        m8_close();
        report( "card" );
    } else {
        int     tris = argc > 2 ? atoi( argv[2] ) : 50;
        int     size = argc > 3 ? atoi( argv[3] ) : 40;
        long    frames = argc > 4 ? atol( argv[4] ) : 200;
        int     vsync = !(argc > 5 && stricmp( argv[5], "nv" ) == 0);
        long    f;
        int     i;

        if( size < 4 ) size = 4;
        if( m8_open() ) {
            return( 1 );
        }
        palette();
        m8_stats_reset();
        for( f = 0; f < frames && !m8_key(); ++f ) {
            m8_frame_begin();
            m8_clear( 0 );
            for( i = 0; i < tris; ++i ) {
                int x = rnd( M8_W - size );
                int y = rnd( M8_H - size );

                m8_tri( x + rnd( size ), y, x + size - 1, y + rnd( size ), x, y + size - 1,
                        1 + rnd( 255 ) );
            }
            m8_frame_end( vsync );
        }
        mailbox_read();
        m8_close();
        sprintf( what, "bench %d tris size %d vsync %s", tris, size, vsync ? "on" : "off" );
        report( what );
    }
    return( 0 );
}
