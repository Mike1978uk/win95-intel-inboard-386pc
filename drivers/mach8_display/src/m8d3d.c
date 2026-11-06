/* Direct3D HAL in M8DD32.DLL: flat shading at 8 bpp (docs/mach8_frame_rate_plan.md,
 * Phase 3).
 *
 * Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
 *
 * DirectDraw reads two tables from the HAL information: the device's caps
 * (D3DHAL_GLOBALDRIVERDATA) and its entry points (D3DHAL_CALLBACKS). With
 * Execute left out and D3DDEVCAPS_EXECUTESYSTEMMEMORY set, the runtime walks
 * execute buffers itself and calls RenderState and RenderPrimitive; those two
 * and ContextCreate are the ones it requires. Layouts as the DX5 DDK's
 * d3dhal.h, checked by size below.
 *
 * What is claimed is what the card does: RGB colour, flat shading, an 8 bpp
 * target, no Z buffer, no textures. A triangle is a polygon fill: its outline
 * in one bit-plane of a scratch area beside the target, the engine fills between
 * those pixels, and the outline goes over the fill (m8hw.c m8_hw_poly, the
 * sequence proven on the card). One clear of the scratch serves eight triangles.
 *
 * At 16 bpp the card still holds 256 colours and RAM holds the 5-6-5 frame: a
 * triangle joins its buffer's description (m8dd32.c) and Flip has the engine draw
 * it, so a frame of them crosses the bus as commands. Only a flip chain's back
 * buffer can be a target.
 *
 * DirectX 7 removes 8 bpp from a hardware device's render depths (D3DIM.DLL
 * 56663de9), so an 8 bpp target needs a patched D3DIM: docs/next_session_2026_11_05.md.
 */

#include <windows.h>
#include <ddraw.h>
#include <conio.h>
#include <string.h>

#define DDHAL_DRIVER_HANDLED    1

/* m8.h */
#define M8_WRT_MASK         0xAAE8
#define M8_RD_MASK          0xAEE8
#define M8_FRGD_COLOR       0xA6E8
#define M8_CUR_Y            0x82E8
#define M8_SCAN_TO_X        0xCAEE      /* draws CUR_X up to (not including) it on CUR_Y */
#define M8_CUR_X            0x86E8
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
#define M8_DP_POLY_FILL     0x2213
#define M8_DP_MONO_VRAM     0x2271      /* FRGD_COLOR where the blit source's RD_MASK plane is set */
#define M8_ALU_BG_FN        0xB6EE
#define M8_MAJ_AXIS_PCNT    0x96E8
#define M8_MIN_AXIS         0xBEE8
#define M8_CMD              0x9AE8
#define M8_CMD_FILL         0x40F3
#define M8_DP_VRAM_BLIT     0x6211
#define M8_SRC_CMP_FN       0x92EE
#define M8_SRC_CMP_COLOR    0xEAEE
#define M8_CMP_UNEQUAL      0x0028
#define M8_MIX_LEAVE        0x0003
#define M8_MIX_XOR          0x0005
#define M8_MIX_REPLACE      0x0007

/* d3dtypes.h */
#define M8D3D_OP_TRIANGLE   3
#define M8D3D_RS_SHADEMODE  9
#define M8D3D_SHADE_GOURAUD 2
#define M8D3D_RS_CULLMODE   22
#define M8D3D_CULL_NONE     1
#define M8D3D_CULL_CW       2
#define M8D3D_CULL_CCW      3

/* DDRAWI_DDRAWSURFACE_INT holds its local part second; ddrawi.h, as m8dd32.c. */
#define M8D3D_INT_LCL       4
#define M8D3D_LCL_GBL       4
#define M8D3D_GBL_VIDMEM    20

/* m8dd32.c: the engine and the card's memory, which it owns. */
extern int  m8d3d_surface( LPVOID lcl, int *x, int *y, int *w, int *h );
extern int  m8d3d_scratch( LPVOID key, int *w, int *h, int *x, int *y );
extern int  m8d3d_fb16( void );
extern int  m8d3d_fill( const RECT *r );
extern int  m8d3d_record( LPVOID lcl, const short *v, BYTE index, WORD colour, const DWORD *rgb );
extern void m8d3d_scratch_free( LPVOID key );
extern int  m8d3d_engine_begin( void );
extern int  m8d3d_clear( int l, int t, int r, int b );
extern void m8d3d_engine_end( void );
extern int  m8d3d_room( int n );

/* To the nearest integer as the FPU rounds, with no library call. */
extern long m8d3d_round( float f );
#pragma aux m8d3d_round = "sub esp, 4" "fistp dword ptr [esp]" "pop eax" \
    parm [8087] value [eax] modify exact [eax];

/* d3dcaps.h and d3dtypes.h, DX5. */
#define M8D3D_DD_COLORMODEL         0x00000001UL
#define M8D3D_DD_DEVCAPS            0x00000002UL
#define M8D3D_DD_TRICAPS            0x00000040UL
#define M8D3D_DD_RENDERBITDEPTH     0x00000080UL
#define M8D3D_DD_ZBUFFERBITDEPTH    0x00000100UL
#define M8D3D_COLOR_RGB             2UL
#define M8D3D_DEVCAPS_FLOATTLVERTEX         0x00000001UL
#define M8D3D_DEVCAPS_EXECUTESYSTEMMEMORY   0x00000010UL
#define M8D3D_DEVCAPS_TLVERTEXSYSTEMMEMORY  0x00000040UL
#define M8D3D_DEVCAPS_DRAWPRIMTLVERTEX      0x00000400UL
#define M8D3D_PMISCCAPS_CULLNONE    0x00000010UL
#define M8D3D_PSHADECAPS_COLORFLATRGB 0x00000002UL
#define M8D3D_PSHADECAPS_COLORGOURAUDRGB 0x00000008UL

#pragma pack( push, 4 )
typedef struct {
    DWORD   dwSize;
    DWORD   dwMiscCaps;
    DWORD   dwRasterCaps;
    DWORD   dwZCmpCaps;
    DWORD   dwSrcBlendCaps;
    DWORD   dwDestBlendCaps;
    DWORD   dwAlphaCmpCaps;
    DWORD   dwShadeCaps;
    DWORD   dwTextureCaps;
    DWORD   dwTextureFilterCaps;
    DWORD   dwTextureBlendCaps;
    DWORD   dwTextureAddressCaps;
    DWORD   dwStippleWidth;
    DWORD   dwStippleHeight;
} M8D3D_PRIMCAPS;

/* D3DDEVICEDESC_V1. */
typedef struct {
    DWORD           dwSize;
    DWORD           dwFlags;
    DWORD           dcmColorModel;
    DWORD           dwDevCaps;
    DWORD           dtcTransformCaps[2];
    DWORD           bClipping;
    DWORD           dlcLightingCaps[4];
    M8D3D_PRIMCAPS  dpcLineCaps;
    M8D3D_PRIMCAPS  dpcTriCaps;
    DWORD           dwDeviceRenderBitDepth;
    DWORD           dwDeviceZBufferBitDepth;
    DWORD           dwMaxBufferSize;
    DWORD           dwMaxVertexCount;
} M8D3D_DEVICEDESC;

typedef struct {
    DWORD               dwSize;
    M8D3D_DEVICEDESC    hwCaps;
    DWORD               dwNumVertices;
    DWORD               dwNumClipVertices;
    DWORD               dwNumTextureFormats;
    LPVOID              lpTextureFormats;
} M8D3D_GLOBAL;

typedef struct {
    DWORD   dwSize;
    LPVOID  ContextCreate;
    LPVOID  ContextDestroy;
    LPVOID  ContextDestroyAll;
    LPVOID  SceneCapture;
    LPVOID  Execute;
    LPVOID  ExecuteClipped;
    LPVOID  RenderState;
    LPVOID  RenderPrimitive;
    DWORD   dwReserved;
    LPVOID  other[25];          /* textures, matrices, lights, materials, GetState, reserved */
} M8D3D_CALLBACKS;

typedef struct {
    LPVOID  lpDDGbl;
    LPVOID  lpDDS;
    LPVOID  lpDDSZ;
    DWORD   dwPID;
    DWORD   dwhContext;
    HRESULT ddrval;
} M8D3D_CONTEXTCREATE;

typedef struct {
    DWORD   dwhContext;         /* or dwPID, for ContextDestroyAll */
    HRESULT ddrval;
} M8D3D_CONTEXTDESTROY;

typedef struct {
    DWORD   dwhContext;
    DWORD   dwFlag;
    HRESULT ddrval;
} M8D3D_SCENECAPTURE;

typedef struct {
    DWORD   dwhContext;
    DWORD   dwOffset;
    DWORD   dwCount;
    LPVOID  lpExeBuf;
    HRESULT ddrval;
} M8D3D_RENDERSTATE;

typedef struct {
    DWORD   dwhContext;
    DWORD   dwOffset;
    DWORD   dwStatus;
    LPVOID  lpExeBuf;
    DWORD   dwTLOffset;
    LPVOID  lpTLBuf;
    BYTE    bOpcode;            /* D3DINSTRUCTION */
    BYTE    bSize;
    WORD    wCount;
    HRESULT ddrval;
} M8D3D_RENDERPRIMITIVE;
#pragma pack( pop )

typedef char m8d3d_desc_size[sizeof( M8D3D_DEVICEDESC ) == 172 ? 1 : -1];
typedef char m8d3d_global_size[sizeof( M8D3D_GLOBAL ) == 192 ? 1 : -1];
typedef char m8d3d_callbacks_size[sizeof( M8D3D_CALLBACKS ) == 140 ? 1 : -1];

/* d3dtypes.h: D3DTLVERTEX, D3DTRIANGLE, D3DSTATE. */
typedef struct {
    float   sx;
    float   sy;
    float   sz;
    float   rhw;
    DWORD   color;
    DWORD   specular;
    float   tu;
    float   tv;
} M8D3D_TLVERTEX;

typedef struct {
    WORD    v1;
    WORD    v2;
    WORD    v3;
    WORD    flags;
} M8D3D_TRIANGLE;

typedef struct {
    DWORD   type;
    DWORD   arg;
} M8D3D_STATE;

typedef char m8d3d_tl_size[sizeof( M8D3D_TLVERTEX ) == 32 ? 1 : -1];

#define M8D3D_CONTEXTS  4

