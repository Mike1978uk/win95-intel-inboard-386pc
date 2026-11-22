/* DirectDraw: the 16-bit half of the hand-over (docs/mach8_directdraw_plan.md).
 *
 * Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
 * The DCICOMMAND sequence follows JHRobotics' vmdisp9x dddrv.c and control.c
 * (MIT, see LICENSE-MIT.txt); ddk/ddrawi.h and ddk/dmemmgr.h come from its ddk/.
 *
 * DDRAW asks through Control(DCICOMMAND) with dwVersion DD_VERSION: the HAL
 * version, its own SetInfo entry, the 32-bit HAL's name and context, then the
 * DDHALINFO. The HAL itself is M8DD32.DLL: DriverInit gets the linear address
 * of the block below and writes its callbacks there; this side copies them
 * into the tables DDRAW reads. Other DCI requests are refused: the DIB
 * Engine's own answer would offer a frame buffer the 8514 half does not have.
 */

#include "m8ddi.h"
#include <string.h>
#include <dos.h>
#include "m8.h"
#include "ddrawi.h"

extern LPDIBENGINE m8_pdevice;

/* DCICOMMAND's DirectDraw requests (vmdisp9x control.c). */
#define DDCREATEDRIVEROBJECT    10
#define DDGET32BITDRIVERNAME    11
#define DDNEWCALLBACKFNS        12
#define DDVERSIONINFO           13

/* ddraw.h */
#define DDPCAPS_8BIT            0x00000004UL
#define DDPCAPS_PRIMARYSURFACE  0x00000010UL

#define M8DD_MARKER     0x44443855UL    /* 'U8DD': DriverInit has run */

/* Shared with M8DD32.DLL, which declares the head up to `blt` (m8dd32.c). */
#pragma pack( push, 1 )
typedef struct {
    DWORD   dwSize;
    DWORD   marker;
    DWORD   hInstance;                  /* M8DD32's module */
    DWORD   hot;                        /* linear address of m8_hot */
    DWORD   width;
    DWORD   height;
    DWORD   pitch;
    DWORD   destroy_driver;             /* 32-bit callbacks, 0 = none */
    DWORD   create_surface;
    DWORD   can_create_surface;
    DWORD   wait_vblank;
    DWORD   destroy_surface;
    DWORD   flip;
    DWORD   lock;
    DWORD   unlock;
    DWORD   blt;
    DWORD   shadow;                     /* linear address of the primary's Lock shadow */
    DWORD   heap;                       /* linear address of the heap rows in the shadow */
    DWORD   heap_top;                   /* its first card row */
    DWORD   heap_rows;
    DWORD   fb16;                       /* 1: the primary is fb16.c's 5-6-5 frame buffer */
    DWORD   fb_lin;                     /* its linear address */
    DWORD   gdi_lin;                    /* linear address of fb16.c's count of GDI calls */
    DWORD   d3d_global;                 /* m8d3d.c's two tables, 0 = no Direct3D */
    DWORD   d3d_callbacks;
    DWORD   d3d_info;                   /* its GetDriverInfo */
    DWORD   scratch_top;                /* at 16 bpp, the off-screen rows m8d3d.c fills from */
    DWORD   scratch_rows;
    DWORD   update_overlay;             /* 32-bit callbacks for the overlay, 0 = none */
    DWORD   set_overlay_position;
    M8_OV   ov;                         /* the overlay as M8DD32 last described it */
    DWORD   vga;                        /* 1: m8_vga; M8DD32's m8dd_vga_* serve the VGA half */
    DWORD   vga_sent;                   /* linear: what A0000 holds, as M8DD32's Flip sent it */
    DWORD   vga_sent_ok;                /* 0: vga_sent is not what A0000 holds */
    /* This side only from here. */
    DDHALINFO_t                 info;
    DDHAL_DDCALLBACKS_t         cb;
    DDHAL_DDSURFACECALLBACKS_t  scb;
    DDHAL_DDPALETTECALLBACKS_t  pcb;
    DDHALMODEINFO_t             modes[M8_MODE_COUNT + 3];
} M8DD_SHARE;
#pragma pack( pop )

/* a * b to 32 bits by one MUL: the runtime's 32-bit multiply is in _TEXT,
 * which this file's segment cannot call near. */
static DWORD m8dd_mul( WORD a, WORD b );
#pragma aux m8dd_mul = "mul dx" parm [ax] [dx] value [dx ax];

