/* Screen drawing for a card with no frame buffer the CPU can reach.
 *
 * Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
 *
 * Under NOT_FRAMEBUFFER the DIB Engine draws memory bitmaps only; every
 * operation on the screen comes here. An operation the engine can do (solid
 * fill, screen copy) goes to the engine. Anything else goes through one
 * fallback: the affected rectangle is read back from the card, the DIB Engine
 * draws it in RAM, and it is written back - in strips through a fixed buffer,
 * so a full-screen operation never needs a full-screen copy in RAM.
 *
 * Each strip reaches the DIB Engine as a temporary DIBENGINE whose bits
 * selector has its base moved back by the strip's top-left offset. Screen
 * coordinates then address the strip directly and no GDI argument needs
 * rewriting; the clip rectangle (or, for BitBlt, the rectangle itself) keeps
 * the DIB Engine inside the strip.
 */

#include "winhack.h"
#include <dos.h>
#include <gdidefs.h>
#include <dibeng.h>
#include "m8.h"

#define M8_BUF_BYTES        0xFFF0u     /* one strip; < 64 KB so one selector covers it */
#define M8_CURSOR_MAX       32
#define M8_CURSOR_SAVE_W    (M8_CURSOR_MAX + 2)     /* even-aligned around a 32-wide cursor */

/* Rectangle, right and bottom exclusive. */
typedef struct {
    int     l;
    int     t;
    int     r;
    int     b;
} M8_RECT;

/* GDI's cursor shape: header, then the AND mask, then the XOR mask. */
typedef struct {
    short   csHotX;
    short   csHotY;
    short   csWidth;
    short   csHeight;
    short   csWidthBytes;
    short   csColor;
} M8_CURSORSHAPE;

/* One strip buffer: RAM, its linear address, and the selector the DIB Engine
 * sees, whose base is moved per strip. */
typedef struct {
    BYTE FAR    *bits;
    DWORD       lin;
    WORD        sel;
    WORD        pitch;
    DIBENGINE   dev;
} M8_STRIP;

DWORD m8_stats[M8_STAT_COUNT];

static LPDIBENGINE  m8_screen;
static M8_STRIP     m8_dst;
static M8_STRIP     m8_src;
static HGLOBAL      m8_dst_mem;
static HGLOBAL      m8_src_mem;
static int          m8_busy;

/* Cursor state. The save buffer holds what is under the cursor while it is
 * shown; the draw buffer is the composed cursor. */
static BYTE     m8_cur_and[M8_CURSOR_MAX * M8_CURSOR_MAX / 8];
static BYTE     m8_cur_xor[M8_CURSOR_MAX * M8_CURSOR_MAX / 8];
static BYTE     m8_cur_save[M8_CURSOR_SAVE_W * M8_CURSOR_MAX];
static BYTE     m8_cur_draw[M8_CURSOR_SAVE_W * M8_CURSOR_MAX];
static int      m8_cur_hot_x;
static int      m8_cur_hot_y;
static int      m8_cur_x;
static int      m8_cur_y;
static int      m8_cur_have_shape;
static int      m8_cur_shown;       /* drawn on the screen now */
static M8_RECT  m8_cur_rect;        /* where m8_cur_save came from */
static int      m8_cur_pending;     /* a move arrived while busy */

/* The strips are plain memory: nothing to exclude, but the pointers must not
 * be null if the DIB Engine calls them. */
static VOID WINAPI __loadds m8_begin_access( LPPDEVICE lpDevice, WORD wLeft, WORD wTop,
                                             WORD wRight, WORD wBottom, WORD wFlags )
{
}

static VOID WINAPI __loadds m8_end_access( LPPDEVICE lpDevice, WORD wFlags )
{
}

/* ------------------------------------------------------------------ rects */

static int m8_rect_empty( const M8_RECT *r )
{
    return( r->l >= r->r || r->t >= r->b );
}

static void m8_rect_and( M8_RECT *r, const M8_RECT *o )
{
    if( o->l > r->l ) r->l = o->l;
    if( o->t > r->t ) r->t = o->t;
    if( o->r < r->r ) r->r = o->r;
    if( o->b < r->b ) r->b = o->b;
}

static void m8_rect_or( M8_RECT *r, const M8_RECT *o )
{
    if( m8_rect_empty( o ) ) {
        return;
    }
    if( m8_rect_empty( r ) ) {
        *r = *o;
        return;
    }
    if( o->l < r->l ) r->l = o->l;
    if( o->t < r->t ) r->t = o->t;
    if( o->r > r->r ) r->r = o->r;
    if( o->b > r->b ) r->b = o->b;
}

static void m8_rect_set( M8_RECT *r, int l, int t, int ri, int b )
{
    r->l = l;
    r->t = t;
    r->r = ri;
    r->b = b;
}

static void m8_rect_screen( M8_RECT *r )
{
    m8_rect_set( r, 0, 0, M8_SCREEN_X, M8_SCREEN_Y );
}

static void m8_rect_from_gdi( M8_RECT *r, LPRECT g )
{
    if( g == NULL ) {
        m8_rect_screen( r );
        return;
    }
    m8_rect_set( r, g->left, g->top, g->right, g->bottom );
}

static int m8_rect_overlap( const M8_RECT *a, const M8_RECT *b )
{
    return( a->l < b->r && b->l < a->r && a->t < b->b && b->t < a->b );
}

/* ------------------------------------------------------------- the strip */

static int m8_is_screen( LPVOID dev )
{
    LPDIBENGINE d = dev;

    return( d != NULL && d->deType == TYPE_DIBENG && (d->deFlags & NOT_FRAMEBUFFER) );
}

/* ROP3 bits: does the result depend on the destination, or on the source? */
static int m8_rop_uses_dest( DWORD rop )
{
    BYTE r = (BYTE)(rop >> 16);

    return( ((r >> 1) & 0x55) != (r & 0x55) );
}

