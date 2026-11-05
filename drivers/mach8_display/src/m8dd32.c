/* M8DD32.DLL: the 32-bit DirectDraw HAL (docs/mach8_directdraw_plan.md).
 *
 * Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
 * The DriverInit hand-over follows JHRobotics' vmhal9x (MIT, see
 * LICENSE-MIT.txt).
 *
 * DDRAW loads this into the shared arena and calls DriverInit with the
 * linear address of dd16.c's block; the callbacks written there go into the
 * tables DDRAW reads.
 *
 * The 8514 half has no frame buffer the CPU can see, so a surface on the
 * card - the primary, or one in the off-screen heap below it - has its
 * address in a shadow in system RAM: Lock reads the locked rectangle from the
 * card into it, Unlock writes it back, both through PIX_TRANS as
 * m8_hw_read_rect and m8_hw_write_rect do. DDRAW holds the Win16 lock from a
 * Lock to its Unlock unless the application asks for DDLOCK_NOSYSLOCK, so
 * GDI's entries do not run in between; the engine set-up GDI's shadows
 * describe is forgotten afterwards, so its next call writes it again.
 */

#include <windows.h>
#include <conio.h>
#include <stddef.h>
#include <ddraw.h>
#include <string.h>

#define M8DD_MARKER     0x44443855UL

/* m8d3d.c */
extern void m8d3d_init( DWORD *global, DWORD *callbacks );
extern void m8d3d_replay( const RECT *erase, int erase_n, BYTE black, const short *v,
                          const WORD *index, int n );
extern DWORD m8d3d_stat[20];

/* The head of dd16.c's M8DD_SHARE. */
#pragma pack( push, 1 )
typedef struct {
    DWORD   dwSize;
    DWORD   marker;
    DWORD   hInstance;
    DWORD   hot;
    DWORD   width;
    DWORD   height;
    DWORD   pitch;
    DWORD   destroy_driver;
    DWORD   create_surface;
    DWORD   can_create_surface;
    DWORD   wait_vblank;
    DWORD   destroy_surface;
    DWORD   flip;
    DWORD   lock;
    DWORD   unlock;
    DWORD   blt;
    DWORD   shadow;
    DWORD   heap;
    DWORD   heap_top;
    DWORD   heap_rows;
    DWORD   fb16;
    DWORD   fb_lin;
    DWORD   gdi_lin;
    DWORD   d3d_global;
    DWORD   d3d_callbacks;
    DWORD   scratch_top;
    DWORD   scratch_rows;
} M8DD_SHARE;

/* DDHAL_LOCKDATA and DDHAL_UNLOCKDATA as DDRAW passes them (Microsoft's
 * DD_LOCKDATA has the same order). Only the fields up to dwFlags are read. */
typedef struct {
    LPVOID  lpDD;
    LPVOID  lpDDSurface;
    DWORD   bHasRect;
    RECTL   rArea;
    LPVOID  lpSurfData;
    HRESULT ddRVal;
    LPVOID  Lock;
    DWORD   dwFlags;
} M8DD_LOCK;

typedef struct {
    LPVOID  lpDD;
    LPVOID  lpDDSurface;
    HRESULT ddRVal;
    LPVOID  Unlock;
} M8DD_UNLOCK;
#pragma pack( pop )

/* DDHAL_BLTDATA (Microsoft's DD_BLTDATA has the same order); DDBLTFX is
 * ddraw.h's own. With IsClipped, the destination is the dwRectCnt rectangles
 * at prDestRects, and rOrigDest and rOrigSrc are the blit before clipping. */
typedef struct {
    LPVOID  lpDD;
    LPVOID  lpDDDestSurface;
    RECTL   rDest;
    LPVOID  lpDDSrcSurface;
    RECTL   rSrc;
    DWORD   dwFlags;
    DWORD   dwROPFlags;
    DDBLTFX bltFX;
    HRESULT ddRVal;
    LPVOID  Blt;
    BOOL    IsClipped;
    RECTL   rOrigDest;
    RECTL   rOrigSrc;
    DWORD   dwRectCnt;
    LPRECT  prDestRects;
} M8DD_BLT;

/* Pixel data by string I/O: the C loop costs more per word than the port
 * does. ISA wait states pace the card's FIFO. Flat model, so ES = DS. */
extern void m8dd_outsw( const WORD *src, unsigned words );
#pragma aux m8dd_outsw =     "mov dx, 0E2E8h" "rep outsw"     parm [esi] [ecx] modify [esi ecx edx];

extern void m8dd_insw( WORD *dst, unsigned words );
#pragma aux m8dd_insw =     "mov dx, 0E2E8h" "rep insw"     parm [edi] [ecx] modify [edi ecx edx];

typedef char m8dd_fx_size[sizeof( DDBLTFX ) == 100 ? 1 : -1];
typedef char m8dd_fill_at[offsetof( DDBLTFX, dwFillColor ) == 80 ? 1 : -1];

/* DDHAL_CREATESURFACEDATA, DDHAL_DESTROYSURFACEDATA, DDHAL_DESTROYDRIVERDATA. */
typedef struct {
    LPVOID  lpDD;
    LPVOID  lpDDSurfaceDesc;
    LPVOID  *lplpSList;
    DWORD   dwSCnt;
    HRESULT ddRVal;
    LPVOID  CreateSurface;
} M8DD_CREATE;

typedef struct {
    LPVOID  lpDD;
    LPVOID  lpDDSurface;
    HRESULT ddRVal;
    LPVOID  DestroySurface;
} M8DD_DESTROY;

typedef struct {
    LPVOID  lpDD;
    HRESULT ddRVal;
    LPVOID  DestroyDriver;
} M8DD_DESTROYDRV;

/* A surface's global part is the second pointer of its local part (ddrawi.h,
 * DirectX 7 DDK). A surface is on the card when its fpVidMem is in the shadow
 * (the primary, then the heap) with the screen's pitch; any other, or a
 * layout not as read here, goes back to DDRAW. */
#define M8DD_LCL_GBL        4
#define M8DD_LCL_FLAGS      28
#define M8DD_LCL_CAPS       32
#define M8DD_LCL_CKSRCBLT   60          /* DDCOLORKEY ddckCKSrcBlt */
#define M8DD_GBL_VIDMEM     20
#define M8DD_GBL_PITCH      24
#define M8DD_GBL_HEIGHT     28          /* WORD */
#define M8DD_GBL_WIDTH      30          /* WORD */
#define M8DD_GBL_PF_FLAGS   44          /* ddpfSurface.dwFlags */
#define M8DD_GBL_PF_BITS    52          /* ddpfSurface.dwRGBBitCount */
#define M8DD_HASPIXELFORMAT 0x00002000UL /* else the surface is in the primary's format */

#define DDHAL_DRIVER_NOTHANDLED 0
#define DDHAL_DRIVER_HANDLED    1
#define M8DD_DDERR_GENERIC      0x80004005L

/* M8_HOT (m8.h), at the offsets blt.asm reads it by. */
#define H_ROOM          4
#define H_ROOM_OK       8
#define H_DP_OK         9
#define H_HW_OFF        11
#define H_BUSY          12
#define H_RD_OK         15
#define H_MM_W          24
#define H_MM_H          26
#define H_SH_OK         28
#define H_SH_COUNT      12

/* m8.h */
#define M8_CUR_Y            0x82E8
#define M8_CUR_X            0x86E8
#define M8_MAJ_AXIS_PCNT    0x96E8
#define M8_GP_STAT          0x9AE8
#define M8_CMD              0x9AE8
#define M8_WRT_MASK         0xAAE8
#define M8_RD_MASK          0xAEE8
#define M8_FRGD_MIX         0xBAE8
#define M8_MULTIFUNC_CNTL   0xBEE8
#define M8_PIX_TRANS        0xE2E8
#define M8_EXT_FIFO_STATUS  0x9AEE
#define M8_DP_CONFIG        0xCEEE
#define M8_EXT_SCISSOR_L    0xDAEE
#define M8_EXT_SCISSOR_T    0xDEEE
#define M8_EXT_SCISSOR_R    0xE2EE
#define M8_EXT_SCISSOR_B    0xE6EE
#define M8_EXT_SCISSOR_MIN  0x0800
#define M8_EXT_SCISSOR_MAX  0x07FF
#define M8_DP_8514          0x2211
#define M8_GP_DATA_READY    0x0100
#define M8_GP_BUSY          0x0200
#define M8_MF_MIN_AXIS      0x0000
#define M8_MF_PIX_CNTL      0xA000
#define M8_MIX_HOST_REPLACE 0x0047
#define M8_CMD_HOST_WRITE   0x53B1
#define M8_CMD_HOST_READ    0x53B0
#define M8_FRGD_COLOR       0xA6E8
#define M8_DESTX_DIASTP     0x8EE8
#define M8_DESTY_AXSTP      0x8AE8
#define M8_MIX_FRGD_REPLACE 0x0027
#define M8_MIX_VRAM_REPLACE 0x0067
#define M8_CMD_FILL         0x40F3
#define M8_CMD_BLIT         0xC0F3      /* x and y increasing */
#define M8_CMD_INC_X        0x0020
#define M8_CMD_INC_Y        0x0080
#define M8DD_SRCCOPY        0x00CC0020UL
#define M8_WAIT_LIMIT       60000u
#define M8_XROW_WORDS       513         /* the widest mode, one word over for an odd start */
#define M8_ESC_DDLEND       0x4D39      /* m8.h */
#define M8DD_HELD_MAX       8
/* m8.h: the extended blit, for a colour-keyed copy. */
#define M8_DP_VRAM_BLIT     0x6011
#define M8_ALU_FG_FN        0xBAEE
#define M8_ALU_BG_FN        0xB6EE
#define M8_SRC_CMP_FN       0x92EE
#define M8_SRC_CMP_COLOR    0xEAEE
#define M8_SRC_Y_DIR        0xC2EE
#define M8_DEST_X_START     0xA6EE
#define M8_DEST_X_END       0xAAEE
#define M8_DEST_Y_END       0xAEEE
#define M8_SRC_X_START      0xB2EE
#define M8_SRC_X_END        0xBEEE
#define M8_MIX_REPLACE      0x0007
#define M8_MIX_LEAVE        0x0003

static HINSTANCE    m8dd_inst;
static M8DD_SHARE   *m8dd;

/* A surface on the card: its card position and size. */
typedef struct {
    int     x;
    int     y;
    int     w;
    int     h;
} M8DD_SURF;

/* The rectangles Unlock writes back, card coordinates with even x and width. */
typedef struct {
    LPVOID  lcl;
    RECT    r;
} M8DD_HELD;

