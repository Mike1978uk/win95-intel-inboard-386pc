/* 16 bpp on an 8 bpp card (docs/mach8_directdraw_plan.md, "16 bpp").
 *
 * Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
 *
 * The Mach8 shows 256 colours. In this mode GDI sees a 640x480 5-6-5 frame
 * buffer in RAM, drawn by the DIB Engine as any frame buffer; the screen
 * device lacks NOT_FRAMEBUFFER and is not m8_hot.screen, so every entry in
 * screen.c, blt.asm and pixel.asm passes it to the DIB Engine as it would a
 * memory bitmap. The DIB Engine brackets each drawing call with the access
 * callbacks; what they name goes to the card when the outermost call ends,
 * each pixel dithered (4x4 ordered) into a fixed palette. Only a byte per
 * pixel crosses the bus, as at 8 bpp, so the rectangles are kept tight:
 * Output, BitBlt and Pixel narrow them to what the call can change (the DIB
 * Engine names a line's clip rectangle, and a copy's source).
 *
 * The pointer is screen.c's, drawn on the card as at 8 bpp: the frame buffer
 * never holds it, and a send lifts it where they overlap.
 */

#include "m8ddi.h"
#include <string.h>
#include <dos.h>
#include "m8.h"

UINT WINAPI GlobalSmartPageLock( HGLOBAL hglb );

/* n bytes from sel:off (a 32-bit offset: the frame buffer's selector spans
 * the whole block, as the DIB Engine also requires) to dst. */
static void m8_copy32( WORD sel, DWORD off, BYTE FAR *dst, WORD n );
#pragma aux m8_copy32 =     ".386"     "push ds"     "mov ds, dx"     "shl ecx, 16"     "mov cx, bx"     "mov esi, ecx"     "movzx ecx, ax"     "movzx edi, di"     "cld"     "db 67h, 0F3h, 0A4h"     "pop ds"     parm [dx] [cx bx] [es di] [ax] modify [cx si di];

#define M8_FB16_W       640
#define M8_FB16_H       480
#define M8_FB16_PITCH   (M8_FB16_W * 2)

int                     m8_fb16;        /* this session's screen is 16 bpp */
RGBQUAD                 m8_fb16_pal[256];

static HGLOBAL          m8_fb16_mem;
static WORD             m8_fb16_sel;
static LPDIBENGINE      m8_fb16_dev;
static int              m8_fb16_depth;  /* access calls open */

/* What the open calls touched: a few rectangles, so a copy's source and
 * destination are not sent as the box around both. */
#define M8_FB16_DIRTY   4
typedef struct {
    int l;
    int t;
    int r;
    int b;
} M8_FB16_RECT;
static M8_FB16_RECT     m8_fb16_dirty[M8_FB16_DIRTY];
static int              m8_fb16_ndirty;

/* While set, what a call draws lies inside m8_fb16_lim (empty: it only
 * reads): the DIB Engine names the clip rectangle for a line. */
static int              m8_fb16_lim_on;
static M8_FB16_RECT     m8_fb16_lim;

/* The palette: a cube of 5 red x 9 green x 5 blue levels and a grey ramp.
 * Each channel has 0, 128, 192 and 255, so the Windows system colours are
 * entries and draw flat; only colours between levels are dithered. Green has
 * most levels, as the eye sees it best.
 *
 * Index i and 255 - i are inverse colours, black 00h and white FFh, as the
 * pointer code in screen.c assumes: it draws white as FFh and inverts with
 * XOR FFh. Cube entry c (red * 45 + green * 5 + blue) and 224 - c are
 * inverse, so the cube sits at c + 15 below its middle grey and c + 16 above
 * it; the middle grey takes 7Fh and 80h both. */
#define M8_FB16_RL      5
#define M8_FB16_GL      9
#define M8_FB16_BL      5
#define M8_FB16_NCUBE   (M8_FB16_RL * M8_FB16_GL * M8_FB16_BL)
#define M8_FB16_MID     (M8_FB16_NCUBE / 2)
static const BYTE m8_fb16_rb_lv[M8_FB16_RL] = { 0, 64, 128, 192, 255 };
static const BYTE m8_fb16_g_lv[M8_FB16_GL] = { 0, 32, 64, 96, 128, 160, 192, 224, 255 };
static BYTE             m8_fb16_cube[M8_FB16_NCUBE];   /* cube entry -> index */