/* n / d by one DIV, for the same reason; the quotient must fit in 16 bits. */
static WORD m8dd_div( DWORD n, WORD d );
#pragma aux m8dd_div = "div bx" parm [dx ax] [bx] value [ax] modify [dx];

/* n bytes from sel:off (32-bit offset) to dst. */
static void m8dd_copy32( WORD sel, DWORD off, BYTE FAR *dst, WORD n );
#pragma aux m8dd_copy32 = \
    ".386" \
    "push ds" \
    "mov ds, dx" \
    "shl ecx, 16" \
    "mov cx, bx" \
    "mov esi, ecx" \
    "movzx ecx, ax" \
    "movzx edi, di" \
    "cld" \
    "db 67h, 0F3h, 0A4h" \
    "pop ds" \
    parm [dx] [cx bx] [es di] [ax] modify [cx si di];

#define M8_OV_MAXW      800             /* fb16.c's widest row */

/* DirectDraw's overlay at 16 bpp (m8.h M8_OV). It is never written into the
 * frame buffer, so GDI's drawing under it is kept and hiding or moving it is
 * a resend. */
M8_OV           m8_ov;
static WORD     m8_ov_sel;
static WORD     m8_ov_buf[M8_OV_MAXW];

static M8DD_SHARE FAR   *m8dd;
static DWORD            m8dd_lin;
static BYTE FAR         *m8dd_shadow;   /* the primary as DDRAW sees it */
static DWORD            m8dd_shadow_lin;
static DWORD            m8dd_shadow_size;
static HGLOBAL          m8dd_shadow_mem;
static LPDDHAL_SETINFO  m8dd_setinfo;

static const DD32BITDRIVERDATA_t m8dd_hal = { "M8DD32.DLL", "DriverInit", 0 };

/* The block, once: it stays for the session, as the 32-bit HAL holds its
 * address. */
static int m8dd_alloc( void )
{
    HGLOBAL h;

    if( m8dd != NULL ) {
        return( 1 );
    }
    h = GlobalAlloc( GMEM_FIXED | GMEM_SHARE | GMEM_ZEROINIT, sizeof( M8DD_SHARE ) );
    if( h == 0 ) {
        return( 0 );
    }
    m8dd = (M8DD_SHARE FAR *)GlobalLock( h );
    m8dd_lin = GetSelectorBase( FP_SEG( m8dd ) ) + FP_OFF( m8dd );
    m8dd->dwSize = sizeof( M8DD_SHARE );
    m8dd->hot = GetSelectorBase( FP_SEG( (LPVOID)&m8_hot ) ) + FP_OFF( (LPVOID)&m8_hot );
    return( 1 );
}

/* The Lock shadow: the card's rows from the top of the screen to the end of
 * the off-screen heap, at the screen's pitch, as a real card's frame buffer
 * would be laid out - the primary at its start, the heap (m8_dd_heap's rows)
 * further on - so an address in it gives the card position. Without memory
 * for the heap, the screen alone. One for a smaller mode is not freed: a
 * DirectDraw object made then may still hold its address. 0 = no memory: no
 * HAL, rather than a primary at linear 0. It is fixed memory, page-locked, so
 * a 16 bpp desktop frees it on the way back (m8dd_shadow_free). */
static int m8dd_shadow_alloc( void )
{
    int     top;
    int     rows;
    int     pass;
    DWORD   size;
    HGLOBAL h;

    m8_dd_heap( &top, &rows );
    for( pass = 0; pass < 2; ++pass ) {
        if( pass == 1 ) {
            rows = 0;
        }
        size = m8dd_mul( M8_SCREEN_X, rows > 0 ? top + rows : M8_SCREEN_Y );
        /* A mode with room for a second page has M8DD32's 8 bpp back buffer there: its rows too. */
        if( pass == 0 && M8_SCREEN_Y <= M8_PAGE1_Y && size < m8dd_mul( M8_SCREEN_X, M8_PAGE1_Y + M8_SCREEN_Y ) ) {
            size = m8dd_mul( M8_SCREEN_X, M8_PAGE1_Y + M8_SCREEN_Y );
        }
        if( m8dd_shadow != NULL && m8dd_shadow_size >= size ) {
            break;
        }
        h = GlobalAlloc( GMEM_FIXED | GMEM_SHARE | GMEM_ZEROINIT, size );
        if( h != 0 ) {
            m8dd_shadow_mem = h;
            m8dd_shadow = (BYTE FAR *)GlobalLock( h );
            m8dd_shadow_lin = GetSelectorBase( FP_SEG( m8dd_shadow ) );
            m8dd_shadow_size = size;
            break;
        }
    }
    if( pass == 2 ) {
        return( 0 );
    }
    m8dd->shadow = m8dd_shadow_lin;
    m8dd->heap_top = rows > 0 ? top : 0;
    m8dd->heap_rows = rows;
    m8dd->heap = rows > 0 ? m8dd_shadow_lin + m8dd_mul( M8_SCREEN_X, top ) : 0;
    return( 1 );
}

