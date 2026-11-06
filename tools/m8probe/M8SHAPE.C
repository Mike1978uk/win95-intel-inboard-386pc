/* M8SHAPE.EXE - what a triangle costs each way the engine can draw it, so the driver is changed
 * only for what the card proves. The triangles are M8D3DF's shape: (x,y), (x+s,y+s), (x,y+s).
 *
 * Arms, each 16 triangles, fastest of three timed runs in us a triangle:
 *   1 flat     the driver's way: edges in a scratch plane, fill, outline, scratch cleared every 8
 *   2 span     the driver's rows, for shapes up to 13 rows: compare with hal_many's engine time
 *   3 reuse    one shape's edges kept in a plane: fill and outline only
 *   4 rows     as reuse, with only the top and bottom rows in place of the outline
 *   5 expand   a whole triangle kept in a plane, drawn in one colour as a mono-from-card copy,
 *              with what never changes written once: 8 writes a triangle and no outline
 *   6 ramp     stored colours, 7 bits, edges XORed in and out of plane 7 of the ramp
 *   7 lookup   8 stored shades looked up in the colour pattern, loaded per triangle
 *   8 erase    the triangle's box filled: what a frame pays to take it away
 *   9 near     as flat, with the scratch on the triangle's own rows: is a fill's cost per pixel
 *              the distance between its source and destination in card memory?
 *  10 bands    as near, the fill's box cut into 3 by rows, each only as wide as it needs
 *  11 queue    flat's writes unchanged, built in a table and sent by a small asm loop, so the C
 *              that works them out runs with the cache intact between OUTs
 *  12 batch    the queue, with the edges of 8 triangles drawn together first and what does not
 *              change between triangles written once
 *  13 qerase   erase with its unchanging registers written once, through the queue
 *  14 noline   batch with the right-hand edges one pixel out in the scratch, so the fill reaches
 *              the edge pixels itself, and only the top and bottom rows drawn over it. Checked
 *              against flat on the bench's shape and on three others at each size
 *  15 core     expand, with the triangle's inscribed rectangle a plain fill and the mask used only
 *              for the two boxes left over: a plain fill costs a fifth of a mask copy a pixel
 *  16 page     one fill of a whole page, and how long the CPU is held: one command runs on its own
 *  18 stamp    a drawn triangle kept beside the page and stamped by a colour-keyed copy: one
 *              command, all 8 bits of colour, written with only room for its 9 writes
 *  17 corechk  m8d3d.c's way of choosing a core, its choice drawn on eight shapes at 32 and 100 px
 *              and each compared with flat pixel by pixel. Slopes are where it can go wrong
 *
 * Arms 2-5 are checked pixel by pixel against arm 1 over one triangle's box; a shape's difference is
 * only usable once that count is understood. Arms 6-7 have every triangle's centre read back. The
 * ALU_BG_FN and pattern state are the card's to lose, so the driver is asked to set up again at the end.
 *
 * Log, C:\M8SHAPE.TXT. On screen: the arm's number as white boxes along the bottom, repeated at each
 * size (8, 32, 100). Palette entries 1-127 become a ramp while it runs. About a minute.
 *
 * A Win32 console on Windows 95 is a DOS VM, so this is a window program.
 *
 * Build: wcl386 -q -bt=nt -l=nt_win -ox -i=<watcom>h -i=<watcom>h\nt -i=<watcom>h\nt\directx
 *        "-fe=M8SHAPE.EXE" M8SHAPE.C ddraw.lib
 */

#define INITGUID
#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <ddraw.h>

#define SCR_W           640
#define SCR_H           480
#define RAMP_X          640
#define RAMP_Y          512
#define RAMP_W          352
#define RAMP_H          256
#define SCRATCH_Y       768
#define SCRATCH_H       192
#define NTRI            16
#define PASSES          10
#define REPS            3
#define HOLD_MS         1000
#define BOX_MAX         102
#define SPAN_ROWS       13
#define LABEL_Y         470

#define GP_STAT         0x9AE8
#define CMD             0x9AE8
#define EXT_FIFO        0x9AEE
#define CUR_Y           0x82E8
#define CUR_X           0x86E8
#define SRC_Y           0x8AE8
#define SRC_X           0x8EE8
#define MAJ_AXIS_PCNT   0x96E8
#define FRGD_COLOR      0xA6E8
#define WRT_MASK        0xAAE8
#define RD_MASK         0xAEE8
#define FRGD_MIX        0xBAE8
#define MULTIFUNC_CNTL  0xBEE8
#define PIX_TRANS       0xE2E8
#define PATT_DATA_INDEX 0x82EE
#define PATT_DATA       0x8EEE
#define LINEDRAW_INDEX  0x9AEE
#define LINEDRAW_OPT    0xA2EE
#define DEST_X_START    0xA6EE
#define DEST_X_END      0xAAEE
#define DEST_Y_END      0xAEEE
#define SRC_X_START     0xB2EE
#define ALU_BG_FN       0xB6EE
#define ALU_FG_FN       0xBAEE
#define SRC_X_END       0xBEEE
#define SRC_Y_DIR       0xC2EE
#define SRC_CMP_FN      0x92EE
#define SRC_CMP_COLOR   0xEAEE
#define SCAN_TO_X       0xCAEE
#define DP_CONFIG       0xCEEE
#define PATT_LENGTH     0xD2EE
#define PATT_INDEX      0xD6EE
#define SCISSOR_L       0xDAEE
#define SCISSOR_T       0xDEEE
#define SCISSOR_R       0xE2EE
#define SCISSOR_B       0xE6EE
#define LINEDRAW        0xFEEE

#define DP_PLAIN        0x2211
#define DP_FILL_FLAT    0x2213
#define DP_FILL_VRAM    0x6213
#define DP_FILL_LOOKUP  0xE213
#define DP_MONO_VRAM    0x2271
#define DP_VRAM_BLIT    0x6211
#define MIX_LEAVE       3
#define MIX_XOR         5
#define MIX_REPLACE     7
#define OPT_LAST_OFF    4
#define OPT_POLY        2
#define ESC_RESYNC      0x4D3E
#define OUTLINE_PLANE   0x0080
#define EDGE_PLANE      0x0001
#define MASK_PLANE      0x0002

static FILE     *out;
static double   qpf;
static int      lost;           /* a wait that ran out: the timings are not trustworthy */
static int      size;
static short    tri[NTRI][6];
static BYTE     ref[BOX_MAX * BOX_MAX];

static double now( void )
{
    LARGE_INTEGER t;

    QueryPerformanceCounter( &t );
    return( (double)t.LowPart + 4294967296.0 * (double)t.HighPart );
}

static void wait_idle( void )
{
    long n;

    for( n = 0; n < 200000L; ++n ) {
        if( inpw( EXT_FIFO ) == 0 && !(inpw( GP_STAT ) & 0x0200) ) {
            return;
        }
    }
    ++lost;
}

/* A full FIFO can lock the card. */
static void room( int n )
{
    long i;

    for( i = 0; i < 200000L; ++i ) {
        if( !(inpw( EXT_FIFO ) & (0x8000 >> n)) ) {
            return;
        }
    }
    ++lost;
}

static void engine_setup( void )
{
    wait_idle();
    outpw( WRT_MASK, 0x00FF );
    outpw( RD_MASK, 0x00FF );
    outpw( MULTIFUNC_CNTL, 0xA000 );
    outpw( SCISSOR_L, 0x0800 );
    outpw( SCISSOR_T, 0x0800 );
    outpw( SCISSOR_R, 0x07FF );
    outpw( SCISSOR_B, 0x07FF );
    outpw( FRGD_MIX, 0x0027 );
    wait_idle();
}

static void rect( int x, int y, int w, int h, int colour )
{
    room( 12 );
    outpw( WRT_MASK, 0x00FF );
    outpw( RD_MASK, 0x00FF );
    outpw( DP_CONFIG, DP_PLAIN );
    outpw( FRGD_MIX, 0x0027 );
    outpw( FRGD_COLOR, colour );
    outpw( CUR_X, x );
    outpw( CUR_Y, y );
    outpw( MAJ_AXIS_PCNT, w - 1 );
    outpw( MULTIFUNC_CNTL, h - 1 );
    outpw( CMD, 0x40F3 );
}

/* Blue through green to red, in 6-bit DAC values. */
static void palette_ramp( void )
{
    int i;

    outp( 0x2EC, 1 );
    for( i = 1; i < 128; ++i ) {
        int r = i < 64 ? 0 : i - 64;
        int g = i < 64 ? i : 127 - i;
        int b = i < 64 ? 63 - i : 0;

        outp( 0x2ED, r );
        outp( 0x2ED, g );
        outp( 0x2ED, b );
    }
}

static BYTE pal_saved[128 * 3];

static void palette_save( void )
{
    int i;

    outp( 0x2EB, 0 );
    for( i = 0; i < 128 * 3; ++i ) {
        pal_saved[i] = (BYTE)inp( 0x2ED );
    }
}

