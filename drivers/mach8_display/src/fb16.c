/* 16 bpp on an 8 bpp card (docs/mach8_directdraw_plan.md, "16 bpp").
 *
 * Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
 *
 * The Mach8 shows 256 colours. In this mode GDI sees a 640x480 or 800x600 5-6-5 frame
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

/* 1 if the n words (n > 0) at sel:off all equal v: repe scasw, 32-bit address. */
static int m8_fb16_same( WORD sel, DWORD off, WORD n, WORD v );
#pragma aux m8_fb16_same = ".386" "push es" "mov es, dx" "shl ecx, 16" "mov cx, bx" "mov edi, ecx" "movzx ecx, si" "cld" "db 67h, 0F3h, 0AFh" "setz al" "mov ah, 0" "pop es" parm [dx] [cx bx] [si] [ax] value [ax] modify [cx di];

#define M8_FB16_MAXW    800         /* the largest 16 bpp mode */
#define M8_FB16_MAXH    600
#define M8_FB16_PITCH   ((DWORD)m8_scr_x * 2)

int                     m8_fb16;        /* this session's screen is 16 bpp */
RGBQUAD                 m8_fb16_pal[256];

static HGLOBAL          m8_fb16_mem;
static WORD             m8_fb16_sel;    /* the buffer the card shows */
static WORD             m8_fb16_msel;   /* GDI's: the frame buffer */
static HGLOBAL          m8_fb16_bmem;   /* DirectDraw's back buffer, while it has one */
static WORD             m8_fb16_bsel;
static LPDIBENGINE      m8_fb16_dev;
static int              m8_fb16_depth;  /* access calls open */

/* What the card shows, as fingerprints of the frame buffer: one per
 * M8_FB16_SEG_W pixels of each row, taken when that piece was last sent, 0
 * where the card may differ (a fill or copy it drew itself, or nothing sent
 * yet). A send skips the pieces whose fingerprint has not changed: hashing a
 * pixel costs the CPU a few cycles, sending it 3.8 us of bus. */
#define M8_FB16_SEG_W   32
#define M8_FB16_HASH_MIN 1024       /* pixels: smaller sends skip the fingerprints */
static HGLOBAL          m8_fb16_hmem;
static DWORD FAR        *m8_fb16_hash;  /* rows x m8_fb16_nseg */
static int              m8_fb16_nseg;

#pragma pack( push, 1 )
typedef struct {
    WORD    fbsel;
    DWORD   fboff;
    WORD    htsel;
    WORD    htoff;
    WORD    count;
    short   first;
    short   last;
} M8_FB16_RD;
#pragma pack( pop )

static M8_FB16_RD       m8_fb16_rd;

extern void M8_FB16_ROWDIFF( WORD rd );
#pragma aux M8_FB16_ROWDIFF "*" parm [bx] modify [];

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

static WORD m8_fb16_src[M8_FB16_MAXW];

/* fb16cv.asm's row tables: per phase (x & 3), near pointers to that dither
 * threshold's red, green, blue and grey parts; the cube; pattern scratch. */
typedef struct {
    WORD    r[4];
    WORD    g[4];
    WORD    b[4];
    WORD    y[4];
    WORD    cube;
    BYTE    pat[8];
} M8_FB16_CV;

static M8_FB16_CV       m8_fb16_cv;
static int              m8_fb16_cv_row = -1;    /* the y & 3 m8_fb16_cv is for */

extern void M8_FB16_CONV( WORD src, BYTE FAR *dst, WORD n, WORD cv, WORD phase );
#pragma aux M8_FB16_CONV "*" parm [si] [es di] [cx] [bx] [dx] modify [ax bx cx dx si di es];

/* Row y, columns l .. r - 1, dithered into dst. A grey stores red = blue =
 * green / 2 (5-bit red and blue, 6-bit green) and takes the grey ramp. */