/* 320x200x8 on the VGA half: mode 13h, linear at A0000, one 64 KB page. */
static void m8dd_vga_row( DDHALMODEINFO_t FAR *m )
{
    _fmemset( m, 0, sizeof( *m ) );
    m->dwWidth = 320;
    m->dwHeight = 200;
    m->lPitch = 320;
    m->dwBPP = 8;
    m->wFlags = DDMODEINFO_PALETTIZED;
}

/* The VGA half's primary is the system VM's own A0000, as KERNEL's __A000H
 * selector addresses it: the VDD maps it to the card while Windows owns the
 * screen, and DirectDraw's Win32 callers run in that VM. Not a DPMI 0800h
 * mapping: that call does not map memory below 1 MB. A program that draws
 * there itself pays the bus once a byte; a copy in RAM would add its stores. */
#define M8DD_VGA_PRIMARY    0xA0000UL

/* What A0000 holds, for M8DD32's Flip to send only what differs. Fixed shared
 * memory, as for m8dd_shadow, so the address holds in every process; kept for
 * the session once made. */
static BYTE FAR         *m8dd_vga_was;

static int m8dd_vga_alloc( void )
{
    HGLOBAL h;

    if( m8dd_vga_was == NULL ) {
        h = GlobalAlloc( GMEM_FIXED | GMEM_SHARE | GMEM_ZEROINIT, 320U * 200U );
        if( h == 0 ) {
            return( 0 );
        }
        m8dd_vga_was = (BYTE FAR *)GlobalLock( h );
    }
    m8dd->vga_sent = GetSelectorBase( FP_SEG( m8dd_vga_was ) );
    return( 1 );
}

/* Mode 13h was just set, which clears A0000: the record no longer holds. */
void __far m8dd_vga_clear( void )
{
    if( m8dd != NULL ) {
        m8dd->vga_sent_ok = 0;
    }
}

static int m8dd_mode_index( void )
{
    int i;
    int x;
    int y;

    if( m8_vga ) {
        return( M8_MODE_COUNT );
    }
    for( i = 0; i < M8_MODE_COUNT; ++i ) {
        if( m8_hw_mode_size( i, &x, &y ) && x == M8_SCREEN_X && y == M8_SCREEN_Y ) {
            return( i );
        }
    }
    return( -1 );
}