static void palette_restore( void )
{
    int i;

    outp( 0x2EC, 0 );
    for( i = 0; i < 128 * 3; ++i ) {
        outp( 0x2ED, pal_saved[i] );
    }
}

static void outline( int ox, int oy, const short *v, int colour, int mask )
{
    int i;

    room( 14 );
    outpw( WRT_MASK, mask );
    outpw( FRGD_COLOR, colour );
    outpw( DP_CONFIG, DP_PLAIN );
    outpw( ALU_FG_FN, MIX_REPLACE );
    outpw( LINEDRAW_OPT, OPT_LAST_OFF );
    outpw( LINEDRAW_INDEX, 0 );
    for( i = 0; i < 4; ++i ) {
        outpw( LINEDRAW, ox + v[2 * (i % 3)] );
        outpw( LINEDRAW, oy + v[2 * (i % 3) + 1] );
    }
}

/* Each edge top to bottom without its last row, so every row between top and bottom holds exactly
 * two pixels; XOR, so an apex row's two pixels cancel. */
static void edges( int ox, int oy, const short *v, int plane )
{
    int i;

    room( 5 );
    outpw( WRT_MASK, plane );
    outpw( FRGD_COLOR, 0x00FF );
    outpw( DP_CONFIG, DP_PLAIN );
    outpw( ALU_FG_FN, MIX_XOR );
    outpw( LINEDRAW_OPT, OPT_POLY | OPT_LAST_OFF );
    for( i = 0; i < 3; ++i ) {
        int a = 2 * i;
        int b = 2 * ((i + 1) % 3);

        if( v[a + 1] == v[b + 1] ) {
            continue;
        }
        if( v[a + 1] > v[b + 1] ) {
            int t = a;

            a = b;
            b = t;
        }
        room( 5 );
        outpw( LINEDRAW_INDEX, 0 );
        outpw( LINEDRAW, ox + v[a] );
        outpw( LINEDRAW, oy + v[a + 1] );
        outpw( LINEDRAW, ox + v[b] );
        outpw( LINEDRAW, oy + v[b + 1] );
    }
}

/* The source's corner (sx, sy) stands for the box's; rows strictly between top and bottom. */
static void fill( const short *v, int dx, int dy, int sx, int sy, int dp, int mask, int rd, int colour )
{
    int l = v[0];
    int r = v[0];
    int t = v[1];
    int b = v[1];
    int i;

    for( i = 1; i < 3; ++i ) {
        if( v[2 * i] < l ) l = v[2 * i];
        if( v[2 * i] > r ) r = v[2 * i];
        if( v[2 * i + 1] < t ) t = v[2 * i + 1];
        if( v[2 * i + 1] > b ) b = v[2 * i + 1];
    }
    room( 15 );
    outpw( WRT_MASK, mask );
    outpw( RD_MASK, rd );
    outpw( DP_CONFIG, dp );
    outpw( ALU_FG_FN, MIX_REPLACE );
    outpw( FRGD_COLOR, colour );
    outpw( SRC_X, sx );
    outpw( SRC_Y, sy + 1 );
    outpw( SRC_X_START, sx );
    outpw( SRC_X_END, sx + r - l + 1 );
    outpw( SRC_Y_DIR, 1 );
    outpw( CUR_X, dx + l );
    outpw( DEST_X_START, dx + l );
    outpw( DEST_X_END, dx + r + 1 );
    outpw( CUR_Y, dy + t + 1 );
    outpw( DEST_Y_END, dy + b );
    room( 1 );
    outpw( RD_MASK, 0x00FF );
}

static void make_tris( void )
{
    int i;

    for( i = 0; i < NTRI; ++i ) {
        short x = (short)(4 + (i % 4) * 158);
        short y = (short)(4 + (i / 4) * 115);

        tri[i][0] = x;
        tri[i][1] = y;
        tri[i][2] = (short)(x + size);
        tri[i][3] = (short)(y + size);
        tri[i][4] = x;
        tri[i][5] = (short)(y + size);
    }
}

static int colour_of( int i )
{
    return( 1 + i * 8 );
}

/* ---- the arms ---- */

static int flat_drawn;
static int flat_dirty;

static void draw_flat( int i )
{
    const short *v = tri[i];
    int         plane = 1 << (flat_drawn & 7);
    int         l = v[0] < v[2] ? v[0] : v[2];
    int         t = v[1] < v[3] ? v[1] : v[3];

    l = v[4] < l ? v[4] : l;
    t = v[5] < t ? v[5] : t;
    edges( RAMP_X - l, SCRATCH_Y - t, v, plane );
    fill( v, 0, 0, RAMP_X, SCRATCH_Y, DP_FILL_FLAT, 0x00FF, plane, colour_of( i ) );
    outline( 0, 0, v, colour_of( i ), 0x00FF );
    if( size + 1 > flat_dirty ) {
        flat_dirty = size + 1;
    }
    if( ++flat_drawn == 8 ) {
        rect( RAMP_X, SCRATCH_Y, RAMP_W, flat_dirty + 1, 0 );
        flat_drawn = 0;
        flat_dirty = 0;
    }
}

/* As m8d3d.c m8d3d_spans. */
static void draw_span( int i )
{
    const short *v = tri[i];
    int         lo[SPAN_ROWS + 1];
    int         hi[SPAN_ROWS + 1];
    int         t = v[1];
    int         b = v[1];
    int         k;
    int         y;

    for( k = 1; k < 3; ++k ) {
        if( v[2 * k + 1] < t ) t = v[2 * k + 1];
        if( v[2 * k + 1] > b ) b = v[2 * k + 1];
    }
    for( y = 0; y <= b - t; ++y ) {
        lo[y] = 0x7FFF;
        hi[y] = -0x7FFF;
    }
    for( k = 0; k < 3; ++k ) {
        int p = 2 * k;
        int q = 2 * ((k + 1) % 3);
        int x;
        int step;

        if( v[p + 1] > v[q + 1] ) {
            int s = p;

            p = q;
            q = s;
        }
        step = v[q + 1] == v[p + 1] ? 0 : ((v[q] - v[p]) << 16) / (v[q + 1] - v[p + 1]);
        x = (v[p] << 16) + 0x8000;
        for( y = v[p + 1] - t; y <= v[q + 1] - t; ++y ) {
            int px = x >> 16;

            if( px < lo[y] ) lo[y] = px;
            if( px > hi[y] ) hi[y] = px;
            x += step;
        }
        if( v[q + 1] == v[p + 1] ) {
            y = v[p + 1] - t;
            if( v[q] < lo[y] ) lo[y] = v[q];
            if( v[q] > hi[y] ) hi[y] = v[q];
        }
    }
    room( 4 );
    outpw( WRT_MASK, 0x00FF );
    outpw( FRGD_COLOR, colour_of( i ) );
    outpw( DP_CONFIG, DP_PLAIN );
    outpw( ALU_FG_FN, MIX_REPLACE );
    for( y = 0; y <= b - t; ++y ) {
        if( y % 5 == 0 ) {
            room( 15 );
        }
        outpw( CUR_Y, t + y );
        outpw( CUR_X, lo[y] );
        outpw( SCAN_TO_X, hi[y] + 1 );
    }
}

static void prep_edges( void )
{
    rect( RAMP_X, SCRATCH_Y, RAMP_W, SCRATCH_H, 0 );
    edges( RAMP_X - tri[0][0], SCRATCH_Y - tri[0][1], tri[0], EDGE_PLANE );
    wait_idle();
}

static void draw_reuse( int i )
{
    fill( tri[i], 0, 0, RAMP_X, SCRATCH_Y, DP_FILL_FLAT, 0x00FF, EDGE_PLANE, colour_of( i ) );
    outline( 0, 0, tri[i], colour_of( i ), 0x00FF );
}

static void draw_rows( int i )
{
    const short *v = tri[i];

    fill( v, 0, 0, RAMP_X, SCRATCH_Y, DP_FILL_FLAT, 0x00FF, EDGE_PLANE, colour_of( i ) );
    room( 10 );
    outpw( FRGD_COLOR, colour_of( i ) );
    outpw( DP_CONFIG, DP_PLAIN );
    outpw( ALU_FG_FN, MIX_REPLACE );
    outpw( CUR_Y, v[1] );
    outpw( CUR_X, v[0] );
    outpw( SCAN_TO_X, v[0] + 1 );
    outpw( CUR_Y, v[5] );
    outpw( CUR_X, v[4] );
    outpw( SCAN_TO_X, v[2] + 1 );
}

/* The whole triangle in the mask plane: the edge plane filled into it where it lies, then the
 * rows the fill leaves. Everything that stays the same is written once. */
