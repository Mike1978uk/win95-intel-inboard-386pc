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

#include "m8ddi.h"
#include <dos.h>
#include <conio.h>
#include <string.h>
#include "m8.h"

#define M8_BUF_BYTES        0xFFF0u     /* one strip; < 64 KB so one selector covers it */
#define M8_CURSOR_MAX       32
#define M8_PATTERN_EXPANSION 1     /* two-colour patterns by mono expansion: see BitBlt */
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

/* A second copy of SaveScreenBitmap's stored rectangle, kept away from the
 * first: on 10-16 the first was found overwritten while a menu was open. A
 * mismatch is reported once in the trace and refuses the restore. */
static M8_RECT  m8_ssb_check;
static int      m8_ssb_reported;

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
static int      m8_cur_soft;        /* shown by the software path: m8_cur_save holds it */
static M8_RECT  m8_cur_rect;        /* where m8_cur_save came from */
static int      m8_cur_pending;     /* a move arrived while busy */

/* Surface access callbacks for the strips and the screen. The strips are plain
 * memory and the screen has no frame buffer: nothing to do, but the pointers
 * must not be null when the DIB Engine calls them. */
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

    s->dev.deWidth = M8_SCREEN_X;
    s->dev.deHeight = M8_SCREEN_Y;
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

/* Only the columns alignment added: the operation overwrites the rest. */
static void m8_strip_read_edges( M8_STRIP *s, const M8_RECT *r, const M8_RECT *area )
{
    int h = r->b - r->t;

    if( r->l != area->l ) {
        m8_hw_read_rect( r->l, r->t, 2, h, s->bits, s->pitch );
    }
    if( r->r != area->r ) {
        m8_hw_read_rect( r->r - 2, r->t, 2, h, s->bits + (r->r - 2 - r->l), s->pitch );
    }
    ++m8_stats[M8_STAT_READBACK];
}

/* Write-back, encoded: a raw upload costs 3.84 us per pixel plus ~35 us of
 * set-up, a fill ~35 us whatever its size (M8TIME, 5160). A run pays as a fill
 * from about 20 pixels in mid-row, where it also splits the raw span around it.
 * Rows without such a run go out together as one raw rectangle, as before. */
#define M8_RUN_MIN          24

/* The first run in a row long enough to fill, on even edges: 1 and [*a, *b). */
static int m8_row_run( const BYTE FAR *row, int from, int w, int *a, int *b )
{
    int i = from;
    int j;

    while( i < w ) {
        for( j = i + 1; j < w && row[j] == row[i]; ++j ) {
        }
        *a = (i + 1) & ~1;
        *b = j & ~1;
        if( *b - *a >= M8_RUN_MIN ) {
            return( 1 );
        }
        i = j;
    }
    return( 0 );
}

static int m8_row_uniform( const BYTE FAR *row, int w )
{
    int i;

    for( i = 1; i < w; ++i ) {
        if( row[i] != row[0] ) {
            return( 0 );
        }
    }
    return( 1 );
}

static void m8_strip_write( M8_STRIP *s, const M8_RECT *r )
{
    int         w = r->r - r->l;
    int         h = r->b - r->t;
    int         y;
    int         k;
    int         plain = -1;     /* first row of a pending raw block */
    int         a;
    int         b;
    int         at;
    BYTE FAR    *row;

    for( y = 0; y < h; ++y ) {
        row = s->bits + (DWORD)y * s->pitch;
        if( !m8_row_run( row, 0, w, &a, &b ) ) {
            if( plain < 0 ) {
                plain = y;
            }
            continue;
        }
        if( plain >= 0 ) {
            m8_hw_write_rect( r->l, r->t + plain, w, y - plain,
                              s->bits + (DWORD)plain * s->pitch, s->pitch );
            plain = -1;
        }
        /* One colour across: one fill, over every following row alike. */
        if( m8_row_uniform( row, w ) ) {
            for( k = y + 1; k < h; ++k ) {
                const BYTE FAR *next = s->bits + (DWORD)k * s->pitch;

                if( next[0] != row[0] || !m8_row_uniform( next, w ) ) {
                    break;
                }
            }
            m8_hw_fill( r->l, r->t + y, w, k - y, row[0] );
            ++m8_stats[M8_STAT_RUN_FILL];
            y = k - 1;
            continue;
        }
        /* Raw pieces between the runs. */
        at = 0;
        do {
            if( a > at ) {
                m8_hw_write_rect( r->l + at, r->t + y, a - at, 1, row + at, s->pitch );
            }
            m8_hw_fill( r->l + a, r->t + y, b - a, 1, row[a] );
            ++m8_stats[M8_STAT_RUN_FILL];
            at = b;
        } while( m8_row_run( row, at, w, &a, &b ) );
        if( at < w ) {
            m8_hw_write_rect( r->l + at, r->t + y, w - at, 1, row + at, s->pitch );
        }
    }
    if( plain >= 0 ) {
        m8_hw_write_rect( r->l, r->t + plain, w, h - plain,
                          s->bits + (DWORD)plain * s->pitch, s->pitch );
    }
}

/* ----------------------------------------------------------------- cursor */

static void m8_cursor_hide( void )
{
    if( !m8_cur_shown ) {
        return;
    }
    if( m8_cur_soft ) {
        m8_hw_write_rect( m8_cur_rect.l, m8_cur_rect.t, m8_cur_rect.r - m8_cur_rect.l,
                          m8_cur_rect.b - m8_cur_rect.t, m8_cur_save, M8_CURSOR_SAVE_W );
    } else {
        M8_STEP( M8_MARK_CURSOR_HIDE );
        M8_STEP( m8_cur_rect.l );
        M8_STEP( m8_cur_rect.t );
        m8_hw_copy( M8_CURSOR_SAVE_X, M8_CURSOR_SAVE_Y, m8_cur_rect.l, m8_cur_rect.t,
                    m8_cur_rect.r - m8_cur_rect.l, m8_cur_rect.b - m8_cur_rect.t );
        m8_hw_wait_idle();
    }
    m8_cur_shown = 0;
}

/* Mono cursor: AND 1 XOR 0 keeps the screen, AND 0 XOR 0 is black,
 * AND 0 XOR 1 is white, AND 1 XOR 1 inverts. Index 0 is black and XOR FFh
 * maps each static colour to its inverse in the default palette. */

/* Software: read back, compose in RAM, write back. Kept for a cursor hanging
 * off the top or left edge, where the engine would need negative coordinates. */
static void m8_cursor_show_soft( int cx, int cy )
{
    M8_RECT r;
    M8_RECT scr;
    int     x;
    int     y;
    int     w;
    int     h;
    int     bit;
    BYTE    p;

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
    m8_cur_soft = 1;
    m8_cur_shown = 1;
}

/* Engine: what is under the cursor is copied to off-screen memory, then the
 * two masks are expanded over it. Only the 256 bytes of mask cross the bus. */