static void m8dd_build( int mode )
{
    DDHALINFO_t FAR *hi = &m8dd->info;
    int             i;
    int             x;
    int             y;

    _fmemset( hi, 0, sizeof( *hi ) );
    for( i = 0; i < M8_MODE_COUNT; ++i ) {
        DDHALMODEINFO_t FAR *m = &m8dd->modes[i];

        _fmemset( m, 0, sizeof( *m ) );
        m8_hw_mode_size( i, &x, &y );
        m->dwWidth = x;
        m->dwHeight = y;
        m->lPitch = x;
        m->dwBPP = 8;
        m->wFlags = DDMODEINFO_PALETTIZED;
    }
    m8dd_vga_row( &m8dd->modes[M8_MODE_COUNT] );
    m8dd->width = M8_SCREEN_X;
    m8dd->height = M8_SCREEN_Y;
    m8dd->pitch = M8_SCREEN_X;
    m8dd->fb16 = 0;
    m8dd->fb_lin = 0;
    m8dd->vga = m8_vga;

    m8dd->cb.dwSize = sizeof( m8dd->cb );
    m8dd->cb.dwFlags = 0;
    m8dd->scb.dwSize = sizeof( m8dd->scb );
    m8dd->scb.dwFlags = 0;
    m8dd->pcb.dwSize = sizeof( m8dd->pcb );
    m8dd->pcb.dwFlags = 0;
    if( m8dd->destroy_driver ) {
        m8dd->cb.DestroyDriver = (LPVOID)m8dd->destroy_driver;
        m8dd->cb.dwFlags |= DDHAL_CB32_DESTROYDRIVER;
    }
    if( m8dd->create_surface ) {
        m8dd->cb.CreateSurface = (LPVOID)m8dd->create_surface;
        m8dd->cb.dwFlags |= DDHAL_CB32_CREATESURFACE;
    }
    if( m8dd->can_create_surface ) {
        m8dd->cb.CanCreateSurface = (LPVOID)m8dd->can_create_surface;
        m8dd->cb.dwFlags |= DDHAL_CB32_CANCREATESURFACE;
    }
    if( m8dd->wait_vblank ) {
        m8dd->cb.WaitForVerticalBlank = (LPVOID)m8dd->wait_vblank;
        m8dd->cb.dwFlags |= DDHAL_CB32_WAITFORVERTICALBLANK;
    }
    if( m8dd->update_overlay && m8dd->set_overlay_position ) {
        m8dd->scb.UpdateOverlay = (LPVOID)m8dd->update_overlay;
        m8dd->scb.SetOverlayPosition = (LPVOID)m8dd->set_overlay_position;
        m8dd->scb.dwFlags |= DDHAL_SURFCB32_UPDATEOVERLAY | DDHAL_SURFCB32_SETOVERLAYPOSITION;
    }
    if( m8dd->destroy_surface ) {
        m8dd->scb.DestroySurface = (LPVOID)m8dd->destroy_surface;
        m8dd->scb.dwFlags |= DDHAL_SURFCB32_DESTROYSURFACE;
    }
    if( m8dd->flip ) {
        m8dd->scb.Flip = (LPVOID)m8dd->flip;
        m8dd->scb.dwFlags |= DDHAL_SURFCB32_FLIP;
    }
    if( m8dd->lock ) {
        m8dd->scb.Lock = (LPVOID)m8dd->lock;
        m8dd->scb.dwFlags |= DDHAL_SURFCB32_LOCK;
    }
    if( m8dd->unlock ) {
        m8dd->scb.Unlock = (LPVOID)m8dd->unlock;
        m8dd->scb.dwFlags |= DDHAL_SURFCB32_UNLOCK;
    }
    if( m8dd->blt ) {
        m8dd->scb.Blt = (LPVOID)m8dd->blt;
        m8dd->scb.dwFlags |= DDHAL_SURFCB32_BLT;
    }

    hi->dwSize = sizeof( *hi );
    hi->lpDDCallbacks = &m8dd->cb;
    hi->lpDDSurfaceCallbacks = &m8dd->scb;
    hi->lpDDPaletteCallbacks = &m8dd->pcb;
    hi->vmiData.fpPrimary = m8dd_shadow_lin;
    hi->vmiData.dwDisplayWidth = M8_SCREEN_X;
    hi->vmiData.dwDisplayHeight = M8_SCREEN_Y;
    hi->vmiData.lDisplayPitch = M8_SCREEN_X;
    hi->vmiData.ddpfDisplay.dwSize = sizeof( hi->vmiData.ddpfDisplay );
    hi->vmiData.ddpfDisplay.dwFlags = DDPF_RGB | DDPF_PALETTEINDEXED8;
    hi->vmiData.ddpfDisplay.dwRGBBitCount = 8;
    /* No heap for DDRAW to manage: M8DD32's CreateSurface places off-screen
     * surfaces in the shadow's heap rows itself. */
    hi->vmiData.dwNumHeaps = 0;
    hi->ddCaps.dwSize = sizeof( hi->ddCaps );
    hi->ddCaps.dwCaps = DDCAPS_GDI;
    hi->ddCaps.dwFXCaps = 0;
    hi->ddCaps.dwMaxVisibleOverlays = 0;
    if( m8dd->blt ) {
        /* M8DD32's Blt: colour fills and SRCCOPY between card surfaces. */
        hi->ddCaps.dwCaps |= DDCAPS_BLT | DDCAPS_BLTCOLORFILL;
        hi->ddCaps.dwRops[0xCC / 32] |= 1UL << (0xCC % 32);
        /* And SRCCOPY from system memory to the card, as host data. */
        hi->ddCaps.dwCaps |= DDCAPS_CANBLTSYSMEM;
        /* Source colour keys between card surfaces: the engine's compare. */
        hi->ddCaps.dwCaps |= DDCAPS_COLORKEY | DDCAPS_COLORKEYHWASSIST;
        hi->ddCaps.dwCKeyCaps = DDCKEYCAPS_SRCBLT;
        hi->ddCaps.dwSVBCaps = DDCAPS_BLT;
        hi->ddCaps.dwSVBRops[0xCC / 32] |= 1UL << (0xCC % 32);
    }
    hi->ddCaps.dwPalCaps = DDPCAPS_8BIT | DDPCAPS_PRIMARYSURFACE;
    hi->ddCaps.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_PALETTE;
    /* M8DD32's Flip: the back buffer is the card's second page, shown by CRT_OFFSET, so only a
     * mode of M8_PAGE1_Y rows or fewer has one. */
    if( m8dd->flip && M8_SCREEN_Y <= M8_PAGE1_Y ) {
        hi->ddCaps.ddsCaps.dwCaps |= DDSCAPS_FLIP | DDSCAPS_BACKBUFFER | DDSCAPS_COMPLEX;
    }
    if( m8dd->d3d_global ) {
        /* M8DD32's Direct3D HAL (m8d3d.c): flat shading at 8 bpp. */
        hi->lpD3DGlobalDriverData = (ULONG_PTR)m8dd->d3d_global;
        hi->lpD3DHALCallbacks = (ULONG_PTR)m8dd->d3d_callbacks;
        hi->ddCaps.dwCaps |= DDCAPS_3D;
        hi->ddCaps.ddsCaps.dwCaps |= DDSCAPS_3DDEVICE;
    }
    /* Not DDSCAPS_OFFSCREENPLAIN: with it DDRAW refuses this HAL outright.
     * Off-screen surfaces in video memory are still asked of CreateSurface. */
    hi->dwModeIndex = mode;
    hi->dwNumModes = M8_MODE_COUNT + 1;
    if( m8_fb16_lin() != 0 ) {
        /* A 16 bpp desktop running a full-screen 8 bpp program: its modes, so DDRAW can go back. */
        DDHALMODEINFO_t FAR *m = &m8dd->modes[M8_MODE_COUNT + 1];

        for( i = 0; i < 2; ++i, ++m ) {
            _fmemset( m, 0, sizeof( *m ) );
            m->dwWidth = i == 0 ? 640 : 800;
            m->dwHeight = i == 0 ? 480 : 600;
            m->lPitch = m->dwWidth * 2;
            m->dwBPP = 16;
            m->dwRBitMask = 0xF800UL;
            m->dwGBitMask = 0x07E0UL;
            m->dwBBitMask = 0x001FUL;
        }
        hi->dwNumModes = M8_MODE_COUNT + 3;
    }
    hi->lpModeInfo = m8dd->modes;
    hi->dwFlags = DDHALINFO_ISPRIMARYDISPLAY | DDHALINFO_MODEXILLEGAL;
    if( m8dd->d3d_global ) {
        hi->GetDriverInfo = (LPDDHAL_GETDRIVERINFO)m8dd->d3d_info;
        hi->dwFlags |= DDHALINFO_GETDRIVERINFOSET;
    }
    hi->lpPDevice = m8_pdevice;
    hi->hInstance = m8dd->hInstance;
    if( m8_vga ) {
        /* The primary is A0000: Lock is DDRAW's, M8DD32 does Blt, Flip and flip
         * chains (m8dd->vga).
         * The callbacks and caps stay as published: a reset that drops them is
         * refused by SetInfo, and DDRAW keeps the old mode's primary. */
        hi->vmiData.fpPrimary = M8DD_VGA_PRIMARY;
        hi->vmiData.lDisplayPitch = 320;
    }
}