static void prep_expand( void )
{
    int ox = RAMP_X - tri[0][0];
    int oy = SCRATCH_Y - tri[0][1];

    prep_edges();
    fill( tri[0], ox, oy, RAMP_X, SCRATCH_Y, DP_FILL_FLAT, MASK_PLANE, EDGE_PLANE, 0x00FF );
    outline( ox, oy, tri[0], 0x00FF, MASK_PLANE );
    wait_idle();
    outpw( WRT_MASK, 0x00FF );
    outpw( RD_MASK, MASK_PLANE );
    outpw( DP_CONFIG, DP_MONO_VRAM );
    outpw( ALU_FG_FN, MIX_REPLACE );
    outpw( ALU_BG_FN, MIX_LEAVE );
    outpw( SRC_X_START, RAMP_X );
    outpw( SRC_X_END, RAMP_X + size + 1 );
    outpw( SRC_Y_DIR, 1 );
    wait_idle();
}

static void draw_expand( int i )
{
    room( 8 );
    outpw( FRGD_COLOR, colour_of( i ) );
    outpw( SRC_X, RAMP_X );
    outpw( SRC_Y, SCRATCH_Y );
    outpw( CUR_X, tri[i][0] );
    outpw( DEST_X_START, tri[i][0] );
    outpw( DEST_X_END, tri[i][0] + size + 1 );
    outpw( CUR_Y, tri[i][1] );
    outpw( DEST_Y_END, tri[i][1] + size + 1 );
}

static void end_expand( void )
{
    wait_idle();
    outpw( RD_MASK, 0x00FF );
    outpw( DP_CONFIG, DP_PLAIN );
    wait_idle();
}

static int ramp_at( int x, int y )
{
    int c = (x / 2 + y / 3) % 252;

    return( 1 + (c < 126 ? c : 251 - c) );
}

static void ramp_build( void )
{
    int x;
    int y;

    wait_idle();
    outpw( DP_CONFIG, DP_PLAIN );
    outpw( ALU_FG_FN, MIX_REPLACE );
    outpw( WRT_MASK, 0x00FF );
    for( y = 0; y < size + 2; ++y ) {
        for( x = 0; x < (size + 2) + 3 * 60; x += 4 ) {
            room( 4 );
            outpw( FRGD_COLOR, ramp_at( x, y ) );
            outpw( CUR_Y, RAMP_Y + y );
            outpw( CUR_X, RAMP_X + x );
            outpw( SCAN_TO_X, RAMP_X + x + 4 );
        }
    }
    wait_idle();
}

static void ramp_place( int i, int *x, int *y )
{
    *x = RAMP_X + (i % 4) * 60;
    *y = RAMP_Y;
}

static void draw_ramp( int i )
{
    const short *v = tri[i];
    int         sx;
    int         sy;
    int         ox;
    int         oy;

    ramp_place( i, &sx, &sy );
    ox = sx - v[0];
    oy = sy - v[1];
    edges( ox, oy, v, OUTLINE_PLANE );
    /* Plane 7 protected at the destination, so the edge marks do not land there. */
    fill( v, 0, 0, sx, sy, DP_FILL_VRAM, 0x007F, OUTLINE_PLANE, 0 );
    edges( ox, oy, v, OUTLINE_PLANE );
    outline( 0, 0, v, ramp_at( sx - RAMP_X + size / 3, size * 2 / 3 ), 0x00FF );
}

/* The source byte's low 3 bits pick a shade: pattern byte (shade << 2) | (x & 1). */
static int field_at( int x, int y )
{
    int s = (x + y) * 8 / (2 * size + 1);

    return( s > 7 ? 7 : s );
}

static void field_build( void )
{
    int x;
    int y;

    wait_idle();
    outpw( DP_CONFIG, DP_PLAIN );
    outpw( ALU_FG_FN, MIX_REPLACE );
    outpw( WRT_MASK, 0x00FF );
    for( y = 0; y < size + 2; ++y ) {
        for( x = 0; x < size + 2; ) {
            int s = field_at( x, y );
            int e = x + 1;

            while( e < size + 2 && field_at( e, y ) == s ) {
                ++e;
            }
            room( 4 );
            outpw( FRGD_COLOR, s );
            outpw( CUR_Y, RAMP_Y + y );
            outpw( CUR_X, RAMP_X + x );
            outpw( SCAN_TO_X, RAMP_X + e );
            x = e;
        }
    }
    wait_idle();
}

/* A triangle's shade `s` in palette terms: each triangle its own stretch of the ramp. */
static int lookup_colour( int i, int s )
{
    return( 1 + i * 6 + s * 4 );
}

static void pattern_load( int i )
{
    int w;

    room( 1 );
    outpw( PATT_DATA_INDEX, 0 );
    for( w = 0; w < 16; ++w ) {
        int c = lookup_colour( i, w / 2 );

        if( w % 8 == 0 ) {
            room( 8 );
        }
        outpw( PATT_DATA, c | c << 8 );
    }
}

static void prep_lookup( void )
{
    field_build();
    outpw( PATT_LENGTH, 0x001F );
    outpw( PATT_INDEX, 0 );
    wait_idle();
}

static void draw_lookup( int i )
{
    const short *v = tri[i];
    int         ox = RAMP_X - v[0];
    int         oy = RAMP_Y - v[1];

    pattern_load( i );
    edges( ox, oy, v, OUTLINE_PLANE );
    fill( v, 0, 0, RAMP_X, RAMP_Y, DP_FILL_LOOKUP, 0x00FF, OUTLINE_PLANE, 0 );
    edges( ox, oy, v, OUTLINE_PLANE );
    outline( 0, 0, v, lookup_colour( i, 4 ), 0x00FF );
}

static void draw_erase( int i )
{
    rect( tri[i][0], tri[i][1], size + 1, size + 1, 0 );
}

static int near_drawn;
static int near_top;
static int near_bottom;

/* The scratch's rows are the triangle's, so eight of them spread down the screen clear a tall box. */
static void near_after( const short *v )
{
    if( v[1] < near_top ) {
        near_top = v[1];
    }
    if( v[5] > near_bottom ) {
        near_bottom = v[5];
    }
    if( ++near_drawn == 8 ) {
        rect( RAMP_X, near_top, size + 2, near_bottom - near_top + 1, 0 );
        near_drawn = 0;
        near_top = 0x7FFF;
        near_bottom = 0;
    }
}

static void near_reset( void )
{
    rect( RAMP_X, 0, RAMP_W, LABEL_Y, 0 );
    near_drawn = 0;
    near_top = 0x7FFF;
    near_bottom = 0;
}

static void draw_near( int i )
{
    const short *v = tri[i];
    int         plane = 1 << (near_drawn & 7);

    edges( RAMP_X - v[0], 0, v, plane );
    fill( v, 0, 0, RAMP_X, v[1], DP_FILL_FLAT, 0x00FF, plane, colour_of( i ) );
    outline( 0, 0, v, colour_of( i ), 0x00FF );
    near_after( v );
}

/* Rows top .. bottom - 1 of the fill, x l .. r - 1 of the screen, the scratch on the same rows. */
static void fill_box( int base_x, int l, int r, int top, int bottom, int plane, int colour )
{
    room( 15 );
    outpw( WRT_MASK, 0x00FF );
    outpw( RD_MASK, plane );
    outpw( DP_CONFIG, DP_FILL_FLAT );
    outpw( ALU_FG_FN, MIX_REPLACE );
    outpw( FRGD_COLOR, colour );
    outpw( SRC_X, RAMP_X + l - base_x );
    outpw( SRC_Y, top );
    outpw( SRC_X_START, RAMP_X + l - base_x );
    outpw( SRC_X_END, RAMP_X + r - base_x );
    outpw( SRC_Y_DIR, 1 );
    outpw( CUR_X, l );
    outpw( DEST_X_START, l );
    outpw( DEST_X_END, r );
    outpw( CUR_Y, top );
    outpw( DEST_Y_END, bottom );
    room( 1 );
    outpw( RD_MASK, 0x00FF );
}

/* In this shape the left edge is upright and the right one runs down at 45 degrees. */
static void draw_bands( int i )
{
    const short *v = tri[i];
    int         plane = 1 << (near_drawn & 7);
    int         x = v[0];
    int         top = v[1];
    int         rows = size - 1;
    int         k;

    edges( RAMP_X - x, 0, v, plane );
    for( k = 0; k < 3 && rows > 0; ++k ) {
        int a = top + 1 + rows * k / 3;
        int b = top + 1 + rows * (k + 1) / 3;

        if( b > a ) {
            fill_box( x, x, x + (b - top) + 1, a, b, plane, colour_of( i ) );
        }
    }
    outline( 0, 0, v, colour_of( i ), 0x00FF );
    near_after( v );
}

/* ---- the queue ---- */

#define Q_MAX       512
#define Q_CHUNK     14

static DWORD    q_buf[Q_MAX];
static int      q_n;

