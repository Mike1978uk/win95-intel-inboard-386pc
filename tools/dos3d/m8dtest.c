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
#include <math.h>
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

/* L6 probes: each option's pixels against option 0's, read back from the card, and its
 * writes and speed. Appends to M8PROBE.TXT. */
static unsigned char    *page0;
static unsigned char    *pagex;

static void probe_line( const char *text )
{
    FILE    *f;

    fputs( text, stdout );
    f = fopen( "M8PROBE.TXT", "a" );
    if( f != NULL ) {
        fputs( text, f );
        fclose( f );
    }
}

static void probe_diff( const char *what, int opt, unsigned long writes, unsigned char *ref,
                        unsigned char *got )
{
    char            line[200];
    long            i;
    long            ndiff = 0;
    int             l = M8_W, t = M8_H, r = -1, b = -1;
    int             shown;
    long            drawn = 0;

    /* Two blank pages are identical: say how much the reference holds. */
    for( i = 0; i < (long)M8_W * M8_H; ++i ) {
        drawn += ref[i] != 0;
    }
    sprintf( line, "  %s: reference has %ld non-background pixels\n", what, drawn );
    probe_line( line );
    for( i = 0; i < (long)M8_W * M8_H; ++i ) {
        if( ref[i] != got[i] ) {
            int x = (int)( i % M8_W );
            int y = (int)( i / M8_W );

            ++ndiff;
            if( x < l ) l = x;
            if( x > r ) r = x;
            if( y < t ) t = y;
            if( y > b ) b = y;
        }
    }
    if( ndiff ) {
        sprintf( line, "  %s opt %d: %lu writes, %ld pixels differ, box %d,%d-%d,%d\n", what, opt,
                 writes, ndiff, l, t, r, b );
    } else {
        sprintf( line, "  %s opt %d: %lu writes, pixels identical\n", what, opt, writes );
    }
    probe_line( line );
    shown = 0;
    for( i = 0; i < (long)M8_W * M8_H && shown < 4; ++i ) {
        if( ref[i] != got[i] ) {
            sprintf( line, "    (%ld,%ld) %02X not %02X\n", i % M8_W, i / M8_W, got[i], ref[i] );
            probe_line( line );
            ++shown;
        }
    }
}

/* Random convex polygons and triangles of both windings, deterministic from seed: the
 * shapes the bench's fixed-shape triangles never produce. */
static void scene( int frame )
{
    int     i;
    int     k;

    seed = 1000u + (unsigned)frame;
    m8_clear( 0 );
    if( frame >= 4 ) {
        /* The middle vertex's row where shallow edges meet, either side, either winding. */
        for( i = 0; i < 120; ++i ) {
            int x = 10 + rnd( M8_W - 220 );
            int y = 10 + rnd( M8_H - 40 );
            int w = 20 + rnd( 190 );
            int h = 3 + rnd( 24 );
            int m = 1 + rnd( h - 1 );
            int bx = rnd( 2 ) ? x - rnd( 10 ) : x + w + rnd( 10 );
            int c = 1 + rnd( 255 );

            if( rnd( 2 ) ) {
                m8_tri( x + rnd( w ), y, bx, y + m, x + rnd( w ), y + h, c );
            } else {
                m8_tri( x + rnd( w ), y, x + rnd( w ), y + h, bx, y + m, c );
            }
        }
        return;
    }
    for( i = 0; i < 40; ++i ) {
        int     n = 3 + rnd( i < 20 ? 1 : 6 );
        int     cx = 70 + rnd( M8_W - 140 );
        int     cy = 70 + rnd( M8_H - 140 );
        int     r = 8 + rnd( 60 );
        int     rev = rnd( 2 );
        double  ang[8];
        M8PT    v[8];

        for( k = 0; k < n; ++k ) {
            ang[k] = rnd( 3600 ) * 3.14159265 / 1800.0;
        }
        for( k = 1; k < n; ++k ) {              /* sort: a convex fan around the centre */
            double  t = ang[k];
            int     m = k;

            while( m > 0 && ang[m - 1] > t ) {
                ang[m] = ang[m - 1];
                --m;
            }
            ang[m] = t;
        }
        for( k = 0; k < n; ++k ) {
            int q = rev ? n - 1 - k : k;

            v[k].x = (short)( cx + r * cos( ang[q] ) * ( 1 + rnd( 3 ) ) / 2 );
            v[k].y = (short)( cy + r * sin( ang[q] ) );
        }
        if( n == 3 ) {
            m8_tri( v[0].x, v[0].y, v[1].x, v[1].y, v[2].x, v[2].y, 1 + rnd( 255 ) );
        } else {
            m8_poly( n, v, 1 + rnd( 255 ) );
        }
    }
}

/* L6: the scratch outline as one closed figure (the default) and as two top-down chains,
 * against one edge at a time, read back from the card frame by frame; then speed. */
static int probe( void )
{
    static const int    opts[] = { M8_OPT_OUTLINE | M8_OPT_EDGES, M8_OPT_OUTLINE, M8_OPT_OUTLINE | M8_OPT_CHAINS };
    static const char   *name[] = { "edges (reference)", "closed (default)", "chains" };
    char                line[200];
    int                 k;
    int                 fr;
    int                 i;
    long                f;
    M8STATS             s;

    page0 = (unsigned char *)malloc( (size_t)M8_W * M8_H );
    pagex = (unsigned char *)malloc( (size_t)M8_W * M8_H );
    if( page0 == NULL || pagex == NULL ) {
        printf( "M8DTEST probe: no memory for two pages\n" );
        return( 1 );
    }
    probe_line( "M8DTEST probe: scratch outline closed / chains, against edges\n" );
    for( fr = 0; fr < 8; ++fr ) {
        for( k = 0; k < 3; ++k ) {
            m8_opt = opts[k];
            if( m8_open() ) {
                return( 1 );
            }
            palette();
            m8_stats_reset();
            m8_frame_begin();
            scene( fr );
            m8_frame_end( 1 );
            m8_stats( &s );
            if( m8_read_shown( k == 0 ? page0 : pagex ) ) {
                m8_close();
                printf( "M8DTEST probe: read failed\n" );
                return( 1 );
            }
            m8_close();
            if( k == 0 ) {
                sprintf( line, " frame %d %s: %lu writes\n", fr, name[k], s.outs );
                probe_line( line );
            } else {
                sprintf( line, " frame %d %s", fr, name[k] );
                probe_diff( line, opts[k], s.outs, page0, pagex );
            }
        }
    }
    for( k = 0; k < 3; ++k ) {
        unsigned long   ticks;

        m8_opt = opts[k];
        seed = 12345;
        if( m8_open() ) {
            return( 1 );
        }
        palette();
        m8_stats_reset();
        for( f = 0; f < 200; ++f ) {
            m8_frame_begin();
            m8_clear( 0 );
            for( i = 0; i < 50; ++i ) {
                int x = rnd( M8_W - 40 );
                int y = rnd( M8_H - 40 );

                m8_tri( x + rnd( 40 ), y, x + 39, y + rnd( 40 ), x, y + 39, 1 + rnd( 255 ) );
            }
            m8_frame_end( 0 );
        }
        m8_stats( &s );
        ticks = s.ticks ? s.ticks : 1;
        m8_close();
        sprintf( line, " bench 50x40 nv %s: %.2f fps, %.1f writes a frame\n", name[k],
                 s.frames * 18.2065 / ticks, (double)s.outs / s.frames );
        probe_line( line );
    }
    m8_opt = 0;
    return( 0 );
}

