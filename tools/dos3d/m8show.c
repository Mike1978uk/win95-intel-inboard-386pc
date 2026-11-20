/* M8SHOW.EXE - the DirectXT demo: one scene that shows what the Mach8 and m8dos do from DOS,
 * with the cost of each card lever on screen. The same scene is the yardstick for the Windows
 * side (worklist X36).
 *
 * The scene: a turning 16 x 16 checkered platform (one parity fill, L6 P5), shaded solids
 * standing and orbiting on it (hardware depth test, L6 P4: no sorting, fog by depth band in
 * the palette), a dithered sky (L6 P6) drawn into each page once and never cleared, and a
 * readout of fps and port writes a frame.
 *
 *   M8SHOW [frames] [nv] [grab N] [quads] [sort] [noshadow] [nohud] [nofloor] [nosolids] [noground]
 *     frames   frames to draw, then report (default 0: until Esc)
 *     nv       do not wait for the flip (throughput; tears)
 *     grab N   save the page shown after frame N to M8SHOW.RAW (640 x 480 palette indices, then
 *              the 768-byte palette, 6 bits a component;
 *              the grab takes seconds, so a run with it reports a lower fps)
 *     quads    start with the floor as 256 quads instead of one parity fill
 *     sort     start with painter's order instead of the depth test
 *     noshadow start without shadows (one polygon a solid, drawn by the mix)
 *     nohud    start without the readout
 *     nofloor, nosolids, noground   probes: leave that part out to price it
 *   Keys: D depth test / sort, P parity / quads, S shadows, H readout, V vsync, Esc quit.
 * Appends a line a run to M8SHOW.TXT.
 *
 * Build: see BUILD.BAT. GPL-3.0-or-later, as drivers/mach8_display.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "m8dos.h"

#define PI          3.14159265358979

/* Palette. Depth band z (0 nearest .. 13) holds the solids, fogged by z: entries z x 16 + c,
 * c 0-7 hue A shades, 8-15 hue B shades. Bands 14 and 15 hold what is always behind, so the
 * depth test passes every solid over them: band 15 the floor, ground, readout (F0-F5) and sky
 * (F8-FF); band 14 the same floor and ground in shadow. A shadow is then one polygon whose mix
 * clears bit 4 (~S & D, S = 10h) in whatever floor or ground is under it, at fill speed, and
 * shadows that overlap clear it once. */
#define SOLID_BANDS 14
#define BACK        0xF0
#define SHADOW_BIT  0x10
#define MIX_CLEAR   0x0E            /* ~S & D */
#define FLOOR_A     ( BACK + 0 )
#define FLOOR_B     ( BACK + 1 )
#define GROUND      ( BACK + 2 )
#define HILL        ( BACK + 3 )
#define HUD_FG      ( BACK + 4 )
#define HUD_BG      ( BACK + 5 )
#define SKY0        ( BACK + 8 )    /* F8..FF: sky, top to horizon */
#define SKY_N       8

#define SKY_B       112             /* sky rows 0..SKY_B-1: drawn into each page once */
#define CHK_N       16
#define EYE_H       12              /* eye above the floor, grid units */
#define VIEW_D      18              /* eye to the platform centre, grid units */
#define FOCAL       200
#define HORIZON     60
#define Z_NEAR      6
#define Z_FAR       30

/* The frame loop is 16.16 fixed point and makes no FPU call: the FPU here sits on the slow
 * bus and its work, not the card, set the frame rate. Floats run only at start (tables,
 * solids, palette) and in the report. Angles are 65536 a turn. */
typedef long FX;
#define FX_SH       16
#define I2FX( n )   ( (FX)(n) << FX_SH )

extern FX fmul( FX a, FX b );
#pragma aux fmul = "imul edx" "shrd eax, edx, 16" parm [eax] [edx] value [eax] modify [edx];
/* a x b / c with a 64-bit product */
extern long muldiv( long a, long b, long c );
#pragma aux muldiv = "imul edx" "idiv ebx" parm [eax] [edx] [ebx] value [eax] modify [edx];

#define SIN_N       4096            /* 0.09 degree a step: the platform edge moves under 1 px */
static FX       sin_t[SIN_N + SIN_N / 4];
#define SIN( a )    sin_t[( (unsigned long)(a) >> 4 ) & ( SIN_N - 1 )]
#define COS( a )    sin_t[( ( (unsigned long)(a) >> 4 ) & ( SIN_N - 1 ) ) + SIN_N / 4]