/* Greys about every 8 steps: between cube levels a grey would dither with
 * unequal red, green and blue, which shows as coloured dots. Below the
 * middle they are 8 .. 120 at indices 1 .. 14, above it their inverses at
 * 255 - those. */
#define M8_FB16_YLOW    14
#define M8_FB16_YL      (2 * M8_FB16_YLOW + 5)
static const BYTE m8_fb16_y_low[M8_FB16_YLOW] = {
    8, 16, 24, 32, 40, 48, 56, 72, 80, 88, 96, 104, 112, 120
};
static BYTE             m8_fb16_y_lv[M8_FB16_YL];
static BYTE             m8_fb16_y_ix[M8_FB16_YL];

/* Cube entry parts per dither threshold: red * 45 + green * 5 + blue; and
 * the index of a grey by its green. */
static BYTE             m8_fb16_r[16][32];
static BYTE             m8_fb16_g[16][64];
static BYTE             m8_fb16_b[16][32];
static BYTE             m8_fb16_y[16][64];
static BYTE             m8_fb16_band[4096];

/* Counters for ExtEscape( M8_ESC_FB16STAT ): access calls, sends, pixels
 * sent; the fourth is unused. */
DWORD                   m8_fb16_stat[4];

static const BYTE m8_fb16_bayer[4][4] = {
    {  0,  8,  2, 10 },
    { 12,  4, 14,  6 },
    {  3, 11,  1,  9 },
    { 15,  7, 13,  5 }
};

/* The level that channel value v of `bits` bits stores exactly, or -1. The
 * DIB Engine stores an 8-bit value c as c * max / 255 rounded down (colours
 * GDI realizes) or as c's top bits (DIBs it converts). Only those codes
 * match, so a gradient is not flattened around each level. */
static int m8_fb16_exact( int v, int bits, const BYTE *lv, int n )
{
    int max = (1 << bits) - 1;
    int i;

    for( i = 0; i < n; ++i ) {
        if( v == lv[i] * max / 255 || v == lv[i] >> (8 - bits) ) {
            return( i );
        }
    }
    return( -1 );
}

/* As m8_fb16_exact; icons reach the frame buffer with 5-bit green, doubled,
 * so an even green is also tried as that. */
static int m8_fb16_exact_any( int v, int bits, const BYTE *lv, int n )
{
    int i = m8_fb16_exact( v, bits, lv, n );

    if( i < 0 && bits == 6 && (v & 1) == 0 ) {
        i = m8_fb16_exact( v >> 1, 5, lv, n );
    }
    return( i );
}

/* The level for channel value v of `bits` bits, by threshold t. A colour
 * GDI was asked for exactly is taken as is: dithering it would only add
 * dots. */
static BYTE m8_fb16_level( int v, int bits, const BYTE *lv, int n, int t )
{
    int max = (1 << bits) - 1;
    int e = (v * 255 * 2 + max) / (2 * max);
    int i;

    i = m8_fb16_exact_any( v, bits, lv, n );
    if( i >= 0 ) {
        return( (BYTE)i );
    }
    for( i = 0; i < n - 1 && e > lv[i + 1]; ++i ) {
    }
    if( (long)(e - lv[i]) * 32 > (long)(2 * t + 1) * (lv[i + 1] - lv[i]) ) {
        ++i;
    }
    return( (BYTE)i );
}

static void m8_fb16_set( int i, BYTE r, BYTE g, BYTE b )
{
    m8_fb16_pal[i].rgbRed = r;
    m8_fb16_pal[i].rgbGreen = g;
    m8_fb16_pal[i].rgbBlue = b;
    m8_fb16_pal[i].rgbReserved = 0;
}

