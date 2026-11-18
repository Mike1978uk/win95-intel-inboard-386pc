/* M8CUBE32.EXE - flat-shaded cubes turning at 640x480x8 on the Mach8's 8514 side, from a
 * plain DOS prompt under DOS/32A (docs/playable_3d_plan.md P3.2, P4.1).
 *
 * The CPU transforms, culls, shades and builds the whole frame as a list of port writes;
 * the engine draws every face. Two pages, pitch 1024: page 0 at row 0, page 1 at row 512,
 * shown by CRT_OFFSET. The scratch for polygon outlines is the off-screen column x 640-1023.
 *
 *   M8CUBE32 [frames] [objects] [nv]
 *     frames    frames to draw, then report (default 600; any key stops sooner)
 *     objects   cubes, 1..8 (default 1)
 *     nv        do not wait for the page flip to take effect (throughput, tears)
 * The report goes to the screen after text mode is back, and is appended to M8CUBE32.TXT.
 *
 * Build: see BUILD.BAT. GPL-3.0-or-later, as drivers/mach8_display.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <i86.h>

typedef unsigned char   BYTE;
typedef unsigned short  WORD;
typedef unsigned long   DWORD;

/* Registers, as drivers/mach8_display/src/m8.h and m8d3d.c. */
#define M8_SUBSYS           0x42E8      /* read: bit 0 vertical blank latch; write 1 acknowledges it */
#define M8_ADVFUNC_CNTL     0x4AE8
#define M8_CRT_OFFSET_LO    0x2AEE
#define M8_CRT_OFFSET_HI    0x2EEE
#define M8_GE_STAT          0x9AE8      /* read: bit 9 engine busy */
#define M8_EXT_FIFO_STATUS  0x9AEE      /* read: one bit per occupied FIFO entry */
#define M8_WRT_MASK         0xAAE8
#define M8_RD_MASK          0xAEE8
#define M8_FRGD_COLOR       0xA6E8
#define M8_FRGD_MIX         0xBAE8
#define M8_MULTIFUNC_CNTL   0xBEE8
#define M8_MIN_AXIS         0xBEE8
#define M8_CUR_X            0x86E8
#define M8_CUR_Y            0x82E8
#define M8_MAJ_AXIS_PCNT    0x96E8
#define M8_CMD              0x9AE8
#define M8_CMD_FILL         0x40F3
#define M8_SCAN_TO_X        0xCAEE      /* draws CUR_X up to (not including) it on CUR_Y */
#define M8_DESTX_DIASTP     0x8EE8
#define M8_DESTY_AXSTP      0x8AE8
#define M8_DP_CONFIG        0xCEEE
#define M8_ALU_FG_FN        0xBAEE
#define M8_LINEDRAW         0xFEEE
#define M8_LINEDRAW_INDEX   0x9AEE
#define M8_LINEDRAW_OPT     0xA2EE
#define M8_LINEDRAW_LAST_OFF 0x0004
#define M8_LINEDRAW_POLY    0x0002
#define M8_SRC_X_START      0xB2EE
#define M8_SRC_X_END        0xBEEE
#define M8_SRC_Y_DIR        0xC2EE
#define M8_DEST_X_START     0xA6EE
#define M8_DEST_X_END       0xAAEE
#define M8_DEST_Y_END       0xAEEE
#define M8_DP_8514          0x2211
#define M8_DP_POLY_FILL     0x2213      /* FRGD_COLOR between the blit source's RD_MASK-plane pixels */
#define M8_MIX_FRGD_REPLACE 0x0027
#define M8_MIX_XOR          0x0005
#define M8_MIX_REPLACE      0x0007
#define M8_DAC_MASK         0x02EA
#define M8_DAC_R_INDEX      0x02EB
#define M8_DAC_W_INDEX      0x02EC
#define M8_DAC_DATA         0x02ED

#define WAIT_LIMIT      60000u          /* status reads before the engine counts as wedged */
#define PAGE1_Y         512
#define PAGE1_HI        2               /* 512 rows x 1024 bytes / 4 = 0x20000 */
#define SCR_X           640             /* scratch column: x 640-1023, never shown at 640 wide */
#define SCR_W           384
#define SCR_H           1024
#define W               640
#define H               480
#define MAXV            4
#define MAX_OBJ         8
#define PASS            8               /* polygons a pass: one scratch bit-plane each */
#define Q_MAX           65536
#define Q_CHUNK         8