static int m8_rop_uses_src( DWORD rop )
{
    BYTE r = (BYTE)(rop >> 16);

    return( ((r >> 2) & 0x33) != (r & 0x33) );
}

static int m8_strip_alloc( M8_STRIP *s, HGLOBAL *mem )
{
    *mem = GlobalAlloc( GMEM_FIXED | GMEM_SHARE | GMEM_ZEROINIT, M8_BUF_BYTES );
    if( *mem == 0 ) {
        return( 0 );
    }
    s->bits = (BYTE FAR *)GlobalLock( *mem );
    s->lin = GetSelectorBase( FP_SEG( s->bits ) );
    s->sel = AllocSelector( FP_SEG( s->bits ) );
    if( s->sel == 0 ) {
        return( 0 );
    }
    s->dev.deType = TYPE_DIBENG;
    s->dev.deWidth = M8_SCREEN_X;
    s->dev.deHeight = M8_SCREEN_Y;
    s->dev.dePlanes = 1;
    s->dev.deBitsPixel = 8;
    s->dev.deBitsOffset = 0;
    s->dev.deBitsSelector = s->sel;
    s->dev.deVersion = 0x400;
    s->dev.deBeginAccess = (void (WINAPI *)())m8_begin_access;
    s->dev.deEndAccess = (void (WINAPI *)())m8_end_access;
    return( 1 );
}

static void m8_strip_free( M8_STRIP *s, HGLOBAL *mem )
{
    if( s->sel != 0 ) {
        FreeSelector( s->sel );
        s->sel = 0;
    }
    if( *mem != 0 ) {
        GlobalUnlock( *mem );
        GlobalFree( *mem );
        *mem = 0;
    }
}

/* Point the strip's selector at rectangle r: offset y * pitch + x lands in
 * the buffer for every (x, y) inside r. Callers have sized r to fit. */
static void m8_strip_place( M8_STRIP *s, const M8_RECT *r )
{
    DWORD first = (DWORD)r->t * s->pitch + r->l;
    DWORD last = (DWORD)(r->b - 1) * s->pitch + (r->r - 1);

    s->dev.deWidthBytes = s->pitch;
    s->dev.deDeltaScan = s->pitch;
    s->dev.deFlags = m8_screen->deFlags & PALETTIZED;
    s->dev.delpPDevice = (LPBYTE)m8_screen;
    s->dev.deBitmapInfo = m8_screen->deBitmapInfo;
    SetSelectorBase( s->sel, s->lin - first );
    SetSelectorLimit( s->sel, last );
}

/* Even left edge and even width: host transfers move two pixels per word. */
static void m8_rect_align( M8_RECT *r )
{
    r->l &= ~1;
    r->r = (r->r + 1) & ~1;
    if( r->r > M8_SCREEN_X ) {
        r->r = M8_SCREEN_X;
    }
}

/* Rows per strip, sized for a rectangle 4 pixels wider than this one: a
 * screen source strip for the same rows may align 2 pixels wider. */
static int m8_strip_rows( const M8_RECT *r, WORD *pitch )
{
    *pitch = (WORD)((r->r - r->l + 3) & ~3);
    return( (int)(M8_BUF_BYTES / (*pitch + 4)) );
}

static void m8_strip_read( M8_STRIP *s, const M8_RECT *r )
{
    m8_hw_read_rect( r->l, r->t, r->r - r->l, r->b - r->t, s->bits, s->pitch );
    ++m8_stats[M8_STAT_READBACK];
}

static void m8_strip_write( M8_STRIP *s, const M8_RECT *r )
{
    m8_hw_write_rect( r->l, r->t, r->r - r->l, r->b - r->t, s->bits, s->pitch );
}

/* ----------------------------------------------------------------- cursor */

static void m8_cursor_hide( void )
{
    if( m8_cur_shown ) {
        m8_hw_write_rect( m8_cur_rect.l, m8_cur_rect.t, m8_cur_rect.r - m8_cur_rect.l,
                          m8_cur_rect.b - m8_cur_rect.t, m8_cur_save, M8_CURSOR_SAVE_W );
        m8_cur_shown = 0;
    }
}

/* Mono cursor: AND 1 XOR 0 keeps the screen, AND 0 XOR 0 is black,
 * AND 0 XOR 1 is white, AND 1 XOR 1 inverts. */
static void m8_cursor_show( void )
{
    M8_RECT r;
    M8_RECT scr;
    int     x;
    int     y;
    int     cx;
    int     cy;
    int     w;
    int     h;
    int     bit;
    BYTE    p;

    if( !m8_cur_have_shape || m8_cur_shown ) {
        return;
    }
    cx = m8_cur_x - m8_cur_hot_x;
    cy = m8_cur_y - m8_cur_hot_y;
    m8_rect_set( &r, cx, cy, cx + M8_CURSOR_MAX, cy + M8_CURSOR_MAX );
    m8_rect_screen( &scr );
    m8_rect_and( &r, &scr );
    m8_rect_align( &r );
    if( m8_rect_empty( &r ) ) {
        return;
    }
    w = r.r - r.l;
    h = r.b - r.t;
    m8_hw_read_rect( r.l, r.t, w, h, m8_cur_save, M8_CURSOR_SAVE_W );
    for( y = 0; y < h; ++y ) {
        for( x = 0; x < w; ++x ) {
            int sx = r.l + x - cx;
            int sy = r.t + y - cy;

            p = m8_cur_save[y * M8_CURSOR_SAVE_W + x];
            if( sx >= 0 && sx < M8_CURSOR_MAX && sy >= 0 && sy < M8_CURSOR_MAX ) {
                bit = sy * M8_CURSOR_MAX + sx;
                if( !(m8_cur_and[bit >> 3] & (0x80 >> (bit & 7))) ) {
                    p = 0;
                }
                if( m8_cur_xor[bit >> 3] & (0x80 >> (bit & 7)) ) {
                    p ^= 0xFF;
                }
            }
            m8_cur_draw[y * M8_CURSOR_SAVE_W + x] = p;
        }
    }
    m8_hw_write_rect( r.l, r.t, w, h, m8_cur_draw, M8_CURSOR_SAVE_W );
    m8_cur_rect = r;
    m8_cur_shown = 1;
}