static void m8_fb16_tables( void )
{
    int t;
    int v;
    int c;
    int i;
    int k;

    for( c = 0; c < M8_FB16_NCUBE; ++c ) {
        m8_fb16_cube[c] = (BYTE)(c < M8_FB16_MID ? c + 15 : c == M8_FB16_MID ? 127 : c + 16);
        m8_fb16_set( m8_fb16_cube[c], m8_fb16_rb_lv[c / (M8_FB16_GL * M8_FB16_BL)],
                     m8_fb16_g_lv[c / M8_FB16_BL % M8_FB16_GL], m8_fb16_rb_lv[c % M8_FB16_BL] );
    }
    m8_fb16_set( 128, 128, 128, 128 );

    /* The ramp in order: cube greys at 0, 64, 128, 192 and 255, the low
     * greys between the first three, their inverses between the last. */
    i = 0;
    for( k = 0; k <= 4; ++k ) {
        m8_fb16_y_lv[i] = m8_fb16_rb_lv[k];
        m8_fb16_y_ix[i++] = (BYTE)(k == 0 ? 0x00 : k == 4 ? 0xFF :
                                   m8_fb16_cube[k * M8_FB16_GL * M8_FB16_BL +
                                                (k * 2) * M8_FB16_BL + k]);
        if( k == 4 ) {
            break;
        }
        for( c = 0; c < M8_FB16_YLOW / 2; ++c ) {
            v = k < 2 ? k * (M8_FB16_YLOW / 2) + c : (3 - k) * (M8_FB16_YLOW / 2) +
                                                     (M8_FB16_YLOW / 2 - 1 - c);
            m8_fb16_y_lv[i] = (BYTE)(k < 2 ? m8_fb16_y_low[v] : 255 - m8_fb16_y_low[v]);
            m8_fb16_y_ix[i++] = (BYTE)(k < 2 ? v + 1 : 254 - v);
        }
    }
    m8_fb16_set( 0x00, 0, 0, 0 );
    m8_fb16_set( 0xFF, 255, 255, 255 );
    for( k = 0; k < M8_FB16_YLOW; ++k ) {
        v = m8_fb16_y_low[k];
        m8_fb16_set( k + 1, (BYTE)v, (BYTE)v, (BYTE)v );
        m8_fb16_set( 254 - k, (BYTE)(255 - v), (BYTE)(255 - v), (BYTE)(255 - v) );
    }

    /* A system grey first: icons store 192 as green 46, which is also the
     * code of a ramp grey near it. */
    for( v = 0; v < 64; ++v ) {
        k = m8_fb16_exact_any( v, 6, m8_fb16_rb_lv, M8_FB16_RL );
        for( t = 0; t < 16; ++t ) {
            m8_fb16_y[t][v] = k >= 0 ?
                m8_fb16_cube[k * M8_FB16_GL * M8_FB16_BL + (k * 2) * M8_FB16_BL + k] :
                m8_fb16_y_ix[m8_fb16_level( v, 6, m8_fb16_y_lv, M8_FB16_YL, t )];
        }
    }
    for( t = 0; t < 16; ++t ) {
        for( v = 0; v < 32; ++v ) {
            m8_fb16_r[t][v] = (BYTE)(m8_fb16_level( v, 5, m8_fb16_rb_lv, M8_FB16_RL, t ) *
                                     M8_FB16_GL * M8_FB16_BL);
            m8_fb16_b[t][v] = m8_fb16_level( v, 5, m8_fb16_rb_lv, M8_FB16_BL, t );
        }
        for( v = 0; v < 64; ++v ) {
            m8_fb16_g[t][v] = (BYTE)(m8_fb16_level( v, 6, m8_fb16_g_lv, M8_FB16_GL, t ) *
                                     M8_FB16_BL);
        }
    }
}

static WORD m8_fb16_src[M8_FB16_W];

/* Row y, columns l .. r - 1, dithered into dst. A grey stores red = blue =
 * green / 2 (5-bit red and blue, 6-bit green) and takes the grey ramp. */
static void m8_fb16_row( BYTE FAR *dst, int y, int l, int r )
{
    const BYTE  *th = m8_fb16_bayer[y & 3];
    WORD        *p = m8_fb16_src;
    WORD        c;
    WORD        rb;
    WORD        g;
    int         t;
    int         x;

    m8_copy32( m8_fb16_sel, (DWORD)y * M8_FB16_PITCH + (DWORD)l * 2, (BYTE FAR *)m8_fb16_src,
               (WORD)((r - l) * 2) );
    for( x = l; x < r; ++x ) {
        c = *p++;
        t = th[x & 3];
        rb = c & 31;
        g = (c >> 5) & 63;
        if( (c >> 11) == rb && (g >> 1) == rb ) {
            *dst++ = m8_fb16_y[t][g];
        } else {
            *dst++ = m8_fb16_cube[m8_fb16_r[t][c >> 11] + m8_fb16_g[t][g] + m8_fb16_b[t][rb]];
        }
    }
}