#define BIOS_TICKS      (*(volatile DWORD *)0x46C)
#define KB_HEAD         (*(volatile WORD *)0x41A)
#define KB_TAIL         (*(volatile WORD *)0x41C)

/* Port access. Every OUT is ~7.6 us on this bus, so each one is counted. */
static DWORD    n_out;
static DWORD    n_in;

extern void io_outw( unsigned port, unsigned v );
#pragma aux io_outw = "out dx, ax" parm [edx] [eax] modify exact [];
extern void io_outb( unsigned port, unsigned v );
#pragma aux io_outb = "out dx, al" parm [edx] [eax] modify exact [];
extern unsigned io_inw( unsigned port );
#pragma aux io_inw = "xor eax, eax" "in ax, dx" parm [edx] value [eax] modify exact [eax];
extern unsigned io_inb( unsigned port );
#pragma aux io_inb = "xor eax, eax" "in al, dx" parm [edx] value [eax] modify exact [eax];

/* As m8d3d_q_send: each entry is port | value << 16. */
extern void q_send_run( const DWORD *p, int n );
#pragma aux q_send_run = \
    "q_next:"               \
    "mov eax, [esi]"        \
    "mov edx, eax"          \
    "shr eax, 16"           \
    "out dx, ax"            \
    "add esi, 4"            \
    "dec ecx"               \
    "jnz q_next"            \
    parm [esi] [ecx] modify [eax edx esi ecx];

/* About 84 us at 83.5 MHz, registers only (m8dd_spin): a loop that stores would hold the bus. */
extern void spin( void );
#pragma aux spin = "mov ecx, 1000" 0xE2 0xFE modify [ecx];

static void outw( unsigned port, unsigned v )
{
    io_outw( port, v );
    ++n_out;
}

static void outb( unsigned port, unsigned v )
{
    io_outb( port, v );
    ++n_out;
}

static unsigned inw( unsigned port )
{
    ++n_in;
    return( io_inw( port ) );
}

static unsigned inb( unsigned port )
{
    ++n_in;
    return( io_inb( port ) );
}

/* ---- state that the exit path must undo ---- */
static int          gfx_on;
static int          pal_saved;
static BYTE         pal_old[768];
static BYTE         dac_mask_old;
static int          engine_bad;
static const char   *fail_msg;

static void restore( void )
{
    union REGS  r;
    int         i;

    if( !gfx_on ) {
        return;
    }
    gfx_on = 0;
    if( engine_bad ) {
        io_outw( M8_SUBSYS, 0x900F );   /* reset, flags acknowledged; a locked FIFO ignores all else */
        io_outw( M8_SUBSYS, 0x500F );
    }
    io_outw( M8_CRT_OFFSET_LO, 0 );
    io_outw( M8_CRT_OFFSET_HI, 0 );
    if( pal_saved ) {
        io_outb( M8_DAC_W_INDEX, 0 );
        for( i = 0; i < 768; ++i ) {
            io_outb( M8_DAC_DATA, pal_old[i] );
        }
        io_outb( M8_DAC_MASK, dac_mask_old );
    }
    io_outw( M8_ADVFUNC_CNTL, 2 );      /* VGA */
    memset( &r, 0, sizeof( r ) );
    r.w.ax = 0x0003;
    int386( 0x10, &r, &r );
}

static void on_signal( int sig )
{
    restore();
    printf( "M8CUBE32: stopped by signal %d, text mode restored\n", sig );
    exit( 3 );
}

static void fail( const char *msg )
{
    engine_bad = 1;
    fail_msg = msg;
    restore();
    printf( "M8CUBE32: %s\n", msg );
    exit( 2 );
}

/* ---- FIFO room, as m8d3d_room: room for n (1..15) while bit 15 - n is clear ---- */
static int      fifo_free;

static void room( int n )
{
    unsigned    i;
    unsigned    s;

    if( n <= fifo_free ) {
        fifo_free -= n;
        return;
    }
    for( i = 0; i < WAIT_LIMIT; ++i ) {
        s = inw( M8_EXT_FIFO_STATUS );
        if( !(s & (0x8000u >> n)) ) {
            int f = n;

            while( f < 15 && !(s & (0x8000u >> (f + 1))) ) {
                ++f;
            }
            fifo_free = f - n;
            return;
        }
    }
    fail( "engine FIFO did not drain (wedged?)" );
}