/* Every screen operation runs between m8_enter and m8_leave. A cursor move
 * that arrives at interrupt time in between is recorded and drawn at m8_leave,
 * so it never interleaves with an engine command. */
static int m8_enter( const M8_RECT *r )
{
    m8_busy = 1;
    if( m8_cur_shown && (r == NULL || m8_rect_overlap( r, &m8_cur_rect )) ) {
        m8_cursor_hide();
        m8_cur_pending = 1;
    }
    return( 1 );
}

static void m8_leave( void )
{
    if( m8_cur_pending ) {
        m8_cur_pending = 0;
        m8_cursor_hide();
        m8_cursor_show();
    }
    m8_busy = 0;
}

/* ------------------------------------------------------------- fallback */

/* Draws one strip: called with the strip placed, its clip in `clip`. */
typedef DWORD (*M8_DRAW)( void *ctx, LPDIBENGINE dev, LPRECT clip );

/* Run `draw` over `bound` in strips. `read` = the operation depends on what
 * is on the screen (or does not cover all of its rectangle); `write` = it
 * changes the screen. `bottom_up` when a screen source lies above the
 * destination: top-down, a strip would overwrite source rows not yet read. */
static DWORD m8_fallback( const M8_RECT *bound_in, int read, int write, int bottom_up,
                          M8_DRAW draw, void *ctx )
{
    M8_RECT bound = *bound_in;
    M8_RECT scr;
    M8_RECT s;
    RECT    clip;
    WORD    pitch;
    int     rows;
    int     y;
    DWORD   rc = 0;

    m8_rect_screen( &scr );
    m8_rect_and( &bound, &scr );
    if( m8_rect_empty( &bound ) ) {
        return( 0 );
    }
    m8_rect_align( &bound );
    rows = m8_strip_rows( &bound, &pitch );
    m8_dst.pitch = pitch;

    m8_enter( &bound );
    for( y = 0; y < bound.b - bound.t; y += rows ) {
        s = bound;
        if( bottom_up ) {
            s.b = bound.b - y;
            s.t = s.b - rows;
            if( s.t < bound.t ) {
                s.t = bound.t;
            }
        } else {
            s.t = bound.t + y;
            s.b = s.t + rows;
            if( s.b > bound.b ) {
                s.b = bound.b;
            }
        }
        m8_strip_place( &m8_dst, &s );
        if( read ) {
            m8_strip_read( &m8_dst, &s );
        }
        clip.left = s.l;
        clip.top = s.t;
        clip.right = s.r;
        clip.bottom = s.b;
        rc = draw( ctx, &m8_dst.dev, &clip );
        if( write ) {
            m8_strip_write( &m8_dst, &s );
        }
    }
    m8_leave();
    return( rc );
}

/* ------------------------------------------------------------------ BitBlt */

typedef struct {
    int         dx;
    int         dy;
    LPPDEVICE   src;
    int         sx;
    int         sy;
    DWORD       rop;
    LPBRUSH     brush;
    LPDRAWMODE  dm;
    int         src_screen;
} M8_BLT;

/* The strip's clip is the part of the destination to draw this time. */
static DWORD m8_draw_bitblt( void *ctx, LPDIBENGINE dev, LPRECT c )
{
    M8_BLT      *b = ctx;
    int         sx = b->sx + (c->left - b->dx);
    int         sy = b->sy + (c->top - b->dy);
    int         w = c->right - c->left;
    int         h = c->bottom - c->top;
    LPPDEVICE   src = b->src;
    M8_RECT     sr;
    M8_RECT     scr;
    WORD        pitch;

    if( b->src_screen ) {
        m8_rect_set( &sr, sx, sy, sx + w, sy + h );
        m8_rect_screen( &scr );
        m8_rect_and( &sr, &scr );
        m8_rect_align( &sr );
        m8_strip_rows( &sr, &pitch );
        m8_src.pitch = pitch;
        m8_strip_place( &m8_src, &sr );
        m8_strip_read( &m8_src, &sr );
        src = &m8_src.dev;
    }
    return( DIB_BitBlt( dev, c->left, c->top, src, sx, sy, w, h, b->rop, b->brush, b->dm ) );
}

/* A solid colour from a realized brush: the first byte of its 8 bpp pattern,
 * which the DIB Engine renders at the screen's depth. */
static int m8_solid_brush( LPBRUSH brush, BYTE *color )
{
    DIB_Brush8 FAR *b = (DIB_Brush8 FAR *)brush;

    if( b == NULL || b->dp8BrushStyle != 0 || !(b->dp8BrushFlags & COLORSOLID) ) {
        return( 0 );
    }
    *color = b->dp8BrushBits[0];
    return( 1 );
}