/* L6 P4: 15 overlapping triangles, one a depth, drawn four ways and read back from the
 * card. Reference: far to near with no test. With the hardware test, random order and near to
 * far must equal it; random order with no test (the control) must not. */
static void zpalette( void )
{
    static unsigned char    rgb[256 * 3];
    static const unsigned char hue[16][3] = {
        { 0, 0, 0 }, { 63, 0, 0 }, { 0, 63, 0 }, { 0, 0, 63 }, { 63, 63, 0 }, { 63, 0, 63 },
        { 0, 63, 63 }, { 63, 63, 63 }, { 63, 32, 0 }, { 32, 63, 0 }, { 0, 32, 63 },
        { 63, 0, 32 }, { 32, 0, 63 }, { 0, 63, 32 }, { 40, 40, 40 }, { 63, 48, 48 } };
    int                     i;
    int                     k;

    for( i = 0; i < 256; ++i ) {
        int d = i >> 4;

        for( k = 0; k < 3; ++k ) {
            rgb[i * 3 + k] = (unsigned char)( hue[i & 15][k] * ( 20 - d ) / 20 );
        }
    }
    m8_palette( 0, 256, rgb );
}

static int  zx[15][3], zy[15][3], zc[15];

static void zdraw( int how, const int *order )
{
    int i;

    m8_frame_begin();
    m8_depth( -1 );
    m8_clear( 0xF0 );
    for( i = 0; i < 15; ++i ) {
        int d = how == 0 ? 14 - i : how == 2 ? i : order[i];

        m8_depth( how == 1 || how == 2 ? d : -1 );
        m8_tri( zx[d][0], zy[d][0], zx[d][1], zy[d][1], zx[d][2], zy[d][2], d << 4 | zc[d] );
    }
    m8_depth( -1 );
    m8_frame_end( 1 );
}

static int zprobe( void )
{
    static const char   *name[] = { "far to near, no test (reference)", "random order, test",
                                    "near to far, test", "random order, no test (control)" };
    char                line[200];
    int                 order[15];
    int                 fr;
    int                 how;
    int                 i;
    M8STATS             s;

    page0 = (unsigned char *)malloc( (size_t)M8_W * M8_H );
    pagex = (unsigned char *)malloc( (size_t)M8_W * M8_H );
    if( page0 == NULL || pagex == NULL ) {
        printf( "M8DTEST zprobe: no memory for two pages\n" );
        return( 1 );
    }
    probe_line( "M8DTEST zprobe: hardware depth test (DEST_CMP_FN <, COLOR_CMP = depth x 16)\n" );
    for( fr = 0; fr < 4; ++fr ) {
        seed = 777u + (unsigned)fr;
        for( i = 0; i < 15; ++i ) {
            int cx = 100 + rnd( M8_W - 200 );
            int cy = 100 + rnd( M8_H - 200 );
            int k;

            for( k = 0; k < 3; ++k ) {
                zx[i][k] = cx - 150 + rnd( 300 );
                zy[i][k] = cy - 120 + rnd( 240 );
            }
            zc[i] = 1 + rnd( 15 );
            order[i] = i;
        }
        for( i = 14; i > 0; --i ) {
            int k = rnd( i + 1 );
            int t = order[i];

            order[i] = order[k];
            order[k] = t;
        }
        for( how = 0; how < 4; ++how ) {
            if( m8_open() ) {
                return( 1 );
            }
            zpalette();
            m8_stats_reset();
            zdraw( how == 3 ? 1 : how, order );
            if( how == 3 ) {
                /* the control: the same random order with the test off */
                m8_frame_begin();
                m8_depth( -1 );
                m8_clear( 0xF0 );
                for( i = 0; i < 15; ++i ) {
                    int d = order[i];

                    m8_tri( zx[d][0], zy[d][0], zx[d][1], zy[d][1], zx[d][2], zy[d][2],
                            d << 4 | zc[d] );
                }
                m8_frame_end( 1 );
            }
            m8_stats( &s );
            if( m8_read_shown( how == 0 ? page0 : pagex ) ) {
                m8_close();
                printf( "M8DTEST zprobe: read failed\n" );
                return( 1 );
            }
            m8_close();
            if( how == 0 ) {
                sprintf( line, " frame %d %s: %lu writes\n", fr, name[how], s.outs );
                probe_line( line );
            } else {
                sprintf( line, " frame %d %s", fr, name[how] );
                probe_diff( line, how, s.outs, page0, pagex );
            }
        }
    }
    return( 0 );
}

/* Where a bench frame's time goes: the 50-triangle bench as it is, without its full-screen
 * clear, and the clear alone; vsync off, 200 frames each. */
static int costs( void )
{
    static const char   *name[] = { "clear + 50 tris", "50 tris, no clear", "clear only" };
    char                line[200];
    int                 k;
    int                 i;
    long                f;
    M8STATS             s;

    probe_line( "M8DTEST costs: bench 50x40, vsync off, 200 frames\n" );
    for( k = 0; k < 3; ++k ) {
        unsigned long   ticks;

        seed = 12345;
        if( m8_open() ) {
            return( 1 );
        }
        palette();
        m8_stats_reset();
        for( f = 0; f < 200; ++f ) {
            m8_frame_begin();
            if( k != 1 ) {
                m8_clear( 0 );
            }
            for( i = 0; k != 2 && i < 50; ++i ) {
                int x = rnd( M8_W - 40 );
                int y = rnd( M8_H - 40 );

                m8_tri( x + rnd( 40 ), y, x + 39, y + rnd( 40 ), x, y + 39, 1 + rnd( 255 ) );
            }
            m8_frame_end( 0 );
        }
        m8_stats( &s );
        ticks = s.ticks ? s.ticks : 1;
        m8_close();
        sprintf( line, " %-18s %6.2f fps %6.2f ms a frame, %7.1f writes, %7.1f status reads a frame\n",
                 name[k], s.frames * 18.2065 / ticks, ticks * 1000.0 / 18.2065 / s.frames,
                 (double)s.outs / s.frames, (double)s.ins / s.frames );
        probe_line( line );
    }
    return( 0 );
}