static void engine_idle( void )
{
    unsigned i;

    for( i = 0; i < WAIT_LIMIT; ++i ) {
        if( inw( M8_EXT_FIFO_STATUS ) == 0 && !(inw( M8_GE_STAT ) & 0x0200) ) {
            fifo_free = 15;
            return;
        }
    }
    fail( "engine did not go idle (wedged?)" );
}

/* ---- the frame's writes, built whole before any is sent: every OUT and status read
 * empties the L1, so sending between passes would leave the next pass running from RAM ---- */
static DWORD    q[Q_MAX];
static int      qn;

#define Q( port, value ) ( q[qn++] = (DWORD)(port) | (DWORD)(WORD)(value) << 16 )

static void q_send( void )
{
    int i = 0;

    while( i < qn ) {
        int n = qn - i < Q_CHUNK ? qn - i : Q_CHUNK;

        room( n );
        q_send_run( q + i, n );
        n_out += n;
        i += n;
    }
    qn = 0;
}

/* Write counts by kind, for the report. */
static DWORD    w_clear, w_pass, w_fill, w_span, w_flip;
static DWORD    n_fill, n_span;

/* A plain fill of l, t .. r - 1, b - 1 in the colour and masks already set. */
static void q_rect( int l, int t, int r, int b )
{
    Q( M8_CUR_X, l );
    Q( M8_CUR_Y, t );
    Q( M8_MAJ_AXIS_PCNT, r - l - 1 );
    Q( M8_MIN_AXIS, b - t - 1 );
    Q( M8_CMD, M8_CMD_FILL );
}

/* Black over a card rectangle, leaving the plain path set up (m8d3d_q_clear). */
static void q_clear( int l, int t, int r, int b )
{
    if( r <= l || b <= t ) {
        return;
    }
    Q( M8_WRT_MASK, 0x00FF );
    Q( M8_RD_MASK, 0x00FF );
    Q( M8_DP_CONFIG, M8_DP_8514 );
    Q( M8_FRGD_MIX, M8_MIX_FRGD_REPLACE );
    Q( M8_FRGD_COLOR, 0 );
    q_rect( l, t, r, b );
}

/* ---- polygons ---- */
typedef struct {
    int     n;
    short   x[MAXV];
    short   y[MAXV];
    BYTE    col;
    short   l, t, r, b;             /* box, inclusive */
    int     fill;                   /* by polygon fill (else spans) */
} POLY;

/* A convex polygon row by row (m8d3d_spans, for n corners): 3 writes a row. */
static short    span_lo[H + 1];
static short    span_hi[H + 1];

static void q_spans( const POLY *p, int dy )
{
    int i;
    int y;
    int t = p->t;
    int rows = p->b - p->t;

    for( y = 0; y <= rows; ++y ) {
        span_lo[y] = 0x7FFF;
        span_hi[y] = -0x7FFF;
    }
    for( i = 0; i < p->n; ++i ) {
        int a = i;
        int c = (i + 1) % p->n;
        long x;
        long step;

        if( p->y[a] > p->y[c] ) {
            int s = a;

            a = c;
            c = s;
        }
        step = p->y[c] == p->y[a] ? 0 : ((long)(p->x[c] - p->x[a]) << 16) / (p->y[c] - p->y[a]);
        x = ((long)p->x[a] << 16) + 0x8000;
        for( y = p->y[a] - t; y <= p->y[c] - t; ++y ) {
            int px = (int)(x >> 16);

            if( px < span_lo[y] ) {
                span_lo[y] = (short)px;
            }
            if( px > span_hi[y] ) {
                span_hi[y] = (short)px;
            }
            x += step;
        }
        if( p->y[c] == p->y[a] ) {
            y = p->y[a] - t;
            if( p->x[c] < span_lo[y] ) {
                span_lo[y] = p->x[c];
            }
            if( p->x[c] > span_hi[y] ) {
                span_hi[y] = p->x[c];
            }
        }
    }
    for( y = 0; y <= rows; ++y ) {
        if( span_hi[y] < span_lo[y] ) {
            continue;
        }
        Q( M8_CUR_Y, dy + t + y );
        Q( M8_CUR_X, span_lo[y] );
        Q( M8_SCAN_TO_X, span_hi[y] + 1 );
    }
}