static void m8_cursor_show( void )
{
    M8_RECT r;
    M8_RECT scr;
    int     cx;
    int     cy;

    if( !m8_cur_have_shape || m8_cur_shown ) {
        return;
    }
    cx = m8_cur_x - m8_cur_hot_x;
    cy = m8_cur_y - m8_cur_hot_y;
    if( cx < 0 || cy < 0 ) {
        m8_cursor_show_soft( cx, cy );
        return;
    }
    m8_rect_set( &r, cx, cy, cx + M8_CURSOR_MAX, cy + M8_CURSOR_MAX );
    m8_rect_screen( &scr );
    m8_rect_and( &r, &scr );
    if( m8_rect_empty( &r ) ) {
        return;
    }
    M8_STEP( M8_MARK_CURSOR_SHOW );
    M8_STEP( r.l );
    M8_STEP( r.t );
    m8_hw_copy( r.l, r.t, M8_CURSOR_SAVE_X, M8_CURSOR_SAVE_Y, r.r - r.l, r.b - r.t );
    m8_hw_mono( cx, cy, M8_CURSOR_MAX / 8, M8_CURSOR_MAX, m8_cur_and,
                M8_MIX_LEAVE, 0, M8_MIX_SRC_BKGD | M8_MIX_REPLACE, 0,
                r.l, r.t, r.r - 1, r.b - 1 );
    m8_hw_mono( cx, cy, M8_CURSOR_MAX / 8, M8_CURSOR_MAX, m8_cur_xor,
                M8_MIX_SRC_FRGD | M8_MIX_XOR, 0xFF, M8_MIX_LEAVE, 0,
                r.l, r.t, r.r - 1, r.b - 1 );
    m8_cur_rect = r;
    m8_cur_soft = 0;
    m8_cur_shown = 1;
}

/* Lift the cursor if it overlaps `r`; it comes back at m8_leave. */
static void m8_exclude( const M8_RECT *r )
{
    if( m8_cur_shown && m8_rect_overlap( r, &m8_cur_rect ) ) {
        m8_cursor_hide();
        m8_cur_pending = 1;
    }
}

/* Every screen operation runs between m8_enter and m8_leave. A cursor move
 * that arrives at interrupt time in between is recorded and drawn at m8_leave,
 * so it never interleaves with an engine command. */
static void m8_ssb_verify( void );