/* At 16 bpp the primary is fb16.c's frame buffer in RAM: DirectDraw locks
 * it in place, and M8DD32 sends what it changed through M8_ESC_FB16SEND, as
 * GDI's drawing goes. One mode, the one running: the colour depth and, at 16
 * bpp, the size change only at a restart (ReEnable). Off-screen surfaces stay
 * in system memory, the card's 256 colours not being this format; textures are
 * the exception, kept on the card at 8 bpp (M8DD32 m8dd_tex_create). */
static void m8dd_build16( void )
{
    DDHALINFO_t FAR     *hi = &m8dd->info;
    DDHALMODEINFO_t FAR *m = &m8dd->modes[0];
    int                 rows_ok;
    int                 i;

    m8dd_build( 0 );
    /* The two 16 bpp sizes: a change between them needs no restart. */
    for( i = 0; i < 2; ++i, ++m ) {
        _fmemset( m, 0, sizeof( *m ) );
        m->dwWidth = i == 0 ? 640 : 800;
        m->dwHeight = i == 0 ? 480 : 600;
        m->lPitch = m->dwWidth * 2;
        m->dwBPP = 16;
        m->dwRBitMask = 0xF800UL;
        m->dwGBitMask = 0x07E0UL;
        m->dwBBitMask = 0x001FUL;
    }
    m8dd->fb16 = 1;
    m8dd->fb_lin = m8_fb16_lin();
    {
        int top;
        int rows;

        m8_dd_heap( &top, &rows );
        m8dd->scratch_top = top;
        m8dd->scratch_rows = rows;
        rows_ok = rows >= 16;
    }
    m8dd->gdi_lin = GetSelectorBase( FP_SEG( (LPVOID)m8_fb16_stat ) ) + FP_OFF( (LPVOID)m8_fb16_stat );
    m8dd->pitch = M8_SCREEN_X * 2;
    m8dd->shadow = 0;
    m8dd->heap = 0;
    m8dd->heap_top = 0;
    m8dd->heap_rows = 0;
    hi->vmiData.fpPrimary = m8dd->fb_lin;
    hi->vmiData.lDisplayPitch = M8_SCREEN_X * 2;
    hi->vmiData.ddpfDisplay.dwFlags = DDPF_RGB;
    hi->vmiData.ddpfDisplay.dwRGBBitCount = 16;
    hi->vmiData.ddpfDisplay.dwRBitMask = 0xF800UL;
    hi->vmiData.ddpfDisplay.dwGBitMask = 0x07E0UL;
    hi->vmiData.ddpfDisplay.dwBBitMask = 0x001FUL;
    hi->ddCaps.dwCaps = DDCAPS_GDI;
    hi->ddCaps.dwCKeyCaps = 0;
    _fmemset( hi->ddCaps.dwRops, 0, sizeof( hi->ddCaps.dwRops ) );
    _fmemset( hi->ddCaps.dwSVBRops, 0, sizeof( hi->ddCaps.dwSVBRops ) );
    hi->ddCaps.dwSVBCaps = 0;
    if( m8dd->blt ) {
        /* M8DD32's Blt: fills and copies in RAM, then the changed part sent. */
        hi->ddCaps.dwCaps |= DDCAPS_BLT | DDCAPS_BLTCOLORFILL | DDCAPS_CANBLTSYSMEM |
                             DDCAPS_COLORKEY | DDCAPS_COLORKEYHWASSIST;
        hi->ddCaps.dwRops[0xCC / 32] |= 1UL << (0xCC % 32);
        hi->ddCaps.dwSVBCaps = DDCAPS_BLT | DDCAPS_COLORKEY | DDCAPS_COLORKEYHWASSIST;
        hi->ddCaps.dwSVBRops[0xCC / 32] |= 1UL << (0xCC % 32);
        hi->ddCaps.dwSVBCKeyCaps = DDCKEYCAPS_SRCBLT;
        hi->ddCaps.dwCKeyCaps = DDCKEYCAPS_SRCBLT;
    }
    hi->ddCaps.dwPalCaps = 0;
    hi->ddCaps.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    hi->ddCaps.dwFXCaps = 0;
    hi->ddCaps.dwMaxVisibleOverlays = 0;
    if( m8dd->update_overlay ) {
        /* One overlay, any size, source-keyed: fb16.c merges it into what it sends. */
        hi->ddCaps.dwCaps |= DDCAPS_OVERLAY | DDCAPS_OVERLAYSTRETCH | DDCAPS_COLORKEY;
        hi->ddCaps.dwCKeyCaps |= DDCKEYCAPS_SRCOVERLAY;
        hi->ddCaps.dwFXCaps = DDFXCAPS_OVERLAYSTRETCHX | DDFXCAPS_OVERLAYSTRETCHY |
                              DDFXCAPS_OVERLAYSHRINKX | DDFXCAPS_OVERLAYSHRINKY;
        hi->ddCaps.dwMaxVisibleOverlays = 1;
        hi->ddCaps.dwCurrVisibleOverlays = 0;
        hi->ddCaps.dwMinOverlayStretch = 100;
        hi->ddCaps.dwMaxOverlayStretch = 20000;
        hi->ddCaps.ddsCaps.dwCaps |= DDSCAPS_OVERLAY;
        /* As textures: an alignment of 0 for a claimed surface type and DDRAW refuses the HAL. */
        hi->vmiData.dwOverlayAlign = 8;
    }
    if( m8dd->flip ) {
        /* M8DD32's Flip: a back buffer in a second frame buffer in RAM. */
        hi->ddCaps.ddsCaps.dwCaps |= DDSCAPS_FLIP | DDSCAPS_BACKBUFFER | DDSCAPS_COMPLEX;
        /* Direct3D draws through the flip chain: triangles in a buffer's description. */
        if( m8dd->d3d_global && rows_ok ) {
            hi->ddCaps.dwCaps |= DDCAPS_3D;
            /* Textures in video memory: on the card, beside page 0 (m8dd_tex_create). DDRAW
             * refuses the whole HAL if a claimed surface type has an alignment of 0 or odd. */
            hi->ddCaps.ddsCaps.dwCaps |= DDSCAPS_3DDEVICE | DDSCAPS_TEXTURE;
            hi->vmiData.dwTextureAlign = 8;
        }
    }
    if( !(hi->ddCaps.dwCaps & DDCAPS_3D) ) {
        hi->lpD3DGlobalDriverData = 0;
        hi->lpD3DHALCallbacks = 0;
        hi->GetDriverInfo = 0;
        hi->dwFlags &= ~DDHALINFO_GETDRIVERINFOSET;
    }
    /* And the 8 bpp modes: a full-screen program's SetDisplayMode reaches ReEnable. */
    for( i = 0; i < M8_MODE_COUNT; ++i, ++m ) {
        int x;
        int y;

        _fmemset( m, 0, sizeof( *m ) );
        m8_hw_mode_size( i, &x, &y );
        m->dwWidth = x;
        m->dwHeight = y;
        m->lPitch = x;
        m->dwBPP = 8;
        m->wFlags = DDMODEINFO_PALETTIZED;
    }
    m8dd_vga_row( m );
    hi->dwModeIndex = M8_SCREEN_X == 640 ? 0 : 1;
    hi->dwNumModes = 3 + M8_MODE_COUNT;
}