BOOL WINAPI __loadds BitBlt( LPPDEVICE lpDestDev, WORD wDestX, WORD wDestY, LPPDEVICE lpSrcDev,
                             WORD wSrcX, WORD wSrcY, WORD wXext, WORD wYext, DWORD dwRop3,
                             LPBRUSH lpPBrush, LPDRAWMODE lpDrawMode )
{
    int         dst_screen = m8_is_screen( lpDestDev );
    int         src_screen = m8_rop_uses_src( dwRop3 ) && m8_is_screen( lpSrcDev );
    M8_BLT      b;
    M8_RECT     r;
    BYTE        color;
    BYTE        rop = (BYTE)(dwRop3 >> 16);
    int         rows;
    WORD        pitch;

    if( !dst_screen && !src_screen ) {
        return( DIB_BitBlt( lpDestDev, wDestX, wDestY, lpSrcDev, wSrcX, wSrcY, wXext, wYext,
                            dwRop3, lpPBrush, lpDrawMode ) );
    }
    ++m8_stats[M8_STAT_BITBLT];

    /* Engine paths. A background palette translation is something the engine
     * cannot apply, so it disqualifies them. */
    if( dst_screen && !(m8_screen->deFlags & PALETTE_XLAT) ) {
        m8_rect_set( &r, wDestX, wDestY, wDestX + wXext, wDestY + wYext );
        if( rop == 0x00 || rop == 0xFF || (rop == 0xF0 && m8_solid_brush( lpPBrush, &color )) ) {
            if( rop == 0x00 ) {
                color = 0;
            } else if( rop == 0xFF ) {
                color = 0xFF;
            }
            m8_enter( &r );
            m8_hw_fill( wDestX, wDestY, wXext, wYext, color );
            m8_leave();
            return( TRUE );
        }
        if( rop == 0xCC && src_screen ) {
            M8_RECT sr;

            m8_rect_set( &sr, wSrcX, wSrcY, wSrcX + wXext, wSrcY + wYext );
            m8_rect_or( &r, &sr );
            m8_enter( &r );
            m8_hw_copy( wSrcX, wSrcY, wDestX, wDestY, wXext, wYext );
            m8_leave();
            return( TRUE );
        }
    }

    b.dx = wDestX;
    b.dy = wDestY;
    b.src = lpSrcDev;
    b.sx = wSrcX;
    b.sy = wSrcY;
    b.rop = dwRop3;
    b.brush = lpPBrush;
    b.dm = lpDrawMode;
    b.src_screen = src_screen;
    m8_rect_set( &r, wDestX, wDestY, wDestX + wXext, wDestY + wYext );

    if( !dst_screen ) {
        /* Screen to memory: read the source in strips, draw into the bitmap. */
        M8_RECT sr;
        int     y;
        int     h;

        m8_rect_set( &sr, wSrcX, wSrcY, wSrcX + wXext, wSrcY + wYext );
        m8_rect_align( &sr );
        rows = m8_strip_rows( &sr, &pitch );
        m8_src.pitch = pitch;
        m8_enter( NULL );
        for( y = 0; y < (int)wYext; y += rows ) {
            h = (int)wYext - y < rows ? (int)wYext - y : rows;
            sr.t = wSrcY + y;
            sr.b = sr.t + h;
            m8_strip_place( &m8_src, &sr );
            m8_strip_read( &m8_src, &sr );
            DIB_BitBlt( lpDestDev, wDestX, wDestY + y, &m8_src.dev, wSrcX, wSrcY + y, wXext, h,
                        dwRop3, lpPBrush, lpDrawMode );
        }
        m8_leave();
        return( TRUE );
    }

    m8_fallback( &r, m8_rop_uses_dest( dwRop3 ), 1,
                 src_screen && wSrcY < wDestY, m8_draw_bitblt, &b );
    return( TRUE );
}

/* ------------------------------------------------------------------ Output */

typedef struct {
    WORD        style;
    WORD        count;
    LPPOINT     pts;
    LPPPEN      pen;
    LPPBRUSH    brush;
    LPDRAWMODE  dm;
} M8_OUT;

static DWORD m8_draw_output( void *ctx, LPDIBENGINE dev, LPRECT c )
{
    M8_OUT *o = ctx;

    return( DIB_Output( dev, o->style, o->count, o->pts, o->pen, o->brush, o->dm, c ) );
}

WORD WINAPI __loadds Output( LPPDEVICE lpDestDev, WORD wStyle, WORD wCount, LPPOINT lpPoints,
                             LPPPEN lpPPen, LPPBRUSH lpPBrush, LPDRAWMODE lpDrawMode,
                             LPRECT lpClipRect )
{
    M8_OUT  o;
    M8_RECT bound;
    M8_RECT pts;
    WORD    i;

    if( !m8_is_screen( lpDestDev ) ) {
        return( DIB_Output( lpDestDev, wStyle, wCount, lpPoints, lpPPen, lpPBrush,
                            lpDrawMode, lpClipRect ) );
    }
    if( wStyle == OS_BEGINNSCAN || wStyle == OS_ENDNSCAN ) {
        return( 1 );
    }
    ++m8_stats[M8_STAT_OUTPUT];
    m8_rect_from_gdi( &bound, lpClipRect );

    /* Shapes drawn inside their points' bounding box: shrink to it, with a
     * margin for the pen. Scanline styles keep the clip rectangle. */
    if( (wStyle == OS_POLYLINE || wStyle == OS_POLYGON || wStyle == OS_WINDPOLYGON ||
         wStyle == OS_RECTANGLE || wStyle == OS_ELLIPSE || wStyle == OS_ARC ||
         wStyle == OS_PIE || wStyle == OS_CHORD || wStyle == OS_POLYBEZIER) && wCount > 0 ) {
        m8_rect_set( &pts, lpPoints[0].xcoord, lpPoints[0].ycoord,
                     lpPoints[0].xcoord, lpPoints[0].ycoord );
        for( i = 1; i < wCount; ++i ) {
            if( lpPoints[i].xcoord < pts.l ) pts.l = lpPoints[i].xcoord;
            if( lpPoints[i].ycoord < pts.t ) pts.t = lpPoints[i].ycoord;
            if( lpPoints[i].xcoord > pts.r ) pts.r = lpPoints[i].xcoord;
            if( lpPoints[i].ycoord > pts.b ) pts.b = lpPoints[i].ycoord;
        }
        pts.l -= 4;
        pts.t -= 4;
        pts.r += 5;
        pts.b += 5;
        m8_rect_and( &bound, &pts );
    }
    o.style = wStyle;
    o.count = wCount;
    o.pts = lpPoints;
    o.pen = lpPPen;
    o.brush = lpPBrush;
    o.dm = lpDrawMode;
    m8_fallback( &bound, 1, 1, 0, m8_draw_output, &o );
    return( 1 );
}