static M8DD_HELD    m8dd_held[M8DD_HELD_MAX];

static int m8dd_idle( void )
{
    unsigned n;

    for( n = 0; n < M8_WAIT_LIMIT; ++n ) {
        if( inpw( M8_EXT_FIFO_STATUS ) == 0 && !(inpw( M8_GP_STAT ) & M8_GP_BUSY) ) {
            return( 1 );
        }
    }
    return( 0 );
}

/* GDI's engine shadows say nothing true after this HAL has used the engine. */
static void m8dd_forget( BYTE *hot )
{
    int i;

    for( i = 0; i < H_SH_COUNT; ++i ) {
        hot[H_SH_OK + i] = 0;
    }
    *(short *)(hot + H_ROOM) = 0;
    hot[H_ROOM_OK] = 0;
    hot[H_DP_OK] = 0;
    hot[H_RD_OK] = 0;
    *(WORD *)(hot + H_MM_W) = 0;
    *(WORD *)(hot + H_MM_H) = 0;
}

/* The card rectangle `r` between the card and the shadow, from an idle
 * engine; the engine is idle again after a write, as a colour write while it
 * still takes host data would be taken as pixels (guide 8-14). */
static int m8dd_transfer( const RECT *r, int to_card )
{
    BYTE    *hot = (BYTE *)m8dd->hot;
    WORD    *row;
    int     w = r->right - r->left;
    int     h = r->bottom - r->top;
    int     y;
    int     ok = 0;
    unsigned n;

    if( hot[H_HW_OFF] || w <= 0 || h <= 0 ) {
        return( hot[H_HW_OFF] == 0 );
    }
    /* The interrupt-time cursor defers its move while this is set. */
    hot[H_BUSY] = 1;
    if( m8dd_idle() ) {
        outpw( M8_WRT_MASK, 0x00FF );
        outpw( M8_RD_MASK, 0x00FF );
        outpw( M8_DP_CONFIG, M8_DP_8514 );
        outpw( M8_MULTIFUNC_CNTL, M8_MF_PIX_CNTL );
        outpw( M8_EXT_SCISSOR_L, M8_EXT_SCISSOR_MIN );
        outpw( M8_EXT_SCISSOR_T, M8_EXT_SCISSOR_MIN );
        outpw( M8_EXT_SCISSOR_R, M8_EXT_SCISSOR_MAX );
        outpw( M8_EXT_SCISSOR_B, M8_EXT_SCISSOR_MAX );
        outpw( M8_FRGD_MIX, M8_MIX_HOST_REPLACE );
        outpw( M8_CUR_X, r->left );
        outpw( M8_CUR_Y, r->top );
        outpw( M8_MAJ_AXIS_PCNT, w - 1 );
        outpw( M8_MULTIFUNC_CNTL, M8_MF_MIN_AXIS | (h - 1) );
        outpw( M8_CMD, to_card ? M8_CMD_HOST_WRITE : M8_CMD_HOST_READ );
        if( to_card ) {
            for( y = r->top; y < r->bottom; ++y ) {
                row = (WORD *)(m8dd->shadow + (DWORD)y * m8dd->pitch + r->left);
                m8dd_outsw( row, w / 2 );
            }
            ok = m8dd_idle();
        } else {
            /* A read of PIX_TRANS before DATA_READY locks the FIFO (guide 8-14). */
            for( n = 0; n < M8_WAIT_LIMIT; ++n ) {
                if( inpw( M8_GP_STAT ) & M8_GP_DATA_READY ) {
                    break;
                }
            }
            if( n < M8_WAIT_LIMIT ) {
                for( y = r->top; y < r->bottom; ++y ) {
                    row = (WORD *)(m8dd->shadow + (DWORD)y * m8dd->pitch + r->left);
                    m8dd_insw( row, w / 2 );
                }
                ok = 1;
            }
        }
    }
    m8dd_forget( hot );
    hot[H_BUSY] = 0;
    return( ok );
}

static int m8dd_surf( LPVOID lcl, M8DD_SURF *s )
{
    BYTE    *gbl;
    DWORD   f;
    DWORD   off;
    DWORD   rows;

    if( lcl == NULL ) {
        return( 0 );
    }
    gbl = *(BYTE **)((BYTE *)lcl + M8DD_LCL_GBL);
    if( gbl == NULL || *(long *)(gbl + M8DD_GBL_PITCH) != (long)m8dd->pitch ) {
        return( 0 );
    }
    f = *(DWORD *)(gbl + M8DD_GBL_VIDMEM);
    rows = m8dd->heap_rows ? m8dd->heap_top + m8dd->heap_rows : m8dd->height;
    if( f < m8dd->shadow || f - m8dd->shadow >= m8dd->pitch * rows ) {
        return( 0 );
    }
    off = f - m8dd->shadow;
    s->x = off % m8dd->pitch;
    s->y = off / m8dd->pitch;
    s->w = *(WORD *)(gbl + M8DD_GBL_WIDTH);
    s->h = *(WORD *)(gbl + M8DD_GBL_HEIGHT);
    if( s->x + s->w > (int)m8dd->pitch || s->y + s->h > (int)rows ) {
        return( 0 );
    }
    return( 1 );
}

/* `r` (surface coordinates) clipped to the surface and moved to the card. */
static void m8dd_to_card( RECT *c, const RECTL *r, const M8DD_SURF *s )
{
    c->left = (r->left < 0 ? 0 : r->left) + s->x;
    c->top = (r->top < 0 ? 0 : r->top) + s->y;
    c->right = (r->right > s->w ? s->w : r->right) + s->x;
    c->bottom = (r->bottom > s->h ? s->h : r->bottom) + s->y;
}

static DWORD m8dd_lock16( M8DD_LOCK *ld );
static DWORD m8dd_unlock16( M8DD_UNLOCK *ud );

/* The locked rectangle, clipped to the surface; host transfers move two
 * pixels a word, so its left edge goes down and its right edge up to even. */
DWORD __stdcall m8dd_lock( M8DD_LOCK *ld )
{
    M8DD_SURF   s;
    RECTL       all;
    RECT        r;
    int         k;

    if( m8dd->fb16 ) {
        return( m8dd_lock16( ld ) );
    }
    if( !m8dd_surf( ld->lpDDSurface, &s ) ) {
        return( DDHAL_DRIVER_NOTHANDLED );
    }
    for( k = 0; k < M8DD_HELD_MAX && m8dd_held[k].lcl != NULL; ++k ) {
    }
    if( k == M8DD_HELD_MAX ) {
        ld->ddRVal = M8DD_DDERR_GENERIC;
        return( DDHAL_DRIVER_HANDLED );
    }
    all.left = 0;
    all.top = 0;
    all.right = s.w;
    all.bottom = s.h;
    m8dd_to_card( &r, ld->bHasRect ? &ld->rArea : &all, &s );
    ld->lpSurfData = (LPVOID)(m8dd->shadow + (DWORD)r.top * m8dd->pitch + r.left);
    r.left &= ~1;
    r.right = (r.right + 1) & ~1;
    if( !m8dd_transfer( &r, 0 ) ) {
        ld->ddRVal = M8DD_DDERR_GENERIC;
        return( DDHAL_DRIVER_HANDLED );
    }
    m8dd_held[k].lcl = ld->lpDDSurface;
    m8dd_held[k].r = r;
    ld->ddRVal = DD_OK;
    return( DDHAL_DRIVER_HANDLED );
}

DWORD __stdcall m8dd_unlock( M8DD_UNLOCK *ud )
{
    M8DD_SURF   s;
    int         k;

    if( m8dd->fb16 ) {
        return( m8dd_unlock16( ud ) );
    }
    if( !m8dd_surf( ud->lpDDSurface, &s ) ) {
        return( DDHAL_DRIVER_NOTHANDLED );
    }
    for( k = M8DD_HELD_MAX - 1; k >= 0 && m8dd_held[k].lcl != ud->lpDDSurface; --k ) {
    }
    if( k < 0 ) {
        ud->ddRVal = M8DD_DDERR_GENERIC;
        return( DDHAL_DRIVER_HANDLED );
    }
    ud->ddRVal = m8dd_transfer( &m8dd_held[k].r, 1 ) ? DD_OK : M8DD_DDERR_GENERIC;
    m8dd_held[k].lcl = NULL;
    return( DDHAL_DRIVER_HANDLED );
}

/* The plain 8514 data path with no clip and this mix, from an idle engine. */
static int m8dd_plain( WORD mix )
{
    if( !m8dd_idle() ) {
        return( 0 );
    }
    outpw( M8_WRT_MASK, 0x00FF );
    outpw( M8_RD_MASK, 0x00FF );
    outpw( M8_DP_CONFIG, M8_DP_8514 );
    outpw( M8_MULTIFUNC_CNTL, M8_MF_PIX_CNTL );
    outpw( M8_EXT_SCISSOR_L, M8_EXT_SCISSOR_MIN );
    outpw( M8_EXT_SCISSOR_T, M8_EXT_SCISSOR_MIN );
    outpw( M8_EXT_SCISSOR_R, M8_EXT_SCISSOR_MAX );
    outpw( M8_EXT_SCISSOR_B, M8_EXT_SCISSOR_MAX );
    outpw( M8_FRGD_MIX, mix );
    return( 1 );
}

/* One rectangle: a fill, or a copy from (sx, sy) in the direction that reads
 * every source pixel before it is overwritten. It waits for room for its
 * seven writes as m8_fifo_need( 7 ) does: EXT_FIFO_STATUS bit 15 - 7 clear
 * (guide 9-16). */
static int m8dd_rect( const RECT *d, int copy, int sx, int sy )
{
    int     w = d->right - d->left;
    int     h = d->bottom - d->top;
    int     dx = d->left;
    int     dy = d->top;
    WORD    cmd = M8_CMD_BLIT;
    unsigned n;

    if( w <= 0 || h <= 0 ) {
        return( 1 );
    }
    for( n = 0; n < M8_WAIT_LIMIT && (inpw( M8_EXT_FIFO_STATUS ) & (0x8000 >> 7)); ++n ) {
    }
    if( n == M8_WAIT_LIMIT ) {
        return( 0 );
    }
    if( copy ) {
        if( dy > sy ) {
            cmd &= ~M8_CMD_INC_Y;
            sy += h - 1;
            dy += h - 1;
        } else if( dy == sy && dx > sx ) {
            cmd &= ~M8_CMD_INC_X;
            sx += w - 1;
            dx += w - 1;
        }
        outpw( M8_CUR_X, sx );
        outpw( M8_CUR_Y, sy );
        outpw( M8_DESTX_DIASTP, dx );
        outpw( M8_DESTY_AXSTP, dy );
    } else {
        outpw( M8_CUR_X, dx );
        outpw( M8_CUR_Y, dy );
        cmd = M8_CMD_FILL;
    }
    outpw( M8_MAJ_AXIS_PCNT, w - 1 );
    outpw( M8_MULTIFUNC_CNTL, M8_MF_MIN_AXIS | (h - 1) );
    outpw( M8_CMD, cmd );
    return( 1 );
}