/* The first n words of m8_fb16_src, as row y from column l, dithered. */
static void m8_fb16_conv( BYTE FAR *dst, int y, int l, int n )
{
    const BYTE  *th;
    int         k;

    if( m8_fb16_cv_row != (y & 3) ) {
        th = m8_fb16_bayer[y & 3];
        for( k = 0; k < 4; ++k ) {
            m8_fb16_cv.r[k] = FP_OFF( m8_fb16_r[th[k]] );
            m8_fb16_cv.g[k] = FP_OFF( m8_fb16_g[th[k]] );
            m8_fb16_cv.b[k] = FP_OFF( m8_fb16_b[th[k]] );
            m8_fb16_cv.y[k] = FP_OFF( m8_fb16_y[th[k]] );
        }
        m8_fb16_cv.cube = FP_OFF( m8_fb16_cube );
        m8_fb16_cv_row = y & 3;
    }
    M8_FB16_CONV( FP_OFF( m8_fb16_src ), dst, (WORD)n, FP_OFF( &m8_fb16_cv ), l & 3 );
}

static void m8_fb16_row( BYTE FAR *dst, int y, int l, int r )
{
    if( r <= l ) {
        return;
    }
    m8_copy32( m8_fb16_sel, (DWORD)y * M8_FB16_PITCH + (DWORD)l * 2, (BYTE FAR *)m8_fb16_src,
               (WORD)((r - l) * 2) );
    m8_fb16_conv( dst, y, l, r - l );
}

/* The same row a pixel at a time in C: FB16CHK's reference, so the check
 * does not trust the routine it checks. */
static void m8_fb16_row_ref( BYTE FAR *dst, int y, int l, int r )
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

/* The pieces under l, t .. r, b no longer known to match the card. */
static void m8_fb16_forget( int l, int t, int r, int b )
{
    int y;
    int s;
    int s0;
    int s1;

    if( m8_fb16_hash == NULL ) {
        return;
    }
    if( l < 0 ) l = 0;
    if( t < 0 ) t = 0;
    if( r > m8_scr_x ) r = m8_scr_x;
    if( b > m8_scr_y ) b = m8_scr_y;
    if( r <= l || b <= t ) {
        return;
    }
    s0 = l / M8_FB16_SEG_W;
    s1 = (r - 1) / M8_FB16_SEG_W;
    for( y = t; y < b; ++y ) {
        for( s = s0; s <= s1; ++s ) {
            m8_fb16_hash[(long)y * m8_fb16_nseg + s] = 0;
        }
    }
}

/* The fingerprint of a piece whose pixels are all c: M8_FB16_ROWDIFF's hash
 * of 16 dwords of c | c << 16. */
static DWORD m8_fb16_hash_colour( WORD c )
{
    DWORD   d = c | ((DWORD)c << 16);
    DWORD   h = 0x811C9DC5UL;
    int     i;

    for( i = 0; i < M8_FB16_SEG_W / 2; ++i ) {
        h += d;
        h += h << 10;
        h ^= h >> 6;
    }
    return( h != 0 ? h : 1 );
}

/* A one-colour fill the card now shows exactly: the pieces it covers whole
 * take c's fingerprint, so a later send of the same pixels skips them; the
 * pieces it covers in part are forgotten. */
static void m8_fb16_known( int l, int t, int r, int b, WORD c )
{
    DWORD   h;
    int     y;
    int     s;
    int     f0;
    int     f1;

    if( m8_fb16_hash == NULL ) {
        return;
    }
    f0 = (l + M8_FB16_SEG_W - 1) / M8_FB16_SEG_W;
    f1 = r / M8_FB16_SEG_W;
    if( f1 <= f0 ) {
        m8_fb16_forget( l, t, r, b );
        return;
    }
    m8_fb16_forget( l, t, f0 * M8_FB16_SEG_W, b );
    m8_fb16_forget( f1 * M8_FB16_SEG_W, t, r, b );
    h = m8_fb16_hash_colour( c );
    for( y = t; y < b; ++y ) {
        for( s = f0; s < f1; ++s ) {
            m8_fb16_hash[(long)y * m8_fb16_nseg + s] = h;
        }
    }
}

