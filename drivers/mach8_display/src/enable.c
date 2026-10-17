/* Driver entry, Enable/Disable, palette and mode validation.
 *
 * Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
 * The GDIINFO and colour-table set-up follow boxv9x/vmdisp9x's enable.c
 * (Michal Necasek, JHRobotics; MIT, see LICENSE-MIT.txt).
 */

#include "m8ddi.h"
/* Not the DDK's valmode.h: it declares ValidateMode without __loadds, and that
 * first declaration wins - ValidateMode then ran on the caller's DS and read
 * the mode table from the wrong segment (bed, 10-16: 1024x768x8 refused). */
typedef struct {
    UINT    dvmSize;
    UINT    dvmBpp;
    int     dvmXRes;
    int     dvmYRes;
} DISPVALMODE;
#define VALMODE_YES         0
#define VALMODE_NO_UNKNOWN  4
#include <string.h>
#include <conio.h>
#include <dos.h>
#include "m8.h"

#define STOP_IO_TRAP        0x4000      /* INT 2Fh: the VDD stops trapping video I/O */
#define START_IO_TRAP       0x4007      /* ... and starts again */
#define DCICOMMAND          3075        /* frame-buffer access for DCI clients */

/* A 208 x 156 mm screen, as the DDK samples report. */
#define DISPLAY_HORZ_MM     208
#define DISPLAY_VERT_MM     156
#define DISPLAY_SIZE_EN     325
#define DISPLAY_SIZE_TWP    2340

extern int  m8_screen_init( LPDIBENGINE screen );
extern void m8_screen_done( void );
extern void m8_screen_suspend( void );
extern void m8_screen_resume( void );

/* switch.asm: the INT 2Fh hook and, in its code segment, the previous vector. */
extern void __far M8_INT2F_HOOK( void );
#pragma aux M8_INT2F_HOOK "*";
extern void __far * __far m8_old_int2f;
UINT WINAPI AllocCStoDSAlias( UINT selCode );

#define REPAINT_ORDINAL     275         /* USER's repaint-everything entry */

UINT WINAPI GlobalSmartPageLock( HGLOBAL hglb );

LPDIBENGINE m8_pdevice = 0;             /* the screen PDevice GDI gave us */
WORD        m8_palettized = 1;          /* pushed by the DIB thunks */
static WORD m8_dib_pdev_size;
static RGBQUAD FAR *m8_color_table;
static WORD m8_dpi = 96;
static WORD m8_repaint_disabled;
static WORD m8_repaint_pending;
static FARPROC m8_repaint;
static WORD m8_int2f_hooked;
static int  m8_mode = M8_MODE_800;
static WORD m8_reenabling;

/* The VDD: its API entry and this VM's handle, for the display settings. */
static DWORD m8_vdd_entry;
static WORD  m8_vm_handle;
#define VDD_ID                  0x000A
#define VDD_GET_DISPLAY_CONFIG  0x0085

/* The head of the VDD's DISPLAYINFO (MINIVDD.INC): what this driver reads. */
typedef struct {
    WORD    diHdrSize;
    WORD    diInfoFlags;
    DWORD   diDevNodeHandle;
    char    diDriverName[16];
    WORD    diXRes;
    WORD    diYRes;
    WORD    diDPI;
    BYTE    diPlanes;
    BYTE    diBpp;
    WORD    diRefreshRateMax;
    WORD    diRefreshRateMin;
    BYTE    diReserved[64];
} M8_DISPLAYINFO;

static DWORD m8_vdd_get_config( WORD fn, WORD size, LPVOID info );
#pragma aux m8_vdd_get_config =     ".386"     "movzx eax, ax"     "movzx ecx, cx"     "movzx ebx, m8_vm_handle"     "movzx edi, di"     "call dword ptr m8_vdd_entry"     "mov edx, eax"     "shr edx, 16"     parm [ax] [cx] [es di] modify [bx];