/* A shape: its corners relative to its box. Each plane of the mask strip holds one, whole, so
 * drawing it is one mono copy (8 writes, one engine command) against an outline, a fill and
 * another outline. A mask costs about a flat triangle to build, so a shape earns one only when
 * seen again, and a frame builds at most one: scenes that never repeat a shape pay a compare. */
#define M8D3D_MASKS         8
#define M8D3D_SEEN          16
#define M8D3D_MASK_ROWS     128

/* A mask copy costs ~69 ns a pixel of its box, a plain fill ~20: a large shape's inscribed
 * rectangle goes as a plain fill and only the boxes around it from the mask. */
#define M8D3D_NS_MASK       69L
#define M8D3D_NS_PLAIN      21L
#define M8D3D_NS_COMMAND    60000L
#define M8D3D_CORE_SAMPLES  6
/* What a mask must earn: hits pay for builds, so scenes without repeats soon stop building. */
#define M8D3D_BUILD_COST    3
#define M8D3D_CREDIT_MAX    16
#define M8D3D_CORE_AFTER    4

typedef struct {
    short   x;
    short   y;
    short   w;
    short   h;
} M8D3D_BOX;

typedef struct {
    short       v[6];
    short       w;
    short       h;
    M8D3D_BOX   core;           /* core.w 0 = none: the whole box from the mask */
    M8D3D_BOX   part[4];
    int         parts;
    int         hits;
    DWORD       used;           /* 0 = empty */
} M8D3D_MASK;

/* A shaded triangle drawn once in the column beside the pages and copied from there with black
 * left out: one engine command in place of edges and bands. Shape and colours must both repeat,
 * so as with masks only what is seen again is kept. fb16.c's palette holds black twice and the
 * colour cube uses the other entry, so entry 0, the key, is never a stamp's own pixel. */
#define M8D3D_STAMPS        32
#define M8D3D_STAMP_SEEN    32
#define M8D3D_STAMP_ROWS    128         /* taken from the scratch */
#define M8D3D_STAMP_PARTS   3
#define M8D3D_STAMP_CUT     3000L       /* pixels of box: below this an extra command costs more */

typedef struct {
    short   v[6];
    DWORD   rgb[3];
} M8D3D_SHADED;

typedef struct {
    M8D3D_SHADED    key;
    short           x;
    short           y;
    short           w;
    short           h;
    M8D3D_BOX       part[M8D3D_STAMP_PARTS];    /* parts 0 = the whole box */
    int             parts;
    DWORD           used;       /* 0 = empty */
} M8D3D_STAMP;

typedef struct {
    LPVOID  target;             /* the surface's interface; 0 = a free context */
    DWORD   shade;
    M8D3D_STAMP     stamp[M8D3D_STAMPS];
    M8D3D_SHADED    stamp_seen[M8D3D_STAMP_SEEN];
    int     stamp_seen_next;
    int     stamp_on;           /* the column is ours: two pages */
    int     stamp_x;            /* where the next goes: shelves */
    int     stamp_y;
    int     stamp_shelf;
    int     mx;                 /* the mask strip; mh 0 = none */
    int     my;
    int     mh;
    M8D3D_MASK  mask[M8D3D_MASKS];
    short   seen[M8D3D_SEEN][6];
    int     seen_next;
    DWORD   clock;
    int     credit;
    DWORD   cull;
    int     sx;                 /* the scratch */
    int     sy;
    int     sw;                 /* the target's width */
    int     tw;                 /* the scratch's; narrower than the target with two pages */
    int     sh;                 /* its rows; at 16 bpp, fewer than the target can have */
    int     fresh;              /* the scratch has not been cleared yet */
    int     drawn;              /* triangles in the scratch since it was cleared */
    int     dl;                 /* their box, in card coordinates */
    int     dt;
    int     dr;
    int     db;
} M8D3D_CONTEXT;

static M8D3D_GLOBAL     m8d3d_global;
static M8D3D_CALLBACKS  m8d3d_callbacks;
static M8D3D_CONTEXT    m8d3d_ctx[M8D3D_CONTEXTS];

/* Exported so a test program can see which callbacks the runtime reached: a
 * failed CreateDevice does not say. [0] ContextCreate, [1] ContextDestroy,
 * [2] SceneCapture, [3] RenderState, [4] RenderPrimitive, [5] primitives offered,
 * [6] the last target surface, [7] the last Z surface, [8] triangles drawn,
 * [9] culled, [10] refused: outside the target, [11] calls refused. Performance
 * counter ticks in Flip at 16 bpp: [12] all, [13] the 16-bit side, [14] erase and
 * triangles; [15] flips; in Blt: [16] ticks, [17] calls, [18] full clears described;
 * [19] page wait ticks; [20] triangles drawn from a mask; [21] masks built;
 * [22] triangles stamped; [23] stamps built; [24] ticks in RenderPrimitive. */
DWORD                   m8d3d_stat[25];

static LPVOID m8d3d_lcl( LPVOID surface_int )
{
    if( surface_int == NULL || IsBadReadPtr( surface_int, 8 ) ) {
        return( NULL );
    }
    return( *(LPVOID *)((BYTE *)surface_int + M8D3D_INT_LCL) );
}

/* A vertex or execute buffer's memory. */
static BYTE *m8d3d_buffer( LPVOID surface_int )
{
    BYTE *lcl = (BYTE *)m8d3d_lcl( surface_int );
    BYTE *gbl;

    if( lcl == NULL || IsBadReadPtr( lcl, 8 ) ) {
        return( NULL );
    }
    gbl = *(BYTE **)(lcl + M8D3D_LCL_GBL);
    if( gbl == NULL || IsBadReadPtr( gbl, M8D3D_GBL_VIDMEM + 4 ) ) {
        return( NULL );
    }
    return( *(BYTE **)(gbl + M8D3D_GBL_VIDMEM) );
}

static M8D3D_CONTEXT *m8d3d_context( DWORD h )
{
    if( h == 0 || h > M8D3D_CONTEXTS || m8d3d_ctx[h - 1].target == NULL ) {
        return( NULL );
    }
    return( &m8d3d_ctx[h - 1] );
}

/* One palette index for an RGB colour at 8 bpp: scaffolding for a grey-ramp palette. */
static BYTE m8d3d_index( DWORD rgb )
{
    return( (BYTE)((((rgb >> 16) & 0xFF) * 77 + ((rgb >> 8) & 0xFF) * 150 + (rgb & 0xFF) * 29) >> 8) );
}

/* At 16 bpp, the nearest entry of fb16.c's colour cube: 5 x 9 x 5 levels, with the
 * middle grey at 7Fh and the entries above it one further on. Flat colours are not
 * dithered: one fill, one index. */
static BYTE m8d3d_index16( DWORD rgb )
{
    int r = (int)(((rgb >> 16) & 0xFF) + 32) / 64;
    int g = (int)(((rgb >> 8) & 0xFF) + 16) / 32;
    int b = (int)((rgb & 0xFF) + 32) / 64;
    int c = r * 45 + g * 5 + b;

    return( (BYTE)(c < 112 ? c + 15 : c == 112 ? 127 : c + 16) );
}

/* A flat triangle's colour: the mean of its corners, so the two triangles of a
 * face do not disagree because one corner of each is lit differently. */
static DWORD m8d3d_mean( DWORD a, DWORD b, DWORD c )
{
    return( ((((a >> 16) & 0xFF) + ((b >> 16) & 0xFF) + ((c >> 16) & 0xFF)) / 3) << 16 |
            ((((a >> 8) & 0xFF) + ((b >> 8) & 0xFF) + ((c >> 8) & 0xFF)) / 3) << 8 |
            ((a & 0xFF) + (b & 0xFF) + (c & 0xFF)) / 3 );
}

static WORD m8d3d_565( DWORD rgb )
{
    return( (WORD)(((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x001F)) );
}

/* The outline over the fill, which supplies its top and bottom rows and the rows an
 * apex emptied. */
static int m8d3d_outline( int ox, int oy, const short *v, WORD alu, WORD opt, WORD colour, WORD mask )
{
    int i;

    if( !m8d3d_room( 14 ) ) {
        return( 0 );
    }
    outpw( M8_WRT_MASK, mask );
    outpw( M8_FRGD_COLOR, colour );
    outpw( M8_DP_CONFIG, M8_DP_8514 );
    outpw( M8_ALU_FG_FN, alu );
    outpw( M8_LINEDRAW_OPT, opt );
    outpw( M8_LINEDRAW_INDEX, 0 );
    for( i = 0; i < 4; ++i ) {
        outpw( M8_LINEDRAW, ox + v[2 * (i % 3)] );      /* the last closes the figure */
        outpw( M8_LINEDRAW, oy + v[2 * (i % 3) + 1] );
    }
    return( 1 );
}

/* The fill's outline in one plane of the scratch: each edge top to bottom without its
 * last row, so every row between top and bottom holds exactly two pixels. Drawn as a
 * closed figure, the row of the middle vertex can hold three, and the fill then runs
 * from the third to the end of the row. XOR, so an apex row's two pixels cancel. */
static int m8d3d_edges( int ox, int oy, const short *v, WORD plane )
{
    int i;
    int a;
    int b;

    if( !m8d3d_room( 6 ) ) {
        return( 0 );
    }
    outpw( M8_WRT_MASK, plane );
    outpw( M8_FRGD_COLOR, 0x00FF );
    outpw( M8_DP_CONFIG, M8_DP_8514 );
    outpw( M8_ALU_FG_FN, M8_MIX_XOR );
    outpw( M8_LINEDRAW_OPT, M8_LINEDRAW_POLY | M8_LINEDRAW_LAST_OFF );
    for( i = 0; i < 3; ++i ) {
        a = 2 * i;
        b = 2 * ((i + 1) % 3);
        if( v[a + 1] == v[b + 1] ) {
            continue;
        }
        if( v[a + 1] > v[b + 1] ) {
            int t = a;

            a = b;
            b = t;
        }
        if( !m8d3d_room( 5 ) ) {
            return( 0 );
        }
        outpw( M8_LINEDRAW_INDEX, 0 );
        outpw( M8_LINEDRAW, ox + v[a] );
        outpw( M8_LINEDRAW, oy + v[a + 1] );
        outpw( M8_LINEDRAW, ox + v[b] );
        outpw( M8_LINEDRAW, oy + v[b + 1] );
    }
    return( 1 );
}

/* A short triangle row by row: 3 writes a row against about 50 for an outline, a fill and
 * a second outline, so this is the cheaper way up to about 14 rows. */
#define M8D3D_SPAN_ROWS     13

static int m8d3d_spans( int dx, int dy, const short *v, BYTE colour, int t, int b )
{
    int lo[M8D3D_SPAN_ROWS + 1];
    int hi[M8D3D_SPAN_ROWS + 1];
    int i;
    int y;

    for( y = 0; y <= b - t; ++y ) {
        lo[y] = 0x7FFF;
        hi[y] = -0x7FFF;
    }
    for( i = 0; i < 3; ++i ) {
        int p = 2 * i;
        int q = 2 * ((i + 1) % 3);
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

            if( px < lo[y] ) {
                lo[y] = px;
            }
            if( px > hi[y] ) {
                hi[y] = px;
            }
            x += step;
        }
        if( v[q + 1] == v[p + 1] ) {
            y = v[p + 1] - t;
            if( v[q] < lo[y] ) {
                lo[y] = v[q];
            }
            if( v[q] > hi[y] ) {
                hi[y] = v[q];
            }
        }
    }
    if( !m8d3d_room( 4 ) ) {
        return( 0 );
    }
    outpw( M8_WRT_MASK, 0x00FF );
    outpw( M8_FRGD_COLOR, colour );
    outpw( M8_DP_CONFIG, M8_DP_8514 );
    outpw( M8_ALU_FG_FN, M8_MIX_REPLACE );
    for( y = 0; y <= b - t; ++y ) {
        if( y % 5 == 0 && !m8d3d_room( 15 ) ) {
            return( 0 );
        }
        outpw( M8_CUR_Y, dy + t + y );
        outpw( M8_CUR_X, dx + lo[y] );
        outpw( M8_SCAN_TO_X, dx + hi[y] + 1 );
    }
    return( 1 );
}