/* ------------------------------------------------------------------- Pixel */

typedef struct {
    int         x;
    int         y;
    DWORD       color;
    LPDRAWMODE  dm;
} M8_PIX;

static DWORD m8_draw_pixel( void *ctx, LPDIBENGINE dev, LPRECT c )
{
    M8_PIX *p = ctx;

    return( DIB_Pixel( dev, p->x, p->y, p->color, p->dm ) );
}

DWORD WINAPI __loadds Pixel( LPPDEVICE lpDestDev, WORD X, WORD Y, DWORD dwPhysColor,
                             LPDRAWMODE lpDrawMode )
{
    M8_PIX  p;
    M8_RECT r;

    if( !m8_is_screen( lpDestDev ) ) {
        return( DIB_Pixel( lpDestDev, X, Y, dwPhysColor, lpDrawMode ) );
    }
    ++m8_stats[M8_STAT_PIXEL];
    p.x = X;
    p.y = Y;
    p.color = dwPhysColor;
    p.dm = lpDrawMode;
    m8_rect_set( &r, X, Y, X + 1, Y + 1 );
    /* No draw mode: a read of the pixel, nothing to write back. */
    return( m8_fallback( &r, 1, lpDrawMode != NULL, 0, m8_draw_pixel, &p ) );
}

/* ------------------------------------------------------------------ ScanLR */

typedef struct {
    int     x;
    int     y;
    DWORD   color;
    WORD    style;
} M8_SCAN;

static DWORD m8_draw_scanlr( void *ctx, LPDIBENGINE dev, LPRECT c )
{
    M8_SCAN *s = ctx;

    return( DIB_ScanLR( dev, s->x, s->y, s->color, s->style ) );
}

WORD WINAPI __loadds ScanLR( LPPDEVICE lpDestDev, WORD X, WORD Y, DWORD dwPhysColor, WORD wStyle )
{
    M8_SCAN s;
    M8_RECT r;

    if( !m8_is_screen( lpDestDev ) ) {
        return( DIB_ScanLR( lpDestDev, X, Y, dwPhysColor, wStyle ) );
    }
    ++m8_stats[M8_STAT_SCANLR];
    s.x = X;
    s.y = Y;
    s.color = dwPhysColor;
    s.style = wStyle;
    /* The scan may run to either edge: read the whole row. */
    m8_rect_set( &r, 0, Y, M8_SCREEN_X, Y + 1 );
    return( (WORD)m8_fallback( &r, 1, 0, 0, m8_draw_scanlr, &s ) );
}

/* ------------------------------------------------------------------- text */

typedef struct {
    int             x;
    int             y;
    LPRECT          clip;
    LPSTR           str;
    int             count;
    LPFONTINFO      font;
    LPDRAWMODE      dm;
    LPTEXTXFORM     xform;
    LPSHORT         widths;
    LPRECT          opaque;
    WORD            options;
} M8_TEXT;

/* The strip's clip, intersected with GDI's. */
static void m8_text_clip( M8_TEXT *t, LPRECT c, RECT *out )
{
    *out = *c;
    if( t->clip != NULL ) {
        if( t->clip->left > out->left ) out->left = t->clip->left;
        if( t->clip->top > out->top ) out->top = t->clip->top;
        if( t->clip->right < out->right ) out->right = t->clip->right;
        if( t->clip->bottom < out->bottom ) out->bottom = t->clip->bottom;
    }
}

static DWORD m8_draw_exttextout( void *ctx, LPDIBENGINE dev, LPRECT c )
{
    M8_TEXT *t = ctx;
    RECT    clip;

    m8_text_clip( t, c, &clip );
    if( clip.left >= clip.right || clip.top >= clip.bottom ) {
        return( 0 );
    }
    return( DIB_ExtTextOut( dev, t->x, t->y, &clip, t->str, t->count, t->font, t->dm,
                            t->xform, t->widths, t->opaque, t->options ) );
}

static DWORD m8_draw_strblt( void *ctx, LPDIBENGINE dev, LPRECT c )
{
    M8_TEXT *t = ctx;
    RECT    clip;

    m8_text_clip( t, c, &clip );
    if( clip.left >= clip.right || clip.top >= clip.bottom ) {
        return( 0 );
    }
    return( DIB_StrBlt( dev, t->x, t->y, &clip, t->str, t->count, t->font, t->dm, t->xform ) );
}

/* The rows the text can touch: from y down by the font's height, with a margin,
 * across the clip; joined with the opaque rectangle and clipped. A measuring
 * call (negative count) is no guide - it returned a 2-row height for icon
 * labels. Rotated text, or a height that makes no sense, takes the whole clip. */
static void m8_text_bound( M8_TEXT *t, M8_RECT *bound )
{
    M8_RECT box;
    M8_RECT op;
    int     h = 0;

    if( t->font != NULL ) {
        h = t->font->dfPixHeight;
    }
    if( t->xform != NULL && t->xform->ftHeight > h ) {
        h = t->xform->ftHeight;
    }
    m8_rect_screen( &box );
    if( h > 0 && h < M8_SCREEN_Y && (t->xform == NULL || t->xform->ftEscapement == 0) ) {
        box.t = t->y - 4;
        box.b = t->y + h + 4;
    }
    if( t->opaque != NULL ) {
        m8_rect_from_gdi( &op, t->opaque );
        m8_rect_or( &box, &op );
    }
    *bound = box;
    if( t->clip != NULL ) {
        m8_rect_from_gdi( &op, t->clip );
        m8_rect_and( bound, &op );
    }
}