/* Row y's changed span within columns l .. r - 1 (even edges), with the
 * fingerprints brought up to date: 0 if nothing in it changed. */
static int m8_fb16_changed( int y, int l, int r, int *cl, int *cr )
{
    int s0 = l / M8_FB16_SEG_W;
    int s1 = (r - 1) / M8_FB16_SEG_W;

    m8_fb16_rd.fbsel = m8_fb16_sel;
    m8_fb16_rd.fboff = (DWORD)y * M8_FB16_PITCH + (DWORD)s0 * (M8_FB16_SEG_W * 2);
    m8_fb16_rd.htsel = FP_SEG( m8_fb16_hash );
    m8_fb16_rd.htoff = FP_OFF( m8_fb16_hash ) + (WORD)(((long)y * m8_fb16_nseg + s0) * 4);
    m8_fb16_rd.count = (WORD)(s1 - s0 + 1);
    M8_FB16_ROWDIFF( FP_OFF( &m8_fb16_rd ) );
    if( m8_fb16_rd.first < 0 ) {
        return( 0 );
    }
    *cl = (s0 + m8_fb16_rd.first) * M8_FB16_SEG_W > l ? (s0 + m8_fb16_rd.first) * M8_FB16_SEG_W : l;
    *cr = (s0 + m8_fb16_rd.last + 1) * M8_FB16_SEG_W < r ? (s0 + m8_fb16_rd.last + 1) * M8_FB16_SEG_W : r;
    return( 1 );
}

/* The rectangle to the card, in bands through m8_fb16_band, with the pointer
 * lifted if they overlap. Host transfers carry two pixels a word, so it
 * widens to even columns. */
/* Set while a send is known to be exactly what changed (M8DD32 compared it
 * with the frame buffer): fingerprinting it would find every piece changed. */
static int m8_fb16_whole;

static void m8_fb16_send( int l, int t, int r, int b )
{
    int w;
    int n;
    int y;
    int k;
    int whole;

    if( l < 0 ) l = 0;
    if( t < 0 ) t = 0;
    if( r > m8_scr_x ) r = m8_scr_x;
    if( b > m8_scr_y ) b = m8_scr_y;
    l &= ~1;
    r = (r + 1) & ~1;
    w = r - l;
    if( w <= 0 || b <= t ) {
        return;
    }
    ++m8_fb16_stat[1];
    m8_cur_lift( l, t, r, b );
    /* A small change costs the bus less than fingerprinting its pieces costs
     * the CPU: it is sent whole, and its pieces are forgotten so a later send
     * cannot skip them. */
    whole = m8_fb16_whole || (long)w * (b - t) < M8_FB16_HASH_MIN;
    if( m8_fb16_hash != NULL && whole ) {
        m8_fb16_forget( l, t, r, b );
    }
    if( m8_fb16_hash == NULL || whole ) {
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
        m8_leave();
        return;
    }
    /* Rows whose changed span is the same go out together; a row with
     * nothing changed is skipped. */
    {
        int bl = 0;
        int br = 0;
        int by = 0;
        int bn = 0;
        int cl;
        int cr;

        for( y = t; y < b; ++y ) {
            if( !m8_fb16_changed( y, l, r, &cl, &cr ) ) {
                cl = cr = 0;
            } else {
                cl &= ~1;
                cr = (cr + 1) & ~1;
            }
            if( bn > 0 && (cl != bl || cr != br || (bn + 1) * (br - bl) > (int)sizeof( m8_fb16_band )) ) {
                m8_hw_write_rect( bl, by, br - bl, bn, m8_fb16_band, (WORD)(br - bl) );
                m8_fb16_stat[2] += (DWORD)(br - bl) * bn;
                bn = 0;
            }
            if( cr <= cl ) {
                continue;
            }
            if( bn == 0 ) {
                bl = cl;
                br = cr;
                by = y;
            }
            m8_fb16_row( m8_fb16_band + bn * (br - bl), y, bl, br );
            ++bn;
        }
        if( bn > 0 ) {
            m8_hw_write_rect( bl, by, br - bl, bn, m8_fb16_band, (WORD)(br - bl) );
            m8_fb16_stat[2] += (DWORD)(br - bl) * bn;
        }
    }
    m8_leave();
}

