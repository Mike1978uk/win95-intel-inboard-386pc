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
#define M8D3D_TEXTURES  64
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
 * [22] triangles stamped; [23] stamps built; [24] ticks in RenderPrimitive. */
DWORD                   m8d3d_stat[46];      /* 25: engine writes sent, 26: triangle pieces drawn, 27-33: m8dd_tri_census, 34-35: FIFO waits, 36-43: Flip phases, 44: sending */
/* Diagnostic: clock ticks since the last mark, into phase k. */
static LARGE_INTEGER    m8d3d_ph;
#define M8D3D_PH( k ) do { LARGE_INTEGER t_; QueryPerformanceCounter( &t_ ); m8d3d_stat[k] += t_.LowPart - m8d3d_ph.LowPart; m8d3d_ph = t_; } while( 0 )
/* Flip off its fast path: GDI touched the card, slow flips, then why. */
DWORD                   m8d3d_flip_why[6];

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

/* Any port access empties the CPU's cache here, so C run between OUTs runs from RAM: a triangle's
 * lookups cost ~11 us after one another and ~150 us with an OUT between. A frame's engine writes
 * are worked out with the cache intact and kept here in order. The engine is topped up from the
 * front without waiting (m8d3d_q_pump), so it draws one pass while the next is worked out; a wait
 * comes only when the queue is full. */
#define M8D3D_Q_MAX     4096
/* Held writes before the FIFO is looked at again: each look is a status read and empties the cache. */
#define M8D3D_Q_PUMP    24
/* Room for this many, not the FIFO's whole 16: waiting for it to empty leaves the engine idle when
 * a long fill ends, with the next command still crossing the bus. */
#define M8D3D_Q_CHUNK   8
#define M8D3D_Q_ONE     32              /* the most one triangle adds */
/* A box this large keeps the engine busy longer than the next triangle's C takes, even from RAM:
 * sent at once, that C runs while the engine draws, where held it would leave the engine idle. */
#define M8D3D_Q_SEND_PX 1500L

static DWORD    m8d3d_q[M8D3D_Q_MAX];
static int      m8d3d_qn;               /* held */
static int      m8d3d_qh;               /* sent, from the front */
static int      m8d3d_q_next;           /* m8d3d_qn at which to look again */

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