/* L6 P3b: which outline pixels does the fill alone leave out? Separate shapes of every
 * orientation and two neighbours sharing an edge, with and without the page outline. For each
 * shape colour, its pixels in the reference are counted by the sides they lie on (left, right,
 * top, bottom: a neighbour of another colour), and those missing without the outline. */
static int edgeprobe( void )
{
    static const short  sh[][8] = {             /* n, then up to 3 corners as x,y pairs ... */
        { 3,  60, 20,  150, 120,   10, 120 },   /* apex up, flat bottom */
        { 3, 170, 20,  280,  20,  230, 120 },   /* flat top, apex down */
        { 3, 300, 20,  420,  60,  320, 130 },   /* general, middle right */
        { 3, 520, 20,  440,  70,  600, 130 },   /* general, middle left */
        { 3,  20, 160,  60, 300,   40, 300 },   /* steep */
        { 3, 100, 180, 300, 200,  120, 230 },   /* shallow */
        { 3, 340, 160, 460, 160,  340, 280 },   /* right angle, left and top straight */
        { 3, 600, 160, 600, 280,  480, 280 },   /* right angle, right and bottom straight */
    };
    char            line[200];
    unsigned long   tot[16][4], miss[16][4];
    int             k;
    int             how;
    int             x;
    int             y;
    M8STATS         s;

    page0 = (unsigned char *)malloc( (size_t)M8_W * M8_H );
    pagex = (unsigned char *)malloc( (size_t)M8_W * M8_H );
    if( page0 == NULL || pagex == NULL ) {
        return( 1 );
    }
    for( how = 0; how < 2; ++how ) {
        m8_opt = how ? M8_OPT_OUTLINE | M8_OPT_NOOUTLINE : M8_OPT_OUTLINE;
        if( m8_open() ) {
            return( 1 );
        }
        palette();
        m8_stats_reset();
        m8_frame_begin();
        m8_clear( 0 );
        for( k = 0; k < 8; ++k ) {
            m8_tri( sh[k][1], sh[k][2], sh[k][3], sh[k][4], sh[k][5], sh[k][6], 0x20 + k );
        }
        /* two neighbours sharing the edge (150,330)-(250,460), and a quad */
        m8_tri( 150, 330, 250, 460, 60, 440, 0x28 );
        m8_tri( 150, 330, 330, 360, 250, 460, 0x29 );
        {
            M8PT    q[4];

            q[0].x = 480; q[0].y = 320; q[1].x = 600; q[1].y = 380;
            q[2].x = 500; q[2].y = 470; q[3].x = 380; q[3].y = 400;
            m8_poly( 4, q, 0x2A );
        }
        m8_frame_end( 1 );
        m8_stats( &s );
        if( m8_read_shown( how ? pagex : page0 ) ) {
            m8_close();
            return( 1 );
        }
        m8_close();
        sprintf( line, "M8DTEST edgeprobe %s: %lu writes\n", how ? "no outline" : "reference", s.outs );
        probe_line( line );
    }
    m8_opt = 0;
    memset( tot, 0, sizeof( tot ) );
    memset( miss, 0, sizeof( miss ) );
    for( y = 1; y < M8_H - 1; ++y ) {
        for( x = 1; x < M8_W - 1; ++x ) {
            int c = page0[y * M8_W + x];
            int side[4];
            int d;

            if( c < 0x20 || c > 0x2A ) {
                continue;
            }
            side[0] = page0[y * M8_W + x - 1] != c;
            side[1] = page0[y * M8_W + x + 1] != c;
            side[2] = page0[( y - 1 ) * M8_W + x] != c;
            side[3] = page0[( y + 1 ) * M8_W + x] != c;
            for( d = 0; d < 4; ++d ) {
                if( side[d] ) {
                    ++tot[c - 0x20][d];
                    if( pagex[y * M8_W + x] != c ) {
                        ++miss[c - 0x20][d];
                    }
                }
            }
            if( !side[0] && !side[1] && !side[2] && !side[3] && pagex[y * M8_W + x] != c ) {
                ++miss[c - 0x20][0], ++tot[15][0];      /* an inside pixel missing: counted apart */
            }
        }
    }
    probe_line( "  shape  left missing/edge   right    top    bottom\n" );
    for( k = 0; k < 11; ++k ) {
        sprintf( line, "  %2d     %4lu/%-4lu   %4lu/%-4lu  %4lu/%-4lu  %4lu/%-4lu\n", k,
                 miss[k][0], tot[k][0], miss[k][1], tot[k][1], miss[k][2], tot[k][2],
                 miss[k][3], tot[k][3] );
        probe_line( line );
    }
    sprintf( line, "  inside pixels missing (all shapes): %lu\n", tot[15][0] );
    probe_line( line );
    return( 0 );
}

/* L6 P3b: the top-left fill rule. A jittered mesh of 160 triangles drawn forwards and
 * backwards: with each pixel in exactly one triangle the two are identical, and no background
 * pixel is left inside the mesh. Then the bench, default against the rule. */
static short    mx[41][33], my[41][33];
static int      mcol[2560];
static int      mnx = 10, mny = 8;

static void mesh( int forwards )
{
    int i;

    m8_frame_begin();
    m8_clear( 0 );
    for( i = 0; i < 2 * mnx * mny; ++i ) {
        int t = forwards ? i : 2 * mnx * mny - 1 - i;
        int q = t >> 1;
        int gx = q % mnx;
        int gy = q / mnx;

        if( t & 1 ) {
            m8_tri( mx[gx][gy], my[gx][gy], mx[gx + 1][gy + 1], my[gx + 1][gy + 1],
                    mx[gx][gy + 1], my[gx][gy + 1], mcol[t] );
        } else {
            m8_tri( mx[gx][gy], my[gx][gy], mx[gx + 1][gy], my[gx + 1][gy],
                    mx[gx + 1][gy + 1], my[gx + 1][gy + 1], mcol[t] );
        }
    }
    m8_frame_end( 1 );
}