/* One rectangle copied from (sx, sy) with the source's pixels of colour
 * `key` left out, as m8hw.c m8_hw_copy_keyed: the extended blit, left to right
 * and top down (the source is another surface, so the two never overlap).
 * Its set-up is extended registers, written from an empty FIFO; it restores
 * the plain data path's DP_CONFIG and turns the compare off behind it. */
static int m8dd_key_rect( const RECT *d, int sx, int sy, BYTE key )
{
    int     w = d->right - d->left;
    int     h = d->bottom - d->top;

    if( w <= 0 || h <= 0 ) {
        return( 1 );
    }
    if( !m8dd_idle() ) {
        return( 0 );
    }
    outpw( M8_DP_CONFIG, M8_DP_VRAM_BLIT );
    outpw( M8_ALU_FG_FN, M8_MIX_REPLACE );
    outpw( M8_ALU_BG_FN, M8_MIX_LEAVE );
    outpw( M8_SRC_CMP_COLOR, key );
    outpw( M8_SRC_CMP_FN, 0x0028 );
    outpw( M8_SRC_Y_DIR, 1 );
    outpw( M8_CUR_X, d->left );
    outpw( M8_CUR_Y, d->top );
    outpw( M8_DEST_X_START, d->left );
    outpw( M8_DEST_X_END, d->right );
    outpw( M8_DESTX_DIASTP, sx );
    outpw( M8_DESTY_AXSTP, sy );
    outpw( M8_SRC_X_START, sx );
    outpw( M8_SRC_X_END, sx + w );
    outpw( M8_DEST_Y_END, d->bottom );
    outpw( M8_SRC_CMP_FN, 0 );
    if( !m8dd_idle() ) {
        return( 0 );
    }
    outpw( M8_DP_CONFIG, M8_DP_8514 );
    return( 1 );
}

/* ---- 16 bpp (m8dd->fb16): the primary is fb16.c's frame buffer in RAM.
 * Locks point into it; what Unlock and Blt change is sent to the card by the
 * driver (M8_ESC_FB16SEND), dithered as GDI's drawing is, so a one-colour
 * rectangle costs the bus a few commands and anything else a byte a pixel. */

#define M8_ESC_FB16SEND     0x4D3C      /* m8.h */
#define M8_ESC_FB16COPY     0x4D3D

#define M8_ESC_FB16BATCH    0x4D40

/* Two buffers at most: 0 the frame buffer, 1 the back buffer of a flipping
 * primary. The card shows m8dd_front. A buffer's description, while `known`:
 * every pixel is `colour` except inside the rectangles d, whose pixels RAM
 * holds; outside them RAM holds the colour only once `filled`. So a clear is
 * one value, not a screen of writes, and a flip sends the rectangles that
 * changed and nothing else. Not `known`: RAM holds the buffer.
 *
 * Direct3D's triangles are part of the description too, over the rectangles:
 * RAM does not hold them until something reads the buffer (m8dd_tris_to_ram),
 * and Flip has the engine draw them. */
#define M8DD_DIRTY_MAX      8
#define M8DD_TRIS_MAX       512

typedef struct {
    int     known;
    int     filled;
    WORD    colour;
    int     n;
    RECT    d[M8DD_DIRTY_MAX + 1];      /* one over: m8dd_dirty */
} M8DD_DESC;

static DWORD        m8dd_back;          /* the back buffer's linear address, 0 = none */
static int          m8dd_front;
static M8DD_DESC    m8dd_desc[2];
static M8DD_DESC    m8dd_card;          /* what the card shows, while its `known` */
static DWORD        m8dd_gdi_seen;      /* fb16.c's GDI call count at the last Flip */
static int          m8dd_tri_n[2];
static short        m8dd_tri_v[2][M8DD_TRIS_MAX * 6];
static WORD         m8dd_tri_i[2][M8DD_TRIS_MAX];       /* the card's palette index */
static WORD         m8dd_tri_c[2][M8DD_TRIS_MAX];       /* 5-6-5, for RAM */
static RECT         m8dd_card_box[M8DD_TRIS_MAX];       /* the triangles' boxes on the card */
static int          m8dd_card_boxes;
static short        m8dd_card_v[M8DD_TRIS_MAX * 6];     /* and the triangles themselves */
static WORD         m8dd_card_i[M8DD_TRIS_MAX];
static int          m8dd_fp_stale;      /* the card changed behind fb16.c's row fingerprints */

/* The buffer a surface's memory is, or -1. */
static int m8dd_buf( LPVOID lcl )
{
    BYTE    *gbl;
    DWORD   m;

    if( lcl == NULL ) {
        return( -1 );
    }
    gbl = *(BYTE **)((BYTE *)lcl + M8DD_LCL_GBL);
    if( gbl == NULL ) {
        return( -1 );
    }
    m = *(DWORD *)(gbl + M8DD_GBL_VIDMEM);
    if( m == m8dd->fb_lin ) {
        return( 0 );
    }
    return( m8dd_back != 0 && m == m8dd_back ? 1 : -1 );
}

static WORD *m8dd_at( int k, int x, int y )
{
    return( (WORD *)((k ? m8dd_back : m8dd->fb_lin) + (DWORD)y * m8dd->pitch + (DWORD)x * 2) );
}

/* fb16.c's batch (m8_fb16_batch): entries queue, one ExtEscape sends them. */
#define M8DD_BATCH_MAX      24

static WORD     m8dd_bl[1 + 6 * M8DD_BATCH_MAX];

static DWORD m8dd_flush( void )
{
    HDC     dc;
    DWORD   out = 0;

    if( m8dd_bl[0] == 0 ) {
        return( 0 );
    }
    dc = GetDC( NULL );
    if( dc != NULL ) {
        ExtEscape( dc, M8_ESC_FB16BATCH, 2 + 12 * m8dd_bl[0], (LPCSTR)m8dd_bl,
                   sizeof( out ), (LPSTR)&out );
        ReleaseDC( NULL, dc );
    }
    m8dd_bl[0] = 0;
    return( out );
}

/* r NULL: the whole screen. */
static void m8dd_op( WORD op, const RECT *r, WORD arg )
{
    WORD    *e;

    if( m8dd_bl[0] == M8DD_BATCH_MAX ) {
        m8dd_flush();
    }
    e = m8dd_bl + 1 + 6 * m8dd_bl[0]++;
    e[0] = op;
    e[1] = r ? (WORD)r->left : 0;
    e[2] = r ? (WORD)r->top : 0;
    e[3] = r ? (WORD)r->right : (WORD)m8dd->width;
    e[4] = r ? (WORD)r->bottom : (WORD)m8dd->height;
    e[5] = arg;
}

static void m8dd_fill16( WORD *p, int n, WORD c )
{
    while( n-- > 0 ) {
        *p++ = c;
    }
}

/* Buffer k's pixels within c put in RAM where its description has them as
 * the colour: before anything reads them or draws over part of them. */
static void m8dd_ready( int k, const RECT *c )
{
    M8DD_DESC   *d = &m8dd_desc[k];
    const RECT  *r;
    int         y;
    int         x;
    int         e;
    int         i;

    if( !d->known || d->filled ) {
        return;
    }
    for( y = c->top; y < c->bottom; ++y ) {
        x = c->left;
        while( x < c->right ) {
            e = c->right;
            for( i = 0; i < d->n; ++i ) {
                r = &d->d[i];
                if( y < r->top || y >= r->bottom || r->right <= x || r->left >= e ) {
                    continue;
                }
                if( r->left <= x ) {
                    x = r->right;
                    e = -1;
                    break;
                }
                e = r->left;
            }
            if( e >= 0 ) {
                m8dd_fill16( m8dd_at( k, x, y ), e - x, d->colour );
                x = e;
            }
        }
    }
}

static void m8dd_dirty( int k, const RECT *c );

/* One triangle in buffer k's RAM, a row at a time. The engine's edge pixels are
 * not matched exactly: a program that reads a frame back sees it within a pixel. */
static void m8dd_tri_ram( int k, const short *v, WORD colour )
{
    int     o[3] = { 0, 1, 2 };
    int     t;
    int     y;
    int     a;
    int     b;
    int     x0;
    int     x1;
    int     y0;
    int     y1;
    int     y2;
    RECT    r;

    if( v[2 * o[1] + 1] < v[2 * o[0] + 1] ) {
        t = o[0]; o[0] = o[1]; o[1] = t;
    }
    if( v[2 * o[2] + 1] < v[2 * o[1] + 1] ) {
        t = o[1]; o[1] = o[2]; o[2] = t;
    }
    if( v[2 * o[1] + 1] < v[2 * o[0] + 1] ) {
        t = o[0]; o[0] = o[1]; o[1] = t;
    }
    y0 = v[2 * o[0] + 1];
    y1 = v[2 * o[1] + 1];
    y2 = v[2 * o[2] + 1];
    if( y2 == y0 ) {
        return;
    }
    r.left = v[0];
    r.right = v[0];
    for( t = 1; t < 3; ++t ) {
        if( v[2 * t] < r.left ) {
            r.left = v[2 * t];
        }
        if( v[2 * t] > r.right ) {
            r.right = v[2 * t];
        }
    }
    ++r.right;
    r.top = y0;
    r.bottom = y2 + 1;
    m8dd_ready( k, &r );
    for( y = y0; y <= y2; ++y ) {
        a = v[2 * o[0]] + (int)((long)(v[2 * o[2]] - v[2 * o[0]]) * (y - y0) / (y2 - y0));
        if( y < y1 ) {
            b = v[2 * o[0]] + (int)((long)(v[2 * o[1]] - v[2 * o[0]]) * (y - y0) / (y1 - y0));
        } else if( y2 != y1 ) {
            b = v[2 * o[1]] + (int)((long)(v[2 * o[2]] - v[2 * o[1]]) * (y - y1) / (y2 - y1));
        } else {
            b = v[2 * o[1]];
        }
        x0 = a < b ? a : b;
        x1 = a < b ? b : a;
        m8dd_fill16( m8dd_at( k, x0, y ), x1 - x0 + 1, colour );
    }
    m8dd_dirty( k, &r );
}