/* Up to PASS polygons of one object, which do not overlap each other. Phases as m8d3d_join:
 * every outline into its own scratch plane, then every fill, then every on-screen outline,
 * then one clear of the planes used, so the shared registers are written once a phase.
 * Outlines go into the scratch with XOR, each edge top to bottom without its last row, so
 * every row strictly between top and bottom holds exactly two pixels (m8d3d_edges). The
 * fill covers those rows only and leaves its right boundary out; the outline drawn over it
 * in the face's colour supplies the edge pixels, the top and the bottom rows (m8d3d_tri).
 * Each polygon's box sits at the scratch's corner: the planes keep them apart. */
static void q_pass( POLY *p, int np, int dy )
{
    int     j;
    int     i;
    int     cw = 0;
    int     ch = 0;
    int     planes = 0;
    DWORD   q0;

    for( j = 0; j < np; ++j ) {
        if( p[j].fill ) {
            planes |= 1 << j;
        }
    }
    if( planes ) {
        q0 = qn;
        Q( M8_FRGD_COLOR, 0x00FF );
        Q( M8_DP_CONFIG, M8_DP_8514 );
        Q( M8_ALU_FG_FN, M8_MIX_XOR );
        Q( M8_LINEDRAW_OPT, M8_LINEDRAW_POLY | M8_LINEDRAW_LAST_OFF );
        Q( M8_RD_MASK, 0x00FF );
        w_pass += qn - q0;
        for( j = 0; j < np; ++j ) {
            int sx = SCR_X - p[j].l;
            int sy = -p[j].t;

            if( !p[j].fill ) {
                continue;
            }
            q0 = qn;
            Q( M8_WRT_MASK, 1 << j );
            for( i = 0; i < p[j].n; ++i ) {
                int a = i;
                int c = (i + 1) % p[j].n;

                if( p[j].y[a] == p[j].y[c] ) {
                    continue;
                }
                if( p[j].y[a] > p[j].y[c] ) {
                    int s = a;

                    a = c;
                    c = s;
                }
                Q( M8_LINEDRAW_INDEX, 0 );
                Q( M8_LINEDRAW, sx + p[j].x[a] );
                Q( M8_LINEDRAW, sy + p[j].y[a] );
                Q( M8_LINEDRAW, sx + p[j].x[c] );
                Q( M8_LINEDRAW, sy + p[j].y[c] );
            }
            w_fill += qn - q0;
            if( p[j].r - p[j].l + 2 > cw ) {
                cw = p[j].r - p[j].l + 2;
            }
            if( p[j].b - p[j].t + 1 > ch ) {
                ch = p[j].b - p[j].t + 1;
            }
        }
        q0 = qn;
        Q( M8_WRT_MASK, 0x00FF );
        Q( M8_DP_CONFIG, M8_DP_POLY_FILL );
        Q( M8_ALU_FG_FN, M8_MIX_REPLACE );
        Q( M8_SRC_Y_DIR, 1 );
        w_pass += qn - q0;
        for( j = 0; j < np; ++j ) {
            int w = p[j].r - p[j].l + 1;

            if( !p[j].fill ) {
                continue;
            }
            q0 = qn;
            Q( M8_RD_MASK, 1 << j );
            Q( M8_FRGD_COLOR, p[j].col );
            Q( M8_DESTX_DIASTP, SCR_X );
            Q( M8_DESTY_AXSTP, 1 );
            Q( M8_SRC_X_START, SCR_X );
            Q( M8_SRC_X_END, SCR_X + w );
            Q( M8_CUR_X, p[j].l );
            Q( M8_DEST_X_START, p[j].l );
            Q( M8_DEST_X_END, p[j].r + 1 );
            Q( M8_CUR_Y, dy + p[j].t + 1 );
            Q( M8_DEST_Y_END, dy + p[j].b );       /* starts the fill */
            w_fill += qn - q0;
        }
    }
    q0 = qn;
    Q( M8_RD_MASK, 0x00FF );
    Q( M8_WRT_MASK, 0x00FF );
    Q( M8_DP_CONFIG, M8_DP_8514 );
    Q( M8_ALU_FG_FN, M8_MIX_REPLACE );
    Q( M8_LINEDRAW_OPT, M8_LINEDRAW_LAST_OFF );
    w_pass += qn - q0;
    for( j = 0; j < np; ++j ) {
        q0 = qn;
        Q( M8_FRGD_COLOR, p[j].col );
        if( p[j].fill ) {
            Q( M8_LINEDRAW_INDEX, 0 );
            for( i = 0; i <= p[j].n; ++i ) {               /* the last closes the figure */
                Q( M8_LINEDRAW, p[j].x[i % p[j].n] );
                Q( M8_LINEDRAW, dy + p[j].y[i % p[j].n] );
            }
            w_fill += qn - q0;
            ++n_fill;
        } else {
            q_spans( &p[j], dy );
            w_span += qn - q0;
            ++n_span;
        }
    }
    if( planes ) {
        /* The FIFO runs in order, so the fills have read the scratch before this clears it. */
        q0 = qn;
        Q( M8_WRT_MASK, planes );
        Q( M8_RD_MASK, 0x00FF );
        Q( M8_DP_CONFIG, M8_DP_8514 );
        Q( M8_FRGD_MIX, M8_MIX_FRGD_REPLACE );
        Q( M8_FRGD_COLOR, 0 );
        q_rect( SCR_X, 0, SCR_X + cw, ch );
        Q( M8_WRT_MASK, 0x00FF );
        w_pass += qn - q0;
    }
}