/* Smaller than this, a one-colour rectangle goes as pixels: the scan and the
 * copies would cost more than the bytes. */
#define M8_FB16_SOLID_MIN   64

/* 1 if every pixel of the rectangle is the 5-6-5 colour *c. */
static int m8_fb16_uniform( int l, int t, int r, int b, WORD *c )
{
    DWORD   off = (DWORD)t * M8_FB16_PITCH + (DWORD)l * 2;
    WORD    v;
    int     y;

    m8_copy32( m8_fb16_sel, off, (BYTE FAR *)&v, 2 );
    for( y = t; y < b; ++y, off += M8_FB16_PITCH ) {
        if( !m8_fb16_same( m8_fb16_sel, off, (WORD)(r - l), v ) ) {
            return( 0 );
        }
    }
    *c = v;
    return( 1 );
}

/* A rectangle of colour c, which RAM need not hold: the dither repeats every
 * 4 pixels both ways, so a 4x4 block crosses the bus and engine copies double
 * it across and then down, each distance a multiple of 4. A colour that
 * dithers to one index is a plain fill. */
static void m8_fb16_solid( int l, int t, int r, int b, WORD c )
{
    BYTE    blk[16];
    int     w = r - l;
    int     h = b - t;
    int     bw = w < 4 ? w : 4;
    int     bh = h < 4 ? h : 4;
    int     done;
    int     n;
    int     k;

    for( k = 0; k < 4; ++k ) {
        m8_fb16_src[k] = c;
    }
    for( k = 0; k < bh; ++k ) {
        m8_fb16_conv( (BYTE FAR *)&blk[k * 4], t + k, l, bw );
    }
    for( k = 1; k < bw * bh && blk[(k / bw) * 4 + k % bw] == blk[0]; ++k ) {
    }
    ++m8_fb16_stat[3];
    if( k == bw * bh ) {
        m8_cur_lift( l, t, r, b );
        m8_hw_fill( l, t, w, h, blk[0] );
        m8_leave();
        m8_fb16_known( l, t, r, b, c );
        return;
    }
    m8_cur_lift( l, t, r, b );
    /* Host words carry two pixels: an odd-width block goes a pixel at a time. */
    if( bw & 1 ) {
        for( k = 0; k < bw * bh; ++k ) {
            m8_hw_fill( l + k % bw, t + k / bw, 1, 1, blk[(k / bw) * 4 + k % bw] );
        }
    } else {
        m8_hw_write_rect( l, t, bw, bh, (BYTE FAR *)blk, 4 );
    }
    for( done = bw; done < w; done += n ) {
        n = w - done < done ? w - done : done;
        m8_hw_copy( l, t, l + done, t, n, bh );
    }
    for( done = bh; done < h; done += n ) {
        n = h - done < done ? h - done : done;
        m8_hw_copy( l, t, l, t + done, w, n );
    }
    m8_leave();
    m8_fb16_known( l, t, r, b, c );
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

/* One dirty rectangle to the card, clipped to the screen. */
static void m8_fb16_flush( const M8_FB16_RECT *d )
{
    int     l = d->l < 0 ? 0 : d->l;
    int     t = d->t < 0 ? 0 : d->t;
    int     r = d->r > m8_scr_x ? m8_scr_x : d->r;
    int     b = d->b > m8_scr_y ? m8_scr_y : d->b;
    WORD    c;

    if( r <= l || b <= t ) {
        return;
    }
    if( (long)(r - l) * (b - t) >= M8_FB16_SOLID_MIN && m8_fb16_uniform( l, t, r, b, &c ) ) {
        m8_fb16_solid( l, t, r, b, c );
    } else {
        m8_fb16_send( l, t, r, b );
    }
}

/* A rectangle DirectDraw changed (M8_ESC_FB16SEND), as GDI's are sent;
 * `solid` 1: DirectDraw filled it with one colour, so it is not scanned;
 * 2: it is exactly what changed, so it is sent whole. */
void m8_fb16_send_rect( int l, int t, int r, int b, int solid )
{
    M8_FB16_RECT    d;

    if( solid == 2 ) {
        m8_fb16_whole = 1;
        m8_fb16_send( l, t, r, b );
        m8_fb16_whole = 0;
        return;
    }

    if( solid ) {
        if( l < 0 ) l = 0;
        if( t < 0 ) t = 0;
        if( r > m8_scr_x ) r = m8_scr_x;
        if( b > m8_scr_y ) b = m8_scr_y;
        if( r > l && b > t ) {
            WORD c;

            m8_copy32( m8_fb16_sel, (DWORD)t * M8_FB16_PITCH + (DWORD)l * 2, (BYTE FAR *)&c, 2 );
            m8_fb16_solid( l, t, r, b, c );
        }
        return;
    }
    d.l = l;
    d.t = t;
    d.r = r;
    d.b = b;
    m8_fb16_flush( &d );
}

/* The screen copied from (sx, sy) to l, t .. r, b, already done in RAM. The
 * dither depends on x & 3 and y & 3 only, so a copy that moves by a multiple
 * of 4 both ways is the card's own copy, exact; any other is sent. The pointer
 * is lifted from both areas, as the engine would copy it too. */
void m8_fb16_copy_rect( int sx, int sy, int l, int t, int r, int b )
{
    M8_FB16_RECT    d;
    int             w = r - l;
    int             h = b - t;

    if( w <= 0 || h <= 0 ) {
        return;
    }
    if( ((sx - l) & 3) == 0 && ((sy - t) & 3) == 0 && l >= 0 && t >= 0 && sx >= 0 &&
        sy >= 0 && r <= m8_scr_x && b <= m8_scr_y && sx + w <= m8_scr_x &&
        sy + h <= m8_scr_y ) {
        ++m8_fb16_stat[3];
        m8_fb16_forget( l, t, r, b );
        m8_cur_lift( l < sx ? l : sx, t < sy ? t : sy, r > sx + w ? r : sx + w,
                     b > sy + h ? b : sy + h );
        m8_hw_copy( sx, sy, l, t, w, h );
        m8_leave();
        return;
    }
    d.l = l;
    d.t = t;
    d.r = r;
    d.b = b;
    m8_fb16_flush( &d );
}

/* The frame buffer's linear address, DirectDraw's primary; 0 if none. */
DWORD __far m8_fb16_lin( void )
{
    return( m8_fb16_sel != 0 ? GetSelectorBase( m8_fb16_sel ) : 0 );
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
            m8_fb16_flush( &m8_fb16_dirty[k] );
        }
        m8_fb16_ndirty = 0;
    }
}