/* Something is about to read buffer k's pixels or draw over part of them. */
static void m8dd_tris_to_ram( int k )
{
    int i;
    int n = m8dd_tri_n[k];

    m8dd_tri_n[k] = 0;
    for( i = 0; i < n; ++i ) {
        m8dd_tri_ram( k, &m8dd_tri_v[k][6 * i], m8dd_tri_c[k][i] );
    }
}

static void m8dd_ready_all( int k )
{
    RECT    all;

    m8dd_tris_to_ram( k );
    all.left = 0;
    all.top = 0;
    all.right = m8dd->width;
    all.bottom = m8dd->height;
    m8dd_ready( k, &all );
    m8dd_desc[k].filled = 1;
}

static int m8dd_inside( const RECT *a, const RECT *b )
{
    return( a->left >= b->left && a->top >= b->top && a->right <= b->right && a->bottom <= b->bottom );
}

/* c of buffer k now drawn in RAM. Past M8DD_DIRTY_MAX rectangles, the one
 * that grows least takes it, its new area put in RAM first. */
static void m8dd_dirty( int k, const RECT *c )
{
    M8DD_DESC   *d = &m8dd_desc[k];
    RECT        u;
    long        g;
    long        bg = 0;
    int         best = 0;
    int         i;

    if( !d->known ) {
        return;
    }
    for( i = 0; i < d->n; ++i ) {
        if( m8dd_inside( c, &d->d[i] ) ) {
            return;
        }
    }
    if( d->n < M8DD_DIRTY_MAX ) {
        d->d[d->n++] = *c;
        return;
    }
    for( i = 0; i < d->n; ++i ) {
        UnionRect( &u, &d->d[i], c );
        g = (long)(u.right - u.left) * (u.bottom - u.top) -
            (long)(d->d[i].right - d->d[i].left) * (d->d[i].bottom - d->d[i].top);
        if( i == 0 || g < bg ) {
            bg = g;
            best = i;
        }
    }
    UnionRect( &u, &d->d[best], c );
    /* c is drawn already: listed for the moment, so its pixels are not put back
     * to the colour. */
    d->d[d->n++] = *c;
    m8dd_ready( k, &u );
    --d->n;
    d->d[best] = u;
}

static void m8dd_send( const RECT *r, int solid )
{
    HDC     dc;
    WORD    w[5];

    if( r->right <= r->left || r->bottom <= r->top ) {
        return;
    }
    w[0] = (WORD)r->left;
    w[1] = (WORD)r->top;
    w[2] = (WORD)r->right;
    w[3] = (WORD)r->bottom;
    w[4] = (WORD)solid;
    dc = GetDC( NULL );
    if( dc != NULL ) {
        ExtEscape( dc, M8_ESC_FB16SEND, sizeof( w ), (LPCSTR)w, 0, NULL );
        ReleaseDC( NULL, dc );
    }
}

/* The screen copied from (sx, sy) to `r` in RAM: the driver has the card
 * copy it too when the dither allows, else sends it. */
static void m8dd_copied( int sx, int sy, const RECT *r )
{
    HDC     dc;
    WORD    w[6];

    w[0] = (WORD)sx;
    w[1] = (WORD)sy;
    w[2] = (WORD)r->left;
    w[3] = (WORD)r->top;
    w[4] = (WORD)r->right;
    w[5] = (WORD)r->bottom;
    dc = GetDC( NULL );
    if( dc != NULL ) {
        ExtEscape( dc, M8_ESC_FB16COPY, sizeof( w ), (LPCSTR)w, 0, NULL );
        ReleaseDC( NULL, dc );
    }
}

/* `r` clipped to the screen. */
static void m8dd_clip_fb( RECT *c, const RECTL *r )
{
    c->left = r->left < 0 ? 0 : r->left;
    c->top = r->top < 0 ? 0 : r->top;
    c->right = r->right > (long)m8dd->width ? (long)m8dd->width : r->right;
    c->bottom = r->bottom > (long)m8dd->height ? (long)m8dd->height : r->bottom;
}

/* The buffer the card shows. */
static WORD *m8dd_fb_at( int x, int y )
{
    return( m8dd_at( m8dd_front, x, y ) );
}

/* A frame copied over the screen is mostly the screen it replaces: each row
 * is compared with the frame buffer and only the part that differs is copied
 * and sent. Rows that change together go as one rectangle; a row with no
 * change ends it. M8_SEND_EXACT tells the driver this is all that changed, so
 * it sends it without fingerprinting. */
#define M8_SEND_EXACT       2

typedef struct {
    RECT    r;
    int     open;
} M8DD_DIFF;

static void m8dd_diff_close( M8DD_DIFF *g )
{
    if( g->open ) {
        m8dd_send( &g->r, M8_SEND_EXACT );
        g->open = 0;
    }
}

/* Row y of the screen from s, columns l .. l + w - 1. */
static void m8dd_diff_row( M8DD_DIFF *g, int y, int l, int w, const WORD *s )
{
    WORD    *d = m8dd_fb_at( l, y );
    int     a = 0;
    int     b = w;

    while( a < b && d[a] == s[a] ) {
        ++a;
    }
    if( a == b ) {
        m8dd_diff_close( g );
        return;
    }
    while( d[b - 1] == s[b - 1] ) {
        --b;
    }
    memcpy( d + a, s + a, (b - a) * 2 );
    a += l;
    b += l;
    if( !g->open ) {
        g->r.left = a;
        g->r.right = b;
        g->r.top = y;
        g->open = 1;
    } else {
        if( a < g->r.left ) {
            g->r.left = a;
        }
        if( b > g->r.right ) {
            g->r.right = b;
        }
    }
    g->r.bottom = y + 1;
}

static DWORD m8dd_lock16( M8DD_LOCK *ld )
{
    RECTL   all;
    RECT    r;
    int     k;
    int     b = m8dd_buf( ld->lpDDSurface );

    if( b < 0 ) {
        return( DDHAL_DRIVER_NOTHANDLED );
    }
    /* The program reads or writes the pixels itself: RAM must hold them all. */
    m8dd_ready_all( b );
    all.left = 0;
    all.top = 0;
    all.right = m8dd->width;
    all.bottom = m8dd->height;
    m8dd_clip_fb( &r, ld->bHasRect ? &ld->rArea : &all );
    ld->lpSurfData = (LPVOID)m8dd_at( b, r.left, r.top );
    ld->ddRVal = DD_OK;
    if( ld->dwFlags & DDLOCK_READONLY ) {
        return( DDHAL_DRIVER_HANDLED );
    }
    m8dd_desc[b].known = 0;
    if( b != m8dd_front ) {
        return( DDHAL_DRIVER_HANDLED );
    }
    m8dd_card.known = 0;
    for( k = 0; k < M8DD_HELD_MAX && m8dd_held[k].lcl != NULL; ++k ) {
    }
    if( k == M8DD_HELD_MAX ) {
        /* Nowhere to remember it: the whole screen goes at the next unlock. */
        k = 0;
        r.left = 0;
        r.top = 0;
        r.right = m8dd->width;
        r.bottom = m8dd->height;
    }
    m8dd_held[k].lcl = ld->lpDDSurface;
    m8dd_held[k].r = r;
    return( DDHAL_DRIVER_HANDLED );
}

static DWORD m8dd_unlock16( M8DD_UNLOCK *ud )
{
    int k;

    if( m8dd_buf( ud->lpDDSurface ) < 0 ) {
        return( DDHAL_DRIVER_NOTHANDLED );
    }
    for( k = 0; k < M8DD_HELD_MAX; ++k ) {
        if( m8dd_held[k].lcl == ud->lpDDSurface ) {
            m8dd_send( &m8dd_held[k].r, 0 );
            m8dd_held[k].lcl = NULL;
            break;
        }
    }
    ud->ddRVal = DD_OK;
    return( DDHAL_DRIVER_HANDLED );
}

/* A system-memory surface in 5-6-5: its first pixel, pitch and size, or NULL. */
static BYTE *m8dd_sysmem16( LPVOID lcl, long *pitch, int *w, int *h )
{
    BYTE    *gbl;

    if( lcl == NULL || !(*(DWORD *)((BYTE *)lcl + M8DD_LCL_CAPS) & DDSCAPS_SYSTEMMEMORY) ) {
        return( NULL );
    }
    gbl = *(BYTE **)((BYTE *)lcl + M8DD_LCL_GBL);
    if( gbl == NULL ) {
        return( NULL );
    }
    if( (*(DWORD *)((BYTE *)lcl + M8DD_LCL_FLAGS) & M8DD_HASPIXELFORMAT) &&
        *(DWORD *)(gbl + M8DD_GBL_PF_BITS) != 16 ) {
        return( NULL );
    }
    *pitch = *(long *)(gbl + M8DD_GBL_PITCH);
    *w = *(WORD *)(gbl + M8DD_GBL_WIDTH);
    *h = *(WORD *)(gbl + M8DD_GBL_HEIGHT);
    return( (BYTE *)*(DWORD *)(gbl + M8DD_GBL_VIDMEM) );
}

/* Fills, and same-size copies (plain or with a one-colour source key) from
 * system memory or from the screen itself, all in RAM, then each changed
 * rectangle sent. Anything else goes back to DDRAW. */