/* Port in the low word, value in the high. */
extern void q_send( const DWORD *p, int n );
#pragma aux q_send = \
    "next_out:"             \
    "mov eax, [esi]"        \
    "mov edx, eax"          \
    "shr eax, 16"           \
    "out dx, ax"            \
    "add esi, 4"            \
    "dec ecx"               \
    "jnz next_out"          \
    parm [esi] [ecx] modify [eax edx esi ecx];

#define Q( port, value ) ( q_buf[q_n++] = (DWORD)(port) | (DWORD)(value) << 16 )

/* A full FIFO can lock the card, so room is seen for each chunk: one status read per 14 writes. */
static void q_flush( void )
{
    int i = 0;

    while( i < q_n ) {
        int n = q_n - i < Q_CHUNK ? q_n - i : Q_CHUNK;

        room( n );
        q_send( q_buf + i, n );
        i += n;
    }
    q_n = 0;
}

static void q_edge_list( int ox, int oy, const short *v )
{
    int k;

    for( k = 0; k < 3; ++k ) {
        int a = 2 * k;
        int b = 2 * ((k + 1) % 3);

        if( v[a + 1] == v[b + 1] ) {
            continue;
        }
        if( v[a + 1] > v[b + 1] ) {
            int t = a;

            a = b;
            b = t;
        }
        Q( LINEDRAW_INDEX, 0 );
        Q( LINEDRAW, ox + v[a] );
        Q( LINEDRAW, oy + v[a + 1] );
        Q( LINEDRAW, ox + v[b] );
        Q( LINEDRAW, oy + v[b + 1] );
    }
}

static void q_outline( const short *v )
{
    int k;

    Q( LINEDRAW_INDEX, 0 );
    for( k = 0; k < 4; ++k ) {
        Q( LINEDRAW, v[2 * (k % 3)] );
        Q( LINEDRAW, v[2 * (k % 3) + 1] );
    }
}

static void q_fill_geometry( const short *v, int plane )
{
    int r = v[2];
    int b = v[5];

    Q( RD_MASK, plane );
    Q( SRC_X, RAMP_X );
    Q( SRC_Y, SCRATCH_Y + 1 );
    Q( SRC_X_END, RAMP_X + r - v[0] + 1 );
    Q( CUR_X, v[0] );
    Q( DEST_X_START, v[0] );
    Q( DEST_X_END, r + 1 );
    Q( CUR_Y, v[1] + 1 );
    Q( DEST_Y_END, b );
}

static int queue_drawn;

static void draw_queue( int i )
{
    const short *v = tri[i];
    int         plane = 1 << (queue_drawn & 7);

    Q( WRT_MASK, plane );
    Q( FRGD_COLOR, 0x00FF );
    Q( DP_CONFIG, DP_PLAIN );
    Q( ALU_FG_FN, MIX_XOR );
    Q( LINEDRAW_OPT, OPT_POLY | OPT_LAST_OFF );
    q_edge_list( RAMP_X - v[0], SCRATCH_Y - v[1], v );
    Q( WRT_MASK, 0x00FF );
    Q( DP_CONFIG, DP_FILL_FLAT );
    Q( ALU_FG_FN, MIX_REPLACE );
    Q( FRGD_COLOR, colour_of( i ) );
    Q( SRC_X_START, RAMP_X );
    Q( SRC_Y_DIR, 1 );
    q_fill_geometry( v, plane );
    Q( RD_MASK, 0x00FF );
    Q( WRT_MASK, 0x00FF );
    Q( FRGD_COLOR, colour_of( i ) );
    Q( DP_CONFIG, DP_PLAIN );
    Q( ALU_FG_FN, MIX_REPLACE );
    Q( LINEDRAW_OPT, OPT_LAST_OFF );
    q_outline( v );
    q_flush();
    if( ++queue_drawn == 8 ) {
        rect( RAMP_X, SCRATCH_Y, size + 2, size + 2, 0 );
        queue_drawn = 0;
    }
}

/* Painting order is kept: only the scratch's edges, which nothing on the screen can see, move ahead. */
static void draw_batch( int i )
{
    const short *v = tri[i];
    int         k;

    if( i % 8 == 0 ) {
        rect( RAMP_X, SCRATCH_Y, size + 2, size + 2, 0 );
        Q( FRGD_COLOR, 0x00FF );
        Q( DP_CONFIG, DP_PLAIN );
        Q( ALU_FG_FN, MIX_XOR );
        Q( LINEDRAW_OPT, OPT_POLY | OPT_LAST_OFF );
        for( k = 0; k < 8 && i + k < NTRI; ++k ) {
            Q( WRT_MASK, 1 << k );
            q_edge_list( RAMP_X - tri[i + k][0], SCRATCH_Y - tri[i + k][1], tri[i + k] );
        }
        Q( WRT_MASK, 0x00FF );
        Q( ALU_FG_FN, MIX_REPLACE );
        Q( SRC_X_START, RAMP_X );
        Q( SRC_Y_DIR, 1 );
        Q( LINEDRAW_OPT, OPT_LAST_OFF );
    }
    Q( FRGD_COLOR, colour_of( i ) );
    Q( DP_CONFIG, DP_FILL_FLAT );
    q_fill_geometry( v, 1 << (i % 8) );
    Q( DP_CONFIG, DP_PLAIN );
    q_outline( v );
    q_flush();
}

/* Which edges bound the fill on the right: the long edge, or the other two when the middle vertex
 * lies to the right of it. Each is sent top down without its last row, as the fill needs. */
static void q_noline_edges( int ox, int oy, const short *v )
{
    int     top = 0;
    int     bot = 0;
    int     mid;
    int     k;
    long    side;

    for( k = 1; k < 3; ++k ) {
        if( v[2 * k + 1] < v[2 * top + 1] ) top = k;
        if( v[2 * k + 1] >= v[2 * bot + 1] ) bot = k;
    }
    if( top == bot ) {
        return;
    }
    mid = 3 - top - bot;
    side = (long)(v[2 * bot] - v[2 * top]) * (v[2 * mid + 1] - v[2 * top + 1]) -
           (long)(v[2 * bot + 1] - v[2 * top + 1]) * (v[2 * mid] - v[2 * top]);
    /* side < 0: the middle vertex is to the right of the long edge. */
    for( k = 0; k < 3; ++k ) {
        int a = k;
        int b = (k + 1) % 3;
        int is_long;
        int shift;

        if( v[2 * a + 1] == v[2 * b + 1] ) {
            continue;
        }
        if( v[2 * a + 1] > v[2 * b + 1] ) {
            int s = a;

            a = b;
            b = s;
        }
        is_long = (a == top && b == bot);
        shift = (side < 0) ? !is_long : is_long;
        Q( LINEDRAW_INDEX, 0 );
        Q( LINEDRAW, ox + v[2 * a] + shift );
        Q( LINEDRAW, oy + v[2 * a + 1] );
        Q( LINEDRAW, ox + v[2 * b] + shift );
        Q( LINEDRAW, oy + v[2 * b + 1] );
    }
}

static void tri_box( const short *v, int *l, int *t, int *r, int *b )
{
    int k;

    *l = *r = v[0];
    *t = *b = v[1];
    for( k = 1; k < 3; ++k ) {
        if( v[2 * k] < *l ) *l = v[2 * k];
        if( v[2 * k] > *r ) *r = v[2 * k];
        if( v[2 * k + 1] < *t ) *t = v[2 * k + 1];
        if( v[2 * k + 1] > *b ) *b = v[2 * k + 1];
    }
}

/* Row y's pixels where only vertices can lie on it: the top or bottom row. */
static void q_end_row( const short *v, int y )
{
    int lo = 0x7FFF;
    int hi = -0x7FFF;
    int k;

    for( k = 0; k < 3; ++k ) {
        if( v[2 * k + 1] == y ) {
            if( v[2 * k] < lo ) lo = v[2 * k];
            if( v[2 * k] > hi ) hi = v[2 * k];
        }
    }
    Q( CUR_Y, y );
    Q( CUR_X, lo );
    Q( SCAN_TO_X, hi + 1 );
}

static void draw_noline( int i )
{
    const short *v = tri[i];
    int         l;
    int         t;
    int         r;
    int         b;
    int         k;

    if( i % 8 == 0 ) {
        rect( RAMP_X, SCRATCH_Y, size + 3, size + 2, 0 );
        Q( FRGD_COLOR, 0x00FF );
        Q( DP_CONFIG, DP_PLAIN );
        Q( ALU_FG_FN, MIX_XOR );
        Q( LINEDRAW_OPT, OPT_POLY | OPT_LAST_OFF );
        for( k = 0; k < 8 && i + k < NTRI; ++k ) {
            tri_box( tri[i + k], &l, &t, &r, &b );
            Q( WRT_MASK, 1 << k );
            q_noline_edges( RAMP_X - l, SCRATCH_Y - t, tri[i + k] );
        }
        Q( WRT_MASK, 0x00FF );
        Q( ALU_FG_FN, MIX_REPLACE );
        Q( SRC_X_START, RAMP_X );
        Q( SRC_Y_DIR, 1 );
    }
    tri_box( v, &l, &t, &r, &b );
    Q( FRGD_COLOR, colour_of( i ) );
    if( b - t >= 2 ) {
        Q( DP_CONFIG, DP_FILL_FLAT );
        Q( RD_MASK, 1 << (i % 8) );
        Q( SRC_X, RAMP_X );
        Q( SRC_Y, SCRATCH_Y + 1 );
        Q( SRC_X_END, RAMP_X + r - l + 2 );
        Q( CUR_X, l );
        Q( DEST_X_START, l );
        Q( DEST_X_END, r + 2 );
        Q( CUR_Y, t + 1 );
        Q( DEST_Y_END, b );
    }
    Q( DP_CONFIG, DP_PLAIN );
    q_end_row( v, t );
    q_end_row( v, b );
    q_flush();
}

