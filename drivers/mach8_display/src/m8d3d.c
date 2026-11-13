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
 * target. No Z buffer: at 16 bpp a frame is sorted by depth before Flip draws it, so
 * ZBUFFERLESSHSR is claimed; triangles that cross each other can come out wrong.
 *
 * A program reads caps before it decides anything, so what it may ask for is claimed where the
 * answer is close enough or fails harmlessly: bilinear draws as nearest, perspective as affine,
 * blending as opaque until the engine's mixes are wired in. A texture stays in system memory, and for now reaches a
 * triangle as the colours under its corners, so the gradient path draws it. A triangle is a polygon fill: its outline
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
#define M8_FRGD_MIX         0xBAE8
#define M8_MIX_FRGD_REPLACE 0x0027
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
#define M8D3D_PT_TRIANGLELIST   4
#define M8D3D_PT_TRIANGLESTRIP  5
#define M8D3D_PT_TRIANGLEFAN    6
#define M8D3D_VT_TLVERTEX       3
#define M8D3D_GUID_CALLBACKS2_DATA1 0x0BA584E1UL
#define M8D3D_GUID_ZFORMATS_DATA1   0x93869880UL
#define M8D3D_GUID_EXTCAPS_DATA1    0x7DE41F80UL
#define M8D3D_RS_FOGCOLOR           34
#define M8D3D_PRASTERCAPS_FOGVERTEX 0x00000080UL
#define M8D3D_PSHADECAPS_ALPHAFLATBLEND 0x00001000UL
#define M8D3D_PSHADECAPS_ALPHAGOURAUDBLEND 0x00004000UL
#define M8D3D_PSHADECAPS_FOGFLAT    0x00040000UL
#define M8D3D_PSHADECAPS_FOGGOURAUD 0x00080000UL
#define M8D3D_PTEXTURECAPS_PERSPECTIVE 0x00000001UL
#define M8D3D_PTEXTURECAPS_ALPHA    0x00000004UL
#define M8D3D_PTEXTURECAPS_TRANSPARENCY 0x00000008UL
#define M8D3D_PTFILTERCAPS_LINEAR   0x00000002UL
#define M8D3D_PTFILTERCAPS_MIPNEAREST 0x00000004UL
#define M8D3D_PTFILTERCAPS_MIPLINEAR 0x00000008UL
#define M8D3D_PTBLENDCAPS_DECALALPHA 0x00000004UL
#define M8D3D_PTBLENDCAPS_MODULATEALPHA 0x00000008UL
#define M8D3D_PTBLENDCAPS_COPY      0x00000040UL
#define M8D3D_PTADDRESSCAPS_MIRROR  0x00000002UL
#define M8D3D_PTADDRESSCAPS_CLAMP   0x00000004UL
#define M8D3D_PBLENDCAPS_ZERO       0x00000001UL
#define M8D3D_PBLENDCAPS_ONE        0x00000002UL
#define M8D3D_PBLENDCAPS_SRCALPHA   0x00000010UL
#define M8D3D_PBLENDCAPS_INVSRCALPHA 0x00000020UL
#define M8D3D_TEXOPCAPS_FIXED       0x0000000FUL    /* disable, the two arguments, modulate */
#define M8D3D_DDPF_ZBUFFER          0x00000400UL
#define M8D3D_CB2_SETRENDERTARGET   0x00000001UL
#define M8D3D_CB2_DRAWONEPRIMITIVE  0x00000004UL
#define M8D3D_CB2_DRAWONEINDEXED    0x00000008UL
#define M8D3D_CB2_DRAWPRIMITIVES    0x00000010UL
#define M8D3D_RS_TEXTURE    1
#define M8D3D_RS_TEXBLEND   21
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
#define M8D3D_GBL_PITCH     24
#define M8D3D_GBL_HEIGHT    28          /* WORD */
#define M8D3D_GBL_WIDTH     30          /* WORD */
#define M8D3D_GBL_GMASK     60          /* ddpfSurface.dwGBitMask */
#define M8D3D_LCL_FLAGS     28
#define M8D3D_HASPIXELFORMAT 0x00002000UL
#define M8D3D_TEXBLEND_MODULATE 2
#define M8D3D_TEXBLEND_MODULATEALPHA 4

/* m8dd32.c: the engine and the card's memory, which it owns. */
extern int  m8d3d_surface( LPVOID lcl, int *x, int *y, int *w, int *h );
extern int  m8d3d_scratch( LPVOID key, int *w, int *h, int *x, int *y );
extern int  m8d3d_fb16( void );
extern int  m8d3d_record( LPVOID lcl, const short *v, BYTE index, WORD colour, const DWORD *rgb, int depth );
extern void m8d3d_scratch_free( LPVOID key );
extern void m8dd_trace( const char *fmt, ... );
extern const WORD *m8dd_tex_summary( LPVOID gbl, int *w, int *h, int *is555 );
extern int  m8dd_tex_rows;
extern int  m8d3d_engine_begin( void );
extern void m8d3d_engine_end( void );
extern int  m8d3d_room( int n );
extern int  m8d3d_room_take( int want );

/* To the nearest integer as the FPU rounds, with no library call. */
extern long m8d3d_round( float f );
#pragma aux m8d3d_round = "sub esp, 4" "fistp dword ptr [esp]" "pop eax" \
    parm [8087] value [eax] modify exact [eax];

/* The same from the float's bits, times 2^sh, without the 387: a load and a store on it cost
 * about what a multiply-add does here (~3 us). Halves round away from zero, where the FPU
 * rounds them to even. Saturates past 2^31; a NaN or infinity saturates too. */
static long m8d3d_fround( const float *p, int sh )
{
    long    b = *(const long *)p;
    int     e = (int)((b >> 23) & 255) + sh;
    long    m;

    if( e < 126 ) {
        return( 0 );
    }
    if( e > 157 ) {
        return( b < 0 ? -0x7FFFFFFFL : 0x7FFFFFFFL );
    }
    m = (b & 0x007FFFFFL) | 0x00800000L;
    m = e >= 150 ? m << (e - 150) : (m + (1L << (149 - e))) >> (150 - e);
    return( b < 0 ? -m : m );
}

/* The float bits of n, 0 <= n < 2^24. */
static DWORD m8d3d_ibits( long n )
{
    int e = 0;

    if( n <= 0 ) {
        return( 0 );
    }
    while( (n >> e) > 1 ) {
        ++e;
    }
    return( (DWORD)(127 + e) << 23 | ((DWORD)n << (23 - e) & 0x007FFFFFUL) );
}

/* A float's bits as a number that sorts as the float does. */
static long m8d3d_ford( const float *p )
{
    long b = *(const long *)p;

    return( b < 0 ? (long)(0x80000000UL - (DWORD)b) : b );
}

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
#define M8D3D_DEVCAPS_TEXTURESYSTEMMEMORY   0x00000100UL
#define M8D3D_DEVCAPS_TEXTUREVIDEOMEMORY    0x00000200UL
#define M8D3D_DEVCAPS_DRAWPRIMTLVERTEX      0x00000400UL
#define M8D3D_DEVCAPS_CANRENDERAFTERFLIP    0x00000800UL
#define M8D3D_PMISCCAPS_CULLNONE    0x00000010UL
#define M8D3D_PSHADECAPS_COLORFLATRGB 0x00000002UL
#define M8D3D_PSHADECAPS_COLORGOURAUDRGB 0x00000008UL
#define M8D3D_PRASTERCAPS_ZTEST     0x00000010UL
#define M8D3D_PRASTERCAPS_ZBUFFERLESSHSR 0x00008000UL
#define M8D3D_PMISCCAPS_MASKZ       0x00000002UL
#define M8D3D_PCMPCAPS_NEVER        0x00000001UL
#define M8D3D_PCMPCAPS_LESS         0x00000002UL
#define M8D3D_PCMPCAPS_LESSEQUAL    0x00000008UL
#define M8D3D_PCMPCAPS_ALWAYS       0x00000080UL
#define M8D3D_RS_ZFUNC              23
#define M8D3D_CMP_ALWAYS            8
#define M8D3D_PTEXTURECAPS_POW2     0x00000002UL
#define M8D3D_PTFILTERCAPS_NEAREST  0x00000001UL
#define M8D3D_PTBLENDCAPS_DECAL     0x00000001UL
#define M8D3D_PTBLENDCAPS_MODULATE  0x00000002UL
#define M8D3D_PTADDRESSCAPS_WRAP    0x00000001UL

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

/* The DX5 route: D3DHAL_CALLBACKS2 and its calls, which GetDriverInfo hands over. */
typedef struct {
    DWORD   dwSize;
    DWORD   dwFlags;
    LPVOID  SetRenderTarget;
    LPVOID  Clear;
    LPVOID  DrawOnePrimitive;
    LPVOID  DrawOneIndexedPrimitive;
    LPVOID  DrawPrimitives;
} M8D3D_CALLBACKS2;

/* D3DHAL_D3DEXTENDEDCAPS7; earlier runtimes ask for a shorter front of it, and dwSize says how much. */
typedef struct {
    DWORD   dwSize;
    DWORD   dwMinTextureWidth;
    DWORD   dwMaxTextureWidth;
    DWORD   dwMinTextureHeight;
    DWORD   dwMaxTextureHeight;
    DWORD   dwMinStippleWidth;
    DWORD   dwMaxStippleWidth;
    DWORD   dwMinStippleHeight;
    DWORD   dwMaxStippleHeight;
    DWORD   dwMaxTextureRepeat;
    DWORD   dwMaxTextureAspectRatio;
    DWORD   dwMaxAnisotropy;
    float   dvGuardBandLeft;
    float   dvGuardBandTop;
    float   dvGuardBandRight;
    float   dvGuardBandBottom;
    float   dvExtentsAdjust;
    DWORD   dwStencilCaps;
    DWORD   dwFVFCaps;
    DWORD   dwTextureOpCaps;
    WORD    wMaxTextureBlendStages;
    WORD    wMaxSimultaneousTextures;
    DWORD   dwMaxActiveLights;
    float   dvMaxVertexW;
    WORD    wMaxUserClipPlanes;
    WORD    wMaxVertexBlendMatrices;
    DWORD   dwVertexProcessingCaps;
    DWORD   dwReserved[4];
} M8D3D_EXTCAPS;

/* A count, then that many DDPIXELFORMATs: size, flags, FourCC, Z bits, stencil bits, Z mask,
 * stencil mask, RGB-Z mask. */
typedef struct {
    DWORD   count;
    DWORD   format[8];
} M8D3D_ZFORMATS;

typedef char m8d3d_zformats_size[sizeof( M8D3D_ZFORMATS ) == 36 ? 1 : -1];

typedef struct {
    DWORD   dwSize;
    DWORD   dwFlags;
    BYTE    guidInfo[16];
    DWORD   dwExpectedSize;
    LPVOID  lpvData;
    DWORD   dwActualSize;
    HRESULT ddRVal;
} M8D3D_DRIVERINFO;

typedef struct {
    DWORD   dwhContext;
    DWORD   dwFlags;
    DWORD   PrimitiveType;
    DWORD   VertexType;
    LPVOID  lpvVertices;
    DWORD   dwNumVertices;
    DWORD   dwReserved;
    HRESULT ddrval;
} M8D3D_DRAWONE;

typedef struct {
    DWORD   dwhContext;
    DWORD   dwFlags;
    DWORD   PrimitiveType;
    DWORD   VertexType;
    LPVOID  lpvVertices;
    DWORD   dwNumVertices;
    WORD    *lpwIndices;
    DWORD   dwNumIndices;
    HRESULT ddrval;
} M8D3D_DRAWONEINDEXED;

typedef struct {
    DWORD   dwhContext;
    DWORD   dwFlags;
    LPVOID  lpvData;
    DWORD   dwReserved;
    HRESULT ddrval;
} M8D3D_DRAWPRIMITIVES;

typedef struct {
    WORD    wNumStateChanges;
    WORD    wPrimitiveType;
    WORD    wVertexType;
    WORD    wNumVertices;
} M8D3D_COUNTS;

typedef struct {
    DWORD   dwhContext;
    LPVOID  lpDDS;
    LPVOID  lpDDSZ;
    HRESULT ddrval;
} M8D3D_SETTARGET;

typedef struct {
    DWORD   dwhContext;
    LPVOID  lpDDS;
    DWORD   dwHandle;
    HRESULT ddrval;
} M8D3D_TEXTURECREATE;

typedef struct {
    DWORD   dwhContext;
    DWORD   dwHandle;
    HRESULT ddrval;
} M8D3D_TEXTUREDESTROY;

typedef struct {
    DWORD   dwhContext;
    DWORD   dwHandle1;
    DWORD   dwHandle2;
    HRESULT ddrval;
} M8D3D_TEXTURESWAP;

typedef struct {
    DWORD   dwhContext;
    LPVOID  lpDDS;
    DWORD   dwHandle;
    HRESULT ddrval;
} M8D3D_TEXTUREGETSURF;

typedef struct {
    DWORD   type;
    DWORD   arg;
} M8D3D_STATE;

typedef char m8d3d_tl_size[sizeof( M8D3D_TLVERTEX ) == 32 ? 1 : -1];

#define M8D3D_CONTEXTS  4
#define M8D3D_TEXTURES  128            /* Monster Truck Madness asks for at least 96 */
/* Below this on its longer side a triangle shows little of a gradient, and a gradient costs
 * several commands where a flat triangle costs a stamp. */
#define M8D3D_GRAD_MIN  32
#define M8D3D_CLIP_MAX  8               /* a triangle gains at most one corner a side */
#define M8D3D_DEPTH_SCALE   341.0f      /* three corners' z, 0 to 1 each, to the 0-1023 of the sort */

typedef struct {
    LPVOID  dds;                /* NULL = free */
    long    pitch;
    int     w;
    int     h;
    int     is555;
    int     scanned;            /* its colours are in m8d3d_pal_base */
} M8D3D_TEX;

static M8D3D_TEX        m8d3d_texs[M8D3D_TEXTURES];
static int              m8d3d_pal_want;     /* a frame was drawn in the texture palette's indices */
static int              m8d3d_pal_on;       /* the DAC holds it, the old one in m8d3d_pal_saved */
static int              m8d3d_lores;        /* trial: 640x480 drawn at half size, shown as 320x240 */
static int              m8d3d_half = 1;     /* corners rounded at half size: [d3d] lores, per program */
static int              m8d3d_zsort;        /* sort by depth with Z off: [d3d] zsort, per program */
static int              m8d3d_pmatch;       /* flat colours in the ramp palette: [d3d] palette, per program */
static int              m8d3d_pal_dirty;    /* a base added since the DAC was loaded */
static int              m8d3d_pal_match( DWORD rgb, DWORD *shown );
static int              m8d3d_pal_kept;     /* the desktop's DAC is in m8d3d_pal_saved from a program that ended without restoring it */
static void             m8d3d_lores_set( void );
static void             m8d3d_pal_load( void );
static void             m8d3d_pal_restore( void );

/* What programs ask of this interface, across every context until restart, so real programs
 * can rank what to build: docs/mach8_frame_rate_plan.md, the census. A few additions a call. */
#define M8D3D_CENSUS_MARK   0x4E43384DUL    /* "M8CN" */
#define M8D3D_RS_ZENABLE        7
#define M8D3D_RS_ALPHATEST      15
#define M8D3D_RS_ALPHABLEND     27
#define M8D3D_RS_FOG            28
#define M8D3D_RS_PERSPECTIVE    4
#define M8D3D_RS_COLORKEY       41

typedef struct {
    DWORD   mark;
    DWORD   state_hits[100];        /* by state type; 99 = any above */
    DWORD   state_values[100];      /* one bit for each value set, 31 = any above */
    DWORD   state_blocks[8];        /* DrawPrimitives' pairs a block, by log2 */
    DWORD   calls_exec;
    DWORD   calls_one;
    DWORD   calls_indexed;
    DWORD   calls_prims;
    DWORD   prim_types[8];
    DWORD   other_ops;              /* RenderPrimitive's other than triangles */
    DWORD   tris;
    DWORD   tris_textured;
    DWORD   tris_z;
    DWORD   tris_blend;
    DWORD   tris_alphatest;
    DWORD   tris_colorkey;
    DWORD   tris_fog;
    DWORD   tris_perspective;
    DWORD   tris_clipped;
    DWORD   tri_sizes[10];          /* bounding box's longer side, by log2 */
    DWORD   batch_sizes[10];        /* triangles a call, by log2 */
    DWORD   contexts;
    DWORD   contexts_z;
    DWORD   targets_set;
    DWORD   tex_created;
    DWORD   tex_sizes[12];          /* longer side, by log2 */
    DWORD   info_data1[16];         /* GetDriverInfo, by the GUID's first DWORD */
    DWORD   info_hits[16];
} M8D3D_CENSUS;

M8D3D_CENSUS            m8d3d_census = { M8D3D_CENSUS_MARK };

static int m8d3d_log2( DWORD n, int top )
{
    int k = 0;

    while( n > 1 && k < top ) {
        n >>= 1;
        ++k;
    }
    return( k );
}
static DDSURFACEDESC    m8d3d_tex_formats[2];

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
    DWORD   tex;                /* texture handle, 0 = none */
    DWORD   texblend;
    DWORD   rs_on;              /* which of the census's states are set */
    DWORD   zfunc;
    DWORD   fogcolor;
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
    int     jclean;             /* the joined path's scratch holds 0 in every plane */
} M8D3D_CONTEXT;

static M8D3D_GLOBAL     m8d3d_global;
static M8D3D_CALLBACKS  m8d3d_callbacks;
static M8D3D_CALLBACKS2 m8d3d_callbacks2;
/* Programs ask for this list whether or not they use a Z buffer, and some stop if it is empty.
 * Depth comes from the sort at Flip: ZBUFFERLESSHSR tells those who check that no surface is needed. */
/* Corners far off the target are ours to cut: m8d3d_draw_clipped. */
static const M8D3D_EXTCAPS m8d3d_extcaps = {
    0, 1, 256, 1, 256, 0, 0, 0, 0, 256, 256, 1,
    -32768.0f, -32768.0f, 32767.0f, 32767.0f, 0.0f,
    0, 0, M8D3D_TEXOPCAPS_FIXED, 1, 1,
    0, 1.0e10f, 0, 0, 0, { 0, 0, 0, 0 }
};
static const M8D3D_ZFORMATS m8d3d_zformats = { 1, { 32, M8D3D_DDPF_ZBUFFER, 0, 16, 0, 0xFFFF, 0, 0 } };
static M8D3D_CONTEXT    m8d3d_ctx[M8D3D_CONTEXTS];

/* Exported so a test program can see which callbacks the runtime reached: a
 * failed CreateDevice does not say. [0] ContextCreate, [1] ContextDestroy,
 * [2] SceneCapture, [3] RenderState, [4] RenderPrimitive, [5] primitives offered,
 * [6] the last target surface, [7] the last Z surface, [8] triangles drawn,
 * [9] culled, [10] refused: outside the target, [11] calls refused. Performance
 * counter ticks in Flip at 16 bpp: [12] all, [13] the 16-bit side, [14] erase and
 * triangles; [15] flips; in Blt: [16] ticks, [17] calls, [18] full clears described;
 * [19] page wait ticks; [20] triangles drawn from a mask; [21] masks built;
 * [22] triangles stamped; [23] stamps built; [24] ticks in RenderPrimitive.
 * [84] textures placed on the card, [85] texels sent to it. At 16 bpp: [86] writable Locks of a
 * back buffer, [87] their rows; system-memory copies over a frame's triangles: [88] kept as
 * overlays, [89] keyed and [90] plain that sent the frame through RAM; frames Locked by the program:
 * [91] HUD found and kept as overlays, sent the slow way as [92] triangles came after the Lock,
 * [93] the HUD too large or no room. [94] at 8 bpp, Locks that needed no read-back.
 * At 8 bpp, for M8MON: [72] Locks, [73] bytes read back for them, [74] Unlocks, [75] bytes
 * written back, [76] ticks moving them, [77] Blts taken, [78-80] pixels filled, copied on the
 * card, sent from RAM, [81] Blts refused, [82] Flips, [83] Locks of the whole surface. */
DWORD                   m8d3d_stat[96];      /* 62-64: triangles put in RAM: on the shown buffer, on a buffer not described, flushes of recorded ones; 65: triangles on a surface not ours; 66: back-buffer part clears described as nothing; 67-68: overlays drawn by the card, sent to it; 60: feeds cut short by the 16-bit driver; 61: back-buffer locks that kept the triangles; 25: engine writes sent, 26: triangle pieces drawn, 27-33: m8dd_tri_census, 34-35: FIFO waits, 36-43: Flip phases, 44: sending, 45: sends forced by a full queue, 46-53: of 36-43, the part spent sending,
                                              * 54-59: feed: opened, frames handed over, entries left for m8d3d_feed_finish, driver time-outs, FIFO-full reads, entries sent */
/* Diagnostic: clock ticks since the last mark, into phase k. */
static LARGE_INTEGER    m8d3d_ph;
static DWORD            m8d3d_ph_send;
#define M8D3D_PH( k ) do { LARGE_INTEGER t_; QueryPerformanceCounter( &t_ ); m8d3d_stat[k] += t_.LowPart - m8d3d_ph.LowPart; m8d3d_ph = t_; \
                           m8d3d_stat[(k) + 10] += m8d3d_stat[44] - m8d3d_ph_send; m8d3d_ph_send = m8d3d_stat[44]; } while( 0 )
