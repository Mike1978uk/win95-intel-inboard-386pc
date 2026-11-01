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
 * The 8514 half has no frame buffer the CPU can see, so the primary is a
 * shadow in system RAM: Lock reads the locked rectangle from the card into
 * it, Unlock writes it back, both through PIX_TRANS as m8_hw_read_rect and
 * m8_hw_write_rect do. DDRAW holds the Win16 lock from a primary Lock to its
 * Unlock unless the application asks for DDLOCK_NOSYSLOCK, so GDI's entries
 * do not run in between; the engine set-up GDI's shadows describe is
 * forgotten afterwards, so its next call writes it again.
 */

#include <windows.h>
#include <conio.h>
#include <stddef.h>
#include <ddraw.h>

#define M8DD_MARKER     0x44443855UL

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

typedef char m8dd_fx_size[sizeof( DDBLTFX ) == 100 ? 1 : -1];
typedef char m8dd_fill_at[offsetof( DDBLTFX, dwFillColor ) == 80 ? 1 : -1];

/* The surface's global part is the second pointer of its local part, and its
 * fpVidMem is at 20 (ddrawi.h). Only a surface whose fpVidMem is the shadow
 * is taken; any other, or a layout not as read here, goes back to DDRAW. */
#define M8DD_LCL_GBL        4
#define M8DD_GBL_VIDMEM     20

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

static HINSTANCE    m8dd_inst;
static M8DD_SHARE   *m8dd;
static RECT         m8dd_locked;        /* the rectangle Unlock writes back, even x and width */

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

/* The rectangle between the card and the shadow, from an idle engine; the
 * engine is idle again after a write, as a colour write while it still takes
 * host data would be taken as pixels (guide 8-14). */
static int m8dd_transfer( const RECT *r, int to_card )
{
    BYTE    *hot = (BYTE *)m8dd->hot;
    WORD    *row;
    int     w = r->right - r->left;
    int     h = r->bottom - r->top;
    int     y;
    int     x;
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
                for( x = 0; x < w / 2; ++x ) {
                    outpw( M8_PIX_TRANS, row[x] );
                }
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
                    for( x = 0; x < w / 2; ++x ) {
                        row[x] = inpw( M8_PIX_TRANS );
                    }
                }
                ok = 1;
            }
        }
    }
    m8dd_forget( hot );
    hot[H_BUSY] = 0;
    return( ok );
}

static int m8dd_is_primary( LPVOID lcl )
{
    BYTE *gbl;

    if( lcl == NULL ) {
        return( 0 );
    }
    gbl = *(BYTE **)((BYTE *)lcl + M8DD_LCL_GBL);
    return( gbl != NULL && *(DWORD *)(gbl + M8DD_GBL_VIDMEM) == m8dd->shadow );
}

/* The locked rectangle, clipped to the screen; host transfers move two
 * pixels a word, so its left edge goes down and its right edge up to even. */
DWORD __stdcall m8dd_lock( M8DD_LOCK *ld )
{
    RECT r;

    if( !m8dd_is_primary( ld->lpDDSurface ) ) {
        return( DDHAL_DRIVER_NOTHANDLED );
    }
    if( ld->bHasRect ) {
        r.left = ld->rArea.left < 0 ? 0 : ld->rArea.left;
        r.top = ld->rArea.top < 0 ? 0 : ld->rArea.top;
        r.right = ld->rArea.right > (LONG)m8dd->width ? (LONG)m8dd->width : ld->rArea.right;
        r.bottom = ld->rArea.bottom > (LONG)m8dd->height ? (LONG)m8dd->height : ld->rArea.bottom;
    } else {
        SetRect( &r, 0, 0, m8dd->width, m8dd->height );
    }
    ld->lpSurfData = (LPVOID)(m8dd->shadow + (DWORD)r.top * m8dd->pitch + r.left);
    r.left &= ~1;
    r.right = (r.right + 1) & ~1;
    if( !m8dd_transfer( &r, 0 ) ) {
        SetRectEmpty( &m8dd_locked );
        ld->ddRVal = M8DD_DDERR_GENERIC;
        return( DDHAL_DRIVER_HANDLED );
    }
    m8dd_locked = r;
    ld->ddRVal = DD_OK;
    return( DDHAL_DRIVER_HANDLED );
}