static FX to_fx( double v )
{
    return( (FX)floor( v * 65536.0 + 0.5 ) );
}

static unsigned long to_ang( double rad )
{
    return( (unsigned long)(long)floor( rad * 65536.0 / ( 2.0 * PI ) + 0.5 ) );
}

typedef struct { double x, y, z; } V3;

/* Solids: corners in object space (unit size) and faces as corner lists, counter-clockwise
 * seen from outside. */
static const V3 cube_v[8] = {
    { -1, 0, -1 }, { 1, 0, -1 }, { 1, 0, 1 }, { -1, 0, 1 },
    { -1, 2, -1 }, { 1, 2, -1 }, { 1, 2, 1 }, { -1, 2, 1 } };
static const int cube_f[6][5] = {
    { 4, 0, 1, 2, 3 }, { 4, 7, 6, 5, 4 }, { 4, 0, 4, 5, 1 },
    { 4, 1, 5, 6, 2 }, { 4, 2, 6, 7, 3 }, { 4, 3, 7, 4, 0 } };
static const V3 pyr_v[5] = {
    { -1, 0, -1 }, { 1, 0, -1 }, { 1, 0, 1 }, { -1, 0, 1 }, { 0, 2.4, 0 } };
static const int pyr_f[5][5] = {
    { 4, 0, 1, 2, 3 }, { 3, 0, 4, 1, 0 }, { 3, 1, 4, 2, 0 }, { 3, 2, 4, 3, 0 }, { 3, 3, 4, 0, 0 } };
static const V3 oct_v[6] = {
    { 0, 0, 0 }, { 1.2, 1.2, 0 }, { 0, 1.2, 1.2 }, { -1.2, 1.2, 0 }, { 0, 1.2, -1.2 }, { 0, 2.4, 0 } };
static const int oct_f[8][5] = {
    { 3, 0, 2, 1, 0 }, { 3, 0, 3, 2, 0 }, { 3, 0, 4, 3, 0 }, { 3, 0, 1, 4, 0 },
    { 3, 5, 1, 2, 0 }, { 3, 5, 2, 3, 0 }, { 3, 5, 3, 4, 0 }, { 3, 5, 4, 1, 0 } };

typedef struct {
    const V3    *v;
    int         nv;
    const int   (*f)[5];
    int         nf;
    int         hue;            /* 0: shades 0-7, 1: shades 8-15 */
    double      u, w;           /* position on the platform, grid units from the centre */
    double      orbit;          /* radians a frame about the centre, 0 standing */
    double      spin;           /* radians a frame about its own axis */
    double      size;
    double      bob;            /* height of a bounce, 0 none */
} SOLID;

static SOLID    solids[] = {
    { cube_v, 8, cube_f, 6, 0,  0.0,  0.0, 0.0,   0.030, 1.6, 0.0 },
    { pyr_v,  5, pyr_f,  5, 1,  4.5,  0.0, 0.012, 0.050, 1.1, 0.0 },
    { oct_v,  6, oct_f,  8, 0, -4.5,  0.0, 0.012, -0.040, 1.0, 1.5 },
    { cube_v, 8, cube_f, 6, 1,  0.0,  5.0, 0.012, 0.045, 0.8, 0.0 },
    { pyr_v,  5, pyr_f,  5, 0,  0.0, -5.0, 0.012, -0.035, 0.9, 0.0 },
    { oct_v,  6, oct_f,  8, 1,  6.0,  6.0, -0.008, 0.060, 0.8, 2.0 },
};
#define NSOLID  ( sizeof( solids ) / sizeof( solids[0] ) )

/* The solids in fixed point: corners scaled by size, and each face's unit outward normal in
 * object space. The solids turn only about the vertical, so a face's normal is its object
 * normal turned by the spin: no cross product or square root a frame. */
typedef struct {
    FX              v[8][3];
    FX              n[8][3];
    FX              u, w, bob;
    unsigned long   orbit, spin;    /* angle a frame */
} SOLIDFX;
static SOLIDFX  sfx[NSOLID];
static FX       light_fx[3];
static unsigned long bob_step;
static unsigned long one_rad;
static FX       shadow_ku;          /* floor offset a unit of height, away from the light */
static FX       shadow_kw;
static M8PT     shad[NSOLID][2 * 8 + 1];    /* hull() builds in up to 2n + 1 */
static int      nshad[NSOLID];

/* Faces of a frame, for painter's order when the depth test is off. */
typedef struct {
    M8PT    p[4];
    int     n;
    int     col;
    int     z;
    FX      depth;
} FACE;
static FACE     faces[NSOLID * 8];
static int      nfaces;

