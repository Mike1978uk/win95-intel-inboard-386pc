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
 * each pixel dithered (4x4 ordered) into a fixed 5x9x5 palette. Only a
 * byte per pixel crosses the bus, as at 8 bpp, so the rectangles are kept
 * tight: Output, BitBlt and Pixel narrow them to what the call can change
 * (the DIB Engine names a line's clip rectangle, and a copy's source).
 *
 * The pointer is the DIB Engine's, drawn in the frame buffer; after it moves,
 * the old and new rectangles go to the card. That can be at interrupt time,
 * so the frame buffer is page-locked, and a send already under way only
 * records the move: it is sent when that send ends.
 */

#include "m8ddi.h"
#include <string.h>
#include <dos.h>
#include "m8.h"

UINT WINAPI GlobalSmartPageLock( HGLOBAL hglb );

static void m8_cli( void );
#pragma aux m8_cli = "cli";
static void m8_sti( void );
#pragma aux m8_sti = "sti";

/* n bytes from sel:off (a 32-bit offset: the frame buffer's selector spans
 * the whole block, as the DIB Engine also requires) to dst. */
static void m8_copy32( WORD sel, DWORD off, BYTE FAR *dst, WORD n );
#pragma aux m8_copy32 =     ".386"     "push ds"     "mov ds, dx"     "shl ecx, 16"     "mov cx, bx"     "mov esi, ecx"     "movzx ecx, ax"     "movzx edi, di"     "cld"     "db 67h, 0F3h, 0A4h"     "pop ds"     parm [dx] [cx bx] [es di] [ax] modify [cx si di];

#define M8_FB16_W       640
#define M8_FB16_H       480
#define M8_FB16_PITCH   (M8_FB16_W * 2)
#define M8_FB16_CUR     32              /* the DIB Engine's pointer, at most */

int                     m8_fb16;        /* this session's screen is 16 bpp */
RGBQUAD                 m8_fb16_pal[256];

static HGLOBAL          m8_fb16_mem;
static WORD             m8_fb16_sel;
static LPDIBENGINE      m8_fb16_dev;
static int              m8_fb16_depth;  /* access calls open */
static int              m8_fb16_busy;   /* a send is under way */

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
static int              m8_fb16_cur_pending;
static int              m8_fb16_cur_late;   /* CheckCursor sends the pointer again */
static int              m8_fb16_cur_x;
static int              m8_fb16_cur_y;
static int              m8_fb16_hot_x;
static int              m8_fb16_hot_y;
#define M8_FB16_ON      4
static M8_FB16_RECT     m8_fb16_on[M8_FB16_ON]; /* where the card may show the pointer */
static int              m8_fb16_non;

/* The palette: 5 red x 9 green x 5 blue levels. Each channel has 0, 128,
 * 192 and 255, so the Windows system colours are entries and draw flat;
 * only colours between levels are dithered. Green has most levels, as the
 * eye sees it best. */
#define M8_FB16_RL      5
#define M8_FB16_GL      9
#define M8_FB16_BL      5
#define M8_FB16_NPAL    (M8_FB16_RL * M8_FB16_GL * M8_FB16_BL)
static const BYTE m8_fb16_rb_lv[M8_FB16_RL] = { 0, 64, 128, 192, 255 };
static const BYTE m8_fb16_g_lv[M8_FB16_GL] = { 0, 32, 64, 96, 128, 160, 192, 224, 255 };

/* Index parts per dither threshold: red * 45 + green * 5 + blue. */
static BYTE             m8_fb16_r[16][32];
static BYTE             m8_fb16_g[16][64];
static BYTE             m8_fb16_b[16][32];
static BYTE             m8_fb16_band[4096];

/* Counters for ExtEscape( M8_ESC_FB16STAT ): access calls, sends, pixels
 * sent, pointer sends. */
DWORD                   m8_fb16_stat[4];

static const BYTE m8_fb16_bayer[4][4] = {
    {  0,  8,  2, 10 },
    { 12,  4, 14,  6 },
    {  3, 11,  1,  9 },
    { 15,  7, 13,  5 }
};

/* The level that channel value v of `bits` bits stores exactly, or -1. The
 * DIB Engine stores an 8-bit value c as c * max / 255 rounded down (colours
 * GDI realizes) or as c's top bits (DIBs it converts). */