/* The rectangle to the card, in bands through m8_fb16_band, with the pointer
 * lifted if they overlap. Host transfers carry two pixels a word, so it
 * widens to even columns. */
static void m8_fb16_send( int l, int t, int r, int b )
{
    int w;
    int n;
    int y;
    int k;

    if( l < 0 ) l = 0;
    if( t < 0 ) t = 0;
    if( r > M8_FB16_W ) r = M8_FB16_W;
    if( b > M8_FB16_H ) b = M8_FB16_H;
    l &= ~1;
    r = (r + 1) & ~1;
    w = r - l;
    if( w <= 0 || b <= t ) {
        return;
    }
    ++m8_fb16_stat[1];
    m8_fb16_stat[2] += (DWORD)w * (b - t);
    m8_cur_lift( l, t, r, b );
    n = (int)(sizeof( m8_fb16_band ) / w);
    for( y = t; y < b; y += n ) {
        if( n > b - y ) {
            n = b - y;
        }
        for( k = 0; k < n; ++k ) {
            m8_fb16_row( m8_fb16_band + k * w, y + k, l, r );
        }
        m8_hw_write_rect( l, y, w, n, m8_fb16_band, (WORD)w );
    }
    m8_leave();
}

static long m8_fb16_area( const M8_FB16_RECT *a )
{
    return( (long)(a->r - a->l) * (a->b - a->t) );
}

static void m8_fb16_union( M8_FB16_RECT *a, const M8_FB16_RECT *n )
{
    if( n->l < a->l ) a->l = n->l;
    if( n->t < a->t ) a->t = n->t;
    if( n->r > a->r ) a->r = n->r;
    if( n->b > a->b ) a->b = n->b;
}

/* Into the list: joined to a rectangle it touches, else added, else joined
 * to the one that grows least. */
static void m8_fb16_add( const M8_FB16_RECT *n )
{
    M8_FB16_RECT    u;
    M8_FB16_RECT    *d;
    long            grow;
    long            best_grow = 0;
    int             best = 0;
    int             k;

    for( k = 0; k < m8_fb16_ndirty; ++k ) {
        d = &m8_fb16_dirty[k];
        if( n->l <= d->r && d->l <= n->r && n->t <= d->b && d->t <= n->b ) {
            m8_fb16_union( d, n );
            return;
        }
    }
    if( m8_fb16_ndirty < M8_FB16_DIRTY ) {
        m8_fb16_dirty[m8_fb16_ndirty++] = *n;
        return;
    }
    for( k = 0; k < M8_FB16_DIRTY; ++k ) {
        u = m8_fb16_dirty[k];
        m8_fb16_union( &u, n );
        grow = m8_fb16_area( &u ) - m8_fb16_area( &m8_fb16_dirty[k] );
        if( k == 0 || grow < best_grow ) {
            best = k;
            best_grow = grow;
        }
    }
    m8_fb16_union( &m8_fb16_dirty[best], n );
}

/* Right and bottom are the last pixel drawn, not one past it. */
static VOID WINAPI __loadds m8_fb16_begin( LPPDEVICE lpDevice, WORD wLeft, WORD wTop,
                                           WORD wRight, WORD wBottom, WORD wFlags )
{
    M8_FB16_RECT    n;

    ++m8_fb16_stat[0];
    DIB_BeginAccess( lpDevice, wLeft, wTop, wRight, wBottom, wFlags );
    ++m8_fb16_depth;
    n.l = (short)wLeft;
    n.t = (short)wTop;
    n.r = ((short)wRight >= n.l ? (short)wRight : n.l) + 1;
    n.b = ((short)wBottom >= n.t ? (short)wBottom : n.t) + 1;
    if( m8_fb16_lim_on ) {
        if( m8_fb16_lim.l > n.l ) n.l = m8_fb16_lim.l;
        if( m8_fb16_lim.t > n.t ) n.t = m8_fb16_lim.t;
        if( m8_fb16_lim.r < n.r ) n.r = m8_fb16_lim.r;
        if( m8_fb16_lim.b < n.b ) n.b = m8_fb16_lim.b;
        if( n.r <= n.l || n.b <= n.t ) {
            return;
        }
    }
    m8_fb16_add( &n );
}