static short    gx[CHK_N + 1][CHK_N + 1];
static short    gy[CHK_N + 1][CHK_N + 1];
static FX       view_c;
static FX       view_s;

static int      use_depth = 1;
static int      use_parity = 1;
static int      use_shadows = 1;
static int      show_hud = 1;
static int      vsync = 1;
static int      skip;               /* probe: 1 floor, 2 solids, 4 ground repaint */
static int      sky_pages;          /* pages that hold the sky: 2 = both */

/* What a page holds besides sky and ground: the box of the platform and solids drawn into it,
 * so the next frame on that page repaints only that box, not the whole ground (the engine,
 * not the bus, is the limit here). full > 0: frames that repaint everything. */
typedef struct { int l, t, r, b; } BOX;
static BOX      drawn[2];
static int      full = 2;

static void box_add( BOX *b, int x, int y )
{
    if( x < b->l ) b->l = x;
    if( x > b->r ) b->r = x;
    if( y < b->t ) b->t = y;
    if( y > b->b ) b->b = y;
}

/* Grid units on the platform (u across, w along, h up), turned by the view, to the screen,
 * rounded to the pixel (8 fraction bits from the divide). */
static void project( FX u, FX w, FX h, short *sx, short *sy, FX *wz )
{
    FX x = fmul( u, view_c ) - fmul( w, view_s );
    FX z = I2FX( VIEW_D ) + fmul( u, view_s ) + fmul( w, view_c );

    if( z < I2FX( 1 ) ) {
        z = I2FX( 1 );              /* nothing comes this close; keeps the divide in range */
    }
    *wz = z;
    *sx = (short)( ( muldiv( x, FOCAL << 8, z ) + ( 320 << 8 ) + 128 ) >> 8 );
    *sy = (short)( ( muldiv( I2FX( EYE_H ) - h, FOCAL << 8, z ) + ( HORIZON << 8 ) + 128 ) >> 8 );
}

/* Tables and the fixed-point solids, once at start. */
static void fx_init( void )
{
    static const double light[3] = { -0.77, 0.60, 0.22 };     /* low and to the side: long shadows */
    unsigned            k;
    int                 i;

    for( i = 0; i < SIN_N + SIN_N / 4; ++i ) {
        sin_t[i] = to_fx( sin( i * 2.0 * PI / SIN_N ) );
    }
    for( i = 0; i < 3; ++i ) {
        light_fx[i] = to_fx( light[i] );
    }
    shadow_ku = to_fx( -light[0] / light[1] );
    shadow_kw = to_fx( -light[2] / light[1] );
    bob_step = to_ang( 0.06 );
    one_rad = to_ang( 1.0 );
    for( k = 0; k < NSOLID; ++k ) {
        SOLID   *o = &solids[k];
        SOLIDFX *x = &sfx[k];

        for( i = 0; i < o->nv; ++i ) {
            x->v[i][0] = to_fx( o->v[i].x * o->size );
            x->v[i][1] = to_fx( o->v[i].y * o->size );
            x->v[i][2] = to_fx( o->v[i].z * o->size );
        }
        for( i = 0; i < o->nf; ++i ) {      /* (v2 - v1) x (v3 - v1): outward, as wound */
            const V3    *a = &o->v[o->f[i][1]];
            const V3    *b = &o->v[o->f[i][2]];
            const V3    *c = &o->v[o->f[i][3]];
            double      ex1 = b->x - a->x, ey1 = b->y - a->y, ez1 = b->z - a->z;
            double      ex2 = c->x - a->x, ey2 = c->y - a->y, ez2 = c->z - a->z;
            double      nx = ey1 * ez2 - ez1 * ey2;
            double      ny = ez1 * ex2 - ex1 * ez2;
            double      nz = ex1 * ey2 - ey1 * ex2;
            double      len = sqrt( nx * nx + ny * ny + nz * nz );

            x->n[i][0] = to_fx( nx / len );
            x->n[i][1] = to_fx( ny / len );
            x->n[i][2] = to_fx( nz / len );
        }
        x->u = to_fx( o->u );
        x->w = to_fx( o->w );
        x->bob = to_fx( o->bob );
        x->orbit = to_ang( o->orbit );
        x->spin = to_ang( o->spin );
    }
}

static unsigned char    rgb[256 * 3];  /* also saved after a grab's pixels */