/* Lines and shapes: inside the box around their points, widened for the
 * pen. Scanline lists and markers are not boxes of their points. */
/* A segment's pixels lie within one of the ideal line (a pen here is one
 * pixel wide), so it goes as strips M8_FB16_SEG long on its major axis, each
 * as high as the line moves across it, clipped as the DIB Engine clips. */
#define M8_FB16_SEG     32

static void m8_fb16_strip( int l, int t, int r, int b, LPRECT clip )
{
    M8_FB16_RECT    d;

    d.l = l;
    d.t = t;
    d.r = r;
    d.b = b;
    if( clip != NULL ) {
        if( d.l < clip->left ) d.l = clip->left;
        if( d.t < clip->top ) d.t = clip->top;
        if( d.r > clip->right ) d.r = clip->right;
        if( d.b > clip->bottom ) d.b = clip->bottom;
    }
    m8_fb16_flush( &d );
}

static void m8_fb16_segment( int x0, int y0, int x1, int y1, LPRECT clip )
{
    long    dx = (long)x1 - x0;
    long    dy = (long)y1 - y0;
    long    a;
    long    b;
    long    m0;
    long    m1;
    int     xmajor = (dx < 0 ? -dx : dx) >= (dy < 0 ? -dy : dy);
    int     lo = xmajor ? (x0 < x1 ? x0 : x1) : (y0 < y1 ? y0 : y1);
    int     hi = xmajor ? (x0 < x1 ? x1 : x0) : (y0 < y1 ? y1 : y0);
    int     m;

    /* A level or upright segment is one strip, which the solid path takes;
     * a segment never draws its end point, so the strip stops short of it. */
    if( (xmajor ? dy : dx) == 0 ) {
        if( xmajor ) {
            m8_fb16_strip( x1 > x0 ? x0 : x1 + 1, y0, x1 > x0 ? x1 : x0 + 1, y0 + 1, clip );
        } else {
            m8_fb16_strip( x0, y1 > y0 ? y0 : y1 + 1, x0 + 1, y1 > y0 ? y1 : y0 + 1, clip );
        }
        return;
    }
    for( m = lo; m <= hi; m += M8_FB16_SEG ) {
        a = m;
        b = m + M8_FB16_SEG - 1 < hi ? m + M8_FB16_SEG - 1 : hi;
        /* The minor coordinate at both ends of the strip, rounded down. */
        if( xmajor ) {
            m0 = dx == 0 ? y0 : y0 + (a - x0) * dy / dx;
            m1 = dx == 0 ? y0 : y0 + (b - x0) * dy / dx;
        } else {
            m0 = dy == 0 ? x0 : x0 + (a - y0) * dx / dy;
            m1 = dy == 0 ? x0 : x0 + (b - y0) * dx / dy;
        }
        if( m0 > m1 ) {
            long k = m0;
            m0 = m1;
            m1 = k;
        }
        if( (xmajor ? dy : dx) != 0 ) {
            --m0;
            m1 += 1;
        }
        if( xmajor ) {
            m8_fb16_strip( (int)a, (int)m0, (int)b + 1, (int)m1 + 1, clip );
        } else {
            m8_fb16_strip( (int)m0, (int)a, (int)m1 + 1, (int)b + 1, clip );
        }
    }
}