static void __far *m8_int2f_get_ep( unsigned ax, unsigned bx );
#pragma aux m8_int2f_get_ep = "int 2Fh" parm [ax] [bx] value [es di];
static WORD m8_int2f_get_vm( unsigned ax );
#pragma aux m8_int2f_get_vm = "int 2Fh" parm [ax] value [bx];

/* The resolution Display Properties chose, from the VDD; 800x600 if it says
 * nothing we can show. 8 bpp only. */
static void m8_read_mode( void )
{
    M8_DISPLAYINFO  di;
    int             i;
    int             x;
    int             y;

    m8_mode = M8_MODE_800;
    if( m8_vdd_entry == 0 ) {
        return;
    }
    _fmemset( &di, 0, sizeof( di ) );
    if( m8_vdd_get_config( VDD_GET_DISPLAY_CONFIG, sizeof( di ), &di ) != 0 ) {
        return;
    }
    for( i = 0; i < M8_MODE_COUNT; ++i ) {
        if( m8_hw_mode_size( i, &x, &y ) && x == (int)di.diXRes && y == (int)di.diYRes ) {
            m8_mode = i;
        }
    }
    m8_hw_mode_size( m8_mode, &m8_scr_x, &m8_scr_y );
}

/* The 20 static colours: first ten and last ten of the 8 bpp table. */
static const DIBColorEntry m8_static_lo[] = {
    { 0,    0,    0,    0          },
    { 0,    0,    0x80, 0          },
    { 0,    0x80, 0,    0          },
    { 0,    0x80, 0x80, 0          },
    { 0x80, 0,    0,    0          },
    { 0x80, 0,    0x80, 0          },
    { 0x80, 0x80, 0,    0          },
    { 0xC0, 0xC0, 0xC0, MAPTOWHITE },
    { 0xC0, 0xDC, 0xC0, MAPTOWHITE | NONSTATIC },
    { 0xF0, 0xCA, 0xA6, MAPTOWHITE | NONSTATIC }
};
static const DIBColorEntry m8_static_hi[] = {
    { 0xF0, 0xFB, 0xFF, MAPTOWHITE | NONSTATIC },
    { 0xA4, 0xA0, 0xA0, MAPTOWHITE | NONSTATIC },
    { 0x80, 0x80, 0x80, MAPTOWHITE },
    { 0,    0,    0xFF, 0          },
    { 0,    0xFF, 0,    MAPTOWHITE },
    { 0,    0xFF, 0xFF, MAPTOWHITE },
    { 0xFF, 0,    0,    0          },
    { 0xFF, 0,    0xFF, 0          },
    { 0xFF, 0xFF, 0,    MAPTOWHITE },
    { 0xFF, 0xFF, 0xFF, MAPTOWHITE }
};

/* The code segment, from CS: DriverInit runs in it. */
static WORD m8_code_seg( void );
#pragma aux m8_code_seg = "mov ax,cs" value [ax];

#pragma aux DriverInit parm [cx] [di] [es si]
UINT FAR DriverInit( UINT cbHeap, UINT hModule, LPSTR lpCmdLine )
{
    M8_STEP( 0xA001 );
    /* The cursor runs at interrupt time: its code must not be paged out. */
    GlobalSmartPageLock( (HGLOBAL)m8_code_seg() );
    m8_dpi = GetPrivateProfileInt( "display", "dpi", 96, "system.ini" );
    m8_vdd_entry = (DWORD)m8_int2f_get_ep( 0x1684, VDD_ID );
    m8_vm_handle = m8_int2f_get_vm( 0x1683 );
    m8_read_mode();
    M8_STEP( 0xA002 );
    return( 1 );
}

static void int_2Fh( unsigned ax );
#pragma aux int_2Fh = "int 2Fh" parm [ax];

static void int_10h( unsigned ax );
#pragma aux int_10h = "int 10h" parm [ax];