/* The other shapes: nothing else in this program draws them, and tri[0] goes back afterwards. */
static long noline_shape_diff( const short *shape )
{
    short   keep[6];
    int     k;
    long    diff;
    int     l;
    int     t;
    int     x;
    int     y;
    int     r;
    int     b;
    static BYTE want[BOX_MAX + 2][BOX_MAX + 2];

    for( k = 0; k < 6; ++k ) {
        keep[k] = tri[0][k];
        tri[0][k] = shape[k];
    }
    tri_box( shape, &l, &t, &r, &b );
    rect( 0, 0, SCR_W, LABEL_Y, 0 );
    rect( RAMP_X, SCRATCH_Y, RAMP_W, SCRATCH_H, 0 );
    flat_drawn = 0;
    flat_dirty = 0;
    draw_flat( 0 );
    wait_idle();
    for( y = t; y <= b; ++y ) {
        for( x = l; x <= r + 1; ++x ) {
            want[y - t][x - l] = (BYTE)read_pixel( x, y );
        }
    }
    rect( 0, 0, SCR_W, LABEL_Y, 0 );
    rect( RAMP_X, SCRATCH_Y, RAMP_W, SCRATCH_H, 0 );
    draw_noline( 0 );
    wait_idle();
    outpw( RD_MASK, 0x00FF );
    diff = 0;
    for( y = t; y <= b; ++y ) {
        for( x = l; x <= r + 1; ++x ) {
            int got = read_pixel( x, y );

            if( got != want[y - t][x - l] ) {
                if( diff < 3 ) {
                    fprintf( out, "    (%d,%d): read %02x, flat %02x\n", x - l, y - t, got, want[y - t][x - l] );
                }
                ++diff;
            }
        }
    }
    for( k = 0; k < 6; ++k ) {
        tri[0][k] = keep[k];
    }
    rect( RAMP_X, SCRATCH_Y, RAMP_W, SCRATCH_H, 0 );
    return( diff );
}

static void noline_shapes( void )
{
    short   x = 20;
    short   y = 20;
    short   s = (short)size;
    short   a[6];
    short   b[6];
    short   c[6];

    a[0] = x + s / 2; a[1] = y;        a[2] = x + s; a[3] = y + s - s / 3; a[4] = x;         a[5] = y + s;
    b[0] = x;         b[1] = y;        b[2] = x + s; b[3] = y + s / 3;     b[4] = x + s / 2; b[5] = y + s;
    c[0] = x + s / 2; c[1] = y;        c[2] = x + s; c[3] = y + s;         c[4] = x;         c[5] = y + s / 3;
    fprintf( out, "  noline differs from flat: shape a %ld, b %ld, c %ld pixels\n",
             noline_shape_diff( a ), noline_shape_diff( b ), noline_shape_diff( c ) );
}

/* The bench's shape has its right angle at bottom left. */
static void expand_box( int i, int x, int y, int w, int h )
{
    room( 9 );
    outpw( SRC_X_END, RAMP_X + x + w );
    outpw( SRC_X, RAMP_X + x );
    outpw( SRC_Y, SCRATCH_Y + y );
    outpw( CUR_X, tri[i][0] + x );
    outpw( DEST_X_START, tri[i][0] + x );
    outpw( DEST_X_END, tri[i][0] + x + w );
    outpw( SRC_X_START, RAMP_X + x );
    outpw( CUR_Y, tri[i][1] + y );
    outpw( DEST_Y_END, tri[i][1] + y + h );
}

static void draw_core( int i )
{
    int half = (size + 1) / 2;

    room( 9 );
    outpw( RD_MASK, 0x00FF );
    outpw( DP_CONFIG, DP_PLAIN );
    outpw( FRGD_COLOR, colour_of( i ) );
    outpw( CUR_X, tri[i][0] );
    outpw( CUR_Y, tri[i][1] + half );
    outpw( MAJ_AXIS_PCNT, half - 1 );
    outpw( MULTIFUNC_CNTL, size - half );
    outpw( CMD, 0x40F3 );
    room( 3 );
    outpw( RD_MASK, MASK_PLANE );
    outpw( DP_CONFIG, DP_MONO_VRAM );
    outpw( SRC_Y_DIR, 1 );
    expand_box( i, 0, 0, half, half );
    expand_box( i, half, half, size + 1 - half, size + 1 - half );
}

/* ---- as m8d3d.c: keep the two in step ---- */

#define CHK_NS_MASK     69L
#define CHK_NS_PLAIN    21L
#define CHK_NS_COMMAND  60000L
#define CHK_SAMPLES     6

typedef struct {
    short x, y, w, h;
} CHK_BOX;

static CHK_BOX  chk_core;
static CHK_BOX  chk_part[4];
static int      chk_parts;

static void chk_row_span( const short *n, int y, int *lo, int *hi, int inner )
{
    int i;

    *lo = 0x7FFF;
    *hi = -0x7FFF;
    for( i = 0; i < 3; ++i ) {
        int ax = n[2 * i];
        int ay = n[2 * i + 1];
        int bx = n[2 * ((i + 1) % 3)];
        int by = n[2 * ((i + 1) % 3) + 1];
        int x;
        int run;

        if( ay > by ) {
            int s = ax;

            ax = bx;
            bx = s;
            s = ay;
            ay = by;
            by = s;
        }
        if( y < ay || y > by ) {
            continue;
        }
        if( ay == by ) {
            if( ax < *lo ) *lo = ax;
            if( bx < *lo ) *lo = bx;
            if( ax > *hi ) *hi = ax;
            if( bx > *hi ) *hi = bx;
            continue;
        }
        x = ax + (bx - ax) * (y - ay) / (by - ay);
        run = bx == ax ? 0 : (bx > ax ? bx - ax : ax - bx) / (by - ay) / 2 + 1;
        if( inner ) {
            run = -run;
        }
        if( x - run < *lo ) *lo = x - run;
        if( x + run > *hi ) *hi = x + run;
    }
}

static void chk_one_part( const short *n, int top, int bottom, int from, int to )
{
    int lo = 0x7FFF;
    int hi = -0x7FFF;
    int k;

    for( k = 0; k < 5; ++k ) {
        int y = k == 0 ? top : k == 1 ? bottom : n[2 * (k - 2) + 1];
        int a;
        int b;

        if( y < top || y > bottom ) {
            continue;
        }
        chk_row_span( n, y, &a, &b, 0 );
        if( a < from ) a = from;
        if( b > to ) b = to;
        if( a <= b ) {
            if( a < lo ) lo = a;
            if( b > hi ) hi = b;
        }
    }
    --lo;
    ++hi;
    if( lo < from ) lo = from;
    if( hi > to ) hi = to;
    if( hi >= lo ) {
        chk_part[chk_parts].x = (short)lo;
        chk_part[chk_parts].y = (short)top;
        chk_part[chk_parts].w = (short)(hi - lo + 1);
        chk_part[chk_parts].h = (short)(bottom - top + 1);
        ++chk_parts;
    }
}

static void chk_parts_of( const short *n, int w, int h, int l, int t, int r, int b )
{
    chk_parts = 0;
    if( t > 0 ) chk_one_part( n, 0, t - 1, 0, w - 1 );
    if( l > 0 ) chk_one_part( n, t, b, 0, l - 1 );
    if( r < w - 1 ) chk_one_part( n, t, b, r + 1, w - 1 );
    if( b < h - 1 ) chk_one_part( n, b + 1, h - 1, 0, w - 1 );
}