static int tileprobe( void )
{
    static const int    opts[] = { M8_OPT_OUTLINE, 0 };
    static const char   *name[] = { "outline rule", "top-left rule (default)" };
    char                line[200];
    int                 k;
    int                 i;
    int                 x;
    int                 y;
    long                f;
    unsigned long       holes;
    M8STATS             s;
    int                 fine;

    page0 = (unsigned char *)malloc( (size_t)M8_W * M8_H );
    pagex = (unsigned char *)malloc( (size_t)M8_W * M8_H );
    if( page0 == NULL || pagex == NULL ) {
        return( 1 );
    }
  for( fine = 0; fine < 2; ++fine ) {
    int cw = fine ? 15 : 60;
    int chh = fine ? 14 : 55;
    int jit = fine ? 4 : 15;

    mnx = fine ? 40 : 10;
    mny = fine ? 32 : 8;
    seed = 4242;
    for( x = 0; x <= mnx; ++x ) {
        for( y = 0; y <= mny; ++y ) {
            int jx = ( x == 0 || x == mnx ) ? 0 : rnd( 2 * jit + 1 ) - jit;
            int jy = ( y == 0 || y == mny ) ? 0 : rnd( 2 * jit + 1 ) - jit;

            mx[x][y] = (short)( 20 + x * cw + jx );
            my[x][y] = (short)( 20 + y * chh + jy );
        }
    }
    for( i = 0; i < 2 * mnx * mny; ++i ) {
        mcol[i] = 1 + rnd( 255 );
    }
    sprintf( line, "M8DTEST tileprobe: jittered mesh, %d triangles, forwards and backwards\n", 2 * mnx * mny );
    probe_line( line );
    for( k = 0; k < 2; ++k ) {
        m8_opt = opts[k];
        if( m8_open() ) {
            return( 1 );
        }
        palette();
        m8_stats_reset();
        mesh( 1 );
        m8_stats( &s );
        if( m8_read_shown( page0 ) ) {
            m8_close();
            return( 1 );
        }
        mesh( 0 );
        if( m8_read_shown( pagex ) ) {
            m8_close();
            return( 1 );
        }
        m8_close();
        holes = 0;
        for( y = 21; y < 20 + mny * chh - 1; ++y ) {
            for( x = 21; x < 20 + mnx * cw - 1; ++x ) {
                if( page0[y * M8_W + x] == 0 ) {
                    ++holes;
                }
            }
        }
        sprintf( line, " %s: %lu writes a mesh, %lu background pixels inside", name[k], s.outs, holes );
        probe_line( line );
        probe_diff( "; forwards vs backwards", opts[k], s.outs, page0, pagex );
    }
  }
    for( k = 0; k < 2; ++k ) {
        unsigned long   ticks;

        m8_opt = opts[k];
        seed = 12345;
        if( m8_open() ) {
            return( 1 );
        }
        palette();
        m8_stats_reset();
        for( f = 0; f < 200; ++f ) {
            m8_frame_begin();
            m8_clear( 0 );
            for( i = 0; i < 50; ++i ) {
                int bx = rnd( M8_W - 40 );
                int by = rnd( M8_H - 40 );

                m8_tri( bx + rnd( 40 ), by, bx + 39, by + rnd( 40 ), bx, by + 39, 1 + rnd( 255 ) );
            }
            m8_frame_end( 0 );
        }
        m8_stats( &s );
        ticks = s.ticks ? s.ticks : 1;
        m8_close();
        sprintf( line, " bench 50x40 nv %s: %.2f fps, %.2f ms, %.1f writes a frame\n", name[k],
                 s.frames * 18.2065 / ticks, ticks * 1000.0 / 18.2065 / s.frames, (double)s.outs / s.frames );
        probe_line( line );
    }
    m8_opt = 0;
    return( 0 );
}

/* L6 joined patches: the jittered mesh in same-colour patches of p x p quads. Joined, forwards
 * and backwards, must give the same pixels as unjoined; then writes, joins and the bench. */
static unsigned char    *pagej;

static int joinprobe( int patch )
{
    static const int    opts[] = { M8_OPT_NOJOIN, 0 };
    static const char   *name[] = { "unjoined", "joined" };
    char                line[200];
    int                 k;
    int                 i;
    int                 x;
    int                 y;
    int                 fine;
    long                f;
    M8STATS             s;

    page0 = (unsigned char *)malloc( (size_t)M8_W * M8_H );
    pagex = (unsigned char *)malloc( (size_t)M8_W * M8_H );
    pagej = (unsigned char *)malloc( (size_t)M8_W * M8_H );
    if( page0 == NULL || pagex == NULL || pagej == NULL ) {
        return( 1 );
    }
    if( patch < 1 ) {
        patch = 2;
    }
  for( fine = 0; fine < 2; ++fine ) {
    int cw = fine ? 15 : 60;
    int chh = fine ? 14 : 55;
    int jit = fine ? 4 : 15;
    int pp = fine ? 2 * patch : patch;

    mnx = fine ? 40 : 10;
    mny = fine ? 32 : 8;
    seed = 4242;
    for( x = 0; x <= mnx; ++x ) {
        for( y = 0; y <= mny; ++y ) {
            int jx = ( x == 0 || x == mnx ) ? 0 : rnd( 2 * jit + 1 ) - jit;
            int jy = ( y == 0 || y == mny ) ? 0 : rnd( 2 * jit + 1 ) - jit;

            mx[x][y] = (short)( 20 + x * cw + jx );
            my[x][y] = (short)( 20 + y * chh + jy );
        }
    }
    for( i = 0; i < 2 * mnx * mny; ++i ) {
        int q = i >> 1;

        mcol[i] = 1 + ( ( q % mnx ) / pp * 37 + ( q / mnx ) / pp * 101 ) % 255;
    }
    sprintf( line, "M8DTEST joinprobe: jittered mesh, %d triangles, patches of %d x %d quads\n",
             2 * mnx * mny, pp, pp );
    probe_line( line );
    for( k = 0; k < 2; ++k ) {
        m8_opt = opts[k];
        if( m8_open() ) {
            return( 1 );
        }
        palette();
        m8_stats_reset();
        mesh( 1 );
        m8_stats( &s );
        if( m8_read_shown( k ? pagex : page0 ) ) {
            m8_close();
            return( 1 );
        }
        if( k ) {
            mesh( 0 );
            if( m8_read_shown( pagej ) ) {
                m8_close();
                return( 1 );
            }
        }
        m8_close();
        sprintf( line, " %s: %lu writes a mesh, %lu fills, %lu joined\n", name[k], s.outs,
                 s.polys_fill - s.joins, s.joins );
        probe_line( line );
    }
    probe_diff( "joined forwards vs unjoined", 0, 0, page0, pagex );
    probe_diff( "joined backwards vs unjoined", 0, 0, page0, pagej );
    for( k = 0; k < 2; ++k ) {
        unsigned long   ticks;

        m8_opt = opts[k];
        if( m8_open() ) {
            return( 1 );
        }
        palette();
        m8_stats_reset();
        for( f = 0; f < 100; ++f ) {
            mesh( 1 );
        }
        m8_stats( &s );
        ticks = s.ticks ? s.ticks : 1;
        m8_close();
        sprintf( line, " mesh x100 %s: %.2f fps, %.2f ms, %.1f writes a frame\n", name[k],
                 s.frames * 18.2065 / ticks, ticks * 1000.0 / 18.2065 / s.frames, (double)s.outs / s.frames );
        probe_line( line );
    }
  }
    m8_opt = 0;
    return( 0 );
}