static void m8_fill_gdiinfo( LPGDIINFO lpInfo )
{
    lpInfo->dpVersion    = 0x0400;
    lpInfo->dpTechnology = DT_RASDISPLAY;
    lpInfo->dpHorzSize   = DISPLAY_HORZ_MM;
    lpInfo->dpVertSize   = DISPLAY_VERT_MM;
    lpInfo->dpHorzRes    = M8_SCREEN_X;
    lpInfo->dpVertRes    = M8_SCREEN_Y;
    lpInfo->dpBitsPixel  = 8;
    lpInfo->dpPlanes     = 1;
    lpInfo->dpNumFonts   = 0;
    lpInfo->dpNumBrushes = -1;
    lpInfo->dpNumPens    = 16;
    lpInfo->dpNumColors  = 20;
    lpInfo->dpNumPalReg  = 256;
    lpInfo->dpPalReserved = 20;
    lpInfo->dpColorRes   = 18;
    lpInfo->dpMLoWin.xcoord = DISPLAY_HORZ_MM * 10;
    lpInfo->dpMLoWin.ycoord = DISPLAY_VERT_MM * 10;
    lpInfo->dpMLoVpt.xcoord = M8_SCREEN_X;
    lpInfo->dpMLoVpt.ycoord = -M8_SCREEN_Y;
    lpInfo->dpMHiWin.xcoord = DISPLAY_HORZ_MM * 100;
    lpInfo->dpMHiWin.ycoord = DISPLAY_VERT_MM * 100;
    lpInfo->dpMHiVpt.xcoord = M8_SCREEN_X;
    lpInfo->dpMHiVpt.ycoord = -M8_SCREEN_Y;
    lpInfo->dpELoWin.xcoord = DISPLAY_SIZE_EN;
    lpInfo->dpELoWin.ycoord = DISPLAY_SIZE_EN;
    lpInfo->dpELoVpt.xcoord = M8_SCREEN_X;
    lpInfo->dpELoVpt.ycoord = -M8_SCREEN_X;
    lpInfo->dpEHiWin.xcoord = DISPLAY_SIZE_EN * 5;
    lpInfo->dpEHiWin.ycoord = DISPLAY_SIZE_EN * 5;
    lpInfo->dpEHiVpt.xcoord = M8_SCREEN_X / 2;
    lpInfo->dpEHiVpt.ycoord = -(M8_SCREEN_X / 2);
    lpInfo->dpTwpWin.xcoord = DISPLAY_SIZE_TWP;
    lpInfo->dpTwpWin.ycoord = DISPLAY_SIZE_TWP;
    lpInfo->dpTwpVpt.xcoord = M8_SCREEN_X / 2;
    lpInfo->dpTwpVpt.ycoord = -(M8_SCREEN_X / 2);
    lpInfo->dpLogPixelsX = m8_dpi;
    lpInfo->dpLogPixelsY = m8_dpi;
    lpInfo->dpDCManage   = DC_IgnoreDFNP;
    /* RC_SAVEBITMAP: USER calls our SaveScreenBitmap (screen.c) for what a menu
     * covers. C1_SLOW_CARD turns off the window effects that read the screen
     * back. */
    lpInfo->dpRaster    |= RC_DIBTODEV | RC_PALETTE | RC_SAVEBITMAP;
    lpInfo->dpCaps1     |= C1_BYTE_PACKED | C1_GLYPH_INDEX | C1_SLOW_CARD;
    lpInfo->dpCaps1     |= C1_REINIT_ABLE;
    lpInfo->dpCaps1     &= ~C1_COLORCURSOR;
}

static void __far *m8_get_vec( BYTE n );
#pragma aux m8_get_vec = "mov ah,35h" "int 21h" parm [al] value [es bx];
static void m8_set_vec( BYTE n, void __far *v );
#pragma aux m8_set_vec = "mov ah,25h" "push ds" "push es" "pop ds" "int 21h" "pop ds"     parm [al] [es dx];