static void chk_choose( const short *n, int w, int h )
{
    long    best = (long)w * h * CHK_NS_MASK + CHK_NS_COMMAND;
    int     step = h / CHK_SAMPLES;
    int     best_t = -1;
    int     best_b = 0;
    int     best_l = 0;
    int     best_r = 0;
    int     a;
    int     b;
    int     i;

    chk_core.w = 0;
    chk_parts = 0;
    if( h < 48 || w < 48 ) {
        return;
    }
    if( step < 1 ) {
        step = 1;
    }
    for( a = 0; a < h; a = a + step < h || a == h - 1 ? a + step : h - 1 ) {
        for( b = a + step; ; b += step ) {
            int     alo;
            int     ahi;
            int     blo;
            int     bhi;
            int     l;
            int     r;
            long    cost;

            if( b > h - 1 ) {
                b = h - 1;
            }
            if( b <= a ) {
                break;
            }
            chk_row_span( n, a, &alo, &ahi, 1 );
            chk_row_span( n, b, &blo, &bhi, 1 );
            l = alo > blo ? alo : blo;
            r = ahi < bhi ? ahi : bhi;
            if( r > l ) {
                chk_parts_of( n, w, h, l, a, r, b );
                cost = (long)(r - l + 1) * (b - a + 1) * CHK_NS_PLAIN + (long)(chk_parts + 1) * CHK_NS_COMMAND;
                for( i = 0; i < chk_parts; ++i ) {
                    cost += (long)chk_part[i].w * chk_part[i].h * CHK_NS_MASK;
                }
                if( cost < best ) {
                    best = cost;
                    best_t = a;
                    best_b = b;
                    best_l = l;
                    best_r = r;
                }
            }
            if( b == h - 1 ) {
                break;
            }
        }
    }
    chk_parts = 0;
    if( best_t >= 0 ) {
        chk_core.x = (short)best_l;
        chk_core.y = (short)best_t;
        chk_core.w = (short)(best_r - best_l + 1);
        chk_core.h = (short)(best_b - best_t + 1);
        chk_parts_of( n, w, h, best_l, best_t, best_r, best_b );
    }
}

static void chk_box_draw( const CHK_BOX *b, int x, int y )
{
    room( 9 );
    outpw( SRC_X_START, RAMP_X + b->x );
    outpw( SRC_X_END, RAMP_X + b->x + b->w );
    outpw( SRC_X, RAMP_X + b->x );
    outpw( SRC_Y, SCRATCH_Y + b->y );
    outpw( CUR_X, x + b->x );
    outpw( DEST_X_START, x + b->x );
    outpw( DEST_X_END, x + b->x + b->w );
    outpw( CUR_Y, y + b->y );
    outpw( DEST_Y_END, y + b->y + b->h );
}

/* Flat, then the core's way, at the same place; prints what it chose and how many pixels differ. */
static void core_check_shape( int which, const short *shape )
{
    static BYTE want[BOX_MAX + 2][BOX_MAX + 2];
    short       keep[6];
    short       n[6];
    int         l = shape[0];
    int         t = shape[1];
    int         r = shape[0];
    int         b = shape[1];
    int         k;
    int         x;
    int         y;
    int         w;
    int         h;
    long        diff = 0;

    for( k = 1; k < 3; ++k ) {
        if( shape[2 * k] < l ) l = shape[2 * k];
        if( shape[2 * k] > r ) r = shape[2 * k];
        if( shape[2 * k + 1] < t ) t = shape[2 * k + 1];
        if( shape[2 * k + 1] > b ) b = shape[2 * k + 1];
    }
    w = r - l + 1;
    h = b - t + 1;
    for( k = 0; k < 3; ++k ) {
        n[2 * k] = (short)(shape[2 * k] - l);
        n[2 * k + 1] = (short)(shape[2 * k + 1] - t);
    }
    for( k = 0; k < 6; ++k ) {
        keep[k] = tri[0][k];
        tri[0][k] = shape[k];
    }
    rect( 0, 0, SCR_W, LABEL_Y, 0 );
    rect( RAMP_X, SCRATCH_Y, RAMP_W, SCRATCH_H, 0 );
    flat_drawn = 0;
    flat_dirty = 0;
    draw_flat( 0 );
    wait_idle();
    for( y = 0; y < h; ++y ) {
        for( x = 0; x < w; ++x ) {
            want[y][x] = (BYTE)read_pixel( l + x, t + y );
        }
    }
    rect( 0, 0, SCR_W, LABEL_Y, 0 );
    rect( RAMP_X, SCRATCH_Y, RAMP_W, SCRATCH_H, 0 );
    edges( RAMP_X, SCRATCH_Y, n, EDGE_PLANE );
    fill( n, RAMP_X, SCRATCH_Y, RAMP_X, SCRATCH_Y, DP_FILL_FLAT, MASK_PLANE, EDGE_PLANE, 0x00FF );
    outline( RAMP_X, SCRATCH_Y, n, 0x00FF, MASK_PLANE );
    wait_idle();
    chk_choose( n, w, h );
    outpw( WRT_MASK, 0x00FF );
    outpw( ALU_FG_FN, MIX_REPLACE );
    outpw( ALU_BG_FN, MIX_LEAVE );
    outpw( FRGD_COLOR, colour_of( 0 ) );
    outpw( SRC_Y_DIR, 1 );
    if( chk_core.w != 0 ) {
        outpw( RD_MASK, 0x00FF );
        outpw( DP_CONFIG, DP_PLAIN );
        wait_idle();
        room( 5 );
        outpw( CUR_X, l + chk_core.x );
        outpw( CUR_Y, t + chk_core.y );
        outpw( MAJ_AXIS_PCNT, chk_core.w - 1 );
        outpw( MULTIFUNC_CNTL, chk_core.h - 1 );
        outpw( CMD, 0x40F3 );
        wait_idle();
    }
    outpw( RD_MASK, MASK_PLANE );
    outpw( DP_CONFIG, DP_MONO_VRAM );
    wait_idle();
    if( chk_core.w != 0 ) {
        for( k = 0; k < chk_parts; ++k ) {
            chk_box_draw( &chk_part[k], l, t );
        }
    } else {
        CHK_BOX whole;

        whole.x = 0;
        whole.y = 0;
        whole.w = (short)w;
        whole.h = (short)h;
        chk_box_draw( &whole, l, t );
    }
    wait_idle();
    outpw( RD_MASK, 0x00FF );
    outpw( DP_CONFIG, DP_PLAIN );
    wait_idle();
    for( y = 0; y < h; ++y ) {
        for( x = 0; x < w; ++x ) {
            int got = read_pixel( l + x, t + y );

            if( got != want[y][x] ) {
                if( diff < 3 ) {
                    fprintf( out, "    (%d,%d): read %02x, flat %02x\n", x, y, got, want[y][x] );
                }
                ++diff;
            }
        }
    }
    fprintf( out, "  corechk size %3d shape %d: core %dx%d at %d,%d, %d parts, %ld pixels differ\n", size, which,
             chk_core.w, chk_core.h, chk_core.x, chk_core.y, chk_parts, diff );
    fflush( out );
    for( k = 0; k < 6; ++k ) {
        tri[0][k] = keep[k];
    }
    rect( RAMP_X, SCRATCH_Y, RAMP_W, SCRATCH_H, 0 );
}

static void core_checks( void )
{
    short   x = 20;
    short   y = 20;
    short   s = (short)size;
    short   shapes[14][6];
    int     k;

    shapes[0][0] = x; shapes[0][1] = y; shapes[0][2] = x + s; shapes[0][3] = y + s; shapes[0][4] = x; shapes[0][5] = y + s;
    shapes[1][0] = x + s / 2; shapes[1][1] = y; shapes[1][2] = x + s; shapes[1][3] = y + s - s / 3; shapes[1][4] = x; shapes[1][5] = y + s;
    shapes[2][0] = x; shapes[2][1] = y; shapes[2][2] = x + s; shapes[2][3] = y + s / 3; shapes[2][4] = x + s / 2; shapes[2][5] = y + s;
    shapes[3][0] = x + s / 2; shapes[3][1] = y; shapes[3][2] = x + s; shapes[3][3] = y + s; shapes[3][4] = x; shapes[3][5] = y + s / 3;
    shapes[4][0] = x; shapes[4][1] = y; shapes[4][2] = x + s; shapes[4][3] = y; shapes[4][4] = x + s; shapes[4][5] = y + s;
    shapes[5][0] = x; shapes[5][1] = y; shapes[5][2] = x + s; shapes[5][3] = y; shapes[5][4] = x + s / 3; shapes[5][5] = y + s;
    shapes[6][0] = x + s / 3; shapes[6][1] = y; shapes[6][2] = x + s; shapes[6][3] = y + s; shapes[6][4] = x; shapes[6][5] = y + s;
    shapes[7][0] = x; shapes[7][1] = y + s / 5; shapes[7][2] = x + s; shapes[7][3] = y; shapes[7][4] = x + s / 4; shapes[7][5] = y + s;
    /* Shallow and steep edges, where a row or column holds runs of pixels. */
    shapes[8][0] = x; shapes[8][1] = y + s / 2; shapes[8][2] = x + s; shapes[8][3] = y; shapes[8][4] = x + s; shapes[8][5] = y + s;
    shapes[9][0] = x; shapes[9][1] = y; shapes[9][2] = x + s; shapes[9][3] = y + s / 2; shapes[9][4] = x; shapes[9][5] = y + s;
    shapes[10][0] = x; shapes[10][1] = y + s / 2 + 3; shapes[10][2] = x + s; shapes[10][3] = y; shapes[10][4] = x + s; shapes[10][5] = y + s / 2 + 8;
    shapes[11][0] = x + s / 2; shapes[11][1] = y; shapes[11][2] = x + s / 2 + 8; shapes[11][3] = y + s; shapes[11][4] = x; shapes[11][5] = y + s - 2;
    shapes[12][0] = x + 2; shapes[12][1] = y + 7; shapes[12][2] = x + s; shapes[12][3] = y + s / 7; shapes[12][4] = x + s / 2; shapes[12][5] = y + s;
    shapes[13][0] = x + s; shapes[13][1] = y; shapes[13][2] = x + s - 3; shapes[13][3] = y + s; shapes[13][4] = x; shapes[13][5] = y + s / 3;
    for( k = 0; k < 14; ++k ) {
        core_check_shape( k, shapes[k] );
    }
}