/* Writes each way would take, so the cheaper is chosen per polygon. Polygon fill: the
 * scratch outline (5 an edge + 1), the fill (11), the outline on screen (2 a corner + 4),
 * and a share of the pass set-up and scratch clear (~6). Spans: 3 a row + 1. */
static void choose( POLY *p )
{
    int rows = p->b - p->t + 1;
    int edges = 0;
    int i;
    int fill_cost;
    int span_cost = 1 + 3 * rows;

    for( i = 0; i < p->n; ++i ) {
        edges += p->y[i] != p->y[(i + 1) % p->n];
    }
    fill_cost = 1 + 5 * edges + 11 + 4 + 2 * p->n + 6;
    p->fill = rows >= 3 && p->r - p->l + 2 <= SCR_W && rows <= SCR_H && fill_cost < span_cost;
}

/* ---- scene: the Windows M8CUBE.C fixed-point build at 640x480 ---- */
#define FX_STEPS    1024
#define HUES        7
#define SHADES      32
#define PAL_BASE    32                  /* hue h, shade s at PAL_BASE + h * SHADES + s */

static long         fx_sin[FX_STEPS];
#define FX_S( a )   fx_sin[(a) & (FX_STEPS - 1)]
#define FX_C( a )   fx_sin[((a) + FX_STEPS / 4) & (FX_STEPS - 1)]

static const int    cube_v[8][3] = {
    { -1, -1, -1 }, { 1, -1, -1 }, { 1, 1, -1 }, { -1, 1, -1 },
    { -1, -1, 1 }, { 1, -1, 1 }, { 1, 1, 1 }, { -1, 1, 1 }
};
/* Corners in one winding seen from outside, and the outward normal: a face whose screen
 * area is negative faces the viewer (y grows downwards). */
static const int    cube_f[6][4] = {
    { 0, 3, 2, 1 }, { 4, 5, 6, 7 }, { 0, 1, 5, 4 }, { 2, 3, 7, 6 }, { 1, 2, 6, 5 }, { 0, 4, 7, 3 }
};
static const int    cube_n[6][3] = {
    { 0, 0, -1 }, { 0, 0, 1 }, { 0, -1, 0 }, { 0, 1, 0 }, { 1, 0, 0 }, { -1, 0, 0 }
};

static int          objects = 1;

static void fx_init( void )
{
    int i;

    for( i = 0; i < FX_STEPS; ++i ) {
        fx_sin[i] = (long)(16384.0 * sin( i * 6.283185307 / FX_STEPS ) + (i < FX_STEPS / 2 ? 0.5 : -0.5));
    }
}

static void pal_load( void )
{
    static const int hue[HUES][3] = {
        { 255, 140, 40 }, { 60, 160, 255 }, { 120, 255, 90 }, { 255, 80, 80 },
        { 240, 240, 90 }, { 200, 110, 255 }, { 220, 220, 220 }
    };
    int i;
    int k;

    outb( M8_DAC_W_INDEX, 0 );
    for( i = 0; i < PAL_BASE; ++i ) {               /* black, then a grey ramp */
        outb( M8_DAC_DATA, i * 2 );
        outb( M8_DAC_DATA, i * 2 );
        outb( M8_DAC_DATA, i * 2 );
    }
    for( i = 0; i < HUES; ++i ) {
        for( k = 0; k < SHADES; ++k ) {
            int s = 24 + k * (256 - 24) / SHADES;

            outb( M8_DAC_DATA, hue[i][0] * s / 256 >> 2 );
            outb( M8_DAC_DATA, hue[i][1] * s / 256 >> 2 );
            outb( M8_DAC_DATA, hue[i][2] * s / 256 >> 2 );
        }
    }
}