static int m8_enter( const M8_RECT *r )
{
    m8_ssb_verify();
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
 * is on the screen (or does not cover all of its rectangle); M8_READ_IF_UNCOVERED
 * = it should cover it but the driver cannot tell which rows it draws (see
 * m8_covered); `write` = it changes the screen. `bottom_up` when a screen source lies above the
 * destination: top-down, a strip would overwrite source rows not yet read. */
#define M8_READ_IF_UNCOVERED 2

/* Draw the strip twice, into buffers pre-filled 00h and FFh: a pixel equal in
 * both was drawn, one that differs was not. 1 if every pixel of `area`'s
 * columns was drawn - then the screen need not be read. The second draw is in
 * RAM only. */
static int m8_covered( M8_DRAW draw, void *ctx, const M8_RECT *s, const M8_RECT *area,
                       LPRECT clip )
{
    DWORD       n = (DWORD)(s->b - s->t) * m8_dst.pitch;
    int         y;
    int         x;
    BYTE FAR    *p;
    BYTE FAR    *q;

    m8_src.pitch = m8_dst.pitch;
    m8_strip_place( &m8_src, s );
    _fmemset( m8_dst.bits, 0x00, (size_t)n );
    _fmemset( m8_src.bits, 0xFF, (size_t)n );
    draw( ctx, &m8_dst.dev, clip );
    draw( ctx, &m8_src.dev, clip );
    for( y = 0; y < s->b - s->t; ++y ) {
        p = m8_dst.bits + (DWORD)y * m8_dst.pitch;
        q = m8_src.bits + (DWORD)y * m8_dst.pitch;
        for( x = area->l - s->l; x < area->r - s->l; ++x ) {
            if( p[x] != q[x] ) {
                return( 0 );
            }
        }
    }
    return( 1 );
}

static DWORD m8_fallback( const M8_RECT *bound_in, int read, int write, int bottom_up,
                          M8_DRAW draw, void *ctx )
{
    M8_RECT bound = *bound_in;
    M8_RECT area;
    M8_RECT scr;
    M8_RECT s;
    RECT    clip;
    WORD    pitch;
    int     rows;
    int     y;
    int     edges = 0;
    DWORD   rc = 0;

    m8_rect_screen( &scr );
    m8_rect_and( &bound, &scr );
    if( m8_rect_empty( &bound ) ) {
        return( 0 );
    }
    /* Widening to even edges brings in columns the operation must not draw:
     * the DIB Engine's clip stays the unwidened `area`, and the extra columns
     * are read first so they go back to the card unchanged. */
    area = bound;
    m8_rect_align( &bound );
    if( !read && (bound.l != area.l || bound.r != area.r) ) {
        edges = 1;
    }
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
        clip.left = area.l;
        clip.top = s.t;
        clip.right = area.r;
        clip.bottom = s.b;
        if( read == M8_READ_IF_UNCOVERED && m8_covered( draw, ctx, &s, &area, &clip ) ) {
            /* Drawn already, into m8_dst. The alignment columns come from the
             * screen, but an edge read takes two columns and the inner one is
             * the DIB's own: read into m8_src and copy only the outer one (the
             * inner one was lost when this read went straight into m8_dst -
             * Explorer's folder icons without their left edge, 10-16). */
            if( bound.l != area.l || bound.r != area.r ) {
                int         row;
                int         last = (s.r - s.l) - 1;
                BYTE FAR    *d;
                BYTE FAR    *e;

                m8_strip_read_edges( &m8_src, &s, &area );
                for( row = 0; row < s.b - s.t; ++row ) {
                    d = m8_dst.bits + (DWORD)row * m8_dst.pitch;
                    e = m8_src.bits + (DWORD)row * m8_dst.pitch;
                    if( bound.l != area.l ) {
                        d[0] = e[0];
                    }
                    if( bound.r != area.r ) {
                        d[last] = e[last];
                    }
                }
            }
            ++m8_stats[M8_STAT_COVERED];
        } else {
            if( read ) {
                m8_strip_read( &m8_dst, &s );
            } else if( edges ) {
                m8_strip_read_edges( &m8_dst, &s, &area );
            }
            rc = draw( ctx, &m8_dst.dev, &clip );
        }
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

#ifdef M8_TRACE
/* Source kind for the trace: 0 none, 1 screen, 2 DIB Engine bitmap, 3 plain
 * BITMAP (GDI's monochrome bitmaps). */
static WORD m8_src_kind( LPPDEVICE src, int src_screen )
{
    if( src == NULL ) {
        return( 0 );
    }
    if( src_screen ) {
        return( 1 );
    }
    return( ((LPDIBENGINE)src)->deType == TYPE_DIBENG ? 2 : 3 );
}
#endif

/* A ROP3 with no source is a 2-input function of pattern and destination,
 * applied to each colour bit; the 8514/A's 16 mixes are exactly the 16 such
 * functions, with the pattern colour as the mix source. Key: the ROP's result
 * for (P,D) = (0,0), (0,1), (1,0), (1,1) in bits 0-3; value: the mix (the
 * 86Box model's MIX table, card-checked by M8MIX). */
static const BYTE m8_mix_for_key[16] = {
    0x1, 0xF, 0xE, 0x4, 0xD, 0x0, 0x5, 0x8, 0xC, 0x6, 0x3, 0x9, 0x7, 0xA, 0xB, 0x2
};

static WORD m8_rop_mix( BYTE rop )
{
    int key = (rop & 1) | (((rop >> 1) & 1) << 1) | (((rop >> 4) & 1) << 2) |
              (((rop >> 5) & 1) << 3);

    return( m8_mix_for_key[key] );
}

static int m8_rop_uses_pat( DWORD rop )
{
    BYTE r = (BYTE)(rop >> 16);

    return( ((r >> 4) & 0x0F) != (r & 0x0F) );
}

/* A realized brush as at most two colours: returns 0 if it is not, 1 for a
 * solid colour (c1), 2 for a pattern - pat[row] has bit 7 for column 0, a 1
 * bit is c1 and a 0 bit c2. The pattern is what the DIB Engine itself paints:
 * an 8x8 PATCOPY into a scratch bitmap in RAM (no bus traffic). Reading the
 * realized brush directly failed - dp8BrushMono had rows 1 byte apart in one
 * brush and 4 apart in another (shutdown grey-out came out in 4-row bands), and
 * dp8BrushBits is not filled for solid brushes. Hatched brushes, whose gaps
 * depend on the background mode, are not handled. */
static int m8_brush_colours( LPBRUSH brush, LPDRAWMODE dm, BYTE *c1, BYTE *c2, BYTE *pat )
{
    DIB_Brush8 FAR  *b = (DIB_Brush8 FAR *)brush;
    M8_RECT         cell;
    BYTE FAR        *px;
    int             i;
    int             two = 0;

    if( b == NULL ) {
        return( 0 );
    }
    M8_STEP( 0xD000 | ((b->dp8BrushFlags & 0xFF) << 4) | (b->dp8BrushStyle & 0xF) );
    if( b->dp8BrushStyle != 0 && b->dp8BrushStyle != 3 ) {
        return( 0 );
    }
    /* COLORSOLID: the start of dp8BrushBits is the colour, as the DDK samples
     * and velocity9x read it. */
    *c1 = b->dp8BrushBits[0];
    if( b->dp8BrushFlags & COLORSOLID ) {
        return( 1 );
    }

    m8_src.pitch = 8;
    m8_rect_set( &cell, 0, 0, 8, 8 );
    m8_strip_place( &m8_src, &cell );
    DIB_BitBlt( &m8_src.dev, 0, 0, NULL, 0, 0, 8, 8, 0x00F00021UL, brush, dm );
    px = m8_src.bits;

    *c1 = px[0];
    *c2 = *c1;
    for( i = 0; i < 64; ++i ) {
        if( px[i] == *c1 ) {
            continue;
        }
        if( !two ) {
            *c2 = px[i];
            two = 1;
        } else if( px[i] != *c2 ) {
            return( 0 );
        }
    }
    if( !two ) {
        return( 1 );
    }
    for( i = 0; i < 8; ++i ) {
        int     col;
        BYTE    bits = 0;

        for( col = 0; col < 8; ++col ) {
            if( px[i * 8 + col] == *c1 ) {
                bits |= (BYTE)(0x80 >> col);
            }
        }
        pat[i] = bits;
        M8_STEP( 0xE200 | bits );
    }
    return( 2 );
}

/* An 8x8 two-colour pattern over a rectangle by mono expansion: the expansion
 * starts on the 8-pixel boundary at or left of x, so every row is its pattern
 * byte repeated, and the scissors cut it to the rectangle. Rows are built in
 * the strip buffer, a band at a time. */
static void m8_pattern_fill( int x, int y, int w, int h, const BYTE *pat,
                             BYTE c1, BYTE c2, WORD mix )
{
    int         x0 = x & ~7;
    WORD        wb = (WORD)(((x + w + 7) >> 3) - (x >> 3));
    long        rows = (long)(M8_BUF_BYTES / wb);
    int         band;
    int         row;
    int         yb;
    int         hb;
    BYTE FAR    *q;

    /* In long: a one-byte-wide rectangle fits 65,520 rows in the buffer, which
     * a 16-bit int reads as -16 - the loop then ran backwards for ever. */
    band = rows < (long)h ? (int)rows : h;
    for( yb = y; yb < y + h; yb += band ) {
        hb = y + h - yb < band ? y + h - yb : band;
        q = m8_dst.bits;
        for( row = 0; row < hb; ++row ) {
            _fmemset( q, pat[(yb + row) & 7], wb );
            q += wb;
        }
        m8_hw_mono( x0, yb, wb, hb, m8_dst.bits, M8_MIX_SRC_FRGD | mix, c1,
                    M8_MIX_SRC_BKGD | mix, c2, x, yb, x + w - 1, yb + hb - 1 );
    }
}

static int m8_sc_top( void );

/* Bitmap cache. Icons, toolbar buttons and card faces are copied from the same
 * memory bitmaps again and again. The source pixels are hashed on the CPU; the
 * second time a content is seen it is uploaded to off-screen memory, and every
 * later copy of the same content is one engine copy through the ROP's mix - no
 * pixels on the bus. A content seen once goes the run-encoded upload route,
 * which costs less than a store. Keyed by content, so a bitmap that changes
 * simply gets a new entry. 8-bit device bitmaps only (a DIB section may carry
 * its own colour table, which the DIB Engine translates).
 *
 * The area is right of the screen, from the top down to the string cache: menu
 * saves below the screen never reach past the screen's width. None at
 * 1024x768. It is cut into shelves of one height class each, filled left to
 * right; when it is full, the least recently used run of adjacent shelves is
 * cleared and cut again, so what is in use survives one-off bitmaps. */
#define M8_BC_ENTRIES       160
#define M8_BC_SHELVES       64          /* more than the area's rows / 16 */
#define M8_BC_SEEN          64
#define M8_BC_MAX_PX        8192        /* a Solitaire card is 71x96 */
#define M8_BC_MAX_H         128

typedef struct {
    DWORD   h1;
    DWORD   h2;
    WORD    w;                          /* 0: free */
    WORD    h;
    int     x;
    int     y;
} M8_BCENTRY;

typedef struct {
    int     y;
    int     h;
    int     x;                          /* next free column, from the area's left */
    DWORD   used;                       /* last store or hit in it; 0: empty */
} M8_BCSHELF;

static M8_BCENTRY   m8_bc[M8_BC_ENTRIES];
static M8_BCSHELF   m8_bcs[M8_BC_SHELVES];
static int          m8_bcs_n;
static int          m8_bc_bottom;       /* first row in no shelf */
static DWORD        m8_bc_clock;
static DWORD        m8_bc_seen[M8_BC_SEEN];
static int          m8_bc_seen_next;
static WORD         m8_bc_sel;

static int m8_bc_left( void )
{
    return( (M8_SCREEN_X + 1) & ~1 );
}

static int m8_bc_width( void )
{
    return( M8_CURSOR_SAVE_X - m8_bc_left() );
}

static int m8_bc_on( void )
{
    return( m8_bc_sel != 0 && m8_bc_width() >= 32 );
}

static void m8_bc_flush( void )
{
    int k;

    for( k = 0; k < M8_BC_ENTRIES; ++k ) {
        m8_bc[k].w = 0;
    }
    for( k = 0; k < M8_BC_SEEN; ++k ) {
        m8_bc_seen[k] = 0;
    }
    m8_bcs_n = 0;
    m8_bc_bottom = 0;
    m8_bc_clock = 0;
    m8_bc_seen_next = 0;
}

/* 1 if this content was seen recently; otherwise remember it. */
static int m8_bc_seen_before( DWORD key )
{
    int k;

    for( k = 0; k < M8_BC_SEEN; ++k ) {
        if( m8_bc_seen[k] == key ) {
            return( 1 );
        }
    }
    m8_bc_seen[m8_bc_seen_next] = key;
    m8_bc_seen_next = (m8_bc_seen_next + 1) % M8_BC_SEEN;
    return( 0 );
}

static void m8_bc_free_rows( int y0, int y1 )
{
    int k;

    for( k = 0; k < M8_BC_ENTRIES; ++k ) {
        if( m8_bc[k].w != 0 && m8_bc[k].y >= y0 && m8_bc[k].y < y1 ) {
            m8_bc[k].w = 0;
        }
    }
}

static void m8_bc_touch( int y )
{
    int k;

    ++m8_bc_clock;
    for( k = 0; k < m8_bcs_n; ++k ) {
        if( y >= m8_bcs[k].y && y < m8_bcs[k].y + m8_bcs[k].h ) {
            m8_bcs[k].used = m8_bc_clock;
            return;
        }
    }
}

/* Replace `remove` shelves at `at` by `insert` shelves, contents to be set. */
static void m8_bc_splice( int at, int remove, int insert )
{
    int k;

    if( insert > remove ) {
        for( k = m8_bcs_n - 1; k >= at + remove; --k ) {
            m8_bcs[k + insert - remove] = m8_bcs[k];
        }
    } else if( insert < remove ) {
        for( k = at + remove; k < m8_bcs_n; ++k ) {
            m8_bcs[k + insert - remove] = m8_bcs[k];
        }
    }
    m8_bcs_n += insert - remove;
}

static void m8_bc_shelf_set( int k, int y, int h )
{
    m8_bcs[k].y = y;
    m8_bcs[k].h = h;
    m8_bcs[k].x = 0;
    m8_bcs[k].used = 0;
}

/* Clear the least recently used run of adjacent shelves at least hc rows tall
 * and cut a shelf of hc rows from its top: its index, or -1. Shelves cover the
 * area from row 0 to m8_bc_bottom without gaps, so a run is contiguous. */
static int m8_bc_evict( int hc )
{
    int     i;
    int     j;
    int     sum;
    int     best = -1;
    int     best_n = 0;
    int     best_sum = 0;
    int     y;
    int     parts;
    DWORD   age;
    DWORD   best_age = 0;

    for( i = 0; i < m8_bcs_n; ++i ) {
        sum = 0;
        age = 0;
        for( j = i; j < m8_bcs_n && sum < hc; ++j ) {
            sum += m8_bcs[j].h;
            if( m8_bcs[j].used > age ) {
                age = m8_bcs[j].used;
            }
        }
        if( sum < hc ) {
            break;
        }
        if( best < 0 || age < best_age || (age == best_age && sum < best_sum) ) {
            best = i;
            best_n = j - i;
            best_sum = sum;
            best_age = age;
        }
    }
    if( best < 0 ) {
        return( -1 );
    }
    if( best_age != 0 ) {
        M8_STEP( M8_MARK_BC_EVICT );
    }
    y = m8_bcs[best].y;
    m8_bc_free_rows( y, y + best_sum );
    parts = (best_sum > hc && m8_bcs_n - best_n + 2 <= M8_BC_SHELVES) ? 2 : 1;
    m8_bc_splice( best, best_n, parts );
    if( parts == 2 ) {
        m8_bc_shelf_set( best, y, hc );
        m8_bc_shelf_set( best + 1, y + hc, best_sum - hc );
    } else {
        m8_bc_shelf_set( best, y, best_sum );
    }
    return( best );
}

static int m8_bc_alloc( int w, int h, int *x, int *y )
{
    int hc = (h + 15) & ~15;
    int bottom = m8_sc_top();
    int k;

    if( w > m8_bc_width() || h > M8_BC_MAX_H || hc > bottom ) {
        return( 0 );
    }
    for( k = 0; k < m8_bcs_n; ++k ) {
        if( m8_bcs[k].h == hc && m8_bcs[k].x + w <= m8_bc_width() ) {
            break;
        }
    }
    if( k == m8_bcs_n ) {
        if( m8_bcs_n < M8_BC_SHELVES && m8_bc_bottom + hc <= bottom ) {
            m8_bc_shelf_set( m8_bcs_n++, m8_bc_bottom, hc );
            m8_bc_bottom += hc;
        } else {
            /* The rows left at the bottom join the area as one empty shelf. */
            if( m8_bc_bottom < bottom && m8_bcs_n < M8_BC_SHELVES ) {
                m8_bc_shelf_set( m8_bcs_n++, m8_bc_bottom, bottom - m8_bc_bottom );
                m8_bc_bottom = bottom;
            }
            k = m8_bc_evict( hc );
            if( k < 0 ) {
                return( 0 );
            }
        }
    }
    *x = m8_bc_left() + m8_bcs[k].x;
    *y = m8_bcs[k].y;
    m8_bcs[k].x += w;
    m8_bcs[k].used = ++m8_bc_clock;
    return( 1 );
}

/* A free entry; when all are taken, the least recently used shelf is cleared. */
static M8_BCENTRY *m8_bc_slot( void )
{
    int k;
    int old = -1;

    for( k = 0; k < M8_BC_ENTRIES; ++k ) {
        if( m8_bc[k].w == 0 ) {
            return( &m8_bc[k] );
        }
    }
    for( k = 0; k < m8_bcs_n; ++k ) {
        if( m8_bcs[k].used != 0 && (old < 0 || m8_bcs[k].used < m8_bcs[old].used) ) {
            old = k;
        }
    }
    if( old < 0 ) {
        return( NULL );
    }
    M8_STEP( M8_MARK_BC_EVICT );
    m8_bc_free_rows( m8_bcs[old].y, m8_bcs[old].y + m8_bcs[old].h );
    m8_bcs[old].x = 0;
    m8_bcs[old].used = 0;
    for( k = 0; k < M8_BC_ENTRIES; ++k ) {
        if( m8_bc[k].w == 0 ) {
            return( &m8_bc[k] );
        }
    }
    return( NULL );
}

/* Copy w x h of an 8-bit memory bitmap through the cache: 1 if done. */
static int m8_bc_draw( LPDIBENGINE src, int sx, int sy, int dx, int dy, int w, int h, WORD mix )
{
    WORD        we = (WORD)((w + 1) & ~1);
    DWORD       base;
    long        delta;
    DWORD       h1 = 2166136261UL;
    DWORD       h2 = (DWORD)w * 131 + h;
    int         row;
    int         col;
    int         k;
    int         store = 0;
    BYTE FAR    *p;
    BYTE FAR    *q;
    M8_BCENTRY  *e = NULL;
    M8_RECT     r;

    if( !m8_bc_on() || src->deBitsPixel != 8 || (src->deFlags & SELECTEDDIB) ||
        (long)we * h > M8_BC_MAX_PX || (long)we * h > 0xFFF0L ) {
        return( 0 );
    }
    base = GetSelectorBase( src->deBitsSelector ) + src->deBitsOffset;
    delta = (long)src->deDeltaScan;
    for( row = 0; row < h; ++row ) {
        SetSelectorBase( m8_bc_sel, base + (DWORD)((long)(sy + row) * delta) + (DWORD)sx );
        SetSelectorLimit( m8_bc_sel, (DWORD)w - 1 );
        p = (BYTE FAR *)MK_FP( m8_bc_sel, 0 );
        q = m8_src.bits + (DWORD)row * we;
        for( col = 0; col < w; ++col ) {
            q[col] = p[col];
            h1 = (h1 ^ p[col]) * 16777619UL;
            h2 = (h2 << 5) + h2 + p[col];
        }
        if( we != (WORD)w ) {
            q[w] = 0;
        }
    }
    for( k = 0; k < M8_BC_ENTRIES; ++k ) {
        if( m8_bc[k].w == (WORD)w && m8_bc[k].h == (WORD)h && m8_bc[k].h1 == h1 &&
            m8_bc[k].h2 == h2 ) {
            e = &m8_bc[k];
            M8_STEP( M8_MARK_BC_HIT );
            m8_bc_touch( e->y );
            break;
        }
    }
    if( e == NULL ) {
        if( !m8_bc_seen_before( h1 ^ h2 ) ) {
            M8_STEP( M8_MARK_BC_FIRST );
            return( 0 );
        }
        e = m8_bc_slot();
        if( e == NULL || !m8_bc_alloc( we, h, &e->x, &e->y ) ) {
            return( 0 );
        }
        e->h1 = h1;
        e->h2 = h2;
        e->w = (WORD)w;
        e->h = (WORD)h;
        store = 1;
    }
    m8_rect_set( &r, dx, dy, dx + w, dy + h );
    m8_enter( &r );
    if( store ) {
        M8_STEP( M8_MARK_BC_STORE );
        m8_hw_write_rect( e->x, e->y, we, h, m8_src.bits, we );
    }
    m8_hw_copy_mix( e->x, e->y, dx, dy, w, h, M8_MIX_SRC_VRAM | mix );
    m8_leave();
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
    BYTE        rop = (BYTE)(dwRop3 >> 16);
    int         rows;
    WORD        pitch;

    if( !dst_screen && !src_screen ) {
        return( DIB_BitBlt( lpDestDev, wDestX, wDestY, lpSrcDev, wSrcX, wSrcY, wXext, wYext,
                            dwRop3, lpPBrush, lpDrawMode ) );
    }
    M8_ENTRY( M8_STAT_BITBLT );

    /* A memory source clips the copy to its own extent, as the DIB Engine
     * does: GDI passed the castle deck's bat sprite 36 wide from x 42 of a
     * 71-wide card bitmap, the DIB Engine drew 29 columns, and the fallback
     * wrote the other 7 back from a stale buffer. deWidth/deHeight sit where a
     * plain BITMAP keeps bmWidth/bmHeight. */
    if( m8_rop_uses_src( dwRop3 ) && lpSrcDev != NULL && !m8_is_screen( lpSrcDev ) ) {
        LPDIBENGINE s = lpSrcDev;
        long        sw = (long)s->deWidth - (short)wSrcX;
        long        sh = (long)s->deHeight - (short)wSrcY;

        if( (short)wSrcX < 0 || (short)wSrcY < 0 || sw <= 0 || sh <= 0 ) {
            return( TRUE );
        }
        if( (long)wXext > sw ) {
            wXext = (WORD)sw;
        }
        if( (long)wYext > sh ) {
            wYext = (WORD)sh;
        }
    }
    M8_STEP( 0xC000 | (m8_src_kind( lpSrcDev, src_screen ) << 8) | rop );
#ifdef M8_TRACE
    /* The call's own arguments, after an F0F0 marker: the castle sprite is drawn
     * wider than ATIM8 draws it, and this shows what GDI passed. */
    if( m8_src_kind( lpSrcDev, src_screen ) == 2 ) {
        M8_STEP( 0xF0F0 );
        M8_STEP( wDestX );
        M8_STEP( wDestY );
        M8_STEP( wXext );
        M8_STEP( wYext );
        M8_STEP( wSrcX );
        M8_STEP( wSrcY );
    }
#endif

    /* Engine paths. A background palette translation is something the engine
     * cannot apply, so it disqualifies them; so does an empty or negative
     * extent, which the engine would take as a huge one. */
    if( dst_screen && !(m8_screen->deFlags & PALETTE_XLAT) &&
        (short)wXext > 0 && (short)wYext > 0 ) {
        m8_rect_set( &r, wDestX, wDestY, wDestX + wXext, wDestY + wYext );
        if( !m8_rop_uses_src( dwRop3 ) ) {
            WORD    mix = m8_rop_mix( rop );
            BYTE    c1 = 0;
            BYTE    c2 = 0;
            BYTE    pat[8];
            int     kind = 1;

            if( m8_rop_uses_pat( dwRop3 ) ) {
                kind = m8_brush_colours( lpPBrush, lpDrawMode, &c1, &c2, pat );
            }
            if( kind == 1 ) {
                m8_enter( &r );
                m8_hw_fill_mix( wDestX, wDestY, wXext, wYext, c1, M8_MIX_SRC_FRGD | mix );
                m8_leave();
                return( TRUE );
            }
            /* Two-colour patterns come from the DIB Engine's own 8x8 rendering
             * (m8_brush_colours); M8_PATTERN_EXPANSION 0 sends them back to the
             * fallback. */
            if( kind == 2 && M8_PATTERN_EXPANSION ) {
                m8_enter( &r );
                m8_pattern_fill( wDestX, wDestY, wXext, wYext, pat, c1, c2, mix );
                m8_leave();
                return( TRUE );
            }
        }
        if( !src_screen && m8_rop_uses_src( dwRop3 ) && !m8_rop_uses_pat( dwRop3 ) &&
            ((LPDIBENGINE)lpSrcDev)->deType == TYPE_DIBENG &&
            m8_bc_draw( (LPDIBENGINE)lpSrcDev, wSrcX, wSrcY, wDestX, wDestY, wXext, wYext,
                        m8_mix_for_key[rop & 0x0F] ) ) {
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

        M8_STEP( M8_MARK_SCR2MEM );
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

/* A solid pen through a ROP2, every point on the engine's 0-1023 grid: the
 * engine draws it. Pens reaching Output are one pixel wide - Enable withdraws
 * LC_WIDE, so GDI turns wide pens into polygons. */
static int m8_line_on_engine( WORD count, LPPOINT pts, LPPPEN pen, LPDRAWMODE dm )
{
    DIB_Pen FAR *p = pen;
    WORD        i;

    if( count < 2 || p == NULL || dm == NULL || p->dpPenStyle != LS_SOLID ||
        dm->Rop2 < 1 || dm->Rop2 > 16 ) {
        return( 0 );
    }
    for( i = 0; i < count; ++i ) {
        if( (WORD)pts[i].xcoord >= M8_PITCH || (WORD)pts[i].ycoord >= M8_PITCH ) {
            return( 0 );
        }
    }
    return( 1 );
}

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
    M8_ENTRY( M8_STAT_OUTPUT );
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
    if( wStyle == OS_POLYLINE &&
        m8_line_on_engine( wCount, lpPoints, lpPPen, lpDrawMode ) ) {
        M8_RECT scr;

        m8_rect_screen( &scr );
        m8_rect_and( &bound, &scr );
        if( !m8_rect_empty( &bound ) ) {
            /* ROP2 codes run 1-16; minus one they are the mix table's key. */
            m8_enter( &bound );
            m8_hw_polyline( lpPoints, wCount, (BYTE)((DIB_Pen FAR *)lpPPen)->dpPenColor,
                            m8_mix_for_key[lpDrawMode->Rop2 - 1],
                            bound.l, bound.t, bound.r - 1, bound.b - 1 );
            m8_leave();
        }
        return( 1 );
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
    M8_ENTRY( M8_STAT_PIXEL );
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
    M8_ENTRY( M8_STAT_SCANLR );
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

/* An upper bound on the string's width: its character widths if GDI passed
 * them, else the font's widest character for each; plus the draw mode's extra
 * spacing. 0 when there is nothing to go on. */
static long m8_text_width( M8_TEXT *t )
{
    long    w = 0;
    int     i;
    int     n = t->count;

    if( t->widths != NULL ) {
        for( i = 0; i < n; ++i ) {
            w += t->widths[i] < 0 ? -t->widths[i] : t->widths[i];
        }
    } else if( t->font != NULL && t->font->dfMaxWidth > 0 ) {
        w = (long)n * t->font->dfMaxWidth;
    } else {
        return( 0 );
    }
    if( t->dm != NULL ) {
        w += (long)n * (t->dm->CharExtra < 0 ? -t->dm->CharExtra : t->dm->CharExtra);
        w += t->dm->TBreakExtra < 0 ? -t->dm->TBreakExtra : t->dm->TBreakExtra;
    }
    return( w < 2L * M8_SCREEN_X ? w : 0 );
}

/* The box the text can touch: from y down by the font's height, from x across
 * by the width bound, each with a margin; joined with the opaque rectangle and
 * clipped. A measuring call (negative count) is no guide - it returned a 2-row
 * height for icon labels. Rotated text, or a height that makes no sense, takes
 * the whole clip; no width bound takes the clip's columns. */
static void m8_text_bound( M8_TEXT *t, M8_RECT *bound )
{
    M8_RECT box;
    M8_RECT op;
    int     h = 0;
    long    w;

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
        w = m8_text_width( t );
        if( w > 0 ) {
            box.l = t->x - 8;
            box.r = t->x + (int)w + 8;
        }
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

/* String cache. The DIB Engine hands text over as one 1-bit bitmap per string,
 * and Windows draws the same strings again and again (menus, labels, file
 * names). Each string is kept in one bit-plane of off-screen memory - 8 planes,
 * so 8 layers in the same rows - and every later draw is one plane-select blit
 * (M8TIME A on the card) with no bits on the bus. Rows just above the cursor's
 * corner; 128 of them (96 at 1024x768, to leave room for menu saves). Packed in
 * shelves per plane; when full, the whole cache is cleared. Keyed by two 32-bit
 * hashes of the bits and the size. */
#define M8_SC_ENTRIES       384
#define M8_SC_W             (M8_CURSOR_SAVE_X)      /* 992: clear of the cursor's corner */
#define M8_SC_MAX_H         32

typedef struct {
    DWORD   h1;
    DWORD   h2;
    WORD    wb;
    WORD    h;
    int     x;
    int     y;
    int     plane;
} M8_SCENTRY;

static M8_SCENTRY   m8_sc[M8_SC_ENTRIES];
static int          m8_sc_n;
static int          m8_sc_plane;
static int          m8_sc_x;
static int          m8_sc_y;
static int          m8_sc_shelf;

static int m8_sc_rows( void )
{
    return( M8_SCREEN_Y >= 768 ? 96 : 128 );
}

static int m8_sc_top( void )
{
    return( M8_CURSOR_SAVE_Y - m8_sc_rows() );
}

static void m8_sc_flush( void )
{
    m8_sc_n = 0;
    m8_sc_plane = 0;
    m8_sc_x = 0;
    m8_sc_y = 0;
    m8_sc_shelf = 0;
}

/* A free spot for w x h, or 0 when every plane is full. */
static int m8_sc_alloc( int w, int h, int *x, int *y, int *plane )
{
    for( ;; ) {
        if( m8_sc_x + w > M8_SC_W ) {
            m8_sc_y += m8_sc_shelf;
            m8_sc_x = 0;
            m8_sc_shelf = 0;
        }
        if( m8_sc_y + h <= m8_sc_rows() ) {
            break;
        }
        if( ++m8_sc_plane >= 8 ) {
            return( 0 );
        }
        m8_sc_x = 0;
        m8_sc_y = 0;
        m8_sc_shelf = 0;
    }
    *x = m8_sc_x;
    *y = m8_sc_top() + m8_sc_y;
    *plane = m8_sc_plane;
    m8_sc_x += w;
    if( h > m8_sc_shelf ) {
        m8_sc_shelf = h;
    }
    return( 1 );
}

/* The cached copy of this string: found, or stored now. NULL if it cannot be
 * cached (too big, or the cache is full and has just been cleared). */
static M8_SCENTRY *m8_sc_get( const BYTE FAR *bits, WORD wb, WORD h )
{
    DWORD       h1 = 2166136261UL;
    DWORD       h2 = 0;
    DWORD       n = (DWORD)wb * h;
    DWORD       i;
    int         k;
    M8_SCENTRY  *e;

    if( (int)wb * 8 > M8_SC_W || h > M8_SC_MAX_H || (int)h > m8_sc_rows() ) {
        return( NULL );
    }
    for( i = 0; i < n; ++i ) {
        h1 = (h1 ^ bits[i]) * 16777619UL;
        h2 = (h2 << 5) + h2 + bits[i] + i;
    }
    for( k = 0; k < m8_sc_n; ++k ) {
        e = &m8_sc[k];
        if( e->h1 == h1 && e->h2 == h2 && e->wb == wb && e->h == h ) {
            M8_STEP( M8_MARK_SC_HIT );
            return( e );
        }
    }
    if( m8_sc_n >= M8_SC_ENTRIES ) {
        m8_sc_flush();
    }
    e = &m8_sc[m8_sc_n];
    if( !m8_sc_alloc( wb * 8, h, &e->x, &e->y, &e->plane ) ) {
        m8_sc_flush();
        return( NULL );
    }
    e->h1 = h1;
    e->h2 = h2;
    e->wb = wb;
    e->h = h;
    ++m8_sc_n;
    M8_STEP( M8_MARK_SC_STORE );
    m8_hw_mono_plane( e->x, e->y, wb, h, bits, e->plane );
    return( e );
}

/* Engine text: DIB_ExtTextOutExt lays the string out and calls back with the
 * opaque rectangle and with the string as one mono bitmap (rows of
 * width_bytes, leftmost pixel in each byte's top bit; the callback list is the
 * Windows 98 DDK xga sample's STRBLT.ASM, as velocity9x transcribes it).
 * A callback cannot decline: one that cannot draw sets m8_text_failed and the
 * caller draws the whole string again through the fallback. */
#define M8_ETO_LEVEL_MODE   0x1000
#define M8_COORD_MAX        2047        /* engine coordinates are 12-bit signed */

static int m8_text_failed;

static WORD WINAPI __loadds m8_text_bitmap( LPDIBENGINE lpDevice, BYTE FAR *lpBits, WORD wFlags,
                                            DWORD dwBg, DWORD dwFg, WORD wX, WORD wY,
                                            WORD wWidthBytes, WORD wHeight, LPRECT lpClip )
{
    M8_RECT r;
    M8_RECT c;
    int     x = (short)wX;
    int     y = (short)wY;

    if( lpDevice != m8_screen || lpBits == NULL ) {
        M8_STEP( M8_MARK_TEXT_WHY | 1 );
        m8_text_failed = 1;
        return( 1 );
    }
    if( wWidthBytes == 0 || wHeight == 0 ) {
        return( 1 );
    }
    /* The feed addresses the bitmap with a 16-bit offset in one selector. */
    if( (DWORD)FP_OFF( lpBits ) + (DWORD)wWidthBytes * wHeight > 0x10000UL ||
        x < 0 || y < 0 || x + (long)wWidthBytes * 8 > M8_COORD_MAX ||
        y + (long)wHeight > M8_COORD_MAX ) {
        /* Trace: 2 = the bitmap crosses its selector, 3 = it starts off the
         * top or left, 4 = beyond the engine's coordinates. */
        M8_STEP( M8_MARK_TEXT_WHY | ((DWORD)FP_OFF( lpBits ) + (DWORD)wWidthBytes * wHeight > 0x10000UL ? 2 :
                                     (x < 0 || y < 0) ? 3 : 4) );
        m8_text_failed = 1;
        return( 1 );
    }
    m8_rect_set( &r, x, y, x + wWidthBytes * 8, y + wHeight );
    if( lpClip != NULL ) {
        m8_rect_from_gdi( &c, lpClip );
        m8_rect_and( &r, &c );
    }
    m8_rect_screen( &c );
    m8_rect_and( &r, &c );
    if( m8_rect_empty( &r ) ) {
        return( 1 );
    }
    m8_exclude( &r );
    {
        M8_SCENTRY *e = m8_sc_get( lpBits, wWidthBytes, wHeight );

        if( e != NULL ) {
            /* The visible part only: the blit has no scissors of its own. */
            m8_hw_plane_blit( e->x + (r.l - x), e->y + (r.t - y), r.l, r.t,
                              r.r - r.l, r.b - r.t, e->plane, (BYTE)dwFg, (BYTE)dwBg,
                              wFlags & 1 );
            return( 1 );
        }
    }
    m8_hw_mono( x, y, wWidthBytes, wHeight, lpBits,
                M8_MIX_SRC_FRGD | M8_MIX_REPLACE, (BYTE)dwFg,
                (wFlags & 1) ? M8_MIX_LEAVE : M8_MIX_SRC_BKGD | M8_MIX_REPLACE, (BYTE)dwBg,
                r.l, r.t, r.r - 1, r.b - 1 );
    return( 1 );
}

static WORD WINAPI __loadds m8_text_rect( LPDIBENGINE lpDevice, DWORD dwBg, WORD wX, WORD wY,
                                          WORD wXExt, WORD wYExt )
{
    M8_RECT r;
    M8_RECT c;

    if( lpDevice != m8_screen ) {
        M8_STEP( M8_MARK_TEXT_WHY | 5 );
        m8_text_failed = 1;
        return( 1 );
    }
    m8_rect_set( &r, (short)wX, (short)wY, (short)wX + (int)wXExt, (short)wY + (int)wYExt );
    m8_rect_screen( &c );
    m8_rect_and( &r, &c );
    if( m8_rect_empty( &r ) ) {
        return( 1 );
    }
    m8_exclude( &r );
    m8_hw_fill( r.l, r.t, r.r - r.l, r.b - r.t, (BYTE)dwBg );
    return( 1 );
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
    M8_ENTRY( M8_STAT_EXTTEXTOUT );
    if( !(wOptions & M8_ETO_LEVEL_MODE) && !(m8_screen->deFlags & PALETTE_XLAT) ) {
        DWORD rc;

        m8_busy = 1;
        m8_text_failed = 0;
        rc = DIB_ExtTextOutExt( lpDestDev, wDestXOrg, wDestYOrg, lpClipRect, lpString, wCount,
                                lpFontInfo, lpDrawMode, lpTextXForm, lpCharWidths,
                                lpOpaqueRect, wOptions, (LPVOID *)m8_text_bitmap,
                                (LPVOID *)m8_text_rect );
        m8_leave();
        if( !m8_text_failed ) {
            return( rc );
        }
        ++m8_stats[M8_STAT_TEXT_FAIL];
        M8_STEP( M8_MARK_TEXT_FALLBACK );
    }
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
    M8_ENTRY( M8_STAT_STRBLT );
    t.x = wDestXOrg;
    t.y = wDestYOrg;
    t.clip = lpClipRect;
    t.str = lpString;
    t.count = wCount;
    t.font = lpFontInfo;
    t.dm = lpDrawMode;
    t.xform = lpTextXForm;
    t.widths = NULL;
    t.opaque = NULL;
    t.options = 0;
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
    M8_ENTRY( M8_STAT_DIBTODEV );
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
    m8_fallback( &bound, M8_READ_IF_UNCOVERED, 1, 0, m8_draw_dibtodev, &d );
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
    M8_ENTRY( M8_STAT_STRETCHDIB );
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
    M8_ENTRY( M8_STAT_STRETCHBLT );
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

/* ------------------------------------------------------- SaveScreenBitmap */

/* USER saves what a menu covers and puts it back when the menu closes (ordinal
 * 92: 0 save, 1 restore, 2 discard; 1 = done, 0 = GDI does it itself). One
 * area at a time, kept in the off-screen rows below the visible screen, moved
 * there and back by engine copy: nothing crosses the bus. */
#define M8_SSB_SAVE     0
#define M8_SSB_RESTORE  1
#define M8_SSB_DISCARD  2
#define M8_SSB_Y        M8_SCREEN_Y         /* the rows below the screen, at x 0 */

static int      m8_ssb_used;
static M8_RECT  m8_ssb_rect;

/* Trace: the first time the two copies differ, log the entry being run (the
 * last B0xx marker shows it) and the corrupted rectangle. */
static void m8_ssb_verify( void )
{
    if( !m8_ssb_used || m8_ssb_reported ) {
        return;
    }
    if( m8_ssb_rect.l != m8_ssb_check.l || m8_ssb_rect.t != m8_ssb_check.t ||
        m8_ssb_rect.r != m8_ssb_check.r || m8_ssb_rect.b != m8_ssb_check.b ) {
        m8_ssb_reported = 1;
        M8_STEP( M8_MARK_SSB_CORRUPT );
        M8_STEP( m8_ssb_rect.l );
        M8_STEP( m8_ssb_rect.t );
        M8_STEP( m8_ssb_rect.r );
        M8_STEP( m8_ssb_rect.b );
    }
}

WORD WINAPI __loadds SaveScreenBitmap( LPRECT lpRect, WORD wCmd )
{
    M8_RECT r;
    M8_RECT scr;
    int     w;
    int     h;

    if( m8_screen == NULL ) {
        return( 0 );
    }
    if( wCmd == M8_SSB_DISCARD ) {
        m8_ssb_used = 0;
        return( 1 );
    }
    if( wCmd == M8_SSB_RESTORE ) {
        if( !m8_ssb_used ) {
            return( 0 );
        }
        m8_ssb_verify();
        m8_rect_screen( &scr );
        if( m8_ssb_rect.l != m8_ssb_check.l || m8_ssb_rect.t != m8_ssb_check.t ||
            m8_ssb_rect.r != m8_ssb_check.r || m8_ssb_rect.b != m8_ssb_check.b ||
            m8_ssb_rect.l < 0 || m8_ssb_rect.t < 0 || m8_ssb_rect.r > scr.r ||
            m8_ssb_rect.b > scr.b ) {
            m8_ssb_used = 0;
            return( 0 );
        }
        M8_STEP( M8_MARK_SSB_RESTORE );
        w = m8_ssb_rect.r - m8_ssb_rect.l;
        h = m8_ssb_rect.b - m8_ssb_rect.t;
        m8_enter( &m8_ssb_rect );
        m8_hw_copy( 0, M8_SSB_Y, m8_ssb_rect.l, m8_ssb_rect.t, w, h );
        m8_leave();
        m8_ssb_used = 0;
        return( 1 );
    }
    if( wCmd != M8_SSB_SAVE || m8_ssb_used || lpRect == NULL ) {
        return( 0 );
    }
    m8_rect_from_gdi( &r, lpRect );
    m8_rect_screen( &scr );
    m8_rect_and( &r, &scr );
    w = r.r - r.l;
    h = r.b - r.t;
    /* Up to the string cache: 384 rows at 640x480, 264 at 800x600, 128 at
     * 1024x768. */
    if( w <= 0 || h <= 0 || w > M8_PITCH || h > m8_sc_top() - M8_SSB_Y ) {
        return( 0 );
    }
    M8_STEP( M8_MARK_SSB_SAVE );
    m8_enter( &r );
    m8_hw_copy( r.l, r.t, 0, M8_SSB_Y, w, h );
    m8_leave();
    m8_ssb_rect = r;
    m8_ssb_check = r;
    m8_ssb_used = 1;
    return( 1 );
}

/* --------------------------------------------------- FastBorder, UpdateColors */

/* Zero hands the border back to GDI, which draws it with BitBlt. */
WORD WINAPI __loadds FastBorder( LPRECT lpRect, WORD wHorizBorder, WORD wVertBorder,
                                 DWORD dwRop3, LPPDEVICE lpDestDev, LPBRUSH lpPBrush,
                                 LPDRAWMODE lpDrawMode, LPRECT lpClipRect )
{
    M8_ENTRY( M8_STAT_FASTBORDER );
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

    M8_ENTRY( M8_STAT_UPDATECOL );
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

    M8_STEP( M8_MARK_CURSOR_SET );
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
    M8_STEP( M8_MARK_CURSOR_MOVE );
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
    m8_ssb_used = 0;
    m8_sc_flush();
    m8_bc_flush();
    /* The DIB Engine calls these on the screen around engine text; the cursor
     * is excluded by the callbacks themselves. */
    screen->deBeginAccess = (void (WINAPI *)())m8_begin_access;
    screen->deEndAccess = (void (WINAPI *)())m8_end_access;
    if( m8_dst.sel == 0 && !m8_strip_alloc( &m8_dst, &m8_dst_mem ) ) {
        return( 0 );
    }
    if( m8_src.sel == 0 && !m8_strip_alloc( &m8_src, &m8_src_mem ) ) {
        return( 0 );
    }
    if( m8_bc_sel == 0 ) {
        m8_bc_sel = AllocSelector( m8_src.sel );
    }
    /* Text measuring calls use m8_dst without a strip: give it a valid one. */
    m8_dst.pitch = 4;
    m8_rect_set( &one, 0, 0, 1, 1 );
    m8_strip_place( &m8_dst, &one );
    return( 1 );
}

/* DOS has the screen (INT 2Fh 4001h), or the mode is changing. The cursor and a
 * saved menu area are left in video memory nobody will repaint for us; forget
 * both. Busy until m8_screen_resume, so a cursor move is only recorded. */
void m8_screen_suspend( void )
{
    m8_busy = 1;
    m8_sc_flush();
    m8_bc_flush();
    m8_cur_shown = 0;
    m8_cur_pending = 0;
    m8_ssb_used = 0;
}

/* Windows has the screen again; USER repaints everything after this. */
void m8_screen_resume( void )
{
    m8_cur_shown = 0;
    m8_cur_pending = 0;
    m8_ssb_used = 0;
    m8_busy = 0;
    m8_cursor_show();
}

void m8_screen_done( void )
{
    m8_busy = 1;
    m8_cursor_hide();
    m8_screen = NULL;
    if( m8_bc_sel != 0 ) {
        FreeSelector( m8_bc_sel );
        m8_bc_sel = 0;
    }
    m8_strip_free( &m8_dst, &m8_dst_mem );
    m8_strip_free( &m8_src, &m8_src_mem );
    m8_busy = 0;
}