/* One triangle, v relative to the target at dx, dy and to the scratch. */
static int m8d3d_tri( M8D3D_CONTEXT *c, int dx, int dy, const short *v, BYTE colour )
{
    BYTE    plane = (BYTE)(1 << (c->drawn & 7));
    int     l = v[0];
    int     r = v[0];
    int     t = v[1];
    int     b = v[1];
    int     sx;
    int     sy;
    int     i;

    for( i = 1; i < 3; ++i ) {
        if( v[2 * i] < l ) {
            l = v[2 * i];
        }
        if( v[2 * i] > r ) {
            r = v[2 * i];
        }
        if( v[2 * i + 1] < t ) {
            t = v[2 * i + 1];
        }
        if( v[2 * i + 1] > b ) {
            b = v[2 * i + 1];
        }
    }
    if( b - t <= M8D3D_SPAN_ROWS ) {
        return( m8d3d_spans( dx, dy, v, colour, t, b ) );
    }
    /* The scratch's corner holds the triangle's box's corner: the scratch can be smaller
     * than the target. */
    sx = c->sx - l;
    sy = c->sy - t;
    /* Rows strictly between top and bottom only: an apex row can hold one outline
     * pixel, and the fill would run to the end of the row. */
    if( b - t >= 2 ) {
        if( !m8d3d_edges( sx, sy, v, plane ) ) {
            return( 0 );
        }
        if( !m8d3d_room( 15 ) ) {
            return( 0 );
        }
        outpw( M8_WRT_MASK, 0x00FF );
        outpw( M8_RD_MASK, plane );
        outpw( M8_DP_CONFIG, M8_DP_POLY_FILL );
        outpw( M8_ALU_FG_FN, M8_MIX_REPLACE );
        outpw( M8_FRGD_COLOR, colour );
        outpw( M8_DESTX_DIASTP, sx + l );
        outpw( M8_DESTY_AXSTP, sy + t + 1 );
        outpw( M8_SRC_X_START, sx + l );
        outpw( M8_SRC_X_END, sx + r + 1 );
        outpw( M8_SRC_Y_DIR, 1 );
        outpw( M8_CUR_X, dx + l );
        outpw( M8_DEST_X_START, dx + l );
        outpw( M8_DEST_X_END, dx + r + 1 );
        outpw( M8_CUR_Y, dy + t + 1 );
        outpw( M8_DEST_Y_END, dy + b );         /* starts the fill */
        if( sx + l < c->dl ) {
            c->dl = sx + l;
        }
        if( sy + t < c->dt ) {
            c->dt = sy + t;
        }
        if( sx + r + 1 > c->dr ) {
            c->dr = sx + r + 1;
        }
        if( sy + b + 1 > c->db ) {
            c->db = sy + b + 1;
        }
        ++c->drawn;
    }
    if( !m8d3d_room( 1 ) ) {
        return( 0 );
    }
    outpw( M8_RD_MASK, 0x00FF );
    return( m8d3d_outline( dx, dy, v, M8_MIX_REPLACE, M8_LINEDRAW_LAST_OFF, colour, 0x00FF ) );
}

/* The scratch's planes that have been used, all at once. Leaves the engine on the
 * plain path with open scissors. */
static int m8d3d_scratch_clear( M8D3D_CONTEXT *c )
{
    int ok = 1;

    if( c->dr > c->dl ) {
        ok = m8d3d_clear( c->dl, c->dt, c->dr, c->db );
    }
    c->drawn = 0;
    c->dl = c->dt = 0x7FFF;
    c->dr = c->db = 0;
    return( ok );
}

/* A triangle too large for the scratch, split across its tallest or widest edge until
 * it fits. Each part's outline covers the seam. */
static int m8d3d_tri_fit( M8D3D_CONTEXT *c, int dx, int dy, const short *v, BYTE colour )
{
    int     e = 0;
    int     ew = 0;
    int     h = 0;
    int     w = 0;
    int     i;
    int     d;
    int     a;
    int     b;
    short   mx;
    short   my;
    short   p[6];

    for( i = 0; i < 3; ++i ) {
        a = v[2 * i + 1];
        b = v[2 * ((i + 1) % 3) + 1];
        d = a < b ? b - a : a - b;
        if( d > h ) {
            h = d;
            e = i;
        }
        a = v[2 * i];
        b = v[2 * ((i + 1) % 3)];
        d = a < b ? b - a : a - b;
        if( d > w ) {
            w = d;
            ew = i;
        }
    }
    if( h + 2 < c->sh && w + 2 < c->tw ) {
        if( !m8d3d_tri( c, dx, dy, v, colour ) ) {
            return( 0 );
        }
        return( c->drawn == 8 ? m8d3d_scratch_clear( c ) : 1 );
    }
    if( h + 2 < c->sh ) {
        e = ew;
    }
    a = e;
    b = (e + 1) % 3;
    d = (e + 2) % 3;
    mx = (short)((v[2 * a] + v[2 * b]) / 2);
    my = (short)((v[2 * a + 1] + v[2 * b + 1]) / 2);
    p[0] = v[2 * a];
    p[1] = v[2 * a + 1];
    p[2] = mx;
    p[3] = my;
    p[4] = v[2 * d];
    p[5] = v[2 * d + 1];
    if( !m8d3d_tri_fit( c, dx, dy, p, colour ) ) {
        return( 0 );
    }
    p[0] = mx;
    p[1] = my;
    p[2] = v[2 * b];
    p[3] = v[2 * b + 1];
    return( m8d3d_tri_fit( c, dx, dy, p, colour ) );
}

static int  m8d3d_stamp_mode;   /* the engine holds the keyed copy's registers */
static int  m8d3d_mono_on;      /* the engine holds the mask copy's registers */
static int  m8d3d_mono_plane;
static int  m8d3d_mono_x;
static int  m8d3d_mono_w;

/* Any port access empties the CPU's cache here, so C run between OUTs runs from RAM: a triangle's
 * lookups cost ~11 us after one another and ~150 us with an OUT between. Copies are worked out
 * first with the cache intact, their writes kept here, then sent in one loop. */
#define M8D3D_Q_MAX     1024
/* Room for this many, not the FIFO's whole 16: waiting for it to empty leaves the engine idle when
 * a long fill ends, with the next command still crossing the bus. */
#define M8D3D_Q_CHUNK   8
#define M8D3D_Q_ONE     32              /* the most one triangle adds */

static DWORD    m8d3d_q[M8D3D_Q_MAX];
static int      m8d3d_qn;

#define M8D3D_Q( port, value ) ( m8d3d_q[m8d3d_qn++] = (DWORD)(port) | (DWORD)(WORD)(value) << 16 )

extern void m8d3d_q_send( const DWORD *p, int n );
#pragma aux m8d3d_q_send = \
    "q_next:"               \
    "mov eax, [esi]"        \
    "mov edx, eax"          \
    "shr eax, 16"           \
    "out dx, ax"            \
    "add esi, 4"            \
    "dec ecx"               \
    "jnz q_next"            \
    parm [esi] [ecx] modify [eax edx esi ecx];

static int m8d3d_q_flush( void )
{
    int i = 0;
    int ok = 1;

    while( i < m8d3d_qn ) {
        int n = m8d3d_qn - i < M8D3D_Q_CHUNK ? m8d3d_qn - i : M8D3D_Q_CHUNK;

        if( !m8d3d_room( n ) ) {
            ok = 0;
            break;
        }
        m8d3d_q_send( m8d3d_q + i, n );
        i += n;
    }
    m8d3d_qn = 0;
    return( ok );
}

/* The plain path: lines and rows drawn with RD_MASK on one plane lose pixels. Everything but a
 * queued copy comes through here first, so what is queued goes out in order. */
static int m8d3d_mono_off( void )
{
    if( !m8d3d_q_flush() ) {
        return( 0 );
    }
    if( m8d3d_stamp_mode ) {
        m8d3d_stamp_mode = 0;
        if( !m8d3d_room( 2 ) ) {
            return( 0 );
        }
        /* An engine reset does not clear this: left on, it would key the next copy. */
        outpw( M8_SRC_CMP_FN, 0 );
        outpw( M8_DP_CONFIG, M8_DP_8514 );
    }
    if( !m8d3d_mono_on ) {
        return( 1 );
    }
    m8d3d_mono_on = 0;
    if( !m8d3d_room( 2 ) ) {
        return( 0 );
    }
    outpw( M8_RD_MASK, 0x00FF );
    outpw( M8_DP_CONFIG, M8_DP_8514 );
    return( 1 );
}