typedef struct {
    long    cz;                         /* depth of the centre, for the painter's order */
    int     np;
    POLY    p[6];
} OBJ;

static OBJ          obj[MAX_OBJ];
static OBJ          *order[MAX_OBJ];

static int cmp_obj( const void *a, const void *b )
{
    long d = (*(OBJ * const *)b)->cz - (*(OBJ * const *)a)->cz;

    return( d > 0 ? 1 : d < 0 ? -1 : 0 );
}

/* Frame f's objects: angles in 1024 steps, sines and normals Q14, coordinates Q8. */
static void build( long f )
{
    static const long   light[3] = { -6554, -9011, -11960 };    /* -0.40 -0.55 -0.73 in Q14 */
    long                ft = f * 8;     /* animation by frame: the same frames on every run */
    long                focal = objects == 1 ? 340 : 300;
    long                size = objects <= 4 ? 256 : 180;        /* half an edge, Q8 */
    int                 o;
    int                 i;
    int                 k;

    for( o = 0; o < objects; ++o ) {
        OBJ     *ob = &obj[o];
        long    a = (ft * (90 + 13 * o)) / 100 + o * 163;
        long    b = (ft * (60 + 7 * o)) / 100 + o * 326;
        long    ca = FX_C( a ), sa = FX_S( a ), cb = FX_C( b ), sb = FX_S( b );
        long    ph = (long)FX_STEPS * o / objects + (ft * 3) / 10;
        long    cx = objects == 1 ? 0 : (819L * FX_C( ph )) >> 14;          /* 3.2 in Q8 */
        long    cy = objects == 1 ? 0 : (256L * FX_S( ph )) >> 14;          /* 1.0 */
        long    cz = (objects == 1 ? 1280 : 2304) + (objects == 1 ? 0 : (819L * FX_S( ph )) >> 14);
        long    sx[8], sy[8];

        ob->cz = cz;
        ob->np = 0;
        for( i = 0; i < 8; ++i ) {
            long v0 = cube_v[i][0] * size, v1 = cube_v[i][1] * size, v2 = cube_v[i][2] * size;
            long x = (v0 * ca + v2 * sa) >> 14;
            long z = (-v0 * sa + v2 * ca) >> 14;
            long y = (v1 * cb - z * sb) >> 14;

            z = (v1 * sb + z * cb) >> 14;
            x += cx;
            y += cy;
            z += cz;
            sx[i] = W / 2 + focal * x / z;
            sy[i] = H / 2 + focal * y / z;
            sx[i] = sx[i] < 0 ? 0 : sx[i] > W - 1 ? W - 1 : sx[i];
            sy[i] = sy[i] < 0 ? 0 : sy[i] > H - 1 ? H - 1 : sy[i];
        }
        for( k = 0; k < 6; ++k ) {
            const int   *fc = cube_f[k];
            long        n0 = cube_n[k][0] * 16384L, n1 = cube_n[k][1] * 16384L;
            long        n2 = cube_n[k][2] * 16384L;
            long        nx = (n0 * ca + n2 * sa) >> 14;
            long        nz = (-n0 * sa + n2 * ca) >> 14;
            long        ny = (n1 * cb - nz * sb) >> 14;
            long        area = 0;
            long        li;
            int         s;
            POLY        *p;

            nz = (n1 * sb + nz * cb) >> 14;
            for( i = 0; i < 4; ++i ) {
                area += sx[fc[i]] * sy[fc[(i + 1) & 3]] - sx[fc[(i + 1) & 3]] * sy[fc[i]];
            }
            if( area >= 0 ) {
                continue;
            }
            li = -(nx * light[0] + ny * light[1] + nz * light[2]) >> 14;
            s = (int)(((li < 0 ? 0 : li) * (SHADES - 8)) >> 14) + 7;
            p = &ob->p[ob->np++];
            p->n = 4;
            p->col = (BYTE)(PAL_BASE + (o % HUES) * SHADES + (s > SHADES - 1 ? SHADES - 1 : s));
            p->l = p->t = 0x7FFF;
            p->r = p->b = -1;
            for( i = 0; i < 4; ++i ) {
                p->x[i] = (short)sx[fc[i]];
                p->y[i] = (short)sy[fc[i]];
                if( p->x[i] < p->l ) p->l = p->x[i];
                if( p->x[i] > p->r ) p->r = p->x[i];
                if( p->y[i] < p->t ) p->t = p->y[i];
                if( p->y[i] > p->b ) p->b = p->y[i];
            }
            choose( p );
        }
        order[o] = ob;
    }
    qsort( order, objects, sizeof( order[0] ), cmp_obj );
}