DWORD WINAPI __loadds ExtTextOut( LPPDEVICE lpDestDev, WORD wDestXOrg, WORD wDestYOrg,
                                  LPRECT lpClipRect, LPSTR lpString, int wCount,
                                  LPFONTINFO lpFontInfo, LPDRAWMODE lpDrawMode,
                                  LPTEXTXFORM lpTextXForm, LPSHORT lpCharWidths,
                                  LPRECT lpOpaqueRect, WORD wOptions )
{
    M8_TEXT t;
    M8_RECT bound;

    if( !m8_is_screen( lpDestDev ) ) {
        return( DIB_ExtTextOut( lpDestDev, wDestXOrg, wDestYOrg, lpClipRect, lpString, wCount,
                                lpFontInfo, lpDrawMode, lpTextXForm, lpCharWidths,
                                lpOpaqueRect, wOptions ) );
    }
    /* A negative count only measures: no pixels, so any surface will do. */
    if( wCount < 0 ) {
        return( DIB_ExtTextOut( &m8_dst.dev, wDestXOrg, wDestYOrg, lpClipRect, lpString, wCount,
                                lpFontInfo, lpDrawMode, lpTextXForm, lpCharWidths, NULL,
                                wOptions ) );
    }
    /* A zero count with an opaque rectangle is a fill and still draws. */
    if( wCount == 0 && lpOpaqueRect == NULL ) {
        return( 1 );
    }
    ++m8_stats[M8_STAT_EXTTEXTOUT];
    t.x = wDestXOrg;
    t.y = wDestYOrg;
    t.clip = lpClipRect;
    t.str = lpString;
    t.count = wCount;
    t.font = lpFontInfo;
    t.dm = lpDrawMode;
    t.xform = lpTextXForm;
    t.widths = lpCharWidths;
    t.opaque = lpOpaqueRect;
    t.options = wOptions;
    m8_text_bound( &t, &bound );
    m8_fallback( &bound, 1, 1, 0, m8_draw_exttextout, &t );
    return( 1 );
}

DWORD WINAPI __loadds StrBlt( LPPDEVICE lpDestDev, WORD wDestXOrg, WORD wDestYOrg,
                              LPRECT lpClipRect, LPSTR lpString, WORD wCount,
                              LPFONTINFO lpFontInfo, LPDRAWMODE lpDrawMode,
                              LPTEXTXFORM lpTextXForm )
{
    M8_TEXT t;
    M8_RECT bound;

    if( !m8_is_screen( lpDestDev ) ) {
        return( DIB_StrBlt( lpDestDev, wDestXOrg, wDestYOrg, lpClipRect, lpString, wCount,
                            lpFontInfo, lpDrawMode, lpTextXForm ) );
    }
    if( (int)wCount <= 0 ) {
        return( DIB_StrBlt( &m8_dst.dev, wDestXOrg, wDestYOrg, lpClipRect, lpString, wCount,
                            lpFontInfo, lpDrawMode, lpTextXForm ) );
    }
    ++m8_stats[M8_STAT_STRBLT];
    t.x = wDestXOrg;
    t.y = wDestYOrg;
    t.clip = lpClipRect;
    t.str = lpString;
    t.count = wCount;
    t.font = lpFontInfo;
    t.dm = lpDrawMode;
    t.xform = lpTextXForm;
    t.opaque = NULL;
    m8_text_bound( &t, &bound );
    m8_fallback( &bound, 1, 1, 0, m8_draw_strblt, &t );
    return( 1 );
}

/* ------------------------------------------------------ DIBs to the screen */

typedef struct {
    int             x;
    int             y;
    WORD            scan;
    WORD            scans;
    LPRECT          clip;
    LPDRAWMODE      dm;
    LPSTR           bits;
    LPBITMAPINFO    info;
    LPINT           xlat;
} M8_D2D;

static DWORD m8_draw_dibtodev( void *ctx, LPDIBENGINE dev, LPRECT c )
{
    M8_D2D  *d = ctx;
    RECT    clip = *c;

    if( d->clip != NULL ) {
        if( d->clip->left > clip.left ) clip.left = d->clip->left;
        if( d->clip->top > clip.top ) clip.top = d->clip->top;
        if( d->clip->right < clip.right ) clip.right = d->clip->right;
        if( d->clip->bottom < clip.bottom ) clip.bottom = d->clip->bottom;
    }
    return( DIB_DibToDevice( dev, d->x, d->y, d->scan, d->scans, &clip, d->dm, d->bits,
                             d->info, d->xlat ) );
}

WORD WINAPI __loadds DibToDevice( LPPDEVICE lpDestDev, WORD X, WORD Y, WORD iScan, WORD cScans,
                                  LPRECT lpClipRect, LPDRAWMODE lpDrawMode, LPSTR lpDIBits,
                                  LPBITMAPINFO lpBitmapInfo, LPINT lpTranslate )
{
    M8_D2D  d;
    M8_RECT bound;

    if( !m8_is_screen( lpDestDev ) ) {
        return( DIB_DibToDevice( lpDestDev, X, Y, iScan, cScans, lpClipRect, lpDrawMode,
                                 lpDIBits, lpBitmapInfo, lpTranslate ) );
    }
    ++m8_stats[M8_STAT_DIBTODEV];
    d.x = X;
    d.y = Y;
    d.scan = iScan;
    d.scans = cScans;
    d.clip = lpClipRect;
    d.dm = lpDrawMode;
    d.bits = lpDIBits;
    d.info = lpBitmapInfo;
    d.xlat = lpTranslate;
    m8_rect_from_gdi( &bound, lpClipRect );
    m8_fallback( &bound, 1, 1, 0, m8_draw_dibtodev, &d );
    return( cScans );
}