/* Part of plane k's mask to (x, y) on the card, in the colour already set. */
static void m8d3d_mask_box( M8D3D_CONTEXT *c, const M8D3D_BOX *b, int x, int y )
{
    if( m8d3d_mono_x != b->x || m8d3d_mono_w != b->w ) {
        M8D3D_Q( M8_SRC_X_START, c->mx + b->x );
        M8D3D_Q( M8_SRC_X_END, c->mx + b->x + b->w );
        m8d3d_mono_x = b->x;
        m8d3d_mono_w = b->w;
    }
    M8D3D_Q( M8_DESTX_DIASTP, c->mx + b->x );
    M8D3D_Q( M8_DESTY_AXSTP, c->my + b->y );
    M8D3D_Q( M8_CUR_X, x + b->x );
    M8D3D_Q( M8_DEST_X_START, x + b->x );
    M8D3D_Q( M8_DEST_X_END, x + b->x + b->w );
    M8D3D_Q( M8_CUR_Y, y + b->y );
    M8D3D_Q( M8_DEST_Y_END, y + b->y + b->h );
}

static int m8d3d_mask_draw( M8D3D_CONTEXT *c, int k, int dx, int dy, int l, int t, BYTE colour )
{
    const M8D3D_MASK    *m = &c->mask[k];
    M8D3D_BOX           whole;
    int                 i;

    /* A stamp's compare left on would key this copy too. */
    if( m8d3d_stamp_mode && !m8d3d_mono_off() ) {
        return( 0 );
    }
    if( m8d3d_qn + M8D3D_Q_ONE * 2 > M8D3D_Q_MAX && !m8d3d_q_flush() ) {
        return( 0 );
    }
    if( !m8d3d_mono_on ) {
        M8D3D_Q( M8_WRT_MASK, 0x00FF );
        M8D3D_Q( M8_DP_CONFIG, M8_DP_MONO_VRAM );
        M8D3D_Q( M8_ALU_FG_FN, M8_MIX_REPLACE );
        M8D3D_Q( M8_ALU_BG_FN, M8_MIX_LEAVE );
        M8D3D_Q( M8_SRC_Y_DIR, 1 );
        m8d3d_mono_on = 1;
        m8d3d_mono_plane = -1;
        m8d3d_mono_x = -1;
        m8d3d_mono_w = -1;
    }
    M8D3D_Q( M8_FRGD_COLOR, colour );
    ++m8d3d_stat[20];
    if( m->core.w != 0 ) {
        M8D3D_Q( M8_RD_MASK, 0x00FF );
        M8D3D_Q( M8_DP_CONFIG, M8_DP_8514 );
        M8D3D_Q( M8_CUR_X, dx + l + m->core.x );
        M8D3D_Q( M8_CUR_Y, dy + t + m->core.y );
        M8D3D_Q( M8_MAJ_AXIS_PCNT, m->core.w - 1 );
        M8D3D_Q( M8_MIN_AXIS, m->core.h - 1 );
        M8D3D_Q( M8_CMD, M8_CMD_FILL );
        M8D3D_Q( M8_DP_CONFIG, M8_DP_MONO_VRAM );
        m8d3d_mono_plane = -1;
    }
    if( m8d3d_mono_plane != k ) {
        M8D3D_Q( M8_RD_MASK, 1 << k );
        m8d3d_mono_plane = k;
    }
    if( m->core.w == 0 ) {
        whole.x = 0;
        whole.y = 0;
        whole.w = m->w;
        whole.h = m->h;
        m8d3d_mask_box( c, &whole, dx + l, dy + t );
        return( 1 );
    }
    for( i = 0; i < m->parts; ++i ) {
        m8d3d_mask_box( c, &m->part[i], dx + l, dy + t );
    }
    return( 1 );
}

/* Row y of a shape as the outline and fill cover it. A sloped edge's pixels on a row are a run
 * about the exact crossing, and the card's line may round either way: `inner` 0 gives a span sure
 * to hold every pixel, for what is copied through the mask; 1 a span sure to hold only pixels of
 * the triangle, for the plain fill, where a pixel too many would show. An upright edge is exact.
 * A triangle's spans are convex, so a run of rows is as narrow as its two end rows. */
static void m8d3d_row_span( const short *n, int y, int *lo, int *hi, int inner )
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
        /* A shallow edge covers a run of pixels on each row. */
        run = bx == ax ? 0 : (bx > ax ? bx - ax : ax - bx) / (by - ay) / 2 + 1;
        if( inner ) {
            run = -run;
        }
        if( x - run < *lo ) *lo = x - run;
        if( x + run > *hi ) *hi = x + run;
    }
}

/* The box the shape's pixels need in rows top .. bottom, and only x from `from` to `to`: empty
 * corners cost as much as pixels. A triangle's edges bend only at its vertices, so the widest
 * a run of rows reaches is at the run's ends or a vertex's row. */
static void m8d3d_part( M8D3D_MASK *m, const short *n, int top, int bottom, int from, int to )
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
        m8d3d_row_span( n, y, &a, &b, 0 );
        if( a < from ) {
            a = from;
        }
        if( b > to ) {
            b = to;
        }
        if( a <= b ) {
            if( a < lo ) {
                lo = a;
            }
            if( b > hi ) {
                hi = b;
            }
        }
    }
    /* A pixel's margin: the card's line may round either way from this arithmetic. */
    --lo;
    ++hi;
    if( lo < from ) {
        lo = from;
    }
    if( hi > to ) {
        hi = to;
    }
    if( hi >= lo ) {
        m->part[m->parts].x = (short)lo;
        m->part[m->parts].y = (short)top;
        m->part[m->parts].w = (short)(hi - lo + 1);
        m->part[m->parts].h = (short)(bottom - top + 1);
        ++m->parts;
    }
}

static void m8d3d_parts( M8D3D_MASK *m, const short *n, int w, int h, int l, int t, int r, int b )
{
    m->parts = 0;
    if( t > 0 ) {
        m8d3d_part( m, n, 0, t - 1, 0, w - 1 );
    }
    if( l > 0 ) {
        m8d3d_part( m, n, t, b, 0, l - 1 );
    }
    if( r < w - 1 ) {
        m8d3d_part( m, n, t, b, r + 1, w - 1 );
    }
    if( b < h - 1 ) {
        m8d3d_part( m, n, b + 1, h - 1, 0, w - 1 );
    }
}

/* The cheapest inscribed rectangle by the card's rates, if it beats one copy of the whole box.
 * A right angle's rectangle reaches its corner, as an upright edge has no margin. */