/* ---- pages ---- */
static int      shown;                  /* the page CRT_OFFSET was last set to */
static int      page_pending;
static int      page_in_blank;          /* the flip was written inside a blank */
static DWORD    page_tick;
static DWORD    flip_polls, flip_timeouts;
static short    pbox[2][4];             /* what each page holds: l, t, r, b exclusive */

/* Until the page last shown has taken effect (m8dd_page_wait). The latch was acknowledged at
 * the write, so set now means a blank has begun since. A write inside a blank misses that
 * blank, and the latch sets again for as long as a blank lasts, so then the latch must be
 * seen to stay clear before a new blank counts. */
static void page_wait( void )
{
    unsigned n = 480;                   /* ~40 ms */

    if( !page_pending ) {
        return;
    }
    page_pending = 0;
    if( !page_in_blank && (inw( M8_SUBSYS ) & 1) ) {
        return;
    }
    if( BIOS_TICKS - page_tick >= 2 ) {
        return;
    }
    if( page_in_blank ) {
        do {
            outw( M8_SUBSYS, 0x0001 );
            spin();
            ++flip_polls;
        } while( (inw( M8_SUBSYS ) & 1) && --n != 0 );
    }
    while( n != 0 && !(inw( M8_SUBSYS ) & 1) ) {
        spin();
        ++flip_polls;
        --n;
    }
    if( n == 0 ) {
        ++flip_timeouts;
    }
}

/* The show goes after the frame's last write, in the same queue (m8dd_page_frame). */
static void page_show( int p )
{
    DWORD q0 = qn;

    Q( M8_CRT_OFFSET_HI, p ? PAGE1_HI : 0 );
    Q( M8_SUBSYS, 0x0001 );
    w_flip += qn - q0;
    q_send();
    page_in_blank = inw( M8_SUBSYS ) & 1;
    page_tick = BIOS_TICKS;
    page_pending = 1;
    shown = p;
}

/* ---- mode ---- */
static void mode_on( void )
{
    int i;

    /* Engine reset and set-up block of tools/m8seq/M8DOS8.ASM, proven from a cold DOS prompt. */
    outw( M8_SUBSYS, 0x9000 );
    outw( M8_SUBSYS, 0x5000 );
    gfx_on = 1;
    engine_idle();
    outw( M8_MULTIFUNC_CNTL, 0x5006 );
    outw( 0x76EE, 0x0080 );             /* GE pitch 1024 */
    outw( 0x72EE, 0 );
    outw( 0x6EEE, 0 );
    outw( M8_MULTIFUNC_CNTL, 0x1000 );  /* scissors 0..1023 */
    outw( M8_MULTIFUNC_CNTL, 0x2000 );
    outw( M8_MULTIFUNC_CNTL, 0x33FF );
    outw( M8_MULTIFUNC_CNTL, 0x43FF );
    outw( M8_WRT_MASK, 0x00FF );
    outw( M8_MULTIFUNC_CNTL, 0xA000 );
    outw( 0xEEEE, 0 );
    outw( M8_CRT_OFFSET_LO, 0 );
    outw( M8_CRT_OFFSET_HI, 0 );

    dac_mask_old = (BYTE)inb( M8_DAC_MASK );
    outb( M8_DAC_R_INDEX, 0 );
    for( i = 0; i < 768; ++i ) {
        pal_old[i] = (BYTE)inb( M8_DAC_DATA );
    }
    pal_saved = 1;
    outb( M8_DAC_MASK, 0xFF );
    pal_load();

    fifo_free = 0;
    q_clear( 0, 0, 1024, 1024 );        /* both pages and the scratch */
    q_send();
    engine_idle();
    outw( M8_ADVFUNC_CNTL, 3 );         /* 8514 on, shadow set 1: 640x480 */
    shown = 0;
}

/* ---- main ---- */
static int key_waiting( void )
{
    return( KB_HEAD != KB_TAIL );
}

/* Discard what was typed, without DOS seeing a Ctrl-C in it. */
static void key_drain( void )
{
    _disable();
    KB_HEAD = KB_TAIL;
    _enable();
}