DWORD __stdcall m8dd_unlock( M8DD_UNLOCK *ud )
{
    if( !m8dd_is_primary( ud->lpDDSurface ) ) {
        return( DDHAL_DRIVER_NOTHANDLED );
    }
    ud->ddRVal = m8dd_transfer( &m8dd_locked, 1 ) ? DD_OK : M8DD_DDERR_GENERIC;
    SetRectEmpty( &m8dd_locked );
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

/* Colour fills on the primary and same-size copies within it, on the engine:
 * only the commands cross the bus. Anything else goes back to DDRAW, and so
 * does a copy clipped to more than one rectangle, as their order matters
 * when source and destination overlap. */
DWORD __stdcall m8dd_blt( M8DD_BLT *bd )
{
    BYTE        *hot = (BYTE *)m8dd->hot;
    DWORD       other = bd->dwFlags & ~(DDBLT_COLORFILL | DDBLT_ROP | DDBLT_WAIT | DDBLT_ASYNC);
    int         copy;
    RECT        one;
    const RECT  *rects;
    DWORD       count;
    DWORD       i;
    int         ok;
    long        ox = 0;
    long        oy = 0;

    if( other != 0 || hot[H_HW_OFF] || !m8dd_is_primary( bd->lpDDDestSurface ) ) {
        return( DDHAL_DRIVER_NOTHANDLED );
    }
    if( bd->dwFlags & DDBLT_COLORFILL ) {
        copy = 0;
    } else if( (bd->dwFlags & DDBLT_ROP) && bd->bltFX.dwROP == M8DD_SRCCOPY &&
               m8dd_is_primary( bd->lpDDSrcSurface ) ) {
        copy = 1;
    } else {
        return( DDHAL_DRIVER_NOTHANDLED );
    }
    if( bd->IsClipped ) {
        rects = bd->prDestRects;
        count = bd->dwRectCnt;
        if( copy ) {
            if( count > 1 ||
                bd->rOrigSrc.right - bd->rOrigSrc.left != bd->rOrigDest.right - bd->rOrigDest.left ||
                bd->rOrigSrc.bottom - bd->rOrigSrc.top != bd->rOrigDest.bottom - bd->rOrigDest.top ) {
                return( DDHAL_DRIVER_NOTHANDLED );
            }
            ox = bd->rOrigSrc.left - bd->rOrigDest.left;
            oy = bd->rOrigSrc.top - bd->rOrigDest.top;
        }
    } else {
        if( copy && (bd->rSrc.right - bd->rSrc.left != bd->rDest.right - bd->rDest.left ||
                     bd->rSrc.bottom - bd->rSrc.top != bd->rDest.bottom - bd->rDest.top) ) {
            return( DDHAL_DRIVER_NOTHANDLED );
        }
        SetRect( &one, bd->rDest.left, bd->rDest.top, bd->rDest.right, bd->rDest.bottom );
        rects = &one;
        count = 1;
        ox = bd->rSrc.left - bd->rDest.left;
        oy = bd->rSrc.top - bd->rDest.top;
    }
    hot[H_BUSY] = 1;
    ok = m8dd_plain( copy ? M8_MIX_VRAM_REPLACE : M8_MIX_FRGD_REPLACE );
    if( ok && !copy ) {
        outpw( M8_FRGD_COLOR, (WORD)(bd->bltFX.dwFillColor & 0xFF) );
    }
    for( i = 0; ok && i < count; ++i ) {
        ok = m8dd_rect( &rects[i], copy, rects[i].left + ox, rects[i].top + oy );
    }
    m8dd_forget( hot );
    hot[H_BUSY] = 0;
    bd->ddRVal = ok ? DD_OK : M8DD_DDERR_GENERIC;
    return( DDHAL_DRIVER_HANDLED );
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
    m8dd->marker = M8DD_MARKER;
    return( 1 );
}