static void build_palette( void )
{
    static const double     hue[2][3] = { { 1.00, 0.55, 0.15 }, { 0.25, 0.65, 1.00 } };
    static const double     fog[3] = { 0.55, 0.62, 0.80 };
    int                     z;
    int                     c;
    int                     k;

    for( z = 0; z < SOLID_BANDS; ++z ) {
        double f = 0.75 * z / ( SOLID_BANDS - 1 );      /* fog share */

        for( c = 0; c < 16; ++c ) {
            double lum = 0.25 + 0.75 * ( c & 7 ) / 7.0;

            for( k = 0; k < 3; ++k ) {
                double v = hue[c >> 3][k] * lum * ( 1.0 - f ) + fog[k] * f;

                rgb[( z * 16 + c ) * 3 + k] = (unsigned char)( v * 63.0 + 0.5 );
            }
        }
    }
    for( c = 0; c < SKY_N; ++c ) {                      /* deep blue down to the fog colour */
        double t = c / (double)( SKY_N - 1 );

        rgb[( SKY0 + c ) * 3 + 0] = (unsigned char)( ( 0.08 + t * ( fog[0] - 0.08 ) ) * 63.0 + 0.5 );
        rgb[( SKY0 + c ) * 3 + 1] = (unsigned char)( ( 0.12 + t * ( fog[1] - 0.12 ) ) * 63.0 + 0.5 );
        rgb[( SKY0 + c ) * 3 + 2] = (unsigned char)( ( 0.35 + t * ( fog[2] - 0.35 ) ) * 63.0 + 0.5 );
    }
#define SETC( i, r, g, b ) ( rgb[(i) * 3] = (r), rgb[(i) * 3 + 1] = (g), rgb[(i) * 3 + 2] = (b) )
    SETC( FLOOR_A, 58, 58, 54 );
    SETC( FLOOR_B, 14, 18, 30 );
    SETC( GROUND, 10, 20, 12 );
    SETC( HILL, 18, 24, 34 );
    SETC( HUD_FG, 63, 63, 20 );
    SETC( HUD_BG, 0, 0, 0 );
    for( c = FLOOR_A; c <= HILL; ++c ) {               /* in shadow: the same, darker */
        for( k = 0; k < 3; ++k ) {
            rgb[( c & ~SHADOW_BIT ) * 3 + k] = (unsigned char)( rgb[c * 3 + k] * 9 / 20 );
        }
    }
    m8_palette( 0, 256, rgb );
}

/* The sky: a ramp of SKY_N colours with 3 dither steps between each, and a hill line. Drawn
 * into a page once; the rows below SKY_B are redrawn every frame and solids stay below it. */
static void draw_sky( void )
{
    int     strips = ( SKY_N - 1 ) * 3 + 1;
    int     i;
    M8PT    h[4];

    for( i = 0; i < strips; ++i ) {
        int t = i * SKY_B / strips;
        int b = ( i + 1 ) * SKY_B / strips - 1;

        m8_dither_rect( 0, t, M8_W - 1, b, SKY0 + i / 3, SKY0 + i / 3 + 1, ( i % 3 ) * 3 );
    }
    for( i = 0; i < 8; ++i ) {                          /* low hills on the horizon */
        int x0 = i * 80;

        h[0].x = (short)x0;        h[0].y = SKY_B - 1;
        h[1].x = (short)( x0 + 28 + ( i * 37 ) % 30 ); h[1].y = (short)( SKY_B - 22 - ( i * 53 ) % 30 );
        h[2].x = (short)( x0 + 79 ); h[2].y = SKY_B - 1;
        m8_poly( 3, h, HILL );
    }
}

static void grid_pt( int i, int j )
{
    FX      wz;

    project( I2FX( i - CHK_N / 2 ), I2FX( j - CHK_N / 2 ), 0, &gx[i][j], &gy[i][j], &wz );
}

/* The parity floor uses only the platform's edge points; the quads need every corner. */
static void floor_grid( void )
{
    int     i;
    int     j;

    if( use_parity ) {
        for( i = 0; i <= CHK_N; ++i ) {
            grid_pt( i, 0 );
            grid_pt( i, CHK_N );
            grid_pt( 0, i );
            grid_pt( CHK_N, i );
        }
        return;
    }
    for( i = 0; i <= CHK_N; ++i ) {
        for( j = 0; j <= CHK_N; ++j ) {
            grid_pt( i, j );
        }
    }
}