typedef struct {
    WORD            get;
    int             dx;
    int             dy;
    int             dw;
    int             dh;
    int             sx;
    int             sy;
    int             sw;
    int             sh;
    VOID            *bits;
    LPBITMAPINFO    info;
    LPINT           xlat;
    DWORD           rop;
    LPBRUSH         brush;
    LPDRAWMODE      dm;
    LPRECT          clip;
} M8_SDIB;

static DWORD m8_draw_stretchdib( void *ctx, LPDIBENGINE dev, LPRECT c )
{
    M8_SDIB *s = ctx;
    RECT    clip = *c;

    if( s->clip != NULL ) {
        if( s->clip->left > clip.left ) clip.left = s->clip->left;
        if( s->clip->top > clip.top ) clip.top = s->clip->top;
        if( s->clip->right < clip.right ) clip.right = s->clip->right;
        if( s->clip->bottom < clip.bottom ) clip.bottom = s->clip->bottom;
    }
    return( DIB_StretchDIBits( dev, s->get, s->dx, s->dy, s->dw, s->dh, s->sx, s->sy, s->sw,
                               s->sh, s->bits, s->info, s->xlat, s->rop, s->brush, s->dm,
                               &clip ) );
}

BOOL WINAPI __loadds StretchDIBits( LPPDEVICE lpDestDev, WORD fGet, WORD wDestX, WORD wDestY,
                                    WORD wDestWidth, WORD wDestHeight, WORD wSrcX, WORD wSrcY,
                                    WORD wSrcWidth, WORD wSrcHeight, VOID *lpBits,
                                    LPBITMAPINFO lpInfo, LPINT lpTranslate, DWORD dwRop3,
                                    LPBRUSH lpPBrush, LPDRAWMODE lpDrawMode, LPRECT lpClipRect )
{
    M8_SDIB s;
    M8_RECT bound;
    M8_RECT d;
    int     w = (int)wDestWidth;
    int     h = (int)wDestHeight;

    if( !m8_is_screen( lpDestDev ) ) {
        return( DIB_StretchDIBits( lpDestDev, fGet, wDestX, wDestY, wDestWidth, wDestHeight,
                                   wSrcX, wSrcY, wSrcWidth, wSrcHeight, lpBits, lpInfo,
                                   lpTranslate, dwRop3, lpPBrush, lpDrawMode, lpClipRect ) );
    }
    ++m8_stats[M8_STAT_STRETCHDIB];
    s.get = fGet;
    s.dx = wDestX;
    s.dy = wDestY;
    s.dw = wDestWidth;
    s.dh = wDestHeight;
    s.sx = wSrcX;
    s.sy = wSrcY;
    s.sw = wSrcWidth;
    s.sh = wSrcHeight;
    s.bits = lpBits;
    s.info = lpInfo;
    s.xlat = lpTranslate;
    s.rop = dwRop3;
    s.brush = lpPBrush;
    s.dm = lpDrawMode;
    s.clip = lpClipRect;
    /* Negative extents mirror; the box is the same either way. */
    m8_rect_set( &d, (int)wDestX, (int)wDestY, (int)wDestX + w, (int)wDestY + h );
    if( w < 0 ) {
        d.l = (int)wDestX + w + 1;
        d.r = (int)wDestX + 1;
    }
    if( h < 0 ) {
        d.t = (int)wDestY + h + 1;
        d.b = (int)wDestY + 1;
    }
    m8_rect_from_gdi( &bound, lpClipRect );
    m8_rect_and( &bound, &d );
    /* Always read: a clipped or mirrored stretch need not cover its box. */
    m8_fallback( &bound, 1, !fGet, 0, m8_draw_stretchdib, &s );
    return( TRUE );
}

/* -------------------------------------------------------------- StretchBlt */

typedef struct {
    int         dx;
    int         dy;
    int         dw;
    int         dh;
    LPPDEVICE   src;
    int         sx;
    int         sy;
    int         sw;
    int         sh;
    DWORD       rop;
    LPBRUSH     brush;
    LPDRAWMODE  dm;
    LPRECT      clip;
} M8_SBLT;

static DWORD m8_draw_stretchblt( void *ctx, LPDIBENGINE dev, LPRECT c )
{
    M8_SBLT *s = ctx;
    RECT    clip = *c;

    if( s->clip != NULL ) {
        if( s->clip->left > clip.left ) clip.left = s->clip->left;
        if( s->clip->top > clip.top ) clip.top = s->clip->top;
        if( s->clip->right < clip.right ) clip.right = s->clip->right;
        if( s->clip->bottom < clip.bottom ) clip.bottom = s->clip->bottom;
    }
    return( DIB_StretchBlt( dev, s->dx, s->dy, s->dw, s->dh, s->src, s->sx, s->sy, s->sw,
                            s->sh, s->rop, s->brush, s->dm, &clip ) );
}

BOOL WINAPI __loadds StretchBlt( LPPDEVICE lpDestDev, WORD wDestX, WORD wDestY, WORD wDestWidth,
                                 WORD wDestHeight, LPPDEVICE lpSrcDev, WORD wSrcX, WORD wSrcY,
                                 WORD wSrcWidth, WORD wSrcHeight, DWORD dwRop3,
                                 LPBRUSH lpPBrush, LPDRAWMODE lpDrawMode, LPRECT lpClipRect )
{
    M8_SBLT s;
    M8_RECT bound;
    M8_RECT d;
    int     w = (int)wDestWidth;
    int     h = (int)wDestHeight;

    /* A screen source would need the whole source rectangle in RAM at once;
     * GDI falls back to its own simulation when this returns FALSE. */
    if( m8_rop_uses_src( dwRop3 ) && m8_is_screen( lpSrcDev ) ) {
        return( FALSE );
    }
    if( !m8_is_screen( lpDestDev ) ) {
        return( DIB_StretchBlt( lpDestDev, wDestX, wDestY, wDestWidth, wDestHeight, lpSrcDev,
                                wSrcX, wSrcY, wSrcWidth, wSrcHeight, dwRop3, lpPBrush,
                                lpDrawMode, lpClipRect ) );
    }
    ++m8_stats[M8_STAT_STRETCHBLT];
    s.dx = wDestX;
    s.dy = wDestY;
    s.dw = wDestWidth;
    s.dh = wDestHeight;
    s.src = lpSrcDev;
    s.sx = wSrcX;
    s.sy = wSrcY;
    s.sw = wSrcWidth;
    s.sh = wSrcHeight;
    s.rop = dwRop3;
    s.brush = lpPBrush;
    s.dm = lpDrawMode;
    s.clip = lpClipRect;
    m8_rect_set( &d, (int)wDestX, (int)wDestY, (int)wDestX + w, (int)wDestY + h );
    if( w < 0 ) {
        d.l = (int)wDestX + w + 1;
        d.r = (int)wDestX + 1;
    }
    if( h < 0 ) {
        d.t = (int)wDestY + h + 1;
        d.b = (int)wDestY + 1;
    }
    m8_rect_from_gdi( &bound, lpClipRect );
    m8_rect_and( &bound, &d );
    m8_fallback( &bound, 1, 1, 0, m8_draw_stretchblt, &s );
    return( TRUE );
}

