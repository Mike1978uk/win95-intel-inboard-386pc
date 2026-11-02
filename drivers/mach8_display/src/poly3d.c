/* Flat polygons for a program, drawn by the engine (ExtEscape M8_ESC_POLY).
 *
 * Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
 *
 * A frame crosses the bus as its vertices and one colour per polygon; the
 * engine fills them (m8_hw_poly). They are drawn into a back buffer in the
 * off-screen rows DirectDraw would have, beside the polygon fill's scratch
 * area, and the changed part is copied to the program's window by the engine,
 * so nothing half-drawn is seen. Mode "direct" draws on the screen instead.
 *
 * The window's position comes from the program and is not clipped against
 * other windows: this is a test path, not a GDI one. 8 bpp only; at 16 bpp
 * the card is not the screen GDI draws on.
 *
 * Input, WORDs:
 *   begin  1, w, h                     out: WORD, the back buffer's row
 *   frame  2, flags, wx, wy, bg, clear l t r b, present l t r b, count,
 *          then per polygon: colour, n, n (x, y) pairs
 *   end    3
 * Coordinates are in the buffer, 0..w-1 and 0..h-1; rectangles are right and
 * bottom exclusive. flags bit 0 = direct.
 */

#include "m8ddi.h"
#include "m8.h"

#define M8P_BEGIN       1
#define M8P_FRAME       2
#define M8P_END         3
#define M8P_DIRECT      0x0001
#define M8P_MAX_COUNT   1024

static int  m8p_on;
static int  m8p_w;
static int  m8p_h;
static int  m8p_top;            /* the back buffer's row; the scratch is beside it */

/* What the scratch's planes hold since its last clear. */
static int  m8p_sl;
static int  m8p_st;
static int  m8p_sr;
static int  m8p_sb;

static void m8p_scratch_clear( void )
{
    if( m8p_sr > m8p_sl ) {
        m8_hw_fill( m8p_w + m8p_sl, m8p_top + m8p_st, m8p_sr - m8p_sl, m8p_sb - m8p_st, 0 );
    }
    m8p_sl = m8p_st = 0x7FFF;
    m8p_sr = m8p_sb = 0;
}

static int m8p_usable( void )
{
    return( m8p_on && !m8_hw_off && !m8_fb16 && m8_hot.screen != NULL );
}

static LONG m8p_begin( const WORD FAR *in, WORD FAR *out )
{
    int top;
    int rows;

    if( m8p_on || m8_hw_off || m8_fb16 || m8_hot.screen == NULL ) {
        return( 0 );
    }
    m8_dd_heap( &top, &rows );
    if( in[1] < 16 || in[2] < 16 || 2 * in[1] > M8_PITCH || in[2] > rows ) {
        return( 0 );
    }
    m8_dd_lend( 1 );
    m8p_w = in[1];
    m8p_h = in[2];
    m8p_top = top;
    m8p_on = 1;
    m8_cur_lift( 0, top, 2 * m8p_w, top + m8p_h );
    m8_hw_fill( 0, top, 2 * m8p_w, m8p_h, 0 );
    m8_leave();
    m8p_sl = m8p_st = 0x7FFF;
    m8p_sr = m8p_sb = 0;
    if( out != NULL ) {
        out[0] = top;
    }
    return( 1 );
}

/* 1 if every vertex lies in the buffer; the polygon's box is added to the
 * scratch's. */
static int m8p_inside( const short FAR *v, int n )
{
    int i;
    int x;
    int y;

    for( i = 0; i < n; ++i ) {
        x = v[2 * i];
        y = v[2 * i + 1];
        if( x < 0 || x >= m8p_w || y < 0 || y >= m8p_h ) {
            return( 0 );
        }
    }
    for( i = 0; i < n; ++i ) {
        x = v[2 * i];
        y = v[2 * i + 1];
        if( x < m8p_sl ) {
            m8p_sl = x;
        }
        if( x + 1 > m8p_sr ) {
            m8p_sr = x + 1;
        }
        if( y < m8p_st ) {
            m8p_st = y;
        }
        if( y + 1 > m8p_sb ) {
            m8p_sb = y + 1;
        }
    }
    return( 1 );
}