static DWORD m8dd_blt16_do( M8DD_BLT *bd )
{
    DWORD       other = bd->dwFlags & ~(DDBLT_COLORFILL | DDBLT_ROP | DDBLT_WAIT | DDBLT_ASYNC |
                                        DDBLT_KEYSRC | DDBLT_KEYSRCOVERRIDE);
    DWORD       keyed = bd->dwFlags & (DDBLT_KEYSRC | DDBLT_KEYSRCOVERRIDE);
    const RECTL *od = bd->IsClipped ? &bd->rOrigDest : &bd->rDest;
    const RECTL *os = bd->IsClipped ? &bd->rOrigSrc : &bd->rSrc;
    const RECTL *rl;
    BYTE        *src = NULL;
    long        pitch = 0;
    int         sw;
    int         sh;
    int         self = 0;
    WORD        key = 0;
    WORD        fillc;
    DWORD       count;
    DWORD       i;
    RECT        c;
    int         w;
    int         y;
    int         x;
    int         step;
    int         y0;
    int         y1;
    WORD        *d;
    const WORD  *s;
    int         fsx = 0;
    int         fsy = 0;
    int         dk = m8dd_buf( bd->lpDDDestSurface );
    int         sk = -1;
    int         back;

    if( other != 0 || dk < 0 ) {
        return( DDHAL_DRIVER_NOTHANDLED );
    }
    back = dk != m8dd_front;
    if( bd->dwFlags & DDBLT_COLORFILL ) {
        if( keyed ) {
            return( DDHAL_DRIVER_NOTHANDLED );
        }
        fillc = (WORD)bd->bltFX.dwFillColor;
    } else if( bd->lpDDSrcSurface != NULL &&
               (!(bd->dwFlags & DDBLT_ROP) || bd->bltFX.dwROP == M8DD_SRCCOPY) ) {
        /* A source and no ROP is a plain copy. */
        sk = m8dd_buf( bd->lpDDSrcSurface );
        if( sk >= 0 ) {
            self = sk == dk;
            src = (BYTE *)m8dd_at( sk, 0, 0 );
            pitch = m8dd->pitch;
            sw = m8dd->width;
            sh = m8dd->height;
            if( keyed && self ) {
                return( DDHAL_DRIVER_NOTHANDLED );
            }
        } else if( (src = m8dd_sysmem16( bd->lpDDSrcSurface, &pitch, &sw, &sh )) == NULL ) {
            return( DDHAL_DRIVER_NOTHANDLED );
        }
        if( os->right - os->left != od->right - od->left ||
            os->bottom - os->top != od->bottom - od->top ||
            os->left < 0 || os->top < 0 || os->right > sw || os->bottom > sh ) {
            return( DDHAL_DRIVER_NOTHANDLED );
        }
        if( keyed ) {
            const DDCOLORKEY *ck = (bd->dwFlags & DDBLT_KEYSRCOVERRIDE) ?
                &bd->bltFX.ddckSrcColorkey :
                (const DDCOLORKEY *)((BYTE *)bd->lpDDSrcSurface + M8DD_LCL_CKSRCBLT);

            if( ck->dwColorSpaceLowValue != ck->dwColorSpaceHighValue ||
                ck->dwColorSpaceLowValue > 0xFFFF ) {
                return( DDHAL_DRIVER_NOTHANDLED );
            }
            key = (WORD)ck->dwColorSpaceLowValue;
        }
    } else {
        return( DDHAL_DRIVER_NOTHANDLED );
    }
    if( bd->IsClipped ) {
        rl = (const RECTL *)bd->prDestRects;
        count = bd->dwRectCnt;
        if( self && count > 1 ) {
            return( DDHAL_DRIVER_NOTHANDLED );
        }
    } else {
        rl = od;
        count = 1;
    }
    if( back ) {
        /* A clear of the whole back buffer is its description, not writes. */
        m8dd_clip_fb( &c, &rl[0] );
        if( (bd->dwFlags & DDBLT_COLORFILL) && count == 1 && c.left == 0 && c.top == 0 &&
            c.right == (long)m8dd->width && c.bottom == (long)m8dd->height ) {
            m8dd_desc[dk].known = 1;
            m8dd_desc[dk].filled = 0;
            m8dd_desc[dk].colour = fillc;
            m8dd_desc[dk].n = 0;
            m8dd_tri_n[dk] = 0;
            ++m8d3d_stat[18];
            bd->ddRVal = DD_OK;
            return( DDHAL_DRIVER_HANDLED );
        }
        m8dd_tris_to_ram( dk );
    } else {
        /* Drawn on the shown buffer directly: RAM is the truth from here. */
        m8dd_ready_all( dk );
        m8dd_desc[dk].known = 0;
        m8dd_card.known = 0;
    }
    if( sk >= 0 && sk != dk ) {
        m8dd_ready_all( sk );
    }
    for( i = 0; i < count; ++i ) {
        m8dd_clip_fb( &c, &rl[i] );
        w = c.right - c.left;
        if( w <= 0 || c.bottom <= c.top ) {
            continue;
        }
        if( back ) {
            m8dd_ready( dk, &c );
            if( self ) {
                RECT sr;

                sr.left = c.left - od->left + os->left;
                sr.top = c.top - od->top + os->top;
                sr.right = sr.left + w;
                sr.bottom = sr.top + (c.bottom - c.top);
                m8dd_ready( dk, &sr );
            }
        }
        if( bd->dwFlags & DDBLT_COLORFILL ) {
            DWORD   two = fillc | ((DWORD)fillc << 16);
            DWORD   *q;
            int     n;

            for( y = c.top; y < c.bottom; ++y ) {
                d = m8dd_at( dk, c.left, y );
                x = 0;
                if( (DWORD)d & 2 ) {
                    d[x++] = fillc;
                }
                q = (DWORD *)(d + x);
                for( n = (w - x) / 2; n > 0; --n ) {
                    *q++ = two;
                }
                if( (w - x) & 1 ) {
                    d[w - 1] = fillc;
                }
            }
            if( back ) {
                m8dd_dirty( dk, &c );
            } else {
                m8dd_send( &c, 1 );
            }
            continue;
        } else {
            /* Rows in the order that reads each source row before it is
             * overwritten; memmove takes care of a row onto itself. */
            long sx = c.left - od->left + os->left;
            long sy = c.top - od->top + os->top;

            fsx = (int)sx;
            fsy = (int)sy;

            if( self && c.top > sy ) {
                y0 = c.bottom - 1;
                y1 = c.top - 1;
                step = -1;
            } else {
                y0 = c.top;
                y1 = c.bottom;
                step = 1;
            }
            if( !self && !keyed && !back ) {
                M8DD_DIFF   g;

                g.open = 0;
                for( y = c.top; y < c.bottom; ++y ) {
                    m8dd_diff_row( &g, y, c.left, w,
                                   (const WORD *)(src + (sy + y - c.top) * pitch + sx * 2) );
                }
                m8dd_diff_close( &g );
                continue;
            }
            for( y = y0; y != y1; y += step ) {
                d = m8dd_at( dk, c.left, y );
                s = (const WORD *)(src + (sy + y - c.top) * pitch + sx * 2);
                if( keyed ) {
                    for( x = 0; x < w; ++x ) {
                        if( s[x] != key ) {
                            d[x] = s[x];
                        }
                    }
                } else {
                    memmove( d, s, w * 2 );
                }
            }
        }
        if( back ) {
            m8dd_dirty( dk, &c );
        } else if( self ) {
            m8dd_copied( fsx, fsy, &c );
        } else {
            m8dd_send( &c, 0 );
        }
    }
    bd->ddRVal = DD_OK;
    return( DDHAL_DRIVER_HANDLED );
}

/* A blit DDRAW does itself writes and reads RAM directly: the buffers it
 * touches are put in RAM first and held as unknown, so no description is
 * left that no longer matches their pixels. */
static DWORD m8dd_blt16( M8DD_BLT *bd )
{
    LARGE_INTEGER   t0;
    LARGE_INTEGER   t1;
    DWORD           rc;
    int             k;

    QueryPerformanceCounter( &t0 );
    rc = m8dd_blt16_do( bd );
    QueryPerformanceCounter( &t1 );
    m8d3d_stat[16] += t1.LowPart - t0.LowPart;
    ++m8d3d_stat[17];

    if( rc == DDHAL_DRIVER_NOTHANDLED ) {
        if( (k = m8dd_buf( bd->lpDDDestSurface )) >= 0 ) {
            m8dd_ready_all( k );
            m8dd_desc[k].known = 0;
            if( k == m8dd_front ) {
                m8dd_card.known = 0;
            }
        }
        if( (k = m8dd_buf( bd->lpDDSrcSurface )) >= 0 ) {
            m8dd_ready_all( k );
        }
    }
    return( rc );
}

/* DDHAL_FLIPDATA. */
typedef struct {
    LPVOID  lpDD;
    LPVOID  lpSurfCurr;
    LPVOID  lpSurfTarg;
    DWORD   dwFlags;
    HRESULT ddRVal;
    LPVOID  Flip;
} M8DD_FLIP;

/* The part of the card's old rectangle o that none of t's rectangles from
 * `from` on covers, filled with `colour`: a pixel the new frame draws over is
 * never filled first, so nothing on screen goes to the background and back. */
static void m8dd_fill_outside( const RECT *o, const M8DD_DESC *t, int from, WORD colour )
{
    const RECT  *n;
    RECT        p;
    long        top;
    long        bot;

    for( ; from < t->n; ++from ) {
        n = &t->d[from];
        if( n->left >= o->right || n->right <= o->left || n->top >= o->bottom || n->bottom <= o->top ) {
            continue;
        }
        top = o->top > n->top ? o->top : n->top;
        bot = o->bottom < n->bottom ? o->bottom : n->bottom;
        p = *o;
        if( o->top < n->top ) {
            p.bottom = n->top;
            m8dd_fill_outside( &p, t, from + 1, colour );
        }
        p = *o;
        if( n->bottom < o->bottom ) {
            p.top = n->bottom;
            m8dd_fill_outside( &p, t, from + 1, colour );
        }
        p.top = top;
        p.bottom = bot;
        if( o->left < n->left ) {
            p.left = o->left;
            p.right = n->left;
            m8dd_fill_outside( &p, t, from + 1, colour );
        }
        if( n->right < o->right ) {
            p.left = n->right;
            p.right = o->right;
            m8dd_fill_outside( &p, t, from + 1, colour );
        }
        return;
    }
    m8dd_op( 1, o, colour );
}

/* The target buffer shown. While both it and the card are described with the
 * same colour, only the card's old rectangles are filled back with it and the
 * target's rectangles sent: the static part of the frame never crosses the
 * bus, and RAM is not read outside the rectangles. */
/* Each of buffer k's triangles' boxes, for the next flip to erase. */
static void m8dd_note_boxes( int k )
{
    const short *v = m8dd_tri_v[k];
    RECT        *r = m8dd_card_box;
    int         i;
    int         j;

    m8dd_card_boxes = m8dd_tri_n[k];
    memcpy( m8dd_card_v, v, m8dd_card_boxes * 6 * sizeof( short ) );
    memcpy( m8dd_card_i, m8dd_tri_i[k], m8dd_card_boxes * sizeof( WORD ) );
    for( i = 0; i < m8dd_card_boxes; ++i, v += 6, ++r ) {
        r->left = r->right = v[0];
        r->top = r->bottom = v[1];
        for( j = 1; j < 3; ++j ) {
            if( v[2 * j] < r->left ) {
                r->left = v[2 * j];
            }
            if( v[2 * j] > r->right ) {
                r->right = v[2 * j];
            }
            if( v[2 * j + 1] < r->top ) {
                r->top = v[2 * j + 1];
            }
            if( v[2 * j + 1] > r->bottom ) {
                r->bottom = v[2 * j + 1];
            }
        }
        ++r->right;
        ++r->bottom;
    }
}

