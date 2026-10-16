/* Driver entry, Enable/Disable, palette and mode validation.
 *
 * Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
 * The GDIINFO and colour-table set-up follow boxv9x/vmdisp9x's enable.c
 * (Michal Necasek, JHRobotics; MIT, see LICENSE-MIT.txt).
 */

#include "m8ddi.h"
#include <valmode.h>
#include <string.h>
#include <conio.h>
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

UINT WINAPI GlobalSmartPageLock( HGLOBAL hglb );

LPDIBENGINE m8_pdevice = 0;             /* the screen PDevice GDI gave us */
WORD        m8_palettized = 1;          /* pushed by the DIB thunks */
static WORD m8_dib_pdev_size;
static RGBQUAD FAR *m8_color_table;
static WORD m8_dpi = 96;
static WORD m8_repaint_disabled;

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
    /* RC_SAVEBITMAP would have the DIB Engine save screen areas by reading a
     * frame buffer; there is none. C1_SLOW_CARD turns off the window effects
     * that read the screen back. */
    lpInfo->dpRaster    |= RC_DIBTODEV | RC_PALETTE;
    lpInfo->dpRaster    &= ~RC_SAVEBITMAP;
    lpInfo->dpCaps1     |= C1_BYTE_PACKED | C1_GLYPH_INDEX | C1_SLOW_CARD;
    lpInfo->dpCaps1     &= ~(C1_COLORCURSOR | C1_REINIT_ABLE);
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
        m8_dib_pdev_size = lpGdi->dpDEVICEsize;
        lpGdi->dpDEVICEsize += sizeof( BITMAPINFOHEADER ) + 256 * 4;
        M8_STEP( 0xA012 );
        return( sizeof( GDIINFO ) );
    }

    M8_STEP( 0xA020 );
    int_2Fh( STOP_IO_TRAP );
    M8_STEP( 0xA021 );
    if( !m8_hw_set_mode() ) {
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
    _fmemcpy( m8_color_table, m8_static_lo, sizeof( m8_static_lo ) );
    for( i = 10; i < 246; ++i ) {
        ((DIBColorEntry FAR *)m8_color_table)[i].dceRed = 0;
        ((DIBColorEntry FAR *)m8_color_table)[i].dceGreen = 0;
        ((DIBColorEntry FAR *)m8_color_table)[i].dceBlue = 0;
        ((DIBColorEntry FAR *)m8_color_table)[i].dceFlags = NONSTATIC;
    }
    _fmemcpy( &m8_color_table[246], m8_static_hi, sizeof( m8_static_hi ) );

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

/* One mode in D8a: 800 x 600 x 8. */
UINT WINAPI __loadds ValidateMode( DISPVALMODE FAR *lpMode )
{
    M8_STEP( 0xA070 );
    if( lpMode->dvmBpp == 8 && lpMode->dvmXRes == M8_SCREEN_X && lpMode->dvmYRes == M8_SCREEN_Y ) {
        return( VALMODE_YES );
    }
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
}