/* L6 P5: a 16 x 16 checkered floor in perspective, as 256 quads and as one parity fill
 * (interior lines once, the boundary pieces an odd number of half-planes hold, one fill over
 * a background quad). Pixels against the quads, writes, speed; both pages to CHKQ/CHKP.RAW. */
#define CHK_N   16
static short    cgx[CHK_N + 1][CHK_N + 1], cgy[CHK_N + 1][CHK_N + 1];

static void chk_grid( double ang )
{
    int     i;
    int     j;
    double  ca = cos( ang );
    double  sa = sin( ang );

    for( i = 0; i <= CHK_N; ++i ) {
        for( j = 0; j <= CHK_N; ++j ) {
            double  u = i - CHK_N / 2.0;
            double  v = j - CHK_N / 2.0;
            double  wx = u * ca - v * sa;
            double  wz = 18.0 + u * sa + v * ca;    /* 320 px wide at most: the scratch takes 382 */

            cgx[i][j] = (short)floor( 320.0 + wx * 200.0 / wz + 0.5 );
            cgy[i][j] = (short)floor( 60.0 + 12.0 * 200.0 / wz + 0.5 );
        }
    }
}

static void chk_quads( int ca, int cb )
{
    int     i;
    int     j;
    M8PT    q[4];

    for( i = 0; i < CHK_N; ++i ) {
        for( j = 0; j < CHK_N; ++j ) {
            q[0].x = cgx[i][j];         q[0].y = cgy[i][j];
            q[1].x = cgx[i + 1][j];     q[1].y = cgy[i + 1][j];
            q[2].x = cgx[i + 1][j + 1]; q[2].y = cgy[i + 1][j + 1];
            q[3].x = cgx[i][j + 1];     q[3].y = cgy[i][j + 1];
            if( ( i + j ) & 1 || cb >= 0 ) {     /* cb -1: B is the background already there */
                m8_poly( 4, q, ( i + j ) & 1 ? ca : cb );
            }
        }
    }
}

static void chk_seg( M8SEG *s, int *n, int x0, int y0, int x1, int y1 )
{
    s[*n].x0 = (short)x0;
    s[*n].y0 = (short)y0;
    s[*n].x1 = (short)x1;
    s[*n].y1 = (short)y1;
    s[*n].planes = 1;
    ++*n;
}

/* Tile (i, j) lies in (15 - i) + (15 - j) half-planes (u < line, v < line): odd = colour A,
 * as the quads' (i + j) & 1. A boundary piece is drawn where its half-plane count is odd. */
static void chk_parity( int ca, int cb )
{
    static M8SEG    s[4 * CHK_N + 2 * CHK_N];
    M8PT            q[4];
    int             n = 0;
    int             k;

    q[0].x = cgx[0][0];         q[0].y = cgy[0][0];
    q[1].x = cgx[CHK_N][0];     q[1].y = cgy[CHK_N][0];
    q[2].x = cgx[CHK_N][CHK_N]; q[2].y = cgy[CHK_N][CHK_N];
    q[3].x = cgx[0][CHK_N];     q[3].y = cgy[0][CHK_N];
    if( cb >= 0 ) {
        m8_poly( 4, q, cb );
    }
    for( k = 1; k < CHK_N; ++k ) {
        chk_seg( s, &n, cgx[k][0], cgy[k][0], cgx[k][CHK_N], cgy[k][CHK_N] );
        chk_seg( s, &n, cgx[0][k], cgy[0][k], cgx[CHK_N][k], cgy[CHK_N][k] );
    }
    for( k = 0; k < CHK_N; ++k ) {
        if( k & 1 ) {           /* u = 0 between v = k and k + 1: 15 + (15 - k) */
            chk_seg( s, &n, cgx[0][k], cgy[0][k], cgx[0][k + 1], cgy[0][k + 1] );
            chk_seg( s, &n, cgx[k][0], cgy[k][0], cgx[k + 1][0], cgy[k + 1][0] );
        } else {                /* u = 16: 15 - k; v = 16 likewise */
            chk_seg( s, &n, cgx[CHK_N][k], cgy[CHK_N][k], cgx[CHK_N][k + 1], cgy[CHK_N][k + 1] );
            chk_seg( s, &n, cgx[k][CHK_N], cgy[k][CHK_N], cgx[k + 1][CHK_N], cgy[k + 1][CHK_N] );
        }
    }
    m8_parity( n, s, 1, &ca );
}

static void chk_save( const char *name, const unsigned char *buf )
{
    FILE *f = fopen( name, "wb" );

    if( f != NULL ) {
        fwrite( buf, 1, (size_t)M8_W * M8_H, f );
        fclose( f );
    }
}

/* Two vertical segments: a 100 x 100 white block, and one quad, counted on the shown page. */
static int parprobe( int opt )
{
    M8SEG           sg[2];
    M8PT            q[4];
    int             col = WHITE;
    char            line[120];
    long            i;
    unsigned long   w = 0;
    unsigned long   red = 0;

    page0 = (unsigned char *)malloc( (size_t)M8_W * M8_H );
    if( page0 == NULL ) {
        return( 1 );
    }
    m8_opt = opt;
    if( m8_open() ) {
        return( 1 );
    }
    palette();
    m8_frame_begin();
    m8_clear( 0 );
    sg[0].x0 = 100; sg[0].y0 = 100; sg[0].x1 = 100; sg[0].y1 = 200; sg[0].planes = 1;
    sg[1].x0 = 200; sg[1].y0 = 100; sg[1].x1 = 200; sg[1].y1 = 200; sg[1].planes = 1;
    m8_parity( 2, sg, 1, &col );
    q[0].x = 300; q[0].y = 100; q[1].x = 400; q[1].y = 100;
    q[2].x = 400; q[2].y = 200; q[3].x = 300; q[3].y = 200;
    m8_poly( 4, q, RED );
    m8_frame_end( 1 );
    if( m8_read_shown( page0 ) ) {
        m8_close();
        return( 1 );
    }
    m8_close();
    for( i = 0; i < (long)M8_W * M8_H; ++i ) {
        w += page0[i] == WHITE;
        red += page0[i] == RED;
    }
    sprintf( line, "M8DTEST parprobe opt %d: %lu white (10000 wanted), %lu red (10000 wanted), err %s\n",
             opt, w, red, m8_error() ? m8_error() : "none" );
    probe_line( line );
    return( 0 );
}