/* Back on a 16 bpp desktop the primary is fb16's and DDRAW has been reset to it,
 * so nothing holds the 8 bpp shadow; kept, it pins ~620 KB of a 5 MB machine. */
static void m8dd_shadow_free( void )
{
    if( m8dd_shadow_mem != 0 ) {
        GlobalUnlock( m8dd_shadow_mem );
        GlobalFree( m8dd_shadow_mem );
        m8dd_shadow_mem = 0;
        m8dd_shadow = NULL;
        m8dd_shadow_lin = 0;
        m8dd_shadow_size = 0;
    }
}

/* ReEnable changed the size at 16 bpp: the description is built again for
 * the new size and handed to DDRAW with bReset set, as the DDK asks after a
 * mode change; M8DD32 clips and addresses by the block's width and pitch. */
/* M8DD32's new description (batch op 6), with a selector over the shown buffer. */
void __far m8_ov_take( void )
{
    if( m8dd == NULL || m8dd->marker != M8DD_MARKER ) {
        m8_ov.on = 0;
        return;
    }
    m8_ov = m8dd->ov;
    if( !m8_ov.on ) {
        return;
    }
    if( m8_ov_sel == 0 ) {
        m8_ov_sel = AllocSelector( FP_SEG( (LPVOID)&m8_ov ) );
    }
    if( m8_ov_sel == 0 || m8_ov.lin == 0 || m8_ov.pitch > 0xFFFFUL || m8_ov.sb <= 0 ||
        m8_ov.sb > 0xFFFFL ) {
        m8_ov.on = 0;
        return;
    }
    SetSelectorBase( m8_ov_sel, m8_ov.lin );
    SetSelectorLimit( m8_ov_sel, m8dd_mul( (WORD)m8_ov.pitch, (WORD)m8_ov.sb ) - 1 );
}