int main( int argc, char **argv )
{
    long    frames = 600;
    long    f;
    int     vsync = 1;
    int     i;
    int     o;
    int     j;
    int     a;
    int     nums = 0;
    int     stopped = 0;
    DWORD   t0;
    DWORD   t1;
    DWORD   out0;
    DWORD   in0;
    double  secs;
    FILE    *log;

    for( a = 1; a < argc; ++a ) {
        if( stricmp( argv[a], "nv" ) == 0 ) {
            vsync = 0;
        } else if( nums++ == 0 ) {
            frames = atol( argv[a] );
        } else {
            objects = atoi( argv[a] );
        }
    }
    if( frames < 1 ) {
        frames = 1;
    }
    if( objects < 1 || objects > MAX_OBJ ) {
        objects = objects < 1 ? 1 : MAX_OBJ;
    }
    fx_init();
    atexit( restore );
    signal( SIGINT, on_signal );
    signal( SIGBREAK, on_signal );

    mode_on();
    memset( pbox, 0, sizeof( pbox ) );
    w_clear = w_pass = w_fill = w_span = w_flip = n_fill = n_span = 0;
    out0 = n_out;
    in0 = n_in;
    t0 = BIOS_TICKS;
    while( BIOS_TICKS == t0 ) {         /* start on a tick edge */
    }
    t0 = BIOS_TICKS;
    for( f = 0; f < frames; ++f ) {
        int     back = !shown;
        int     dy = back ? PAGE1_Y : 0;
        short   nb[4] = { W, H, 0, 0 };
        DWORD   q0;

        if( key_waiting() ) {
            stopped = 1;
            break;
        }
        build( f );
        q0 = qn;
        q_clear( pbox[back][0], dy + pbox[back][1], pbox[back][2], dy + pbox[back][3] );
        w_clear += qn - q0;
        for( o = 0; o < objects; ++o ) {
            OBJ *ob = order[o];

            for( j = 0; j < ob->np; ++j ) {
                POLY *p = &ob->p[j];

                if( p->l < nb[0] ) nb[0] = p->l;
                if( p->t < nb[1] ) nb[1] = p->t;
                if( p->r + 1 > nb[2] ) nb[2] = p->r + 1;
                if( p->b + 1 > nb[3] ) nb[3] = p->b + 1;
            }
            for( j = 0; j < ob->np; j += PASS ) {
                q_pass( ob->p + j, ob->np - j < PASS ? ob->np - j : PASS, dy );
            }
        }
        if( qn > Q_MAX - 4096 ) {
            fail( "frame queue overflow" );
        }
        for( i = 0; i < 4; ++i ) {
            pbox[back][i] = nb[i];
        }
        if( vsync ) {
            page_wait();
        }
        q_send();
        page_show( back );
    }
    engine_idle();
    t1 = BIOS_TICKS;
    page_wait();
    out0 = n_out - out0;
    in0 = n_in - in0;
    restore();
    key_drain();

    secs = (t1 - t0) / 18.2065;
    if( secs <= 0 ) {
        secs = 1 / 18.2065;
    }
    if( f == 0 ) {
        f = 1;
    }
    {
        char line[512];

        sprintf( line,
            "M8CUBE32 objects %d vsync %s: %ld frames in %lu ticks = %.2f s, %.2f fps%s\n"
            "  port writes %lu (%.1f a frame), status reads %lu (%.1f a frame)\n"
            "  a frame: %.2f faces by polygon fill, %.2f by spans\n"
            "  writes a frame: back-page clear %.1f, pass set-up and scratch clears %.1f,\n"
            "    polygon-fill faces %.1f (%.1f a face), span faces %.1f (%.1f a face), flip %.1f\n"
            "  flip wait polls %lu, timeouts %lu\n",
            objects, vsync ? "on" : "off", f, t1 - t0, secs, f / secs, stopped ? " (key)" : "",
            out0, (double)out0 / f, in0, (double)in0 / f,
            (double)n_fill / f, (double)n_span / f,
            (double)w_clear / f, (double)w_pass / f,
            (double)w_fill / f, n_fill ? (double)w_fill / n_fill : 0.0,
            (double)w_span / f, n_span ? (double)w_span / n_span : 0.0, (double)w_flip / f,
            flip_polls, flip_timeouts );
        fputs( line, stdout );
        log = fopen( "M8CUBE32.TXT", "a" );
        if( log != NULL ) {
            fputs( line, log );
            fclose( log );
        }
    }
    return( 0 );
}