#define STAMP_Y         0

static void prep_stamp( void )
{
    short   keep[6];
    int     k;

    rect( RAMP_X, STAMP_Y, RAMP_W, size + 2, 0 );
    rect( RAMP_X, SCRATCH_Y, RAMP_W, SCRATCH_H, 0 );
    for( k = 0; k < 6; ++k ) {
        keep[k] = tri[0][k];
    }
    tri[0][0] = RAMP_X;
    tri[0][1] = STAMP_Y;
    tri[0][2] = RAMP_X + (short)size;
    tri[0][3] = STAMP_Y + (short)size;
    tri[0][4] = RAMP_X;
    tri[0][5] = STAMP_Y + (short)size;
    flat_drawn = 0;
    flat_dirty = 0;
    draw_flat( 0 );
    for( k = 0; k < 6; ++k ) {
        tri[0][k] = keep[k];
    }
    wait_idle();
    outpw( WRT_MASK, 0x00FF );
    outpw( RD_MASK, 0x00FF );
    outpw( DP_CONFIG, DP_VRAM_BLIT );
    outpw( ALU_FG_FN, MIX_REPLACE );
    outpw( ALU_BG_FN, MIX_LEAVE );
    outpw( SRC_CMP_COLOR, 0 );
    outpw( SRC_CMP_FN, 0x0028 );
    outpw( SRC_Y_DIR, 1 );
    outpw( SRC_X_START, RAMP_X );
    outpw( SRC_X_END, RAMP_X + size + 1 );
    wait_idle();
}

static void draw_stamp( int i )
{
    room( 7 );
    outpw( SRC_X, RAMP_X );
    outpw( SRC_Y, STAMP_Y );
    outpw( CUR_X, tri[i][0] );
    outpw( DEST_X_START, tri[i][0] );
    outpw( DEST_X_END, tri[i][0] + size + 1 );
    outpw( CUR_Y, tri[i][1] );
    outpw( DEST_Y_END, tri[i][1] + size + 1 );
}

static void end_stamp( void )
{
    wait_idle();
    outpw( SRC_CMP_FN, 0 );
    outpw( DP_CONFIG, DP_PLAIN );
    wait_idle();
}

static double page_cpu_us;

static void page_fill( void )
{
    double t0;

    wait_idle();
    t0 = now();
    rect( 0, 0, SCR_W, SCR_H, 0 );
    page_cpu_us = (now() - t0) * 1e6 / qpf;
    wait_idle();
}

static void prep_qerase( void )
{
    rect( 0, 0, 1, 1, 0 );
    wait_idle();
}

static void draw_qerase( int i )
{
    Q( CUR_X, tri[i][0] );
    Q( CUR_Y, tri[i][1] );
    Q( MAJ_AXIS_PCNT, size );
    Q( MULTIFUNC_CNTL, size );
    Q( CMD, 0x40F3 );
    q_flush();
}

/* ---- measuring ---- */

static int read_pixel( int x, int y )
{
    int v;
    long n;

    wait_idle();
    outpw( WRT_MASK, 0x00FF );
    outpw( RD_MASK, 0x00FF );
    outpw( DP_CONFIG, DP_PLAIN );
    outpw( FRGD_MIX, 0x0047 );
    outpw( CUR_X, x );
    outpw( CUR_Y, y );
    outpw( MAJ_AXIS_PCNT, 1 );
    outpw( MULTIFUNC_CNTL, 0 );
    wait_idle();
    outpw( CMD, 0x53B0 );
    for( n = 0; n < 200000L && !(inpw( GP_STAT ) & 0x0100); ++n ) {
    }
    v = inpw( PIX_TRANS ) & 0xFF;
    wait_idle();
    outpw( FRGD_MIX, 0x0027 );
    return( v );
}

/* Triangle 0's box as drawn by an arm: taken as the reference, or counted against it. */
static long box_check( int take )
{
    int     x;
    int     y;
    long    diff = 0;

    for( y = 0; y <= size; ++y ) {
        for( x = 0; x <= size; ++x ) {
            int v = read_pixel( tri[0][0] + x, tri[0][1] + y );

            if( take ) {
                ref[y * BOX_MAX + x] = (BYTE)v;
            } else if( ref[y * BOX_MAX + x] != (BYTE)v ) {
                if( diff < 4 ) {
                    fprintf( out, "    (%d,%d): read %02x, flat %02x\n", x, y, v, ref[y * BOX_MAX + x] );
                }
                ++diff;
            }
        }
    }
    return( diff );
}

static void label( int arm )
{
    int k;

    rect( 0, LABEL_Y, SCR_W, SCR_H - LABEL_Y, 0 );
    for( k = 0; k < arm; ++k ) {
        rect( 4 + k * 14, LABEL_Y, 10, 8, 127 );
    }
}

static double timed( void (*draw)( int ) )
{
    double  best = 1e30;
    int     r;
    int     p;
    int     i;

    for( r = 0; r < REPS; ++r ) {
        double t0;

        wait_idle();
        t0 = now();
        for( p = 0; p < PASSES; ++p ) {
            for( i = 0; i < NTRI; ++i ) {
                draw( i );
            }
        }
        wait_idle();
        t0 = now() - t0;
        if( t0 < best ) {
            best = t0;
        }
    }
    return( best * 1e6 / qpf / (PASSES * NTRI) );
}

/* Show the arm, and leave triangle 0 alone on the screen for its check. */
static void show( int arm, void (*draw)( int ) )
{
    int i;

    rect( 0, 0, SCR_W, LABEL_Y, 0 );
    label( arm );
    for( i = 0; i < NTRI; ++i ) {
        draw( i );
    }
    wait_idle();
    Sleep( HOLD_MS );
    rect( 0, 0, SCR_W, LABEL_Y, 0 );
    draw( 0 );
    wait_idle();
}

static int centre_x( int i )
{
    return( tri[i][0] + size / 3 );
}

static int centre_y( int i )
{
    return( tri[i][1] + size * 2 / 3 );
}

static void check_ramp( void )
{
    int i;
    int bad = 0;

    for( i = 0; i < NTRI; ++i ) {
        int sx;
        int sy;
        int got;
        int want;

        ramp_place( i, &sx, &sy );
        got = read_pixel( centre_x( i ), centre_y( i ) );
        want = ramp_at( (sx - RAMP_X + size / 3) & ~3, size * 2 / 3 );
        if( got != want ) {
            if( bad < 3 ) {
                fprintf( out, "    ramp %d: read %02x, want %02x\n", i, got, want );
            }
            ++bad;
        }
    }
    fprintf( out, "  ramp centres wrong: %d of %d\n", bad, NTRI );
}

static void check_lookup( void )
{
    int i;
    int bad = 0;

    for( i = 0; i < NTRI; ++i ) {
        int got = read_pixel( centre_x( i ), centre_y( i ) );
        int want = lookup_colour( i, field_at( size / 3, size * 2 / 3 ) );

        if( got != want ) {
            if( bad < 3 ) {
                fprintf( out, "    lookup %d: read %02x, want %02x\n", i, got, want );
            }
            ++bad;
        }
    }
    fprintf( out, "  lookup centres wrong: %d of %d\n", bad, NTRI );
}

static void report( const char *name, double us, double base )
{
    fprintf( out, "size %3d  %-7s %8.1f us/tri  %.2fx flat\n", size, name, us, us / base );
    fflush( out );
}