/* Flip off its fast path: GDI touched the card, slow flips, then why. */
DWORD                   m8d3d_flip_why[6];
/* Feeds stopped while still running, by the call that stopped them: m8d3d_feed_stop's site. */
DWORD                   m8d3d_feed_why[16];

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

/* For M8DD32's overlays: a 5-6-5 pixel's entry, as a flat triangle's. */
BYTE m8d3d_index565( WORD p )
{
    DWORD r = (p >> 11) & 0x1F;
    DWORD g = (p >> 5) & 0x3F;
    DWORD b = p & 0x1F;
    DWORD rgb = ((r << 3 | r >> 2) << 16) | ((g << 2 | g >> 4) << 8) | (b << 3 | b >> 2);
    DWORD shown;
    int   k;

    if( m8d3d_pmatch && (k = m8d3d_pal_match( rgb, &shown )) >= 0 ) {
        m8d3d_pal_want = 1;
        return( (BYTE)k );
    }
    return( m8d3d_index16( rgb ) );
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

/* Any port access empties the CPU's cache here, so C run between OUTs runs from RAM: a triangle's
 * lookups cost ~11 us after one another and ~150 us with an OUT between. A frame's engine writes
 * are worked out with the cache intact and kept here in order. Outside Flip the engine is topped
 * up from the front without waiting (m8d3d_q_pump); a wait comes only when the queue is full. */
#define M8D3D_Q_MAX     8192            /* Flip's whole frame, ~6,500 writes at 320x240 */
/* Held writes before the FIFO is looked at again: each look is a status read and empties the cache. */
#define M8D3D_Q_PUMP    256
/* Room for this many, not the FIFO's whole 16: waiting for it to empty leaves the engine idle when
 * a long fill ends, with the next command still crossing the bus. */
#define M8D3D_Q_CHUNK   8
#define M8D3D_Q_ONE     32              /* the most one triangle adds */
/* A box this large keeps the engine busy longer than the next triangle's C takes, even from RAM:
 * sent at once, that C runs while the engine draws, where held it would leave the engine idle. */
#define M8D3D_Q_SEND_PX 1500L

static DWORD    m8d3d_q_own[M8D3D_Q_MAX];
static DWORD    *m8d3d_q = m8d3d_q_own;
static int      m8d3d_q_cap = M8D3D_Q_MAX;
static int      m8d3d_qn;               /* held */
static int      m8d3d_qh;               /* sent, from the front */
static int      m8d3d_q_next;           /* m8d3d_qn at which to look again */
/* Flip's frame is worked out whole before any of it is sent: every status read and every OUT
 * empties the cache, so sends between passes left the next pass running from RAM. */
static int      m8d3d_q_hold;
static int m8d3d_nohold;                /* [diag] nohold=1: sends between passes */

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

/* Held writes up to `end` sent, waiting for FIFO room as needed. */
static int m8d3d_q_send_to( int end )
{
    int             i;
    int             ok = 1;
    LARGE_INTEGER   t0;
    LARGE_INTEGER   t1;

    QueryPerformanceCounter( &t0 );
    i = m8d3d_qh;
    while( i < end ) {
        int n = end - i < M8D3D_Q_CHUNK ? end - i : M8D3D_Q_CHUNK;

        if( !m8d3d_room( n ) ) {
            ok = 0;
            break;
        }
        m8d3d_q_send( m8d3d_q + i, n );
        m8d3d_stat[25] += n;
        i += n;
    }
    m8d3d_qh = i;
    QueryPerformanceCounter( &t1 );
    m8d3d_stat[44] += t1.LowPart - t0.LowPart;
    return( ok );
}

static int m8d3d_q_send_all( void )
{
    int             i = 0;
    int             ok = 1;
    LARGE_INTEGER   t0;
    LARGE_INTEGER   t1;

    QueryPerformanceCounter( &t0 );
    i = m8d3d_qh;
    while( i < m8d3d_qn ) {
        int n = m8d3d_qn - i < M8D3D_Q_CHUNK ? m8d3d_qn - i : M8D3D_Q_CHUNK;

        if( !m8d3d_room( n ) ) {
            ok = 0;
            break;
        }
        m8d3d_q_send( m8d3d_q + i, n );
        m8d3d_stat[25] += n;
        i += n;
    }
    m8d3d_qn = 0;
    m8d3d_qh = 0;
    m8d3d_q_next = M8D3D_Q_PUMP;
    QueryPerformanceCounter( &t1 );
    m8d3d_stat[44] += t1.LowPart - t0.LowPart;
    return( ok );
}

static int m8d3d_q_flush( void )
{
    return( m8d3d_q_hold ? 1 : m8d3d_q_send_all() );
}

/* As many held writes as the FIFO takes now, with one status read and no wait. */
static void m8d3d_q_pump( void )
{
    int n;

    if( m8d3d_q_hold || m8d3d_qn < m8d3d_q_next ) {
        return;
    }
    m8d3d_q_next = m8d3d_qn + M8D3D_Q_PUMP;
    n = m8d3d_room_take( m8d3d_qn - m8d3d_qh );
    if( n > 0 ) {
        m8d3d_q_send( m8d3d_q + m8d3d_qh, n );
        m8d3d_stat[25] += n;
        m8d3d_qh += n;
    }
}

/* Room for n more writes: the engine topped up, and what is held sent if the queue is full. */
static int m8d3d_q_need( int n )
{
    if( m8d3d_qn + n + M8D3D_Q_ONE * 2 > m8d3d_q_cap ) {
        ++m8d3d_stat[45];
        return( m8d3d_q_send_all() );
    }
    m8d3d_q_pump();
    return( 1 );
}

/* The card fed in the background (M8FEED.VXD): the 16-entry FIFO, no DMA and no engine interrupt
 * mean the card draws only while the CPU writes, and nothing of ours runs while the program does.
 * Flip sends its frame but the last m8d3d_feed_tail writes, leaves those in the driver's queue and
 * returns; the driver's 1 ms time-out tops the FIFO up. Every entry into this DLL that may touch the card calls m8d3d_feed_finish first, so the
 * card is never written from two places. Without the VxD, Flip sends as before. */
#define M8FEED_INIT     1
#define M8FEED_START    2
#define M8FEED_STOP     3
#define M8FEED_HEAD     0               /* DWORDs of the queue's header */
#define M8FEED_TAIL     1
#define M8FEED_STATE    2
#define M8FEED_BLANK    3
#define M8FEED_SHOWMS   4
#define M8FEED_TICKS    5
#define M8FEED_FULL     6
#define M8FEED_SENT     7
#define M8FEED_FIRST    16
#define M8FEED_CAP      (8 * 4096 / 4 - M8FEED_FIRST)
#define M8FEED_MARK     0               /* port 0: the driver reads the blank latch and the time */

extern void *m8d3d_hot( void );
extern void m8d3d_room_forget( void );
extern void m8d3d_feed_lost( void );
extern void m8d3d_feed_hand( void );

static HANDLE   m8d3d_feed_h = INVALID_HANDLE_VALUE;
static DWORD    *m8d3d_feed;            /* the driver's queue, header first */
static int      m8d3d_feeding;          /* a frame handed over, not yet finished */
static int      m8d3d_feed_held;        /* Flip's frame left in the queue, engine begun */
/* Writes left for the driver: the frame's last fills keep the 16-entry FIFO busy about a tick each,
 * where its lines and register writes would leave the engine idle between ticks. */
static int      m8d3d_feed_tail = 4000;
int             m8d3d_feed_blank;       /* blank latch read after the last page show */
DWORD           m8d3d_feed_showms;      /* and the system time then */

/* This DLL's data is shared by every process, and the feed's handle, the queue it maps, the
 * 320x240 mode and the palette belong to the process that set them up. A program that ends
 * without destroying its context leaves them set: its handle is closed and the driver unloads
 * with the queue, so the next program must start them again, not use them. */
static DWORD    m8d3d_owner;

static void m8d3d_owner_check( void )
{
    DWORD pid = GetCurrentProcessId();

    if( pid == m8d3d_owner ) {
        return;
    }
    m8d3d_owner = pid;
    /* The last owner may have ended mid-feed, its frame still being sent by the driver's tick.
     * The queue is in the system arena, seen by every process, and the tick stops on any state
     * but 1: stopped here, before this process writes the card. Freed if the driver unloaded. */
    if( m8d3d_feed != NULL && !IsBadWritePtr( m8d3d_feed, 4 * (M8FEED_STATE + 1) ) &&
        m8d3d_feed[M8FEED_STATE] == 1 ) {
        m8d3d_feed[M8FEED_STATE] = 3;
        ((BYTE *)m8d3d_hot())[12] = 1;
        m8d3d_room_forget();
        m8d3d_engine_end();
    }
    m8d3d_feed = NULL;
    m8d3d_feed_h = INVALID_HANDLE_VALUE;
    m8d3d_feeding = 0;
    m8d3d_feed_held = 0;
    m8d3d_q = m8d3d_q_own;
    m8d3d_q_cap = M8D3D_Q_MAX;
    m8d3d_qn = 0;
    m8d3d_qh = 0;
    if( m8d3d_pal_on ) {
        m8d3d_pal_kept = 1;
    }
    m8d3d_pal_on = 0;
    m8d3d_lores = 0;
}

static void m8d3d_feed_open( void )
{
    DWORD   hot;
    DWORD   q = 0;
    DWORD   got = 0;

    /* [feed] on=0 turns it off. Each time-out empties the cache, the program's too, and between ticks
     * a 16-entry FIFO of lines runs dry: an engine-idle interrupt (8514 IGPIDLE) would pace it
     * better, but no 8-bit IRQ is free on the 5160. */
    if( m8d3d_feed != NULL || !GetPrivateProfileInt( "feed", "on", 1, "C:\\M8D3D.INI" ) ) {
        return;
    }
    m8d3d_feed_h = CreateFileA( "\\\\.\\M8FEED.VXD", 0, 0, NULL, 0, FILE_FLAG_DELETE_ON_CLOSE, NULL );
    if( m8d3d_feed_h == INVALID_HANDLE_VALUE ) {
        return;
    }
    hot = (DWORD)m8d3d_hot();
    if( !DeviceIoControl( m8d3d_feed_h, M8FEED_INIT, &hot, 4, &q, 4, &got, NULL ) || q == 0 ) {
        CloseHandle( m8d3d_feed_h );
        m8d3d_feed_h = INVALID_HANDLE_VALUE;
        return;
    }
    m8d3d_feed = (DWORD *)q;
    m8d3d_stat[54] = 1;
    m8d3d_feed_tail = (int)GetPrivateProfileInt( "feed", "tail", 4000, "C:\\M8D3D.INI" );
    m8d3d_feed_tail = m8d3d_feed_tail < 3 ? 3 : m8d3d_feed_tail;
}

/* Appends one write to the frame Flip left in the queue. */
void m8d3d_q_add( unsigned port, unsigned value )
{
    if( m8d3d_qn < m8d3d_q_cap ) {
        M8D3D_Q( port, value );
    }
}

/* Hands Flip's frame to the driver, or sends it here without one. */
void m8d3d_feed_go( void )
{
    DWORD got;

    if( !m8d3d_feed_held ) {
        return;
    }
    m8d3d_feed_held = 0;
    if( m8d3d_q == m8d3d_q_own ) {
        m8d3d_q_send_all();
        m8d3d_engine_end();
        return;
    }
    if( m8d3d_qn - m8d3d_feed_tail > m8d3d_qh ) {
        m8d3d_q_send_to( m8d3d_qn - m8d3d_feed_tail );
    }
    m8d3d_feed[M8FEED_HEAD] = m8d3d_qh;
    m8d3d_feed[M8FEED_TAIL] = m8d3d_qn;
    m8d3d_feeding = 1;
    ++m8d3d_stat[55];
    m8d3d_room_forget();
    m8d3d_feed_hand();
    DeviceIoControl( m8d3d_feed_h, M8FEED_START, NULL, 0, NULL, 0, &got, NULL );
}

/* The feed stopped and what it left sent: the card is ours again, the engine idle-able. */
void m8d3d_feed_finish( void )
{
    DWORD   head = 0;
    DWORD   got;
    int     lost;
    int     cut;

    if( m8d3d_feed_held ) {
        m8d3d_feed_go();
    }
    if( !m8d3d_feeding ) {
        return;
    }
    m8d3d_feeding = 0;
    DeviceIoControl( m8d3d_feed_h, M8FEED_STOP, NULL, 0, &head, 4, &got, NULL );
    lost = m8d3d_feed[M8FEED_STATE] == 2 || ((BYTE *)m8d3d_hot())[11] != 0;
    cut = !lost && (m8d3d_feed[M8FEED_STATE] == 3 || ((BYTE *)m8d3d_hot())[12] != 2);
    ((BYTE *)m8d3d_hot())[12] = 1;
    m8d3d_feed[M8FEED_STATE] = 0;
    m8d3d_room_forget();
    m8d3d_stat[56] += m8d3d_qn - head;
    m8d3d_stat[57] += m8d3d_feed[M8FEED_TICKS];
    m8d3d_stat[58] += m8d3d_feed[M8FEED_FULL];
    m8d3d_stat[59] += m8d3d_feed[M8FEED_SENT];
    m8d3d_feed[M8FEED_TICKS] = 0;
    m8d3d_feed[M8FEED_FULL] = 0;
    m8d3d_feed[M8FEED_SENT] = 0;
    m8d3d_feed_blank = (int)m8d3d_feed[M8FEED_BLANK];
    m8d3d_feed_showms = m8d3d_feed[M8FEED_SHOWMS];
    if( cut ) {
        /* The 16-bit driver used the engine since the hand-over: the rest of the frame was
         * queued for registers it has since rewritten. */
        ++m8d3d_stat[60];
        m8d3d_qn = 0;
        m8d3d_qh = 0;
        m8d3d_q_next = M8D3D_Q_PUMP;
        m8d3d_engine_end();
        return;
    }
    if( lost ) {
        /* A DOS box took the screen: what is left must not reach the card. */
        m8d3d_qn = 0;
        m8d3d_qh = 0;
        m8d3d_feed_lost();
        return;
    }
    /* The rest here; a marker is read, never sent: port 0 is the DMA controller's. */
    m8d3d_stat[25] += head;
    for( ; (int)head < m8d3d_qn; ++head ) {
        DWORD e = m8d3d_q[head];

        if( (WORD)e == M8FEED_MARK ) {
            m8d3d_feed_blank = inpw( 0x42E8 ) & 1;
            m8d3d_feed_showms = GetTickCount();
            continue;
        }
        if( !m8d3d_room( 1 ) ) {
            break;
        }
        outpw( (WORD)e, (WORD)(e >> 16) );
        ++m8d3d_stat[25];
    }
    m8d3d_qn = 0;
    m8d3d_qh = 0;
    m8d3d_q_next = M8D3D_Q_PUMP;
    m8d3d_engine_end();
}

/* m8d3d_feed_finish, counted against `site` when a feed was still running. */
void m8d3d_feed_stop( int site )
{
    if( m8d3d_feeding || m8d3d_feed_held ) {
        ++m8d3d_feed_why[site & 15];
    }
    m8d3d_feed_finish();
}

/* The feed closed: the driver unloads with its last handle and frees the queue. */
void m8d3d_feed_close( void )
{
    m8d3d_feed_finish();
    if( m8d3d_feed_h != INVALID_HANDLE_VALUE ) {
        CloseHandle( m8d3d_feed_h );
        m8d3d_feed_h = INVALID_HANDLE_VALUE;
    }
    m8d3d_feed = NULL;
    m8d3d_q = m8d3d_q_own;
    m8d3d_q_cap = M8D3D_Q_MAX;
}

/* A plain fill of l, t .. r - 1, b - 1 in the colour and masks already set. */
static int m8d3d_q_rect( int l, int t, int r, int b )
{
    if( r <= l || b <= t ) {
        return( 1 );
    }
    if( !m8d3d_q_need( 5 ) ) {
        return( 0 );
    }
    M8D3D_Q( M8_CUR_X, l );
    M8D3D_Q( M8_CUR_Y, t );
    M8D3D_Q( M8_MAJ_AXIS_PCNT, r - l - 1 );
    M8D3D_Q( M8_MIN_AXIS, b - t - 1 );
    M8D3D_Q( M8_CMD, M8_CMD_FILL );
    return( 1 );
}

/* Black over a card rectangle, leaving the plain path set up. */
static int m8d3d_q_clear( int l, int t, int r, int b )
{
    if( !m8d3d_q_need( 5 ) ) {
        return( 0 );
    }
    M8D3D_Q( M8_WRT_MASK, 0x00FF );
    M8D3D_Q( M8_RD_MASK, 0x00FF );
    M8D3D_Q( M8_DP_CONFIG, M8_DP_8514 );
    M8D3D_Q( M8_FRGD_MIX, M8_MIX_FRGD_REPLACE );
    M8D3D_Q( M8_FRGD_COLOR, 0 );
    return( m8d3d_q_rect( l, t, r, b ) );
}

/* The outline over the fill, which supplies its top and bottom rows and the rows an
 * apex emptied. */
static int m8d3d_outline( int ox, int oy, const short *v, WORD alu, WORD opt, WORD colour, WORD mask )
{
    int i;

    if( !m8d3d_q_need( 14 ) ) {
        return( 0 );
    }
    M8D3D_Q( M8_WRT_MASK, mask );
    M8D3D_Q( M8_FRGD_COLOR, colour );
    M8D3D_Q( M8_DP_CONFIG, M8_DP_8514 );
    M8D3D_Q( M8_ALU_FG_FN, alu );
    M8D3D_Q( M8_LINEDRAW_OPT, opt );
    M8D3D_Q( M8_LINEDRAW_INDEX, 0 );
    for( i = 0; i < 4; ++i ) {
        M8D3D_Q( M8_LINEDRAW, ox + v[2 * (i % 3)] );      /* the last closes the figure */
        M8D3D_Q( M8_LINEDRAW, oy + v[2 * (i % 3) + 1] );
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

    if( !m8d3d_q_need( 6 ) ) {
        return( 0 );
    }
    M8D3D_Q( M8_WRT_MASK, plane );
    M8D3D_Q( M8_FRGD_COLOR, 0x00FF );
    M8D3D_Q( M8_DP_CONFIG, M8_DP_8514 );
    M8D3D_Q( M8_ALU_FG_FN, M8_MIX_XOR );
    M8D3D_Q( M8_LINEDRAW_OPT, M8_LINEDRAW_POLY | M8_LINEDRAW_LAST_OFF );
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
        if( !m8d3d_q_need( 5 ) ) {
            return( 0 );
        }
        M8D3D_Q( M8_LINEDRAW_INDEX, 0 );
        M8D3D_Q( M8_LINEDRAW, ox + v[a] );
        M8D3D_Q( M8_LINEDRAW, oy + v[a + 1] );
        M8D3D_Q( M8_LINEDRAW, ox + v[b] );
        M8D3D_Q( M8_LINEDRAW, oy + v[b + 1] );
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
    if( !m8d3d_q_need( 4 ) ) {
        return( 0 );
    }
    M8D3D_Q( M8_WRT_MASK, 0x00FF );
    M8D3D_Q( M8_FRGD_COLOR, colour );
    M8D3D_Q( M8_DP_CONFIG, M8_DP_8514 );
    M8D3D_Q( M8_ALU_FG_FN, M8_MIX_REPLACE );
    for( y = 0; y <= b - t; ++y ) {
        if( y % 5 == 0 && !m8d3d_q_need( 15 ) ) {
            return( 0 );
        }
        M8D3D_Q( M8_CUR_Y, dy + t + y );
        M8D3D_Q( M8_CUR_X, dx + lo[y] );
        M8D3D_Q( M8_SCAN_TO_X, dx + hi[y] + 1 );
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
        if( !m8d3d_q_need( 15 ) ) {
            return( 0 );
        }
        M8D3D_Q( M8_WRT_MASK, 0x00FF );
        M8D3D_Q( M8_RD_MASK, plane );
        M8D3D_Q( M8_DP_CONFIG, M8_DP_POLY_FILL );
        M8D3D_Q( M8_ALU_FG_FN, M8_MIX_REPLACE );
        M8D3D_Q( M8_FRGD_COLOR, colour );
        M8D3D_Q( M8_DESTX_DIASTP, sx + l );
        M8D3D_Q( M8_DESTY_AXSTP, sy + t + 1 );
        M8D3D_Q( M8_SRC_X_START, sx + l );
        M8D3D_Q( M8_SRC_X_END, sx + r + 1 );
        M8D3D_Q( M8_SRC_Y_DIR, 1 );
        M8D3D_Q( M8_CUR_X, dx + l );
        M8D3D_Q( M8_DEST_X_START, dx + l );
        M8D3D_Q( M8_DEST_X_END, dx + r + 1 );
        M8D3D_Q( M8_CUR_Y, dy + t + 1 );
        M8D3D_Q( M8_DEST_Y_END, dy + b );         /* starts the fill */
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
    if( !m8d3d_q_need( 1 ) ) {
        return( 0 );
    }
    M8D3D_Q( M8_RD_MASK, 0x00FF );
    return( m8d3d_outline( dx, dy, v, M8_MIX_REPLACE, M8_LINEDRAW_LAST_OFF, colour, 0x00FF ) );
}

/* The scratch's planes that have been used, all at once. Leaves the engine on the
 * plain path with open scissors. */
static int m8d3d_scratch_clear( M8D3D_CONTEXT *c )
{
    int ok = 1;

    if( c->dr > c->dl ) {
        ok = m8d3d_q_clear( c->dl, c->dt, c->dr, c->db );
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
        ++m8d3d_stat[26];
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

/* The plain path: lines and rows drawn with RD_MASK on one plane lose pixels. */
static int m8d3d_mono_off( void )
{
    if( m8d3d_stamp_mode ) {
        m8d3d_stamp_mode = 0;
        if( !m8d3d_q_need( 2 ) ) {
            return( 0 );
        }
        /* An engine reset does not clear this: left on, it would key the next copy. */
        M8D3D_Q( M8_SRC_CMP_FN, 0 );
        M8D3D_Q( M8_DP_CONFIG, M8_DP_8514 );
    }
    if( !m8d3d_mono_on ) {
        return( 1 );
    }
    m8d3d_mono_on = 0;
    if( !m8d3d_q_need( 2 ) ) {
        return( 0 );
    }
    M8D3D_Q( M8_RD_MASK, 0x00FF );
    M8D3D_Q( M8_DP_CONFIG, M8_DP_8514 );
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

        if( !m8d3d_q_need( 5 ) ) {
            return( 0 );
        }
        M8D3D_Q( M8_WRT_MASK, q );
        M8D3D_Q( M8_RD_MASK, 0x00FF );
        M8D3D_Q( M8_DP_CONFIG, M8_DP_8514 );
        M8D3D_Q( M8_ALU_FG_FN, M8_MIX_REPLACE );
        M8D3D_Q( M8_FRGD_COLOR, 0 );
        r.left = c->mx;
        r.top = c->my;
        r.right = c->mx + m->w;
        r.bottom = c->my + m->h;
        if( !m8d3d_q_rect( r.left, r.top, r.right, r.bottom ) ) {
            return( 0 );
        }
    }
    if( h >= 3 ) {
        if( !m8d3d_edges( c->sx, c->sy, n, plane ) || !m8d3d_q_need( 15 ) ) {
            return( 0 );
        }
        M8D3D_Q( M8_WRT_MASK, q );
        M8D3D_Q( M8_RD_MASK, plane );
        M8D3D_Q( M8_DP_CONFIG, M8_DP_POLY_FILL );
        M8D3D_Q( M8_ALU_FG_FN, M8_MIX_REPLACE );
        M8D3D_Q( M8_FRGD_COLOR, 0x00FF );
        M8D3D_Q( M8_DESTX_DIASTP, c->sx );
        M8D3D_Q( M8_DESTY_AXSTP, c->sy + 1 );
        M8D3D_Q( M8_SRC_X_START, c->sx );
        M8D3D_Q( M8_SRC_X_END, c->sx + w );
        M8D3D_Q( M8_SRC_Y_DIR, 1 );
        M8D3D_Q( M8_CUR_X, c->mx );
        M8D3D_Q( M8_DEST_X_START, c->mx );
        M8D3D_Q( M8_DEST_X_END, c->mx + w );
        M8D3D_Q( M8_CUR_Y, c->my + 1 );
        M8D3D_Q( M8_DEST_Y_END, c->my + h - 1 );
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
        if( !m8d3d_q_need( 1 ) ) {
            return( 0 );
        }
        M8D3D_Q( M8_RD_MASK, 0x00FF );
    }
    if( !m8d3d_outline( c->mx, c->my, n, M8_MIX_REPLACE, M8_LINEDRAW_LAST_OFF, 0x00FF, q ) ) {
        return( 0 );
    }
    if( !m8d3d_q_need( 1 ) ) {
        return( 0 );
    }
    M8D3D_Q( M8_WRT_MASK, 0x00FF );
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
    if( !m8d3d_q_need( 5 ) ) {
        return( 0 );
    }
    M8D3D_Q( M8_WRT_MASK, plane );
    M8D3D_Q( M8_FRGD_COLOR, 0x00FF );
    M8D3D_Q( M8_DP_CONFIG, M8_DP_8514 );
    M8D3D_Q( M8_ALU_FG_FN, M8_MIX_XOR );
    M8D3D_Q( M8_LINEDRAW_OPT, M8_LINEDRAW_POLY | M8_LINEDRAW_LAST_OFF );
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
        if( !m8d3d_q_need( 5 ) ) {
            return( 0 );
        }
        M8D3D_Q( M8_LINEDRAW_INDEX, 0 );
        M8D3D_Q( M8_LINEDRAW, ox + v[2 * a] + shift );
        M8D3D_Q( M8_LINEDRAW, oy + v[2 * a + 1] );
        M8D3D_Q( M8_LINEDRAW, ox + v[2 * b] + shift );
        M8D3D_Q( M8_LINEDRAW, oy + v[2 * b + 1] );
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
    if( !m8d3d_q_need( 4 ) ) {
        return( 0 );
    }
    M8D3D_Q( M8_FRGD_COLOR, m8d3d_plane_index( p, (lo + hi) / 2, y ) );
    M8D3D_Q( M8_CUR_Y, dy + y );
    M8D3D_Q( M8_CUR_X, dx + lo );
    M8D3D_Q( M8_SCAN_TO_X, dx + hi + 1 );
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
    if( !m8d3d_mono_off() || !m8d3d_edges_grad( sx, sy, v, plane ) || !m8d3d_q_need( 5 ) ) {
        return( 0 );
    }
    M8D3D_Q( M8_WRT_MASK, 0x00FF );
    M8D3D_Q( M8_RD_MASK, plane );
    M8D3D_Q( M8_DP_CONFIG, M8_DP_POLY_FILL );
    M8D3D_Q( M8_ALU_FG_FN, M8_MIX_REPLACE );
    M8D3D_Q( M8_SRC_Y_DIR, 1 );
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
        if( !m8d3d_q_need( 10 ) ) {
            return( 0 );
        }
        M8D3D_Q( M8_FRGD_COLOR, colour );
        M8D3D_Q( M8_DESTX_DIASTP, sx + lo );
        M8D3D_Q( M8_DESTY_AXSTP, sy + a );
        M8D3D_Q( M8_SRC_X_START, sx + lo );
        M8D3D_Q( M8_SRC_X_END, sx + hi + 1 );
        M8D3D_Q( M8_CUR_X, dx + lo );
        M8D3D_Q( M8_DEST_X_START, dx + lo );
        M8D3D_Q( M8_DEST_X_END, dx + hi + 1 );
        M8D3D_Q( M8_CUR_Y, dy + a );
        M8D3D_Q( M8_DEST_Y_END, dy + e );
        i = j;
    }
    if( !m8d3d_q_need( 2 ) ) {
        return( 0 );
    }
    M8D3D_Q( M8_RD_MASK, 0x00FF );
    M8D3D_Q( M8_DP_CONFIG, M8_DP_8514 );
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

/* Textures came to the column's top rows, or left them: the stamps start again below them. */
void m8d3d_stamps_forget( void )
{
    int k;

    for( k = 0; k < M8D3D_CONTEXTS; ++k ) {
        M8D3D_CONTEXT *c = &m8d3d_ctx[k];

        if( c->target != NULL && c->stamp_on ) {
            memset( c->stamp, 0, sizeof( c->stamp ) );
            memset( c->stamp_seen, 0, sizeof( c->stamp_seen ) );
            c->stamp_seen_next = 0;
            c->stamp_x = c->mx;
            c->stamp_y = m8dd_tex_rows;
            c->stamp_shelf = 0;
        }
    }
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
        c->stamp_y = m8dd_tex_rows;
        c->stamp_shelf = 0;
        if( h > c->my - m8dd_tex_rows ) {
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
    if( !m8d3d_mono_off() || !m8d3d_q_clear( x, y, x + r - l + 2, y + b - t + 2 ) ) {
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
/* A frame's triangles as patches of one colour. An edge used twice in one colour is inside its
 * patch and is not drawn, so a patch costs its outline and one fill, not each triangle's ~48
 * writes. Only for triangles that cannot overlap (culled, all corners on the target, their area
 * no more than the target's): the patches are filled in colour order, not the program's.
 *
 * The scratch is the column right of both pages, the screen in two halves: x 0-320 at rows
 * 0-479, x 320-639 at rows 512-991; it takes the stamp and mask rows, whose contents are then
 * forgotten. Each colour has a plane. The fill reads a row from the left, so the right half must
 * start in the state the left half ends in: an edge's part left of the seam is carried to the
 * seam as a vertical run over the same rows, one toggle a row, as the edge itself gives. */
#define M8D3D_JOIN_SEAM     320
#define M8D3D_JOIN_X        640         /* the column's left */
#define M8D3D_JOIN_RY       512         /* the right half's first row */
#define M8D3D_JOIN_W        321
#define M8D3D_JOIN_MIN      16
#define M8D3D_JOIN_EDGES    3072
#define M8D3D_JOIN_BAND     64          /* rows a fill band; at 320x240 the engine has room, fewer fills win */
#define M8D3D_JOIN_RUNS     8           /* x runs a band */
static int m8d3d_jruns = M8D3D_JOIN_RUNS;   /* [diag] runs=, to rule the run joining in or out */
static int m8d3d_dclean;                    /* [diag] clean=1: the scratch cleared whole every frame */
static int m8d3d_dnoout;                    /* [diag] nooutline=1: no on-screen outlines, to time them */
static int m8d3d_jshift;                    /* [diag] shift=0: on-screen outlines again */
static int m8d3d_dnolap;                    /* [diag] nooverlap=1: runs not stopped at overlaps */
#define M8D3D_JOIN_FILL_PX  3200L       /* one fill's 9 writes and start, in polygon-fill pixels */
static int  m8d3d_jband = M8D3D_JOIN_BAND;     /* [join] band=, fillpx=: tuned to whichever of CPU and */
static long m8d3d_jfillpx = M8D3D_JOIN_FILL_PX; /* engine is the limit, which moves with each change */

typedef struct {
    short   col;
    short   y0;
    short   x0;
    short   y1;
    short   x1;
} M8D3D_JEDGE;

static M8D3D_JEDGE   m8d3d_je[M8D3D_JOIN_EDGES];
static short        m8d3d_jseam[M8D3D_JOIN_EDGES * 2];
static WORD         m8d3d_jtri[M8D3D_JOIN_EDGES / 3];  /* triangle numbers in colour order */
static WORD         m8d3d_jfrom[257];                   /* each colour's first in m8d3d_jtri */

/* Inner edges, met an even number of times in one colour, cancel. A hash finds an edge's first
 * copy, whose parity each later copy flips; a count by colour then puts the outer edges in colour
 * order. One pass each: a sort of the frame's edges ran at uncached speed, the array the size of
 * the L1. */
#define M8D3D_JHASH         8192
static WORD         m8d3d_jhash[M8D3D_JHASH];           /* edge number + 1, 0 empty */
static WORD         m8d3d_jused[M8D3D_JOIN_EDGES];      /* the slots to empty after */
static BYTE         m8d3d_jodd[M8D3D_JOIN_EDGES];
#define M8D3D_PT_HASH( x, y ) ((((unsigned)(x) * 641u + (unsigned)(y)) * 2654435761u >> 19) & (M8D3D_JHASH - 1))

static int m8d3d_edge_cmp( const M8D3D_JEDGE *a, const M8D3D_JEDGE *b )
{
    if( (a->col & 0xFF) != (b->col & 0xFF) ) {
        return( (a->col & 0xFF) - (b->col & 0xFF) );
    }
    if( a->y0 != b->y0 ) {
        return( a->y0 - b->y0 );
    }
    if( a->x0 != b->x0 ) {
        return( a->x0 - b->x0 );
    }
    if( a->y1 != b->y1 ) {
        return( a->y1 - b->y1 );
    }
    return( a->x1 - b->x1 );
}

/* Shell sort: no library calls from here. */
static void m8d3d_shorts_sort( short *s, int n )
{
    int     i;
    int     j;
    short   t;

    for( i = 1; i < n; ++i ) {
        t = s[i];
        for( j = i; j > 0 && s[j - 1] > t; --j ) {
            s[j] = s[j - 1];
        }
        s[j] = t;
    }
}

/* How many of the frame's first triangles may be drawn as patches, or 0. Patches are drawn
 * in colour order, so they must not overlap: the run stops before the triangles' area passes
 * the screen's. A program without a Z buffer paints far to near, so what is left is drawn
 * afterwards, one triangle at a time, over it. */
/* Where a run of joined triangles must stop: a triangle over cells another of the run already
 * covers would be painted in colour order, not the program's. A cell counts as covered only when
 * a triangle holds all of it, so neighbours that share an edge do not stop the run; overlaps
 * smaller than a cell are missed. Cells are 8 units; the stamp marks this run's cells. For each
 * edge only the cell corner furthest outside it is tested, stepped by adds along the row. */
#define M8D3D_CELL      m8d3d_cellsh
#define M8D3D_CELLS_X   (640 / 8 + 1)
#define M8D3D_CELLS_Y   (480 / 8 + 1)
static int      m8d3d_cellsh = 3;   /* [diag] cell=16 for 16-unit cells: cheaper, misses thin overlaps */
static BYTE     m8d3d_cell[M8D3D_CELLS_Y][M8D3D_CELLS_X];
static BYTE     m8d3d_cell_stamp;

/* 1 if the triangle covers a cell the run already holds; marks its cells either way, since a
 * run that stops here starts the next with a new stamp. */
static int m8d3d_cells_mark( const short *p )
{
    long    s = (long)(p[2] - p[0]) * (p[5] - p[1]) - (long)(p[4] - p[0]) * (p[3] - p[1]);
    long    ea[3];
    long    eb[3];
    long    ec[3];
    int     lx = p[0] < p[2] ? p[0] : p[2];
    int     hx = p[0] > p[2] ? p[0] : p[2];
    int     ly = p[1] < p[3] ? p[1] : p[3];
    int     hy = p[1] > p[3] ? p[1] : p[3];
    int     c0;
    int     c1;
    int     cy;
    int     e;
    const int sz = 1 << M8D3D_CELL;

    lx = p[4] < lx ? p[4] : lx;
    hx = p[4] > hx ? p[4] : hx;
    ly = p[5] < ly ? p[5] : ly;
    hy = p[5] > hy ? p[5] : hy;
    if( s == 0 || hx - lx < sz || hy - ly < sz ) {
        return( 0 );
    }
    /* f = a x + b y + c, at least 0 inside; c taken at the worst corner of a cell from (x, y). */
    for( e = 0; e < 3; ++e ) {
        const short *a = p + 2 * e;
        const short *b = p + 2 * ((e + 1) % 3);

        ea[e] = (long)(a[1] - b[1]);
        eb[e] = (long)(b[0] - a[0]);
        if( s < 0 ) {
            ea[e] = -ea[e];
            eb[e] = -eb[e];
        }
        ec[e] = -(ea[e] * a[0] + eb[e] * a[1]) + (ea[e] < 0 ? ea[e] * sz : 0) + (eb[e] < 0 ? eb[e] * sz : 0);
    }
    c0 = (lx + sz - 1) >> M8D3D_CELL;
    c1 = hx >> M8D3D_CELL;              /* cells c0 .. c1 - 1 lie inside the box */
    for( cy = (ly + sz - 1) >> M8D3D_CELL; ((cy + 1) << M8D3D_CELL) <= hy; ++cy ) {
        long    y = (long)cy << M8D3D_CELL;
        long    f0 = ea[0] * ((long)c0 << M8D3D_CELL) + eb[0] * y + ec[0];
        long    f1 = ea[1] * ((long)c0 << M8D3D_CELL) + eb[1] * y + ec[1];
        long    f2 = ea[2] * ((long)c0 << M8D3D_CELL) + eb[2] * y + ec[2];
        long    s0 = ea[0] << M8D3D_CELL;
        long    s1 = ea[1] << M8D3D_CELL;
        long    s2 = ea[2] << M8D3D_CELL;
        BYTE    *row = m8d3d_cell[cy];
        int     cx;

        for( cx = c0; cx < c1; ++cx, f0 += s0, f1 += s1, f2 += s2 ) {
            if( f0 < 0 || f1 < 0 || f2 < 0 ) {
                continue;
            }
            if( row[cx] == m8d3d_cell_stamp ) {
                return( 1 );
            }
            row[cx] = m8d3d_cell_stamp;
        }
    }
    return( 0 );
}

static int m8d3d_join_ok( M8D3D_CONTEXT *c, const short *v, int n, int least )
{
    long    area = 0;
    long    most;
    int     i;

    if( !c->stamp_on || n < least || c->sw != 640 ) {
        return( 0 );
    }
    if( n * 3 > M8D3D_JOIN_EDGES ) {
        n = M8D3D_JOIN_EDGES / 3;
    }
    /* The viewport's right and bottom edges are 640 and 480: a corner there is on screen. */
    for( i = 0; i < n * 6; i += 2 ) {
        if( v[i] < 0 || v[i] > 640 || v[i + 1] < 0 || v[i + 1] > 480 ) {
            return( 0 );
        }
    }
    if( ++m8d3d_cell_stamp == 0 ) {
        memset( m8d3d_cell, 0, sizeof( m8d3d_cell ) );
        m8d3d_cell_stamp = 1;
    }
    /* Twice the target's area, the cross product's measure, and a margin: at 320x240 the corners
     * are halved, so the target is a quarter of 640x480. */
    most = m8d3d_lores ? 2L * 320 * 240 + 2L * 320 * 240 / 32 : 2L * 640 * 480 + 2L * 640 * 480 / 32;
    for( i = 0; i < n; ++i ) {
        const short *p = v + 6 * i;
        long        a = (long)(p[2] - p[0]) * (p[5] - p[1]) - (long)(p[4] - p[0]) * (p[3] - p[1]);

        area += a < 0 ? -a : a;
        if( area > most || (!m8d3d_dnolap && m8d3d_cells_mark( p )) ) {
            break;
        }
    }
    return( i >= least ? i : 0 );
}

/* One edge top to bottom without its last row, into the scratch at ox, oy. */
static int m8d3d_join_line( int ox, int oy, int xa, int ya, int xb, int yb )
{
    if( ya == yb ) {
        return( 1 );
    }
    if( ya > yb ) {
        int t = xa;

        xa = xb;
        xb = t;
        t = ya;
        ya = yb;
        yb = t;
    }
    if( !m8d3d_q_need( 5 ) ) {
        return( 0 );
    }
    M8D3D_Q( M8_LINEDRAW_INDEX, 0 );
    M8D3D_Q( M8_LINEDRAW, ox + xa );
    M8D3D_Q( M8_LINEDRAW, oy + ya );
    M8D3D_Q( M8_LINEDRAW, ox + xb );
    M8D3D_Q( M8_LINEDRAW, oy + yb );
    return( 1 );
}

/* Planes of both halves to 0 over rows t to b - 1. */
/* Planes of the left half over x l to r - 1, and of the right half's whole width if `right`. */
static int m8d3d_join_clear_part( int planes, int t, int b, int l, int r, int right )
{
    if( !m8d3d_q_need( 6 ) ) {
        return( 0 );
    }
    M8D3D_Q( M8_WRT_MASK, planes );
    M8D3D_Q( M8_RD_MASK, 0x00FF );
    M8D3D_Q( M8_DP_CONFIG, M8_DP_8514 );
    M8D3D_Q( M8_FRGD_MIX, M8_MIX_FRGD_REPLACE );
    M8D3D_Q( M8_FRGD_COLOR, 0 );
    if( !m8d3d_q_rect( M8D3D_JOIN_X + l, t, M8D3D_JOIN_X + r, b ) ||
        (right && !m8d3d_q_rect( M8D3D_JOIN_X, M8D3D_JOIN_RY + t, M8D3D_JOIN_X + M8D3D_JOIN_W, M8D3D_JOIN_RY + b )) ||
        !m8d3d_q_need( 1 ) ) {
        return( 0 );
    }
    M8D3D_Q( M8_WRT_MASK, 0x00FF );
    return( m8d3d_q_flush() );
}

static int m8d3d_join_clear( int planes, int t, int b )
{
    return( m8d3d_join_clear_part( planes, t, b, 0, M8D3D_JOIN_W, 1 ) );
}

/* The registers every fill of one colour shares. */
static int m8d3d_join_fill_begin( int plane, BYTE col )
{
    if( !m8d3d_q_need( 6 ) ) {
        return( 0 );
    }
    M8D3D_Q( M8_WRT_MASK, 0x00FF );
    M8D3D_Q( M8_RD_MASK, plane );
    M8D3D_Q( M8_DP_CONFIG, M8_DP_POLY_FILL );
    M8D3D_Q( M8_ALU_FG_FN, M8_MIX_REPLACE );
    M8D3D_Q( M8_FRGD_COLOR, col );
    M8D3D_Q( M8_SRC_Y_DIR, 1 );
    return( 1 );
}

/* One half's fill, rows t to b - 1, target x l to r - 1. The fill counts toggles from l, so
 * l must lie outside the patch on every row, or be the seam. */
static int m8d3d_join_fill( int dy, int l, int r, int t, int b, int ox, int oy )
{
    if( r <= l || b <= t ) {
        return( 1 );
    }
    if( !m8d3d_q_need( 9 ) ) {
        return( 0 );
    }
    M8D3D_Q( M8_DESTX_DIASTP, ox + l );
    M8D3D_Q( M8_DESTY_AXSTP, oy + t );
    M8D3D_Q( M8_SRC_X_START, ox + l );
    M8D3D_Q( M8_SRC_X_END, ox + r );
    M8D3D_Q( M8_CUR_X, l );
    M8D3D_Q( M8_DEST_X_START, l );
    M8D3D_Q( M8_DEST_X_END, r );
    M8D3D_Q( M8_CUR_Y, dy + t );
    M8D3D_Q( M8_DEST_Y_END, dy + b );         /* starts the fill */
    ++m8d3d_stat[26];
    return( m8d3d_q_flush() );
}

/* Rows t to b - 1, x l to r - 1, split at the seam into the two halves. */
static int m8d3d_join_fill_run( int dy, int l, int r, int t, int b )
{
    r = r < 640 ? r : 640;
    return( m8d3d_join_fill( dy, l, r < M8D3D_JOIN_SEAM ? r : M8D3D_JOIN_SEAM, t, b, M8D3D_JOIN_X, 0 ) &&
            m8d3d_join_fill( dy, l > M8D3D_JOIN_SEAM ? l : M8D3D_JOIN_SEAM, r, t, b,
                             M8D3D_JOIN_X - M8D3D_JOIN_SEAM, M8D3D_JOIN_RY ) );
}

/* A triangle's x extent over rows ya to yb, as l to r - 1. */
static void m8d3d_tri_span( const short *p, int ya, int yb, int *l, int *r )
{
    int lo = 32767;
    int hi = -32768;
    int k;

    for( k = 0; k < 3; ++k ) {
        const short *a = p + 2 * k;
        const short *b = p + 2 * ((k + 1) % 3);
        int         y;

        if( a[1] >= ya && a[1] <= yb ) {
            lo = a[0] < lo ? a[0] : lo;
            hi = a[0] > hi ? a[0] : hi;
        }
        for( y = ya; a[1] != b[1]; y = yb ) {
            if( (y > a[1] && y < b[1]) || (y < a[1] && y > b[1]) ) {
                int x = a[0] + (int)((long)(b[0] - a[0]) * (y - a[1]) / (b[1] - a[1]));

                lo = x < lo ? x : lo;
                hi = x > hi ? x : hi;
            }
            if( y == yb ) {
                break;
            }
        }
    }
    *l = lo;
    *r = hi + 1;
}

/* Adds l to r - 1 to a band's runs, joining runs closer than gap; when full, the nearest. */
static void m8d3d_run_add( short *run, int *nr, int l, int r, int gap )
{
    int i;
    int k;
    int bd = 32767;

    for( i = 0; i < *nr; ) {
        if( l <= run[2 * i + 1] + gap && run[2 * i] <= r + gap ) {
            l = run[2 * i] < l ? run[2 * i] : l;
            r = run[2 * i + 1] > r ? run[2 * i + 1] : r;
            for( k = 2 * i; k < 2 * *nr - 2; ++k ) {
                run[k] = run[k + 2];
            }
            --*nr;
        } else {
            ++i;
        }
    }
    if( *nr >= m8d3d_jruns ) {
        for( i = 0; i < *nr; ++i ) {
            int d = run[2 * i] > r ? run[2 * i] - r : l - run[2 * i + 1];

            bd = d < bd ? d : bd;
        }
        m8d3d_run_add( run, nr, l, r, bd );
        return;
    }
    for( i = *nr; i > 0 && run[2 * i - 2] > l; --i ) {
        run[2 * i] = run[2 * i - 2];
        run[2 * i + 1] = run[2 * i - 1];
    }
    run[2 * i] = (short)l;
    run[2 * i + 1] = (short)r;
    ++*nr;
}

/* Fills one colour's patch over rows t to b - 1 by bands: in each band only the x runs its
 * triangles reach, since a polygon fill costs ~69 ns a pixel of its box. Bands whose runs
 * line up are joined where the pixels added cost less than the fills saved. */
static int m8d3d_join_fill_bands( int dy, const short *v, const WORD *tri, int nt, int t, int b )
{
    short   grun[M8D3D_JOIN_RUNS * 2];
    short   brun[M8D3D_JOIN_RUNS * 2];
    int     gn = 0;
    int     gt = t;
    int     y;
    int     i;

    for( y = t; y < b; y += m8d3d_jband ) {
        int     y2 = y + m8d3d_jband < b ? y + m8d3d_jband : b;
        int     bn = 0;
        int     gap = (int)(m8d3d_jfillpx / (y2 - y));
        long    extra = 0;

        for( i = 0; i < nt; ++i ) {
            const short *p = v + 6 * tri[i];
            int         ylo = p[1] < p[3] ? p[1] : p[3];
            int         yhi = p[1] > p[3] ? p[1] : p[3];
            int         l;
            int         r;

            ylo = p[5] < ylo ? p[5] : ylo;
            yhi = p[5] > yhi ? p[5] : yhi;
            if( yhi < y || ylo > y2 - 1 ) {
                continue;
            }
            m8d3d_tri_span( p, ylo > y ? ylo : y, yhi < y2 - 1 ? yhi : y2 - 1, &l, &r );
            /* A pixel each side for where the outline's line steps. */
            m8d3d_run_add( brun, &bn, l > 0 ? l - 1 : 0, r + 1, gap );
        }
        if( gn != 0 && bn == gn ) {
            for( i = 0; i + 1 < gn; ++i ) {
                /* Joined runs must stay apart: a fill starting inside a patch inverts it. */
                if( (grun[2 * i + 1] > brun[2 * i + 1] ? grun[2 * i + 1] : brun[2 * i + 1]) >
                    (grun[2 * i + 2] < brun[2 * i + 2] ? grun[2 * i + 2] : brun[2 * i + 2]) ) {
                    extra = m8d3d_jfillpx * gn;
                }
            }
            for( i = 0; i < gn; ++i ) {
                int ul = grun[2 * i] < brun[2 * i] ? grun[2 * i] : brun[2 * i];
                int ur = grun[2 * i + 1] > brun[2 * i + 1] ? grun[2 * i + 1] : brun[2 * i + 1];

                extra += (long)(ur - ul) * (y2 - gt) - (long)(grun[2 * i + 1] - grun[2 * i]) * (y - gt) -
                         (long)(brun[2 * i + 1] - brun[2 * i]) * (y2 - y);
            }
            if( extra < m8d3d_jfillpx * gn ) {
                for( i = 0; i < gn; ++i ) {
                    grun[2 * i] = grun[2 * i] < brun[2 * i] ? grun[2 * i] : brun[2 * i];
                    grun[2 * i + 1] = grun[2 * i + 1] > brun[2 * i + 1] ? grun[2 * i + 1] : brun[2 * i + 1];
                }
                continue;
            }
        }
        for( i = 0; i < gn; ++i ) {
            if( !m8d3d_join_fill_run( dy, grun[2 * i], grun[2 * i + 1], gt, y ) ) {
                return( 0 );
            }
        }
        memcpy( grun, brun, sizeof( grun ) );
        gn = bn;
        gt = y;
    }
    for( i = 0; i < gn; ++i ) {
        if( !m8d3d_join_fill_run( dy, grun[2 * i], grun[2 * i + 1], gt, b ) ) {
            return( 0 );
        }
    }
    return( 1 );
}

static int m8d3d_join( M8D3D_CONTEXT *c, int dy, const short *v, const WORD *index, int n )
{
    int         ne = 0;
    int         nb = 0;
    int         i;
    int         j;
    int         k;
    int         first;
    int         ok = 1;
    const int   lx = M8D3D_JOIN_X;
    const int   rx = M8D3D_JOIN_X - M8D3D_JOIN_SEAM;
    const int   ry = M8D3D_JOIN_RY;

    /* Every edge with its ends in one order, sorted, then pairs within a colour dropped. */
    for( i = 0; i < n; ++i ) {
        const short *p = v + 6 * i;

        for( j = 0; j < 3; ++j ) {
            const short *a = p + 2 * j;
            const short *b = p + 2 * ((j + 1) % 3);
            M8D3D_JEDGE  *e = &m8d3d_je[ne++];
            int         swap = a[1] > b[1] || (a[1] == b[1] && a[0] > b[0]);

            int         dy = b[1] - a[1];
            long        cr = (long)(b[0] - a[0]) * (p[2 * ((j + 2) % 3) + 1] - a[1]) -
                             (long)dy * (p[2 * ((j + 2) % 3)] - a[0]);

            /* Bit 8: the triangle lies left of the edge, so it bounds the patch on the right. The
             * fill leaves a right boundary out (guide 6943-6945), so such an edge goes one pixel
             * right in the scratch and no outline is needed. Not part of the cancel key: the two
             * copies of an inner edge differ in it. */
            e->col = (short)((index[i] & 0xFF) | ((dy > 0 ? cr > 0 : dy < 0 && cr < 0) ? 0x100 : 0));
            e->y0 = swap ? b[1] : a[1];
            e->x0 = swap ? b[0] : a[0];
            e->y1 = swap ? a[1] : b[1];
            e->x1 = swap ? a[0] : b[0];
        }
    }
    {
        int nu = 0;

        for( i = 0; i < ne; ++i ) {
            const M8D3D_JEDGE   *e = &m8d3d_je[i];
            unsigned            h = ((unsigned)(e->col & 0xFF) * 31u + (unsigned)e->y0 * 977u + (unsigned)e->x0 * 131u +
                                     (unsigned)e->y1 * 7919u + (unsigned)e->x1 * 59u) & (M8D3D_JHASH - 1);

            m8d3d_jodd[i] = 1;
            for( ;; h = (h + 1) & (M8D3D_JHASH - 1) ) {
                int f = m8d3d_jhash[h];

                if( f == 0 ) {
                    m8d3d_jhash[h] = (WORD)(i + 1);
                    m8d3d_jused[nu++] = (WORD)h;
                    break;
                }
                if( m8d3d_edge_cmp( &m8d3d_je[f - 1], e ) == 0 ) {
                    m8d3d_jodd[f - 1] ^= 1;
                    m8d3d_jodd[i] = 0;
                    break;
                }
            }
        }
        for( i = 0; i < nu; ++i ) {
            m8d3d_jhash[m8d3d_jused[i]] = 0;
        }
    }
    memset( m8d3d_jfrom, 0, sizeof( m8d3d_jfrom ) );
    for( i = 0; i < ne; ++i ) {
        if( m8d3d_jodd[i] ) {
            ++m8d3d_jfrom[(m8d3d_je[i].col & 0xFF) + 1];
        }
    }
    for( i = 1; i < 257; ++i ) {
        m8d3d_jfrom[i] += m8d3d_jfrom[i - 1];
    }
    nb = m8d3d_jfrom[256];
    /* Outer edges to the front, each one's place by colour in m8d3d_jused, then moved by
     * following cycles: no second edge array. */
    for( i = j = 0; i < ne; ++i ) {
        if( m8d3d_jodd[i] ) {
            m8d3d_jused[j] = m8d3d_jfrom[m8d3d_je[i].col & 0xFF]++;
            m8d3d_je[j++] = m8d3d_je[i];
        }
    }
    for( i = 0; i < nb; ++i ) {
        while( m8d3d_jused[i] != (WORD)i ) {
            int         d = m8d3d_jused[i];
            M8D3D_JEDGE t = m8d3d_je[d];

            m8d3d_je[d] = m8d3d_je[i];
            m8d3d_je[i] = t;
            m8d3d_jused[i] = m8d3d_jused[d];
            m8d3d_jused[d] = (WORD)d;
        }
    }
    memset( m8d3d_jfrom, 0, sizeof( m8d3d_jfrom ) );
    for( i = 0; i < n; ++i ) {
        ++m8d3d_jfrom[(index[i] & 0xFF) + 1];
    }
    for( i = 1; i < 257; ++i ) {
        m8d3d_jfrom[i] += m8d3d_jfrom[i - 1];
    }
    for( i = 0; i < n; ++i ) {
        m8d3d_jtri[m8d3d_jfrom[index[i] & 0xFF]++] = (WORD)i;
    }
    /* Each slot now holds the next colour's start: shift back by one. */
    for( i = 256; i > 0; --i ) {
        m8d3d_jfrom[i] = m8d3d_jfrom[i - 1];
    }
    m8d3d_jfrom[0] = 0;
    if( !c->jclean || m8d3d_dclean ) {
        /* The column held stamps and masks: forget them, and start every plane at 0. */
        memset( c->stamp, 0, sizeof( c->stamp ) );
        memset( c->stamp_seen, 0, sizeof( c->stamp_seen ) );
        c->stamp_seen_next = 0;
        c->stamp_x = c->mx;
        c->stamp_y = m8dd_tex_rows;
        c->stamp_shelf = 0;
        memset( c->mask, 0, sizeof( c->mask ) );
        memset( c->seen, 0, sizeof( c->seen ) );
        c->fresh = 1;
        if( !m8d3d_join_clear( 0x00FF, 0, 480 ) ) {
            return( 0 );
        }
        c->jclean = 1;
    }
    M8D3D_PH( 37 );
    /* Up to 8 colours a pass, a plane each. */
    for( first = 0; ok && first < nb; first = k ) {
        int     cols[8];
        int     from[9];
        int     nc = 0;
        int     t = 480;
        int     b = 0;
        int     xl = M8D3D_JOIN_W;      /* the pass's x extent in the left half */
        int     xr = 0;
        int     right = 0;              /* anything right of the seam: at 320x240, nothing */

        for( k = first; k < nb && (nc < 8 || (m8d3d_je[k].col & 0xFF) == cols[nc - 1]); ++k ) {
            if( nc == 0 || (m8d3d_je[k].col & 0xFF) != cols[nc - 1] ) {
                cols[nc] = m8d3d_je[k].col & 0xFF;
                from[nc++] = k;
            }
        }
        from[nc] = k;
        /* Outlines into the scratch: XOR, one pixel a row, without the last row. */
        if( !m8d3d_q_need( 5 ) ) {
            return( 0 );
        }
        M8D3D_Q( M8_FRGD_COLOR, 0x00FF );
        M8D3D_Q( M8_DP_CONFIG, M8_DP_8514 );
        M8D3D_Q( M8_ALU_FG_FN, M8_MIX_XOR );
        M8D3D_Q( M8_LINEDRAW_OPT, M8_LINEDRAW_POLY | M8_LINEDRAW_LAST_OFF );
        M8D3D_Q( M8_RD_MASK, 0x00FF );
        for( j = 0; ok && j < nc; ++j ) {
            int ns = 0;
            int s;

            if( !m8d3d_q_need( 1 ) ) {
                return( 0 );
            }
            M8D3D_Q( M8_WRT_MASK, 1 << j );
            for( i = from[j]; ok && i < from[j + 1]; ++i ) {
                const M8D3D_JEDGE *e = &m8d3d_je[i];
                int         sh = m8d3d_jshift ? (e->col >> 8) & 1 : 0;
                int         xa = (e->x0 < e->x1 ? e->x0 : e->x1) + sh;
                int         xb = (e->x0 < e->x1 ? e->x1 : e->x0) + sh;
                int         ya = e->x0 < e->x1 ? e->y0 : e->y1;
                int         yb = e->x0 < e->x1 ? e->y1 : e->y0;

                t = e->y0 < t ? e->y0 : t;
                b = e->y1 > b ? e->y1 : b;
                xl = xa < xl ? xa : xl;
                xr = xb + 1 > xr ? xb + 1 : xr;     /* xb has the shift in */
                right |= xb > M8D3D_JOIN_SEAM;
                if( ya == yb ) {
                    continue;
                }
                if( xb <= M8D3D_JOIN_SEAM ) {
                    ok = m8d3d_join_line( lx, 0, xa, ya, xb, yb );
                    m8d3d_jseam[ns++] = (short)ya;
                    m8d3d_jseam[ns++] = (short)yb;
                } else if( xa >= M8D3D_JOIN_SEAM ) {
                    ok = m8d3d_join_line( rx, ry, xa, ya, xb, yb );
                } else {
                    /* Cut at the seam, the row rounded: rows above and below it stay one toggle each. */
                    long    d = (long)(xb - xa);
                    int     py = ya + (int)(((long)(yb - ya) * (M8D3D_JOIN_SEAM - xa) * 2 + (yb > ya ? d : -d)) / (2 * d));

                    ok = m8d3d_join_line( lx, 0, xa, ya, M8D3D_JOIN_SEAM, py ) &&
                         m8d3d_join_line( rx, ry, M8D3D_JOIN_SEAM, py, xb, yb );
                    m8d3d_jseam[ns++] = (short)ya;
                    m8d3d_jseam[ns++] = (short)py;
                }
            }
            /* The left parts' rows at the seam, XOR-merged: an end seen an even number of times
             * cancels, and what is left pairs into runs. */
            for( i = 0; i < ns; i += 2 ) {
                if( m8d3d_jseam[i] > m8d3d_jseam[i + 1] ) {
                    short x = m8d3d_jseam[i];

                    m8d3d_jseam[i] = m8d3d_jseam[i + 1];
                    m8d3d_jseam[i + 1] = x;
                }
            }
            m8d3d_shorts_sort( m8d3d_jseam, ns );
            for( i = 0, s = 0; i < ns; i = k ) {
                for( k = i + 1; k < ns && m8d3d_jseam[k] == m8d3d_jseam[i]; ++k ) {
                }
                if( (k - i) & 1 ) {
                    m8d3d_jseam[s++] = m8d3d_jseam[i];
                }
            }
            for( i = 0; ok && i + 1 < s; i += 2 ) {
                ok = m8d3d_join_line( rx, ry, M8D3D_JOIN_SEAM, m8d3d_jseam[i], M8D3D_JOIN_SEAM, m8d3d_jseam[i + 1] );
            }
            k = from[nc];
        }
        if( !ok ) {
            return( 0 );
        }
        M8D3D_PH( 38 );
        /* Each colour's fill, then its outline over the rows the fill leaves. */
        for( j = 0; ok && j < nc; ++j ) {
            int ct = 480;
            int cb = 0;

            for( i = from[j]; i < from[j + 1]; ++i ) {
                ct = m8d3d_je[i].y0 < ct ? m8d3d_je[i].y0 : ct;
                cb = m8d3d_je[i].y1 > cb ? m8d3d_je[i].y1 : cb;
            }
            ok = m8d3d_join_fill_begin( 1 << j, (BYTE)cols[j] ) &&
                 m8d3d_join_fill_bands( dy, v, m8d3d_jtri + m8d3d_jfrom[cols[j]],
                                        m8d3d_jfrom[cols[j] + 1] - m8d3d_jfrom[cols[j]],
                                        m8d3d_jshift ? ct : ct + 1, cb );
        }
        M8D3D_PH( 39 );
        if( !ok || !m8d3d_q_need( 6 ) ) {
            return( 0 );
        }
        M8D3D_Q( M8_RD_MASK, 0x00FF );
        M8D3D_Q( M8_WRT_MASK, 0x00FF );
        M8D3D_Q( M8_DP_CONFIG, M8_DP_8514 );
        M8D3D_Q( M8_ALU_FG_FN, M8_MIX_REPLACE );
        M8D3D_Q( M8_LINEDRAW_OPT, 0 );
        for( j = 0; ok && j < nc && !m8d3d_dnoout && !m8d3d_jshift; ++j ) {
            if( !m8d3d_q_need( 1 ) ) {
                return( 0 );
            }
            M8D3D_Q( M8_FRGD_COLOR, cols[j] );
            /* Edges chained end to start: after a move, each further x, y pair draws on from the
             * last end, 2 writes an edge where a lone line takes 5. Plain lines, so the order
             * edges are drawn in does not change the pixels. */
            /* Each end listed under its point: the hash is empty between edge passes, and the
             * seam rows are spent, so they hold the lists' links. */
            for( i = from[j]; i < from[j + 1]; ++i ) {
                const M8D3D_JEDGE   *e = &m8d3d_je[i];
                unsigned            h0 = M8D3D_PT_HASH( e->x0, e->y0 );
                unsigned            h1 = M8D3D_PT_HASH( e->x1, e->y1 );
                int                 q = 2 * (i - from[j]);

                m8d3d_jodd[i] = 0;
                m8d3d_jseam[q] = (short)m8d3d_jhash[h0];
                m8d3d_jhash[h0] = (WORD)(q + 1);
                m8d3d_jseam[q + 1] = (short)m8d3d_jhash[h1];
                m8d3d_jhash[h1] = (WORD)(q + 2);
            }
            for( i = from[j]; ok && i < from[j + 1]; ++i ) {
                const M8D3D_JEDGE   *e = &m8d3d_je[i];
                int                 px;
                int                 py;

                if( m8d3d_jodd[i] ) {
                    continue;
                }
                m8d3d_jodd[i] = 1;
                if( !m8d3d_q_need( 5 ) ) {
                    return( 0 );
                }
                /* Corners on the viewport's far edges are a pixel past the page. */
                M8D3D_Q( M8_LINEDRAW_INDEX, 0 );
                M8D3D_Q( M8_LINEDRAW, e->x0 < 640 ? e->x0 : 639 );
                M8D3D_Q( M8_LINEDRAW, dy + (e->y0 < 480 ? e->y0 : 479) );
                M8D3D_Q( M8_LINEDRAW, e->x1 < 640 ? e->x1 : 639 );
                M8D3D_Q( M8_LINEDRAW, dy + (e->y1 < 480 ? e->y1 : 479) );
                px = e->x1;
                py = e->y1;
                for( ;; ) {
                    int g = -1;
                    int ge = 0;
                    int nx;
                    int ny;
                    int q;

                    /* The lowest-numbered unused edge with an end here, its start end first. */
                    for( q = m8d3d_jhash[M8D3D_PT_HASH( px, py )]; q != 0; q = m8d3d_jseam[q - 1] ) {
                        int                 h = from[j] + ((q - 1) >> 1);
                        int                 end = (q - 1) & 1;
                        const M8D3D_JEDGE   *f = &m8d3d_je[h];

                        if( m8d3d_jodd[h] || (end ? f->x1 != px || f->y1 != py : f->x0 != px || f->y0 != py) ) {
                            continue;
                        }
                        if( g < 0 || h < g || (h == g && end < ge) ) {
                            g = h;
                            ge = end;
                        }
                    }
                    if( g < 0 ) {
                        break;
                    }
                    nx = ge ? m8d3d_je[g].x0 : m8d3d_je[g].x1;
                    ny = ge ? m8d3d_je[g].y0 : m8d3d_je[g].y1;
                    m8d3d_jodd[g] = 1;
                    if( !m8d3d_q_need( 2 ) ) {
                        return( 0 );
                    }
                    M8D3D_Q( M8_LINEDRAW, nx < 640 ? nx : 639 );
                    M8D3D_Q( M8_LINEDRAW, dy + (ny < 480 ? ny : 479) );
                    px = nx;
                    py = ny;
                }
            }
            for( i = from[j]; i < from[j + 1]; ++i ) {
                m8d3d_jhash[M8D3D_PT_HASH( m8d3d_je[i].x0, m8d3d_je[i].y0 )] = 0;
                m8d3d_jhash[M8D3D_PT_HASH( m8d3d_je[i].x1, m8d3d_je[i].y1 )] = 0;
            }
        }
        xl = xl < 0 ? 0 : xl;
        xr = xr > M8D3D_JOIN_W + 1 ? M8D3D_JOIN_W + 1 : xr;   /* a shifted edge at the seam reaches one past */
        ok = ok && m8d3d_join_clear_part( (1 << nc) - 1, t, b + 1, xl, xr > xl ? xr : xl + 1, right );
        M8D3D_PH( 40 );
    }
    m8d3d_stat[8] += n;
    return( ok );
}

int m8d3d_replay( const RECT *erase, int erase_n, BYTE black, int dy, const short *v, const WORD *index,
                  const DWORD *rgb, int n )
{
    M8D3D_CONTEXT   *c = NULL;
    RECT            r;
    int             k;
    int             i;
    int             ok = 1;
    int             build = 1;
    int             first;

    for( k = 0; k < M8D3D_CONTEXTS && c == NULL; ++k ) {
        if( m8d3d_ctx[k].target != NULL ) {
            c = &m8d3d_ctx[k];
        }
    }
    m8d3d_owner_check();
    m8d3d_feed_stop( 12 );
    if( c == NULL || !m8d3d_engine_begin() ) {
        ++m8d3d_stat[11];
        return( 0 );
    }
    m8d3d_mono_on = 0;
    m8d3d_stamp_mode = 0;
    m8d3d_qn = 0;
    m8d3d_qh = 0;
    m8d3d_q_next = M8D3D_Q_PUMP;
    m8d3d_q_hold = !m8d3d_nohold;
    m8d3d_feed_open();
    if( m8d3d_feed != NULL ) {
        m8d3d_q = m8d3d_feed + M8FEED_FIRST;
        m8d3d_q_cap = M8FEED_CAP;
    }
    QueryPerformanceCounter( &m8d3d_ph );
    m8d3d_ph_send = m8d3d_stat[44];
    if( m8d3d_pal_want && (!m8d3d_pal_on || m8d3d_pal_dirty) ) {
        m8d3d_pal_load();
    }
    /* Corners rounded at half size (m8d3d_draw_on) need the screen at 320x240 from the first
     * frame, not only with the texture palette. */
    if( c->sw == 640 && m8d3d_half && !m8d3d_lores ) {
        m8d3d_lores_set();
        m8d3d_lores = 1;
    }
    if( c->fresh ) {
        ok = m8d3d_scratch_clear( c );
        if( ok && c->mh != 0 ) {
            ok = m8d3d_q_clear( c->mx, c->my, c->mx + c->tw, c->my + c->mh );
        }
        c->fresh = 0;
    }
    if( ok && erase_n != 0 ) {
        RECT    u = erase[0];

        /* One rectangle over them all where its pixels cost less than the boxes' writes:
         * a plain fill is ~15 ns a pixel, a box 5 writes of ~7.6 us. */
        for( i = 1; i < erase_n; ++i ) {
            u.left = erase[i].left < u.left ? erase[i].left : u.left;
            u.top = erase[i].top < u.top ? erase[i].top : u.top;
            u.right = erase[i].right > u.right ? erase[i].right : u.right;
            u.bottom = erase[i].bottom > u.bottom ? erase[i].bottom : u.bottom;
        }
        if( erase_n > 1 && (long)(u.right - u.left) * (u.bottom - u.top) < (long)erase_n * 2500 ) {
            erase = &u;
            erase_n = 1;
        }
        ok = m8d3d_q_need( 1 );
        M8D3D_Q( M8_FRGD_COLOR, black );
        for( i = 0; ok && i < erase_n; ++i ) {
            r = erase[i];
            r.top += dy;
            r.bottom += dy;
            ok = m8d3d_q_rect( r.left, r.top, r.right, r.bottom );
            if( ok && (long)(r.right - r.left) * (r.bottom - r.top) >= M8D3D_Q_SEND_PX ) {
                ok = m8d3d_q_flush();
            }
        }
    }
    first = ok && n != 0 ? m8d3d_join_ok( c, v, n, M8D3D_JOIN_MIN ) : 0;
    M8D3D_PH( 36 );
    if( first != 0 ) {
        ok = m8d3d_mono_off() && m8d3d_q_flush() && m8d3d_join( c, dy, v, index, first );
        /* Further runs where one stopped at an overlap: painted after it, as the program would. */
        while( ok && first < n ) {
            int more = m8d3d_join_ok( c, v + 6 * first, n - first, 1 );

            if( more == 0 ) {
                break;
            }
            ok = m8d3d_join( c, dy, v + 6 * first, index + first, more );
            first += more;
        }
    }
    if( first < n ) {
        /* The one-at-a-time path keeps stamps and masks in the joined path's scratch. */
        c->jclean = 0;
    }
    M8D3D_PH( 40 );
    for( i = first; ok && i < n; ++i ) {
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
            if( ok ) {
                const short *p = v + 6 * i;
                int         w = p[0] > p[2] ? p[0] - p[2] : p[2] - p[0];
                int         w2 = p[0] > p[4] ? p[0] - p[4] : p[4] - p[0];
                int         h = p[1] > p[3] ? p[1] - p[3] : p[3] - p[1];
                int         h2 = p[1] > p[5] ? p[1] - p[5] : p[5] - p[1];

                if( (long)(w > w2 ? w : w2) * (h > h2 ? h : h2) >= M8D3D_Q_SEND_PX ) {
                    ok = m8d3d_q_flush();
                }
            }
        }
        ++m8d3d_stat[8];
    }
    M8D3D_PH( 41 );
    if( ok ) {
        ok = m8d3d_mono_off();
    }
    if( ok ) {
        ok = m8d3d_scratch_clear( c );
    }
    /* Whatever failed, the compare must not outlive this: GDI's copies would be keyed. */
    if( m8d3d_stamp_mode ) {
        m8d3d_stamp_mode = 0;
        M8D3D_Q( M8_SRC_CMP_FN, 0 );
    }
    m8d3d_q_hold = 0;
    M8D3D_PH( 42 );
    /* Held for the caller to add the page show and hand over (m8d3d_feed_go). */
    m8d3d_feed_held = 1;
    M8D3D_PH( 43 );
    if( !ok ) {
        ++m8d3d_stat[11];
    }
    return( ok );
}

/* Diagnostic, from C:\M8D3D.INI [diag] mode= at each context: 0 draws; 1 takes each triangle and
 * drops it, which leaves the program's and DirectX's own time; 2 drops it after facing; 3 does all
 * but keep it for Flip; 4 draws with the light rounded, not dithered; 5 drops each piece as it
 * reaches m8d3d_draw_on, after clipping; 6 draws but drops the triangles that need clipping; 7
 * samples texture patches with the 387, not in integers; 8 clips with the 387. */
static int m8d3d_diag;
static int m8d3d_accent = 1;            /* [diag] accent=0: plain majority vote in a patch */
static void m8d3d_patch_forget( void );

static DWORD __stdcall m8d3d_context_create( M8D3D_CONTEXTCREATE *d )
{
    int             k;
    int             x;
    int             y;
    M8D3D_CONTEXT   *c;

    ++m8d3d_stat[0];
    m8d3d_owner_check();
    m8d3d_diag = (int)GetPrivateProfileInt( "diag", "mode", 0, "C:\\M8D3D.INI" );
    m8d3d_nohold = (int)GetPrivateProfileInt( "diag", "nohold", 0, "C:\\M8D3D.INI" );
    m8d3d_dclean = (int)GetPrivateProfileInt( "diag", "clean", 0, "C:\\M8D3D.INI" );
    m8d3d_dnoout = (int)GetPrivateProfileInt( "diag", "nooutline", 0, "C:\\M8D3D.INI" );
    m8d3d_jshift = (int)GetPrivateProfileInt( "diag", "shift", 1, "C:\\M8D3D.INI" );
    m8d3d_accent = (int)GetPrivateProfileInt( "diag", "accent", 1, "C:\\M8D3D.INI" );
    m8d3d_patch_forget();
    m8d3d_cellsh = GetPrivateProfileInt( "diag", "cell", 8, "C:\\M8D3D.INI" ) >= 16 ? 4 : 3;
    m8d3d_dnolap = (int)GetPrivateProfileInt( "diag", "nooverlap", 0, "C:\\M8D3D.INI" );
    m8d3d_jband = (int)GetPrivateProfileInt( "join", "band", M8D3D_JOIN_BAND, "C:\\M8D3D.INI" );
    m8d3d_jband = m8d3d_jband < 8 ? 8 : m8d3d_jband;
    m8d3d_jfillpx = (long)GetPrivateProfileInt( "join", "fillpx", (int)M8D3D_JOIN_FILL_PX, "C:\\M8D3D.INI" );
    m8d3d_jruns = (int)GetPrivateProfileInt( "diag", "runs", M8D3D_JOIN_RUNS, "C:\\M8D3D.INI" );
    m8d3d_jruns = m8d3d_jruns < 1 || m8d3d_jruns > M8D3D_JOIN_RUNS ? M8D3D_JOIN_RUNS : m8d3d_jruns;
    m8d3d_stat[6] = (DWORD)d->lpDDS;
    m8d3d_stat[7] = (DWORD)d->lpDDSZ;
    d->ddrval = DDERR_OUTOFVIDEOMEMORY;
    for( k = 0; k < M8D3D_CONTEXTS && m8d3d_ctx[k].target != NULL; ++k ) {
    }
    if( k == M8D3D_CONTEXTS ) {
        return( DDHAL_DRIVER_HANDLED );
    }
    c = &m8d3d_ctx[k];
    c->tex = 0;
    c->texblend = M8D3D_TEXBLEND_MODULATE;
    /* An attached Z surface turns depth testing on until a program says otherwise. */
    c->rs_on = d->lpDDSZ != NULL ? 1UL << M8D3D_RS_ZENABLE : 0;
    c->zfunc = 0;
    ++m8d3d_census.contexts;
    if( d->lpDDSZ != NULL ) {
        ++m8d3d_census.contexts_z;
    }
    if( !m8d3d_surface( m8d3d_lcl( d->lpDDS ), &x, &y, &c->sw, &c->sh ) ) {
        m8dd_trace( "d3d context refused: target not on the card (Z %d)", d->lpDDSZ != NULL );
        return( DDHAL_DRIVER_HANDLED );
    }
    /* [d3d] lores=1 (the default) draws at half size and shows 320x240: fewer pixels for a
     * scene that fills the screen. A section named for the program, [M8TWIST.EXE] lores=0,
     * keeps a light scene sharp at the target's size. */
    {
        char    path[MAX_PATH];
        char    *n;

        GetModuleFileName( NULL, path, sizeof( path ) );
        n = strrchr( path, '\\' );
        n = n != NULL ? n + 1 : path;
        m8d3d_half = (int)GetPrivateProfileInt( "d3d", "lores", 1, "C:\\M8D3D.INI" );
        m8d3d_half = (int)GetPrivateProfileInt( n, "lores", m8d3d_half, "C:\\M8D3D.INI" ) != 0;
        /* zsort=1: a program that wanted a Z-buffer, which does not fit, still gets its frame
         * drawn furthest first. */
        m8d3d_zsort = (int)GetPrivateProfileInt( "d3d", "zsort", 0, "C:\\M8D3D.INI" );
        m8d3d_zsort = (int)GetPrivateProfileInt( n, "zsort", m8d3d_zsort, "C:\\M8D3D.INI" ) != 0;
        /* palette=1: flat colours as the program's own hues at 16 light levels, in place of
         * the fixed colour cube. */
        m8d3d_pmatch = (int)GetPrivateProfileInt( "d3d", "palette", 0, "C:\\M8D3D.INI" );
        m8d3d_pmatch = (int)GetPrivateProfileInt( n, "palette", m8d3d_pmatch, "C:\\M8D3D.INI" ) != 0;
    }
    c->tw = c->sw;
    if( !m8d3d_scratch( c, &c->tw, &c->sh, &c->sx, &c->sy ) ) {
        m8dd_trace( "d3d context refused: no scratch (target %dx%d)", c->sw, c->sh );
        return( DDHAL_DRIVER_HANDLED );
    }
    m8dd_trace( "d3d context: target %dx%d at %d,%d scratch %dx%d at %d,%d lores %d Z %d", c->sw, c->sh, x, y,
                c->tw, c->sh, c->sx, c->sy, m8d3d_half, d->lpDDSZ != NULL );
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
    c->jclean = 0;
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
            c->stamp_y = m8dd_tex_rows;
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
    m8d3d_feed_close();
    if( c != NULL ) {
        m8d3d_context_free( c );
    }
    m8d3d_pal_restore();
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

/* What a texture needs from its surface, which is DirectDraw's and stays in system memory. */
static int m8d3d_tex_read( M8D3D_TEX *t, BYTE **bits )
{
    BYTE *lcl = (BYTE *)m8d3d_lcl( t->dds );
    BYTE *gbl;

    if( lcl == NULL ) {
        return( 0 );
    }
    gbl = *(BYTE **)(lcl + M8D3D_LCL_GBL);
    if( gbl == NULL || IsBadReadPtr( gbl, M8D3D_GBL_GMASK + 4 ) ) {
        return( 0 );
    }
    /* A texture on the card: its summary in RAM. */
    {
        const WORD *sum = m8dd_tex_summary( gbl, &t->w, &t->h, &t->is555 );

        if( sum != NULL ) {
            *bits = (BYTE *)sum;
            t->pitch = 16;
            return( 1 );
        }
    }
    *bits = *(BYTE **)(gbl + M8D3D_GBL_VIDMEM);
    t->pitch = *(long *)(gbl + M8D3D_GBL_PITCH);
    t->h = *(WORD *)(gbl + M8D3D_GBL_HEIGHT);
    t->w = *(WORD *)(gbl + M8D3D_GBL_WIDTH);
    t->is555 = (*(DWORD *)(lcl + M8D3D_LCL_FLAGS) & M8D3D_HASPIXELFORMAT) &&
               *(DWORD *)(gbl + M8D3D_GBL_GMASK) == 0x03E0;
    return( *bits != NULL && t->w != 0 && t->h != 0 );
}

/* POW2 is claimed, so a mask wraps any coordinate. */
static DWORD m8d3d_texel( const M8D3D_TEX *t, const BYTE *bits, float u, float v )
{
    int     x = (int)m8d3d_round( u * t->w ) & (t->w - 1);
    int     y = (int)m8d3d_round( v * t->h ) & (t->h - 1);
    DWORD   p = *(const WORD *)(bits + y * t->pitch + 2 * x);

    if( t->is555 ) {
        return( ((p & 0x7C00) << 9) | ((p & 0x03E0) << 6) | ((p & 0x001F) << 3) );
    }
    return( ((p & 0xF800) << 8) | ((p & 0x07E0) << 5) | ((p & 0x001F) << 3) );
}

/* Vertex fog: the specular colour's alpha is how much of the vertex's own colour is left. */
static DWORD m8d3d_fogged( DWORD colour, DWORD fog, DWORD left )
{
    DWORD gone = 255 - left;

    return( ((((colour >> 16) & 255) * left + ((fog >> 16) & 255) * gone) / 255 << 16) |
            ((((colour >> 8) & 255) * left + ((fog >> 8) & 255) * gone) / 255 << 8) |
            (((colour & 255) * left + (fog & 255) * gone) / 255) );
}

static DWORD m8d3d_modulate( DWORD a, DWORD b )
{
    return( ((((a >> 16) & 255) * ((b >> 16) & 255) >> 8) << 16) |
            ((((a >> 8) & 255) * ((b >> 8) & 255) >> 8) << 8) |
            ((a & 255) * (b & 255) >> 8) );
}

static M8D3D_TEX *m8d3d_tex_of( DWORD h )
{
    return( h == 0 || h > M8D3D_TEXTURES || m8d3d_texs[h - 1].dds == NULL ? NULL : &m8d3d_texs[h - 1] );
}

/* A program whose textures hold 16 colours or fewer gets a palette of its own: each colour at
 * 16 light levels, index = colour * 16 + level. Lit texels then keep the author's colours,
 * where the 5 x 9 x 5 cube turns them to a few greys. More colours keep the cube. */
#define M8D3D_PAL_BASES     16
#define M8D3D_PAL_LEVELS    16

static DWORD    m8d3d_pal_base[M8D3D_PAL_BASES];
static int      m8d3d_pal_nb;           /* -1: too many colours, the cube stays */
static long     m8d3d_pal_count[M8D3D_PAL_BASES];   /* texels of each colour, all textures */
static BYTE     m8d3d_pal_saved[768];

extern void m8d3d_outb( unsigned port, unsigned v );
#pragma aux m8d3d_outb = "out dx, al" parm [edx] [eax] modify exact [];
extern unsigned m8d3d_inb( unsigned port );
#pragma aux m8d3d_inb = "xor eax, eax" "in al, dx" parm [edx] value [eax] modify exact [eax];

extern void m8d3d_outw( unsigned port, unsigned v );
#pragma aux m8d3d_outw = "out dx, ax" parm [edx] [eax] modify exact [];

/* Trial (Z6): shadow set 1 as 320x240, double scan and the pixel clock halved, at
 * 640x480's line and frame rate (M8LORES). The next mode set rewrites set 1 whole. */
/* For M8DD32's RAM drawing: the screen shows 320x240. */
int m8d3d_lores_on( void )
{
    return( m8d3d_lores );
}

/* For M8DD32: corners are recorded at half size. */
int m8d3d_half_on( void )
{
    return( m8d3d_half );
}

static void m8d3d_lores_set( void )
{
    static const WORD   port[8] = { 0x02E8, 0x06E8, 0x0AE8, 0x0EE8, 0x12E8, 0x16E8, 0x1AE8, 0x1EE8 };
    static const WORD   val[8] = { 0x34, 0x27, 0x29, 0x03, 0x0209, 0x01DF, 0x01EC, 0x03 };
    int                 i;

    m8d3d_outw( 0x22E8, 0x0060 );
    m8d3d_outw( 0x4AE8, 0x0002 );
    m8d3d_outb( 0x7AEE, 0x0A );
    m8d3d_outw( 0x22E8, 0x0020 );
    m8d3d_outw( 0x5AEE, 0x0001 );
    m8d3d_outw( 0x46EE, 0x0000 );
    for( i = 0; i < 8; ++i ) {
        if( i < 4 || i == 7 ) {
            m8d3d_outb( port[i], val[i] );
        } else {
            m8d3d_outw( port[i], val[i] );
        }
    }
    m8d3d_outb( 0x22E8, 0x2B );
    m8d3d_outw( 0x4AEE, 0x1864 );
    m8d3d_outw( 0x46EE, 0x003F );
    m8d3d_outw( 0x5AEE, 0x0000 );
    m8d3d_outw( 0x46EE, 0x0000 );
    m8d3d_outw( 0x22E8, 0x0060 );
    m8d3d_outw( 0x4AE8, 0x0003 );
    m8d3d_outb( 0x7AEE, 0x0A );
    m8d3d_outw( 0x22E8, 0x0028 );
}

static DWORD m8d3d_rgb16( const M8D3D_TEX *t, DWORD p )
{
    if( t->is555 ) {
        return( ((p & 0x7C00) << 9) | ((p & 0x03E0) << 6) | ((p & 0x001F) << 3) );
    }
    return( ((p & 0xF800) << 8) | ((p & 0x07E0) << 5) | ((p & 0x001F) << 3) );
}

static int m8d3d_pal_find( DWORD rgb )
{
    int k;

    for( k = 0; k < m8d3d_pal_nb; ++k ) {
        if( m8d3d_pal_base[k] == rgb ) {
            return( k );
        }
    }
    return( -1 );
}

/* Adds a texture's colours, once. Read at first use: at create its bits may not be loaded. */
static void m8d3d_pal_scan( M8D3D_TEX *t, const BYTE *bits )
{
    DWORD   last = 0xFFFFFFFFUL;
    int     lastk = 0;
    int     x;
    int     y;

    if( t->scanned ) {
        return;
    }
    t->scanned = 1;
    for( y = 0; y < t->h && m8d3d_pal_nb >= 0; ++y ) {
        const WORD *row = (const WORD *)(bits + y * t->pitch);

        for( x = 0; x < t->w; ++x ) {
            DWORD rgb;

            if( row[x] != last ) {
                last = row[x];
                rgb = m8d3d_rgb16( t, last );
                lastk = m8d3d_pal_find( rgb );
                if( lastk < 0 ) {
                    if( m8d3d_pal_nb == M8D3D_PAL_BASES ) {
                        m8d3d_pal_nb = -1;
                        return;
                    }
                    lastk = m8d3d_pal_nb;
                    m8d3d_pal_count[lastk] = 0;
                    m8d3d_pal_base[m8d3d_pal_nb++] = rgb;
                }
            }
            ++m8d3d_pal_count[lastk];
        }
    }
}

static DWORD m8d3d_pal_rgb( int k, int level )
{
    DWORD b = m8d3d_pal_base[k];

    return( ((b >> 16 & 255) * level / 15) << 16 | ((b >> 8 & 255) * level / 15) << 8 |
            (b & 255) * level / 15 );
}

/* A colour as one of the palette's bases at one of its 16 light levels. The base is the colour
 * at full brightness, 16 steps a gun, so a material under white light keeps one base whatever
 * its lighting. A new base is added; with all 16 taken, the nearest serves. The index, and in
 * *shown the colour it shows; -1 if textures have taken the palette past its size. */
static int m8d3d_pal_match( DWORD rgb, DWORD *shown )
{
    int     r = (int)(rgb >> 16 & 255);
    int     g = (int)(rgb >> 8 & 255);
    int     b = (int)(rgb & 255);
    int     m = r > g ? r : g;
    int     k;
    int     level;
    DWORD   base;

    if( m8d3d_pal_nb < 0 ) {
        return( -1 );
    }
    m = b > m ? b : m;
    if( m == 0 ) {
        m = 1;
    }
    level = (m * 15 + 127) / 255;
    /* Level 0 is black in every base: no base of its own. */
    if( level == 0 && m8d3d_pal_nb > 0 ) {
        *shown = 0;
        return( 0 );
    }
    base = (DWORD)((r * 255 / m + 8) / 17 * 17) << 16 | (DWORD)((g * 255 / m + 8) / 17 * 17) << 8 |
           (DWORD)((b * 255 / m + 8) / 17 * 17);
    if( (k = m8d3d_pal_find( base )) < 0 ) {
        if( m8d3d_pal_nb < M8D3D_PAL_BASES ) {
            k = m8d3d_pal_nb++;
            m8d3d_pal_count[k] = 0;
            m8d3d_pal_base[k] = base;
            m8d3d_pal_dirty = 1;
        } else {
            long    best = 0x7FFFFFFFL;
            int     i;

            for( i = 0; i < m8d3d_pal_nb; ++i ) {
                long dr = (long)(m8d3d_pal_base[i] >> 16 & 255) - (long)(base >> 16 & 255);
                long dg = (long)(m8d3d_pal_base[i] >> 8 & 255) - (long)(base >> 8 & 255);
                long db = (long)(m8d3d_pal_base[i] & 255) - (long)(base & 255);
                long d = dr * dr + dg * dg + db * db;

                if( d < best ) {
                    best = d;
                    k = i;
                }
            }
        }
    }
    *shown = m8d3d_pal_rgb( k, level );
    return( k * 16 + level );
}

/* At Flip, before the first frame in the palette's indices, and again when a base was added:
 * the 8514 DAC, 6 bits a gun. The desktop's is saved only the first time. */
static void m8d3d_pal_load( void )
{
    int i;

    m8d3d_pal_dirty = 0;
    if( !m8d3d_pal_kept && !m8d3d_pal_on ) {
        m8d3d_outb( 0x02EB, 0 );
        for( i = 0; i < 768; ++i ) {
            m8d3d_pal_saved[i] = (BYTE)m8d3d_inb( 0x02ED );
        }
    }
    m8d3d_pal_kept = 0;
    m8d3d_outb( 0x02EA, 0xFF );
    m8d3d_outb( 0x02EC, 0 );
    for( i = 0; i < 256; ++i ) {
        DWORD rgb = i >> 4 < m8d3d_pal_nb ? m8d3d_pal_rgb( i >> 4, i & 15 ) : 0;

        m8d3d_outb( 0x02ED, (rgb >> 16 & 255) >> 2 );
        m8d3d_outb( 0x02ED, (rgb >> 8 & 255) >> 2 );
        m8d3d_outb( 0x02ED, (rgb & 255) >> 2 );
    }
    m8d3d_pal_on = 1;
}

static void m8d3d_pal_restore( void )
{
    int i;

    if( m8d3d_pal_on ) {
        m8d3d_outb( 0x02EC, 0 );
        for( i = 0; i < 768; ++i ) {
            m8d3d_outb( 0x02ED, m8d3d_pal_saved[i] );
        }
    }
    m8d3d_pal_on = 0;
    m8d3d_pal_want = 0;
    m8d3d_lores = 0;
    m8d3d_pal_nb = 0;
    for( i = 0; i < M8D3D_TEXTURES; ++i ) {
        m8d3d_texs[i].scanned = 0;
    }
}

/* Detail by distance: a triangle whose box is M8D3D_FAR_PX or less on both sides is far away,
 * and takes every fourth light level, so a far ring joins into fewer patches. */
#define M8D3D_FAR_PX    16

/* The colour shown most on a 3 x 3 grid over the texture rectangle a triangle's corners span.
 * The two halves of a wall quad span the same rectangle, so they agree and the diagonal
 * between them joins away; a mean would mix colours the texture does not have. */
static int m8d3d_pal_quad( const M8D3D_TEX *t, const BYTE *bits, const M8D3D_TLVERTEX **a )
{
    static const float  f[3] = { 1.0f / 6.0f, 0.5f, 5.0f / 6.0f };
    int                 count[M8D3D_PAL_BASES];
    float               u0 = a[0]->tu;
    float               u1 = a[0]->tu;
    float               v0 = a[0]->tv;
    float               v1 = a[0]->tv;
    int                 best = 0;
    int                 i;
    int                 j;

    for( i = 1; i < 3; ++i ) {
        u0 = a[i]->tu < u0 ? a[i]->tu : u0;
        u1 = a[i]->tu > u1 ? a[i]->tu : u1;
        v0 = a[i]->tv < v0 ? a[i]->tv : v0;
        v1 = a[i]->tv > v1 ? a[i]->tv : v1;
    }
    for( i = 0; i < m8d3d_pal_nb; ++i ) {
        count[i] = 0;
    }
    for( i = 0; i < 3; ++i ) {
        for( j = 0; j < 3; ++j ) {
            int k = m8d3d_pal_find( m8d3d_texel( t, bits, u0 + (u1 - u0) * f[j], v0 + (v1 - v0) * f[i] ) );

            if( k >= 0 && ++count[k] > count[best] ) {
                best = k;
            }
        }
    }
    return( best );
}

/* As m8d3d_pal_quad, in integers: the texture is a power of two (POW2 is claimed), so a corner's
 * texel position in 16.16 comes from its float's bits, and the 387 is not touched. */
static int m8d3d_pal_quad_i( const M8D3D_TEX *t, const BYTE *bits, const M8D3D_TLVERTEX **a )
{
    int     count[M8D3D_PAL_BASES];
    long    u0;
    long    u1;
    long    v0;
    long    v1;
    int     lw = 0;
    int     lh = 0;
    int     best = 0;
    int     i;
    int     j;

    while( (1 << lw) < t->w ) {
        ++lw;
    }
    while( (1 << lh) < t->h ) {
        ++lh;
    }
    u0 = u1 = m8d3d_fround( &a[0]->tu, lw + 16 );
    v0 = v1 = m8d3d_fround( &a[0]->tv, lh + 16 );
    for( i = 1; i < 3; ++i ) {
        long u = m8d3d_fround( &a[i]->tu, lw + 16 );
        long v = m8d3d_fround( &a[i]->tv, lh + 16 );

        u0 = u < u0 ? u : u0;
        u1 = u > u1 ? u : u1;
        v0 = v < v0 ? v : v0;
        v1 = v > v1 ? v : v1;
    }
    for( i = 0; i < m8d3d_pal_nb; ++i ) {
        count[i] = 0;
    }
    /* At 1/6, 1/2 and 5/6 across, as the float version. */
    for( i = 1; i < 6; i += 2 ) {
        int y = (int)((v0 + (v1 - v0) / 6 * i + 32768L) >> 16) & (t->h - 1);
        const WORD *row = (const WORD *)(bits + y * t->pitch);

        for( j = 1; j < 6; j += 2 ) {
            int     x = (int)((u0 + (u1 - u0) / 6 * j + 32768L) >> 16) & (t->w - 1);
            DWORD   q = row[x];
            DWORD   rgb = t->is555 ? ((q & 0x7C00) << 9) | ((q & 0x03E0) << 6) | ((q & 0x001F) << 3) :
                                     ((q & 0xF800) << 8) | ((q & 0x07E0) << 5) | ((q & 0x001F) << 3);
            int     k = m8d3d_pal_find( rgb );

            if( k >= 0 && ++count[k] > count[best] ) {
                best = k;
            }
        }
    }
    /* A thin stripe of a colour the texture holds little of loses the vote in a far piece and
     * appears only once the piece is near and split: two hits of nine let it win. */
    if( m8d3d_accent ) {
        int r = best;

        for( i = 0; i < m8d3d_pal_nb; ++i ) {
            if( count[i] >= 2 && m8d3d_pal_count[i] < m8d3d_pal_count[r] ) {
                r = i;
            }
        }
        if( m8d3d_pal_count[r] * 4 < m8d3d_pal_count[best] ) {
            best = r;
        }
    }
    return( best );
}

/* The colour of a texture rectangle, kept: a program draws the same patch of its texture
 * frame after frame, and a sample is nine texel reads and eighteen float conversions. The
 * key is the texture's slot and the rectangle in 64ths of the texture; emptied when a slot
 * is reused. */
#define M8D3D_PATCH_HASH    1024
static DWORD    m8d3d_patch_key[M8D3D_PATCH_HASH];
static BYTE     m8d3d_patch_col[M8D3D_PATCH_HASH];

/* Shared by every process: a new context must not take the last program's colours, or colours
 * voted under other switches. */
static void m8d3d_patch_forget( void )
{
    memset( m8d3d_patch_key, 0, sizeof( m8d3d_patch_key ) );
}
/* The last patch's dither threshold, 0-255: the R2 sequence (plastic number, 1/p = 0.7549 and
 * 1/p^2 = 0.5698, times 256) over its corner in 64ths of the texture. Aperiodic, and tied to the
 * texture rather than the screen, so the pattern stays on the wall as the view moves. */
static int      m8d3d_patch_t;

static int m8d3d_pal_patch( const M8D3D_TEX *t, const BYTE *bits, const M8D3D_TLVERTEX **a )
{
    const float *u0 = &a[0]->tu;
    const float *u1 = &a[0]->tu;
    const float *v0 = &a[0]->tv;
    const float *v1 = &a[0]->tv;
    DWORD       key;
    unsigned    h;
    int         i;

    /* Integer compares on the bits: the 387 is slow here. */
    for( i = 1; i < 3; ++i ) {
        long u = m8d3d_ford( &a[i]->tu );
        long v = m8d3d_ford( &a[i]->tv );

        u0 = u < m8d3d_ford( u0 ) ? &a[i]->tu : u0;
        u1 = u > m8d3d_ford( u1 ) ? &a[i]->tu : u1;
        v0 = v < m8d3d_ford( v0 ) ? &a[i]->tv : v0;
        v1 = v > m8d3d_ford( v1 ) ? &a[i]->tv : v1;
    }
    /* Bit 31 marks a used slot, so key 0 is never a hit on an empty one. */
    key = 0x80000000UL | (DWORD)(t - m8d3d_texs) << 24 |
          ((DWORD)m8d3d_fround( u0, 6 ) & 63) << 18 | ((DWORD)m8d3d_fround( v0, 6 ) & 63) << 12 |
          ((DWORD)m8d3d_fround( u1, 6 ) & 63) << 6 | ((DWORD)m8d3d_fround( v1, 6 ) & 63);
    h = (unsigned)((key * 2654435761UL) >> 22) & (M8D3D_PATCH_HASH - 1);
    m8d3d_patch_t = (int)(((key >> 18 & 63) * 193 + (key >> 12 & 63) * 146) & 255);
    if( m8d3d_patch_key[h] != key ) {
        m8d3d_patch_key[h] = key;
        m8d3d_patch_col[h] = (BYTE)(m8d3d_diag == 7 ? m8d3d_pal_quad( t, bits, a ) : m8d3d_pal_quad_i( t, bits, a ));
    }
    return( m8d3d_patch_col[h] );
}

static DWORD __stdcall m8d3d_texture_create( M8D3D_TEXTURECREATE *d )
{
    int k;

    for( k = 0; k < M8D3D_TEXTURES && m8d3d_texs[k].dds != NULL; ++k ) {
    }
    if( k == M8D3D_TEXTURES ) {
        d->ddrval = DDERR_OUTOFVIDEOMEMORY;
        return( DDHAL_DRIVER_HANDLED );
    }
    m8d3d_texs[k].dds = d->lpDDS;
    m8d3d_texs[k].scanned = 0;
    /* The slot may have held another texture. */
    memset( m8d3d_patch_key, 0, sizeof( m8d3d_patch_key ) );
    d->dwHandle = k + 1;
    ++m8d3d_census.tex_created;
    {
        BYTE *bits;

        if( m8d3d_tex_read( &m8d3d_texs[k], &bits ) ) {
            ++m8d3d_census.tex_sizes[m8d3d_log2( m8d3d_texs[k].w > m8d3d_texs[k].h ? m8d3d_texs[k].w : m8d3d_texs[k].h, 11 )];
        }
    }
    d->ddrval = DD_OK;
    return( DDHAL_DRIVER_HANDLED );
}

static DWORD __stdcall m8d3d_texture_destroy( M8D3D_TEXTUREDESTROY *d )
{
    M8D3D_TEX   *t = m8d3d_tex_of( d->dwHandle );
    int         k;

    if( t != NULL ) {
        t->dds = NULL;
    }
    for( k = 0; k < M8D3D_CONTEXTS; ++k ) {
        if( m8d3d_ctx[k].tex == d->dwHandle ) {
            m8d3d_ctx[k].tex = 0;
        }
    }
    d->ddrval = DD_OK;
    return( DDHAL_DRIVER_HANDLED );
}

static DWORD __stdcall m8d3d_texture_swap( M8D3D_TEXTURESWAP *d )
{
    M8D3D_TEX   *a = m8d3d_tex_of( d->dwHandle1 );
    M8D3D_TEX   *b = m8d3d_tex_of( d->dwHandle2 );
    M8D3D_TEX   t;

    d->ddrval = DDERR_GENERIC;
    if( a != NULL && b != NULL ) {
        t = *a;
        *a = *b;
        *b = t;
        memset( m8d3d_patch_key, 0, sizeof( m8d3d_patch_key ) );
        d->ddrval = DD_OK;
    }
    return( DDHAL_DRIVER_HANDLED );
}

static DWORD __stdcall m8d3d_texture_getsurf( M8D3D_TEXTUREGETSURF *d )
{
    M8D3D_TEX *t = m8d3d_tex_of( d->dwHandle );

    d->ddrval = DDERR_GENERIC;
    if( t != NULL ) {
        d->lpDDS = t->dds;
        d->ddrval = DD_OK;
    }
    return( DDHAL_DRIVER_HANDLED );
}

static void m8d3d_states( M8D3D_CONTEXT *c, const M8D3D_STATE *s, DWORD n )
{
    DWORD i;

    for( i = 0; i < n; ++i ) {
        DWORD t = s[i].type < 99 ? s[i].type : 99;

        ++m8d3d_census.state_hits[t];
        m8d3d_census.state_values[t] |= 1UL << (s[i].arg < 31 ? s[i].arg : 31);
        if( t < 32 || t == M8D3D_RS_COLORKEY ) {
            DWORD bit = 1UL << (t == M8D3D_RS_COLORKEY ? 31 : t);

            c->rs_on = s[i].arg ? c->rs_on | bit : c->rs_on & ~bit;
        }
        if( s[i].type == M8D3D_RS_FOGCOLOR ) {
            c->fogcolor = s[i].arg;
        }
        if( s[i].type == M8D3D_RS_ZFUNC ) {
            c->zfunc = s[i].arg;
        }
        if( s[i].type == M8D3D_RS_CULLMODE ) {
            c->cull = s[i].arg;
        }
        if( s[i].type == M8D3D_RS_SHADEMODE ) {
            c->shade = s[i].arg;
        }
        if( s[i].type == M8D3D_RS_TEXTURE ) {
            c->tex = s[i].arg;
        }
        if( s[i].type == M8D3D_RS_TEXBLEND ) {
            c->texblend = s[i].arg;
        }
    }
}

static DWORD __stdcall m8d3d_render_state( M8D3D_RENDERSTATE *d )
{
    M8D3D_CONTEXT       *c = m8d3d_context( d->dwhContext );
    BYTE                *exe = m8d3d_buffer( d->lpExeBuf );

    ++m8d3d_stat[3];
    if( c != NULL && exe != NULL ) {
        m8d3d_states( c, (const M8D3D_STATE *)(exe + d->dwOffset), d->dwCount );
    }
    d->ddrval = DD_OK;
    return( DDHAL_DRIVER_HANDLED );
}

/* What one call's triangles share. */
typedef struct {
    M8D3D_CONTEXT   *c;
    M8D3D_TEX       *tex;
    BYTE            *texbits;
    LPVOID          lcl;
    int             x;
    int             y;
    int             w;
    int             h;
    int             fb16;
    int             ok;
    DWORD           rb;             /* the target's last column and row, as float bits */
    DWORD           bb;
} M8D3D_DRAW;

static int m8d3d_draw_begin( M8D3D_DRAW *p, M8D3D_CONTEXT *c )
{
    p->c = c;
    p->ok = 1;
    p->fb16 = m8d3d_fb16();
    m8d3d_owner_check();
    if( !p->fb16 ) {
        m8d3d_feed_stop( 13 );
        m8d3d_q = m8d3d_q_own;
        m8d3d_q_cap = M8D3D_Q_MAX;
    }
    /* A flip swaps the surfaces' memory, so the target is looked up each time. */
    if( c == NULL || !m8d3d_surface( m8d3d_lcl( c->target ), &p->x, &p->y, &p->w, &p->h ) || p->w > c->sw ||
        (!p->fb16 && (p->h > c->sh || !m8d3d_engine_begin())) ) {
        ++m8d3d_stat[11];
        return( 0 );
    }
    /* A new context's scratch holds whatever the card left there; its box was set
     * to the whole scratch when it was created. At 16 bpp, the replay sees to it. */
    if( !p->fb16 ) {
        m8d3d_qn = 0;
        m8d3d_qh = 0;
        m8d3d_q_next = M8D3D_Q_PUMP;
    }
    if( !p->fb16 && c->fresh ) {
        p->ok = m8d3d_scratch_clear( c );
        c->fresh = 0;
    }
    /* Once a call: it asks the kernel whether the pointer is good. */
    p->lcl = m8d3d_lcl( c->target );
    p->rb = m8d3d_ibits( p->w - 1 );
    p->bb = m8d3d_ibits( p->h - 1 );
    p->texbits = NULL;
    p->tex = m8d3d_tex_of( c->tex );
    if( p->tex != NULL && !m8d3d_tex_read( p->tex, &p->texbits ) ) {
        p->tex = NULL;
    }
    return( 1 );
}

static void m8d3d_draw_on( M8D3D_DRAW *p, const M8D3D_TLVERTEX *v0, const M8D3D_TLVERTEX *v1,
                           const M8D3D_TLVERTEX *v2 );

static DWORD m8d3d_lerp_colour( DWORD a, DWORD b, float t )
{
    DWORD   r = 0;
    int     s;

    for( s = 0; s < 32; s += 8 ) {
        int ca = (int)((a >> s) & 255);
        int cb = (int)((b >> s) & 255);

        r |= (DWORD)(ca + m8d3d_round( (float)(cb - ca) * t )) << s;
    }
    return( r );
}

/* Where an edge meets one side of the target. */
static void m8d3d_lerp( M8D3D_TLVERTEX *o, const M8D3D_TLVERTEX *a, const M8D3D_TLVERTEX *b, float t )
{
    o->sx = a->sx + (b->sx - a->sx) * t;
    o->sy = a->sy + (b->sy - a->sy) * t;
    o->sz = a->sz + (b->sz - a->sz) * t;
    o->rhw = a->rhw + (b->rhw - a->rhw) * t;
    o->tu = a->tu + (b->tu - a->tu) * t;
    o->tv = a->tv + (b->tv - a->tv) * t;
    o->color = m8d3d_lerp_colour( a->color, b->color, t );
    o->specular = a->specular;
}

/* side 0 and 1: x against lim, 2 and 3: y; odd sides keep what is at or below the limit. */
static int m8d3d_clip_side( const M8D3D_TLVERTEX *in, int n, M8D3D_TLVERTEX *out, int side, float lim )
{
    int k = 0;
    int i;

    for( i = 0; i < n; ++i ) {
        const M8D3D_TLVERTEX    *a = &in[i];
        const M8D3D_TLVERTEX    *b = &in[(i + 1) % n];
        float                   da = (side < 2 ? a->sx : a->sy) - lim;
        float                   db = (side < 2 ? b->sx : b->sy) - lim;

        if( side & 1 ) {
            da = -da;
            db = -db;
        }
        if( da >= 0.0f ) {
            out[k++] = *a;
        }
        if( (da >= 0.0f) != (db >= 0.0f) ) {
            m8d3d_lerp( &out[k++], a, b, da / (da - db) );
        }
    }
    return( k );
}

/* A corner in integers for clipping: x and y in sixteenths of a pixel, u and v in 16.16. */
typedef struct {
    long    x;
    long    y;
    long    u;
    long    v;
    DWORD   c;
    DWORD   spec;
    float   z;
} M8D3D_ICORNER;

/* A float's bits from a fixed-point value with `frac` fraction bits, truncated. Bits, not a float:
 * a float return goes through the 387's register. */
static DWORD m8d3d_fixf( long v, int frac )
{
    DWORD   s = 0;
    DWORD   m;
    int     e = 31;
    DWORD   b;

    if( v == 0 ) {
        return( 0 );
    }
    if( v < 0 ) {
        s = 0x80000000UL;
        m = (DWORD)-v;
    } else {
        m = (DWORD)v;
    }
    while( !(m & 0x80000000UL) ) {
        m <<= 1;
        --e;
    }
    b = s | (DWORD)(e - frac + 127) << 23 | (m >> 8 & 0x007FFFFFUL);
    return( b );
}

/* As m8d3d_clip_side, on integer corners: t in 16.16, products in 64 bits, since a corner
 * outside the target can be far outside. z only when depth is used. */
static int m8d3d_clip_side_i( const M8D3D_ICORNER *in, int n, M8D3D_ICORNER *out, int side, long lim, int zon )
{
    int k = 0;
    int i;

    for( i = 0; i < n; ++i ) {
        const M8D3D_ICORNER *a = &in[i];
        const M8D3D_ICORNER *b = &in[(i + 1) % n];
        long                da = (side < 2 ? a->x : a->y) - lim;
        long                db = (side < 2 ? b->x : b->y) - lim;

        if( side & 1 ) {
            da = -da;
            db = -db;
        }
        if( da >= 0 ) {
            out[k++] = *a;
        }
        if( (da >= 0) != (db >= 0) ) {
            M8D3D_ICORNER   *o = &out[k++];
            long            t = (long)(((long long)da << 16) / (da - db));
            int             sh;

            o->x = a->x + (long)(((long long)(b->x - a->x) * t) >> 16);
            o->y = a->y + (long)(((long long)(b->y - a->y) * t) >> 16);
            o->u = a->u + (long)(((long long)(b->u - a->u) * t) >> 16);
            o->v = a->v + (long)(((long long)(b->v - a->v) * t) >> 16);
            o->c = 0;
            for( sh = 0; sh < 32; sh += 8 ) {
                long ca = (long)(a->c >> sh & 255);
                long cb = (long)(b->c >> sh & 255);

                o->c |= (DWORD)(ca + ((cb - ca) * t >> 16)) << sh;
            }
            o->spec = a->spec;
            o->z = zon ? a->z + (b->z - a->z) * ((float)t * (1.0f / 65536.0f)) : a->z;
        }
    }
    return( k );
}

/* The rasteriser fills its outline too, so the last row and column in are the limits: a
 * full-target quad, 0-640 by 0-480 in Direct3D's convention, then covers every pixel. */
static void m8d3d_draw_clipped( M8D3D_DRAW *p, const M8D3D_TLVERTEX *v0, const M8D3D_TLVERTEX *v1,
                                const M8D3D_TLVERTEX *v2 )
{
    M8D3D_TLVERTEX  a[M8D3D_CLIP_MAX];
    M8D3D_TLVERTEX  b[M8D3D_CLIP_MAX];
    int             n = 3;
    int             i;

    a[0] = *v0;
    a[1] = *v1;
    a[2] = *v2;
    /* Most triangles cross one side: the others are skipped, found from the float bits. Each
     * pass copies every corner and works in float on the 387. A NaN corner is past every side. */
    {
        long    rx = m8d3d_ford( (const float *)&p->rb );
        long    by = m8d3d_ford( (const float *)&p->bb );
        int     out = 0;

        for( i = 0; i < 3; ++i ) {
            long x = m8d3d_ford( &a[i].sx );
            long y = m8d3d_ford( &a[i].sy );

            out |= (x < 0 ? 1 : 0) | (x > rx ? 2 : 0) | (y < 0 ? 4 : 0) | (y > by ? 8 : 0);
        }
        if( m8d3d_diag != 8 ) {
            M8D3D_ICORNER   ia[M8D3D_CLIP_MAX];
            M8D3D_ICORNER   ib[M8D3D_CLIP_MAX];
            long            xl = (long)(p->w - 1) << 4;
            long            yl = (long)(p->h - 1) << 4;
            int             zon = (int)((p->c->rs_on >> M8D3D_RS_ZENABLE) & 1);

            for( i = 0; i < 3; ++i ) {
                ia[i].x = m8d3d_fround( &a[i].sx, 4 );
                ia[i].y = m8d3d_fround( &a[i].sy, 4 );
                ia[i].u = m8d3d_fround( &a[i].tu, 16 );
                ia[i].v = m8d3d_fround( &a[i].tv, 16 );
                ia[i].c = a[i].color;
                ia[i].spec = a[i].specular;
                ia[i].z = a[i].sz;
            }
            if( out & 1 ) {
                n = m8d3d_clip_side_i( ia, n, ib, 0, 0, zon );
                memcpy( ia, ib, n * sizeof( ia[0] ) );
            }
            if( out & 2 ) {
                n = m8d3d_clip_side_i( ia, n, ib, 1, xl, zon );
                memcpy( ia, ib, n * sizeof( ia[0] ) );
            }
            if( out & 4 ) {
                n = m8d3d_clip_side_i( ia, n, ib, 2, 0, zon );
                memcpy( ia, ib, n * sizeof( ia[0] ) );
            }
            if( out & 8 ) {
                n = m8d3d_clip_side_i( ia, n, ib, 3, yl, zon );
                memcpy( ia, ib, n * sizeof( ia[0] ) );
            }
            /* On the target: the clamp the float path does below, done here in integers. */
            for( i = 0; i < n; ++i ) {
                long x = ia[i].x < 0 ? 0 : ia[i].x > xl ? xl : ia[i].x;
                long y = ia[i].y < 0 ? 0 : ia[i].y > yl ? yl : ia[i].y;

                a[i] = *v0;
                *(DWORD *)&a[i].sx = m8d3d_fixf( x, 4 );
                *(DWORD *)&a[i].sy = m8d3d_fixf( y, 4 );
                *(DWORD *)&a[i].tu = m8d3d_fixf( ia[i].u, 16 );
                *(DWORD *)&a[i].tv = m8d3d_fixf( ia[i].v, 16 );
                a[i].color = ia[i].c;
                a[i].specular = ia[i].spec;
                a[i].sz = ia[i].z;
            }
            for( i = 1; p->ok && i + 1 < n; ++i ) {
                m8d3d_draw_on( p, &a[0], &a[i], &a[i + 1] );
            }
            return;
        }
        if( out & 1 ) {
            n = m8d3d_clip_side( a, n, b, 0, 0.0f );
            memcpy( a, b, n * sizeof( a[0] ) );
        }
        if( out & 2 ) {
            n = m8d3d_clip_side( a, n, b, 1, (float)(p->w - 1) );
            memcpy( a, b, n * sizeof( a[0] ) );
        }
        if( out & 4 ) {
            n = m8d3d_clip_side( a, n, b, 2, 0.0f );
            memcpy( a, b, n * sizeof( a[0] ) );
        }
        if( out & 8 ) {
            n = m8d3d_clip_side( a, n, b, 3, (float)(p->h - 1) );
            memcpy( a, b, n * sizeof( a[0] ) );
        }
    }
    /* A cut corner can land a rounding error past the side it was cut on, so each is put on the
     * target before drawing, and the pieces are not checked again. */
    for( i = 0; i < n; ++i ) {
        a[i].sx = a[i].sx < 0.0f ? 0.0f : a[i].sx > (float)(p->w - 1) ? (float)(p->w - 1) : a[i].sx;
        a[i].sy = a[i].sy < 0.0f ? 0.0f : a[i].sy > (float)(p->h - 1) ? (float)(p->h - 1) : a[i].sy;
    }
    for( i = 1; p->ok && i + 1 < n; ++i ) {
        m8d3d_draw_on( p, &a[0], &a[i], &a[i + 1] );
    }
}

#define M8D3D_SPLIT_PX  80              /* a piece's longer side, in the drawn pixels */
#define M8D3D_SPLIT2_PX 160             /* and split again: only the nearest panels are this large */
static int  m8d3d_split_depth;

/* Halfway along an edge, every field the drawing reads. */
static void m8d3d_mid( M8D3D_TLVERTEX *o, const M8D3D_TLVERTEX *a, const M8D3D_TLVERTEX *b )
{
    *o = *a;
    o->sx = (a->sx + b->sx) * 0.5f;
    o->sy = (a->sy + b->sy) * 0.5f;
    o->sz = (a->sz + b->sz) * 0.5f;
    o->tu = (a->tu + b->tu) * 0.5f;
    o->tv = (a->tv + b->tv) * 0.5f;
    o->color = ((a->color >> 1) & 0x7F7F7F7FUL) + ((b->color >> 1) & 0x7F7F7F7FUL);
}

/* All three corners on the target. */
static void m8d3d_draw_on( M8D3D_DRAW *p, const M8D3D_TLVERTEX *v0, const M8D3D_TLVERTEX *v1,
                           const M8D3D_TLVERTEX *v2 )
{
    M8D3D_CONTEXT           *c = p->c;
    const M8D3D_TLVERTEX    *a[3];
    short                   v[6];
    DWORD                   texc[3];
    DWORD                   rgb;
    long                    area;
    int                     j;
    int                     fog;
    int                     side;

    if( !p->ok || m8d3d_diag == 5 ) {
        return;
    }
    a[0] = v0;
    a[1] = v1;
    a[2] = v2;
    for( j = 0; j < 3; ++j ) {
        v[2 * j] = (short)m8d3d_fround( &a[j]->sx, -m8d3d_half );
        v[2 * j + 1] = (short)m8d3d_fround( &a[j]->sy, -m8d3d_half );
    }
    {
        int     lx = v[0] < v[2] ? v[0] : v[2];
        int     hx = v[0] > v[2] ? v[0] : v[2];
        int     ly = v[1] < v[3] ? v[1] : v[3];
        int     hy = v[1] > v[3] ? v[1] : v[3];
        DWORD   on = c->rs_on;

        lx = v[4] < lx ? v[4] : lx;
        hx = v[4] > hx ? v[4] : hx;
        ly = v[5] < ly ? v[5] : ly;
        hy = v[5] > hy ? v[5] : hy;
        side = hx - lx > hy - ly ? hx - lx : hy - ly;
        ++m8d3d_census.tri_sizes[m8d3d_log2( side, 9 )];
        ++m8d3d_census.tris;
        m8d3d_census.tris_textured += c->tex != 0;
        m8d3d_census.tris_z += (on >> M8D3D_RS_ZENABLE) & 1;
        m8d3d_census.tris_blend += (on >> M8D3D_RS_ALPHABLEND) & 1;
        m8d3d_census.tris_alphatest += (on >> M8D3D_RS_ALPHATEST) & 1;
        m8d3d_census.tris_colorkey += on >> 31;
        m8d3d_census.tris_fog += (on >> M8D3D_RS_FOG) & 1;
        m8d3d_census.tris_perspective += (on >> M8D3D_RS_PERSPECTIVE) & 1;
    }
    /* Above zero runs clockwise on the screen. Facing is taken from the corners at an eighth of a
     * pixel, not the rounded ones: rounding, at half size most, can turn a thin back face round,
     * and joined patches fill in colour order, so a kept back face shows through the wall. */
    area = (long)(v[2] - v[0]) * (v[5] - v[1]) - (long)(v[3] - v[1]) * (v[4] - v[0]);
    if( area != 0 && c->cull != M8D3D_CULL_NONE ) {
        long    s[6];

        for( j = 0; j < 3; ++j ) {
            s[2 * j] = m8d3d_fround( &a[j]->sx, 3 );
            s[2 * j + 1] = m8d3d_fround( &a[j]->sy, 3 );
        }
        /* Corners on the target, so 8 x 640 by 8 x 480 at most: the products fit. */
        area = ((s[2] - s[0]) * (s[5] - s[1]) - (s[3] - s[1]) * (s[4] - s[0])) < 0 ? -1 : 1;
    }
    if( area == 0 || (c->cull == M8D3D_CULL_CCW && area < 0) ||
        (c->cull == M8D3D_CULL_CW && area > 0) ) {
        ++m8d3d_stat[9];
        return;
    }
    if( m8d3d_diag == 2 ) {
        return;
    }
    /* A large textured piece takes one colour, so a stripe sliding through its part of the
     * texture holds the whole piece in its colour: split in four, once, each part
     * sampled for itself. Parts that come out the same colour join again. Only the large ones:
     * splitting every piece cost four times the time (M8D3EO). */
    if( p->tex != NULL && ((side > M8D3D_SPLIT_PX && m8d3d_split_depth < 1) ||
                           (side > M8D3D_SPLIT2_PX && m8d3d_split_depth < 2)) ) {
        M8D3D_TLVERTEX  m[3];

        for( j = 0; j < 3; ++j ) {
            m8d3d_mid( &m[j], a[j], a[(j + 1) % 3] );
        }
        ++m8d3d_split_depth;
        m8d3d_draw_on( p, a[0], &m[0], &m[2] );
        m8d3d_draw_on( p, &m[0], a[1], &m[1] );
        m8d3d_draw_on( p, &m[2], &m[1], a[2] );
        m8d3d_draw_on( p, &m[0], &m[1], &m[2] );
        --m8d3d_split_depth;
        return;
    }
    rgb = m8d3d_mean( a[0]->color, a[1]->color, a[2]->color );
    if( p->tex != NULL && p->fb16 && !((c->rs_on >> M8D3D_RS_FOG) & 1) ) {
        m8d3d_pal_scan( p->tex, p->texbits );
    }
    if( p->tex != NULL && p->fb16 && m8d3d_pal_nb > 0 && !((c->rs_on >> M8D3D_RS_FOG) & 1) ) {
        int     lo_x = v[0] < v[2] ? v[0] : v[2];
        int     hi_x = v[0] > v[2] ? v[0] : v[2];
        int     lo_y = v[1] < v[3] ? v[1] : v[3];
        int     hi_y = v[1] > v[3] ? v[1] : v[3];
        int     distant;
        int     k;
        int     level = 15;
        DWORD   corners[3];

        lo_x = v[4] < lo_x ? v[4] : lo_x;
        hi_x = v[4] > hi_x ? v[4] : hi_x;
        lo_y = v[5] < lo_y ? v[5] : lo_y;
        hi_y = v[5] > hi_y ? v[5] : hi_y;
        distant = hi_x - lo_x <= M8D3D_FAR_PX && hi_y - lo_y <= M8D3D_FAR_PX;
        /* Far patches take their own colour too, kept: a stripe must not appear only as it nears. */
        k = m8d3d_pal_patch( p->tex, p->texbits, a );
        if( c->texblend == M8D3D_TEXBLEND_MODULATE || c->texblend == M8D3D_TEXBLEND_MODULATEALPHA ) {
            long lv = (long)((rgb >> 16 & 255) + (rgb >> 8 & 255) + (rgb & 255)) * 15 * 256 / 765;

            /* Between two of the 16 levels, the piece takes the upper one as often as the
             * fraction says, by an aperiodic threshold: the eye sees a gradient, not a step, and
             * no command is added. Far pieces keep coarse steps, which join into fewer patches. */
            level = (int)(lv >> 8);
            if( !distant && level < 15 && m8d3d_diag != 4 && (int)(lv & 255) > m8d3d_patch_t ) {
                ++level;
            } else if( distant || m8d3d_diag == 4 ) {
                level = (int)((lv + 128) >> 8);
            }
        }
        if( distant && c->texblend != M8D3D_TEXBLEND_MODULATE && c->texblend != M8D3D_TEXBLEND_MODULATEALPHA ) {
            level &= ~3;
        } else if( distant ) {
            /* Every fourth level, the nearest: rounding down would darken the far rings by up to three. */
            level = (int)((((long)((rgb >> 16 & 255) + (rgb >> 8 & 255) + (rgb & 255)) * 15 * 256 / 765) + 512) >> 10) << 2;
            level = level > 15 ? 15 : level;
        }
        rgb = m8d3d_pal_rgb( k, level );
        corners[0] = corners[1] = corners[2] = rgb;
        m8d3d_pal_want = 1;
        if( m8d3d_diag == 3 ) {
            return;
        }
        p->ok = m8d3d_record( p->lcl, v, (BYTE)(k * 16 + level), m8d3d_565( rgb ), corners,
                              (((c->rs_on >> M8D3D_RS_ZENABLE) & 1) && c->zfunc != M8D3D_CMP_ALWAYS) || m8d3d_zsort ?
                              (int)m8d3d_round( (a[0]->sz + a[1]->sz + a[2]->sz) * M8D3D_DEPTH_SCALE ) : -1 );
        return;
    }
    if( p->tex != NULL ) {
        for( j = 0; j < 3; ++j ) {
            texc[j] = m8d3d_texel( p->tex, p->texbits, a[j]->tu, a[j]->tv );
            if( c->texblend == M8D3D_TEXBLEND_MODULATE || c->texblend == M8D3D_TEXBLEND_MODULATEALPHA ) {
                texc[j] = m8d3d_modulate( texc[j], a[j]->color );
            }
        }
        rgb = m8d3d_mean( texc[0], texc[1], texc[2] );
    }
    /* Distance fades towards the fog colour across the triangle: the gradient path draws it. */
    fog = (c->rs_on >> M8D3D_RS_FOG) & 1;
    if( fog ) {
        for( j = 0; j < 3; ++j ) {
            DWORD from = p->tex != NULL ? texc[j] :
                         c->shade == M8D3D_SHADE_GOURAUD ? a[j]->color & 0x00FFFFFFUL : rgb;

            texc[j] = m8d3d_fogged( from, c->fogcolor, a[j]->specular >> 24 );
        }
        rgb = m8d3d_mean( texc[0], texc[1], texc[2] );
    }
    if( p->fb16 ) {
        DWORD corners[3];

        /* Flat shading keeps one colour, the corners' mean: a face's two triangles then
         * agree where the first vertex alone would not. */
        for( j = 0; j < 3; ++j ) {
            corners[j] = side < M8D3D_GRAD_MIN || p->tex != NULL ? rgb : fog ? texc[j] :
                         c->shade == M8D3D_SHADE_GOURAUD ? a[j]->color & 0x00FFFFFFUL : rgb;
        }
        if( m8d3d_pmatch && p->tex == NULL ) {
            DWORD   shown;
            int     pk = m8d3d_pal_match( rgb, &shown );

            if( pk >= 0 ) {
                corners[0] = corners[1] = corners[2] = shown;
                m8d3d_pal_want = 1;
                p->ok = m8d3d_record( p->lcl, v, (BYTE)pk, m8d3d_565( shown ), corners,
                                      (((c->rs_on >> M8D3D_RS_ZENABLE) & 1) && c->zfunc != M8D3D_CMP_ALWAYS) || m8d3d_zsort ?
                                      (int)m8d3d_round( (a[0]->sz + a[1]->sz + a[2]->sz) * M8D3D_DEPTH_SCALE ) : -1 );
                return;
            }
        }
        /* Described, not drawn: Flip has the engine draw it, furthest first if Z is on. */
        p->ok = m8d3d_record( p->lcl, v, m8d3d_index16( rgb ), m8d3d_565( rgb ), corners,
                              (((c->rs_on >> M8D3D_RS_ZENABLE) & 1) && c->zfunc != M8D3D_CMP_ALWAYS) || m8d3d_zsort ?
                              (int)m8d3d_round( (a[0]->sz + a[1]->sz + a[2]->sz) * M8D3D_DEPTH_SCALE ) : -1 );
        return;
    }
    p->ok = m8d3d_tri_fit( c, p->x, p->y, v, m8d3d_index( rgb ) );
    ++m8d3d_stat[8];
}

static void m8d3d_draw_tri( M8D3D_DRAW *p, const M8D3D_TLVERTEX *v0, const M8D3D_TLVERTEX *v1,
                            const M8D3D_TLVERTEX *v2 )
{
    DWORD   r = p->rb;
    DWORD   b = p->bb;

    if( !p->ok || m8d3d_diag == 1 ) {
        return;
    }
    /* On the bits, unsigned: a negative corner has bit 31 set and a NaN is above any finite
     * limit, so both fail and go to the clipper, as -0.0 does. */
#define M8D3D_ON( f, lim ) ( *(const DWORD *)&(f) <= (lim) )
    if( M8D3D_ON( v0->sx, r ) && M8D3D_ON( v0->sy, b ) && M8D3D_ON( v1->sx, r ) && M8D3D_ON( v1->sy, b ) &&
        M8D3D_ON( v2->sx, r ) && M8D3D_ON( v2->sy, b ) ) {
        m8d3d_draw_on( p, v0, v1, v2 );
        return;
    }
    ++m8d3d_stat[10];
    ++m8d3d_census.tris_clipped;
    if( m8d3d_diag == 6 ) {
        return;
    }
    m8d3d_draw_clipped( p, v0, v1, v2 );
}

static void m8d3d_draw_end( M8D3D_DRAW *p )
{
    if( !p->fb16 ) {
        if( p->ok ) {
            p->ok = m8d3d_scratch_clear( p->c );
        }
        if( !m8d3d_q_flush() ) {
            p->ok = 0;
        }
        m8d3d_engine_end();
    }
    if( !p->ok ) {
        ++m8d3d_stat[11];
    }
}

static DWORD __stdcall m8d3d_render_primitive( M8D3D_RENDERPRIMITIVE *d )
{
    M8D3D_CONTEXT           *c = m8d3d_context( d->dwhContext );
    BYTE                    *exe = m8d3d_buffer( d->lpExeBuf );
    BYTE                    *tl = m8d3d_buffer( d->lpTLBuf );
    const M8D3D_TLVERTEX    *vtx;
    const M8D3D_TRIANGLE    *tri;
    unsigned                i;
    M8D3D_DRAW              p;
    LARGE_INTEGER           rp0;
    LARGE_INTEGER           rp1;

    ++m8d3d_stat[4];
    m8d3d_stat[5] += d->wCount;
    d->ddrval = DD_OK;
    QueryPerformanceCounter( &rp0 );
    ++m8d3d_census.calls_exec;
    if( d->bOpcode != M8D3D_OP_TRIANGLE ) {
        ++m8d3d_census.other_ops;
        return( DDHAL_DRIVER_HANDLED );
    }
    ++m8d3d_census.batch_sizes[m8d3d_log2( d->wCount, 9 )];
    if( exe == NULL || tl == NULL || d->bSize < sizeof( M8D3D_TRIANGLE ) || !m8d3d_draw_begin( &p, c ) ) {
        ++m8d3d_stat[11];
        return( DDHAL_DRIVER_HANDLED );
    }
    vtx = (const M8D3D_TLVERTEX *)(tl + d->dwTLOffset);
    for( i = 0; p.ok && i < d->wCount; ++i ) {
        tri = (const M8D3D_TRIANGLE *)(exe + d->dwOffset + (DWORD)i * d->bSize);
        m8d3d_draw_tri( &p, &vtx[tri->v1], &vtx[tri->v2], &vtx[tri->v3] );
    }
    m8d3d_draw_end( &p );
    QueryPerformanceCounter( &rp1 );
    m8d3d_stat[24] += rp1.LowPart - rp0.LowPart;
    return( DDHAL_DRIVER_HANDLED );
}

/* A list, strip or fan, from an array or through indices. Lines and points are not
 * claimed, so a program that sends them anyway gets nothing drawn. */
static void m8d3d_draw_list( M8D3D_DRAW *p, DWORD type, const M8D3D_TLVERTEX *vtx, const WORD *index, DWORD n )
{
    DWORD i;

    ++m8d3d_census.prim_types[type & 7];
    ++m8d3d_census.batch_sizes[m8d3d_log2( n / (type == M8D3D_PT_TRIANGLELIST ? 3 : 1), 9 )];
#define M8D3D_V( k ) (index != NULL ? &vtx[index[k]] : &vtx[k])
    if( type == M8D3D_PT_TRIANGLELIST ) {
        for( i = 0; p->ok && i + 2 < n; i += 3 ) {
            m8d3d_draw_tri( p, M8D3D_V( i ), M8D3D_V( i + 1 ), M8D3D_V( i + 2 ) );
        }
    } else if( type == M8D3D_PT_TRIANGLESTRIP ) {
        /* Every second triangle of a strip runs the other way round. */
        for( i = 0; p->ok && i + 2 < n; ++i ) {
            if( i & 1 ) {
                m8d3d_draw_tri( p, M8D3D_V( i + 1 ), M8D3D_V( i ), M8D3D_V( i + 2 ) );
            } else {
                m8d3d_draw_tri( p, M8D3D_V( i ), M8D3D_V( i + 1 ), M8D3D_V( i + 2 ) );
            }
        }
    } else if( type == M8D3D_PT_TRIANGLEFAN ) {
        for( i = 1; p->ok && i + 1 < n; ++i ) {
            m8d3d_draw_tri( p, M8D3D_V( 0 ), M8D3D_V( i ), M8D3D_V( i + 1 ) );
        }
    }
#undef M8D3D_V
}

static DWORD __stdcall m8d3d_draw_one( M8D3D_DRAWONE *d )
{
    M8D3D_DRAW      p;
    LARGE_INTEGER   rp0;
    LARGE_INTEGER   rp1;

    ++m8d3d_stat[1];
    ++m8d3d_census.calls_one;
    QueryPerformanceCounter( &rp0 );
    d->ddrval = DD_OK;
    if( d->VertexType == M8D3D_VT_TLVERTEX && m8d3d_draw_begin( &p, m8d3d_context( d->dwhContext ) ) ) {
        m8d3d_draw_list( &p, d->PrimitiveType, (const M8D3D_TLVERTEX *)d->lpvVertices, NULL, d->dwNumVertices );
        m8d3d_draw_end( &p );
    }
    QueryPerformanceCounter( &rp1 );
    m8d3d_stat[24] += rp1.LowPart - rp0.LowPart;
    return( DDHAL_DRIVER_HANDLED );
}

static DWORD __stdcall m8d3d_draw_one_indexed( M8D3D_DRAWONEINDEXED *d )
{
    M8D3D_DRAW      p;
    LARGE_INTEGER   rp0;
    LARGE_INTEGER   rp1;

    ++m8d3d_stat[1];
    ++m8d3d_census.calls_indexed;
    QueryPerformanceCounter( &rp0 );
    d->ddrval = DD_OK;
    if( d->VertexType == M8D3D_VT_TLVERTEX && m8d3d_draw_begin( &p, m8d3d_context( d->dwhContext ) ) ) {
        m8d3d_draw_list( &p, d->PrimitiveType, (const M8D3D_TLVERTEX *)d->lpvVertices, d->lpwIndices, d->dwNumIndices );
        m8d3d_draw_end( &p );
    }
    QueryPerformanceCounter( &rp1 );
    m8d3d_stat[24] += rp1.LowPart - rp0.LowPart;
    return( DDHAL_DRIVER_HANDLED );
}

/* Counts, then that many state pairs unaligned, then vertices on a 32-byte boundary, and a
 * count of no vertices ends it. */
static DWORD __stdcall m8d3d_draw_primitives( M8D3D_DRAWPRIMITIVES *d )
{
    M8D3D_CONTEXT       *c = m8d3d_context( d->dwhContext );
    BYTE                *q = (BYTE *)d->lpvData;
    const M8D3D_COUNTS  *n;
    M8D3D_DRAW          p;
    LARGE_INTEGER       rp0;
    LARGE_INTEGER       rp1;

    ++m8d3d_stat[1];
    ++m8d3d_census.calls_prims;
    QueryPerformanceCounter( &rp0 );
    d->ddrval = DD_OK;
    for( ;; ) {
        n = (const M8D3D_COUNTS *)q;
        q += sizeof( *n );
        ++m8d3d_census.state_blocks[m8d3d_log2( n->wNumStateChanges, 7 )];
        if( c != NULL ) {
            m8d3d_states( c, (const M8D3D_STATE *)q, n->wNumStateChanges );
        }
        q += n->wNumStateChanges * sizeof( M8D3D_STATE );
        if( n->wNumVertices == 0 ) {
            break;
        }
        q = (BYTE *)(((DWORD)q + 31UL) & ~31UL);
        if( n->wVertexType == M8D3D_VT_TLVERTEX && m8d3d_draw_begin( &p, c ) ) {
            m8d3d_draw_list( &p, n->wPrimitiveType, (const M8D3D_TLVERTEX *)q, NULL, n->wNumVertices );
            m8d3d_draw_end( &p );
        }
        q += (DWORD)n->wNumVertices * sizeof( M8D3D_TLVERTEX );
    }
    QueryPerformanceCounter( &rp1 );
    m8d3d_stat[24] += rp1.LowPart - rp0.LowPart;
    return( DDHAL_DRIVER_HANDLED );
}

/* A target of the same size only: scratch and masks were placed for the one it was made with. */
static DWORD __stdcall m8d3d_set_target( M8D3D_SETTARGET *d )
{
    M8D3D_CONTEXT   *c = m8d3d_context( d->dwhContext );
    int             x;
    int             y;
    int             w;
    int             h;

    ++m8d3d_census.targets_set;
    d->ddrval = DDERR_GENERIC;
    if( c != NULL && m8d3d_surface( m8d3d_lcl( d->lpDDS ), &x, &y, &w, &h ) && w <= c->sw ) {
        c->target = d->lpDDS;
        d->ddrval = DD_OK;
    }
    return( DDHAL_DRIVER_HANDLED );
}

/* The size a caller gave bounds the copy: a later runtime's table is not ours to overrun. */
static DWORD __stdcall m8d3d_driver_info( M8D3D_DRIVERINFO *d )
{
    DWORD   g = *(const DWORD *)d->guidInfo;
    int     k;

    for( k = 0; k < 16 && m8d3d_census.info_data1[k] != g && m8d3d_census.info_data1[k] != 0; ++k ) {
    }
    if( k < 16 ) {
        m8d3d_census.info_data1[k] = g;
        ++m8d3d_census.info_hits[k];
    }
    d->dwActualSize = 0;
    d->ddRVal = DDERR_CURRENTLYNOTAVAIL;
    if( *(const DWORD *)d->guidInfo == M8D3D_GUID_CALLBACKS2_DATA1 && d->lpvData != NULL ) {
        d->dwActualSize = sizeof( m8d3d_callbacks2 );
        memcpy( d->lpvData, &m8d3d_callbacks2,
                d->dwExpectedSize < sizeof( m8d3d_callbacks2 ) ? d->dwExpectedSize : sizeof( m8d3d_callbacks2 ) );
        d->ddRVal = DD_OK;
    }
    if( g == M8D3D_GUID_EXTCAPS_DATA1 && d->lpvData != NULL && d->dwExpectedSize >= sizeof( DWORD ) ) {
        DWORD n = d->dwExpectedSize < sizeof( m8d3d_extcaps ) ? d->dwExpectedSize : sizeof( m8d3d_extcaps );

        memcpy( d->lpvData, &m8d3d_extcaps, n );
        *(DWORD *)d->lpvData = n;
        d->dwActualSize = sizeof( m8d3d_extcaps );
        d->ddRVal = DD_OK;
    }
    if( g == M8D3D_GUID_ZFORMATS_DATA1 && d->lpvData != NULL && d->dwExpectedSize >= sizeof( m8d3d_zformats ) ) {
        d->dwActualSize = sizeof( m8d3d_zformats );
        memcpy( d->lpvData, &m8d3d_zformats, sizeof( m8d3d_zformats ) );
        d->ddRVal = DD_OK;
    }
    return( DDHAL_DRIVER_HANDLED );
}

/* The two tables, filled; their linear addresses for the HAL information. */
void m8d3d_init( DWORD *global, DWORD *callbacks, DWORD *driver_info )
{
    M8D3D_DEVICEDESC *c = &m8d3d_global.hwCaps;
    int k;

    memset( &m8d3d_global, 0, sizeof( m8d3d_global ) );
    m8d3d_global.dwSize = sizeof( m8d3d_global );
    c->dwSize = sizeof( *c );
    c->dwFlags = M8D3D_DD_COLORMODEL | M8D3D_DD_DEVCAPS | M8D3D_DD_TRICAPS |
                 M8D3D_DD_RENDERBITDEPTH | M8D3D_DD_ZBUFFERBITDEPTH;
    c->dcmColorModel = M8D3D_COLOR_RGB;
    /* The runtime wants all four even of a device that keeps no vertices. */
    c->dwDevCaps = M8D3D_DEVCAPS_FLOATTLVERTEX | M8D3D_DEVCAPS_EXECUTESYSTEMMEMORY |
                   M8D3D_DEVCAPS_TLVERTEXSYSTEMMEMORY | M8D3D_DEVCAPS_DRAWPRIMTLVERTEX |
                   M8D3D_DEVCAPS_TEXTURESYSTEMMEMORY |
                   /* At 16 bpp also on the card (m8dd_tex_create). */
                   M8D3D_DEVCAPS_TEXTUREVIDEOMEMORY |
                   /* Flip has drawn the frame before it returns: nothing for the runtime to wait on. */
                   M8D3D_DEVCAPS_CANRENDERAFTERFLIP;
    c->dtcTransformCaps[0] = sizeof( c->dtcTransformCaps );
    c->dlcLightingCaps[0] = sizeof( c->dlcLightingCaps );
    c->dpcLineCaps.dwSize = sizeof( c->dpcLineCaps );
    c->dpcTriCaps.dwSize = sizeof( c->dpcTriCaps );
    c->dpcTriCaps.dwMiscCaps = M8D3D_PMISCCAPS_CULLNONE;
    c->dpcTriCaps.dwShadeCaps = M8D3D_PSHADECAPS_COLORFLATRGB | M8D3D_PSHADECAPS_COLORGOURAUDRGB |
                                M8D3D_PSHADECAPS_ALPHAFLATBLEND | M8D3D_PSHADECAPS_ALPHAGOURAUDBLEND |
                                M8D3D_PSHADECAPS_FOGFLAT | M8D3D_PSHADECAPS_FOGGOURAUD;
    /* What the sort at Flip honours: nearer wins, or a program's own order. */
    c->dpcTriCaps.dwRasterCaps = M8D3D_PRASTERCAPS_ZTEST | M8D3D_PRASTERCAPS_ZBUFFERLESSHSR |
                                 M8D3D_PRASTERCAPS_FOGVERTEX;
    c->dpcTriCaps.dwSrcBlendCaps = M8D3D_PBLENDCAPS_ZERO | M8D3D_PBLENDCAPS_ONE | M8D3D_PBLENDCAPS_SRCALPHA |
                                   M8D3D_PBLENDCAPS_INVSRCALPHA;
    c->dpcTriCaps.dwDestBlendCaps = c->dpcTriCaps.dwSrcBlendCaps;
    c->dpcTriCaps.dwAlphaCmpCaps = M8D3D_PCMPCAPS_ALWAYS;
    c->dpcTriCaps.dwZCmpCaps = M8D3D_PCMPCAPS_NEVER | M8D3D_PCMPCAPS_LESS | M8D3D_PCMPCAPS_LESSEQUAL |
                               M8D3D_PCMPCAPS_ALWAYS;
    c->dpcTriCaps.dwMiscCaps |= M8D3D_PMISCCAPS_MASKZ;
    c->dpcTriCaps.dwTextureCaps = M8D3D_PTEXTURECAPS_POW2 | M8D3D_PTEXTURECAPS_PERSPECTIVE |
                                  M8D3D_PTEXTURECAPS_ALPHA | M8D3D_PTEXTURECAPS_TRANSPARENCY;
    c->dpcTriCaps.dwTextureFilterCaps = M8D3D_PTFILTERCAPS_NEAREST | M8D3D_PTFILTERCAPS_LINEAR |
                                        M8D3D_PTFILTERCAPS_MIPNEAREST | M8D3D_PTFILTERCAPS_MIPLINEAR;
    c->dpcTriCaps.dwTextureBlendCaps = M8D3D_PTBLENDCAPS_DECAL | M8D3D_PTBLENDCAPS_MODULATE |
                                       M8D3D_PTBLENDCAPS_DECALALPHA | M8D3D_PTBLENDCAPS_MODULATEALPHA |
                                       M8D3D_PTBLENDCAPS_COPY;
    c->dpcTriCaps.dwTextureAddressCaps = M8D3D_PTADDRESSCAPS_WRAP | M8D3D_PTADDRESSCAPS_MIRROR |
                                         M8D3D_PTADDRESSCAPS_CLAMP;
    /* Only the desktop's own depth: the modes listed are the desktop's depth only (the depth
     * changes at a restart), so a program that picked the other depth could not switch to it and
     * gives up ("unable to switch to full screen"). [d3d] render8=0 leaves 8 bpp out: a program
     * that would start in a window on the 8 bpp desktop starts full screen instead. */
    {
        HDC dc = GetDC( NULL );
        int bpp = dc != NULL ? GetDeviceCaps( dc, BITSPIXEL ) : 16;

        if( dc != NULL ) {
            ReleaseDC( NULL, dc );
        }
        c->dwDeviceRenderBitDepth = bpp == 8 ? (GetPrivateProfileInt( "d3d", "render8", 1, "C:\\M8D3D.INI" ) ? DDBD_8 : 0) : DDBD_16;
    }
    /* No Z depth: a Z-buffer beside two 640x480 pages does not fit in the card's 1 MB, and DX5's
     * sample framework then fails rather than draw without one. Depth comes from the sort at
     * Flip. [z] depth=16 claims 16-bit Z, for programs that will not start without it. */
    c->dwDeviceZBufferBitDepth = GetPrivateProfileInt( "z", "depth", 0, "C:\\M8D3D.INI" ) == 16 ? DDBD_16 : 0;
    m8d3d_global.dwNumTextureFormats = 2;
    m8d3d_global.lpTextureFormats = m8d3d_tex_formats;
    memset( m8d3d_tex_formats, 0, sizeof( m8d3d_tex_formats ) );
    for( k = 0; k < 2; ++k ) {
        DDSURFACEDESC *f = &m8d3d_tex_formats[k];

        f->dwSize = sizeof( *f );
        f->dwFlags = DDSD_CAPS | DDSD_PIXELFORMAT;
        f->ddsCaps.dwCaps = DDSCAPS_TEXTURE;
        f->ddpfPixelFormat.dwSize = sizeof( f->ddpfPixelFormat );
        f->ddpfPixelFormat.dwFlags = DDPF_RGB;
        f->ddpfPixelFormat.dwRGBBitCount = 16;
        f->ddpfPixelFormat.dwRBitMask = k == 0 ? 0xF800 : 0x7C00;
        f->ddpfPixelFormat.dwGBitMask = k == 0 ? 0x07E0 : 0x03E0;
        f->ddpfPixelFormat.dwBBitMask = 0x001F;
    }

    memset( &m8d3d_callbacks, 0, sizeof( m8d3d_callbacks ) );
    m8d3d_callbacks.dwSize = sizeof( m8d3d_callbacks );
    m8d3d_callbacks.ContextCreate = (LPVOID)m8d3d_context_create;
    m8d3d_callbacks.ContextDestroy = (LPVOID)m8d3d_context_destroy;
    m8d3d_callbacks.ContextDestroyAll = (LPVOID)m8d3d_context_destroy_all;
    m8d3d_callbacks.SceneCapture = (LPVOID)m8d3d_scene_capture;
    m8d3d_callbacks.RenderState = (LPVOID)m8d3d_render_state;
    m8d3d_callbacks.RenderPrimitive = (LPVOID)m8d3d_render_primitive;
    /* In the DDK's order: any of the four means all of them. */
    m8d3d_callbacks.other[0] = (LPVOID)m8d3d_texture_create;
    m8d3d_callbacks.other[1] = (LPVOID)m8d3d_texture_destroy;
    m8d3d_callbacks.other[2] = (LPVOID)m8d3d_texture_swap;
    m8d3d_callbacks.other[3] = (LPVOID)m8d3d_texture_getsurf;

    memset( &m8d3d_callbacks2, 0, sizeof( m8d3d_callbacks2 ) );
    m8d3d_callbacks2.dwSize = sizeof( m8d3d_callbacks2 );
    m8d3d_callbacks2.dwFlags = M8D3D_CB2_SETRENDERTARGET | M8D3D_CB2_DRAWONEPRIMITIVE |
                               M8D3D_CB2_DRAWONEINDEXED | M8D3D_CB2_DRAWPRIMITIVES;
    m8d3d_callbacks2.SetRenderTarget = (LPVOID)m8d3d_set_target;
    m8d3d_callbacks2.DrawOnePrimitive = (LPVOID)m8d3d_draw_one;
    m8d3d_callbacks2.DrawOneIndexedPrimitive = (LPVOID)m8d3d_draw_one_indexed;
    m8d3d_callbacks2.DrawPrimitives = (LPVOID)m8d3d_draw_primitives;
    *driver_info = (DWORD)m8d3d_driver_info;

    *global = (DWORD)&m8d3d_global;
    *callbacks = (DWORD)&m8d3d_callbacks;
}