/* Row y of src (columns l .. r) with the overlay's pixels over it: the
 * nearest source pixel, stretched or shrunk to the destination, stepped
 * without a division per pixel. */
void __far m8_ov_merge( WORD FAR *src, int y, int l, int r )
{
    long    dwl = m8_ov.dr - m8_ov.dl;
    long    dhl = m8_ov.db - m8_ov.dt;
    long    swl = m8_ov.sr - m8_ov.sl;
    long    shl = m8_ov.sb - m8_ov.st;
    int     x0 = l > (int)m8_ov.dl ? l : (int)m8_ov.dl;
    int     x1 = r < (int)m8_ov.dr ? r : (int)m8_ov.dr;
    int     keyed = m8_ov.key <= 0xFFFFUL;
    WORD    key = (WORD)m8_ov.key;
    WORD    dw;
    WORD    sw;
    WORD    sy;
    WORD    i;
    WORD    n;
    WORD    rem;
    WORD    k;
    DWORD   num;
    int     x;
    WORD    p;

    if( y < (int)m8_ov.dt || y >= (int)m8_ov.db || x1 <= x0 || dwl <= 0 || dhl <= 0 ||
        swl <= 0 || shl <= 0 || dwl > M8_OV_MAXW * 4 || swl > M8_OV_MAXW * 4 || dhl > 0x7FFF ||
        shl > 0x7FFF ) {
        return;
    }
    dw = (WORD)dwl;
    sw = (WORD)swl;
    sy = (WORD)m8_ov.st + m8dd_div( m8dd_mul( (WORD)(y - (int)m8_ov.dt), (WORD)shl ), (WORD)dhl );
    num = m8dd_mul( (WORD)(x0 - (int)m8_ov.dl), sw );
    i = m8dd_div( num, dw );
    rem = (WORD)(num - m8dd_mul( i, dw ));
    n = m8dd_div( m8dd_mul( (WORD)(x1 - 1 - (int)m8_ov.dl), sw ), dw ) - i + 1;
    if( n > M8_OV_MAXW ) {
        return;
    }
    m8dd_copy32( m8_ov_sel, m8dd_mul( sy, (WORD)m8_ov.pitch ) + m8dd_mul( (WORD)m8_ov.sl + i, 2 ),
                 (BYTE FAR *)m8_ov_buf, n * 2 );
    k = 0;
    for( x = x0; x < x1; ++x ) {
        p = m8_ov_buf[k];
        if( !keyed || p != key ) {
            src[x - l] = p;
        }
        rem += sw;
        while( rem >= dw ) {
            rem -= dw;
            ++k;
        }
    }
}

