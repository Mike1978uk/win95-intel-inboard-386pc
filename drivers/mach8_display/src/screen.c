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

/* Trace and profile builds: each timed entry is defined under an m8t_ name and
 * wrapped at the end of this file (m8_tm, m8_pf_in). */
#ifdef M8_TRACE
static void m8_tm_point( WORD id );
#define M8_TP( id )         m8_tm_point( id )
#else
#define M8_TP( id )
#endif
#if defined( M8_TRACE ) || defined( M8_PROF )
#define BitBlt              m8t_BitBlt
#define ExtTextOut          m8t_ExtTextOut
#define Output              m8t_Output
#define Pixel               m8t_Pixel
#define StrBlt              m8t_StrBlt
#define DibToDevice         m8t_DibToDevice
#define StretchBlt          m8t_StretchBlt
#define StretchDIBits       m8t_StretchDIBits
#define SaveScreenBitmap    m8t_SaveScreenBitmap
#define ScanLR              m8t_ScanLR
#endif

#define M8_BUF_BYTES        0xFFF0u     /* one strip; < 64 KB so one selector covers it */
#define M8_CURSOR_MAX       32
#define M8_PATTERN_EXPANSION 1     /* two-colour brushes through the pattern registers; 0: fallback */
#define M8_CURSOR_SAVE_W    (M8_CURSOR_MAX + 2)     /* even-aligned around a 32-wide cursor */


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

#define m8_screen           ((LPDIBENGINE)m8_hot.screen)
static M8_STRIP     m8_dst;
static M8_STRIP     m8_src;
static HGLOBAL      m8_dst_mem;
static HGLOBAL      m8_src_mem;
#define m8_busy             m8_hot.busy

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
#define m8_cur_shown    m8_hot.cur_shown    /* drawn on the screen now */
static int      m8_cur_soft;        /* shown by the software path: m8_cur_save holds it */
#define m8_cur_rect     m8_hot.cur_rect     /* where m8_cur_save came from */
#define m8_cur_pending  m8_hot.cur_pending  /* a move arrived while busy */

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

/* GDI can still draw to the screen device after Disable (m8_screen NULL),
 * at shutdown: every screen entry returns at once then. */
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
        /* A row the same as the one above (a stretch, a gradient band) is an
         * engine copy of it once that row is on the screen: no pixels on the
         * bus. A run of them doubles, so n rows cost about log2 n copies.
         * One colour across is cheaper as the fill below. */
        if( y > 0 && _fmemcmp( row, row - s->pitch, w ) == 0 && !m8_row_uniform( row, w ) ) {
            int n = 1;
            int c;

            if( plain >= 0 ) {
                m8_hw_write_rect( r->l, r->t + plain, w, y - plain,
                                  s->bits + (DWORD)plain * s->pitch, s->pitch );
                plain = -1;
            }
            for( k = y + 1; k < h && _fmemcmp( s->bits + (DWORD)k * s->pitch, row, w ) == 0; ++k ) {
            }
            while( y < k ) {
                c = k - y < n ? k - y : n;
                m8_hw_copy( r->l, r->t + y - n, r->l, r->t + y, w, c );
                y += c;
                n += c;
            }
            --y;
            continue;
        }
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

/* The masks go to off-screen memory once per shape (M8_CURSOR_MASK_X/Y): the
 * AND mask inverted in plane 0, so a 1 there means black, the XOR mask in plane
 * 1. Lost with the caches whenever DOS has had the screen. */
static int m8_cur_planes_ok;

/* A shape that inverts nothing is black, white and transparent: kept as an
 * 8-bit image in the same place, transparent as M8_CURSOR_KEY, which neither
 * black (00h) nor white (FFh) can be, and drawn with one keyed copy. */
#define M8_CURSOR_KEY       0x01
static int  m8_cur_keyed;
static BYTE m8_cur_img[M8_CURSOR_MAX * M8_CURSOR_MAX];

static void m8_cursor_planes( void )
{
    BYTE    inv[M8_CURSOR_MAX * M8_CURSOR_MAX / 8];
    int     i;

    if( m8_cur_keyed ) {
        for( i = 0; i < sizeof( m8_cur_img ); ++i ) {
            BYTE bit = (BYTE)(0x80 >> (i & 7));

            m8_cur_img[i] = (m8_cur_and[i >> 3] & bit) ? M8_CURSOR_KEY :
                            (m8_cur_xor[i >> 3] & bit) ? 0xFF : 0x00;
        }
        m8_hw_write_rect( M8_CURSOR_MASK_X, M8_CURSOR_MASK_Y, M8_CURSOR_MAX, M8_CURSOR_MAX,
                          m8_cur_img, M8_CURSOR_MAX );
        m8_cur_planes_ok = 1;
        return;
    }
    for( i = 0; i < sizeof( inv ); ++i ) {
        inv[i] = (BYTE)~m8_cur_and[i];
    }
    m8_hw_mono_plane( M8_CURSOR_MASK_X, M8_CURSOR_MASK_Y, M8_CURSOR_MAX / 8, M8_CURSOR_MAX,
                      inv, 0 );
    m8_hw_mono_plane( M8_CURSOR_MASK_X, M8_CURSOR_MASK_Y, M8_CURSOR_MAX / 8, M8_CURSOR_MAX,
                      m8_cur_xor, 1 );
    m8_cur_planes_ok = 1;
}

/* Engine: what is under the cursor is copied to off-screen memory, then the
 * two mask planes are drawn over it from off-screen memory: no pixels or mask
 * bits cross the bus. */
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
    if( !m8_cur_planes_ok ) {
        m8_cursor_planes();
    }
    if( m8_cur_keyed ) {
        m8_hw_copy_keyed( M8_CURSOR_MASK_X + r.l - cx, M8_CURSOR_MASK_Y + r.t - cy, r.l, r.t,
                          r.r - r.l, r.b - r.t, M8_CURSOR_KEY );
        m8_cur_rect = r;
        m8_cur_soft = 0;
        m8_cur_shown = 1;
        return;
    }
    m8_hw_plane_mix( M8_CURSOR_MASK_X + r.l - cx, M8_CURSOR_MASK_Y + r.t - cy, r.l, r.t,
                     r.r - r.l, r.b - r.t, 0, 0x00, M8_MIX_SRC_FRGD | M8_MIX_REPLACE );
    m8_hw_plane_mix( M8_CURSOR_MASK_X + r.l - cx, M8_CURSOR_MASK_Y + r.t - cy, r.l, r.t,
                     r.r - r.l, r.b - r.t, 1, 0xFF, M8_MIX_SRC_FRGD | M8_MIX_XOR );
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
#ifdef M8_TRACE
    m8_ssb_verify();
#endif
    m8_busy = 1;
    if( m8_cur_shown && (r == NULL || m8_rect_overlap( r, &m8_cur_rect )) ) {
        m8_cursor_hide();
        m8_cur_pending = 1;
    }
    return( 1 );
}

static void m8_leave( void );

/* m8_leave's common case inline, for the short paths: no cursor move arrived
 * while the call drew. */
#define M8_LEAVE()  { if( m8_cur_pending ) m8_leave(); else m8_busy = 0; }

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

/* As m8_fallback, with the strips' left edge, top edge and height on multiples
 * of `align` (2, or 8 so that a shift by the strip's origin keeps brush
 * patterns in phase: see m8_draw_output). */
static DWORD m8_fallback_aligned( const M8_RECT *bound_in, int read, int write, int bottom_up,
                                  M8_DRAW draw, void *ctx, int align )
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
    if( align > 2 ) {
        bound.l &= ~(align - 1);
        bound.t &= ~(align - 1);
    }
    if( !read && (bound.l != area.l || bound.r != area.r) ) {
        edges = 1;
    }
    rows = m8_strip_rows( &bound, &pitch );
    if( align > 2 ) {
        rows &= ~(align - 1);
    }
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
        clip.top = s.t > area.t ? s.t : area.t;
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