static int m8_fb16_exact( int v, int bits, const BYTE *lv, int n )
{
    int max = (1 << bits) - 1;
    int lo = (v * 255 + max - 1) / max;
    int hi = ((v + 1) * 255 + max - 1) / max - 1;
    int top = v << (8 - bits);
    int i;

    for( i = 0; i < n; ++i ) {
        if( (lv[i] >= lo && lv[i] <= hi) ||
            (lv[i] >= top && lv[i] < top + (1 << (8 - bits))) ) {
            return( i );
        }
    }
    return( -1 );
}

/* The level for channel value v of `bits` bits, by threshold t. A colour
 * GDI was asked for exactly is taken as is: dithering it would only add
 * dots. Icons reach the frame buffer with 5-bit green, doubled, so an even
 * green is also tried as that. */
static BYTE m8_fb16_level( int v, int bits, const BYTE *lv, int n, int t )
{
    int max = (1 << bits) - 1;
    int e = (v * 255 * 2 + max) / (2 * max);
    int i;

    i = m8_fb16_exact( v, bits, lv, n );
    if( i < 0 && bits == 6 && (v & 1) == 0 ) {
        i = m8_fb16_exact( v >> 1, 5, lv, n );
    }
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

static void m8_fb16_tables( void )
{
    int t;
    int v;
    int i;

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
    for( i = 0; i < 256; ++i ) {
        if( i < M8_FB16_NPAL ) {
            m8_fb16_pal[i].rgbRed = m8_fb16_rb_lv[i / (M8_FB16_GL * M8_FB16_BL)];
            m8_fb16_pal[i].rgbGreen = m8_fb16_g_lv[i / M8_FB16_BL % M8_FB16_GL];
            m8_fb16_pal[i].rgbBlue = m8_fb16_rb_lv[i % M8_FB16_BL];
        } else {
            m8_fb16_pal[i].rgbRed = 0;
            m8_fb16_pal[i].rgbGreen = 0;
            m8_fb16_pal[i].rgbBlue = 0;
        }
        m8_fb16_pal[i].rgbReserved = 0;
    }
}

static WORD m8_fb16_src[M8_FB16_W];

/* Row y, columns l .. r - 1, dithered into dst. */
static void m8_fb16_row( BYTE FAR *dst, int y, int l, int r )
{
    const BYTE  *th = m8_fb16_bayer[y & 3];
    WORD        *p = m8_fb16_src;
    WORD        c;
    int         t;
    int         x;

    m8_copy32( m8_fb16_sel, (DWORD)y * M8_FB16_PITCH + (DWORD)l * 2, (BYTE FAR *)m8_fb16_src,
               (WORD)((r - l) * 2) );
    for( x = l; x < r; ++x ) {
        c = *p++;
        t = th[x & 3];
        *dst++ = (BYTE)(m8_fb16_r[t][c >> 11] + m8_fb16_g[t][(c >> 5) & 63] +
                        m8_fb16_b[t][c & 31]);
    }
}

/* The rectangle to the card, in bands through m8_fb16_band. Host transfers
 * carry two pixels a word, so it widens to even columns. */
static void m8_fb16_send_rect( int l, int t, int r, int b )
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
}

static int m8_fb16_meets( const M8_FB16_RECT *a, int l, int t )
{
    return( a->l < l + M8_FB16_CUR && l < a->r && a->t < t + M8_FB16_CUR && t < a->b );
}

/* The card may show the pointer at (l, t): the next pointer send clears it.
 * When the list is full the last entry grows to cover both. */
static void m8_fb16_cur_shown( int l, int t )
{
    int k;

    for( k = 0; k < m8_fb16_non; ++k ) {
        if( m8_fb16_on[k].l == l && m8_fb16_on[k].t == t ) {
            return;
        }
    }
    if( m8_fb16_non < M8_FB16_ON ) {
        m8_fb16_on[m8_fb16_non].l = l;
        m8_fb16_on[m8_fb16_non].t = t;
        m8_fb16_on[m8_fb16_non].r = l + M8_FB16_CUR;
        m8_fb16_on[m8_fb16_non].b = t + M8_FB16_CUR;
        ++m8_fb16_non;
        return;
    }
    k = M8_FB16_ON - 1;
    if( l < m8_fb16_on[k].l ) m8_fb16_on[k].l = l;
    if( t < m8_fb16_on[k].t ) m8_fb16_on[k].t = t;
    if( l + M8_FB16_CUR > m8_fb16_on[k].r ) m8_fb16_on[k].r = l + M8_FB16_CUR;
    if( t + M8_FB16_CUR > m8_fb16_on[k].b ) m8_fb16_on[k].b = t + M8_FB16_CUR;
}