static void one_size( int s )
{
    double  base;
    double  us;
    int     i;

    size = s;
    make_tris();

    /* 1 flat: the reference */
    flat_drawn = 0;
    flat_dirty = 0;
    rect( RAMP_X, SCRATCH_Y, RAMP_W, SCRATCH_H, 0 );
    show( 1, draw_flat );
    box_check( 1 );
    base = timed( draw_flat );
    report( "flat", base, base );
    rect( RAMP_X, SCRATCH_Y, RAMP_W, SCRATCH_H, 0 );
    wait_idle();

    /* 2 span */
    if( s <= SPAN_ROWS ) {
        show( 2, draw_span );
        fprintf( out, "  span differs from flat in %ld pixels\n", box_check( 0 ) );
        report( "span", timed( draw_span ), base );
    }

    /* 3 reuse */
    prep_edges();
    show( 3, draw_reuse );
    fprintf( out, "  reuse differs from flat in %ld pixels\n", box_check( 0 ) );
    report( "reuse", timed( draw_reuse ), base );

    /* 4 rows */
    show( 4, draw_rows );
    fprintf( out, "  rows differs from flat in %ld pixels\n", box_check( 0 ) );
    report( "rows", timed( draw_rows ), base );

    /* 5 expand */
    rect( 0, 0, SCR_W, LABEL_Y, 0 );
    label( 5 );
    /* Last: a rect() would undo the state it writes once. */
    prep_expand();
    for( i = 0; i < NTRI; ++i ) {
        draw_expand( i );
    }
    wait_idle();
    Sleep( HOLD_MS );
    us = timed( draw_expand );
    end_expand();
    rect( 0, 0, SCR_W, LABEL_Y, 0 );
    prep_expand();
    draw_expand( 0 );
    end_expand();
    fprintf( out, "  expand differs from flat in %ld pixels\n", box_check( 0 ) );
    report( "expand", us, base );
    rect( RAMP_X, SCRATCH_Y, RAMP_W, SCRATCH_H, 0 );
    wait_idle();

    /* 6 ramp */
    ramp_build();
    rect( 0, 0, SCR_W, LABEL_Y, 0 );
    label( 6 );
    for( i = 0; i < NTRI; ++i ) {
        draw_ramp( i );
    }
    wait_idle();
    check_ramp();
    Sleep( HOLD_MS );
    report( "ramp", timed( draw_ramp ), base );

    /* 7 lookup */
    prep_lookup();
    rect( 0, 0, SCR_W, LABEL_Y, 0 );
    label( 7 );
    for( i = 0; i < NTRI; ++i ) {
        draw_lookup( i );
    }
    wait_idle();
    check_lookup();
    Sleep( HOLD_MS );
    report( "lookup", timed( draw_lookup ), base );

    /* 8 erase */
    label( 8 );
    report( "erase", timed( draw_erase ), base );

    /* 9 near */
    near_reset();
    show( 9, draw_near );
    fprintf( out, "  near differs from flat in %ld pixels\n", box_check( 0 ) );
    report( "near", timed( draw_near ), base );

    /* 10 bands */
    near_reset();
    show( 10, draw_bands );
    fprintf( out, "  bands differs from flat in %ld pixels\n", box_check( 0 ) );
    report( "bands", timed( draw_bands ), base );
    rect( RAMP_X, 0, RAMP_W, LABEL_Y, 0 );

    /* 11 queue */
    rect( RAMP_X, SCRATCH_Y, RAMP_W, SCRATCH_H, 0 );
    queue_drawn = 0;
    show( 11, draw_queue );
    fprintf( out, "  queue differs from flat in %ld pixels\n", box_check( 0 ) );
    report( "queue", timed( draw_queue ), base );
    rect( RAMP_X, SCRATCH_Y, RAMP_W, SCRATCH_H, 0 );

    /* 12 batch */
    show( 12, draw_batch );
    fprintf( out, "  batch differs from flat in %ld pixels\n", box_check( 0 ) );
    report( "batch", timed( draw_batch ), base );
    rect( RAMP_X, SCRATCH_Y, RAMP_W, SCRATCH_H, 0 );

    /* 13 qerase */
    label( 13 );
    prep_qerase();
    report( "qerase", timed( draw_qerase ), base );

    /* 14 noline */
    rect( RAMP_X, SCRATCH_Y, RAMP_W, SCRATCH_H, 0 );
    show( 14, draw_noline );
    outpw( RD_MASK, 0x00FF );
    fprintf( out, "  noline differs from flat in %ld pixels\n", box_check( 0 ) );
    report( "noline", timed( draw_noline ), base );
    rect( RAMP_X, SCRATCH_Y, RAMP_W, SCRATCH_H, 0 );
    noline_shapes();

    /* 15 core */
    rect( 0, 0, SCR_W, LABEL_Y, 0 );
    label( 15 );
    prep_expand();
    for( i = 0; i < NTRI; ++i ) {
        draw_core( i );
    }
    wait_idle();
    Sleep( HOLD_MS );
    us = timed( draw_core );
    end_expand();
    rect( 0, 0, SCR_W, LABEL_Y, 0 );
    prep_expand();
    draw_core( 0 );
    end_expand();
    fprintf( out, "  core differs from flat in %ld pixels\n", box_check( 0 ) );
    report( "core", us, base );
    rect( RAMP_X, SCRATCH_Y, RAMP_W, SCRATCH_H, 0 );

    /* 16 page */
    {
        double t0 = now();

        page_fill();
        fprintf( out, "size %3d  page fill, all %.1f us, CPU held %.1f us\n", size, (now() - t0) * 1e6 / qpf, page_cpu_us );
    }
    label( 16 );
    wait_idle();

    /* 18 stamp: every triangle is flat's colour for triangle 0, so the check is against that */
    rect( 0, 0, SCR_W, LABEL_Y, 0 );
    label( 18 );
    prep_stamp();
    us = timed( draw_stamp );
    end_stamp();
    rect( 0, 0, SCR_W, LABEL_Y, 0 );
    prep_stamp();
    draw_stamp( 0 );
    end_stamp();
    fprintf( out, "  stamp differs from flat in %ld pixels\n", box_check( 0 ) );
    report( "stamp", us, base );
    rect( RAMP_X, STAMP_Y, RAMP_W, size + 2, 0 );
    rect( RAMP_X, SCRATCH_Y, RAMP_W, SCRATCH_H, 0 );
    wait_idle();

    /* 17 corechk */
    if( s >= 24 ) {
        label( 17 );
        core_checks();
    }

    rect( RAMP_X, RAMP_Y, RAMP_W, RAMP_H + SCRATCH_H, 0 );
    wait_idle();
}

static LRESULT CALLBACK wndproc( HWND w, UINT m, WPARAM a, LPARAM b )
{
    return( DefWindowProc( w, m, a, b ) );
}

int PASCAL WinMain( HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show_cmd )
{
    WNDCLASS        wc;
    HWND            wnd;
    LPDIRECTDRAW    dd;
    LARGE_INTEGER   f;
    HRESULT         hr;
    HDC             dc;

    out = fopen( "C:\\M8SHAPE.TXT", "a" );
    if( out == NULL ) {
        return( 1 );
    }
    QueryPerformanceFrequency( &f );
    qpf = (double)f.LowPart;
    memset( &wc, 0, sizeof( wc ) );
    wc.lpfnWndProc = wndproc;
    wc.hInstance = inst;
    wc.lpszClassName = "M8SHAPE";
    RegisterClass( &wc );
    wnd = CreateWindowEx( WS_EX_TOPMOST, "M8SHAPE", "M8SHAPE", WS_POPUP | WS_VISIBLE, 0, 0, SCR_W, SCR_H,
                          NULL, NULL, inst, NULL );
    hr = DirectDrawCreate( NULL, &dd, NULL );
    fprintf( out, "== M8SHAPE %s  ddraw %08lx\n", cmd, (unsigned long)hr );
    fflush( out );
    if( hr != DD_OK ) {
        return( 1 );
    }
    dd->lpVtbl->SetCooperativeLevel( dd, wnd, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN );
    hr = dd->lpVtbl->SetDisplayMode( dd, SCR_W, SCR_H, 16 );
    fprintf( out, "setmode %08lx\n", (unsigned long)hr );
    fflush( out );
    if( hr != DD_OK ) {
        return( 1 );
    }
    ShowCursor( FALSE );
    Sleep( 1000 );

    palette_save();
    engine_setup();
    rect( 0, 0, SCR_W, SCR_H, 0 );
    rect( RAMP_X, RAMP_Y, RAMP_W, RAMP_H + SCRATCH_H, 0 );
    wait_idle();
    palette_ramp();
    one_size( 8 );
    one_size( 32 );
    one_size( 100 );
    fprintf( out, "waits that ran out: %d\n", lost );

    palette_restore();
    rect( 0, 0, SCR_W, SCR_H, 0 );
    wait_idle();
    ShowCursor( TRUE );
    dd->lpVtbl->RestoreDisplayMode( dd );
    dd->lpVtbl->SetCooperativeLevel( dd, wnd, DDSCL_NORMAL );
    dd->lpVtbl->Release( dd );
    dc = GetDC( NULL );
    if( dc != NULL ) {
        ExtEscape( dc, ESC_RESYNC, 0, NULL, 0, NULL );
        ReleaseDC( NULL, dc );
    }
    fprintf( out, "done\n\n" );
    fclose( out );
    DestroyWindow( wnd );
    return( 0 );
}