static int checkprobe( void )
{
    static const double angs[] = { 0.0, 0.35 };
    char                line[200];
    char                name[16];
    int                 a;
    int                 k;
    long                f;
    M8STATS             s;

    page0 = (unsigned char *)malloc( (size_t)M8_W * M8_H );
    pagex = (unsigned char *)malloc( (size_t)M8_W * M8_H );
    if( page0 == NULL || pagex == NULL ) {
        return( 1 );
    }
    for( a = 0; a < 4; ++a ) {
        int cb = a & 2 ? -1 : RED;      /* 2, 3: B is the cleared background, not drawn */

        chk_grid( angs[a & 1] );
        sprintf( line, "M8DTEST checkprobe: %d x %d floor, turned %.2f rad, B %s\n", CHK_N, CHK_N, angs[a & 1],
                 cb < 0 ? "= background" : "red" );
        probe_line( line );
        for( k = 0; k < 2; ++k ) {
            unsigned long   ticks;
            unsigned long   w;

            if( m8_open() ) {
                return( 1 );
            }
            palette();
            m8_stats_reset();
            m8_frame_begin();
            m8_clear( 0 );
            if( k ) chk_parity( WHITE, cb ); else chk_quads( WHITE, cb );
            m8_frame_end( 1 );
            m8_stats( &s );
            w = s.outs;
            if( m8_read_shown( k ? pagex : page0 ) ) {
                m8_close();
                return( 1 );
            }
            m8_stats_reset();
            for( f = 0; f < 50; ++f ) {
                m8_frame_begin();
                m8_clear( 0 );
                if( k ) chk_parity( WHITE, cb ); else chk_quads( WHITE, cb );
                m8_frame_end( 0 );
            }
            m8_stats( &s );
            ticks = s.ticks ? s.ticks : 1;
            m8_close();
            sprintf( line, " %s: %lu writes a frame, %.2f ms a frame (50 nv)\n", k ? "parity" : "quads ",
                     w, ticks * 1000.0 / 18.2065 / s.frames );
            probe_line( line );
            sprintf( name, "CHK%c%d.RAW", k ? 'P' : 'Q', a );
            chk_save( name, k ? pagex : page0 );
        }
        probe_diff( "parity vs quads", 0, 0, page0, pagex );
    }
    return( 0 );
}

/* L6 P5b: a Penrose rhomb patch from de Bruijn's pentagrid. Corners are integer 5-tuples, so
 * neighbours share corners exactly and every edge is shared whole (no T-junctions). The tiles
 * are coloured so no two sharing an edge match; each edge goes once into the planes of the
 * tiles on both sides, then one fill a colour. Against the rhombs as polygons: pixels, writes,
 * speed. Both pages to PENQ / PENP.RAW. */
#ifdef __386__
#define PEN_T   1200
#else
#define PEN_T   160
#endif
#define PEN_V   ( PEN_T + 64 )
#define PEN_E   ( 2 * PEN_T + 64 )

typedef struct {
    signed char k[5];
    short       x, y;
} PENV;

static PENV     *pv;
static int      npv;
static short    (*pt)[4];               /* tile corners, vertex numbers */
static unsigned char *pcls;             /* tile colour class */
static int      npt;
static short    (*pe)[4];               /* edge: vertex a, b, tile 0, tile 1 (-1 none) */
static int      npe;
static short    (*pn)[4];               /* tile neighbours across an edge, -1 none */

static int pen_vertex( const signed char *k, double sc )
{
    int     i;
    int     j;
    double  x = 0.0;
    double  y = 0.0;

    for( i = 0; i < npv; ++i ) {
        if( memcmp( pv[i].k, k, 5 ) == 0 ) {
            return( i );
        }
    }
    if( npv == PEN_V ) {
        return( -1 );
    }
    for( j = 0; j < 5; ++j ) {
        x += k[j] * cos( 2.0 * 3.14159265358979 * j / 5.0 );
        y += k[j] * sin( 2.0 * 3.14159265358979 * j / 5.0 );
    }
    memcpy( pv[npv].k, k, 5 );
    pv[npv].x = (short)floor( 320.0 + x * sc + 0.5 );
    pv[npv].y = (short)floor( 240.0 + y * sc + 0.5 );
    return( npv++ );
}

static void pen_edge( int a, int b, int t )
{
    int i;

    for( i = 0; i < npe; ++i ) {
        if( ( pe[i][0] == a && pe[i][1] == b ) || ( pe[i][0] == b && pe[i][1] == a ) ) {
            pe[i][3] = (short)t;
            return;
        }
    }
    if( npe < PEN_E ) {
        pe[npe][0] = (short)a;
        pe[npe][1] = (short)b;
        pe[npe][2] = (short)t;
        pe[npe][3] = -1;
        ++npe;
    }
}

/* Rhombs whose centre lies within r units of the origin, sc pixels a unit. */
static void pen_build( double r, double sc )
{
    static const double g[5] = { 0.13, 0.27, -0.31, 0.44, -0.53 };    /* sum 0: Penrose */
    double  ex[5];
    double  ey[5];
    int     a;
    int     b;
    int     ka;
    int     kb;
    int     j;
    int     n = (int)r + 2;

    npv = npt = npe = 0;
    for( j = 0; j < 5; ++j ) {
        ex[j] = cos( 2.0 * 3.14159265358979 * j / 5.0 );
        ey[j] = sin( 2.0 * 3.14159265358979 * j / 5.0 );
    }
    for( a = 0; a < 5; ++a ) {
        for( b = a + 1; b < 5; ++b ) {
            double det = ex[a] * ey[b] - ey[a] * ex[b];

            for( ka = -n; ka <= n; ++ka ) {
                for( kb = -n; kb <= n; ++kb ) {
                    /* the point where grid line a = ka meets grid line b = kb */
                    double      ra = ka - g[a];
                    double      rb = kb - g[b];
                    double      zx = ( ra * ey[b] - rb * ey[a] ) / det;
                    double      zy = ( rb * ex[a] - ra * ex[b] ) / det;
                    signed char k[5];
                    signed char c[5];
                    int         v[4];
                    double      cx = 0.0;
                    double      cy = 0.0;
                    int         m;

                    for( j = 0; j < 5; ++j ) {
                        k[j] = (signed char)ceil( zx * ex[j] + zy * ey[j] + g[j] );
                    }
                    for( m = 0; m < 4; ++m ) {
                        memcpy( c, k, 5 );
                        c[a] = (signed char)( ka + ( m == 1 || m == 2 ) );
                        c[b] = (signed char)( kb + ( m >= 2 ) );
                        for( j = 0; j < 5; ++j ) {
                            cx += c[j] * ex[j] / 4.0;
                            cy += c[j] * ey[j] / 4.0;
                        }
                    }
                    if( cx * cx + cy * cy > r * r || npt == PEN_T ) {
                        continue;
                    }
                    for( m = 0; m < 4; ++m ) {
                        memcpy( c, k, 5 );
                        c[a] = (signed char)( ka + ( m == 1 || m == 2 ) );
                        c[b] = (signed char)( kb + ( m >= 2 ) );
                        v[m] = pen_vertex( c, sc );
                        if( v[m] < 0 ) {
                            return;
                        }
                    }
                    for( m = 0; m < 4; ++m ) {
                        pt[npt][m] = (short)v[m];
                        pen_edge( v[m], v[( m + 1 ) % 4], npt );
                    }
                    ++npt;
                }
            }
        }
    }
}