/* The previous INT 2Fh vector is kept in the code segment (the hook jumps to
 * it CS-relative), so it is written through a data alias of that segment. */
static void m8_hook_int2f( void )
{
    WORD                alias;
    void __far * __far  *slot;

    if( m8_int2f_hooked ) {
        return;
    }
    if( m8_repaint == NULL ) {
        m8_repaint = GetProcAddress( GetModuleHandle( "USER" ), (LPCSTR)REPAINT_ORDINAL );
    }
    alias = AllocCStoDSAlias( FP_SEG( &m8_old_int2f ) );
    slot = (void __far * __far *)MK_FP( alias, FP_OFF( &m8_old_int2f ) );
    *slot = m8_get_vec( 0x2F );
    FreeSelector( alias );
    m8_set_vec( 0x2F, (void __far *)M8_INT2F_HOOK );
    m8_int2f_hooked = 1;
}

static void m8_unhook_int2f( void )
{
    if( m8_int2f_hooked ) {
        m8_set_vec( 0x2F, m8_old_int2f );
        m8_int2f_hooked = 0;
    }
}

static void m8_request_repaint( void )
{
    if( m8_repaint == NULL ) {
        return;
    }
    if( m8_repaint_disabled ) {
        m8_repaint_pending = 1;
    } else {
        m8_repaint();
    }
}

/* INT 2Fh 4001h: a DOS session takes the screen. Nothing may draw on the Mach8
 * until 4002h, and the monitor goes back to the VGA half. */
void __cdecl m8_switch_out( void )
{
    if( m8_pdevice == NULL ) {
        return;
    }
    m8_pdevice->deFlags |= BUSY;
    m8_screen_suspend();
    m8_hw_passthrough();
    m8_hw_off = 1;
}

/* INT 2Fh 4002h: Windows has the screen again. A DOS program may have used the
 * 8514 side (ATI's TEST.COM does), so the mode is set again and USER repaints
 * everything rather than trusting what is in video memory. */
void __cdecl m8_switch_in( void )
{
    if( m8_pdevice == NULL ) {
        return;
    }
    m8_hw_off = 0;
    m8_hw_set_mode( m8_mode );
    m8_hw_set_palette( 0, 256, m8_color_table );
    m8_pdevice->deFlags &= ~BUSY;
    m8_screen_resume();
    m8_request_repaint();
}

/* GDI calls Enable twice: style bit 0 set to fill in GDIINFO, then clear to
 * build the PDevice and set the mode. */