WORD m8_fb16_output( LPPDEVICE lpDestDev, WORD wStyle, WORD wCount, LPPOINT lpPoints,
                     LPPPEN lpPPen, LPPBRUSH lpPBrush, LPDRAWMODE lpDrawMode,
                     LPRECT lpClipRect )
{
    WORD    rc;
    WORD    i;

    if( wStyle == OS_POLYLINE && lpPoints != NULL && wCount >= 2 && m8_fb16_depth == 0 &&
        (LPVOID)lpDestDev == (LPVOID)m8_fb16_dev ) {
        /* Nothing marked while the DIB Engine draws; the strips go after. */
        m8_fb16_lim.l = m8_fb16_lim.r = 0;
        m8_fb16_lim_on = 1;
        rc = DIB_Output( lpDestDev, wStyle, wCount, lpPoints, lpPPen, lpPBrush, lpDrawMode,
                         lpClipRect );
        m8_fb16_lim_on = 0;
        for( i = 1; i < wCount; ++i ) {
            m8_fb16_segment( lpPoints[i - 1].xcoord, lpPoints[i - 1].ycoord, lpPoints[i].xcoord,
                             lpPoints[i].ycoord, lpClipRect );
        }
        return( rc );
    }
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

    if( (LPVOID)lpDestDev == (LPVOID)m8_fb16_dev && (LPVOID)lpSrcDev == (LPVOID)m8_fb16_dev &&
        dwRop3 == 0x00CC0020UL && m8_fb16_depth == 0 && (short)wXext > 0 && (short)wYext > 0 &&
        ((wSrcX - wDestX) & 3) == 0 && ((wSrcY - wDestY) & 3) == 0 ) {
        /* Nothing marked while the DIB Engine copies; the card copies after. */
        m8_fb16_lim.l = m8_fb16_lim.r = 0;
        m8_fb16_lim_on = 1;
        rc = DIB_BitBlt( lpDestDev, wDestX, wDestY, lpSrcDev, wSrcX, wSrcY, wXext, wYext,
                         dwRop3, lpPBrush, lpDrawMode );
        m8_fb16_lim_on = 0;
        m8_fb16_copy_rect( (short)wSrcX, (short)wSrcY, (short)wDestX, (short)wDestY,
                           (short)wDestX + (short)wXext, (short)wDestY + (short)wYext );
        return( rc );
    }
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

/* `dwords` zero dwords from sel:0, a 32-bit address (addr32 rep stosd). */
static void m8_fb16_zero( WORD sel, DWORD dwords );
#pragma aux m8_fb16_zero = ".386" "push es" "mov es, dx" "shl ecx, 16" "mov cx, bx" "xor edi, edi" "xor eax, eax" "cld" "db 67h, 0F3h, 66h, 0ABh" "pop es" parm [dx] [cx bx] modify [ax cx di];

/* A new size: what the frame buffer holds is at the old pitch, and a program
 * that only erases what it moves keeps it as its background. */
void m8_fb16_clear( void )
{
    if( m8_fb16_msel != 0 ) {
        m8_fb16_zero( m8_fb16_msel, (DWORD)M8_FB16_MAXW * M8_FB16_MAXH / 2 );
    }
}

/* The frame buffer, once. 0 = no memory. */
int m8_fb16_alloc( void )
{
    if( m8_fb16_sel != 0 ) {
        return( 1 );
    }
    /* Sized for the largest 16 bpp mode, so a resolution change reuses it:
     * DirectDraw holds its linear address. */
    m8_fb16_mem = GlobalAlloc( GMEM_FIXED | GMEM_SHARE | GMEM_ZEROINIT,
                               (DWORD)M8_FB16_MAXW * 2 * M8_FB16_MAXH );
    if( m8_fb16_mem == 0 ) {
        return( 0 );
    }
    GlobalSmartPageLock( m8_fb16_mem );
    m8_fb16_sel = FP_SEG( GlobalLock( m8_fb16_mem ) );
    m8_fb16_msel = m8_fb16_sel;
    m8_fb16_tables();
    /* The fingerprints, if memory allows: without them every send is whole. */
    m8_fb16_nseg = M8_FB16_MAXW / M8_FB16_SEG_W;
    m8_fb16_hmem = GlobalAlloc( GMEM_FIXED | GMEM_ZEROINIT,
                                (DWORD)M8_FB16_MAXH * m8_fb16_nseg * sizeof( DWORD ) );
    if( m8_fb16_hmem != 0 && (DWORD)M8_FB16_MAXH * m8_fb16_nseg * sizeof( DWORD ) < 0x10000UL ) {
        m8_fb16_hash = (DWORD FAR *)GlobalLock( m8_fb16_hmem );
    }
    return( 1 );
}

/* The screen device over the frame buffer. `info` has room for the header
 * and three colour masks. */
void m8_fb16_device( LPDIBENGINE dev, LPBITMAPINFO info )
{
    DWORD FAR *masks = (DWORD FAR *)info->bmiColors;

    _fmemset( &info->bmiHeader, 0, sizeof( info->bmiHeader ) );
    info->bmiHeader.biSize = sizeof( info->bmiHeader );
    info->bmiHeader.biWidth = m8_scr_x;
    info->bmiHeader.biHeight = m8_scr_y;
    info->bmiHeader.biPlanes = 1;
    info->bmiHeader.biBitCount = 16;
    info->bmiHeader.biCompression = BI_BITFIELDS;
    masks[0] = 0xF800UL;
    masks[1] = 0x07E0UL;
    masks[2] = 0x001FUL;

    dev->deType = TYPE_DIBENG;
    dev->deFlags = MINIDRIVER | VRAM | FIVE6FIVE;
    dev->deWidth = m8_scr_x;
    dev->deHeight = m8_scr_y;
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
    BYTE    *have = m8_fb16_band + m8_scr_x;
    int     x;
    int     y;

    out[0] = 0;
    out[1] = m8_scr_x;
    out[2] = m8_scr_y;
    out[3] = 0;
    out[4] = 0;
    m8_cur_lift( 0, 0, m8_scr_x, m8_scr_y );
    for( y = 0; y < m8_scr_y; ++y ) {
        m8_fb16_row_ref( (BYTE FAR *)want, y, 0, m8_scr_x );
        m8_hw_read_rect( 0, y, m8_scr_x, 1, (BYTE FAR *)have, m8_scr_x );
        for( x = 0; x < m8_scr_x; ++x ) {
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

/* M8_ESC_FB16BATCH: M8DD32's flip in one call. `in` is a count, then that
 * many entries of six WORDs: op, l, t, r, b, arg. Ops: 0 send (arg as
 * m8_fb16_send_rect's `solid`); 1 fill with 5-6-5 colour arg, RAM not read;
 * 2 show buffer arg (0 the frame buffer, 1 the back buffer); 3 allocate the
 * back buffer, its linear address to *out (0: no memory); 4 free it; 5 the card
 * changed there behind the row fingerprints (Direct3D's triangles), so they are
 * dropped, or a later send would see RAM unchanged and leave those pixels. */
void m8_fb16_batch( const WORD FAR *in, DWORD FAR *out )
{
    const WORD FAR  *e = in + 1;
    WORD            n;
    int             l;
    int             t;
    int             r;
    int             b;

    /* Registers are written afresh at each flip: the shadows have been found
     * stale here (a black fill drawn white), cause not yet found. */
    m8_hw_forget();
    for( n = in[0]; n > 0; --n, e += 6 ) {
        l = (short)e[1] < 0 ? 0 : (short)e[1];
        t = (short)e[2] < 0 ? 0 : (short)e[2];
        r = (short)e[3] > m8_scr_x ? m8_scr_x : (short)e[3];
        b = (short)e[4] > m8_scr_y ? m8_scr_y : (short)e[4];
        switch( e[0] ) {
        case 0:
            m8_fb16_send_rect( l, t, r, b, e[5] );
            break;
        case 1:
            if( r > l && b > t ) {
                m8_fb16_solid( l, t, r, b, e[5] );
            }
            break;
        case 2:
            m8_fb16_sel = (e[5] && m8_fb16_bsel) ? m8_fb16_bsel : m8_fb16_msel;
            break;
        case 3:
            if( m8_fb16_bmem == 0 ) {
                m8_fb16_bmem = GlobalAlloc( GMEM_FIXED | GMEM_SHARE,
                                            (DWORD)m8_scr_x * 2 * m8_scr_y );
                if( m8_fb16_bmem != 0 ) {
                    m8_fb16_bsel = FP_SEG( GlobalLock( m8_fb16_bmem ) );
                }
            }
            if( out != NULL ) {
                *out = m8_fb16_bsel ? GetSelectorBase( m8_fb16_bsel ) : 0;
            }
            break;
        case 4:
            m8_fb16_sel = m8_fb16_msel;
            if( m8_fb16_bmem != 0 ) {
                GlobalUnlock( m8_fb16_bmem );
                GlobalFree( m8_fb16_bmem );
                m8_fb16_bmem = 0;
                m8_fb16_bsel = 0;
            }
            break;
        case 5:
            if( r > l && b > t ) {
                m8_fb16_forget( l, t, r, b );
            }
            break;
        }
    }
}

/* The whole frame buffer to the card, after the mode is set: what the card
 * held, the pointer and its masks among it, is gone. A mode set does not clear
 * video memory, and the send takes a visible time row by row, so one fill first
 * or the old contents show as static at the new pitch. */
void m8_fb16_send_all( void )
{
    m8_hw_set_palette( 0, 256, m8_fb16_pal );
    m8_cur_reset();
    m8_hw_fill( 0, 0, m8_scr_x, m8_scr_y, 0 );
    m8_fb16_forget( 0, 0, m8_scr_x, m8_scr_y );
    m8_fb16_send( 0, 0, m8_scr_x, m8_scr_y );
}