static VOID WINAPI __loadds m8_fb16_end( LPPDEVICE lpDevice, WORD wFlags )
{
    int k;

    DIB_EndAccess( lpDevice, wFlags );
    if( m8_fb16_depth > 0 && --m8_fb16_depth == 0 ) {
        for( k = 0; k < m8_fb16_ndirty; ++k ) {
            m8_fb16_send( m8_fb16_dirty[k].l, m8_fb16_dirty[k].t, m8_fb16_dirty[k].r,
                          m8_fb16_dirty[k].b );
        }
        m8_fb16_ndirty = 0;
    }
}

/* Lines and shapes: inside the box around their points, widened for the
 * pen. Scanline lists and markers are not boxes of their points. */
WORD m8_fb16_output( LPPDEVICE lpDestDev, WORD wStyle, WORD wCount, LPPOINT lpPoints,
                     LPPPEN lpPPen, LPPBRUSH lpPBrush, LPDRAWMODE lpDrawMode,
                     LPRECT lpClipRect )
{
    WORD    rc;
    WORD    i;

    if( lpPoints != NULL && wCount > 0 && wStyle != OS_SCANLINES && wStyle != OS_POLYMARKER &&
        wStyle != OS_BEGINNSCAN && wStyle != OS_ENDNSCAN ) {
        m8_fb16_lim.l = m8_fb16_lim.r = lpPoints[0].xcoord;
        m8_fb16_lim.t = m8_fb16_lim.b = lpPoints[0].ycoord;
        for( i = 1; i < wCount; ++i ) {
            if( lpPoints[i].xcoord < m8_fb16_lim.l ) m8_fb16_lim.l = lpPoints[i].xcoord;
            if( lpPoints[i].xcoord > m8_fb16_lim.r ) m8_fb16_lim.r = lpPoints[i].xcoord;
            if( lpPoints[i].ycoord < m8_fb16_lim.t ) m8_fb16_lim.t = lpPoints[i].ycoord;
            if( lpPoints[i].ycoord > m8_fb16_lim.b ) m8_fb16_lim.b = lpPoints[i].ycoord;
        }
        m8_fb16_lim.l -= 2;
        m8_fb16_lim.t -= 2;
        m8_fb16_lim.r += 3;
        m8_fb16_lim.b += 3;
        m8_fb16_lim_on = 1;
    }
    rc = DIB_Output( lpDestDev, wStyle, wCount, lpPoints, lpPPen, lpPBrush, lpDrawMode,
                     lpClipRect );
    m8_fb16_lim_on = 0;
    return( rc );
}

/* A copy changes its destination rectangle only (the DIB Engine names the
 * box around source and destination); to anywhere but the screen it only
 * reads it. */
BOOL m8_fb16_bitblt( LPPDEVICE lpDestDev, WORD wDestX, WORD wDestY, LPPDEVICE lpSrcDev,
                     WORD wSrcX, WORD wSrcY, WORD wXext, WORD wYext, DWORD dwRop3,
                     LPBRUSH lpPBrush, LPDRAWMODE lpDrawMode )
{
    BOOL    rc;

    if( (LPVOID)lpDestDev != (LPVOID)m8_fb16_dev ) {
        m8_fb16_lim.l = m8_fb16_lim.r = 0;
    } else {
        m8_fb16_lim.l = (short)wDestX;
        m8_fb16_lim.t = (short)wDestY;
        m8_fb16_lim.r = (short)wDestX + (short)wXext;
        m8_fb16_lim.b = (short)wDestY + (short)wYext;
    }
    m8_fb16_lim_on = 1;
    rc = DIB_BitBlt( lpDestDev, wDestX, wDestY, lpSrcDev, wSrcX, wSrcY, wXext, wYext, dwRop3,
                     lpPBrush, lpDrawMode );
    m8_fb16_lim_on = 0;
    return( rc );
}

/* No draw mode: a get. */
DWORD m8_fb16_pixel( LPPDEVICE lpDestDev, WORD X, WORD Y, DWORD dwPhysColor,
                     LPDRAWMODE lpDrawMode )
{
    DWORD   rc;

    if( lpDrawMode == NULL ) {
        m8_fb16_lim.l = m8_fb16_lim.r = 0;
        m8_fb16_lim_on = 1;
    }
    rc = DIB_Pixel( lpDestDev, X, Y, dwPhysColor, lpDrawMode );
    m8_fb16_lim_on = 0;
    return( rc );
}