static void floor_quads( void )
{
    int     i;
    int     j;
    M8PT    q[4];

    for( i = 0; i < CHK_N; ++i ) {
        for( j = 0; j < CHK_N; ++j ) {
            q[0].x = gx[i][j];         q[0].y = gy[i][j];
            q[1].x = gx[i + 1][j];     q[1].y = gy[i + 1][j];
            q[2].x = gx[i + 1][j + 1]; q[2].y = gy[i + 1][j + 1];
            q[3].x = gx[i][j + 1];     q[3].y = gy[i][j + 1];
            m8_poly( 4, q, ( i + j ) & 1 ? FLOOR_A : FLOOR_B );
        }
    }
}

static void seg( M8SEG *s, int *n, int i0, int j0, int i1, int j1 )
{
    s[*n].x0 = gx[i0][j0];
    s[*n].y0 = gy[i0][j0];
    s[*n].x1 = gx[i1][j1];
    s[*n].y1 = gy[i1][j1];
    s[*n].planes = 1;
    ++*n;
}

/* As M8DTEST's checkprobe: the platform as one quad of colour B, then colour A by parity of
 * the interior lines and the boundary pieces whose half-plane count is odd. */
static void floor_parity( void )
{
    static M8SEG    s[6 * CHK_N];
    static const int ca = FLOOR_A;
    M8PT            q[4];
    int             n = 0;
    int             k;

    q[0].x = gx[0][0];         q[0].y = gy[0][0];
    q[1].x = gx[CHK_N][0];     q[1].y = gy[CHK_N][0];
    q[2].x = gx[CHK_N][CHK_N]; q[2].y = gy[CHK_N][CHK_N];
    q[3].x = gx[0][CHK_N];     q[3].y = gy[0][CHK_N];
    m8_poly( 4, q, FLOOR_B );
    for( k = 1; k < CHK_N; ++k ) {
        seg( s, &n, k, 0, k, CHK_N );
        seg( s, &n, 0, k, CHK_N, k );
    }
    for( k = 0; k < CHK_N; ++k ) {
        if( k & 1 ) {
            seg( s, &n, 0, k, 0, k + 1 );
            seg( s, &n, k, 0, k + 1, 0 );
        } else {
            seg( s, &n, CHK_N, k, CHK_N, k + 1 );
            seg( s, &n, k, CHK_N, k + 1, CHK_N );
        }
    }
    m8_parity( n, s, 1, &ca );
}

/* The convex hull of n screen points (monotone chain), into h; returns its corner count. A
 * solid's shadow is the hull of its corners cast to the floor, as the floor's image of a
 * convex solid is convex. */
static int hull( int n, M8PT *p, M8PT *h )
{
    int     i;
    int     j;
    int     m = 0;
    int     lo;

    for( i = 1; i < n; ++i ) {                          /* by x, then y */
        M8PT t = p[i];

        for( j = i; j > 0 && ( p[j - 1].x > t.x || ( p[j - 1].x == t.x && p[j - 1].y > t.y ) ); --j ) {
            p[j] = p[j - 1];
        }
        p[j] = t;
    }
#define TURN( a, b, c ) ( (long)( (b).x - (a).x ) * ( (c).y - (a).y ) - (long)( (b).y - (a).y ) * ( (c).x - (a).x ) )
    for( i = 0; i < n; ++i ) {
        while( m >= 2 && TURN( h[m - 2], h[m - 1], p[i] ) <= 0 ) --m;
        h[m++] = p[i];
    }
    for( i = n - 2, lo = m + 1; i >= 0; --i ) {
        while( m >= lo && TURN( h[m - 2], h[m - 1], p[i] ) <= 0 ) --m;
        h[m++] = p[i];
    }
#undef TURN
    return( m - 1 );
}

/* Each solid: turned, placed, back faces dropped, shaded by a low light from the left,
 * depth band from the face's mean distance; and its shadow. */