/* Every place the card may show the pointer, then where it is now. */
static void m8_fb16_send_cursor( void )
{
    int l = m8_fb16_cur_x - m8_fb16_hot_x;
    int t = m8_fb16_cur_y - m8_fb16_hot_y;
    int k;

    ++m8_fb16_stat[3];
    for( k = 0; k < m8_fb16_non; ++k ) {
        if( m8_fb16_on[k].l != l || m8_fb16_on[k].t != t ) {
            m8_fb16_send_rect( m8_fb16_on[k].l, m8_fb16_on[k].t, m8_fb16_on[k].r,
                               m8_fb16_on[k].b );
        }
    }
    m8_fb16_send_rect( l, t, l + M8_FB16_CUR, t + M8_FB16_CUR );
    m8_fb16_non = 0;
    m8_fb16_cur_shown( l, t );
}

/* Send, unless a send is already under way: then it is left to that one. */
static void m8_fb16_send( int l, int t, int r, int b, int cursor )
{
    m8_cli();
    if( m8_fb16_busy ) {
        if( cursor ) {
            m8_fb16_cur_pending = 1;
        }
        m8_sti();
        return;
    }
    m8_fb16_busy = 1;
    m8_sti();
    if( cursor ) {
        m8_fb16_send_cursor();
    } else {
        M8_FB16_RECT    a;
        int             cl = m8_fb16_cur_x - m8_fb16_hot_x;
        int             ct = m8_fb16_cur_y - m8_fb16_hot_y;

        m8_fb16_send_rect( l, t, r, b );
        a.l = l;
        a.t = t;
        a.r = r;
        a.b = b;
        if( m8_fb16_meets( &a, cl, ct ) ) {
            m8_fb16_cur_shown( cl, ct );
        }
    }
    for( ;; ) {
        m8_cli();
        if( !m8_fb16_cur_pending ) {
            m8_fb16_busy = 0;
            m8_sti();
            return;
        }
        m8_fb16_cur_pending = 0;
        m8_sti();
        m8_fb16_send_cursor();
    }
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
    /* The DIB Engine lifts the pointer out of what this call draws and puts
     * it back later: CheckCursor sends it again. */
    if( m8_fb16_meets( &n, m8_fb16_cur_x - m8_fb16_hot_x, m8_fb16_cur_y - m8_fb16_hot_y ) ) {
        m8_fb16_cur_late = 1;
    }
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
                          m8_fb16_dirty[k].b, 0 );
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

/* The frame buffer, once: page-locked, as the pointer draws in it at
 * interrupt time. 0 = no memory. */
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

/* The whole frame buffer to the card: after the mode is set. */
void m8_fb16_send_all( void )
{
    m8_hw_set_palette( 0, 256, m8_fb16_pal );
    m8_fb16_send( 0, 0, M8_FB16_W, M8_FB16_H, 0 );
}

void m8_fb16_set_cursor( LPVOID shape )
{
    if( m8_fb16_dev == NULL ) {
        return;
    }
    if( shape != NULL ) {
        m8_fb16_hot_x = ((short FAR *)shape)[0];
        m8_fb16_hot_y = ((short FAR *)shape)[1];
    }
    DIB_SetCursorExt( shape, m8_fb16_dev );
    m8_fb16_send( 0, 0, 0, 0, 1 );
}

/* While the device is BUSY the DIB Engine leaves a move to be drawn later,
 * and during a drawing call the pointer may be lifted out: either way
 * CheckCursor sends it once it is back. */
void m8_fb16_move_cursor( int x, int y )
{
    if( m8_fb16_dev == NULL ) {
        return;
    }
    m8_fb16_cur_x = x;
    m8_fb16_cur_y = y;
    DIB_MoveCursorExt( (WORD)x, (WORD)y, m8_fb16_dev );
    if( (m8_fb16_dev->deFlags & BUSY) || m8_fb16_depth > 0 ) {
        m8_fb16_cur_late = 1;
        return;
    }
    m8_fb16_send( 0, 0, 0, 0, 1 );
}

void m8_fb16_check_cursor( void )
{
    if( m8_fb16_dev == NULL || !m8_fb16_cur_late || m8_fb16_depth > 0 ||
        (m8_fb16_dev->deFlags & BUSY) ) {
        return;
    }
    m8_fb16_cur_late = 0;
    DIB_CheckCursorExt( m8_fb16_dev );
    m8_fb16_send( 0, 0, 0, 0, 1 );
}