static DWORD m8_fallback( const M8_RECT *bound_in, int read, int write, int bottom_up,
                          M8_DRAW draw, void *ctx )
{
    return( m8_fallback_aligned( bound_in, read, write, bottom_up, draw, ctx, 2 ) );
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
 * dp8BrushBits is not filled for solid brushes. A hatched brush leaves its gaps
 * alone under a transparent background: the cell is painted twice, over two
 * different fillers, and a pixel that differs between them is a gap - 3 then,
 * with pat's 1 bits the hatch (c1) and 0 bits to be left as they are. */
static int m8_brush_render( LPBRUSH brush, LPDRAWMODE dm, BYTE *c1, BYTE *c2, BYTE *pat )
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
    if( b->dp8BrushStyle != 0 && b->dp8BrushStyle != 2 && b->dp8BrushStyle != 3 ) {
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
    px = m8_src.bits;
    if( b->dp8BrushStyle == 2 ) {
        BYTE    first[64];
        BYTE    bits;
        int     gaps = 0;
        int     col;

        _fmemset( px, 0x00, 64 );
        DIB_BitBlt( &m8_src.dev, 0, 0, NULL, 0, 0, 8, 8, 0x00F00021UL, brush, dm );
        _fmemcpy( first, px, 64 );
        _fmemset( px, 0xFF, 64 );
        DIB_BitBlt( &m8_src.dev, 0, 0, NULL, 0, 0, 8, 8, 0x00F00021UL, brush, dm );
        for( i = 0; i < 64; ++i ) {
            if( first[i] != px[i] ) {
                ++gaps;
            }
        }
        if( gaps > 0 ) {
            *c1 = 0;
            *c2 = 0;
            two = 0;
            for( i = 0; i < 8; ++i ) {
                bits = 0;
                for( col = 0; col < 8; ++col ) {
                    BYTE v = px[i * 8 + col];

                    if( first[i * 8 + col] != v ) {
                        continue;
                    }
                    if( !two ) {
                        *c1 = v;
                        two = 1;
                    } else if( v != *c1 ) {
                        return( 0 );
                    }
                    bits |= (BYTE)(0x80 >> col);
                }
                pat[i] = bits;
            }
            return( two ? 3 : 0 );
        }
    } else {
        DIB_BitBlt( &m8_src.dev, 0, 0, NULL, 0, 0, 8, 8, 0x00F00021UL, brush, dm );
    }

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

/* The last pattern brush and its answer. GDI hands a filled shape over as many
 * scanline calls with the same brush, and m8_brush_render paints the brush on
 * the CPU each time; the answer depends only on the brush and the draw mode's
 * background and text settings, so an identical pair gets the same answer. */
static DIB_Brush8   m8_bk_brush;
static short        m8_bk_mode;
static DWORD        m8_bk_bg;
static DWORD        m8_bk_fg;
static int          m8_bk_kind = -1;
static BYTE         m8_bk_c1;
static BYTE         m8_bk_c2;
static BYTE         m8_bk_pat[8];

static int m8_brush_colours( LPBRUSH brush, LPDRAWMODE dm, BYTE *c1, BYTE *c2, BYTE *pat )
{
    DIB_Brush8 FAR  *b = (DIB_Brush8 FAR *)brush;

    if( b == NULL || dm == NULL || (b->dp8BrushFlags & COLORSOLID) ) {
        return( m8_brush_render( brush, dm, c1, c2, pat ) );
    }
    if( m8_bk_kind >= 0 && m8_bk_mode == dm->bkMode && m8_bk_bg == dm->bkColor &&
        m8_bk_fg == dm->TextColor && _fmemcmp( &m8_bk_brush, b, sizeof( DIB_Brush8 ) ) == 0 ) {
        *c1 = m8_bk_c1;
        *c2 = m8_bk_c2;
        _fmemcpy( pat, m8_bk_pat, 8 );
        return( m8_bk_kind );
    }
    m8_bk_kind = m8_brush_render( brush, dm, &m8_bk_c1, &m8_bk_c2, m8_bk_pat );
    _fmemcpy( &m8_bk_brush, b, sizeof( DIB_Brush8 ) );
    m8_bk_mode = dm->bkMode;
    m8_bk_bg = dm->bkColor;
    m8_bk_fg = dm->TextColor;
    *c1 = m8_bk_c1;
    *c2 = m8_bk_c2;
    _fmemcpy( pat, m8_bk_pat, 8 );
    return( m8_bk_kind );
}

/* An 8x8 two-colour pattern over a rectangle: three port writes a row through
 * the pattern registers. When the result does not depend on the screen, only
 * the first eight rows are drawn and the rest are engine copies of them,
 * doubling each time, so the bus carries a few writes whatever the height. A
 * copy distance is a multiple of 8, so every row keeps its pattern row. */
static void m8_pattern_fill( int x, int y, int w, int h, const BYTE *pat,
                             BYTE c1, BYTE c2, WORD mix, int transparent )
{
    int     done = h;
    int     n;

    if( !transparent && mix == M8_MIX_REPLACE && h > 8 ) {
        done = 8;
    }
    m8_hw_pattern_rows( x, y, w, done, pat, c1, c2, mix, transparent ? M8_MIX_LEAVE : mix );
    while( done < h ) {
        n = h - done < done ? h - done : done;
        m8_hw_copy( x, y, x, y + done, w, n );
        done += n;
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
 * Two areas, sized by the mode so every mode has a cache: area 0 right of the
 * screen, from the top down to the string cache (none at 1024x768); area 1
 * below the screen, as wide as the screen (all 1024 columns when there is no
 * area 0), down to the string cache. SaveScreenBitmap takes area 1's top rows
 * when it saves, and the shelves there go first. Each area is cut into shelves
 * of one height class each, filled left to right; when it is full, the least
 * recently used run of adjacent shelves is cleared and cut again, so what is in
 * use survives one-off bitmaps. */
#define M8_BC_ENTRIES       160
#define M8_BC_SHELVES       64          /* more than the area's rows / 16 */
#define M8_BC_SEEN          64
#define M8_BC_MAX_PX        8192        /* a Solitaire card is 71x96 */
#define M8_BC_MAX_H         400         /* a proxy (below) may be a card stack's height */

typedef struct {
    DWORD   h1;
    DWORD   h2;
    WORD    w;                          /* 0: free */
    WORD    h;
    int     x;
    int     y;
    WORD    px;                         /* 1: a proxy's slot, never a cache hit */
} M8_BCENTRY;

static void m8_px_evicted( M8_BCENTRY *e );

typedef struct {
    int     y;
    int     h;
    int     x;                          /* next free column, from the area's left */
    DWORD   used;                       /* last store or hit in it; 0: empty */
} M8_BCSHELF;

static M8_BCENTRY   m8_bc[M8_BC_ENTRIES];
#define M8_BC_AREAS         2

static M8_BCSHELF   m8_bcs[M8_BC_AREAS][M8_BC_SHELVES];
static int          m8_bcs_n[M8_BC_AREAS];
static int          m8_bc_bottom[M8_BC_AREAS];  /* first row in no shelf */
static DWORD        m8_bc_clock;
static DWORD        m8_bc_seen[M8_BC_SEEN];
static int          m8_bc_seen_next;
static WORD         m8_bc_sel;

/* Hashes run on 16-bit operations only: the compiler turns every 32-bit shift
 * or multiply into a loop or a library call. A Fletcher pair (position
 * sensitive) and a rotate-xor, joined into two DWORDs once at the end. */
typedef struct {
    WORD    a;
    WORD    b;
    WORD    c;
} M8_HASH;

static void m8_hash_init( M8_HASH *h, WORD seed )
{
    h->a = seed;
    h->b = 0x5A5A;
    h->c = 0x1234;
}

static void m8_hash_bytes( M8_HASH *h, const BYTE FAR *p, WORD n )
{
    WORD    a = h->a;
    WORD    b = h->b;
    WORD    c = h->c;

    while( n-- != 0 ) {
        a += *p;
        b += a;
        c = ((c << 3) | (c >> 13)) ^ *p++;
    }
    h->a = a;
    h->b = b;
    h->c = c;
}

static void m8_hash_word( M8_HASH *h, WORD v )
{
    m8_hash_bytes( h, (const BYTE FAR *)&v, 2 );
}

static DWORD m8_hash_h1( const M8_HASH *h )
{
    return( ((DWORD)h->a << 16) | h->b );
}

static DWORD m8_hash_h2( const M8_HASH *h, WORD extra )
{
    return( ((DWORD)h->c << 16) | extra );
}

/* Row y of a bitmap's bits from column x, w bytes. Rows inside the bitmap's
 * own segment are reached by plain far pointer; only bits past 64 KB need
 * m8_bc_sel moved, which costs KERNEL calls into DPMI - per row, they made a
 * 96-row cache lookup take ~30 ms. */
static BYTE FAR *m8_bits_row( WORD sel, DWORD off, long delta, int x, int y, int w )
{
    DWORD at = off + (DWORD)((long)y * delta) + (DWORD)x;

    if( delta > 0 && at + (DWORD)w <= 0x10000UL ) {
        return( (BYTE FAR *)MK_FP( sel, (WORD)at ) );
    }
    SetSelectorBase( m8_bc_sel, GetSelectorBase( sel ) + at );
    SetSelectorLimit( m8_bc_sel, (DWORD)w - 1 );
    return( (BYTE FAR *)MK_FP( m8_bc_sel, 0 ) );
}

static int m8_bc_area_x( int a )
{
    return( a == 0 ? (M8_SCREEN_X + 1) & ~1 : 0 );
}

static int m8_bc_area_w( int a )
{
    int w0 = M8_CURSOR_SAVE_X - m8_bc_area_x( 0 );

    if( w0 < 32 ) {
        w0 = 0;
    }
    if( a == 0 ) {
        return( w0 );
    }
    return( w0 > 0 ? m8_bc_area_x( 0 ) : M8_PITCH );
}

static int m8_bc_area_top( int a )
{
    return( a == 0 ? 0 : M8_SCREEN_Y );
}

/* The area a slot at column x lies in. */
static int m8_bc_area_of( int x )
{
    return( m8_bc_area_w( 0 ) > 0 && x >= m8_bc_area_x( 0 ) ? 0 : 1 );
}

/* Off while DOS has the card: the engine writes nothing then, and an entry
 * recorded anyway would later copy whatever the slot held. */
static int m8_bc_on( void )
{
    return( m8_bc_sel != 0 && !m8_hw_off );
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
    for( k = 0; k < M8_BC_AREAS; ++k ) {
        m8_bcs_n[k] = 0;
        m8_bc_bottom[k] = m8_bc_area_top( k );
    }
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

static void m8_bc_free_rows( int a, int y0, int y1 )
{
    int k;

    for( k = 0; k < M8_BC_ENTRIES; ++k ) {
        if( m8_bc[k].w != 0 && m8_bc[k].y >= y0 && m8_bc[k].y < y1 &&
            m8_bc_area_of( m8_bc[k].x ) == a ) {
            if( m8_bc[k].px ) {
                m8_px_evicted( &m8_bc[k] );
            }
            m8_bc[k].w = 0;
        }
    }
}

static void m8_bc_touch( int x, int y )
{
    int a = m8_bc_area_of( x );
    int k;

    ++m8_bc_clock;
    for( k = 0; k < m8_bcs_n[a]; ++k ) {
        if( y >= m8_bcs[a][k].y && y < m8_bcs[a][k].y + m8_bcs[a][k].h ) {
            m8_bcs[a][k].used = m8_bc_clock;
            return;
        }
    }
}

/* Replace `remove` shelves at `at` by `insert` shelves, contents to be set. */
static void m8_bc_splice( int a, int at, int remove, int insert )
{
    M8_BCSHELF  *sh = m8_bcs[a];
    int         k;

    if( insert > remove ) {
        for( k = m8_bcs_n[a] - 1; k >= at + remove; --k ) {
            sh[k + insert - remove] = sh[k];
        }
    } else if( insert < remove ) {
        for( k = at + remove; k < m8_bcs_n[a]; ++k ) {
            sh[k + insert - remove] = sh[k];
        }
    }
    m8_bcs_n[a] += insert - remove;
}

static void m8_bc_shelf_set( int a, int k, int y, int h )
{
    m8_bcs[a][k].y = y;
    m8_bcs[a][k].h = h;
    m8_bcs[a][k].x = 0;
    m8_bcs[a][k].used = 0;
}

/* Clear the least recently used run of adjacent shelves at least hc rows tall
 * and cut a shelf of hc rows from its top: its index, or -1. An area's shelves
 * are in row order with no gaps between them, so a run is contiguous. */
static int m8_bc_evict( int a, int hc )
{
    M8_BCSHELF  *sh = m8_bcs[a];
    int         i;
    int         j;
    int         sum;
    int         best = -1;
    int         best_n = 0;
    int         best_sum = 0;
    int         y;
    int         parts;
    DWORD       age;
    DWORD       best_age = 0;

    for( i = 0; i < m8_bcs_n[a]; ++i ) {
        sum = 0;
        age = 0;
        for( j = i; j < m8_bcs_n[a] && sum < hc; ++j ) {
            sum += sh[j].h;
            if( sh[j].used > age ) {
                age = sh[j].used;
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
    y = sh[best].y;
    m8_bc_free_rows( a, y, y + best_sum );
    parts = (best_sum > hc && m8_bcs_n[a] - best_n + 2 <= M8_BC_SHELVES) ? 2 : 1;
    m8_bc_splice( a, best, best_n, parts );
    if( parts == 2 ) {
        m8_bc_shelf_set( a, best, y, hc );
        m8_bc_shelf_set( a, best + 1, y + hc, best_sum - hc );
    } else {
        m8_bc_shelf_set( a, best, y, best_sum );
    }
    return( best );
}

static int m8_bc_alloc_in( int a, int w, int h, int *x, int *y )
{
    M8_BCSHELF  *sh = m8_bcs[a];
    int         hc = (h + 15) & ~15;
    int         bottom = m8_sc_top();
    int         width = m8_bc_area_w( a );
    int         k;

    if( w > width || h > M8_BC_MAX_H || hc > bottom - m8_bc_area_top( a ) ) {
        return( 0 );
    }
    for( k = 0; k < m8_bcs_n[a]; ++k ) {
        if( sh[k].h == hc && sh[k].x + w <= width ) {
            break;
        }
    }
    if( k == m8_bcs_n[a] ) {
        if( m8_bcs_n[a] < M8_BC_SHELVES && m8_bc_bottom[a] + hc <= bottom ) {
            m8_bc_shelf_set( a, m8_bcs_n[a]++, m8_bc_bottom[a], hc );
            m8_bc_bottom[a] += hc;
        } else {
            /* The rows left at the bottom join the area as one empty shelf. */
            if( m8_bc_bottom[a] < bottom && m8_bcs_n[a] < M8_BC_SHELVES ) {
                m8_bc_shelf_set( a, m8_bcs_n[a]++, m8_bc_bottom[a], bottom - m8_bc_bottom[a] );
                m8_bc_bottom[a] = bottom;
            }
            k = m8_bc_evict( a, hc );
            if( k < 0 ) {
                return( 0 );
            }
        }
    }
    *x = m8_bc_area_x( a ) + sh[k].x;
    *y = sh[k].y;
    sh[k].x += w;
    sh[k].used = ++m8_bc_clock;
    return( 1 );
}

/* Area 0 first: area 1's rows are SaveScreenBitmap's when a menu opens. */
static int m8_bc_alloc( int w, int h, int *x, int *y )
{
    return( m8_bc_alloc_in( 0, w, h, x, y ) || m8_bc_alloc_in( 1, w, h, x, y ) );
}

/* SaveScreenBitmap takes area 1's rows above y1: the shelves reaching into
 * them go, their proxies written back first. */
static void m8_bc_claim( int y1 )
{
    int n = 0;

    while( n < m8_bcs_n[1] && m8_bcs[1][n].y < y1 ) {
        ++n;
    }
    if( n > 0 ) {
        m8_bc_free_rows( 1, m8_bcs[1][0].y, m8_bcs[1][n - 1].y + m8_bcs[1][n - 1].h );
        m8_bc_splice( 1, 0, n, 0 );
    }
    if( m8_bcs_n[1] == 0 && m8_bc_bottom[1] < y1 ) {
        m8_bc_bottom[1] = y1;
    }
}

/* A free entry; when all are taken, the least recently used shelf is cleared. */
static M8_BCENTRY *m8_bc_slot( void )
{
    int a;
    int k;
    int old_a = -1;
    int old = -1;

    for( k = 0; k < M8_BC_ENTRIES; ++k ) {
        if( m8_bc[k].w == 0 ) {
            return( &m8_bc[k] );
        }
    }
    for( a = 0; a < M8_BC_AREAS; ++a ) {
        for( k = 0; k < m8_bcs_n[a]; ++k ) {
            if( m8_bcs[a][k].used != 0 &&
                (old < 0 || m8_bcs[a][k].used < m8_bcs[old_a][old].used) ) {
                old_a = a;
                old = k;
            }
        }
    }
    if( old < 0 ) {
        return( NULL );
    }
    M8_STEP( M8_MARK_BC_EVICT );
    m8_bc_free_rows( old_a, m8_bcs[old_a][old].y, m8_bcs[old_a][old].y + m8_bcs[old_a][old].h );
    m8_bcs[old_a][old].x = 0;
    m8_bcs[old_a][old].used = 0;
    for( k = 0; k < M8_BC_ENTRIES; ++k ) {
        if( m8_bc[k].w == 0 ) {
            return( &m8_bc[k] );
        }
    }
    return( NULL );
}

/* The cache entry holding w x h of an 8-bit memory bitmap at (sx,sy), stored
 * now if need be; NULL if it cannot be cached. `force` stores on first sight:
 * a proxy's slot needs the pixels now, and the alternative is a write-back. */
static M8_BCENTRY *m8_bc_lookup( LPDIBENGINE src, int sx, int sy, int w, int h, int force )
{
    WORD        we = (WORD)((w + 1) & ~1);
    long        delta;
    DWORD       h1;
    DWORD       h2;
    M8_HASH     hs;
    int         row;
    int         k;
    int         nested = m8_busy;
    BYTE FAR    *p;
    BYTE FAR    *q;
    M8_BCENTRY  *e = NULL;
    M8_RECT     r;

    if( !m8_bc_on() || src->deBitsPixel != 8 || (src->deFlags & SELECTEDDIB) ||
        (long)we * h > M8_BC_MAX_PX || (long)we * h > 0xFFF0L ) {
        return( NULL );
    }
    M8_TP( 0 );
    m8_hash_init( &hs, (WORD)w );
    delta = (long)src->deDeltaScan;
    for( row = 0; row < h; ++row ) {
        p = m8_bits_row( src->deBitsSelector, src->deBitsOffset, delta, sx, sy + row, w );
        q = m8_src.bits + (DWORD)row * we;
        _fmemcpy( q, p, w );
        m8_hash_bytes( &hs, p, (WORD)w );
        if( we != (WORD)w ) {
            q[w] = 0;
        }
    }
    h1 = m8_hash_h1( &hs );
    h2 = m8_hash_h2( &hs, (WORD)h );
    M8_TP( 1 );
    for( k = 0; k < M8_BC_ENTRIES; ++k ) {
        if( !m8_bc[k].px && m8_bc[k].w == (WORD)w && m8_bc[k].h == (WORD)h && m8_bc[k].h1 == h1 &&
            m8_bc[k].h2 == h2 ) {
            e = &m8_bc[k];
            M8_STEP( M8_MARK_BC_HIT );
            m8_bc_touch( e->x, e->y );
            M8_TP( 2 );
            return( e );
        }
    }
    M8_TP( 2 );
    if( !m8_bc_seen_before( h1 ^ h2 ) && !force ) {
        M8_STEP( M8_MARK_BC_FIRST );
        return( NULL );
    }
    e = m8_bc_slot();
    if( e == NULL || !m8_bc_alloc( we, h, &e->x, &e->y ) ) {
        return( NULL );
    }
    e->h1 = h1;
    e->h2 = h2;
    e->w = (WORD)w;
    e->h = (WORD)h;
    e->px = 0;
    M8_STEP( M8_MARK_BC_STORE );
    m8_rect_set( &r, e->x, e->y, e->x + we, e->y + h );
    if( !nested ) {
        m8_enter( &r );
    }
    m8_hw_write_rect( e->x, e->y, we, h, m8_src.bits, we );
    if( !nested ) {
        m8_leave();
    }
    return( e );
}

/* Copy w x h of an 8-bit memory bitmap to the screen through the cache: 1 if
 * done. */
static int m8_bc_draw( LPDIBENGINE src, int sx, int sy, int dx, int dy, int w, int h, WORD mix )
{
    M8_BCENTRY  *e = m8_bc_lookup( src, sx, sy, w, h, 0 );
    M8_RECT     r;

    if( e == NULL ) {
        return( 0 );
    }
    m8_rect_set( &r, dx, dy, dx + w, dy + h );
    m8_enter( &r );
    M8_TP( 3 );
    m8_hw_copy_mix( e->x, e->y, dx, dy, w, h, M8_MIX_SRC_VRAM | mix );
    M8_TP( 4 );
    m8_leave();
    M8_TP( 5 );
    return( 1 );
}

/* Memory bitmaps kept on the card (proxies). A copy from the screen into a
 * memory bitmap - Solitaire saving what is under a dragged card - goes by
 * engine copy to a bitmap-cache slot instead of over the bus: the bitmap's RAM
 * is left stale and the slot holds its pixels. Copies between proxied bitmaps,
 * from a cached bitmap into a proxied one, and from a proxy to the screen are
 * engine copies too, so a whole drag frame stays on the card. Any other use of the bitmap writes the slot back to RAM
 * first: the C entry points call m8_px_touch, the entries forwarded to the DIB
 * Engine call M8PXFLUSH or M8PXREALIZE (thunks.asm). Device bitmaps only - a
 * DIB section's bits are the application's to read. GDI creates and deletes
 * every colour bitmap through RealizeObject (OBJ_BITMAP), so a deleted
 * bitmap's proxy is dropped there; one whose header no longer matches is
 * dropped without writing. M8_PROXY 0 turns the whole path off. */
#define M8_PROXY            1
#define M8_PX_MAX           16
#define M8_PX_MAX_PX        0xFFF0      /* the write-back goes through m8_dst */

/* Why a proxy was written back (the word after M8_MARK_PX_BACK). */
#define M8_PX_WHY_ENTRY     1           /* a C entry point used the bitmap */
#define M8_PX_WHY_MEM       2           /* memory-to-memory BitBlt */
#define M8_PX_WHY_FORWARD   3           /* an entry forwarded to the DIB Engine */
#define M8_PX_WHY_EVICT     4           /* its slot's shelf was evicted, or the table was full */
#define M8_PX_WHY_SUSPEND   5           /* DOS or a mode change takes the card */
#define M8_PX_WHY_OVERLAP   6           /* a new save overlaps it in part */

typedef struct {
    LPDIBENGINE bmp;                    /* NULL: free */
    WORD        sel;
    DWORD       off;
    DWORD       delta;
    WORD        bw;
    WORD        bh;
    int         x;                      /* rectangle in the bitmap */
    int         y;
    int         w;
    int         h;
    int         lead;                   /* slot columns before x: the slot copy starts on an even column */
    M8_BCENTRY  *e;
    int         unfilled;               /* slot not yet written: the RAM is still current */
} M8_PXENTRY;

static M8_PXENTRY   m8_px[M8_PX_MAX];
static int          m8_px_n;

/* Row `row` of the proxy's rectangle in the bitmap. */
static BYTE FAR *m8_px_row( const M8_PXENTRY *p, int row )
{
    return( m8_bits_row( p->sel, p->off, (long)p->delta, p->x, p->y + row, p->w ) );
}

/* The bitmap is the one the proxy was made for. Test the pointer before
 * reading through it: GDI's heap may have reused it. */
static int m8_px_valid( const M8_PXENTRY *p )
{
    LPDIBENGINE b = p->bmp;

    if( IsBadReadPtr( b, sizeof( DIBENGINE ) ) || b->deType != TYPE_DIBENG || b->deBitsSelector != p->sel || b->deBitsOffset != p->off ||
        b->deDeltaScan != p->delta || b->deWidth != p->bw || b->deHeight != p->bh ||
        GetSelectorBase( p->sel ) == 0 ||
        IsBadWritePtr( MK_FP( p->sel, (WORD)p->off ), 1 ) ) {
        return( 0 );
    }
    return( 1 );
}

static void m8_px_drop( M8_PXENTRY *p, int keep_slot )
{
    if( !keep_slot ) {
        p->e->w = 0;
        p->e->px = 0;
    }
    p->bmp = NULL;
    --m8_px_n;
}

/* Copy the slot back into the bitmap's RAM, then forget the proxy. */
static void m8_px_writeback( M8_PXENTRY *p, WORD why )
{
    WORD        we = p->e->w;
    int         nested = m8_busy;
    int         row;
    BYTE FAR    *q;
    BYTE FAR    *s;
    M8_RECT     sr;

    if( p->unfilled ) {
        m8_px_drop( p, 0 );
        return;
    }
    /* With the card handed to DOS, the slot cannot be read. */
    if( m8_hw_off || !m8_px_valid( p ) ) {
        M8_STEP( M8_MARK_PX_LOST );
        m8_px_drop( p, 0 );
        return;
    }
    M8_STEP( M8_MARK_PX_BACK );
    M8_STEP( why );
    m8_rect_set( &sr, p->e->x, p->e->y, p->e->x + we, p->e->y + p->h );
    if( !nested ) {
        m8_enter( &sr );
    }
    m8_hw_read_rect( p->e->x, p->e->y, we, p->h, m8_dst.bits, we );
    if( !nested ) {
        m8_leave();
    }
    for( row = 0; row < p->h; ++row ) {
        q = m8_px_row( p, row );
        s = m8_dst.bits + (DWORD)row * we + p->lead;
        _fmemcpy( q, s, p->w );
    }
    m8_px_drop( p, 0 );
}

/* Write back every proxy of `dev` (all of them for NULL). */
static void m8_px_flush( LPVOID dev, WORD why )
{
    int k;

    for( k = 0; k < M8_PX_MAX && m8_px_n > 0; ++k ) {
        if( m8_px[k].bmp != NULL && (dev == NULL || m8_px[k].bmp == dev) ) {
            m8_px_writeback( &m8_px[k], why );
        }
    }
}

static void m8_px_touch( LPVOID dev, WORD why )
{
    if( m8_px_n > 0 && dev != NULL && !m8_is_screen( dev ) ) {
        m8_px_flush( dev, why );
    }
}

/* The bitmap cache is about to reuse a proxy's slot. */
static void m8_px_evicted( M8_BCENTRY *e )
{
    int k;

    for( k = 0; k < M8_PX_MAX; ++k ) {
        if( m8_px[k].bmp != NULL && m8_px[k].e == e ) {
            m8_px_writeback( &m8_px[k], M8_PX_WHY_EVICT );
            return;
        }
    }
}

/* Forwarded entries that may read or write a bitmap (thunks.asm). */
VOID WINAPI __loadds M8PXFLUSH( VOID )
{
    if( m8_px_n > 0 ) {
        m8_px_flush( NULL, M8_PX_WHY_FORWARD );
    }
}

/* RealizeObject reads a bitmap for a pattern brush, and deletes bitmaps.
 * GDI's style for a bitmap is 5; no DDK header names it. */
#define M8_OBJ_BITMAP       5

VOID WINAPI __loadds M8PXREALIZE( short iStyle, LPVOID lpInObj )
{
    int k;

    /* A bitmap being deleted: its proxies go without a write-back. */
    if( m8_px_n > 0 && iStyle == -M8_OBJ_BITMAP && lpInObj != NULL ) {
        for( k = 0; k < M8_PX_MAX; ++k ) {
            if( m8_px[k].bmp == lpInObj ) {
                m8_px_drop( &m8_px[k], 0 );
            }
        }
        return;
    }
    if( m8_px_n > 0 && iStyle == OBJ_BRUSH && lpInObj != NULL &&
        ((LPLOGBRUSH)lpInObj)->lbStyle == BS_PATTERN ) {
        m8_px_flush( NULL, M8_PX_WHY_FORWARD );
    }
}

/* The live proxy of bitmap b holding the whole rectangle, or NULL. One that
 * fails the check is dropped. */
static M8_PXENTRY *m8_px_find( LPVOID b, int x, int y, int w, int h )
{
    M8_PXENTRY  *p;
    int         k;

    for( k = 0; k < M8_PX_MAX && m8_px_n > 0; ++k ) {
        p = &m8_px[k];
        if( p->bmp == b && x >= p->x && y >= p->y && x + w <= p->x + p->w &&
            y + h <= p->y + p->h ) {
            if( !m8_px_valid( p ) ) {
                M8_STEP( M8_MARK_PX_LOST );
                m8_px_drop( p, 0 );
                return( NULL );
            }
            return( p );
        }
    }
    return( NULL );
}

/* A device bitmap the proxies can hold: 8-bit, the DIB Engine's own, not a DIB
 * section, and the rectangle inside it. */
static int m8_px_ok( LPDIBENGINE b, int x, int y, int w, int h )
{
    return( m8_bc_on() && b != NULL && b->deType == TYPE_DIBENG && b->deBitsPixel == 8 &&
            !(b->deFlags & (SELECTEDDIB | OFFSCREEN | NOT_FRAMEBUFFER)) &&
            !(m8_screen->deFlags & PALETTE_XLAT) && w > 0 && h > 0 && x >= 0 && y >= 0 &&
            x + w <= (int)b->deWidth && y + h <= (int)b->deHeight );
}

/* A new proxy for the rectangle, its slot not yet filled; e = a slot to reuse.
 * The caller fills the slot at once, or drops the proxy without a write-back
 * (the RAM is still current until the slot is filled). */
static M8_PXENTRY *m8_px_new( LPDIBENGINE b, int x, int y, int w, int h, int lead, M8_BCENTRY *e )
{
    WORD        we = (WORD)((lead + w + 1) & ~1);
    M8_PXENTRY  *p = NULL;
    int         k;

    if( (long)we * h > M8_PX_MAX_PX ) {
        return( NULL );
    }
    for( k = 0; k < M8_PX_MAX; ++k ) {
        if( m8_px[k].bmp == NULL ) {
            p = &m8_px[k];
            break;
        }
    }
    if( p == NULL ) {
        p = &m8_px[0];
        m8_px_writeback( p, M8_PX_WHY_EVICT );
    }
    if( e == NULL ) {
        e = m8_bc_slot();
        if( e == NULL || !m8_bc_alloc( we, h, &e->x, &e->y ) ) {
            return( NULL );
        }
    }
    e->h1 = 0;
    e->h2 = 0;
    e->w = we;
    e->h = (WORD)h;
    e->px = 1;
    p->sel = b->deBitsSelector;
    p->off = b->deBitsOffset;
    p->delta = b->deDeltaScan;
    p->bw = b->deWidth;
    p->bh = b->deHeight;
    p->x = x;
    p->y = y;
    p->w = w;
    p->h = h;
    p->lead = lead;
    p->e = e;
    p->bmp = b;
    p->unfilled = 1;
    ++m8_px_n;
    m8_bc_touch( e->x, e->y );
    return( p );
}

static int m8_px_has( LPVOID b )
{
    int k;

    for( k = 0; k < M8_PX_MAX && m8_px_n > 0; ++k ) {
        if( m8_px[k].bmp != NULL && m8_px[k].bmp == b ) {
            return( 1 );
        }
    }
    return( 0 );
}

/* Slot coordinates of bitmap point (x,y) inside proxy p. */
#define M8_PX_SX( P, X )    ((P)->e->x + (P)->lead + ((X) - (P)->x))
#define M8_PX_SY( P, Y )    ((P)->e->y + ((Y) - (P)->y))

/* Write back the proxies of dev that overlap the rectangle: what a RAM
 * operation on that rectangle needs current. */
static void m8_px_touch_rect( LPVOID dev, int x, int y, int w, int h, WORD why )
{
    M8_PXENTRY  *p;
    int         k;

    for( k = 0; k < M8_PX_MAX && m8_px_n > 0; ++k ) {
        p = &m8_px[k];
        if( p->bmp != NULL && p->bmp == dev && p->x < x + w && x < p->x + p->w &&
            p->y < y + h && y < p->y + p->h ) {
            m8_px_writeback( p, why );
        }
    }
}

/* Screen (sx,sy) to bitmap (dx,dy), w x h, SRCCOPY: 1 if done by proxy. */
static int m8_px_save( LPDIBENGINE b, int dx, int dy, int sx, int sy, int w, int h )
{
    int         sx0 = sx & ~1;
    int         lead = sx - sx0;
    WORD        we = (WORD)((lead + w + 1) & ~1);
    M8_BCENTRY  *e = NULL;
    M8_PXENTRY  *p;
    M8_RECT     r;
    int         k;
    int         fx;
    int         tx;

    if( !m8_px_ok( b, dx, dy, w, h ) || sx < 0 || sy < 0 || sx + w > M8_SCREEN_X ||
        sy + h > M8_SCREEN_Y ) {
        return( 0 );
    }
    /* Already held: the screen goes straight into that slot. */
    p = m8_px_find( b, dx, dy, w, h );
    if( p != NULL ) {
        fx = sx;
        tx = M8_PX_SX( p, dx );
    } else {
        if( (long)we * h > M8_PX_MAX_PX ) {
            return( 0 );
        }
        /* Proxies the new rectangle overlaps: one inside it with the same slot
         * size lends its slot; the rest go back to RAM. */
        for( k = 0; k < M8_PX_MAX; ++k ) {
            M8_PXENTRY *o = &m8_px[k];

            if( o->bmp != b || o->x >= dx + w || dx >= o->x + o->w || o->y >= dy + h ||
                dy >= o->y + o->h ) {
                continue;
            }
            if( e == NULL && o->x >= dx && o->y >= dy && o->x + o->w <= dx + w &&
                o->y + o->h <= dy + h && o->e->w == we && o->h == h ) {
                e = o->e;
                m8_px_drop( o, 1 );
            } else {
                m8_px_writeback( o, M8_PX_WHY_OVERLAP );
            }
        }
        p = m8_px_new( b, dx, dy, w, h, lead, e );
        if( p == NULL ) {
            return( 0 );
        }
        fx = sx0;
        tx = p->e->x;
        w = we;
    }
    M8_STEP( M8_MARK_PX_SAVE );
    m8_bc_touch( p->e->x, p->e->y );
    m8_rect_set( &r, fx, sy, fx + w, sy + h );
    m8_enter( &r );
    m8_hw_copy( fx, sy, tx, M8_PX_SY( p, dy ), w, h );
    m8_leave();
    p->unfilled = 0;
    return( 1 );
}

#ifdef M8_TRACE
#define M8_PX_NO_( why )    { M8_STEP( M8_MARK_PX_M2M_NO ); M8_STEP( why ); }
#else
#define M8_PX_NO_( why )
#endif

/* The pieces of rectangle (x,y,w,h) of bitmap d, each held by a proxy, for an
 * operation that writes the whole rectangle. One proxy holding all of it: one
 * piece. Otherwise, when the operation ignores the destination (`fresh`), the
 * part inside one overlapping proxy plus new proxies for the rest - nothing
 * needs reading, everything will be overwritten. 0 pieces: not possible. The
 * caller passes fresh only where a new proxy pays: the bitmap is already in use
 * on the card, or the source is. */
#define M8_PX_PIECES        5

typedef struct {
    M8_PXENTRY  *p;
    int         x;
    int         y;
    int         w;
    int         h;
    int         made;           /* a new proxy, slot not yet filled */
} M8_PXPIECE;

/* Drop the new, unfilled proxies among the pieces. */
static void m8_px_pieces_drop( LPDIBENGINE d, M8_PXPIECE *pc, int n )
{
    int k;

    for( k = 0; k < n; ++k ) {
        if( pc[k].made && pc[k].p->bmp == d ) {
            m8_px_drop( pc[k].p, 0 );
        }
    }
}

static int m8_px_pieces( LPDIBENGINE d, int x, int y, int w, int h, int fresh, M8_PXPIECE *pc )
{
    M8_PXENTRY  *o = NULL;
    M8_RECT     band[4];
    int         n = 0;
    int         nb = 0;
    int         k;
    int         mt;
    int         mb;

    if( !m8_px_ok( d, x, y, w, h ) ) {
        return( 0 );
    }
    for( k = 0; k < M8_PX_MAX; ++k ) {
        M8_PXENTRY *p = &m8_px[k];

        if( p->bmp == d && p->x < x + w && x < p->x + p->w && p->y < y + h && y < p->y + p->h ) {
            if( o != NULL ) {
                return( 0 );
            }
            o = p;
        }
    }
    if( o != NULL && !m8_px_valid( o ) ) {
        M8_STEP( M8_MARK_PX_LOST );
        m8_px_drop( o, 0 );
        o = NULL;
    }
    if( o != NULL && x >= o->x && y >= o->y && x + w <= o->x + o->w && y + h <= o->y + o->h ) {
        pc[0].p = o;
        pc[0].x = x;
        pc[0].y = y;
        pc[0].w = w;
        pc[0].h = h;
        pc[0].made = 0;
        return( 1 );
    }
    if( !fresh ) {
        return( 0 );
    }
    if( o == NULL ) {
        m8_rect_set( &band[nb++], x, y, x + w, y + h );
    } else {
        mt = y > o->y ? y : o->y;
        mb = y + h < o->y + o->h ? y + h : o->y + o->h;
        if( y < o->y ) {
            m8_rect_set( &band[nb++], x, y, x + w, o->y );
        }
        if( y + h > o->y + o->h ) {
            m8_rect_set( &band[nb++], x, o->y + o->h, x + w, y + h );
        }
        if( x < o->x ) {
            m8_rect_set( &band[nb++], x, mt, o->x, mb );
        }
        if( x + w > o->x + o->w ) {
            m8_rect_set( &band[nb++], o->x + o->w, mt, x + w, mb );
        }
        pc[n].p = o;
        pc[n].x = x > o->x ? x : o->x;
        pc[n].y = mt;
        pc[n].w = (x + w < o->x + o->w ? x + w : o->x + o->w) - pc[n].x;
        pc[n].h = mb - mt;
        pc[n].made = 0;
        ++n;
    }
    for( k = 0; k < nb; ++k ) {
        pc[n].p = m8_px_new( d, band[k].l, band[k].t, band[k].r - band[k].l,
                             band[k].b - band[k].t, 0, NULL );
        if( pc[n].p == NULL ) {
            m8_px_pieces_drop( d, pc, n );
            return( 0 );
        }
        pc[n].x = band[k].l;
        pc[n].y = band[k].t;
        pc[n].w = band[k].r - band[k].l;
        pc[n].h = band[k].b - band[k].t;
        pc[n].made = 1;
        ++n;
    }
    return( n );
}

/* Every piece's proxy still alive after slots were allocated (an allocation can
 * evict); if not, the new ones are dropped - their RAM is still current. */
static int m8_px_pieces_live( LPDIBENGINE d, M8_PXPIECE *pc, int n )
{
    int k;
    int live = 1;

    for( k = 0; k < n; ++k ) {
        if( pc[k].p->bmp != d ) {
            live = 0;
        }
    }
    if( !live ) {
        m8_px_pieces_drop( d, pc, n );
    }
    return( live );
}

/* Memory to memory into a bitmap held on the card: from another proxy's slot,
 * or from the source's bitmap-cache entry. `fresh`: the ROP ignores the
 * destination. 1 if done. */
static int m8_px_m2m( LPVOID d, LPVOID s, int dx, int dy, int sx, int sy, int w, int h, WORD mix,
                      int fresh )
{
    LPDIBENGINE db = d;
    LPDIBENGINE sb = s;
    M8_PXPIECE  pc[M8_PX_PIECES];
    M8_PXENTRY  *ps;
    M8_BCENTRY  *e = NULL;
    int         n;
    int         k;
    int         made = 0;
    int         fx;
    int         fy;
    M8_RECT     r;

    if( m8_hw_off || m8_px_n == 0 || w <= 0 || h <= 0 || sb == NULL ||
        sb->deType != TYPE_DIBENG ) {
        return( 0 );
    }
    ps = m8_px_find( s, sx, sy, w, h );
    n = m8_px_pieces( db, dx, dy, w, h, fresh && (ps != NULL || m8_px_has( d )), pc );
    if( n == 0 ) {
        M8_PX_NO_( 1 );
        return( 0 );
    }
    for( k = 0; k < n; ++k ) {
        made |= pc[k].made;
    }
    if( ps == NULL ) {
        /* From RAM through the cache: never from the destination itself, and a
         * first sight is stored only when it saves a write-back. */
        if( s == d || sx < 0 || sy < 0 || sx + w > (int)sb->deWidth ||
            sy + h > (int)sb->deHeight ) {
            m8_px_pieces_drop( db, pc, n );
            M8_PX_NO_( 2 );
            return( 0 );
        }
        m8_px_touch_rect( s, sx, sy, w, h, M8_PX_WHY_ENTRY );
        e = m8_bc_lookup( sb, sx, sy, w, h, !made );
    }
    if( (ps == NULL && e == NULL) || (ps != NULL && ps->bmp != s) ||
        !m8_px_pieces_live( db, pc, n ) ) {
        m8_px_pieces_drop( db, pc, n );
        M8_PX_NO_( ps == NULL && e == NULL ? 3 : 4 );
        return( 0 );
    }
    M8_STEP( M8_MARK_PX_M2M );
    for( k = 0; k < n; ++k ) {
        if( ps != NULL ) {
            fx = M8_PX_SX( ps, sx + (pc[k].x - dx) );
            fy = M8_PX_SY( ps, sy + (pc[k].y - dy) );
            m8_bc_touch( ps->e->x, ps->e->y );
        } else {
            fx = e->x + (pc[k].x - dx);
            fy = e->y + (pc[k].y - dy);
        }
        m8_bc_touch( pc[k].p->e->x, pc[k].p->e->y );
        m8_rect_set( &r, M8_PX_SX( pc[k].p, pc[k].x ), M8_PX_SY( pc[k].p, pc[k].y ),
                     M8_PX_SX( pc[k].p, pc[k].x ) + pc[k].w, M8_PX_SY( pc[k].p, pc[k].y ) + pc[k].h );
        m8_enter( &r );
        m8_hw_copy_mix( fx, fy, r.l, r.t, pc[k].w, pc[k].h, M8_MIX_SRC_VRAM | mix );
        m8_leave();
        pc[k].p->unfilled = 0;
    }
    return( 1 );
}

/* A solid fill into a memory bitmap, on the card when the bitmap is held or
 * the fill ignores the destination. 1 if done. */
static int m8_px_fill( LPVOID d, int x, int y, int w, int h, BYTE colour, WORD mix, int fresh )
{
    LPDIBENGINE db = d;
    M8_PXPIECE  pc[M8_PX_PIECES];
    int         n;
    int         k;
    M8_RECT     r;

    if( m8_hw_off || m8_px_n == 0 ) {
        return( 0 );
    }
    n = m8_px_pieces( db, x, y, w, h, fresh && m8_px_has( d ), pc );
    if( n == 0 || !m8_px_pieces_live( db, pc, n ) ) {
        return( 0 );
    }
    M8_STEP( M8_MARK_PX_FILL );
    for( k = 0; k < n; ++k ) {
        m8_bc_touch( pc[k].p->e->x, pc[k].p->e->y );
        m8_rect_set( &r, M8_PX_SX( pc[k].p, pc[k].x ), M8_PX_SY( pc[k].p, pc[k].y ),
                     M8_PX_SX( pc[k].p, pc[k].x ) + pc[k].w, M8_PX_SY( pc[k].p, pc[k].y ) + pc[k].h );
        m8_enter( &r );
        m8_hw_fill_mix( r.l, r.t, pc[k].w, pc[k].h, colour, M8_MIX_SRC_FRGD | mix );
        m8_leave();
        pc[k].p->unfilled = 0;
    }
    return( 1 );
}

/* Bitmap (sx,sy) to screen (dx,dy) through a source mix: 1 if a proxy held
 * the whole source rectangle. */
/* Upload a whole device bitmap into a new proxy, so it is drawn from the card
 * from now on. Left unfilled: the RAM still matches, so a RAM use only drops
 * it. One segment only, as m8_hw_write_rect reads it through one pointer. */
static int m8_px_upload( LPDIBENGINE b )
{
    M8_PXENTRY  *p;
    M8_RECT     r;
    int         w;
    int         h;
    WORD        we;

    if( b == NULL || b->deType != TYPE_DIBENG || m8_hw_off ) {
        return( 0 );
    }
    w = (int)b->deWidth;
    h = (int)b->deHeight;
    we = (WORD)((w + 1) & ~1);
    if( !m8_px_ok( b, 0, 0, w, h ) || (long)b->deDeltaScan < (long)we ||
        b->deBitsOffset + (DWORD)b->deDeltaScan * h > 0x10000UL || m8_px_has( b ) ) {
        return( 0 );
    }
    p = m8_px_new( b, 0, 0, w, h, 0, NULL );
    if( p == NULL ) {
        return( 0 );
    }
    M8_STEP( M8_MARK_PX_UPLOAD );
    m8_rect_set( &r, p->e->x, p->e->y, p->e->x + we, p->e->y + h );
    m8_enter( &r );
    m8_hw_write_rect( p->e->x, p->e->y, we, h,
                      (const BYTE FAR *)MK_FP( b->deBitsSelector, (WORD)b->deBitsOffset ),
                      (WORD)b->deDeltaScan );
    m8_leave();
    return( 1 );
}

static int m8_px_restore( LPDIBENGINE b, int sx, int sy, int dx, int dy, int w, int h, WORD mix )
{
    M8_PXENTRY  *p;
    M8_RECT     r;
    int         k;

    if( m8_px_n == 0 ) {
        return( 0 );
    }
    for( k = 0; k < M8_PX_MAX; ++k ) {
        p = &m8_px[k];
        if( p->bmp == b && sx >= p->x && sy >= p->y && sx + w <= p->x + p->w &&
            sy + h <= p->y + p->h ) {
            if( !m8_px_valid( p ) ) {
                M8_STEP( M8_MARK_PX_LOST );
                m8_px_drop( p, 0 );
                return( 0 );
            }
            M8_TP( 10 );
            M8_STEP( M8_MARK_PX_HIT );
            m8_bc_touch( p->e->x, p->e->y );
            m8_rect_set( &r, dx, dy, dx + w, dy + h );
            m8_enter( &r );
            M8_TP( 11 );
            m8_hw_copy_mix( p->e->x + p->lead + (sx - p->x), p->e->y + (sy - p->y), dx, dy, w, h,
                            M8_MIX_SRC_VRAM | mix );
            m8_leave();
            return( 1 );
        }
    }
    return( 0 );
}

/* A monochrome bitmap to the screen: the engine expands it, 1 bit per pixel on
 * the bus instead of 8, and applies the ROP's mix itself (icon masks need no
 * read-back). GDI's rule for mono to colour: a 1 bit takes the background
 * colour, a 0 bit the text colour. The source rectangle is repacked to start
 * on a byte; rows run on unpadded, as m8_hw_mono streams them, and the
 * scissors cut each row back to w. Single-segment bitmaps only. */
static int m8_mono_blt( BITMAP FAR *bm, int sx, int sy, int dx, int dy, int w, int h, WORD mix,
                        LPDRAWMODE dm )
{
    WORD        wb = (WORD)((w + 7) / 8);
    int         shift = sx & 7;
    WORD        first = (WORD)(sx >> 3);
    BYTE FAR    *row;
    BYTE FAR    *out = m8_dst.bits;
    WORD        i;
    WORD        k;
    int         y;
    M8_RECT     r;
    M8_RECT     scr;

    if( bm == NULL || dm == NULL || bm->bmType != 0 || bm->bmBitsPixel != 1 ||
        bm->bmPlanes != 1 || bm->bmSegmentIndex != 0 || bm->bmBits == NULL ||
        w <= 0 || h <= 0 || sx < 0 || sy < 0 || (long)wb * h > M8_BUF_BYTES ) {
        return( 0 );
    }
    for( y = 0; y < h; ++y ) {
        row = bm->bmBits + (WORD)(sy + y) * bm->bmWidthBytes;
        for( i = 0; i < wb; ++i ) {
            k = first + i;
            if( shift == 0 ) {
                *out++ = row[k];
            } else {
                *out++ = (BYTE)((row[k] << shift) |
                                (k + 1 < bm->bmWidthBytes ? row[k + 1] >> (8 - shift) : 0));
            }
        }
    }
    m8_rect_set( &r, dx, dy, dx + w, dy + h );
    m8_rect_screen( &scr );
    m8_rect_and( &r, &scr );
    if( m8_rect_empty( &r ) ) {
        return( 1 );
    }
    M8_STEP( M8_MARK_MONO_BLT );
    m8_enter( &r );
    m8_hw_mono( dx, dy, wb, (WORD)h, m8_dst.bits,
                M8_MIX_SRC_FRGD | mix, (BYTE)dm->bkColor,
                M8_MIX_SRC_BKGD | mix, (BYTE)dm->TextColor,
                r.l, r.t, r.r - 1, r.b - 1 );
    m8_leave();
    return( 1 );
}

#ifdef M8_TRACE
/* A 16-bit value as two bytes, so it can never read as a marker. */
static void m8_step_word( WORD v )
{
    M8_STEP( v >> 8 );
    M8_STEP( v & 0xFF );
}

/* Memory-to-memory copies: which bitmaps, how big - the data for keeping
 * bitmaps on the card. Pointers as segment and offset. */
static void m8_trace_mem( WORD mark, LPVOID dst, LPVOID src, WORD rop, int w, int h, int dx,
                          int dy, int sx, int sy )
{
    M8_STEP( mark );
    m8_step_word( FP_SEG( dst ) );
    m8_step_word( FP_OFF( dst ) );
    m8_step_word( FP_SEG( src ) );
    m8_step_word( FP_OFF( src ) );
    m8_step_word( rop );
    m8_step_word( (WORD)w );
    m8_step_word( (WORD)h );
    m8_step_word( (WORD)dx );
    m8_step_word( (WORD)dy );
    m8_step_word( (WORD)sx );
    m8_step_word( (WORD)sy );
}
#else
#define m8_trace_mem( m, d, s, r, w, h, dx, dy, sx, sy )
#endif

#if defined( M8_TRACE ) || defined( M8_PROF )
/* Entry timing. PIT channel 2 runs free at 1.193 MHz and wraps every 55 ms; the
 * BIOS tick count beside it gives the wraps. Channel 2 feeds only the speaker,
 * which stays off (port 61h bit 1 clear). */
static void m8_tm_init( void )
{
    outp( 0x61, (inp( 0x61 ) & ~0x02) | 0x01 );
    outp( 0x43, 0xB4 );
    outp( 0x42, 0 );
    outp( 0x42, 0 );
}
#endif

#ifdef M8_TRACE
/* A marker, then the counter (2 bytes) and the tick (1). */
static void m8_tm( WORD mark )
{
    WORD c;
    WORD tick;

    outp( 0x43, 0x80 );
    c = inp( 0x42 );
    c |= (WORD)inp( 0x42 ) << 8;
    tick = *(WORD FAR *)MK_FP( 0x40, 0x6C );
    M8_STEP( mark );
    m8_step_word( c );
    M8_STEP( tick & 0xFF );
}
#endif

#ifdef M8_PROF
/* Read only at entry and exit and summed in RAM: a port access inside the call
 * would flush the L1, which is the cost being measured. */
static M8_PF_TAB    m8_pf;
static WORD         m8_pf_depth;
static WORD         m8_pf_style;

/* BIOS tick in the high word, channel 2 counting up in the low. */
static DWORD m8_pf_now( void )
{
    WORD    c;
    WORD    t0;
    WORD    t1;

    do {
        t0 = *(WORD FAR *)MK_FP( 0x40, 0x6C );
        outp( 0x43, 0x80 );
        c = inp( 0x42 );
        c |= (WORD)inp( 0x42 ) << 8;
        t1 = *(WORD FAR *)MK_FP( 0x40, 0x6C );
    } while( t0 != t1 );
    return( ((DWORD)t0 << 16) | (WORD)~c );
}

/* Nested entries (a DIB Engine callback into another entry) count once, in the
 * outer one. */
static DWORD m8_pf_in( void )
{
    if( m8_pf_depth++ != 0 ) {
        return( 0 );
    }
    return( m8_pf_now() );
}

static void m8_pf_out( int i, DWORD t0 )
{
    DWORD       t1;
    WORD        e;
    WORD        dt;
    M8_PF_ENT   *p;

    if( --m8_pf_depth != 0 ) {
        return;
    }
    t1 = m8_pf_now();
    e = (WORD)t1 - (WORD)t0;
    dt = (WORD)(t1 >> 16) - (WORD)(t0 >> 16);
    p = &m8_pf.e[i];
    if( dt == 0 ) {
        ++p->calls;
        p->ticks += e;
        if( i == 2 ) {
            ++m8_pf.out_calls[m8_pf_style];
            m8_pf.out_ticks[m8_pf_style] += e;
        }
    } else {
        ++p->long_calls;
        p->long_ticks += ((DWORD)(dt - 1) << 16) + e;
    }
}

LONG m8_pf_escape( LPVOID in, LPVOID out )
{
    DWORD   a;
    DWORD   b;
    WORD    best = 0xFFFF;
    int     i;

    if( m8_pf.version == 0 ) {
        m8_tm_init();
    }
    for( i = 0; i < 16; ++i ) {
        a = m8_pf_now();
        b = m8_pf_now();
        if( (WORD)(b >> 16) == (WORD)(a >> 16) && (WORD)b - (WORD)a < best ) {
            best = (WORD)b - (WORD)a;
        }
    }
    m8_pf.version = 3;
    m8_pf.n = M8_PF_N;
    m8_pf.stamp_ticks = best;
    if( out != NULL ) {
        _fmemcpy( out, &m8_pf, sizeof( m8_pf ) );
    }
    if( in != NULL && *(LPWORD)in == 1 ) {
        _fmemset( m8_pf.e, 0, sizeof( m8_pf.e ) );
        m8_pf.hits = 0;
        m8_pf.checked = 0;
        m8_pf.bad = 0;
        _fmemset( m8_pf.out_calls, 0, sizeof( m8_pf.out_calls ) );
        _fmemset( m8_pf.out_ticks, 0, sizeof( m8_pf.out_ticks ) );
    }
    return( 1 );
}
#define M8_PF_HIT()         (++m8_pf.hits)
#define M8_PF_STYLE( s )    (m8_pf_style = (s) < M8_PF_STYLES - 1 ? (s) : M8_PF_STYLES - 1)
/* Every eighth short-path read is read again the long way: the bench does not
 * check what a read returns. */
#define M8_PF_CHECK( x, y, v ) \
    if( (m8_pf.hits & 7) == 0 ) { \
        ++m8_pf.checked; \
        if( m8_hw_getpixel( x, y ) != (BYTE)(v) ) ++m8_pf.bad; \
    }
#else
#define M8_PF_HIT()
#define M8_PF_STYLE( s )
#define M8_PF_CHECK( x, y, v )
#endif

#ifdef M8_TRACE

/* A checkpoint inside an entry: M8_MARK_TIME_POINT, its id, then as m8_tm. */
static void m8_tm_point( WORD id )
{
    M8_STEP( M8_MARK_TIME_POINT );
    M8_STEP( id );
    m8_tm( M8_MARK_TIME_POINT );
}
#endif

/* Solid fills and screen-to-screen copies on the screen, clear of the cursor,
 * with the engine set up by the last call: the short path (see Pixel). 0 = the
 * full path draws it. */
static int m8_blt_short( WORD dx, WORD dy, LPPDEVICE src, WORD sx, WORD sy, WORD w, WORD h,
                         DWORD rop3, LPBRUSH brush )
{
    BYTE    rop = (BYTE)(rop3 >> 16);
    int     l = dx;
    int     t = dy;
    int     r = dx + w;
    int     b = dy + h;
    BYTE    c1 = 0;
    int     ok;

    /* Everything inline: a helper's code is another line to fetch. */
    if( (short)w <= 0 || (short)h <= 0 || (m8_screen->deFlags & PALETTE_XLAT) ) {
        return( 0 );
    }
    if( rop == 0xCC ) {
        if( (LPVOID)src != m8_hot.screen ) {
            return( 0 );
        }
        if( (int)sx < l ) l = sx;
        if( (int)sy < t ) t = sy;
        if( (int)(sx + w) > r ) r = sx + w;
        if( (int)(sy + h) > b ) b = sy + h;
    } else if( ((rop >> 2) & 0x33) != (rop & 0x33) ) {
        return( 0 );                    /* a source other than the screen */
    } else if( ((rop >> 4) & 0x0F) != (rop & 0x0F) ) {
        /* A pattern: only a solid one, as m8_brush_render reads it. */
        DIB_Brush8 FAR *br = (DIB_Brush8 FAR *)brush;

        if( br == NULL || !(br->dp8BrushFlags & COLORSOLID) ||
            (br->dp8BrushStyle != 0 && br->dp8BrushStyle != 2 && br->dp8BrushStyle != 3) ) {
            return( 0 );
        }
        c1 = br->dp8BrushBits[0];
    }
    if( m8_cur_shown && l < m8_cur_rect.r && m8_cur_rect.l < r && t < m8_cur_rect.b &&
        m8_cur_rect.t < b ) {
        return( 0 );
    }
    m8_busy = 1;
    if( rop == 0xCC ) {
        ok = m8_hw_copy_fast( sx, sy, dx, dy, w, h, M8_MIX_SRC_VRAM | M8_MIX_REPLACE );
    } else {
        ok = m8_hw_fill_fast( dx, dy, w, h, c1, M8_MIX_SRC_FRGD |
                              m8_mix_for_key[(rop & 1) | ((rop >> 1) & 1) << 1 |
                                             ((rop >> 4) & 1) << 2 | ((rop >> 5) & 1) << 3] );
    }
    if( !ok ) {
        m8_busy = 0;
        return( 0 );
    }
    M8_PF_HIT();
    M8_LEAVE();
    return( 1 );
}

static BOOL m8_bitblt( LPPDEVICE lpDestDev, WORD wDestX, WORD wDestY, LPPDEVICE lpSrcDev,
                       WORD wSrcX, WORD wSrcY, WORD wXext, WORD wYext, DWORD dwRop3,
                       LPBRUSH lpPBrush, LPDRAWMODE lpDrawMode );

BOOL WINAPI __loadds BitBlt( LPPDEVICE lpDestDev, WORD wDestX, WORD wDestY, LPPDEVICE lpSrcDev,
                             WORD wSrcX, WORD wSrcY, WORD wXext, WORD wYext, DWORD dwRop3,
                             LPBRUSH lpPBrush, LPDRAWMODE lpDrawMode )
{
    if( (LPVOID)lpDestDev == m8_hot.screen && lpDestDev != NULL &&
        m8_blt_short( wDestX, wDestY, lpSrcDev, wSrcX, wSrcY, wXext, wYext, dwRop3, lpPBrush ) ) {
        return( TRUE );
    }
    return( m8_bitblt( lpDestDev, wDestX, wDestY, lpSrcDev, wSrcX, wSrcY, wXext, wYext, dwRop3,
                       lpPBrush, lpDrawMode ) );
}

static BOOL m8_bitblt( LPPDEVICE lpDestDev, WORD wDestX, WORD wDestY, LPPDEVICE lpSrcDev,
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
        m8_trace_mem( M8_MARK_MEM2MEM, lpDestDev, m8_rop_uses_src( dwRop3 ) ? lpSrcDev : NULL,
                      rop, (short)wXext, (short)wYext, (short)wDestX, (short)wDestY,
                      (short)wSrcX, (short)wSrcY );
        if( M8_PROXY && m8_rop_uses_src( dwRop3 ) && !m8_rop_uses_pat( dwRop3 ) &&
            m8_px_m2m( lpDestDev, lpSrcDev, (short)wDestX, (short)wDestY, (short)wSrcX,
                       (short)wSrcY, (short)wXext, (short)wYext, m8_mix_for_key[rop & 0x0F],
                       !m8_rop_uses_dest( dwRop3 ) ) ) {
            return( TRUE );
        }
        if( M8_PROXY && !m8_rop_uses_src( dwRop3 ) && m8_px_n > 0 ) {
            BYTE    c1 = 0;
            BYTE    c2 = 0;
            BYTE    pat[8];

            if( (!m8_rop_uses_pat( dwRop3 ) ||
                 m8_brush_colours( lpPBrush, lpDrawMode, &c1, &c2, pat ) == 1) &&
                m8_px_fill( lpDestDev, (short)wDestX, (short)wDestY, (short)wXext, (short)wYext,
                            c1, m8_rop_mix( rop ), !m8_rop_uses_dest( dwRop3 ) ) ) {
                return( TRUE );
            }
        }
        m8_px_touch_rect( lpDestDev, (short)wDestX, (short)wDestY, (short)wXext, (short)wYext,
                          M8_PX_WHY_MEM );
        if( m8_rop_uses_src( dwRop3 ) ) {
            m8_px_touch_rect( lpSrcDev, (short)wSrcX, (short)wSrcY, (short)wXext, (short)wYext,
                              M8_PX_WHY_MEM );
        }
        return( DIB_BitBlt( lpDestDev, wDestX, wDestY, lpSrcDev, wSrcX, wSrcY, wXext, wYext,
                            dwRop3, lpPBrush, lpDrawMode ) );
    }
    if( m8_screen == NULL ) {
        return( FALSE );
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
            if( (kind == 2 || kind == 3) && M8_PATTERN_EXPANSION ) {
                m8_enter( &r );
                m8_pattern_fill( wDestX, wDestY, wXext, wYext, pat, c1, c2, mix, kind == 3 );
                m8_leave();
                return( TRUE );
            }
        }
        if( !src_screen && m8_rop_uses_src( dwRop3 ) && !m8_rop_uses_pat( dwRop3 ) &&
            ((LPDIBENGINE)lpSrcDev)->deType != TYPE_DIBENG &&
            m8_mono_blt( (BITMAP FAR *)lpSrcDev, wSrcX, wSrcY, wDestX, wDestY, wXext, wYext,
                         m8_mix_for_key[rop & 0x0F], lpDrawMode ) ) {
            return( TRUE );
        }
        if( M8_PROXY && !src_screen && m8_rop_uses_src( dwRop3 ) && !m8_rop_uses_pat( dwRop3 ) &&
            (m8_px_restore( (LPDIBENGINE)lpSrcDev, wSrcX, wSrcY, wDestX, wDestY, wXext, wYext,
                            m8_mix_for_key[rop & 0x0F] ) ||
             (m8_px_upload( (LPDIBENGINE)lpSrcDev ) &&
              m8_px_restore( (LPDIBENGINE)lpSrcDev, wSrcX, wSrcY, wDestX, wDestY, wXext, wYext,
                             m8_mix_for_key[rop & 0x0F] ))) ) {
            return( TRUE );
        }
        if( !src_screen && m8_rop_uses_src( dwRop3 ) ) {
            m8_px_touch( lpSrcDev, M8_PX_WHY_ENTRY );
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

    if( !src_screen && m8_rop_uses_src( dwRop3 ) ) {
        m8_px_touch( lpSrcDev, M8_PX_WHY_ENTRY );
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
        m8_trace_mem( M8_MARK_SCR2MEM_ARGS, lpDestDev, NULL, rop, (short)wXext, (short)wYext,
                      (short)wDestX, (short)wDestY, (short)wSrcX, (short)wSrcY );
        if( M8_PROXY && rop == 0xCC && (short)wXext > 0 && (short)wYext > 0 &&
            m8_px_save( (LPDIBENGINE)lpDestDev, wDestX, wDestY, wSrcX, wSrcY, wXext, wYext ) ) {
            return( TRUE );
        }
        m8_px_touch( lpDestDev, M8_PX_WHY_ENTRY );
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

/* A solid or styled pen (dash, dot, dash-dot, dash-dot-dot) through a ROP2,
 * every point on the engine's 0-1023 grid: the engine draws it. Pens reaching
 * Output are one pixel wide - Enable withdraws LC_WIDE, so GDI turns wide pens
 * into polygons. */
static int m8_line_on_engine( WORD count, LPPOINT pts, LPPPEN pen, LPDRAWMODE dm )
{
    DIB_Pen FAR *p = pen;
    WORD        i;

    if( count < 2 || p == NULL || dm == NULL || p->dpPenStyle > LS_DASHDOTDOT ||
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

/* The DIB Engine clips a polygon's or an ellipse's fill to the bitmap, not to
 * the clip rectangle, so a strip that claims the whole screen gets rows
 * outside it: below, past the selector limit; above, in memory before the
 * buffer. The shape is drawn instead into a device exactly the strip's size,
 * its points shifted by the strip's origin - a multiple of 8, so patterns stay
 * in phase (Output's strips are aligned to 8). The engine does not clip a
 * scan style's rows to the device either, so those are cut to the strip here.
 * OS_SCANLINES carries (count, y) and then x pairs. OS_POLYSCANLINE carries
 * count records, each in words: n, top, bottom (exclusive), n x values in
 * pairs, n again. */
#define M8_OUT_MAX_PTS      1024

static POINT    m8_out_pts[M8_OUT_MAX_PTS];

/* The rows and spans a scan style names; 0 if it names none. */
static int m8_scan_bound( WORD style, WORD count, LPPOINT p, M8_RECT *r )
{
    const short FAR *w = (const short FAR *)p;
    WORD            rec;
    WORD            cnt;
    WORD            i;
    int             any = 0;

    if( style == OS_SCANLINES ) {
        if( count < 2 ) {
            return( 0 );
        }
        m8_rect_set( r, p[1].xcoord, p[0].ycoord, p[1].ycoord + 1, p[0].ycoord + 1 );
        for( i = 2; i < count; ++i ) {
            if( p[i].xcoord < r->l ) r->l = p[i].xcoord;
            if( p[i].ycoord + 1 > r->r ) r->r = p[i].ycoord + 1;
        }
        return( 1 );
    }
    for( rec = 0; rec < count; ++rec ) {
        cnt = (WORD)w[0];
        for( i = 0; i < cnt; ++i ) {
            if( !any ) {
                m8_rect_set( r, w[3], w[1], w[3] + 1, w[2] );
                any = 1;
            }
            if( w[3 + i] < r->l ) r->l = w[3 + i];
            if( w[3 + i] + 1 > r->r ) r->r = w[3 + i] + 1;
            if( w[1] < r->t ) r->t = w[1];
            if( w[2] > r->b ) r->b = w[2];
        }
        w += cnt + 4;
    }
    return( any );
}

/* OS_POLYSCANLINE records moved to the strip's origin and cut to its h rows;
 * the records written, 0 if no row is left (or they would not fit). */
static WORD m8_scan_shift( const M8_OUT *o, int x0, int y0, int h )
{
    const short FAR *s = (const short FAR *)o->pts;
    short           *d = (short *)m8_out_pts;
    WORD            rec;
    WORD            out = 0;
    WORD            j = 0;
    WORD            cnt;
    WORD            i;
    int             top;
    int             bot;

    for( rec = 0; rec < o->count; ++rec ) {
        cnt = (WORD)s[0];
        top = s[1] - y0;
        bot = s[2] - y0;
        if( top < 0 ) {
            top = 0;
        }
        if( bot > h ) {
            bot = h;
        }
        if( top < bot ) {
            if( j + cnt + 4 > M8_OUT_MAX_PTS * 2 ) {
                return( 0 );
            }
            d[j++] = (short)cnt;
            d[j++] = (short)top;
            d[j++] = (short)bot;
            for( i = 0; i < cnt; ++i ) {
                d[j++] = (short)(s[3 + i] - x0);
            }
            d[j++] = (short)cnt;
            ++out;
        }
        s += cnt + 4;
    }
    return( out );
}

/* Scan styles with one colour: each span is one engine fill, nothing read
 * back. Spans are [x1, x2) on their rows, cut to clip. */
static void m8_scan_fill( WORD style, WORD count, LPPOINT p, const M8_RECT *clip, int kind,
                          BYTE c1, BYTE c2, const BYTE *pat, WORD fn )
{
    m8_enter( clip );
    /* fn is the ALU function alone. */
    if( kind == 1 ) {
        m8_hw_fill_begin( c1, M8_MIX_SRC_FRGD | fn );
    } else {
        m8_hw_pattern_begin( c1, c2, fn, kind == 3 ? M8_MIX_LEAVE : fn );
    }
    m8_hw_scan_spans( style == OS_POLYSCANLINE, count, (const short FAR *)p,
                      clip->l, clip->t, clip->r, clip->b, kind == 1 ? NULL : pat );
    m8_leave();
}

static DWORD m8_draw_output( void *ctx, LPDIBENGINE dev, LPRECT c )
{
    M8_OUT      *o = ctx;
    DIBENGINE   t;
    RECT        tc;
    int         x0 = c->left & ~7;
    int         y0 = c->top & ~7;
    int         r = (c->right + 1) & ~1;
    WORD        n = o->count;
    WORD        i;

    if( o->count > M8_OUT_MAX_PTS ) {
        return( 0 );
    }
    if( r > M8_SCREEN_X ) {
        r = M8_SCREEN_X;
    }
    _fmemcpy( &t, dev, sizeof( DIBENGINE ) );
    t.deBitsOffset += (DWORD)y0 * dev->deDeltaScan + x0;
    t.deWidth = (WORD)(r - x0);
    t.deHeight = (WORD)(c->bottom - y0);
    tc.left = c->left - x0;
    tc.top = c->top - y0;
    tc.right = c->right - x0;
    tc.bottom = c->bottom - y0;
    if( o->style == OS_POLYSCANLINE ) {
        n = m8_scan_shift( o, x0, y0, (int)t.deHeight );
        if( n == 0 ) {
            return( 1 );
        }
    } else if( o->style == OS_SCANLINES &&
               (o->pts[0].ycoord < y0 || o->pts[0].ycoord >= c->bottom) ) {
        return( 1 );
    } else {
        for( i = 0; i < n; ++i ) {
            if( o->style == OS_SCANLINES && i == 0 ) {
                m8_out_pts[0].xcoord = o->pts[0].xcoord;
                m8_out_pts[0].ycoord = o->pts[0].ycoord - y0;
            } else if( o->style == OS_SCANLINES ) {
                m8_out_pts[i].xcoord = o->pts[i].xcoord - x0;
                m8_out_pts[i].ycoord = o->pts[i].ycoord - x0;
            } else {
                m8_out_pts[i].xcoord = o->pts[i].xcoord - x0;
                m8_out_pts[i].ycoord = o->pts[i].ycoord - y0;
            }
        }
    }
    return( DIB_Output( &t, o->style, n, m8_out_pts, o->pen, o->brush, o->dm, &tc ) );
}

/* A solid polyline on the screen with every point inside the clip and clear of
 * the cursor: the short path (see Pixel). It needs no scissors, as the line
 * stays inside its points' box. 0 = the full path draws it. */
static int m8_out_short( WORD count, LPPOINT p, LPPPEN pen, LPDRAWMODE dm, LPRECT clip )
{
    DIB_Pen FAR *dp = pen;
    M8_RECT     c;
    M8_RECT     b;
    WORD        i;
    BYTE        color;
    WORD        mix;

    if( count < 2 || dp == NULL || dm == NULL || dp->dpPenStyle != LS_SOLID ||
        dm->Rop2 < 1 || dm->Rop2 > 16 ) {
        return( 0 );
    }
    c.l = 0;
    c.t = 0;
    c.r = M8_SCREEN_X;
    c.b = M8_SCREEN_Y;
    if( clip != NULL ) {
        if( clip->left > c.l ) c.l = clip->left;
        if( clip->top > c.t ) c.t = clip->top;
        if( clip->right < c.r ) c.r = clip->right;
        if( clip->bottom < c.b ) c.b = clip->bottom;
    }
    b.l = b.r = p[0].xcoord;
    b.t = b.b = p[0].ycoord;
    for( i = 0; i < count; ++i ) {
        if( p[i].xcoord < c.l || p[i].xcoord >= c.r || p[i].ycoord < c.t || p[i].ycoord >= c.b ) {
            return( 0 );
        }
        if( p[i].xcoord < b.l ) b.l = p[i].xcoord;
        if( p[i].ycoord < b.t ) b.t = p[i].ycoord;
        if( p[i].xcoord >= b.r ) b.r = p[i].xcoord + 1;
        if( p[i].ycoord >= b.b ) b.b = p[i].ycoord + 1;
    }
    if( m8_cur_shown && b.l < m8_cur_rect.r && m8_cur_rect.l < b.r && b.t < m8_cur_rect.b &&
        m8_cur_rect.t < b.b ) {
        return( 0 );
    }
    color = (BYTE)dp->dpPenColor;
    mix = m8_mix_for_key[dm->Rop2 - 1];
    m8_busy = 1;
    if( !m8_hw_line_fast( p, count, color, mix ) ) {
        m8_hw_polyline_plain( p, count, color, mix );
    }
    M8_PF_HIT();
    M8_LEAVE();
    return( 1 );
}

WORD WINAPI __loadds Output( LPPDEVICE lpDestDev, WORD wStyle, WORD wCount, LPPOINT lpPoints,
                             LPPPEN lpPPen, LPPBRUSH lpPBrush, LPDRAWMODE lpDrawMode,
                             LPRECT lpClipRect )
{
    M8_OUT  o;
    M8_RECT bound;
    M8_RECT pts;
    WORD    i;

    M8_PF_STYLE( wStyle );
    if( wStyle == OS_POLYLINE && (LPVOID)lpDestDev == m8_hot.screen && lpDestDev != NULL &&
        m8_out_short( wCount, lpPoints, lpPPen, lpDrawMode, lpClipRect ) ) {
        return( 1 );
    }
    m8_px_touch( lpDestDev, M8_PX_WHY_ENTRY );
    if( !m8_is_screen( lpDestDev ) ) {
        return( DIB_Output( lpDestDev, wStyle, wCount, lpPoints, lpPPen, lpPBrush,
                            lpDrawMode, lpClipRect ) );
    }
    if( m8_screen == NULL ) {
        return( 0 );
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
                            ((DIB_Pen FAR *)lpPPen)->dpPenStyle,
                            lpDrawMode->bkMode == OPAQUE, (BYTE)lpDrawMode->bkColor,
                            bound.l, bound.t, bound.r - 1, bound.b - 1 );
            m8_leave();
        }
        return( 1 );
    }
    if( (wStyle == OS_POLYSCANLINE || wStyle == OS_SCANLINES) && lpPBrush != NULL &&
        lpDrawMode != NULL && lpDrawMode->Rop2 >= 1 && lpDrawMode->Rop2 <= 16 ) {
        BYTE    c1;
        BYTE    c2;
        BYTE    pat[8];
        M8_RECT scr;
        int     kind = m8_brush_colours( (LPBRUSH)lpPBrush, lpDrawMode, &c1, &c2, pat );

        if( kind == 1 || ((kind == 2 || kind == 3) && M8_PATTERN_EXPANSION) ) {
            m8_rect_screen( &scr );
            m8_rect_and( &bound, &scr );
            /* The spans are clipped as they are drawn; their own bound is
             * worth a pass over them only to keep the cursor up. */
            if( m8_cur_shown && m8_rect_overlap( &bound, &m8_cur_rect ) &&
                m8_scan_bound( wStyle, wCount, lpPoints, &pts ) ) {
                m8_rect_and( &bound, &pts );
            }
            if( !m8_rect_empty( &bound ) ) {
                m8_scan_fill( wStyle, wCount, lpPoints, &bound, kind, c1, c2, pat,
                              m8_mix_for_key[lpDrawMode->Rop2 - 1] );
            }
            return( 1 );
        }
    }
    /* A scan style can arrive with a NULL clip: shrink to the rows and spans
     * it names, so the strips are no wider or taller than the drawing. */
    if( (wStyle == OS_SCANLINES || wStyle == OS_POLYSCANLINE) &&
        m8_scan_bound( wStyle, wCount, lpPoints, &pts ) ) {
        m8_rect_and( &bound, &pts );
    }
    o.style = wStyle;
    o.count = wCount;
    o.pts = lpPoints;
    o.pen = lpPPen;
    o.brush = lpPBrush;
    o.dm = lpDrawMode;
    m8_fallback_aligned( &bound, 1, 1, 0, m8_draw_output, &o, 8 );
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

    /* A set on the screen, clear of the cursor, with the engine set up by the
     * last call: the short path. The screen is known by its pointer, without
     * reading the device. */
    if( (LPVOID)lpDestDev == m8_hot.screen && lpDestDev != NULL && lpDrawMode != NULL &&
        lpDrawMode->Rop2 >= 1 && lpDrawMode->Rop2 <= 16 &&
        X < (WORD)M8_SCREEN_X && Y < (WORD)M8_SCREEN_Y &&
        !(m8_cur_shown && (int)X >= m8_cur_rect.l && (int)X < m8_cur_rect.r &&
          (int)Y >= m8_cur_rect.t && (int)Y < m8_cur_rect.b) ) {
        m8_busy = 1;
        if( m8_hw_fill_fast( X, Y, 1, 1, (BYTE)dwPhysColor,
                         M8_MIX_SRC_FRGD | m8_mix_for_key[lpDrawMode->Rop2 - 1] ) ) {
            M8_PF_HIT();
            M8_LEAVE();
            return( dwPhysColor );
        }
        m8_busy = 0;
    }
    /* A get, likewise, while the engine holds the read set-up. */
    if( lpDrawMode == NULL && (LPVOID)lpDestDev == m8_hot.screen && lpDestDev != NULL &&
        X < (WORD)M8_SCREEN_X && Y < (WORD)M8_SCREEN_Y &&
        !(m8_screen->deFlags & PALETTE_XLAT) &&
        !(m8_cur_shown && (int)X >= m8_cur_rect.l && (int)X < m8_cur_rect.r &&
          (int)Y >= m8_cur_rect.t && (int)Y < m8_cur_rect.b) ) {
        int v;

        m8_busy = 1;
        v = m8_hw_getpixel_fast( X, Y );
        if( v >= 0 ) {
            M8_PF_HIT();
            M8_PF_CHECK( X, Y, v );
            M8_LEAVE();
            return( 0xFF000000UL | (BYTE)v );
        }
        m8_busy = 0;
    }
    m8_px_touch( lpDestDev, M8_PX_WHY_ENTRY );
    if( !m8_is_screen( lpDestDev ) ) {
        return( DIB_Pixel( lpDestDev, X, Y, dwPhysColor, lpDrawMode ) );
    }
    if( m8_screen == NULL ) {
        return( 0 );
    }
    M8_ENTRY( M8_STAT_PIXEL );
    /* A set on the engine: a 1x1 fill through the ROP2's mix, no read-back.
     * GDI does not use a set's return value: ATIM8 returns what its registers
     * hold (seg07:0048). */
    if( lpDrawMode != NULL && lpDrawMode->Rop2 >= 1 && lpDrawMode->Rop2 <= 16 &&
        X < (WORD)M8_SCREEN_X && Y < (WORD)M8_SCREEN_Y ) {
        m8_rect_set( &r, X, Y, X + 1, Y + 1 );
        m8_enter( &r );
        m8_hw_fill_mix( X, Y, 1, 1, (BYTE)dwPhysColor,
                        M8_MIX_SRC_FRGD | m8_mix_for_key[lpDrawMode->Rop2 - 1] );
        m8_leave();
        return( dwPhysColor );
    }
    m8_rect_set( &r, X, Y, X + 1, Y + 1 );
    /* A get on the engine, answered as the DIB Engine answers it for an 8-bit
     * device (ATIM8 seg07:00fa): FF00h above the pixel. A translated palette
     * needs the reverse table, which the fallback has. */
    if( lpDrawMode == NULL && !m8_hw_off && !(m8_screen->deFlags & PALETTE_XLAT) &&
        X < (WORD)M8_SCREEN_X && Y < (WORD)M8_SCREEN_Y ) {
        BYTE v;

        m8_enter( &r );
        v = m8_hw_getpixel( X, Y );
        m8_leave();
        return( 0xFF000000UL | v );
    }
    p.x = X;
    p.y = Y;
    p.color = dwPhysColor;
    p.dm = lpDrawMode;
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

    /* On the screen, the cursor clear of the row: the short path, as Pixel's. */
    if( (LPVOID)lpDestDev == m8_hot.screen && lpDestDev != NULL && !m8_hw_off &&
        X < (WORD)M8_SCREEN_X && Y < (WORD)M8_SCREEN_Y &&
        !(m8_cur_shown && (int)Y >= m8_cur_rect.t && (int)Y < m8_cur_rect.b) ) {
        WORD ret;

        m8_busy = 1;
        ret = m8_hw_scanlr( X, Y, (BYTE)dwPhysColor, wStyle, M8_SCREEN_X );
        M8_PF_HIT();
        M8_LEAVE();
        return( ret );
    }
    m8_px_touch( lpDestDev, M8_PX_WHY_ENTRY );
    if( !m8_is_screen( lpDestDev ) ) {
        return( DIB_ScanLR( lpDestDev, X, Y, dwPhysColor, wStyle ) );
    }
    if( m8_screen == NULL ) {
        return( 0 );
    }
    M8_ENTRY( M8_STAT_SCANLR );
    if( X >= M8_SCREEN_X || Y >= M8_SCREEN_Y ) {
        return( 0x8000 );
    }
    /* The scan may run to either edge; the cursor must not be in the row. */
    m8_rect_set( &r, 0, Y, M8_SCREEN_X, Y + 1 );
    if( !m8_hw_off ) {
        WORD ret;

        m8_enter( &r );
        ret = m8_hw_scanlr( X, Y, (BYTE)dwPhysColor, wStyle, M8_SCREEN_X );
        m8_leave();
        return( ret );
    }
    s.x = X;
    s.y = Y;
    s.color = dwPhysColor;
    s.style = wStyle;
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
 * hashes of the bits and the size. A string is stored the second time it is
 * seen: storing costs an upload into the plane and then the blit, where a
 * string drawn once (a counter, a status line) is one mono expansion. */
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

#define M8_SC_SEEN          64

static M8_SCENTRY   m8_sc[M8_SC_ENTRIES];
static DWORD        m8_sc_seen[M8_SC_SEEN];
static int          m8_sc_seen_next;
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

static WORD          m8_sc_gen = 1;     /* changes on each flush: m8_tk entries die with it */

static void m8_sc_flush( void )
{
    if( ++m8_sc_gen == 0 ) {
        m8_sc_gen = 1;
    }
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
    DWORD       h1;
    DWORD       h2;
    M8_HASH     hs;
    int         k;
    M8_SCENTRY  *e;

    if( m8_hw_off || (int)wb * 8 > M8_SC_W || h > M8_SC_MAX_H || (int)h > m8_sc_rows() ) {
        return( NULL );
    }
    m8_hash_init( &hs, wb );
    m8_hash_bytes( &hs, bits, (WORD)(wb * h) );
    h1 = m8_hash_h1( &hs );
    h2 = m8_hash_h2( &hs, h );
    for( k = 0; k < m8_sc_n; ++k ) {
        e = &m8_sc[k];
        if( e->h1 == h1 && e->h2 == h2 && e->wb == wb && e->h == h ) {
            M8_STEP( M8_MARK_SC_HIT );
            return( e );
        }
    }
    for( k = 0; k < M8_SC_SEEN && m8_sc_seen[k] != h1; ++k ) {
    }
    if( k == M8_SC_SEEN ) {
        m8_sc_seen[m8_sc_seen_next] = h1;
        m8_sc_seen_next = (m8_sc_seen_next + 1) % M8_SC_SEEN;
        return( NULL );
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

/* Strings already drawn, keyed by what was asked rather than by the pixels:
 * font, characters, spacing, transform, options, colours' mode, and the clip
 * and opaque rectangles relative to the origin. A hit replays what the DIB
 * Engine did - its opaque fills and the string bitmap, which the string cache
 * still holds - with no layout and no hash over rendered bits. */
#define M8_TK_N             256
#define M8_TK_RECTS         2

typedef struct {
    DWORD   h1;
    DWORD   h2;
    WORD    gen;                        /* m8_sc_gen when stored; 0: empty */
    int     sc;                         /* index in m8_sc */
    int     bx;                         /* bitmap origin, relative to the string's */
    int     by;
    WORD    wb;
    WORD    h;
    WORD    flags;                      /* the bitmap callback's: bit 0 transparent */
    int     clip;                       /* 1: crect holds the callback's clip */
    RECT    crect;
    int     nrect;
    RECT    rect[M8_TK_RECTS];          /* opaque fills */
    DWORD   rc;
} M8_TKENTRY;

static M8_TKENTRY   m8_tk[M8_TK_N];
static M8_TKENTRY   m8_tk_new;
static int          m8_tk_cap;          /* recording a miss */
static int          m8_tk_bad;          /* the miss cannot be replayed */
static int          m8_tk_bitmaps;
static int          m8_tk_x0;
static int          m8_tk_y0;
static BYTE         m8_tk_fg;
static BYTE         m8_tk_bg;

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
        x + (long)wWidthBytes * 8 > M8_COORD_MAX || y + (long)wHeight > M8_COORD_MAX ) {
        /* Trace: 2 = the bitmap crosses its selector, 4 = beyond the engine's
         * coordinates. */
        M8_STEP( M8_MARK_TEXT_WHY | ((DWORD)FP_OFF( lpBits ) + (DWORD)wWidthBytes * wHeight > 0x10000UL ? 2 : 4) );
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
        m8_tk_bad = 1;
        return( 1 );
    }
    m8_exclude( &r );
    {
        M8_SCENTRY *e = m8_sc_get( lpBits, wWidthBytes, wHeight );

        if( m8_tk_cap ) {
            ++m8_tk_bitmaps;
            if( e == NULL || (BYTE)dwFg != m8_tk_fg || (BYTE)dwBg != m8_tk_bg ) {
                m8_tk_bad = 1;
            } else {
                m8_tk_new.sc = (int)(e - m8_sc);
                m8_tk_new.bx = x - m8_tk_x0;
                m8_tk_new.by = y - m8_tk_y0;
                m8_tk_new.wb = wWidthBytes;
                m8_tk_new.h = wHeight;
                m8_tk_new.flags = wFlags;
                m8_tk_new.clip = lpClip != NULL;
                if( lpClip != NULL ) {
                    m8_tk_new.crect.left = lpClip->left - m8_tk_x0;
                    m8_tk_new.crect.top = lpClip->top - m8_tk_y0;
                    m8_tk_new.crect.right = lpClip->right - m8_tk_x0;
                    m8_tk_new.crect.bottom = lpClip->bottom - m8_tk_y0;
                }
            }
        }
        if( e != NULL ) {
            /* The visible part only: the blit has no scissors of its own. */
            m8_hw_plane_blit( e->x + (r.l - x), e->y + (r.t - y), r.l, r.t,
                              r.r - r.l, r.b - r.t, e->plane, (BYTE)dwFg, (BYTE)dwBg,
                              wFlags & 1 );
            return( 1 );
        }
    }
    /* A bitmap starting off the top or left is drawn from the cache only:
     * the cached blit takes just the visible part (trace 3). */
    if( x < 0 || y < 0 ) {
        M8_STEP( M8_MARK_TEXT_WHY | 3 );
        m8_text_failed = 1;
        return( 1 );
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
    if( m8_tk_cap ) {
        if( m8_tk_new.nrect >= M8_TK_RECTS || (BYTE)dwBg != m8_tk_bg ) {
            m8_tk_bad = 1;
        } else {
            RECT *q = &m8_tk_new.rect[m8_tk_new.nrect++];

            q->left = (short)wX - m8_tk_x0;
            q->top = (short)wY - m8_tk_y0;
            q->right = q->left + (int)wXExt;
            q->bottom = q->top + (int)wYExt;
        }
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

/* What the DIB Engine's layout depends on, translated to the string's origin.
 * The font's header goes in too: a freed font's address can be reused. */
static void m8_tk_rect( M8_HASH *hs, LPRECT r, int x0, int y0 )
{
    if( r == NULL ) {
        m8_hash_word( hs, 0xFFFF );
    } else {
        m8_hash_word( hs, (WORD)(r->left - x0) );
        m8_hash_word( hs, (WORD)(r->top - y0) );
        m8_hash_word( hs, (WORD)(r->right - x0) );
        m8_hash_word( hs, (WORD)(r->bottom - y0) );
    }
}

static void m8_tk_key( DWORD *p1, DWORD *p2, int x0, int y0, LPRECT clip, LPSTR str, int n,
                       LPFONTINFO font, LPDRAWMODE dm, LPTEXTXFORM xf, LPSHORT widths,
                       LPRECT opaque, WORD options )
{
    M8_HASH hs;

    m8_hash_init( &hs, (WORD)n );
    m8_hash_word( &hs, FP_SEG( font ) );
    m8_hash_word( &hs, FP_OFF( font ) );
    m8_hash_word( &hs, options );
    m8_hash_word( &hs, (WORD)dm->bkMode );
    m8_hash_bytes( &hs, (const BYTE FAR *)font, 34 );
    m8_hash_bytes( &hs, (const BYTE FAR *)&dm->TBreakExtra, 12 );
    m8_hash_bytes( &hs, (const BYTE FAR *)str, (WORD)n );
    if( widths != NULL ) {
        m8_hash_bytes( &hs, (const BYTE FAR *)widths, (WORD)(n * 2) );
    }
    if( xf != NULL ) {
        m8_hash_bytes( &hs, (const BYTE FAR *)xf, sizeof( TEXTXFORM ) );
    }
    m8_tk_rect( &hs, clip, x0, y0 );
    m8_tk_rect( &hs, opaque, x0, y0 );
    *p1 = m8_hash_h1( &hs );
    *p2 = m8_hash_h2( &hs, FP_SEG( font ) ^ (WORD)n );
}

static void m8_tk_replay( const M8_TKENTRY *k, int x0, int y0, BYTE fg, BYTE bg )
{
    M8_SCENTRY  *e = &m8_sc[k->sc];
    M8_RECT     r;
    M8_RECT     c;
    int         x = x0 + k->bx;
    int         y = y0 + k->by;
    int         i;

    m8_busy = 1;
    for( i = 0; i < k->nrect; ++i ) {
        m8_text_rect( m8_screen, bg, (WORD)(x0 + k->rect[i].left), (WORD)(y0 + k->rect[i].top),
                      (WORD)(k->rect[i].right - k->rect[i].left),
                      (WORD)(k->rect[i].bottom - k->rect[i].top) );
    }
    m8_rect_set( &r, x, y, x + k->wb * 8, y + k->h );
    if( k->clip ) {
        m8_rect_set( &c, x0 + k->crect.left, y0 + k->crect.top, x0 + k->crect.right,
                     y0 + k->crect.bottom );
        m8_rect_and( &r, &c );
    }
    m8_rect_screen( &c );
    m8_rect_and( &r, &c );
    if( !m8_rect_empty( &r ) ) {
        m8_exclude( &r );
        m8_hw_plane_blit( e->x + (r.l - x), e->y + (r.t - y), r.l, r.t, r.r - r.l, r.b - r.t,
                          e->plane, fg, bg, k->flags & 1 );
    }
    m8_leave();
}

DWORD WINAPI __loadds ExtTextOut( LPPDEVICE lpDestDev, WORD wDestXOrg, WORD wDestYOrg,
                                  LPRECT lpClipRect, LPSTR lpString, int wCount,
                                  LPFONTINFO lpFontInfo, LPDRAWMODE lpDrawMode,
                                  LPTEXTXFORM lpTextXForm, LPSHORT lpCharWidths,
                                  LPRECT lpOpaqueRect, WORD wOptions )
{
    M8_TEXT t;
    M8_RECT bound;

    m8_px_touch( lpDestDev, M8_PX_WHY_ENTRY );
    if( !m8_is_screen( lpDestDev ) ) {
        return( DIB_ExtTextOut( lpDestDev, wDestXOrg, wDestYOrg, lpClipRect, lpString, wCount,
                                lpFontInfo, lpDrawMode, lpTextXForm, lpCharWidths,
                                lpOpaqueRect, wOptions ) );
    }
    if( m8_screen == NULL ) {
        return( 0 );
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
        DWORD       rc;
        DWORD       h1 = 0;
        DWORD       h2 = 0;
        M8_TKENTRY  *k = NULL;

        if( wCount > 0 && lpFontInfo != NULL && lpDrawMode != NULL && !m8_hw_off ) {
            m8_tk_key( &h1, &h2, (short)wDestXOrg, (short)wDestYOrg, lpClipRect, lpString, wCount,
                       lpFontInfo, lpDrawMode, lpTextXForm, lpCharWidths, lpOpaqueRect,
                       wOptions );
            k = &m8_tk[(WORD)h1 & (M8_TK_N - 1)];
            if( k->gen == m8_sc_gen && k->h1 == h1 && k->h2 == h2 ) {
                M8_STEP( M8_MARK_TK_HIT );
                m8_tk_replay( k, (short)wDestXOrg, (short)wDestYOrg, (BYTE)lpDrawMode->TextColor,
                              (BYTE)lpDrawMode->bkColor );
                return( k->rc );
            }
            m8_tk_new.nrect = 0;
            m8_tk_x0 = (short)wDestXOrg;
            m8_tk_y0 = (short)wDestYOrg;
            m8_tk_fg = (BYTE)lpDrawMode->TextColor;
            m8_tk_bg = (BYTE)lpDrawMode->bkColor;
            m8_tk_bad = 0;
            m8_tk_bitmaps = 0;
            m8_tk_cap = 1;
        }
        m8_busy = 1;
        m8_text_failed = 0;
        rc = DIB_ExtTextOutExt( lpDestDev, wDestXOrg, wDestYOrg, lpClipRect, lpString, wCount,
                                lpFontInfo, lpDrawMode, lpTextXForm, lpCharWidths,
                                lpOpaqueRect, wOptions, (LPVOID *)m8_text_bitmap,
                                (LPVOID *)m8_text_rect );
        m8_leave();
        m8_tk_cap = 0;
        if( k != NULL && !m8_text_failed && !m8_tk_bad && m8_tk_bitmaps == 1 ) {
            m8_tk_new.h1 = h1;
            m8_tk_new.h2 = h2;
            m8_tk_new.rc = rc;
            m8_tk_new.gen = m8_sc_gen;
            *k = m8_tk_new;
        }
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

    m8_px_touch( lpDestDev, M8_PX_WHY_ENTRY );
    if( !m8_is_screen( lpDestDev ) ) {
        return( DIB_StrBlt( lpDestDev, wDestXOrg, wDestYOrg, lpClipRect, lpString, wCount,
                            lpFontInfo, lpDrawMode, lpTextXForm ) );
    }
    if( m8_screen == NULL ) {
        return( 0 );
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

    m8_px_touch( lpDestDev, M8_PX_WHY_ENTRY );
    if( !m8_is_screen( lpDestDev ) ) {
        return( DIB_DibToDevice( lpDestDev, X, Y, iScan, cScans, lpClipRect, lpDrawMode,
                                 lpDIBits, lpBitmapInfo, lpTranslate ) );
    }
    if( m8_screen == NULL ) {
        return( 0 );
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

    m8_px_touch( lpDestDev, M8_PX_WHY_ENTRY );
    if( !m8_is_screen( lpDestDev ) ) {
        return( DIB_StretchDIBits( lpDestDev, fGet, wDestX, wDestY, wDestWidth, wDestHeight,
                                   wSrcX, wSrcY, wSrcWidth, wSrcHeight, lpBits, lpInfo,
                                   lpTranslate, dwRop3, lpPBrush, lpDrawMode, lpClipRect ) );
    }
    if( m8_screen == NULL ) {
        return( 0 );
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
    /* The screen is read only when the result depends on it, or when a
     * clipped or mirrored stretch leaves part of its box undrawn. */
    m8_fallback( &bound, fGet || m8_rop_uses_dest( dwRop3 ) ? 1 : M8_READ_IF_UNCOVERED, !fGet,
                 0, m8_draw_stretchdib, &s );
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
    if( m8_rop_uses_src( dwRop3 ) ) {
        m8_px_touch( lpSrcDev, M8_PX_WHY_ENTRY );
    }
    m8_px_touch( lpDestDev, M8_PX_WHY_ENTRY );
    if( !m8_is_screen( lpDestDev ) ) {
        return( DIB_StretchBlt( lpDestDev, wDestX, wDestY, wDestWidth, wDestHeight, lpSrcDev,
                                wSrcX, wSrcY, wSrcWidth, wSrcHeight, dwRop3, lpPBrush,
                                lpDrawMode, lpClipRect ) );
    }
    if( m8_screen == NULL ) {
        return( 0 );
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
    m8_fallback( &bound, m8_rop_uses_dest( dwRop3 ) ? 1 : M8_READ_IF_UNCOVERED, 1, 0,
                 m8_draw_stretchblt, &s );
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
    m8_bc_claim( M8_SSB_Y + h );
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

    if( m8_screen == NULL ) {
        return;
    }
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
        m8_cur_planes_ok = 0;
        m8_cur_keyed = 1;
        for( x = 0; x < sizeof( m8_cur_and ); ++x ) {
            if( m8_cur_and[x] & m8_cur_xor[x] ) {
                m8_cur_keyed = 0;
            }
        }
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

#if defined( M8_TRACE ) || defined( M8_PROF )
    m8_tm_init();
#endif

    m8_hot.screen = screen;
    m8_ssb_used = 0;
    m8_cur_planes_ok = 0;
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
    m8_px_flush( NULL, M8_PX_WHY_SUSPEND );
    m8_busy = 1;
    m8_sc_flush();
    m8_bc_flush();
    m8_cur_planes_ok = 0;
    m8_cur_shown = 0;
    m8_cur_pending = 0;
    m8_ssb_used = 0;
}

/* Windows has the screen again; USER repaints everything after this. */
void m8_screen_resume( void )
{
#if defined( M8_TRACE ) || defined( M8_PROF )
    m8_tm_init();
#endif
    m8_sc_flush();
    m8_bc_flush();
    m8_cur_planes_ok = 0;
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
    m8_hot.screen = NULL;
    if( m8_bc_sel != 0 ) {
        FreeSelector( m8_bc_sel );
        m8_bc_sel = 0;
    }
    m8_strip_free( &m8_dst, &m8_dst_mem );
    m8_strip_free( &m8_src, &m8_src_mem );
    m8_busy = 0;
}

#if defined( M8_TRACE ) || defined( M8_PROF )
/* The timed entry points (see m8_tm, m8_pf_in). */
#undef BitBlt
#undef ExtTextOut
#undef Output
#undef Pixel
#undef StrBlt
#undef DibToDevice
#undef StretchBlt
#undef StretchDIBits
#undef SaveScreenBitmap
#undef ScanLR

#ifdef M8_PROF
#define M8_TIMED( i, type, call )   { type r; DWORD t = m8_pf_in(); r = call; \
                                      m8_pf_out( i, t ); return( r ); }
#else
#define M8_TIMED( i, type, call )   { type r; m8_tm( M8_MARK_TIME_IN ); r = call; \
                                      m8_tm( M8_MARK_TIME_OUT ); return( r ); }
#endif

BOOL WINAPI __loadds BitBlt( LPPDEVICE lpDestDev, WORD wDestX, WORD wDestY, LPPDEVICE lpSrcDev,
                             WORD wSrcX, WORD wSrcY, WORD wXext, WORD wYext, DWORD dwRop3,
                             LPBRUSH lpPBrush, LPDRAWMODE lpDrawMode )
M8_TIMED( 0, BOOL, m8t_BitBlt( lpDestDev, wDestX, wDestY, lpSrcDev, wSrcX, wSrcY, wXext, wYext,
                            dwRop3, lpPBrush, lpDrawMode ) )

DWORD WINAPI __loadds ExtTextOut( LPPDEVICE lpDestDev, WORD wDestXOrg, WORD wDestYOrg,
                                  LPRECT lpClipRect, LPSTR lpString, int wCount,
                                  LPFONTINFO lpFontInfo, LPDRAWMODE lpDrawMode,
                                  LPTEXTXFORM lpTextXForm, LPSHORT lpCharWidths,
                                  LPRECT lpOpaqueRect, WORD wOptions )
M8_TIMED( 1, DWORD, m8t_ExtTextOut( lpDestDev, wDestXOrg, wDestYOrg, lpClipRect, lpString, wCount,
                                 lpFontInfo, lpDrawMode, lpTextXForm, lpCharWidths,
                                 lpOpaqueRect, wOptions ) )

WORD WINAPI __loadds Output( LPPDEVICE lpDestDev, WORD wStyle, WORD wCount, LPPOINT lpPoints,
                             LPPPEN lpPPen, LPPBRUSH lpPBrush, LPDRAWMODE lpDrawMode,
                             LPRECT lpClipRect )
M8_TIMED( 2, WORD, m8t_Output( lpDestDev, wStyle, wCount, lpPoints, lpPPen, lpPBrush, lpDrawMode,
                            lpClipRect ) )

DWORD WINAPI __loadds Pixel( LPPDEVICE lpDestDev, WORD X, WORD Y, DWORD dwPhysColor,
                             LPDRAWMODE lpDrawMode )
M8_TIMED( 3, DWORD, m8t_Pixel( lpDestDev, X, Y, dwPhysColor, lpDrawMode ) )

DWORD WINAPI __loadds StrBlt( LPPDEVICE lpDestDev, WORD wDestXOrg, WORD wDestYOrg,
                              LPRECT lpClipRect, LPSTR lpString, WORD wCount,
                              LPFONTINFO lpFontInfo, LPDRAWMODE lpDrawMode,
                              LPTEXTXFORM lpTextXForm )
M8_TIMED( 4, DWORD, m8t_StrBlt( lpDestDev, wDestXOrg, wDestYOrg, lpClipRect, lpString, wCount,
                             lpFontInfo, lpDrawMode, lpTextXForm ) )

WORD WINAPI __loadds DibToDevice( LPPDEVICE lpDestDev, WORD X, WORD Y, WORD iScan, WORD cScans,
                                  LPRECT lpClipRect, LPDRAWMODE lpDrawMode, LPSTR lpDIBits,
                                  LPBITMAPINFO lpBitmapInfo, LPINT lpTranslate )
M8_TIMED( 5, WORD, m8t_DibToDevice( lpDestDev, X, Y, iScan, cScans, lpClipRect, lpDrawMode,
                                 lpDIBits, lpBitmapInfo, lpTranslate ) )

BOOL WINAPI __loadds StretchBlt( LPPDEVICE lpDestDev, WORD wDestX, WORD wDestY, WORD wDestWidth,
                                 WORD wDestHeight, LPPDEVICE lpSrcDev, WORD wSrcX, WORD wSrcY,
                                 WORD wSrcWidth, WORD wSrcHeight, DWORD dwRop3,
                                 LPBRUSH lpPBrush, LPDRAWMODE lpDrawMode, LPRECT lpClipRect )
M8_TIMED( 6, BOOL, m8t_StretchBlt( lpDestDev, wDestX, wDestY, wDestWidth, wDestHeight, lpSrcDev,
                                wSrcX, wSrcY, wSrcWidth, wSrcHeight, dwRop3, lpPBrush,
                                lpDrawMode, lpClipRect ) )

BOOL WINAPI __loadds StretchDIBits( LPPDEVICE lpDestDev, WORD fGet, WORD wDestX, WORD wDestY,
                                    WORD wDestWidth, WORD wDestHeight, WORD wSrcX, WORD wSrcY,
                                    WORD wSrcWidth, WORD wSrcHeight, VOID *lpBits,
                                    LPBITMAPINFO lpInfo, LPINT lpTranslate, DWORD dwRop3,
                                    LPBRUSH lpPBrush, LPDRAWMODE lpDrawMode, LPRECT lpClipRect )
M8_TIMED( 7, BOOL, m8t_StretchDIBits( lpDestDev, fGet, wDestX, wDestY, wDestWidth, wDestHeight,
                                   wSrcX, wSrcY, wSrcWidth, wSrcHeight, lpBits, lpInfo,
                                   lpTranslate, dwRop3, lpPBrush, lpDrawMode, lpClipRect ) )

WORD WINAPI __loadds SaveScreenBitmap( LPRECT lpRect, WORD wCmd )
M8_TIMED( 8, WORD, m8t_SaveScreenBitmap( lpRect, wCmd ) )

WORD WINAPI __loadds ScanLR( LPPDEVICE lpDestDev, WORD X, WORD Y, DWORD dwPhysColor, WORD wStyle )
M8_TIMED( 9, WORD, m8t_ScanLR( lpDestDev, X, Y, dwPhysColor, wStyle ) )
#endif