static void build_solids( unsigned long f )
{
    unsigned        k;
    FX              eye_u = -VIEW_D * view_s;   /* the eye on the platform: project()'s x = 0, z = 0 */
    FX              eye_w = -VIEW_D * view_c;

    nfaces = 0;
    for( k = 0; k < NSOLID; ++k ) {
        SOLID   *o = &solids[k];
        SOLIDFX *x = &sfx[k];
        FX      ca = COS( x->orbit * f );
        FX      sa = SIN( x->orbit * f );
        FX      u = fmul( x->u, ca ) - fmul( x->w, sa );
        FX      w = fmul( x->u, sa ) + fmul( x->w, ca );
        FX      cr = COS( x->spin * f );
        FX      sr = SIN( x->spin * f );
        FX      lift = x->bob > 0 ? fmul( x->bob, labs( SIN( f * bob_step + k * one_rad ) ) ) : 0;
        FX      wx[8], wy[8], wz[8], pz[8];
        short   px[8], py[8];
        int     i;

        for( i = 0; i < o->nv; ++i ) {
            FX vx = x->v[i][0];
            FX vz = x->v[i][2];

            wx[i] = u + fmul( vx, cr ) - fmul( vz, sr );
            wz[i] = w + fmul( vx, sr ) + fmul( vz, cr );
            wy[i] = x->v[i][1] + lift;
            project( wx[i], wz[i], wy[i], &px[i], &py[i], &pz[i] );
        }
        if( use_shadows ) {
            M8PT    g[8];
            FX      gz;

            for( i = 0; i < o->nv; ++i ) {      /* each corner along the light to the floor */
                FX gu = wx[i] + fmul( wy[i], shadow_ku );
                FX gw = wz[i] + fmul( wy[i], shadow_kw );

                project( gu, gw, 0, &g[i].x, &g[i].y, &gz );
            }
            nshad[k] = hull( o->nv, g, shad[k] );
        }
        for( i = 0; i < o->nf; ++i ) {
            const int   *fc = o->f[i];
            int         n = fc[0];
            int         v1 = fc[1];
            FX          nx = fmul( x->n[i][0], cr ) - fmul( x->n[i][2], sr );
            FX          ny = x->n[i][1];
            FX          nz = fmul( x->n[i][0], sr ) + fmul( x->n[i][2], cr );
            FX          lit;
            FX          depth = 0;
            FACE        *fa;
            int         j;
            int         z;
            int         shade;

            /* A face shows when the eye is on its outer side. The light is fixed to the platform. */
            if( fmul( eye_u - wx[v1], nx ) + fmul( I2FX( EYE_H ) - wy[v1], ny ) + fmul( eye_w - wz[v1], nz ) <= 0 ) {
                continue;
            }
            lit = fmul( nx, light_fx[0] ) + fmul( ny, light_fx[1] ) + fmul( nz, light_fx[2] );
            if( lit < 0 ) lit = 0;
            fa = &faces[nfaces++];
            fa->n = n;
            for( j = 0; j < n; ++j ) {
                fa->p[j].x = px[fc[1 + j]];
                fa->p[j].y = py[fc[1 + j]];
                depth += pz[fc[1 + j]];
            }
            depth /= n;
            z = (int)( ( ( depth - I2FX( Z_NEAR ) ) * ( SOLID_BANDS - 1 ) / ( Z_FAR - Z_NEAR ) ) >> FX_SH );
            if( z < 0 ) z = 0;
            if( z > SOLID_BANDS - 1 ) z = SOLID_BANDS - 1;
            shade = (int)( ( lit * 7 + 0x8000 ) >> FX_SH );
            if( shade > 7 ) shade = 7;
            fa->z = z;
            fa->depth = depth;
            fa->col = z * 16 + o->hue * 8 + shade;
        }
    }
}

/* The next key from the BIOS buffer, read directly (DOS/32A maps the first megabyte flat), so
 * the loop makes no DOS call: ASCII, or the scan code + 0x100 for keys without one. 0 none. */
static int key_next( void )
{
    volatile unsigned short *head = (volatile unsigned short *)0x41A;
    volatile unsigned short *tail = (volatile unsigned short *)0x41C;
    unsigned short          h = *head;
    unsigned short          c;

    if( h == *tail ) {
        return( 0 );
    }
    c = *(volatile unsigned short *)( 0x400 + h );
    h += 2;
    if( h >= *(volatile unsigned short *)0x482 ) {
        h = *(volatile unsigned short *)0x480;
    }
    *head = h;
    return( ( c & 0xFF ) ? ( c & 0xFF ) : ( ( c >> 8 ) | 0x100 ) );
}

static int cmp_far_first( const void *a, const void *b )
{
    FX da = ( (const FACE *)a )->depth;
    FX db = ( (const FACE *)b )->depth;

    return( db > da ? 1 : db < da ? -1 : 0 );
}

static void draw_shadows( void )
{
    unsigned k;

    m8_mix( MIX_CLEAR );
    for( k = 0; k < NSOLID; ++k ) {
        if( nshad[k] >= 3 ) {
            m8_poly( nshad[k], shad[k], SHADOW_BIT );
        }
    }
    m8_mix( M8_MIX_POLY_REPLACE );
}