/* As many held writes as the FIFO takes now, with one status read and no wait. */
static void m8d3d_q_pump( void )
{
    int n;

    if( m8d3d_qn < m8d3d_q_next ) {
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
    if( m8d3d_qn + n + M8D3D_Q_ONE * 2 > M8D3D_Q_MAX ) {
        return( m8d3d_q_flush() );
    }
    m8d3d_q_pump();
    return( 1 );
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
#define M8D3D_JOIN_FILL_PX  3200L       /* one fill's 9 writes and start, in polygon-fill pixels */

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

static int m8d3d_edge_cmp( const M8D3D_JEDGE *a, const M8D3D_JEDGE *b )
{
    if( a->col != b->col ) {
        return( a->col - b->col );
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
static int m8d3d_join_ok( M8D3D_CONTEXT *c, const short *v, int n )
{
    long    area = 0;
    int     i;

    if( !c->stamp_on || n < M8D3D_JOIN_MIN || n * 3 > M8D3D_JOIN_EDGES || c->sw != 640 ) {
        return( 0 );
    }
    /* The viewport's right and bottom edges are 640 and 480: a corner there is on screen. */
    for( i = 0; i < n * 6; i += 2 ) {
        if( v[i] < 0 || v[i] > 640 || v[i + 1] < 0 || v[i + 1] > 480 ) {
            return( 0 );
        }
    }
    for( i = 0; i < n; ++i ) {
        const short *p = v + 6 * i;
        long        a = (long)(p[2] - p[0]) * (p[5] - p[1]) - (long)(p[4] - p[0]) * (p[3] - p[1]);

        area += a < 0 ? -a : a;
        if( area > 2L * 640 * 480 + 2L * 640 * 480 / 32 ) {
            break;
        }
    }
    return( i >= M8D3D_JOIN_MIN ? i : 0 );
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
static int m8d3d_join_clear( int planes, int t, int b )
{
    if( !m8d3d_q_need( 6 ) ) {
        return( 0 );
    }
    M8D3D_Q( M8_WRT_MASK, planes );
    M8D3D_Q( M8_RD_MASK, 0x00FF );
    M8D3D_Q( M8_DP_CONFIG, M8_DP_8514 );
    M8D3D_Q( M8_FRGD_MIX, M8_MIX_FRGD_REPLACE );
    M8D3D_Q( M8_FRGD_COLOR, 0 );
    if( !m8d3d_q_rect( M8D3D_JOIN_X, t, M8D3D_JOIN_X + M8D3D_JOIN_W, b ) ||
        !m8d3d_q_rect( M8D3D_JOIN_X, M8D3D_JOIN_RY + t, M8D3D_JOIN_X + M8D3D_JOIN_W, M8D3D_JOIN_RY + b ) ||
        !m8d3d_q_need( 1 ) ) {
        return( 0 );
    }
    M8D3D_Q( M8_WRT_MASK, 0x00FF );
    return( m8d3d_q_flush() );
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
    if( *nr == M8D3D_JOIN_RUNS ) {
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

    for( y = t; y < b; y += M8D3D_JOIN_BAND ) {
        int     y2 = y + M8D3D_JOIN_BAND < b ? y + M8D3D_JOIN_BAND : b;
        int     bn = 0;
        int     gap = (int)(M8D3D_JOIN_FILL_PX / (y2 - y));
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
                    extra = M8D3D_JOIN_FILL_PX * gn;
                }
            }
            for( i = 0; i < gn; ++i ) {
                int ul = grun[2 * i] < brun[2 * i] ? grun[2 * i] : brun[2 * i];
                int ur = grun[2 * i + 1] > brun[2 * i + 1] ? grun[2 * i + 1] : brun[2 * i + 1];

                extra += (long)(ur - ul) * (y2 - gt) - (long)(grun[2 * i + 1] - grun[2 * i]) * (y - gt) -
                         (long)(brun[2 * i + 1] - brun[2 * i]) * (y2 - y);
            }
            if( extra < M8D3D_JOIN_FILL_PX * gn ) {
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

            e->col = (short)(index[i] & 0xFF);
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
            unsigned            h = ((unsigned)e->col * 31u + (unsigned)e->y0 * 977u + (unsigned)e->x0 * 131u +
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
            ++m8d3d_jfrom[m8d3d_je[i].col + 1];
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
            m8d3d_jused[j] = m8d3d_jfrom[m8d3d_je[i].col]++;
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
    if( !c->jclean ) {
        /* The column held stamps and masks: forget them, and start every plane at 0. */
        memset( c->stamp, 0, sizeof( c->stamp ) );
        memset( c->stamp_seen, 0, sizeof( c->stamp_seen ) );
        c->stamp_seen_next = 0;
        c->stamp_x = c->mx;
        c->stamp_y = 0;
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

        for( k = first; k < nb && (nc < 8 || m8d3d_je[k].col == cols[nc - 1]); ++k ) {
            if( nc == 0 || m8d3d_je[k].col != cols[nc - 1] ) {
                cols[nc] = m8d3d_je[k].col;
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
                int         xa = e->x0 < e->x1 ? e->x0 : e->x1;
                int         xb = e->x0 < e->x1 ? e->x1 : e->x0;
                int         ya = e->x0 < e->x1 ? e->y0 : e->y1;
                int         yb = e->x0 < e->x1 ? e->y1 : e->y0;

                t = e->y0 < t ? e->y0 : t;
                b = e->y1 > b ? e->y1 : b;
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
                                        m8d3d_jfrom[cols[j] + 1] - m8d3d_jfrom[cols[j]], ct + 1, cb );
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
        for( j = 0; ok && j < nc; ++j ) {
            if( !m8d3d_q_need( 1 ) ) {
                return( 0 );
            }
            M8D3D_Q( M8_FRGD_COLOR, cols[j] );
            /* Edges chained end to start: after a move, each further x, y pair draws on from the
             * last end, 2 writes an edge where a lone line takes 5. Plain lines, so the order
             * edges are drawn in does not change the pixels. */
            for( i = from[j]; i < from[j + 1]; ++i ) {
                m8d3d_jodd[i] = 0;
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
                    int g;
                    int nx = 0;
                    int ny = 0;

                    for( g = i + 1; g < from[j + 1]; ++g ) {
                        const M8D3D_JEDGE *f = &m8d3d_je[g];

                        if( m8d3d_jodd[g] ) {
                            continue;
                        }
                        if( f->x0 == px && f->y0 == py ) {
                            nx = f->x1;
                            ny = f->y1;
                            break;
                        }
                        if( f->x1 == px && f->y1 == py ) {
                            nx = f->x0;
                            ny = f->y0;
                            break;
                        }
                    }
                    if( g == from[j + 1] ) {
                        break;
                    }
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
        }
        ok = ok && m8d3d_join_clear( (1 << nc) - 1, t, b + 1 );
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
    if( c == NULL || !m8d3d_engine_begin() ) {
        ++m8d3d_stat[11];
        return( 0 );
    }
    m8d3d_mono_on = 0;
    m8d3d_stamp_mode = 0;
    m8d3d_qn = 0;
    m8d3d_qh = 0;
    m8d3d_q_next = M8D3D_Q_PUMP;
    QueryPerformanceCounter( &m8d3d_ph );
    if( m8d3d_pal_want && !m8d3d_pal_on ) {
        m8d3d_pal_load();
        if( c->sw == 640 ) {
            m8d3d_lores_set();
            m8d3d_lores = 1;
        }
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
    first = ok && n != 0 ? m8d3d_join_ok( c, v, n ) : 0;
    M8D3D_PH( 36 );
    if( first != 0 ) {
        ok = m8d3d_mono_off() && m8d3d_q_flush() && m8d3d_join( c, dy, v, index, first );
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
    if( !m8d3d_q_flush() ) {
        ok = 0;
    }
    M8D3D_PH( 42 );
    m8d3d_engine_end();
    M8D3D_PH( 43 );
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

/* At Flip, before the first frame in the palette's indices: the 8514 DAC, 6 bits a gun. */
static void m8d3d_pal_load( void )
{
    int i;

    m8d3d_outb( 0x02EB, 0 );
    for( i = 0; i < 768; ++i ) {
        m8d3d_pal_saved[i] = (BYTE)m8d3d_inb( 0x02ED );
    }
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

/* The colour of a texture rectangle, kept: a program draws the same patch of its texture
 * frame after frame, and a sample is nine texel reads and eighteen float conversions. The
 * key is the texture's slot and the rectangle in 64ths of the texture; emptied when a slot
 * is reused. */
#define M8D3D_PATCH_HASH    1024
static DWORD    m8d3d_patch_key[M8D3D_PATCH_HASH];
static BYTE     m8d3d_patch_col[M8D3D_PATCH_HASH];

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
    if( m8d3d_patch_key[h] != key ) {
        m8d3d_patch_key[h] = key;
        m8d3d_patch_col[h] = (BYTE)m8d3d_pal_quad( t, bits, a );
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

    if( !p->ok ) {
        return;
    }
    a[0] = v0;
    a[1] = v1;
    a[2] = v2;
    for( j = 0; j < 3; ++j ) {
        v[2 * j] = (short)m8d3d_fround( &a[j]->sx, -1 );
        v[2 * j + 1] = (short)m8d3d_fround( &a[j]->sy, -1 );
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
            level = (int)(((rgb >> 16 & 255) + (rgb >> 8 & 255) + (rgb & 255)) * 15 + 382) / 765;
        }
        if( distant ) {
            level &= ~3;
        }
        rgb = m8d3d_pal_rgb( k, level );
        corners[0] = corners[1] = corners[2] = rgb;
        m8d3d_pal_want = 1;
        p->ok = m8d3d_record( p->lcl, v, (BYTE)(k * 16 + level), m8d3d_565( rgb ), corners,
                              ((c->rs_on >> M8D3D_RS_ZENABLE) & 1) && c->zfunc != M8D3D_CMP_ALWAYS ?
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
        /* Described, not drawn: Flip has the engine draw it, furthest first if Z is on. */
        p->ok = m8d3d_record( p->lcl, v, m8d3d_index16( rgb ), m8d3d_565( rgb ), corners,
                              ((c->rs_on >> M8D3D_RS_ZENABLE) & 1) && c->zfunc != M8D3D_CMP_ALWAYS ?
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

    if( !p->ok ) {
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
    c->dwDeviceRenderBitDepth = DDBD_8 | DDBD_16;
    c->dwDeviceZBufferBitDepth = DDBD_16;
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