/* A frame of only triangles on black, over a card that shows the same: nothing goes
 * through the 16-bit side, which costs a GetDC and a thunk. The last frame's boxes
 * are erased and the triangles drawn; the buffer's selection waits in the batch for
 * the next time RAM has to reach the card. */
static void m8dd_flip16_fast( int k )
{
    WORD    *e;

    if( m8dd_bl[0] != 0 && (e = m8dd_bl + 1 + 6 * (m8dd_bl[0] - 1))[0] == 2 ) {
        e[5] = (WORD)k;
    } else {
        m8dd_op( 2, NULL, (WORD)k );
    }
    /* Index 0 is black in fb16.c's palette. */
    /* The same triangles as the card shows: the frame is there already. */
    if( m8dd_tri_n[k] != m8dd_card_boxes ||
        memcmp( m8dd_tri_v[k], m8dd_card_v, m8dd_card_boxes * 6 * sizeof( short ) ) != 0 ||
        memcmp( m8dd_tri_i[k], m8dd_card_i, m8dd_card_boxes * sizeof( WORD ) ) != 0 ) {
        m8d3d_replay( m8dd_card_box, m8dd_card_boxes, 0, m8dd_tri_v[k], m8dd_tri_i[k], m8dd_tri_n[k] );
        m8dd_note_boxes( k );
        m8dd_fp_stale = 1;
    }
    m8dd_card = m8dd_desc[k];
    m8dd_front = k;
}

static DWORD m8dd_flip16( M8DD_FLIP *fd )
{
    int             k = m8dd_buf( fd->lpSurfTarg );
    M8DD_DESC       *t;
    int             i;
    LARGE_INTEGER   t0;
    LARGE_INTEGER   t1;
    LARGE_INTEGER   t2;

    if( k < 0 || m8dd_back == 0 ) {
        return( DDHAL_DRIVER_NOTHANDLED );
    }
    QueryPerformanceCounter( &t0 );
    /* GDI drew on the card since the last Flip (a program's own window
     * repainted): the card is no longer as described. */
    if( m8dd->gdi_lin != 0 && *(DWORD *)m8dd->gdi_lin != m8dd_gdi_seen ) {
        m8dd_gdi_seen = *(DWORD *)m8dd->gdi_lin;
        m8dd_card.known = 0;
    }
    t = &m8dd_desc[k];
    if( t->known && t->n == 0 && t->colour == 0 && m8dd_card.known && m8dd_card.n == 0 && m8dd_card.colour == 0 ) {
        m8dd_flip16_fast( k );
        QueryPerformanceCounter( &t2 );
        m8d3d_stat[12] += t2.LowPart - t0.LowPart;
        m8d3d_stat[14] += t2.LowPart - t0.LowPart;
        ++m8d3d_stat[15];
        fd->ddRVal = DD_OK;
        return( DDHAL_DRIVER_HANDLED );
    }
    if( m8dd_fp_stale ) {
        m8dd_op( 5, NULL, 0 );
        m8dd_fp_stale = 0;
    }
    m8dd_op( 2, NULL, (WORD)k );
    if( t->known ) {
        if( m8dd_card.known && m8dd_card.colour == t->colour ) {
            for( i = 0; i < m8dd_card.n; ++i ) {
                m8dd_fill_outside( &m8dd_card.d[i], t, 0, t->colour );
            }
            for( i = 0; i < m8dd_card_boxes; ++i ) {
                m8dd_fill_outside( &m8dd_card_box[i], t, 0, t->colour );
            }
        } else {
            m8dd_op( 1, NULL, t->colour );
        }
        for( i = 0; i < t->n; ++i ) {
            m8dd_op( 0, &t->d[i], 0 );
        }
        m8dd_card = *t;
    } else {
        m8dd_op( 0, NULL, 0 );
        m8dd_card.known = 0;
    }
    m8dd_flush();
    QueryPerformanceCounter( &t1 );
    if( m8dd_tri_n[k] ) {
        m8d3d_replay( NULL, 0, 0, m8dd_tri_v[k], m8dd_tri_i[k], m8dd_tri_n[k] );
        m8dd_fp_stale = 1;
    }
    m8dd_note_boxes( k );
    QueryPerformanceCounter( &t2 );
    m8d3d_stat[12] += t2.LowPart - t0.LowPart;
    m8d3d_stat[13] += t1.LowPart - t0.LowPart;
    m8d3d_stat[14] += t2.LowPart - t1.LowPart;
    ++m8d3d_stat[15];
    m8dd_front = k;
    fd->ddRVal = DD_OK;
    return( DDHAL_DRIVER_HANDLED );
}

DWORD __stdcall m8dd_flip( M8DD_FLIP *fd )
{
    if( !m8dd->fb16 ) {
        return( DDHAL_DRIVER_NOTHANDLED );
    }
    return( m8dd_flip16( fd ) );
}

/* At 16 bpp, a primary with one back buffer: the primary is the frame
 * buffer, the back buffer a second one fb16.c allocates. Anything else goes
 * back to DDRAW. */
static DWORD m8dd_create16( M8DD_CREATE *cd )
{
    BYTE    *lcl;
    BYTE    *gbl;
    DWORD   caps;
    DWORD   i;
    int     prim = -1;

    if( cd->dwSCnt != 2 || m8dd_back != 0 ) {
        return( DDHAL_DRIVER_NOTHANDLED );
    }
    for( i = 0; i < 2; ++i ) {
        caps = *(DWORD *)((BYTE *)cd->lplpSList[i] + M8DD_LCL_CAPS);
        if( !(caps & DDSCAPS_FLIP) || (caps & DDSCAPS_SYSTEMMEMORY) ) {
            return( DDHAL_DRIVER_NOTHANDLED );
        }
        if( caps & DDSCAPS_PRIMARYSURFACE ) {
            prim = (int)i;
        }
    }
    if( prim < 0 ) {
        return( DDHAL_DRIVER_NOTHANDLED );
    }
    m8dd_op( 3, NULL, 0 );
    m8dd_back = m8dd_flush();
    if( m8dd_back == 0 ) {
        cd->ddRVal = DDERR_OUTOFVIDEOMEMORY;
        return( DDHAL_DRIVER_HANDLED );
    }
    for( i = 0; i < 2; ++i ) {
        lcl = cd->lplpSList[i];
        gbl = *(BYTE **)(lcl + M8DD_LCL_GBL);
        *(DWORD *)(gbl + M8DD_GBL_VIDMEM) = (int)i == prim ? m8dd->fb_lin : m8dd_back;
        *(long *)(gbl + M8DD_GBL_PITCH) = m8dd->pitch;
        *(DWORD *)(lcl + M8DD_LCL_CAPS) |= DDSCAPS_VIDEOMEMORY;
    }
    /* Both buffers start black, as described, not written: the frame
     * buffer still holds the desktop at the old mode's pitch, and a program
     * that only erases what it moved would keep it as its background. The
     * card is not touched from inside CreateSurface: the first Flip fills it. */
    m8dd_front = 0;
    m8dd_desc[0].known = 1;
    m8dd_desc[0].filled = 0;
    m8dd_desc[0].colour = 0;
    m8dd_desc[0].n = 0;
    m8dd_desc[1] = m8dd_desc[0];
    m8dd_tri_n[0] = 0;
    m8dd_tri_n[1] = 0;
    m8dd_card_boxes = 0;
    m8dd_fp_stale = 0;
    m8dd_card.known = 0;
    cd->ddRVal = DD_OK;
    return( DDHAL_DRIVER_HANDLED );
}

/* The back buffer goes: the frame buffer, put in RAM if it is described,
 * is shown again and sent whole, as the card may show either buffer. */
static int m8dd_destroy16( BYTE *gbl )
{
    if( m8dd_back == 0 || *(DWORD *)(gbl + M8DD_GBL_VIDMEM) != m8dd_back ) {
        return( 0 );
    }
    m8dd_ready_all( 0 );
    m8dd_op( 2, NULL, 0 );
    m8dd_op( 5, NULL, 0 );
    m8dd_op( 0, NULL, 0 );
    m8dd_op( 4, NULL, 0 );
    m8dd_flush();
    m8dd_tri_n[0] = 0;
    m8dd_tri_n[1] = 0;
    m8dd_back = 0;
    m8dd_front = 0;
    m8dd_desc[0].known = 0;
    m8dd_desc[1].known = 0;
    m8dd_card.known = 0;
    *(DWORD *)(gbl + M8DD_GBL_VIDMEM) = 0;
    return( 1 );
}

/* A system-memory surface in the primary's 8bpp format: its first byte,
 * pitch and size, or NULL. */
static BYTE *m8dd_sysmem( LPVOID lcl, long *pitch, int *w, int *h )
{
    BYTE    *gbl;

    if( lcl == NULL || !(*(DWORD *)((BYTE *)lcl + M8DD_LCL_CAPS) & DDSCAPS_SYSTEMMEMORY) ) {
        return( NULL );
    }
    gbl = *(BYTE **)((BYTE *)lcl + M8DD_LCL_GBL);
    if( gbl == NULL ) {
        return( NULL );
    }
    if( (*(DWORD *)((BYTE *)lcl + M8DD_LCL_FLAGS) & M8DD_HASPIXELFORMAT) &&
        (*(DWORD *)(gbl + M8DD_GBL_PF_BITS) != 8 ||
         !(*(DWORD *)(gbl + M8DD_GBL_PF_FLAGS) & DDPF_PALETTEINDEXED8)) ) {
        return( NULL );
    }
    *pitch = *(long *)(gbl + M8DD_GBL_PITCH);
    *w = *(WORD *)(gbl + M8DD_GBL_WIDTH);
    *h = *(WORD *)(gbl + M8DD_GBL_HEIGHT);
    return( (BYTE *)*(DWORD *)(gbl + M8DD_GBL_VIDMEM) );
}

/* One rectangle of a system-memory surface to the card as host data, from
 * the plain data path with a host mix. Host words carry two pixels, so the
 * blit starts and ends on even columns and the scissors cut it to `d`; a
 * column outside `d` is sent as 0, not read. */