static void draw_solids( void )
{
    int i;

    if( !use_depth ) {
        qsort( faces, nfaces, sizeof( FACE ), cmp_far_first );
    }
    for( i = 0; i < nfaces; ++i ) {
        m8_depth( use_depth ? faces[i].z : -1 );
        m8_poly( faces[i].n, faces[i].p, faces[i].col );
    }
    m8_depth( -1 );
}

/* The readout: seven-segment digits, one rectangle a lit segment. */
static void digit( int x, int y, int d )
{
    static const unsigned char  segs[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };
    unsigned                    m = segs[d];

    if( m & 0x01 ) m8_rect( x + 1, y,      x + 6, y + 1,  HUD_FG );
    if( m & 0x02 ) m8_rect( x + 6, y + 1,  x + 7, y + 6,  HUD_FG );
    if( m & 0x04 ) m8_rect( x + 6, y + 7,  x + 7, y + 12, HUD_FG );
    if( m & 0x08 ) m8_rect( x + 1, y + 12, x + 6, y + 13, HUD_FG );
    if( m & 0x10 ) m8_rect( x,     y + 7,  x + 1, y + 12, HUD_FG );
    if( m & 0x20 ) m8_rect( x,     y + 1,  x + 1, y + 6,  HUD_FG );
    if( m & 0x40 ) m8_rect( x + 1, y + 6,  x + 6, y + 7,  HUD_FG );
}

static void number( int x, int y, unsigned long v, int digits )
{
    int i;

    for( i = digits - 1; i >= 0; --i ) {
        digit( x + i * 10, y, (int)( v % 10 ) );
        v /= 10;
    }
}

/* Readout: fps x 10, writes a frame, and a lamp a lever (lit = on): depth, parity, vsync,
 * shadows. */
static void draw_hud( unsigned long fps10, unsigned long writes )
{
    int y = M8_H - 20;

    m8_rect( 4, y - 2, 4 + 8 * 10 + 84, y + 15, HUD_BG );
    number( 8, y, fps10 / 10, 3 );
    m8_rect( 38, y + 12, 39, y + 13, HUD_FG );
    digit( 42, y, (int)( fps10 % 10 ) );
    number( 58, y, writes > 9999 ? 9999 : writes, 4 );
    if( use_depth ) m8_rect( 104, y, 113, y + 4, HUD_FG );
    if( use_parity ) m8_rect( 118, y, 127, y + 4, HUD_FG );
    if( vsync ) m8_rect( 132, y, 141, y + 4, HUD_FG );
    if( use_shadows ) m8_rect( 146, y, 155, y + 4, HUD_FG );
}

static void grab( const char *raw )
{
    unsigned char   *buf = (unsigned char *)malloc( (size_t)M8_W * M8_H );
    FILE            *f;

    if( buf == NULL || m8_read_shown( buf ) ) {
        free( buf );
        return;
    }
    if( ( f = fopen( raw, "wb" ) ) != NULL ) {
        fwrite( buf, 1, (size_t)M8_W * M8_H, f );
        fwrite( rgb, 1, sizeof( rgb ), f );
        fclose( f );
    }
    free( buf );
}