static LONG m8p_frame( const WORD FAR *in )
{
    int             direct = (in[1] & M8P_DIRECT) != 0;
    int             wx = (short)in[2];
    int             wy = (short)in[3];
    int             ox;
    int             oy;
    int             count = in[13];
    const short FAR *p = (const short FAR *)(in + 14);
    int             k;
    int             n;
    int             drawn = 0;
    int             l;
    int             t;
    int             r;
    int             b;

    if( !m8p_usable() || count > M8P_MAX_COUNT ) {
        return( 0 );
    }
    if( direct ) {
        /* Off the screen the engine would draw into other off-screen data. */
        if( wx < 0 || wy < 0 || wx + m8p_w > M8_SCREEN_X || wy + m8p_h > M8_SCREEN_Y ) {
            return( 0 );
        }
        ox = wx;
        oy = wy;
        m8_cur_lift( wx, wy, wx + m8p_w, wy + m8p_h );
    } else {
        ox = 0;
        oy = m8p_top;
        m8_cur_lift( wx + (short)in[9], wy + (short)in[10], wx + (short)in[11],
                     wy + (short)in[12] );
    }
    l = (short)in[5];
    t = (short)in[6];
    r = (short)in[7];
    b = (short)in[8];
    if( l < 0 ) {
        l = 0;
    }
    if( t < 0 ) {
        t = 0;
    }
    if( r > m8p_w ) {
        r = m8p_w;
    }
    if( b > m8p_h ) {
        b = m8p_h;
    }
    if( r > l && b > t ) {
        m8_hw_fill( ox + l, oy + t, r - l, b - t, (BYTE)in[4] );
    }
    if( !m8_hw_poly_begin() ) {
        m8_leave();
        return( 0 );
    }
    for( k = 0; k < count; ++k ) {
        n = p[1];
        if( n < 3 || n > M8_POLY_MAX ) {
            break;
        }
        if( m8p_inside( p + 2, n ) ) {
            if( !m8_hw_poly( ox, oy, m8p_w, m8p_top, p + 2, n, (BYTE)(1 << (drawn & 7)),
                             (BYTE)p[0] ) ) {
                break;
            }
            /* Eight planes used: clear them all at once. */
            if( (++drawn & 7) == 0 ) {
                m8_hw_poly_end();
                m8p_scratch_clear();
                m8_hw_poly_begin();
            }
        }
        p += 2 + 2 * n;
    }
    m8_hw_poly_end();
    m8p_scratch_clear();
    if( !direct ) {
        l = (short)in[9];
        t = (short)in[10];
        r = (short)in[11];
        b = (short)in[12];
        /* The part of the window on the screen. */
        if( l < 0 ) {
            l = 0;
        }
        if( t < 0 ) {
            t = 0;
        }
        if( r > m8p_w ) {
            r = m8p_w;
        }
        if( b > m8p_h ) {
            b = m8p_h;
        }
        if( wx + l < 0 ) {
            l = -wx;
        }
        if( wy + t < 0 ) {
            t = -wy;
        }
        if( wx + r > M8_SCREEN_X ) {
            r = M8_SCREEN_X - wx;
        }
        if( wy + b > M8_SCREEN_Y ) {
            b = M8_SCREEN_Y - wy;
        }
        if( r > l && b > t ) {
            m8_hw_copy( ox + l, oy + t, wx + l, wy + t, r - l, b - t );
        }
    }
    m8_leave();
    return( 1 );
}

LONG m8_poly_escape( LPVOID lpInput, LPVOID lpOutput )
{
    const WORD FAR *in = (const WORD FAR *)lpInput;

    switch( in[0] ) {
    case M8P_BEGIN:
        return( m8p_begin( in, (WORD FAR *)lpOutput ) );
    case M8P_FRAME:
        return( m8p_frame( in ) );
    case M8P_END:
        if( m8p_on ) {
            m8p_on = 0;
            m8_dd_lend( 0 );
        }
        return( 1 );
    }
    return( 0 );
}