static int m8dd_host_rect( const RECT *d, const BYTE *src, long pitch )
{
    static WORD row[M8_XROW_WORDS];
    int     x0 = d->left & ~1;
    int     words = (((d->right + 1) & ~1) - x0) / 2;
    int     h = d->bottom - d->top;
    int     y;
    int     i;
    int     c;
    const BYTE *s;

    if( d->right <= d->left || h <= 0 ) {
        return( 1 );
    }
    if( words > M8_XROW_WORDS || !m8dd_idle() ) {
        return( 0 );
    }
    outpw( M8_EXT_SCISSOR_L, d->left );
    outpw( M8_EXT_SCISSOR_R, d->right - 1 );
    outpw( M8_CUR_X, x0 );
    outpw( M8_CUR_Y, d->top );
    outpw( M8_MAJ_AXIS_PCNT, words * 2 - 1 );
    outpw( M8_MULTIFUNC_CNTL, M8_MF_MIN_AXIS | (h - 1) );
    outpw( M8_CMD, M8_CMD_HOST_WRITE );
    for( y = 0; y < h; ++y ) {
        /* `s` is the source pixel for column d->left. */
        s = src + y * pitch;
        if( !(d->left & 1) && !(d->right & 1) ) {
            m8dd_outsw( (const WORD *)s, words );
            continue;
        }
        for( i = 0; i < words; ++i ) {
            c = x0 + i * 2;
            row[i] = (WORD)((c >= d->left ? s[c - d->left] : 0) |
                            ((c + 1 < d->right ? s[c + 1 - d->left] : 0) << 8));
        }
        m8dd_outsw( row, words );
    }
    return( m8dd_idle() );
}

/* Colour fills on a card surface and same-size SRCCOPY between card surfaces,
 * on the engine: only the commands cross the bus. Same-size SRCCOPY from a
 * system-memory surface, as host data: the pixels cross once, where DDRAW's
 * own path would read the whole destination back and write it again.
 * Anything else goes back to DDRAW, and so does a copy within one surface
 * clipped to more than one rectangle, as their order matters when they
 * overlap. */
DWORD __stdcall m8dd_blt( M8DD_BLT *bd )
{
    BYTE        *hot = (BYTE *)m8dd->hot;
    DWORD       other = bd->dwFlags & ~(DDBLT_COLORFILL | DDBLT_ROP | DDBLT_WAIT | DDBLT_ASYNC |
                                        DDBLT_KEYSRC | DDBLT_KEYSRCOVERRIDE);
    DWORD       keyed = bd->dwFlags & (DDBLT_KEYSRC | DDBLT_KEYSRCOVERRIDE);
    BYTE        key = 0;
    const RECTL *od = bd->IsClipped ? &bd->rOrigDest : &bd->rDest;
    const RECTL *os = bd->IsClipped ? &bd->rOrigSrc : &bd->rSrc;
    const RECTL *rl;
    M8DD_SURF   d;
    M8DD_SURF   s;
    BYTE        *src = NULL;
    long        pitch = 0;
    int         sw;
    int         sh;
    int         kind;                   /* 0 fill, 1 card to card, 2 host data */
    DWORD       count;
    DWORD       i;
    RECT        c;
    long        sx;
    long        sy;
    int         ok;

    if( m8dd->fb16 ) {
        return( m8dd_blt16( bd ) );
    }
    if( other != 0 || hot[H_HW_OFF] || !m8dd_surf( bd->lpDDDestSurface, &d ) ) {
        return( DDHAL_DRIVER_NOTHANDLED );
    }
    if( bd->dwFlags & DDBLT_COLORFILL ) {
        kind = 0;
    } else if( (bd->dwFlags & DDBLT_ROP) && bd->bltFX.dwROP == M8DD_SRCCOPY ) {
        if( m8dd_surf( bd->lpDDSrcSurface, &s ) ) {
            kind = 1;
            sw = s.w;
            sh = s.h;
        } else if( (src = m8dd_sysmem( bd->lpDDSrcSurface, &pitch, &sw, &sh )) != NULL ) {
            kind = 2;
        } else {
            return( DDHAL_DRIVER_NOTHANDLED );
        }
        if( os->right - os->left != od->right - od->left ||
            os->bottom - os->top != od->bottom - od->top ||
            os->left < 0 || os->top < 0 || os->right > sw || os->bottom > sh ) {
            return( DDHAL_DRIVER_NOTHANDLED );
        }
    } else {
        return( DDHAL_DRIVER_NOTHANDLED );
    }
    /* A source colour key: one colour (not a range), card to card, between
     * two surfaces. */
    if( keyed ) {
        const DDCOLORKEY *ck = (bd->dwFlags & DDBLT_KEYSRCOVERRIDE) ? &bd->bltFX.ddckSrcColorkey :
            (const DDCOLORKEY *)((BYTE *)bd->lpDDSrcSurface + M8DD_LCL_CKSRCBLT);

        if( kind != 1 || bd->lpDDSrcSurface == bd->lpDDDestSurface ||
            ck->dwColorSpaceLowValue != ck->dwColorSpaceHighValue ||
            ck->dwColorSpaceLowValue > 0xFF ) {
            return( DDHAL_DRIVER_NOTHANDLED );
        }
        key = (BYTE)ck->dwColorSpaceLowValue;
    }
    if( bd->IsClipped ) {
        rl = (const RECTL *)bd->prDestRects;
        count = bd->dwRectCnt;
        if( kind == 1 && count > 1 && bd->lpDDSrcSurface == bd->lpDDDestSurface ) {
            return( DDHAL_DRIVER_NOTHANDLED );
        }
    } else {
        rl = od;
        count = 1;
    }
    hot[H_BUSY] = 1;
    ok = m8dd_plain( kind == 0 ? M8_MIX_FRGD_REPLACE :
                     kind == 1 ? M8_MIX_VRAM_REPLACE : M8_MIX_HOST_REPLACE );
    if( ok && kind == 0 ) {
        outpw( M8_FRGD_COLOR, (WORD)(bd->bltFX.dwFillColor & 0xFF) );
    }
    for( i = 0; ok && i < count; ++i ) {
        m8dd_to_card( &c, &rl[i], &d );
        /* The source pixel for the card rectangle's top left. */
        sx = c.left - d.x - od->left + os->left;
        sy = c.top - d.y - od->top + os->top;
        if( c.right <= c.left || c.bottom <= c.top ) {
            continue;
        }
        if( kind != 0 && (sx < os->left || sy < os->top) ) {
            ok = 0;
        } else if( kind == 0 ) {
            ok = m8dd_rect( &c, 0, 0, 0 );
        } else if( kind == 1 && keyed ) {
            ok = m8dd_key_rect( &c, s.x + (int)sx, s.y + (int)sy, key );
        } else if( kind == 1 ) {
            ok = m8dd_rect( &c, 1, s.x + (int)sx, s.y + (int)sy );
        } else {
            ok = m8dd_host_rect( &c, src + sy * pitch + sx, pitch );
        }
    }
    m8dd_forget( hot );
    hot[H_BUSY] = 0;
    bd->ddRVal = ok ? DD_OK : M8DD_DDERR_GENERIC;
    return( DDHAL_DRIVER_HANDLED );
}

/* The heap's rows are GDI's caches' until DirectDraw has a surface there:
 * the driver's escape (screen.c m8_dd_lend) clears them first. */
static int m8dd_lend( WORD on )
{
    HDC dc = GetDC( NULL );
    int ok = 0;

    if( dc != NULL ) {
        ok = ExtEscape( dc, M8_ESC_DDLEND, sizeof( on ), (LPCSTR)&on, 0, NULL ) > 0;
        ReleaseDC( NULL, dc );
    }
    return( ok );
}

/* Off-screen surfaces: rectangles in the heap rows, placed here rather than
 * by DDRAW from a heap. A candidate corner is the heap's, or right of or
 * below a placed surface; the lowest that fits is taken. */
#define M8DD_PLACED_MAX     32

typedef struct {
    BYTE    *gbl;                       /* 0: free */
    RECT    r;
} M8DD_PLACED;

static M8DD_PLACED  m8dd_placed[M8DD_PLACED_MAX];
static int          m8dd_placed_n;

static int m8dd_fits( int x, int y, int w, int h )
{
    int     k;
    RECT    *p;

    if( x + w > (int)m8dd->pitch || y + h > (int)(m8dd->heap_top + m8dd->heap_rows) ) {
        return( 0 );
    }
    for( k = 0; k < M8DD_PLACED_MAX; ++k ) {
        p = &m8dd_placed[k].r;
        if( m8dd_placed[k].gbl != NULL &&
            x < p->right && p->left < x + w && y < p->bottom && p->top < y + h ) {
            return( 0 );
        }
    }
    return( 1 );
}

static int m8dd_place( int w, int h, RECT *out )
{
    int     cx[1 + 2 * M8DD_PLACED_MAX];
    int     cy[1 + 2 * M8DD_PLACED_MAX];
    int     n = 1;
    int     k;
    int     best = -1;
    RECT    *p;

    cx[0] = 0;
    cy[0] = m8dd->heap_top;
    for( k = 0; k < M8DD_PLACED_MAX; ++k ) {
        if( m8dd_placed[k].gbl != NULL ) {
            p = &m8dd_placed[k].r;
            cx[n] = p->right;
            cy[n++] = p->top;
            cx[n] = p->left;
            cy[n++] = p->bottom;
        }
    }
    for( k = 0; k < n; ++k ) {
        if( m8dd_fits( cx[k], cy[k], w, h ) &&
            (best < 0 || cy[k] < cy[best] || (cy[k] == cy[best] && cx[k] < cx[best])) ) {
            best = k;
        }
    }
    if( best < 0 ) {
        return( 0 );
    }
    SetRect( out, cx[best], cy[best], cx[best] + w, cy[best] + h );
    return( 1 );
}

static void m8dd_unplace( BYTE *gbl )
{
    int k;

    for( k = 0; k < M8DD_PLACED_MAX; ++k ) {
        if( m8dd_placed[k].gbl == gbl ) {
            m8dd_placed[k].gbl = NULL;
            if( --m8dd_placed_n == 0 ) {
                m8dd_lend( 0 );
            }
            return;
        }
    }
}

/* m8d3d.c's way to the engine and the card's memory. */
int m8d3d_surface( LPVOID lcl, int *x, int *y, int *w, int *h )
{
    M8DD_SURF s;

    if( m8dd->fb16 ) {
        if( m8dd_buf( lcl ) < 0 ) {
            return( 0 );
        }
        *x = 0;
        *y = 0;
        *w = m8dd->width;
        *h = m8dd->height;
        return( 1 );
    }
    if( !m8dd_surf( lcl, &s ) ) {
        return( 0 );
    }
    *x = s.x;
    *y = s.y;
    *w = s.w;
    *h = s.h;
    return( 1 );
}