int main( int argc, char **argv )
{
    long            frames = 0;
    long            grab_at = -1;
    long            f;
    unsigned long   t_start;
    unsigned long   t_sec;
    unsigned long   f_sec = 0;
    unsigned long   fps10 = 0;
    unsigned long   w_frame = 0;
    unsigned long   w_prev = 0;
    unsigned long   view_step;
    M8STATS         s;
    FILE            *o;
    int             i;
    int             key;

    for( i = 1; i < argc; ++i ) {
        if( stricmp( argv[i], "nv" ) == 0 ) vsync = 0;
        else if( stricmp( argv[i], "quads" ) == 0 ) use_parity = 0;
        else if( stricmp( argv[i], "sort" ) == 0 ) use_depth = 0;
        else if( stricmp( argv[i], "noshadow" ) == 0 ) use_shadows = 0;
        else if( stricmp( argv[i], "nohud" ) == 0 ) show_hud = 0;
        else if( stricmp( argv[i], "grab" ) == 0 && i + 1 < argc ) grab_at = atol( argv[++i] );
        else if( stricmp( argv[i], "nofloor" ) == 0 ) skip |= 1;
        else if( stricmp( argv[i], "nosolids" ) == 0 ) skip |= 2;
        else if( stricmp( argv[i], "noground" ) == 0 ) skip |= 4;
        else frames = atol( argv[i] );
    }
    if( m8_open() ) {
        printf( "M8SHOW: no Mach8 engine (%s)\n", m8_error() ? m8_error() : "open failed" );
        return( 1 );
    }
    build_palette();
    m8_dither_init();                   /* without it the sky is bands, not ramps */
    fx_init();
    view_step = to_ang( 0.012 );
    m8_stats_reset();
    t_start = t_sec = m8_ticks();
    for( f = 0; frames == 0 || f < frames; ++f ) {
        view_c = COS( view_step * f );
        view_s = SIN( view_step * f );
        floor_grid();
        build_solids( f );
        m8_frame_begin();
        if( sky_pages < 2 ) {
            draw_sky();
            ++sky_pages;
        }
        {
            BOX     now = { M8_W, M8_H, -1, -1 };
            BOX     *old = &drawn[f & 1];
            int     j;

            for( i = 0; i <= CHK_N; i += CHK_N ) {
                for( j = 0; j <= CHK_N; j += CHK_N ) {
                    box_add( &now, gx[i][j], gy[i][j] );
                }
            }
            for( i = 0; i < nfaces; ++i ) {
                for( j = 0; j < faces[i].n; ++j ) {
                    box_add( &now, faces[i].p[j].x, faces[i].p[j].y );
                }
            }
            for( i = 0; use_shadows && i < (int)NSOLID; ++i ) {
                for( j = 0; j < nshad[i]; ++j ) {
                    box_add( &now, shad[i][j].x, shad[i][j].y );
                }
            }
            if( full > 0 ) {
                m8_rect( 0, SKY_B, M8_W - 1, M8_H - 1, GROUND );
                --full;
            } else if( old->r >= old->l && !( skip & 4 ) ) {
                m8_rect( old->l, old->t, old->r, old->b, GROUND );
            }
            now.l = now.l - 1 < 0 ? 0 : now.l - 1;
            now.t = now.t - 1 < SKY_B ? SKY_B : now.t - 1;
            now.r = now.r + 1 > M8_W - 1 ? M8_W - 1 : now.r + 1;
            now.b = now.b + 1 > M8_H - 1 ? M8_H - 1 : now.b + 1;
            *old = now;
        }
        if( !( skip & 1 ) ) { if( use_parity ) floor_parity(); else floor_quads(); }
        if( use_shadows && !( skip & 2 ) ) draw_shadows();
        if( !( skip & 2 ) ) draw_solids();
        if( show_hud ) draw_hud( fps10, w_frame );
        m8_frame_end( vsync );
        if( f == grab_at ) {
            grab( "M8SHOW.RAW" );
        }
        m8_stats( &s );
        ++f_sec;
        if( m8_ticks() - t_sec >= 18 ) {
            unsigned long dt = m8_ticks() - t_sec;

            fps10 = f_sec * 182065ul / ( dt * 1000ul );
            w_frame = ( s.outs - w_prev ) / f_sec;
            w_prev = s.outs;
            f_sec = 0;
            t_sec = m8_ticks();
        }
        while( ( key = key_next() ) != 0 ) {
            switch( key < 0x100 ? ( key | 0x20 ) : 0 ) {
            case 'd': use_depth = !use_depth; break;
            case 'p': use_parity = !use_parity; break;
            case 's': use_shadows = !use_shadows; break;
            case 'h': show_hud = !show_hud; full = 2; break;
            case 'v': vsync = !vsync; break;
            default:
                if( key == 27 ) frames = f + 1;
                break;
            }
        }
        if( m8_error() != NULL ) {
            break;
        }
    }
    m8_stats( &s );
    m8_close();
    {
        unsigned long   ticks = m8_ticks() - t_start;
        double          secs = ticks / 18.2065;
        char            line[200];

        sprintf( line, "M8SHOW %ld frames in %.2f s = %.2f fps; writes %lu (%.1f a frame), status reads %lu;"
                 " depth %d parity %d shadows %d hud %d vsync %d skip %d%s%s\n", f, secs, secs > 0 ? f / secs : 0.0,
                 s.outs, f ? (double)s.outs / f : 0.0, s.ins, use_depth, use_parity, use_shadows, show_hud, vsync, skip,
                 m8_error() ? "; engine: " : "", m8_error() ? m8_error() : "" );
        fputs( line, stdout );
        if( ( o = fopen( "M8SHOW.TXT", "a" ) ) != NULL ) {
            fputs( line, o );
            fclose( o );
        }
    }
    return( 0 );
}