void __far m8dd_mode_changed( void )
{
    int mode;

    if( m8dd == NULL || m8dd->marker != M8DD_MARKER ) {
        return;
    }
    if( m8_fb16 ) {
        m8dd_build16();
    } else {
        /* A full-screen program's own mode: DirectDraw must see its size, pitch and pages, not
         * the desktop's. Only once DirectDraw has been set up (the shadow exists, or a 16 bpp
         * desktop went to 8 bpp for it): a desktop change alone should not pin a shadow in RAM. */
        mode = m8dd_mode_index();
        if( (m8dd_shadow == NULL && m8_fb16_lin() == 0) || mode < 0 ||
            (m8_vga ? !m8dd_vga_alloc() : !m8dd_shadow_alloc()) ) {
            return;
        }
        m8dd_build( mode );
    }
    if( m8dd_setinfo != NULL ) {
        m8dd_setinfo( &m8dd->info, 1 );
    }
    if( m8_fb16 ) {
        m8dd_shadow_free();
    }
}

/* Control(DCICOMMAND): 1 = answered, 0 = refused. */
LONG __far m8dd_command( LPVOID lpInput, LPVOID lpOutput )
{
    DCICMD_t FAR    *c = lpInput;
    int             mode;

    if( c == NULL || c->dwVersion != DD_VERSION || !m8dd_alloc() ) {
        return( 0 );
    }
    switch( c->dwCommand ) {
    case DDVERSIONINFO:
        _fmemset( lpOutput, 0, sizeof( DDVERSIONDATA_t ) );
        ((DDVERSIONDATA_t FAR *)lpOutput)->dwHALVersion = DD_RUNTIME_VERSION;
        return( 1 );
    case DDNEWCALLBACKFNS:
        m8dd_setinfo = ((LPDDHALDDRAWFNS)c->dwParam1)->lpSetInfo;
        return( 1 );
    case DDGET32BITDRIVERNAME:
        _fmemcpy( lpOutput, &m8dd_hal, sizeof( m8dd_hal ) );
        ((DD32BITDRIVERDATA_t FAR *)lpOutput)->dwContext = m8dd_lin;
        return( 1 );
    case DDCREATEDRIVEROBJECT:
        if( m8_fb16 ) {
            if( m8dd_setinfo == NULL || m8dd->marker != M8DD_MARKER || m8_fb16_lin() == 0 ) {
                return( 0 );
            }
            m8dd_build16();
        } else {
            mode = m8dd_mode_index();
            if( m8dd_setinfo == NULL || mode < 0 || m8dd->marker != M8DD_MARKER ||
                (m8_vga ? !m8dd_vga_alloc() : !m8dd_shadow_alloc()) ) {
                return( 0 );
            }
            m8dd_build( mode );
        }
        if( !m8dd_setinfo( &m8dd->info, 0 ) ) {
            return( 0 );
        }
        *(DWORD FAR *)lpOutput = m8dd->hInstance;
        return( 1 );
    }
    return( 0 );
}