/* DSatur: the most constrained tile first, the lowest colour its neighbours leave. Returns the
 * number of colours used. */
static int pen_seen( int t )
{
    int m;
    int seen = 0;

    for( m = 0; m < 4; ++m ) {
        if( pn[t][m] >= 0 && pcls[pn[t][m]] != 0xFF ) {
            seen |= 1 << pcls[pn[t][m]];
        }
    }
    return( seen );
}

static int pen_colour( void )
{
    int i;
    int e;
    int used = 0;

    for( i = 0; i < npt; ++i ) {
        pcls[i] = 0xFF;
        pn[i][0] = pn[i][1] = pn[i][2] = pn[i][3] = -1;
    }
    for( e = 0; e < npe; ++e ) {
        int s;

        for( s = 0; s < 2 && pe[e][3] >= 0; ++s ) {
            int t = pe[e][2 + s];
            int m;

            for( m = 0; m < 4 && pn[t][m] >= 0; ++m ) {
            }
            if( m < 4 ) {
                pn[t][m] = pe[e][3 - s];
            }
        }
    }
    for( ;; ) {
        int best = -1;
        int bsat = -1;
        int c;

        for( i = 0; i < npt; ++i ) {
            int seen;
            int sat = 0;

            if( pcls[i] != 0xFF ) {
                continue;
            }
            seen = pen_seen( i );
            for( c = 0; c < 8; ++c ) {
                sat += ( seen >> c ) & 1;
            }
            if( sat > bsat ) {
                bsat = sat;
                best = i;
            }
        }
        if( best < 0 ) {
            return( used );
        }
        for( c = 0; ( pen_seen( best ) >> c ) & 1; ++c ) {
        }
        pcls[best] = (unsigned char)c;
        if( c + 1 > used ) {
            used = c + 1;
        }
    }
}

static short    *pord;
static long     pen_steps;

static int pen_try( int i )
{
    int t;
    int c;
    int seen;

    if( i == npt ) {
        return( 1 );
    }
    if( ++pen_steps > 200000L ) {
        return( 0 );
    }
    t = pord[i];
    seen = pen_seen( t );
    for( c = 0; c < 3; ++c ) {
        if( !( ( seen >> c ) & 1 ) ) {
            pcls[t] = (unsigned char)c;
            if( pen_try( i + 1 ) ) {
                return( 1 );
            }
        }
    }
    pcls[t] = 0xFF;
    return( 0 );
}

/* Penrose rhombs need only three colours (Sibley and Wagon, 2000); a greedy colouring can use
 * a fourth, which costs a plane and a fill over the whole box. */
static int pen_colour3( void )
{
    int n = 0;
    int h = 0;
    int i;
    int m;

    for( i = 0; i < npt; ++i ) {
        pcls[i] = 0xFF;
    }
    for( i = 0; i < npt; ++i ) {
        pcls[i] = 0xFE;                 /* marks "queued" for the walk */
        if( i != 0 ) {
            pcls[i] = 0xFF;
        }
    }
    pord[n++] = 0;
    while( h < n ) {
        int t = pord[h++];

        for( m = 0; m < 4; ++m ) {
            int o = pn[t][m];

            if( o >= 0 && pcls[o] == 0xFF ) {
                pcls[o] = 0xFE;
                pord[n++] = (short)o;
            }
        }
    }
    for( i = 0; i < npt; ++i ) {
        if( pcls[i] == 0xFF ) {
            pord[n++] = (short)i;       /* tiles cut off from the rest */
        }
        pcls[i] = 0xFF;
    }
    pen_steps = 0;
    return( pen_try( 0 ) ? 3 : 0 );
}

static void pen_polys( const int *col, int skip )
{
    int     i;
    int     m;
    M8PT    q[4];

    for( i = 0; i < npt; ++i ) {
        if( pcls[i] == skip ) {
            continue;
        }
        for( m = 0; m < 4; ++m ) {
            q[m].x = pv[pt[i][m]].x;
            q[m].y = pv[pt[i][m]].y;
        }
        m8_poly( 4, q, col[pcls[i]] );
    }
}

static M8SEG    *pseg;

static int      npseg;

static void pen_segs( int skip, int order )
{
    int e;
    int n = 0;

    for( e = 0; e < npe; ++e ) {
        int m = 1 << pcls[pe[e][2]];

        if( pe[e][3] >= 0 ) {
            m |= 1 << pcls[pe[e][3]];
        }
        if( skip >= 0 ) {
            m &= ~( 1 << skip );
        }
        if( !m ) {
            continue;
        }
        pseg[n].x0 = pv[pe[e][0]].x;
        pseg[n].y0 = pv[pe[e][0]].y;
        pseg[n].x1 = pv[pe[e][1]].x;
        pseg[n].y1 = pv[pe[e][1]].y;
        pseg[n].planes = (unsigned char)m;
        ++n;
    }
    npseg = n;
    if( order ) {
        m8_parity_order( n, pseg );
    }
}