static void m8d3d_core_choose( M8D3D_MASK *m, const short *n, int w, int h )
{
    long    best = (long)w * h * M8D3D_NS_MASK + M8D3D_NS_COMMAND;
    int     step = h / M8D3D_CORE_SAMPLES;
    int     best_t = -1;
    int     best_b = 0;
    int     best_l = 0;
    int     best_r = 0;
    int     a;
    int     b;
    int     i;

    m->core.w = 0;
    m->parts = 0;
    /* Below this the rectangle's commands cost more than its pixels save. */
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
            m8d3d_row_span( n, a, &alo, &ahi, 1 );
            m8d3d_row_span( n, b, &blo, &bhi, 1 );
            l = alo > blo ? alo : blo;
            r = ahi < bhi ? ahi : bhi;
            if( r > l ) {
                m8d3d_parts( m, n, w, h, l, a, r, b );
                cost = (long)(r - l + 1) * (b - a + 1) * M8D3D_NS_PLAIN + (long)(m->parts + 1) * M8D3D_NS_COMMAND;
                for( i = 0; i < m->parts; ++i ) {
                    cost += (long)m->part[i].w * m->part[i].h * M8D3D_NS_MASK;
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
    m->parts = 0;
    if( best_t < 0 ) {
        return;
    }
    m->core.x = (short)best_l;
    m->core.y = (short)best_t;
    m->core.w = (short)(best_r - best_l + 1);
    m->core.h = (short)(best_b - best_t + 1);
    m8d3d_parts( m, n, w, h, best_l, best_t, best_r, best_b );
}

/* The shape (corners relative to its box) into plane k of the strip: the same outline and fill as
 * a triangle on the screen, so the same pixels. What the plane held before is cleared first. */
static int m8d3d_mask_build( M8D3D_CONTEXT *c, int k, const short *n, int w, int h )
{
    M8D3D_MASK  *m = &c->mask[k];
    BYTE        plane = (BYTE)(1 << (c->drawn & 7));
    WORD        q = (WORD)(1 << k);
    int         i;

    if( m->used != 0 ) {
        RECT r;

        if( !m8d3d_room( 5 ) ) {
            return( 0 );
        }
        outpw( M8_WRT_MASK, q );
        outpw( M8_RD_MASK, 0x00FF );
        outpw( M8_DP_CONFIG, M8_DP_8514 );
        outpw( M8_ALU_FG_FN, M8_MIX_REPLACE );
        outpw( M8_FRGD_COLOR, 0 );
        r.left = c->mx;
        r.top = c->my;
        r.right = c->mx + m->w;
        r.bottom = c->my + m->h;
        if( !m8d3d_fill( &r ) ) {
            return( 0 );
        }
    }
    if( h >= 3 ) {
        if( !m8d3d_edges( c->sx, c->sy, n, plane ) || !m8d3d_room( 15 ) ) {
            return( 0 );
        }
        outpw( M8_WRT_MASK, q );
        outpw( M8_RD_MASK, plane );
        outpw( M8_DP_CONFIG, M8_DP_POLY_FILL );
        outpw( M8_ALU_FG_FN, M8_MIX_REPLACE );
        outpw( M8_FRGD_COLOR, 0x00FF );
        outpw( M8_DESTX_DIASTP, c->sx );
        outpw( M8_DESTY_AXSTP, c->sy + 1 );
        outpw( M8_SRC_X_START, c->sx );
        outpw( M8_SRC_X_END, c->sx + w );
        outpw( M8_SRC_Y_DIR, 1 );
        outpw( M8_CUR_X, c->mx );
        outpw( M8_DEST_X_START, c->mx );
        outpw( M8_DEST_X_END, c->mx + w );
        outpw( M8_CUR_Y, c->my + 1 );
        outpw( M8_DEST_Y_END, c->my + h - 1 );
        if( c->sx < c->dl ) {
            c->dl = c->sx;
        }
        if( c->sy < c->dt ) {
            c->dt = c->sy;
        }
        if( c->sx + w > c->dr ) {
            c->dr = c->sx + w;
        }
        if( c->sy + h > c->db ) {
            c->db = c->sy + h;
        }
        ++c->drawn;
        if( !m8d3d_room( 1 ) ) {
            return( 0 );
        }
        outpw( M8_RD_MASK, 0x00FF );
    }
    if( !m8d3d_outline( c->mx, c->my, n, M8_MIX_REPLACE, M8_LINEDRAW_LAST_OFF, 0x00FF, q ) ) {
        return( 0 );
    }
    if( !m8d3d_room( 1 ) ) {
        return( 0 );
    }
    outpw( M8_WRT_MASK, 0x00FF );
    for( i = 0; i < 6; ++i ) {
        m->v[i] = n[i];
    }
    m->w = (short)w;
    m->h = (short)h;
    m->core.w = 0;
    m->parts = 0;
    m->hits = 0;
    ++m8d3d_stat[21];
    return( c->drawn == 8 ? m8d3d_scratch_clear( c ) : 1 );
}

static int m8d3d_same_shape( const short *a, const short *b )
{
    return( a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3] && a[4] == b[4] && a[5] == b[5] );
}

/* One replayed triangle, from a mask if its shape has one. `*build` is how many masks this frame
 * may still build. */
static int m8d3d_tri_cached( M8D3D_CONTEXT *c, int dx, int dy, const short *v, BYTE colour, int *build )
{
    short   n[6];
    int     l = v[0];
    int     r = v[0];
    int     t = v[1];
    int     b = v[1];
    int     i;
    int     k;

    for( i = 1; i < 3; ++i ) {
        if( v[2 * i] < l ) {
            l = v[2 * i];
        }
        if( v[2 * i] > r ) {
            r = v[2 * i];
        }
        if( v[2 * i + 1] < t ) {
            t = v[2 * i + 1];
        }
        if( v[2 * i + 1] > b ) {
            b = v[2 * i + 1];
        }
    }
    if( c->mh != 0 && b - t + 1 < c->mh && r - l + 1 <= c->tw ) {
        for( i = 0; i < 3; ++i ) {
            n[2 * i] = (short)(v[2 * i] - l);
            n[2 * i + 1] = (short)(v[2 * i + 1] - t);
        }
        for( k = 0; k < M8D3D_MASKS; ++k ) {
            if( c->mask[k].used != 0 && m8d3d_same_shape( c->mask[k].v, n ) ) {
                c->mask[k].used = ++c->clock;
                if( c->credit < M8D3D_CREDIT_MAX ) {
                    ++c->credit;
                }
                /* The search is paid once, and only by a shape that keeps coming back. */
                if( ++c->mask[k].hits == M8D3D_CORE_AFTER ) {
                    m8d3d_core_choose( &c->mask[k], n, c->mask[k].w, c->mask[k].h );
                }
                return( m8d3d_mask_draw( c, k, dx, dy, l, t, colour ) );
            }
        }
        for( i = 0; i < M8D3D_SEEN && !m8d3d_same_shape( c->seen[i], n ); ++i ) {
        }
        if( i == M8D3D_SEEN ) {
            for( i = 0; i < 6; ++i ) {
                c->seen[c->seen_next][i] = n[i];
            }
            c->seen_next = (c->seen_next + 1) % M8D3D_SEEN;
        } else if( *build > 0 && c->credit >= M8D3D_BUILD_COST ) {
            int oldest = 0;

            for( k = 1; k < M8D3D_MASKS; ++k ) {
                if( c->mask[k].used < c->mask[oldest].used ) {
                    oldest = k;
                }
            }
            --*build;
            c->credit -= M8D3D_BUILD_COST;
            if( !m8d3d_mono_off() || !m8d3d_mask_build( c, oldest, n, r - l + 1, b - t + 1 ) ) {
                return( 0 );
            }
            c->mask[oldest].used = ++c->clock;
            return( m8d3d_mask_draw( c, oldest, dx, dy, l, t, colour ) );
        }
    }
    return( m8d3d_mono_off() && m8d3d_tri_fit( c, dx, dy, v, colour ) );
}

/* A triangle whose corners differ: colour as a plane over the screen, drawn as bands of rows, each
 * one colour. The engine has no per-pixel colour step that takes three corners, and a band costs
 * one command, so how much the corners differ sets how many there are. */
#define M8D3D_BANDS_MAX     8
#define M8D3D_BAND_STEP     24          /* of 255, the change one more band is worth */
#define M8D3D_BAND_ROWS     3

typedef struct {
    long    c0[3];              /* 16.16 */
    long    gx[3];
    long    gy[3];
    int     x0;
    int     y0;
} M8D3D_PLANE;

static void m8d3d_plane_make( M8D3D_PLANE *p, const short *v, const DWORD *rgb, long area )
{
    int i;

    p->x0 = v[0];
    p->y0 = v[1];
    for( i = 0; i < 3; ++i ) {
        int     s = 16 - 8 * i;
        long    a = (long)((rgb[0] >> s) & 0xFF);
        long    b = (long)((rgb[1] >> s) & 0xFF) - a;
        long    c = (long)((rgb[2] >> s) & 0xFF) - a;

        p->c0[i] = a << 16;
        /* At most 255 x a 10-bit distance, shifted by 8 here and 8 below: inside 32 bits. */
        p->gx[i] = ((b * (v[5] - v[1]) - c * (v[3] - v[1])) << 8) / area << 8;
        p->gy[i] = ((c * (v[2] - v[0]) - b * (v[4] - v[0])) << 8) / area << 8;
    }
}

static BYTE m8d3d_plane_index( const M8D3D_PLANE *p, int x, int y )
{
    DWORD   rgb = 0;
    int     i;

    for( i = 0; i < 3; ++i ) {
        long c = (p->c0[i] + p->gx[i] * (x - p->x0) + p->gy[i] * (y - p->y0) + 0x8000) >> 16;

        if( c < 0 ) {
            c = 0;
        }
        if( c > 255 ) {
            c = 255;
        }
        rgb = rgb << 8 | (DWORD)c;
    }
    return( m8d3d_index16( rgb ) );
}

/* The fill stops at the last boundary pixel, leaving the right edge out, so here the edges bounding
 * it on the right go one pixel further: bands need no outline in one colour over them. */
static int m8d3d_edges_grad( int ox, int oy, const short *v, WORD plane )
{
    int     top = 0;
    int     bot = 0;
    int     mid;
    int     k;
    long    side;

    for( k = 1; k < 3; ++k ) {
        if( v[2 * k + 1] < v[2 * top + 1] ) {
            top = k;
        }
        if( v[2 * k + 1] >= v[2 * bot + 1] ) {
            bot = k;
        }
    }
    mid = 3 - top - bot;
    side = (long)(v[2 * bot] - v[2 * top]) * (v[2 * mid + 1] - v[2 * top + 1]) -
           (long)(v[2 * bot + 1] - v[2 * top + 1]) * (v[2 * mid] - v[2 * top]);
    if( !m8d3d_room( 5 ) ) {
        return( 0 );
    }
    outpw( M8_WRT_MASK, plane );
    outpw( M8_FRGD_COLOR, 0x00FF );
    outpw( M8_DP_CONFIG, M8_DP_8514 );
    outpw( M8_ALU_FG_FN, M8_MIX_XOR );
    outpw( M8_LINEDRAW_OPT, M8_LINEDRAW_POLY | M8_LINEDRAW_LAST_OFF );
    for( k = 0; k < 3; ++k ) {
        int a = k;
        int b = (k + 1) % 3;
        int shift;

        if( v[2 * a + 1] == v[2 * b + 1] ) {
            continue;
        }
        if( v[2 * a + 1] > v[2 * b + 1] ) {
            int s = a;

            a = b;
            b = s;
        }
        /* side < 0: the middle vertex lies right of the long edge, so the short edges bound it. */
        shift = (a == top && b == bot) ? side >= 0 : side < 0;
        if( !m8d3d_room( 5 ) ) {
            return( 0 );
        }
        outpw( M8_LINEDRAW_INDEX, 0 );
        outpw( M8_LINEDRAW, ox + v[2 * a] + shift );
        outpw( M8_LINEDRAW, oy + v[2 * a + 1] );
        outpw( M8_LINEDRAW, ox + v[2 * b] + shift );
        outpw( M8_LINEDRAW, oy + v[2 * b + 1] );
    }
    return( 1 );
}

/* A triangle's edges, top down, for what a run of rows reaches: one division an edge a triangle. */
typedef struct {
    int     ay;
    int     by;
    long    ax;                 /* 16.16 */
    long    step;
    int     run;
} M8D3D_EDGE;

static void m8d3d_edges_make( M8D3D_EDGE *e, const short *v )
{
    int i;

    for( i = 0; i < 3; ++i ) {
        int ax = v[2 * i];
        int ay = v[2 * i + 1];
        int bx = v[2 * ((i + 1) % 3)];
        int by = v[2 * ((i + 1) % 3) + 1];

        if( ay > by ) {
            int s = ax;

            ax = bx;
            bx = s;
            s = ay;
            ay = by;
            by = s;
        }
        e[i].ay = ay;
        e[i].by = by;
        e[i].ax = (long)ax << 16;
        e[i].step = by == ay ? 0 : ((long)(bx - ax) << 16) / (by - ay);
        e[i].run = bx == ax ? 0 : (int)((e[i].step < 0 ? -e[i].step : e[i].step) >> 17) + 1;
    }
}

/* Row y's first and last pixel, sure to hold every pixel of the outline. */
static void m8d3d_edges_at( const M8D3D_EDGE *e, const short *v, int y, int *lo, int *hi )
{
    int i;

    for( i = 0; i < 3; ++i ) {
        int x;

        if( y < e[i].ay || y > e[i].by ) {
            continue;
        }
        if( e[i].ay == e[i].by ) {
            int a = v[2 * i];
            int b = v[2 * ((i + 1) % 3)];

            if( a < *lo ) *lo = a;
            if( b < *lo ) *lo = b;
            if( a > *hi ) *hi = a;
            if( b > *hi ) *hi = b;
            continue;
        }
        x = (int)((e[i].ax + e[i].step * (y - e[i].ay)) >> 16);
        if( x - e[i].run < *lo ) *lo = x - e[i].run;
        if( x + e[i].run > *hi ) *hi = x + e[i].run;
    }
}

/* What rows top .. bottom reach: edges bend only at vertices. */
static void m8d3d_rows_reach( const M8D3D_EDGE *e, const short *v, int top, int bottom, int *lo, int *hi )
{
    int k;

    *lo = 0x7FFF;
    *hi = -0x7FFF;
    m8d3d_edges_at( e, v, top, lo, hi );
    if( bottom != top ) {
        m8d3d_edges_at( e, v, bottom, lo, hi );
    }
    for( k = 0; k < 3; ++k ) {
        int y = v[2 * k + 1];

        if( y > top && y < bottom ) {
            m8d3d_edges_at( e, v, y, lo, hi );
        }
    }
}

static int m8d3d_end_row( const short *v, const M8D3D_PLANE *p, int dx, int dy, int y )
{
    int lo = 0x7FFF;
    int hi = -0x7FFF;
    int k;

    for( k = 0; k < 3; ++k ) {
        if( v[2 * k + 1] == y ) {
            if( v[2 * k] < lo ) {
                lo = v[2 * k];
            }
            if( v[2 * k] > hi ) {
                hi = v[2 * k];
            }
        }
    }
    if( !m8d3d_room( 4 ) ) {
        return( 0 );
    }
    outpw( M8_FRGD_COLOR, m8d3d_plane_index( p, (lo + hi) / 2, y ) );
    outpw( M8_CUR_Y, dy + y );
    outpw( M8_CUR_X, dx + lo );
    outpw( M8_SCAN_TO_X, dx + hi + 1 );
    return( 1 );
}

/* Returns 1 with *done 0 when too small to band or too large for the scratch: the flat path's. */
static int m8d3d_tri_bands( M8D3D_CONTEXT *c, int dx, int dy, const short *v, const DWORD *rgb, int *done )
{
    M8D3D_PLANE p;
    M8D3D_EDGE  edge[3];
    int         midx;
    WORD        plane = (WORD)(1 << (c->drawn & 7));
    long        area = (long)(v[2] - v[0]) * (v[5] - v[1]) - (long)(v[3] - v[1]) * (v[4] - v[0]);
    int         l = v[0];
    int         r = v[0];
    int         t = v[1];
    int         b = v[1];
    int         spread = 0;
    int         i;
    int         n;
    int         rows;
    int         sx;
    int         sy;

    *done = 0;
    for( i = 1; i < 3; ++i ) {
        if( v[2 * i] < l ) l = v[2 * i];
        if( v[2 * i] > r ) r = v[2 * i];
        if( v[2 * i + 1] < t ) t = v[2 * i + 1];
        if( v[2 * i + 1] > b ) b = v[2 * i + 1];
    }
    for( i = 0; i < 3; ++i ) {
        int s = 16 - 8 * i;
        int a = (int)((rgb[0] >> s) & 0xFF);
        int e = (int)((rgb[1] >> s) & 0xFF);
        int f = (int)((rgb[2] >> s) & 0xFF);
        int lo = a < e ? (a < f ? a : f) : (e < f ? e : f);
        int hi = a > e ? (a > f ? a : f) : (e > f ? e : f);

        if( hi - lo > spread ) {
            spread = hi - lo;
        }
    }
    rows = b - t - 1;
    n = spread / M8D3D_BAND_STEP + 1;
    if( n > M8D3D_BANDS_MAX ) {
        n = M8D3D_BANDS_MAX;
    }
    if( n > rows / M8D3D_BAND_ROWS ) {
        n = rows / M8D3D_BAND_ROWS;
    }
    if( area == 0 || n < 2 || b - t + 2 >= c->sh || r - l + 3 >= c->tw ) {
        return( 1 );
    }
    *done = 1;
    m8d3d_plane_make( &p, v, rgb, area );
    m8d3d_edges_make( edge, v );
    sx = c->sx - l;
    sy = c->sy - t;
    if( !m8d3d_mono_off() || !m8d3d_edges_grad( sx, sy, v, plane ) || !m8d3d_room( 5 ) ) {
        return( 0 );
    }
    outpw( M8_WRT_MASK, 0x00FF );
    outpw( M8_RD_MASK, plane );
    outpw( M8_DP_CONFIG, M8_DP_POLY_FILL );
    outpw( M8_ALU_FG_FN, M8_MIX_REPLACE );
    outpw( M8_SRC_Y_DIR, 1 );
    midx = (l + r) / 2;
    for( i = 0; i < n; ) {
        int     a = t + 1 + rows * i / n;
        BYTE    colour = m8d3d_plane_index( &p, midx, t + 1 + (rows * i + rows / 2) / n );
        int     j = i + 1;
        int     e;
        int     lo;
        int     hi;

        /* The palette has few levels: bands that land on one entry are one command. */
        while( j < n && m8d3d_plane_index( &p, midx, t + 1 + (rows * j + rows / 2) / n ) == colour ) {
            ++j;
        }
        e = t + 1 + rows * j / n;
        m8d3d_rows_reach( edge, v, a, e - 1, &lo, &hi );
        /* Each band's box is only as wide as its rows reach: empty corners cost like pixels. */
        if( lo < l ) {
            lo = l;
        }
        if( hi > r ) {
            hi = r;
        }
        if( !m8d3d_room( 10 ) ) {
            return( 0 );
        }
        outpw( M8_FRGD_COLOR, colour );
        outpw( M8_DESTX_DIASTP, sx + lo );
        outpw( M8_DESTY_AXSTP, sy + a );
        outpw( M8_SRC_X_START, sx + lo );
        outpw( M8_SRC_X_END, sx + hi + 1 );
        outpw( M8_CUR_X, dx + lo );
        outpw( M8_DEST_X_START, dx + lo );
        outpw( M8_DEST_X_END, dx + hi + 1 );
        outpw( M8_CUR_Y, dy + a );
        outpw( M8_DEST_Y_END, dy + e );
        i = j;
    }
    if( !m8d3d_room( 2 ) ) {
        return( 0 );
    }
    outpw( M8_RD_MASK, 0x00FF );
    outpw( M8_DP_CONFIG, M8_DP_8514 );
    if( !m8d3d_end_row( v, &p, dx, dy, t ) || !m8d3d_end_row( v, &p, dx, dy, b ) ) {
        return( 0 );
    }
    if( sx + l < c->dl ) {
        c->dl = sx + l;
    }
    if( sy + t < c->dt ) {
        c->dt = sy + t;
    }
    if( sx + r + 2 > c->dr ) {
        c->dr = sx + r + 2;
    }
    if( sy + b + 1 > c->db ) {
        c->db = sy + b + 1;
    }
    ++c->drawn;
    return( c->drawn == 8 ? m8d3d_scratch_clear( c ) : 1 );
}

static int m8d3d_same_shaded( const M8D3D_SHADED *a, const short *n, const DWORD *rgb )
{
    return( a->rgb[0] == rgb[0] && a->rgb[1] == rgb[1] && a->rgb[2] == rgb[2] && m8d3d_same_shape( a->v, n ) );
}

static int m8d3d_stamp_copy( const M8D3D_STAMP *s, int x, int y )
{
    M8D3D_BOX       whole;
    const M8D3D_BOX *box;
    int             n;
    int             i;

    if( !m8d3d_stamp_mode ) {
        if( !m8d3d_mono_off() ) {
            return( 0 );
        }
        M8D3D_Q( M8_WRT_MASK, 0x00FF );
        M8D3D_Q( M8_RD_MASK, 0x00FF );
        M8D3D_Q( M8_DP_CONFIG, M8_DP_VRAM_BLIT );
        M8D3D_Q( M8_ALU_FG_FN, M8_MIX_REPLACE );
        M8D3D_Q( M8_ALU_BG_FN, M8_MIX_LEAVE );
        M8D3D_Q( M8_SRC_CMP_COLOR, 0 );
        M8D3D_Q( M8_SRC_CMP_FN, M8_CMP_UNEQUAL );
        M8D3D_Q( M8_SRC_Y_DIR, 1 );
        m8d3d_stamp_mode = 1;
    }
    if( m8d3d_qn + M8D3D_Q_ONE * 2 > M8D3D_Q_MAX && !m8d3d_q_flush() ) {
        return( 0 );
    }
    if( s->parts == 0 ) {
        whole.x = 0;
        whole.y = 0;
        whole.w = s->w;
        whole.h = s->h;
        box = &whole;
        n = 1;
    } else {
        box = s->part;
        n = s->parts;
    }
    for( i = 0; i < n; ++i ) {
        M8D3D_Q( M8_SRC_X_START, s->x + box[i].x );
        M8D3D_Q( M8_SRC_X_END, s->x + box[i].x + box[i].w );
        M8D3D_Q( M8_DESTX_DIASTP, s->x + box[i].x );
        M8D3D_Q( M8_DESTY_AXSTP, s->y + box[i].y );
        M8D3D_Q( M8_CUR_X, x + box[i].x );
        M8D3D_Q( M8_DEST_X_START, x + box[i].x );
        M8D3D_Q( M8_DEST_X_END, x + box[i].x + box[i].w );
        M8D3D_Q( M8_CUR_Y, y + box[i].y );
        M8D3D_Q( M8_DEST_Y_END, y + box[i].y + box[i].h );
    }
    ++m8d3d_stat[22];
    return( 1 );
}

/* A large stamp cut by rows, each box only as wide as its rows reach: a triangle's box is half empty
 * and an empty pixel costs what a drawn one does. */
static void m8d3d_stamp_cut( M8D3D_STAMP *s, const short *n )
{
    M8D3D_EDGE  e[3];
    int         i;

    s->parts = 0;
    if( (long)s->w * s->h < M8D3D_STAMP_CUT ) {
        return;
    }
    m8d3d_edges_make( e, n );
    for( i = 0; i < M8D3D_STAMP_PARTS; ++i ) {
        int a = s->h * i / M8D3D_STAMP_PARTS;
        int b = s->h * (i + 1) / M8D3D_STAMP_PARTS;
        int lo;
        int hi;

        m8d3d_rows_reach( e, n, a, b - 1, &lo, &hi );
        if( lo < 0 ) {
            lo = 0;
        }
        if( hi > s->w - 1 ) {
            hi = s->w - 1;
        }
        s->part[i].x = (short)lo;
        s->part[i].y = (short)a;
        s->part[i].w = (short)(hi - lo + 1);
        s->part[i].h = (short)(b - a);
    }
    s->parts = M8D3D_STAMP_PARTS;
}

/* A place for a w x h stamp: shelves down the column beside page 0, then the rows taken from the
 * scratch. When full they all go: replacing one at a time would need a free list for a case
 * where the scene has more shaded triangles than the column holds, and is not repeating them. */
static int m8d3d_stamp_place( M8D3D_CONTEXT *c, int w, int h, int *x, int *y )
{
    int k;

    if( w > c->tw ) {
        return( 0 );
    }
    if( c->stamp_x + w > c->mx + c->tw ) {
        c->stamp_x = c->mx;
        c->stamp_y += c->stamp_shelf;
        c->stamp_shelf = 0;
    }
    /* Below page 0's rows lies the mask strip; the rest of the column follows it. */
    if( c->stamp_y < c->my && c->stamp_y + h > c->my ) {
        c->stamp_y = c->my + c->mh;
        c->stamp_x = c->mx;
        c->stamp_shelf = 0;
    }
    if( c->stamp_y >= c->my && c->stamp_y + h > c->my + c->mh + M8D3D_STAMP_ROWS ) {
        for( k = 0; k < M8D3D_STAMPS; ++k ) {
            c->stamp[k].used = 0;
        }
        c->stamp_x = c->mx;
        c->stamp_y = 0;
        c->stamp_shelf = 0;
        if( h > c->my ) {
            return( 0 );
        }
    }
    *x = c->stamp_x;
    *y = c->stamp_y;
    c->stamp_x += w;
    if( h > c->stamp_shelf ) {
        c->stamp_shelf = h;
    }
    return( 1 );
}

/* A shaded triangle by stamp if it has one or has earned one. Returns 1 with *done 0 for bands. */
static int m8d3d_tri_stamped( M8D3D_CONTEXT *c, int dx, int dy, const short *v, const DWORD *rgb, int *build, int *done )
{
    short   n[6];
    int     l = v[0];
    int     r = v[0];
    int     t = v[1];
    int     b = v[1];
    int     i;
    int     k;
    int     x;
    int     y;
    int     oldest = 0;
    int     band_done = 0;

    *done = 0;
    if( !c->stamp_on ) {
        return( 1 );
    }
    for( i = 1; i < 3; ++i ) {
        if( v[2 * i] < l ) l = v[2 * i];
        if( v[2 * i] > r ) r = v[2 * i];
        if( v[2 * i + 1] < t ) t = v[2 * i + 1];
        if( v[2 * i + 1] > b ) b = v[2 * i + 1];
    }
    /* Too short for two bands: nothing to keep, and a place in the column wasted. */
    if( b - t - 1 < 2 * M8D3D_BAND_ROWS ) {
        return( 1 );
    }
    for( i = 0; i < 3; ++i ) {
        n[2 * i] = (short)(v[2 * i] - l);
        n[2 * i + 1] = (short)(v[2 * i + 1] - t);
    }
    for( k = 0; k < M8D3D_STAMPS; ++k ) {
        if( c->stamp[k].used != 0 && m8d3d_same_shaded( &c->stamp[k].key, n, rgb ) ) {
            c->stamp[k].used = ++c->clock;
            *done = 1;
            return( m8d3d_stamp_copy( &c->stamp[k], dx + l, dy + t ) );
        }
    }
    for( i = 0; i < M8D3D_STAMP_SEEN && !m8d3d_same_shaded( &c->stamp_seen[i], n, rgb ); ++i ) {
    }
    if( i == M8D3D_STAMP_SEEN ) {
        M8D3D_SHADED *s = &c->stamp_seen[c->stamp_seen_next];

        for( i = 0; i < 6; ++i ) {
            s->v[i] = n[i];
        }
        for( i = 0; i < 3; ++i ) {
            s->rgb[i] = rgb[i];
        }
        c->stamp_seen_next = (c->stamp_seen_next + 1) % M8D3D_STAMP_SEEN;
        return( 1 );
    }
    if( *build <= 0 ) {
        return( 1 );
    }
    for( k = 1; k < M8D3D_STAMPS; ++k ) {
        if( c->stamp[k].used < c->stamp[oldest].used ) {
            oldest = k;
        }
    }
    if( !m8d3d_stamp_place( c, r - l + 2, b - t + 2, &x, &y ) ) {
        return( 1 );
    }
    c->stamp[oldest].used = 0;
    if( !m8d3d_mono_off() || !m8d3d_clear( x, y, x + r - l + 2, y + b - t + 2 ) ) {
        return( 0 );
    }
    if( !m8d3d_tri_bands( c, x, y, n, rgb, &band_done ) ) {
        return( 0 );
    }
    if( !band_done ) {
        return( 1 );
    }
    --*build;
    {
        M8D3D_STAMP *s = &c->stamp[oldest];

        for( i = 0; i < 6; ++i ) {
            s->key.v[i] = n[i];
        }
        for( i = 0; i < 3; ++i ) {
            s->key.rgb[i] = rgb[i];
        }
        s->x = (short)x;
        s->y = (short)y;
        s->w = (short)(r - l + 1);
        s->h = (short)(b - t + 1);
        m8d3d_stamp_cut( s, n );
        s->used = ++c->clock;
        ++m8d3d_stat[23];
        *done = 1;
        return( m8d3d_stamp_copy( s, dx + l, dy + t ) );
    }
}

/* At 16 bpp, from Flip: the last frame's boxes back to black, then a buffer's
 * triangles, on the card, dy rows down. `rgb` holds three corner colours a triangle. The engine
 * is idle on return: 0 if it could not be used. */
int m8d3d_replay( const RECT *erase, int erase_n, BYTE black, int dy, const short *v, const WORD *index,
                  const DWORD *rgb, int n )
{
    M8D3D_CONTEXT   *c = NULL;
    RECT            r;
    int             k;
    int             i;
    int             ok = 1;
    int             build = 1;

    for( k = 0; k < M8D3D_CONTEXTS && c == NULL; ++k ) {
        if( m8d3d_ctx[k].target != NULL ) {
            c = &m8d3d_ctx[k];
        }
    }
    if( c == NULL || !m8d3d_engine_begin() ) {
        ++m8d3d_stat[11];
        return( 0 );
    }
    m8d3d_mono_on = 0;
    m8d3d_stamp_mode = 0;
    m8d3d_qn = 0;
    if( c->fresh ) {
        ok = m8d3d_scratch_clear( c );
        if( ok && c->mh != 0 ) {
            ok = m8d3d_clear( c->mx, c->my, c->mx + c->tw, c->my + c->mh );
        }
        c->fresh = 0;
    }
    if( ok && erase_n != 0 ) {
        ok = m8d3d_room( 1 );
        outpw( M8_FRGD_COLOR, black );
        for( i = 0; ok && i < erase_n; ++i ) {
            r = erase[i];
            r.top += dy;
            r.bottom += dy;
            ok = m8d3d_fill( &r );
        }
    }
    for( i = 0; ok && i < n; ++i ) {
        {
            const DWORD *g = rgb + 3 * i;
            int         done = 0;

            if( g[0] != g[1] || g[1] != g[2] ) {
                ok = m8d3d_tri_stamped( c, 0, dy, v + 6 * i, g, &build, &done );
                if( ok && !done ) {
                    ok = m8d3d_tri_bands( c, 0, dy, v + 6 * i, g, &done );
                }
            }
            if( ok && !done ) {
                ok = m8d3d_tri_cached( c, 0, dy, v + 6 * i, (BYTE)index[i], &build );
            }
        }
        ++m8d3d_stat[8];
    }
    if( ok ) {
        ok = m8d3d_mono_off();
    }
    if( ok ) {
        ok = m8d3d_scratch_clear( c );
    }
    m8d3d_q_flush();
    /* Whatever failed, the compare must not outlive this: GDI's copies would be keyed. */
    if( m8d3d_stamp_mode ) {
        m8d3d_stamp_mode = 0;
        outpw( M8_SRC_CMP_FN, 0 );
    }
    m8d3d_engine_end();
    if( !ok ) {
        ++m8d3d_stat[11];
    }
    return( ok );
}

static DWORD __stdcall m8d3d_context_create( M8D3D_CONTEXTCREATE *d )
{
    int             k;
    int             x;
    int             y;
    M8D3D_CONTEXT   *c;

    ++m8d3d_stat[0];
    m8d3d_stat[6] = (DWORD)d->lpDDS;
    m8d3d_stat[7] = (DWORD)d->lpDDSZ;
    d->ddrval = DDERR_OUTOFVIDEOMEMORY;
    for( k = 0; k < M8D3D_CONTEXTS && m8d3d_ctx[k].target != NULL; ++k ) {
    }
    if( k == M8D3D_CONTEXTS ) {
        return( DDHAL_DRIVER_HANDLED );
    }
    c = &m8d3d_ctx[k];
    if( !m8d3d_surface( m8d3d_lcl( d->lpDDS ), &x, &y, &c->sw, &c->sh ) ) {
        return( DDHAL_DRIVER_HANDLED );
    }
    c->tw = c->sw;
    if( !m8d3d_scratch( c, &c->tw, &c->sh, &c->sx, &c->sy ) ) {
        return( DDHAL_DRIVER_HANDLED );
    }
    /* Two pages leave a tall column beside them: the strip takes its top rows. */
    memset( c->stamp, 0, sizeof( c->stamp ) );
    memset( c->stamp_seen, 0, sizeof( c->stamp_seen ) );
    c->stamp_seen_next = 0;
    c->stamp_on = 0;
    memset( c->mask, 0, sizeof( c->mask ) );
    memset( c->seen, 0, sizeof( c->seen ) );
    c->seen_next = 0;
    c->clock = 0;
    c->credit = M8D3D_CREDIT_MAX;
    c->mh = 0;
    if( m8d3d_fb16() && c->sh > 2 * M8D3D_MASK_ROWS && c->tw > 128 ) {
        c->mx = c->sx;
        c->my = c->sy;
        c->mh = M8D3D_MASK_ROWS;
        c->sy += M8D3D_MASK_ROWS;
        c->sh -= M8D3D_MASK_ROWS;
        /* Only the two-page column has rows above it, beside page 0. */
        if( c->mx == 640 && c->my == 512 && c->sh > 2 * M8D3D_STAMP_ROWS ) {
            c->sy += M8D3D_STAMP_ROWS;
            c->sh -= M8D3D_STAMP_ROWS;
            c->stamp_on = 1;
            c->stamp_x = c->mx;
            c->stamp_y = 0;
            c->stamp_shelf = 0;
        }
    }
    c->target = d->lpDDS;
    c->shade = M8D3D_SHADE_GOURAUD;
    c->cull = M8D3D_CULL_CCW;
    c->fresh = 1;
    c->drawn = 0;
    c->dl = c->sx;
    c->dt = c->sy;
    c->dr = c->sx + c->tw;
    c->db = c->sy + c->sh;
    d->dwhContext = k + 1;
    d->ddrval = DD_OK;
    return( DDHAL_DRIVER_HANDLED );
}

static void m8d3d_context_free( M8D3D_CONTEXT *c )
{
    if( c->target != NULL ) {
        m8d3d_scratch_free( c );
        c->target = NULL;
    }
}

static DWORD __stdcall m8d3d_context_destroy( M8D3D_CONTEXTDESTROY *d )
{
    M8D3D_CONTEXT *c = m8d3d_context( d->dwhContext );

    ++m8d3d_stat[1];
    if( c != NULL ) {
        m8d3d_context_free( c );
    }
    d->ddrval = DD_OK;
    return( DDHAL_DRIVER_HANDLED );
}

/* Its argument is a process id, and a context here records no owner, so none can
 * be matched to it: a context lost this way holds its slot and scratch until reboot. */
static DWORD __stdcall m8d3d_context_destroy_all( M8D3D_CONTEXTDESTROY *d )
{
    ++m8d3d_stat[1];
    d->ddrval = DD_OK;
    return( DDHAL_DRIVER_HANDLED );
}

static DWORD __stdcall m8d3d_scene_capture( M8D3D_SCENECAPTURE *d )
{
    ++m8d3d_stat[2];
    d->ddrval = DD_OK;
    return( DDHAL_DRIVER_HANDLED );
}

static DWORD __stdcall m8d3d_render_state( M8D3D_RENDERSTATE *d )
{
    M8D3D_CONTEXT       *c = m8d3d_context( d->dwhContext );
    BYTE                *exe = m8d3d_buffer( d->lpExeBuf );
    const M8D3D_STATE   *s;
    DWORD               i;

    ++m8d3d_stat[3];
    if( c != NULL && exe != NULL ) {
        s = (const M8D3D_STATE *)(exe + d->dwOffset);
        for( i = 0; i < d->dwCount; ++i ) {
            if( s[i].type == M8D3D_RS_CULLMODE ) {
                c->cull = s[i].arg;
            }
            if( s[i].type == M8D3D_RS_SHADEMODE ) {
                c->shade = s[i].arg;
            }
        }
    }
    d->ddrval = DD_OK;
    return( DDHAL_DRIVER_HANDLED );
}

static DWORD __stdcall m8d3d_render_primitive( M8D3D_RENDERPRIMITIVE *d )
{
    M8D3D_CONTEXT           *c = m8d3d_context( d->dwhContext );
    BYTE                    *exe = m8d3d_buffer( d->lpExeBuf );
    BYTE                    *tl = m8d3d_buffer( d->lpTLBuf );
    const M8D3D_TLVERTEX    *vtx;
    const M8D3D_TRIANGLE    *tri;
    const M8D3D_TLVERTEX    *a[3];
    short                   v[6];
    int                     x;
    int                     y;
    int                     w;
    int                     h;
    long                    area;
    unsigned                i;
    int                     j;
    int                     ok = 1;
    int                     fb16;
    DWORD                   rgb;
    LPVOID                  lcl;
    LARGE_INTEGER           rp0;
    LARGE_INTEGER           rp1;

    ++m8d3d_stat[4];
    m8d3d_stat[5] += d->wCount;
    d->ddrval = DD_OK;
    QueryPerformanceCounter( &rp0 );
    if( d->bOpcode != M8D3D_OP_TRIANGLE ) {
        return( DDHAL_DRIVER_HANDLED );
    }
    fb16 = m8d3d_fb16();
    /* A flip swaps the surfaces' memory, so the target is looked up each time. */
    if( c == NULL || exe == NULL || tl == NULL || d->bSize < sizeof( M8D3D_TRIANGLE ) ||
        !m8d3d_surface( m8d3d_lcl( c->target ), &x, &y, &w, &h ) || w > c->sw ||
        (!fb16 && (h > c->sh || !m8d3d_engine_begin())) ) {
        ++m8d3d_stat[11];
        return( DDHAL_DRIVER_HANDLED );
    }
    /* A new context's scratch holds whatever the card left there; its box was set
     * to the whole scratch when it was created. At 16 bpp, the replay sees to it. */
    if( !fb16 && c->fresh ) {
        ok = m8d3d_scratch_clear( c );
        c->fresh = 0;
    }
    /* Once a call: it asks the kernel whether the pointer is good. */
    lcl = m8d3d_lcl( c->target );
    vtx = (const M8D3D_TLVERTEX *)(tl + d->dwTLOffset);
    for( i = 0; ok && i < d->wCount; ++i ) {
        tri = (const M8D3D_TRIANGLE *)(exe + d->dwOffset + (DWORD)i * d->bSize);
        a[0] = &vtx[tri->v1];
        a[1] = &vtx[tri->v2];
        a[2] = &vtx[tri->v3];
        for( j = 0; j < 3; ++j ) {
            v[2 * j] = (short)m8d3d_round( a[j]->sx );
            v[2 * j + 1] = (short)m8d3d_round( a[j]->sy );
        }
        /* Above zero runs clockwise on the screen. */
        area = (long)(v[2] - v[0]) * (v[5] - v[1]) - (long)(v[3] - v[1]) * (v[4] - v[0]);
        if( area == 0 || (c->cull == M8D3D_CULL_CCW && area < 0) ||
            (c->cull == M8D3D_CULL_CW && area > 0) ) {
            ++m8d3d_stat[9];
            continue;
        }
        if( v[0] < 0 || v[0] >= w || v[1] < 0 || v[1] >= h ||
            v[2] < 0 || v[2] >= w || v[3] < 0 || v[3] >= h ||
            v[4] < 0 || v[4] >= w || v[5] < 0 || v[5] >= h ) {
            ++m8d3d_stat[10];
            continue;
        }
        rgb = m8d3d_mean( a[0]->color, a[1]->color, a[2]->color );
        if( fb16 ) {
            DWORD corners[3];

            /* Flat shading keeps one colour, the corners' mean: a face's two triangles then
             * agree where the first vertex alone would not. */
            for( j = 0; j < 3; ++j ) {
                corners[j] = c->shade == M8D3D_SHADE_GOURAUD ? a[j]->color & 0x00FFFFFFUL : rgb;
            }
            /* Described, not drawn: Flip has the engine draw it. */
            ok = m8d3d_record( lcl, v, m8d3d_index16( rgb ), m8d3d_565( rgb ), corners );
            continue;
        }
        ok = m8d3d_tri_fit( c, x, y, v, m8d3d_index( rgb ) );
        ++m8d3d_stat[8];
    }
    if( !fb16 ) {
        if( ok ) {
            ok = m8d3d_scratch_clear( c );
        }
        m8d3d_engine_end();
    }
    if( !ok ) {
        ++m8d3d_stat[11];
    }
    QueryPerformanceCounter( &rp1 );
    m8d3d_stat[24] += rp1.LowPart - rp0.LowPart;
    return( DDHAL_DRIVER_HANDLED );
}

/* The two tables, filled; their linear addresses for the HAL information. */
void m8d3d_init( DWORD *global, DWORD *callbacks )
{
    M8D3D_DEVICEDESC *c = &m8d3d_global.hwCaps;

    memset( &m8d3d_global, 0, sizeof( m8d3d_global ) );
    m8d3d_global.dwSize = sizeof( m8d3d_global );
    c->dwSize = sizeof( *c );
    c->dwFlags = M8D3D_DD_COLORMODEL | M8D3D_DD_DEVCAPS | M8D3D_DD_TRICAPS |
                 M8D3D_DD_RENDERBITDEPTH | M8D3D_DD_ZBUFFERBITDEPTH;
    c->dcmColorModel = M8D3D_COLOR_RGB;
    /* The runtime wants all four even of a device that keeps no vertices. */
    c->dwDevCaps = M8D3D_DEVCAPS_FLOATTLVERTEX | M8D3D_DEVCAPS_EXECUTESYSTEMMEMORY |
                   M8D3D_DEVCAPS_TLVERTEXSYSTEMMEMORY | M8D3D_DEVCAPS_DRAWPRIMTLVERTEX;
    c->dtcTransformCaps[0] = sizeof( c->dtcTransformCaps );
    c->dlcLightingCaps[0] = sizeof( c->dlcLightingCaps );
    c->dpcLineCaps.dwSize = sizeof( c->dpcLineCaps );
    c->dpcTriCaps.dwSize = sizeof( c->dpcTriCaps );
    c->dpcTriCaps.dwMiscCaps = M8D3D_PMISCCAPS_CULLNONE;
    c->dpcTriCaps.dwShadeCaps = M8D3D_PSHADECAPS_COLORFLATRGB | M8D3D_PSHADECAPS_COLORGOURAUDRGB;
    c->dwDeviceRenderBitDepth = DDBD_8 | DDBD_16;
    c->dwDeviceZBufferBitDepth = 0;

    memset( &m8d3d_callbacks, 0, sizeof( m8d3d_callbacks ) );
    m8d3d_callbacks.dwSize = sizeof( m8d3d_callbacks );
    m8d3d_callbacks.ContextCreate = (LPVOID)m8d3d_context_create;
    m8d3d_callbacks.ContextDestroy = (LPVOID)m8d3d_context_destroy;
    m8d3d_callbacks.ContextDestroyAll = (LPVOID)m8d3d_context_destroy_all;
    m8d3d_callbacks.SceneCapture = (LPVOID)m8d3d_scene_capture;
    m8d3d_callbacks.RenderState = (LPVOID)m8d3d_render_state;
    m8d3d_callbacks.RenderPrimitive = (LPVOID)m8d3d_render_primitive;

    *global = (DWORD)&m8d3d_global;
    *callbacks = (DWORD)&m8d3d_callbacks;
}
