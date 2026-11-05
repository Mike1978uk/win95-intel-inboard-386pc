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
 * DirectX 7 removes 8 bpp from a hardware device's render depths (D3DIM.DLL
 * 56663de9), so this target needs a patched D3DIM: docs/next_session_2026_11_05.md.
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
#define M8_MIX_XOR          0x0005
#define M8_MIX_REPLACE      0x0007

/* d3dtypes.h */
#define M8D3D_OP_TRIANGLE   3
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
extern int  m8d3d_scratch( LPVOID key, int w, int h, int *x, int *y );
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
#define M8D3D_PSHADECAPS_COLORFLATRGB 0x00000008UL

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

typedef struct {
    LPVOID  target;             /* the surface's interface; 0 = a free context */
    DWORD   cull;
    int     sx;                 /* the scratch, as large as the target */
    int     sy;
    int     sw;
    int     sh;
    int     fresh;              /* the scratch has not been cleared yet */
    int     drawn;              /* triangles in the scratch since it was cleared */
    int     dl;                 /* their box, in target coordinates */
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
 * [9] culled, [10] refused: outside the target, [11] calls refused. */
DWORD                   m8d3d_stat[12];

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

/* One palette index for an RGB colour: scaffolding for a grey-ramp palette, until
 * the HAL takes its colours from the mapping 16 bpp uses. */
static BYTE m8d3d_index( DWORD rgb )
{
    return( (BYTE)((((rgb >> 16) & 0xFF) * 77 + ((rgb >> 8) & 0xFF) * 150 + (rgb & 0xFF) * 29) >> 8) );
}

/* The outline's pixels in one plane; an XOR, so where both edges fall on one pixel
 * of a row they cancel and the fill skips the row rather than running to its end. */
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

/* One triangle, v relative to the target at dx, dy and to the scratch. */
static int m8d3d_tri( M8D3D_CONTEXT *c, int dx, int dy, const short *v, BYTE colour )
{
    BYTE    plane = (BYTE)(1 << (c->drawn & 7));
    int     l = v[0];
    int     r = v[0];
    int     t = v[1];
    int     b = v[1];
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
    /* Rows strictly between top and bottom only: an apex row can hold one outline
     * pixel, and the fill would run to the end of the row. */
    if( b - t >= 2 ) {
        if( !m8d3d_outline( c->sx, c->sy, v, M8_MIX_XOR, M8_LINEDRAW_POLY | M8_LINEDRAW_LAST_OFF,
                            0x00FF, plane ) ) {
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
        outpw( M8_DESTX_DIASTP, c->sx + l );
        outpw( M8_DESTY_AXSTP, c->sy + t + 1 );
        outpw( M8_SRC_X_START, c->sx + l );
        outpw( M8_SRC_X_END, c->sx + r + 1 );
        outpw( M8_SRC_Y_DIR, 1 );
        outpw( M8_CUR_X, dx + l );
        outpw( M8_DEST_X_START, dx + l );
        outpw( M8_DEST_X_END, dx + r + 1 );
        outpw( M8_CUR_Y, dy + t + 1 );
        outpw( M8_DEST_Y_END, dy + b );         /* starts the fill */
        if( l < c->dl ) {
            c->dl = l;
        }
        if( t < c->dt ) {
            c->dt = t;
        }
        if( r + 1 > c->dr ) {
            c->dr = r + 1;
        }
        if( b + 1 > c->db ) {
            c->db = b + 1;
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
        ok = m8d3d_clear( c->sx + c->dl, c->sy + c->dt, c->sx + c->dr, c->sy + c->db );
    }
    c->drawn = 0;
    c->dl = c->dt = 0x7FFF;
    c->dr = c->db = 0;
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
    if( !m8d3d_surface( m8d3d_lcl( d->lpDDS ), &x, &y, &c->sw, &c->sh ) ||
        !m8d3d_scratch( c, c->sw, c->sh, &c->sx, &c->sy ) ) {
        return( DDHAL_DRIVER_HANDLED );
    }
    c->target = d->lpDDS;
    c->cull = M8D3D_CULL_CCW;
    c->fresh = 1;
    c->drawn = 0;
    c->dl = c->dt = 0;
    c->dr = c->sw;
    c->db = c->sh;
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

    ++m8d3d_stat[4];
    m8d3d_stat[5] += d->wCount;
    d->ddrval = DD_OK;
    if( d->bOpcode != M8D3D_OP_TRIANGLE ) {
        return( DDHAL_DRIVER_HANDLED );
    }
    /* A flip swaps the surfaces' memory, so the target is looked up each time. */
    if( c == NULL || exe == NULL || tl == NULL || d->bSize < sizeof( M8D3D_TRIANGLE ) ||
        !m8d3d_surface( m8d3d_lcl( c->target ), &x, &y, &w, &h ) || w > c->sw || h > c->sh ||
        !m8d3d_engine_begin() ) {
        ++m8d3d_stat[11];
        return( DDHAL_DRIVER_HANDLED );
    }
    /* A new context's scratch holds whatever the card left there; its box was set
     * to the whole scratch when it was created. */
    if( c->fresh ) {
        ok = m8d3d_scratch_clear( c );
        c->fresh = 0;
    }
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
        ok = m8d3d_tri( c, x, y, v, m8d3d_index( a[0]->color ) );
        ++m8d3d_stat[8];
        if( ok && c->drawn == 8 ) {
            ok = m8d3d_scratch_clear( c );
        }
    }
    if( ok ) {
        ok = m8d3d_scratch_clear( c );
    }
    m8d3d_engine_end();
    if( !ok ) {
        ++m8d3d_stat[11];
    }
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
    c->dpcTriCaps.dwShadeCaps = M8D3D_PSHADECAPS_COLORFLATRGB;
    c->dwDeviceRenderBitDepth = DDBD_8;
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