static int penprobe( void )
{
    static const double rs[] = { 6.0, 9.0 };
    static const double scs[] = { 26.0, 18.0 };     /* patch under 382 px wide */
    static const int    col[4] = { RED, YELLOW, BLUE, GREEN };
    char                line[200];
    char                name[16];
    int                 sz;
    int                 skip;
    int                 k;
    int                 ncol;
    long                f;
    M8STATS             s;

    pv = (PENV *)malloc( sizeof( PENV ) * PEN_V );
    pt = (short (*)[4])malloc( sizeof( short ) * 4 * PEN_T );
    pcls = (unsigned char *)malloc( PEN_T );
    pn = (short (*)[4])malloc( sizeof( short ) * 4 * PEN_T );
    pe = (short (*)[4])malloc( sizeof( short ) * 4 * PEN_E );
    pseg = (M8SEG *)malloc( sizeof( M8SEG ) * PEN_E );
    pord = (short *)malloc( sizeof( short ) * PEN_T );
    page0 = (unsigned char *)malloc( (size_t)M8_W * M8_H );
    pagex = (unsigned char *)malloc( (size_t)M8_W * M8_H );
    if( !pv || !pt || !pcls || !pn || !pe || !pseg || !pord || !page0 || !pagex ) {
        return( 1 );
    }
    for( sz = 0; sz < 2; ++sz ) {
        pen_build( rs[sz], scs[sz] );
        ncol = pen_colour();            /* also fills the neighbour lists */
        if( ncol > 3 && pen_colour3() == 3 ) {
            ncol = 3;
        } else if( ncol > 3 ) {
            ncol = pen_colour();
        }
        for( skip = -1; skip <= 0; ++skip ) {
            sprintf( line, "M8DTEST penprobe: %d rhombs, %d corners, %d edges, %d colours, %s\n", npt, npv,
                     npe, ncol, skip < 0 ? "all drawn" : "colour 0 = background" );
            probe_line( line );
            for( k = 0; k < 3; ++k ) {
                unsigned long   ticks;
                unsigned long   w;
                int             c0[4];
                int             i;

                if( k ) {
                    pen_segs( skip, k == 2 );
                }

                for( i = 0; i < 4; ++i ) {
                    c0[i] = ( i == skip ) ? 0 : col[i];
                }
                if( m8_open() ) {
                    return( 1 );
                }
                palette();
                m8_stats_reset();
                m8_frame_begin();
                m8_clear( 0 );
                if( k ) m8_parity( npseg, pseg, ncol, c0 ); else pen_polys( c0, skip );
                m8_frame_end( 1 );
                m8_stats( &s );
                w = s.outs;
                if( m8_read_shown( k ? pagex : page0 ) ) {
                    m8_close();
                    return( 1 );
                }
                m8_stats_reset();
                for( f = 0; f < 50; ++f ) {
                    m8_frame_begin();
                    m8_clear( 0 );
                    if( k ) m8_parity( npseg, pseg, ncol, c0 ); else pen_polys( c0, skip );
                    m8_frame_end( 0 );
                }
                m8_stats( &s );
                ticks = s.ticks ? s.ticks : 1;
                m8_close();
                sprintf( line, " %s: %lu writes a frame, %.2f ms a frame (50 nv)\n",
                         k == 2 ? "parity, chained" : k ? "parity, edge order" : "rhombs",
                         w, ticks * 1000.0 / 18.2065 / s.frames );
                probe_line( line );
                sprintf( name, "PEN%c%d%d.RAW", "QPC"[k], sz, skip + 1 );
                chk_save( name, k ? pagex : page0 );
                if( k ) {
                    probe_diff( k == 2 ? "chained vs rhombs" : "edge order vs rhombs", 0, 0, page0, pagex );
                }
            }
        }
    }
    return( 0 );
}

/* L6 P6: ten bands, levels 0-9 of YELLOW over BLUE through the off-screen threshold map; every
 * pixel checked against the map the CPU computes; then 50 frames of the bands, against plain
 * fills of the same areas. Page to DITH.RAW. */
static void dith_bands( int plain )
{
    int k;

    for( k = 0; k < 10; ++k ) {
        if( plain ) {
            m8_rect( 0, 20 + k * 44, M8_W - 1, 59 + k * 44, k < 5 ? BLUE : YELLOW );
        } else {
            m8_dither_rect( 0, 20 + k * 44, M8_W - 1, 59 + k * 44, BLUE, YELLOW, k );
        }
    }
}

static int ditherprobe( void )
{
    char            line[200];
    unsigned long   t0;
    unsigned long   bad = 0;
    unsigned long   lit = 0;
    unsigned long   w;
    int             x;
    int             y;
    int             k;
    M8STATS         s;

    page0 = (unsigned char *)malloc( (size_t)M8_W * M8_H );
    if( page0 == NULL ) {
        return( 1 );
    }
    if( m8_open() ) {
        return( 1 );
    }
    palette();
    t0 = m8_ticks();
    if( m8_dither_init() ) {
        m8_close();
        return( 1 );
    }
    sprintf( line, "M8DTEST ditherprobe: map 384 x 512 uploaded in %.2f s\n", ( m8_ticks() - t0 ) / 18.2065 );
    probe_line( line );
    m8_stats_reset();
    m8_frame_begin();
    m8_clear( 0 );
    dith_bands( 0 );
    m8_frame_end( 1 );
    m8_stats( &s );
    w = s.outs;
    if( m8_read_shown( page0 ) ) {
        m8_close();
        return( 1 );
    }
    for( k = 0; k < 10; ++k ) {
        unsigned long kb = 0;
        unsigned long ky = 0;

        for( y = 20 + k * 44; y <= 59 + k * 44; ++y ) {
            for( x = 0; x < M8_W; ++x ) {
                int want = k == 0 ? BLUE : k == 9 ? YELLOW :
                           ( m8_dither_map( x, y ) >> ( k - 1 ) & 1 ) ? YELLOW : BLUE;
                int got = page0[y * M8_W + x];

                bad += got != want;
                ky += got == YELLOW;
                lit += got != 0;
            }
            kb += M8_W;
        }
        sprintf( line, "  level %d: %5.1f %% yellow (want %5.1f)\n", k, 100.0 * ky / kb, 100.0 * k / 9 );
        probe_line( line );
    }
    sprintf( line, "  %lu pixels drawn, %lu differ from the map, %lu writes a frame\n", lit, bad, w );
    probe_line( line );
    chk_save( "DITH.RAW", page0 );
    for( k = 0; k < 2; ++k ) {
        long            f;
        unsigned long   ticks;

        m8_stats_reset();
        for( f = 0; f < 50; ++f ) {
            m8_frame_begin();
            m8_clear( 0 );
            dith_bands( k );
            m8_frame_end( 0 );
        }
        m8_stats( &s );
        ticks = s.ticks ? s.ticks : 1;
        sprintf( line, " %s: %.1f writes a frame, %.2f ms a frame (50 nv)\n", k ? "plain fills" : "dithered   ",
                 (double)s.outs / s.frames, ticks * 1000.0 / 18.2065 / s.frames );
        probe_line( line );
    }
    m8_close();
    return( 0 );
}

int main( int argc, char **argv )
{
    const char  *mode = argc > 1 ? argv[1] : "card";
    char        what[80];

    if( stricmp( mode, "ditherprobe" ) == 0 ) {
        return( ditherprobe() );
    }
    if( stricmp( mode, "penprobe" ) == 0 ) {
        return( penprobe() );
    }
    if( stricmp( mode, "checkprobe" ) == 0 ) {
        return( checkprobe() );
    }
    if( stricmp( mode, "parprobe" ) == 0 ) {
        return( parprobe( argc > 2 ? atoi( argv[2] ) : 0 ) );
    }
    if( stricmp( mode, "joinprobe" ) == 0 ) {
        return( joinprobe( argc > 2 ? atoi( argv[2] ) : 2 ) );
    }
    if( stricmp( mode, "tileprobe" ) == 0 ) {
        return( tileprobe() );
    }
    if( stricmp( mode, "edgeprobe" ) == 0 ) {
        return( edgeprobe() );
    }
    if( stricmp( mode, "costs" ) == 0 ) {
        return( costs() );
    }
    if( stricmp( mode, "zprobe" ) == 0 ) {
        return( zprobe() );
    }
    if( stricmp( mode, "probe" ) == 0 ) {
        return( probe() );
    }
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