UINT WINAPI __loadds Enable( LPVOID lpDevice, UINT style, LPSTR lpDeviceType,
                             LPSTR lpOutputFile, LPVOID lpStuff )
{
    LPDIBENGINE     lpEng = lpDevice;
    LPBITMAPINFO    lpInfo;
    int             i;

    if( style & 1 ) {
        LPGDIINFO lpGdi = lpDevice;

        M8_STEP( 0xA011 );
        _fmemset( lpGdi, 0, sizeof( GDIINFO ) );
        DIB_Enable( lpDevice, style, lpDeviceType, lpOutputFile, lpStuff );
        m8_fill_gdiinfo( lpGdi );
        M8_STEP( 0xA013 );
        M8_STEP( lpGdi->dpLines );
        lpGdi->dpLines &= ~(LC_WIDE | LC_WIDESTYLED);
        /* Output shifts every point into its strip (screen.c); a PolyPolygon's
         * layout is not known here, so GDI breaks those into polygons. */
        M8_STEP( lpGdi->dpPolygonals );
        lpGdi->dpPolygonals &= ~PC_POLYPOLYGON;
        m8_dib_pdev_size = lpGdi->dpDEVICEsize;
        lpGdi->dpDEVICEsize += sizeof( BITMAPINFOHEADER ) + 256 * 4;
        M8_STEP( 0xA012 );
        return( sizeof( GDIINFO ) );
    }

    M8_STEP( 0xA020 );
    int_2Fh( STOP_IO_TRAP );
    M8_STEP( 0xA021 );
    if( !m8_hw_set_mode( m8_mode ) ) {
        m8_hw_passthrough();
        return( 0 );
    }
    M8_STEP( 0xA022 );
    DIB_Enable( lpDevice, style, lpDeviceType, lpOutputFile, lpStuff );
    M8_STEP( 0xA023 );
    DIB_SetPaletteTranslateExt( NULL, lpDevice );

    lpInfo = (LPBITMAPINFO)((LPBYTE)lpDevice + m8_dib_pdev_size);
    _fmemset( &lpInfo->bmiHeader, 0, sizeof( lpInfo->bmiHeader ) );
    lpInfo->bmiHeader.biSize     = sizeof( lpInfo->bmiHeader );
    lpInfo->bmiHeader.biWidth    = M8_SCREEN_X;
    lpInfo->bmiHeader.biHeight   = M8_SCREEN_Y;
    lpInfo->bmiHeader.biPlanes   = 1;
    lpInfo->bmiHeader.biBitCount = 8;
    m8_color_table = (RGBQUAD FAR *)lpInfo->bmiColors;
    /* A live mode change keeps the palette applications have realized. */
    if( !m8_reenabling ) {
    _fmemcpy( m8_color_table, m8_static_lo, sizeof( m8_static_lo ) );
    for( i = 10; i < 246; ++i ) {
        ((DIBColorEntry FAR *)m8_color_table)[i].dceRed = 0;
        ((DIBColorEntry FAR *)m8_color_table)[i].dceGreen = 0;
        ((DIBColorEntry FAR *)m8_color_table)[i].dceBlue = 0;
        ((DIBColorEntry FAR *)m8_color_table)[i].dceFlags = NONSTATIC;
    }
    _fmemcpy( &m8_color_table[246], m8_static_hi, sizeof( m8_static_hi ) );
    }

    /* No bits pointer: the DIB Engine never addresses this surface. */
    lpEng->deType         = TYPE_DIBENG;
    lpEng->deFlags        = MINIDRIVER | PALETTIZED | VRAM | NOT_FRAMEBUFFER;
    lpEng->deWidth        = M8_SCREEN_X;
    lpEng->deHeight       = M8_SCREEN_Y;
    lpEng->deWidthBytes   = M8_PITCH;
    lpEng->deDeltaScan    = M8_PITCH;
    lpEng->dePlanes       = 1;
    lpEng->deBitsPixel    = 8;
    lpEng->deReserved1    = 0;
    lpEng->delpPDevice    = 0;
    lpEng->deBitsOffset   = 0;
    lpEng->deBitsSelector = 0;
    lpEng->deBitmapInfo   = lpInfo;
    lpEng->deVersion      = 0x400;
    lpEng->deBeginAccess  = 0;
    lpEng->deEndAccess    = 0;

    m8_pdevice = lpEng;
    M8_STEP( 0xA024 );
    if( !m8_screen_init( lpEng ) ) {
        m8_pdevice = 0;
        m8_hw_passthrough();
        return( 0 );
    }
    m8_hw_set_palette( 0, 256, m8_color_table );
    m8_hook_int2f();
    M8_STEP( 0xA025 );
    return( 1 );
}

UINT WINAPI __loadds Disable( LPVOID lpDevice )
{
    LPDIBENGINE lpEng = lpDevice;

    M8_STEP( 0xA040 );
    if( lpEng != NULL ) {
        lpEng->deFlags |= BUSY;
    }
    /* As every DDK-era minidriver leaves: trapping back on before the VGA mode
     * is set, the monitor back on the VGA half, the BIOS's 80x25 text mode. */
    m8_unhook_int2f();
    int_2Fh( START_IO_TRAP );
    m8_screen_done();
    m8_pdevice = 0;
    m8_hw_passthrough();
    int_10h( 0x0003 );
    return( 1 );
}