int m8d3d_fb16( void )
{
    return( m8dd->fb16 != 0 );
}

/* One fill on the path m8d3d_engine_begin set, in the colour the caller wrote. */
int m8d3d_fill( const RECT *r )
{
    return( m8dd_rect( r, 0, 0, 0 ) );
}

/* A triangle added to the description of the buffer `lcl` is, or 0 if there is
 * none to keep it. The shown buffer has no description worth keeping: its RAM
 * takes it, and the changed part is sent. */
int m8d3d_record( LPVOID lcl, const short *v, BYTE index, WORD colour )
{
    int     k = m8dd_buf( lcl );
    int     t;
    int     n;
    short   *p;
    RECT    r;

    /* Per triangle: nothing here may call out of this DLL, least of all USER. */
    if( k < 0 ) {
        return( 0 );
    }
    if( k == m8dd_front || !m8dd_desc[k].known ) {
        m8dd_tri_ram( k, v, colour );
        if( k == m8dd_front ) {
            m8dd_card.known = 0;
            r.left = 0;
            r.top = 0;
            r.right = m8dd->width;
            r.bottom = m8dd->height;
            m8dd_send( &r, 0 );
        }
        return( 1 );
    }
    if( m8dd_tri_n[k] == M8DD_TRIS_MAX ) {
        m8dd_tris_to_ram( k );
    }
    n = m8dd_tri_n[k]++;
    p = &m8dd_tri_v[k][6 * n];
    for( t = 0; t < 6; ++t ) {
        p[t] = v[t];
    }
    m8dd_tri_i[k][n] = index;
    m8dd_tri_c[k][n] = colour;
    return( 1 );
}

/* A rectangle no surface will take, held under `key`. At 16 bpp, the off-screen rows;
 * *h then says how many there are, which can be fewer than the target has. */
int m8d3d_scratch( LPVOID key, int w, int *h, int *x, int *y )
{
    RECT    r;
    int     k;

    if( m8dd->fb16 ) {
        if( !m8dd_lend( 1 ) ) {
            return( 0 );
        }
        *x = 0;
        *y = m8dd->scratch_top;
        *h = m8dd->scratch_rows;
        return( 1 );
    }
    if( !m8dd->heap_rows || !m8dd_place( w, *h, &r ) ) {
        return( 0 );
    }
    for( k = 0; k < M8DD_PLACED_MAX && m8dd_placed[k].gbl != NULL; ++k ) {
    }
    if( k == M8DD_PLACED_MAX || (m8dd_placed_n == 0 && !m8dd_lend( 1 )) ) {
        return( 0 );
    }
    m8dd_placed[k].gbl = (BYTE *)key;
    m8dd_placed[k].r = r;
    ++m8dd_placed_n;
    *x = r.left;
    *y = r.top;
    return( 1 );
}

void m8d3d_scratch_free( LPVOID key )
{
    if( m8dd->fb16 ) {
        m8dd_lend( 0 );
        return;
    }
    m8dd_unplace( (BYTE *)key );
}

/* Room for n writes (1..15), as m8_fifo_need: bit 15 - n clear. */
int m8d3d_room( int n )
{
    unsigned i;

    for( i = 0; i < M8_WAIT_LIMIT; ++i ) {
        if( !(inpw( M8_EXT_FIFO_STATUS ) & (0x8000 >> n)) ) {
            return( 1 );
        }
    }
    return( 0 );
}

int m8d3d_engine_begin( void )
{
    BYTE *hot = (BYTE *)m8dd->hot;

    if( hot[H_HW_OFF] ) {
        return( 0 );
    }
    hot[H_BUSY] = 1;
    if( !m8dd_plain( M8_MIX_FRGD_REPLACE ) ) {
        hot[H_BUSY] = 0;
        return( 0 );
    }
    return( 1 );
}

/* Black over a card rectangle, with the plain path back afterwards. */
int m8d3d_clear( int l, int t, int r, int b )
{
    RECT c;

    if( !m8dd_plain( M8_MIX_FRGD_REPLACE ) ) {
        return( 0 );
    }
    outpw( M8_FRGD_COLOR, 0 );
    SetRect( &c, l, t, r, b );
    return( m8dd_rect( &c, 0, 0, 0 ) );
}

void m8d3d_engine_end( void )
{
    BYTE *hot = (BYTE *)m8dd->hot;

    m8dd_plain( M8_MIX_FRGD_REPLACE );
    m8dd_forget( hot );
    hot[H_BUSY] = 0;
}

/* An off-screen surface in the screen's 8bpp format goes in the heap rows;
 * the first one has GDI's caches leave them. A list holding the primary, or
 * anything else, goes back to DDRAW, which then puts it in system memory. */
DWORD __stdcall m8dd_create_surface( M8DD_CREATE *cd )
{
    BYTE    *lcl;
    BYTE    *gbl;
    DWORD   i;
    DWORD   caps;
    int     k;
    RECT    r;

    if( m8dd->fb16 ) {
        return( m8dd_create16( cd ) );
    }
    if( m8dd->heap_rows == 0 || cd->dwSCnt == 0 ) {
        return( DDHAL_DRIVER_NOTHANDLED );
    }
    for( i = 0; i < cd->dwSCnt; ++i ) {
        lcl = cd->lplpSList[i];
        caps = *(DWORD *)(lcl + M8DD_LCL_CAPS);
        gbl = *(BYTE **)(lcl + M8DD_LCL_GBL);
        if( (caps & (DDSCAPS_PRIMARYSURFACE | DDSCAPS_SYSTEMMEMORY | DDSCAPS_FLIP |
                     DDSCAPS_TEXTURE | DDSCAPS_ZBUFFER | DDSCAPS_OVERLAY)) ||
            ((*(DWORD *)(lcl + M8DD_LCL_FLAGS) & M8DD_HASPIXELFORMAT) &&
             (*(DWORD *)(gbl + M8DD_GBL_PF_BITS) != 8 ||
              !(*(DWORD *)(gbl + M8DD_GBL_PF_FLAGS) & DDPF_PALETTEINDEXED8))) ) {
            return( DDHAL_DRIVER_NOTHANDLED );
        }
    }
    if( m8dd_placed_n + cd->dwSCnt > M8DD_PLACED_MAX ) {
        cd->ddRVal = DDERR_OUTOFVIDEOMEMORY;
        return( DDHAL_DRIVER_HANDLED );
    }
    if( m8dd_placed_n == 0 && !m8dd_lend( 1 ) ) {
        cd->ddRVal = DDERR_OUTOFVIDEOMEMORY;
        return( DDHAL_DRIVER_HANDLED );
    }
    for( i = 0; i < cd->dwSCnt; ++i ) {
        lcl = cd->lplpSList[i];
        gbl = *(BYTE **)(lcl + M8DD_LCL_GBL);
        if( !m8dd_place( *(WORD *)(gbl + M8DD_GBL_WIDTH), *(WORD *)(gbl + M8DD_GBL_HEIGHT), &r ) ) {
            while( i-- > 0 ) {
                m8dd_unplace( *(BYTE **)((BYTE *)cd->lplpSList[i] + M8DD_LCL_GBL) );
            }
            if( m8dd_placed_n == 0 ) {
                m8dd_lend( 0 );
            }
            cd->ddRVal = DDERR_OUTOFVIDEOMEMORY;
            return( DDHAL_DRIVER_HANDLED );
        }
        for( k = 0; m8dd_placed[k].gbl != NULL; ++k ) {
        }
        m8dd_placed[k].gbl = gbl;
        m8dd_placed[k].r = r;
        ++m8dd_placed_n;
        *(DWORD *)(gbl + M8DD_GBL_VIDMEM) = m8dd->shadow + (DWORD)r.top * m8dd->pitch + r.left;
        *(long *)(gbl + M8DD_GBL_PITCH) = m8dd->pitch;
    }
    cd->ddRVal = DD_OK;
    return( DDHAL_DRIVER_HANDLED );
}

DWORD __stdcall m8dd_destroy_surface( M8DD_DESTROY *dd )
{
    BYTE    *gbl;
    int     k;

    for( k = 0; k < M8DD_HELD_MAX; ++k ) {
        if( m8dd_held[k].lcl == dd->lpDDSurface ) {
            m8dd_held[k].lcl = NULL;
        }
    }
    gbl = *(BYTE **)((BYTE *)dd->lpDDSurface + M8DD_LCL_GBL);
    if( m8dd->fb16 && gbl != NULL && m8dd_destroy16( gbl ) ) {
        dd->ddRVal = DD_OK;
        return( DDHAL_DRIVER_HANDLED );
    }
    for( k = 0; k < M8DD_PLACED_MAX; ++k ) {
        if( m8dd_placed[k].gbl == gbl ) {
            m8dd_unplace( gbl );
            *(DWORD *)(gbl + M8DD_GBL_VIDMEM) = 0;
            dd->ddRVal = DD_OK;
            return( DDHAL_DRIVER_HANDLED );
        }
    }
    return( DDHAL_DRIVER_NOTHANDLED );
}

DWORD __stdcall m8dd_destroy_driver( M8DD_DESTROYDRV *dd )
{
    int k;

    if( m8dd_placed_n > 0 ) {
        for( k = 0; k < M8DD_PLACED_MAX; ++k ) {
            m8dd_placed[k].gbl = NULL;
        }
        m8dd_placed_n = 0;
        m8dd_lend( 0 );
    }
    return( DDHAL_DRIVER_NOTHANDLED );
}

BOOL WINAPI DllMain( HINSTANCE inst, DWORD reason, LPVOID reserved )
{
    if( reason == DLL_PROCESS_ATTACH ) {
        m8dd_inst = inst;
        DisableThreadLibraryCalls( inst );
    }
    return( TRUE );
}

DWORD __stdcall DriverInit( LPVOID context )
{
    if( context == NULL ) {
        return( 1 );
    }
    m8dd = context;
    m8dd->hInstance = (DWORD)m8dd_inst;
    m8dd->lock = (DWORD)m8dd_lock;
    m8dd->unlock = (DWORD)m8dd_unlock;
    m8dd->blt = (DWORD)m8dd_blt;
    m8dd->flip = (DWORD)m8dd_flip;
    m8d3d_init( &m8dd->d3d_global, &m8dd->d3d_callbacks );
    m8dd->create_surface = (DWORD)m8dd_create_surface;
    m8dd->destroy_surface = (DWORD)m8dd_destroy_surface;
    m8dd->destroy_driver = (DWORD)m8dd_destroy_driver;
    m8dd->marker = M8DD_MARKER;
    return( 1 );
}
