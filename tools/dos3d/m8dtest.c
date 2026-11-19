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

int main( int argc, char **argv )
{
    const char  *mode = argc > 1 ? argv[1] : "card";
    char        what[80];

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