/* The frame buffer, once. 0 = no memory. */
int m8_fb16_alloc( void )
{
    if( m8_fb16_sel != 0 ) {
        return( 1 );
    }
    m8_fb16_mem = GlobalAlloc( GMEM_FIXED | GMEM_SHARE | GMEM_ZEROINIT,
                               (DWORD)M8_FB16_PITCH * M8_FB16_H );
    if( m8_fb16_mem == 0 ) {
        return( 0 );
    }
    GlobalSmartPageLock( m8_fb16_mem );
    m8_fb16_sel = FP_SEG( GlobalLock( m8_fb16_mem ) );
    m8_fb16_tables();
    return( 1 );
}

/* The screen device over the frame buffer. `info` has room for the header
 * and three colour masks. */
void m8_fb16_device( LPDIBENGINE dev, LPBITMAPINFO info )
{
    DWORD FAR *masks = (DWORD FAR *)info->bmiColors;

    _fmemset( &info->bmiHeader, 0, sizeof( info->bmiHeader ) );
    info->bmiHeader.biSize = sizeof( info->bmiHeader );
    info->bmiHeader.biWidth = M8_FB16_W;
    info->bmiHeader.biHeight = M8_FB16_H;
    info->bmiHeader.biPlanes = 1;
    info->bmiHeader.biBitCount = 16;
    info->bmiHeader.biCompression = BI_BITFIELDS;
    masks[0] = 0xF800UL;
    masks[1] = 0x07E0UL;
    masks[2] = 0x001FUL;

    dev->deType = TYPE_DIBENG;
    dev->deFlags = MINIDRIVER | VRAM | FIVE6FIVE;
    dev->deWidth = M8_FB16_W;
    dev->deHeight = M8_FB16_H;
    dev->deWidthBytes = M8_FB16_PITCH;
    dev->deDeltaScan = M8_FB16_PITCH;
    dev->dePlanes = 1;
    dev->deBitsPixel = 16;
    dev->deReserved1 = 0;
    dev->delpPDevice = 0;
    dev->deBitsOffset = 0;
    dev->deBitsSelector = m8_fb16_sel;
    dev->deBitmapInfo = info;
    dev->deVersion = 0x400;
    dev->deBeginAccess = (void (WINAPI *)())m8_fb16_begin;
    dev->deEndAccess = (void (WINAPI *)())m8_fb16_end;
    m8_fb16_dev = dev;
    m8_fb16_depth = 0;
}

/* The card against the frame buffer, row by row, with the pointer lifted:
 * out[0] = pixels that differ from what the frame buffer dithers to, out[1..4]
 * = the box around them (left, top, right, bottom; exclusive). */
void m8_fb16_check( DWORD FAR *out )
{
    BYTE    *want = m8_fb16_band;
    BYTE    *have = m8_fb16_band + M8_FB16_W;
    int     x;
    int     y;

    out[0] = 0;
    out[1] = M8_FB16_W;
    out[2] = M8_FB16_H;
    out[3] = 0;
    out[4] = 0;
    m8_cur_lift( 0, 0, M8_FB16_W, M8_FB16_H );
    for( y = 0; y < M8_FB16_H; ++y ) {
        m8_fb16_row( (BYTE FAR *)want, y, 0, M8_FB16_W );
        m8_hw_read_rect( 0, y, M8_FB16_W, 1, (BYTE FAR *)have, M8_FB16_W );
        for( x = 0; x < M8_FB16_W; ++x ) {
            if( want[x] != have[x] ) {
                ++out[0];
                if( x < (int)out[1] ) out[1] = x;
                if( y < (int)out[2] ) out[2] = y;
                if( x >= (int)out[3] ) out[3] = x + 1;
                if( y >= (int)out[4] ) out[4] = y + 1;
            }
        }
    }
    m8_leave();
}

/* The whole frame buffer to the card, after the mode is set: what the card
 * held, the pointer and its masks among it, is gone. */
void m8_fb16_send_all( void )
{
    m8_hw_set_palette( 0, 256, m8_fb16_pal );
    m8_cur_reset();
    m8_fb16_send( 0, 0, M8_FB16_W, M8_FB16_H );
}