/* --------------------------------------------------- FastBorder, UpdateColors */

/* Zero hands the border back to GDI, which draws it with BitBlt. */
WORD WINAPI __loadds FastBorder( LPRECT lpRect, WORD wHorizBorder, WORD wVertBorder,
                                 DWORD dwRop3, LPPDEVICE lpDestDev, LPBRUSH lpPBrush,
                                 LPDRAWMODE lpDrawMode, LPRECT lpClipRect )
{
    ++m8_stats[M8_STAT_FASTBORDER];
    return( 0 );
}

typedef struct {
    LPWORD  xlat;
} M8_UPD;

static DWORD m8_draw_updatecolors( void *ctx, LPDIBENGINE dev, LPRECT c )
{
    M8_UPD *u = ctx;

    DIB_UpdateColorsExt( c->left, c->top, c->right - c->left, c->bottom - c->top, u->xlat,
                         dev );
    return( 0 );
}

VOID WINAPI __loadds UpdateColors( WORD wStartX, WORD wStartY, WORD wExtX, WORD wExtY,
                                   LPWORD lpTranslate )
{
    M8_UPD  u;
    M8_RECT r;

    ++m8_stats[M8_STAT_UPDATECOL];
    u.xlat = lpTranslate;
    m8_rect_set( &r, wStartX, wStartY, wStartX + wExtX, wStartY + wExtY );
    m8_fallback( &r, 1, 1, 0, m8_draw_updatecolors, &u );
}

/* ------------------------------------------------------------- the cursor */

VOID WINAPI __loadds M8SetCursor( M8_CURSORSHAPE FAR *lpShape )
{
    BYTE FAR    *and_mask;
    BYTE FAR    *xor_mask;
    int         y;
    int         x;
    int         wb;

    m8_busy = 1;
    m8_cursor_hide();
    m8_cur_have_shape = 0;
    if( lpShape != NULL && lpShape->csWidth <= M8_CURSOR_MAX && lpShape->csHeight <= M8_CURSOR_MAX ) {
        wb = lpShape->csWidthBytes;
        and_mask = (BYTE FAR *)(lpShape + 1);
        xor_mask = and_mask + wb * lpShape->csHeight;
        for( y = 0; y < M8_CURSOR_MAX; ++y ) {
            for( x = 0; x < M8_CURSOR_MAX / 8; ++x ) {
                int i = y * (M8_CURSOR_MAX / 8) + x;

                if( y < lpShape->csHeight && x < wb ) {
                    m8_cur_and[i] = and_mask[y * wb + x];
                    m8_cur_xor[i] = xor_mask[y * wb + x];
                } else {
                    m8_cur_and[i] = 0xFF;
                    m8_cur_xor[i] = 0;
                }
            }
        }
        m8_cur_hot_x = lpShape->csHotX;
        m8_cur_hot_y = lpShape->csHotY;
        m8_cur_have_shape = 1;
        m8_cursor_show();
    }
    m8_busy = 0;
}

VOID WINAPI __loadds MoveCursor( WORD wAbsX, WORD wAbsY )
{
    m8_cur_x = wAbsX;
    m8_cur_y = wAbsY;
    if( m8_busy || m8_screen == NULL ) {
        m8_cur_pending = 1;
        return;
    }
    m8_busy = 1;
    m8_cursor_hide();
    m8_cursor_show();
    m8_busy = 0;
}

VOID WINAPI __loadds CheckCursor( VOID )
{
    if( m8_busy || !m8_cur_pending || m8_screen == NULL ) {
        return;
    }
    m8_busy = 1;
    m8_cur_pending = 0;
    m8_cursor_hide();
    m8_cursor_show();
    m8_busy = 0;
}

/* ------------------------------------------------------------ set up/down */

int m8_screen_init( LPDIBENGINE screen )
{
    M8_RECT one;

    m8_screen = screen;
    if( m8_dst.sel == 0 && !m8_strip_alloc( &m8_dst, &m8_dst_mem ) ) {
        return( 0 );
    }
    if( m8_src.sel == 0 && !m8_strip_alloc( &m8_src, &m8_src_mem ) ) {
        return( 0 );
    }
    /* Text measuring calls use m8_dst without a strip: give it a valid one. */
    m8_dst.pitch = 4;
    m8_rect_set( &one, 0, 0, 1, 1 );
    m8_strip_place( &m8_dst, &one );
    return( 1 );
}

void m8_screen_done( void )
{
    m8_busy = 1;
    m8_cursor_hide();
    m8_screen = NULL;
    m8_strip_free( &m8_dst, &m8_dst_mem );
    m8_strip_free( &m8_src, &m8_src_mem );
    m8_busy = 0;
}