UINT WINAPI __loadds SetPalette( UINT wStartIndex, UINT wNumEntries, LPVOID lpPalette )
{
    DIB_SetPaletteExt( wStartIndex, wNumEntries, lpPalette, m8_pdevice );
    if( m8_pdevice != NULL && !(m8_pdevice->deFlags & BUSY) ) {
        m8_hw_set_palette( wStartIndex, wNumEntries, &m8_color_table[wStartIndex] );
    }
    return( 0 );
}

/* DCI hands applications a frame buffer; there is none to hand. */
LONG WINAPI __loadds Control( LPVOID lpDevice, UINT function, LPVOID lpInput, LPVOID lpOutput )
{
    if( function == DCICOMMAND ) {
        return( 0 );
    }
    if( function == QUERYESCSUPPORT && lpInput != NULL && *(LPWORD)lpInput == DCICOMMAND ) {
        return( 0 );
    }
    return( DIB_Control( lpDevice, function, lpInput, lpOutput ) );
}

/* Display Properties changed the resolution: set the new mode and rebuild the
 * PDevice in place (GDI's allocation does not change size); if that fails, the
 * old mode goes back. Then GDIINFO for the new mode. */
UINT WINAPI __loadds ReEnable( LPVOID lpDevice, LPGDIINFO lpInfo )
{
    int     old = m8_mode;
    UINT    rc;

    m8_read_mode();
    m8_reenabling = 1;
    /* Quiet while the mode changes: the cursor moves at interrupt time. */
    m8_screen_suspend();
    rc = Enable( lpDevice, 0, NULL, NULL, NULL );
    if( rc ) {
        Enable( lpInfo, 1, NULL, NULL, NULL );
    } else {
        m8_mode = old;
        m8_hw_mode_size( m8_mode, &m8_scr_x, &m8_scr_y );
        Enable( lpDevice, 0, NULL, NULL, NULL );
        m8_request_repaint();
    }
    m8_reenabling = 0;
    m8_screen_resume();
    return( rc ? 1 : 0 );
}

/* 640x480, 800x600 and 1024x768, at 8 bpp. */
UINT WINAPI __loadds ValidateMode( DISPVALMODE FAR *lpMode )
{
    int i;
    int x;
    int y;

    /* Trace: what Windows asks (size, bpp, x, y) and the answer (A071 yes,
     * A072 no). */
    M8_STEP( 0xA070 );
    M8_STEP( lpMode->dvmSize );
    M8_STEP( lpMode->dvmBpp );
    M8_STEP( lpMode->dvmXRes );
    M8_STEP( lpMode->dvmYRes );
    if( lpMode->dvmBpp == 8 ) {
        for( i = 0; i < M8_MODE_COUNT; ++i ) {
            if( m8_hw_mode_size( i, &x, &y ) && lpMode->dvmXRes == x && lpMode->dvmYRes == y ) {
                M8_STEP( 0xA071 );
                return( VALMODE_YES );
            }
        }
    }
    M8_STEP( 0xA072 );
    return( VALMODE_NO_UNKNOWN );
}

/* USER asks which font resource suits the DPI (ordinal 450). */
DWORD WINAPI __loadds GetDriverResourceID( WORD wResID, LPSTR lpResType )
{
    if( wResID == OBJ_FONT && m8_dpi != 96 ) {
        return( 2003 );
    }
    return( wResID );
}

/* USER says whether a repaint request may be issued (ordinal 500). */
VOID WINAPI __loadds UserRepaintDisable( BOOL bDisable )
{
    m8_repaint_disabled = bDisable;
    if( !bDisable && m8_repaint_pending ) {
        m8_repaint_pending = 0;
        m8_request_repaint();
    }
}
