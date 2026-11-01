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
    /* This side only from here. */
    DDHALINFO_t                 info;
    DDHAL_DDCALLBACKS_t         cb;
    DDHAL_DDSURFACECALLBACKS_t  scb;
    DDHAL_DDPALETTECALLBACKS_t  pcb;
    DDHALMODEINFO_t             modes[M8_MODE_COUNT];
} M8DD_SHARE;
#pragma pack( pop )

static M8DD_SHARE FAR   *m8dd;
static DWORD            m8dd_lin;
static BYTE FAR         *m8dd_shadow;   /* the primary as DDRAW sees it */
static DWORD            m8dd_shadow_lin;
static DWORD            m8dd_shadow_size;
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

/* The primary's Lock shadow, the size of this mode's screen. One for a
 * smaller mode is not freed: a DirectDraw object made then may still hold
 * its address. 0 = no memory: no HAL, rather than a primary at linear 0. */
static int m8dd_shadow_alloc( void )
{
    DWORD   size = (DWORD)M8_SCREEN_X * M8_SCREEN_Y;
    HGLOBAL h;

    if( m8dd_shadow != NULL && m8dd_shadow_size >= size ) {
        return( 1 );
    }
    h = GlobalAlloc( GMEM_FIXED | GMEM_SHARE | GMEM_ZEROINIT, size );
    if( h == 0 ) {
        return( 0 );
    }
    m8dd_shadow = (BYTE FAR *)GlobalLock( h );
    m8dd_shadow_lin = GetSelectorBase( FP_SEG( m8dd_shadow ) );
    m8dd_shadow_size = size;
    m8dd->shadow = m8dd_shadow_lin;
    return( 1 );
}

static int m8dd_mode_index( void )
{
    int i;
    int x;
    int y;

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
    m8dd->width = M8_SCREEN_X;
    m8dd->height = M8_SCREEN_Y;
    m8dd->pitch = M8_SCREEN_X;

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
    hi->vmiData.dwNumHeaps = 0;
    hi->ddCaps.dwSize = sizeof( hi->ddCaps );
    hi->ddCaps.dwCaps = DDCAPS_GDI;
    if( m8dd->blt ) {
        /* M8DD32's Blt: colour fills and SRCCOPY within the primary. */
        hi->ddCaps.dwCaps |= DDCAPS_BLT | DDCAPS_BLTCOLORFILL;
        hi->ddCaps.dwRops[0xCC / 32] |= 1UL << (0xCC % 32);
        /* And SRCCOPY from system memory to the primary, as host data. */
        hi->ddCaps.dwCaps |= DDCAPS_CANBLTSYSMEM;
        hi->ddCaps.dwSVBCaps = DDCAPS_BLT;
        hi->ddCaps.dwSVBRops[0xCC / 32] |= 1UL << (0xCC % 32);
    }
    hi->ddCaps.dwPalCaps = DDPCAPS_8BIT | DDPCAPS_PRIMARYSURFACE;
    hi->ddCaps.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_PALETTE;
    hi->dwModeIndex = mode;
    hi->dwNumModes = M8_MODE_COUNT;
    hi->lpModeInfo = m8dd->modes;
    hi->dwFlags = DDHALINFO_ISPRIMARYDISPLAY | DDHALINFO_MODEXILLEGAL;
    hi->lpPDevice = m8_pdevice;
    hi->hInstance = m8dd->hInstance;
}

/* Control(DCICOMMAND): 1 = answered, 0 = refused. */
LONG m8dd_command( LPVOID lpInput, LPVOID lpOutput )
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
        mode = m8dd_mode_index();
        if( m8dd_setinfo == NULL || mode < 0 || m8dd->marker != M8DD_MARKER ||
            !m8dd_shadow_alloc() ) {
            return( 0 );
        }
        m8dd_build( mode );
        if( !m8dd_setinfo( &m8dd->info, 0 ) ) {
            return( 0 );
        }
        *(DWORD FAR *)lpOutput = m8dd->hInstance;
        return( 1 );
    }
    return( 0 );
}
