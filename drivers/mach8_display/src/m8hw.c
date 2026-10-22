/* Mach8 hardware access: mode set, palette, and the engine primitives the
 * driver builds on.
 *
 * Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
 */

#include "m8ddi.h"
#include <conio.h>
#include "m8.h"

/* Set while a DOS session has the screen (INT 2Fh 4001h-4002h): every primitive
 * returns without touching the card. A read-back then leaves its buffer as it
 * was; nothing it leads to reaches the screen. */
int m8_hw_off;

#define M8_WAIT_LIMIT       60000u      /* polls; ~0.4 s of GP_STAT reads on the 5160 */

/* Pixel data by string I/O, as ATIM8 moves it: the C loop costs more per word
 * than the port does. ISA wait states pace the card's FIFO. */
extern void m8_outsw( const WORD FAR *src, WORD words );
#pragma aux m8_outsw = \
    "push ds" "push es" "pop ds" "mov dx, 0E2E8h" "rep outsw" "pop ds" \
    parm [es si] [cx] modify [si cx dx];

extern void m8_insw( WORD FAR *dst, WORD words );
#pragma aux m8_insw = \
    "mov dx, 0E2E8h" "rep insw" \
    parm [es di] [cx] modify [di cx dx];

/* One register write: port, value, and whether the port takes a byte. */
typedef struct {
    WORD    port;
    WORD    value;
    WORD    is_byte;
} M8_REG;

/* The modes, with ATIM8's values for this card: which shadow set it loads, the
 * ADVFUNC_CNTL that selects it, the CRTC registers H_TOTAL .. V_SYNC_WID (the
 * horizontal four and V_SYNC_WID take a byte) and CLOCK_SEL. 800x600 from its
 * 800x600 mode set; 640x480 and 1024x768 from the two sets it loads at start.
 * Source: docs/captures/2026-10-16_atim8_tap/ (atim8_tap_1016.log.gz). The
 * EEPROM has a Mach8 record for none of these (docs/mach8_levers.md). */
typedef struct {
    int     x;
    int     y;
    WORD    set;
    WORD    advfunc;
    WORD    crtc[8];
    WORD    clock;
} M8_MODE;

static const M8_MODE m8_modes[M8_MODE_COUNT] = {
    {  640, 480, 1, 0x0003,
       { 0x69, 0x4F, 0x53, 0x05, 0x0412, 0x03BF, 0x03D4, 0x03 }, 0x1824 },
    {  800, 600, 2, 0x0007,
       { 0x82, 0x63, 0x66, 0x2E, 0x0535, 0x04AB, 0x04C9, 0x02 }, 0x1E10 },
    { 1024, 768, 2, 0x0007,
       { 0xA1, 0x7F, 0x81, 0x12, 0x0649, 0x05FF, 0x0602, 0x06 }, 0x1E38 },
};

static const WORD m8_crtc_port[8] = {
    0x02E8, 0x06E8, 0x0AE8, 0x0EE8, 0x12E8, 0x16E8, 0x1AE8, 0x1EE8
};

/* After the timings: the drawing engine as ATIM8 leaves it. */
static const M8_REG m8_engine_init[] = {
    { 0x76EE, 0x0080, 0 },      /* GE_PITCH: 1024 / 8 */
    { 0x6EEE, 0x0000, 0 },      /* GE_OFFSET */
    { 0x72EE, 0x0000, 0 },
    { 0xB2E8, 0x0000, 0 },      /* COLOR_CMP */
    { 0xAAE8, 0x00FF, 0 },      /* WRT_MASK */
    { 0xAEE8, 0x00FF, 0 },      /* RD_MASK */
    { 0xB6EE, 0x0002, 0 },
    { 0xDAEE, 0x0800, 0 },      /* EXT_SCISSOR_L */
    { 0xDEEE, 0x0800, 0 },      /* EXT_SCISSOR_T */
    { 0xE2EE, 0x07FF, 0 },      /* EXT_SCISSOR_R */
    { 0xE6EE, 0x07FF, 0 },      /* EXT_SCISSOR_B */
    { 0xCEEE, 0x2211, 0 },      /* DP_CONFIG */
    { M8_MULTIFUNC_CNTL, M8_MF_PIX_CNTL, 0 },
};

int m8_scr_x = 800;
int m8_scr_y = 600;

static void m8_write_regs( const M8_REG *regs, int count )
{
    int i;

    for( i = 0; i < count; ++i ) {
        if( regs[i].is_byte ) {
            outp( regs[i].port, (BYTE)regs[i].value );
        } else {
            outpw( regs[i].port, regs[i].value );
        }
    }
}

/* Free FIFO entries since the last wait, less the setup writes m8_put sent. */
static int m8_room;

static void m8_engine_reset( void );

/* Every engine call starts from an empty FIFO, so a burst of writes follows
 * without another check. A write with all 16 entries full overruns the FIFO and
 * locks it (INVALID_IO, guide 8-14); bursts stop one short of the depth. */
static int m8_wait_fifo_empty( void )
{
    WORD n;

    m8_room = M8_FIFO_DEPTH - 1;
    for( n = 0; n < M8_WAIT_LIMIT; ++n ) {
        if( inpw( M8_EXT_FIFO_STATUS ) == 0 ) {
            return( 1 );
        }
    }
    m8_engine_reset();
    return( 0 );
}

/* Room for n more writes: with the cache cold the setup alone can fill the FIFO. */
static void m8_room_for( int n )
{
    if( m8_room < n ) {
        m8_wait_fifo_empty();
    }
}

/* The setup registers hold their value until written, so each is written only
 * when it changes; every routine states the ones it depends on and leaves them
 * set. Coordinates, MIN_AXIS and CMD go every time, and so does the data path:
 * DP_CONFIG, FRGD_MIX, BKGD_MIX and PIX_CNTL program one set of sources and ALU
 * functions from two views (guide 7-1, 9-13), so a write to any of them changes
 * what the others hold - do not cache them; a stale one hangs the engine (card).
 * The cache is emptied when anything but these routines may have set the card:
 * mode set, VGA through, engine reset. */
enum {
    M8_SH_FRGD_MIX, M8_SH_BKGD_MIX, M8_SH_FRGD_COLOR, M8_SH_BKGD_COLOR,
    M8_SH_WRT_MASK, M8_SH_RD_MASK, M8_SH_DP_CONFIG, M8_SH_PIX_CNTL,
    M8_SH_SC_L, M8_SH_SC_T, M8_SH_SC_R, M8_SH_SC_B,
    M8_SH_COUNT
};

static const WORD m8_sh_port[M8_SH_COUNT] = {
    M8_FRGD_MIX, M8_BKGD_MIX, M8_FRGD_COLOR, M8_BKGD_COLOR,
    M8_WRT_MASK, M8_RD_MASK, M8_DP_CONFIG, M8_MULTIFUNC_CNTL,
    M8_EXT_SCISSOR_L, M8_EXT_SCISSOR_T, M8_EXT_SCISSOR_R, M8_EXT_SCISSOR_B
};

static WORD m8_sh_val[M8_SH_COUNT];
static BYTE m8_sh_ok[M8_SH_COUNT];

static void m8_sh_clear( void )
{
    int i;

    for( i = 0; i < M8_SH_COUNT; ++i ) {
        m8_sh_ok[i] = 0;
    }
}

/* A wait that runs out means the engine is wedged: a transfer left unfinished,
 * or a locked FIFO (INVALID_IO), which ignores every write until the engine is
 * reset (guide 8-14). ATIM8 resets the same way; the set-up is then rewritten. */
static void m8_engine_reset( void )
{
    outpw( M8_SUBSYS_CNTL, M8_SUBSYS_RESET );
    outpw( M8_SUBSYS_CNTL, M8_SUBSYS_RUN );
    m8_write_regs( m8_engine_init, sizeof( m8_engine_init ) / sizeof( m8_engine_init[0] ) );
    m8_sh_clear();
    m8_room = 0;
}

static void m8_put( int r, WORD v )
{
    m8_room_for( 1 );
    outpw( m8_sh_port[r], v );
    --m8_room;
    m8_sh_val[r] = v;
    m8_sh_ok[r] = 1;
}

/* The unchanged case is inline: it runs a dozen times per operation. */
#define M8_SET( r, v ) \
    do { \
        WORD m8_v_ = (v); \
        if( !m8_sh_ok[r] || m8_sh_val[r] != m8_v_ ) { \
            m8_put( r, m8_v_ ); \
        } \
    } while( 0 )

/* Clip rectangle for every engine path, inclusive edges; the scissors clip the
 * 8514/A commands as well as the extended ones. */
static void m8_scissors( int l, int t, int r, int b )
{
    M8_SET( M8_SH_SC_L, l );
    M8_SET( M8_SH_SC_T, t );
    M8_SET( M8_SH_SC_R, r );
    M8_SET( M8_SH_SC_B, b );
}

/* What the 8514/A commands expect: all planes, the 8514 data path, the mixes
 * chosen by FRGD_MIX alone, no clip. */
static void m8_plain( void )
{
    M8_SET( M8_SH_WRT_MASK, 0x00FF );
    M8_SET( M8_SH_RD_MASK, 0x00FF );
    m8_put( M8_SH_DP_CONFIG, M8_DP_8514 );
    m8_put( M8_SH_PIX_CNTL, M8_MF_PIX_CNTL );
    m8_scissors( M8_EXT_SCISSOR_MIN, M8_EXT_SCISSOR_MIN, M8_EXT_SCISSOR_MAX, M8_EXT_SCISSOR_MAX );
}

int m8_hw_wait_idle( void )
{
    WORD n;

    /* Idle is an empty FIFO as well as GE_BUSY clear, as ATIM8 tests it: a
     * command still queued has not made the engine busy yet. */
    for( n = 0; n < M8_WAIT_LIMIT; ++n ) {
        if( inpw( M8_EXT_FIFO_STATUS ) == 0 && !(inpw( M8_GP_STAT ) & M8_GP_BUSY) ) {
            return( 1 );
        }
    }
    m8_engine_reset();
    return( 0 );
}

int m8_hw_mode_size( int mode, int *x, int *y )
{
    if( mode < 0 || mode >= M8_MODE_COUNT ) {
        return( 0 );
    }
    *x = m8_modes[mode].x;
    *y = m8_modes[mode].y;
    return( 1 );
}

/* ATIM8's order: display off and VGA through while the shadow set loads,
 * timings, CLOCK_SEL, lock; then the set selected and the display on. */
int m8_hw_set_mode( int mode )
{
    const M8_MODE   *m;
    int             i;

    if( mode < 0 || mode >= M8_MODE_COUNT ) {
        return( 0 );
    }
    m = &m8_modes[mode];
    m8_sh_clear();
    outpw( M8_DISP_CNTL, 0x0060 );                      /* display off */
    outpw( M8_ADVFUNC_CNTL, 0x0002 );                   /* VGA through */
    outp( 0x7AEE, 0x0A );                               /* EXT_GE_CONFIG */
    outpw( M8_DISP_CNTL, 0x0020 );
    outpw( 0x5AEE, m->set );                            /* SHADOW_SET */
    outpw( 0x46EE, 0x0000 );                            /* SHADOW_CTL: unlocked */
    for( i = 0; i < 8; ++i ) {
        if( i < 4 || i == 7 ) {
            outp( m8_crtc_port[i], (BYTE)m->crtc[i] );
        } else {
            outpw( m8_crtc_port[i], m->crtc[i] );
        }
    }
    outp( M8_DISP_CNTL, 0x23 );
    outpw( 0x4AEE, m->clock );                          /* CLOCK_SEL */
    outpw( 0x46EE, 0x003F );                            /* SHADOW_CTL: locked */
    outpw( 0x5AEE, 0x0000 );
    outpw( 0x46EE, 0x0000 );
    outpw( M8_DISP_CNTL, 0x0060 );
    outpw( M8_ADVFUNC_CNTL, m->advfunc );               /* 8514 output, this set */
    outp( 0x7AEE, 0x0A );
    outpw( M8_DISP_CNTL, 0x0020 );                      /* display on */
    m8_write_regs( m8_engine_init, sizeof( m8_engine_init ) / sizeof( m8_engine_init[0] ) );
    m8_scr_x = m->x;
    m8_scr_y = m->y;
    m8_hw_fill( 0, 0, M8_SCREEN_X, M8_SCREEN_Y, 0 );
    return( m8_hw_wait_idle() );
}

/* Hand the monitor back to the VGA half (DOS, Disable). */
void m8_hw_passthrough( void )
{
    m8_hw_wait_idle();
    outpw( M8_ADVFUNC_CNTL, 0x0002 );
    m8_sh_clear();
}

/* The 8514 DAC ports; the RAMDAC is an IMSG176, 6 bits per gun. */
void m8_hw_set_palette( UINT start, UINT count, RGBQUAD FAR *pal )
{
    UINT i;

    if( m8_hw_off ) {
        return;
    }
    outp( 0x02EA, 0xFF );
    outp( 0x02EC, (BYTE)start );
    for( i = 0; i < count; ++i ) {
        outp( 0x02ED, pal[i].rgbRed >> 2 );
        outp( 0x02ED, pal[i].rgbGreen >> 2 );
        outp( 0x02ED, pal[i].rgbBlue >> 2 );
    }
}

/* A rectangle through the foreground mix (a full mix-register value) with
 * FRGD_COLOR as its source. */
void m8_hw_fill_mix( int x, int y, int w, int h, BYTE color, WORD mix )
{
    if( m8_hw_off || w <= 0 || h <= 0 ) {
        return;
    }
    m8_wait_fifo_empty();
    m8_plain();
    m8_put( M8_SH_FRGD_MIX, mix );
    M8_SET( M8_SH_FRGD_COLOR, color );
    m8_room_for( 5 );
    outpw( M8_CUR_X, x );
    outpw( M8_CUR_Y, y );
    outpw( M8_MAJ_AXIS_PCNT, w - 1 );
    outpw( M8_MULTIFUNC_CNTL, M8_MF_MIN_AXIS | (h - 1) );
    outpw( M8_CMD, M8_CMD_FILL );
    ++m8_stats[M8_STAT_FILL_HW];
}

void m8_hw_fill( int x, int y, int w, int h, BYTE color )
{
    m8_hw_fill_mix( x, y, w, h, color, M8_MIX_SRC_FRGD | M8_MIX_REPLACE );
}

/* An 8x8 two-colour pattern, a row at a time through the linear mono pattern,
 * as ATIM8 fills hatches (seg16:3fe9): the X range and phase once, then per row
 * its pattern byte and DEST_Y_END, which draws the row and leaves the engine on
 * the next. Index 0 is bit 7 of the low byte, so pat[] goes in as it is with the
 * phase x & 7. The pattern repeats along a row only: PATT_LENGTH bits 8, 13 and
 * 14 do not step it from row to row on this card (tools/m8probe/M8PATT.C). */
/* A span one row tall goes through SCAN_TO_X (guide 9-61), as ATIM8's scanline
 * routine draws them (seg08:0dcd): CUR_X and one write draw it, on the row and
 * pattern already loaded. The row, its pattern byte and the engine's place are
 * remembered so only what changed is written; anything else that draws resets
 * them. CUR_X is written every time (guide 9-61, caution). */
static int  m8_scan_y = -1;
static WORD m8_scan_byte = 0xFFFF;

void m8_hw_pattern_begin( BYTE fg, BYTE bg, WORD fg_fn, WORD bg_fn )
{
    if( m8_hw_off ) {
        return;
    }
    m8_wait_fifo_empty();
    M8_SET( M8_SH_WRT_MASK, 0x00FF );
    M8_SET( M8_SH_RD_MASK, 0x00FF );
    m8_put( M8_SH_PIX_CNTL, M8_MF_PIX_CNTL );
    m8_scissors( M8_EXT_SCISSOR_MIN, M8_EXT_SCISSOR_MIN, M8_EXT_SCISSOR_MAX, M8_EXT_SCISSOR_MAX );
    m8_put( M8_SH_DP_CONFIG, M8_DP_MONO_PATT );
    M8_SET( M8_SH_FRGD_COLOR, fg );
    M8_SET( M8_SH_BKGD_COLOR, bg );
    /* The ALU registers are the mixes' function half: the data path. */
    m8_sh_ok[M8_SH_FRGD_MIX] = 0;
    m8_sh_ok[M8_SH_BKGD_MIX] = 0;
    m8_room_for( 3 );
    outpw( M8_ALU_FG_FN, fg_fn );
    outpw( M8_ALU_BG_FN, bg_fn );
    outpw( M8_PATT_LENGTH, 7 );
    m8_room -= 3;
    m8_scan_y = -1;
    m8_scan_byte = 0xFFFF;
}

/* A rectangle of the pattern set up by m8_hw_pattern_begin; nothing else may
 * touch the engine in between. */
void m8_hw_pattern_span( int x, int y, int w, int h, const BYTE *pat )
{
    int     row;

    if( m8_hw_off || w <= 0 || h <= 0 ) {
        return;
    }
    if( h == 1 ) {
        m8_room_for( 6 );
        if( y != m8_scan_y ) {
            outpw( M8_CUR_Y, y );
            --m8_room;
            m8_scan_y = y;
        }
        if( pat[y & 7] != m8_scan_byte ) {
            outpw( M8_PATT_DATA_INDEX, 0x10 );
            outpw( M8_PATT_DATA, pat[y & 7] );
            m8_room -= 2;
            m8_scan_byte = pat[y & 7];
        }
        outpw( M8_CUR_X, x );
        outpw( M8_PATT_INDEX, x & 7 );
        outpw( M8_SCAN_TO_X, x + w );
        m8_room -= 3;
        ++m8_stats[M8_STAT_FILL_HW];
        return;
    }
    m8_scan_y = -1;
    m8_scan_byte = 0xFFFF;
    m8_room_for( 5 );
    outpw( M8_PATT_INDEX, x & 7 );
    outpw( M8_CUR_X, x );
    outpw( M8_DEST_X_START, x );
    outpw( M8_DEST_X_END, x + w );
    outpw( M8_CUR_Y, y );
    m8_room -= 5;
    for( row = 0; row < h; ++row ) {
        m8_room_for( 3 );
        outpw( M8_PATT_DATA_INDEX, 0x10 );
        outpw( M8_PATT_DATA, pat[(y + row) & 7] );
        outpw( M8_DEST_Y_END, y + row + 1 );
        m8_room -= 3;
    }
    ++m8_stats[M8_STAT_FILL_HW];
}

void m8_hw_pattern_rows( int x, int y, int w, int h, const BYTE *pat,
                         BYTE fg, BYTE bg, WORD fg_fn, WORD bg_fn )
{
    m8_hw_pattern_begin( fg, bg, fg_fn, bg_fn );
    m8_hw_pattern_span( x, y, w, h, pat );
}

/* Every span of one GDI scanline call, clipped to l,t,r,b (right and bottom
 * exclusive), after m8_hw_fill_begin (pat NULL) or m8_hw_pattern_begin: one
 * loop over GDI's own list, as ATIM8 walks it (seg08:0b96), so a span costs its
 * port writes and little else. poly 0 is OS_SCANLINES: the first point is
 * (count, y), the rest x pairs. poly 1 is OS_POLYSCANLINE: records of x count,
 * top, bottom, the x pairs and the count again. A one-row span goes through
 * SCAN_TO_X; y and byte are the row and pattern byte the engine holds. */
void m8_hw_scan_spans( int poly, WORD count, const short FAR *w,
                       int l, int t, int r, int b, const BYTE *pat )
{
    int                 y = m8_scan_y;
    WORD                byte = m8_scan_byte;
    int                 room = m8_room;
    WORD                recs = poly ? count : 1;
    const short FAR     *xs;
    WORD                n;
    WORD                i;
    int                 y0;
    int                 y1;
    int                 x0;
    int                 x1;

    if( m8_hw_off ) {
        return;
    }
    for( ; recs > 0; --recs ) {
        if( poly ) {
            n = (WORD)w[0];
            y0 = w[1];
            y1 = w[2];
            xs = w + 3;
        } else {
            n = (WORD)((count - 1) * 2);
            y0 = w[1];
            y1 = y0 + 1;
            xs = w + 2;
        }
        if( y0 < t ) {
            y0 = t;
        }
        if( y1 > b ) {
            y1 = b;
        }
        for( i = 0; y0 < y1 && i + 1 < n; i += 2 ) {
            x0 = xs[i] < l ? l : xs[i];
            x1 = xs[i + 1] > r ? r : xs[i + 1];
            if( x0 >= x1 ) {
                continue;
            }
            ++m8_stats[M8_STAT_FILL_HW];
            if( y1 - y0 != 1 ) {
                m8_room = room;
                if( pat != NULL ) {
                    m8_hw_pattern_span( x0, y0, x1 - x0, y1 - y0, pat );
                } else {
                    m8_hw_fill_span( x0, y0, x1 - x0, y1 - y0 );
                }
                room = m8_room;
                y = -1;
                byte = 0xFFFF;
                continue;
            }
            if( room < 6 ) {
                m8_wait_fifo_empty();
                room = m8_room;
            }
            if( y0 != y ) {
                y = y0;
                outpw( M8_CUR_Y, y );
                --room;
            }
            outpw( M8_CUR_X, x0 );
            if( pat != NULL ) {
                if( pat[y & 7] != byte ) {
                    byte = pat[y & 7];
                    outpw( M8_PATT_DATA_INDEX, 0x10 );
                    outpw( M8_PATT_DATA, byte );
                    room -= 2;
                }
                outpw( M8_PATT_INDEX, x0 & 7 );
                --room;
            }
            outpw( M8_SCAN_TO_X, x1 );
            room -= 2;
        }
        w += n + 4;
    }
    m8_room = room;
    m8_scan_y = y;
    m8_scan_byte = byte;
}

/* ScanLR on the card, as ATIM8 does it (seg09:0109): a SCAN_TO_X read from x
 * towards the row's end, compared as it arrives, so a scan reads only up to
 * the pixel that stops it. DP_CONFIG 5210h reads two pixels a word, the first
 * in scan order in the low byte (tools/m8probe/M8SCANR.C). Style bit 1 scans
 * left: CUR_X is x + 1 and the first pixel read is x. Bit 0 clear stops at the
 * first pixel that is not colour, set at the first that is. The answer is that
 * pixel's x (seg07:01ad counts from x either way); FFFFh if nothing stops it.
 * A transfer left unread is aborted as ATIM8 aborts it, and the set-up is
 * rewritten. */
static int m8_read_scan( int from, int y, int to )
{
    WORD n;

    m8_wait_fifo_empty();
    m8_hw_wait_idle();
    M8_SET( M8_SH_WRT_MASK, 0x00FF );
    M8_SET( M8_SH_RD_MASK, 0x00FF );
    m8_put( M8_SH_PIX_CNTL, M8_MF_PIX_CNTL );
    m8_scissors( M8_EXT_SCISSOR_MIN, M8_EXT_SCISSOR_MIN, M8_EXT_SCISSOR_MAX, M8_EXT_SCISSOR_MAX );
    m8_put( M8_SH_DP_CONFIG, M8_DP_READ_SCAN );
    m8_sh_ok[M8_SH_FRGD_MIX] = 0;
    m8_sh_ok[M8_SH_BKGD_MIX] = 0;
    m8_room_for( 4 );
    outpw( M8_ALU_FG_FN, M8_MIX_REPLACE );
    outpw( M8_CUR_Y, y );
    outpw( M8_CUR_X, from );
    outpw( M8_SCAN_TO_X, to );
    m8_room -= 4;
    for( n = 0; n < M8_WAIT_LIMIT; ++n ) {
        if( inpw( M8_GP_STAT ) & M8_GP_DATA_READY ) {
            return( 1 );
        }
    }
    m8_engine_reset();
    return( 0 );
}

/* One pixel read as ATIM8's Pixel reads it (seg07:00aa): a one-pixel
 * SCAN_TO_X read, the pixel in the low byte of the word. */
BYTE m8_hw_getpixel( int x, int y )
{
    if( m8_hw_off || !m8_read_scan( x, y, x + 1 ) ) {
        return( 0 );
    }
    ++m8_stats[M8_STAT_READBACK];
    return( (BYTE)inpw( M8_PIX_TRANS ) );
}

WORD m8_hw_scanlr( int x, int y, BYTE colour, WORD style, int width )
{
    int     left = (style & 2) != 0;
    int     want = (style & 1) != 0;
    int     step = left ? -1 : 1;
    int     pos = x;
    int     count = left ? x + 1 : width - x;
    WORD    v;
    BYTE    p;
    int     half;

    if( m8_hw_off || count <= 0 || !m8_read_scan( left ? x + 1 : x, y, left ? 0 : width ) ) {
        return( 0xFFFF );
    }
    while( count > 0 ) {
        v = inpw( M8_PIX_TRANS );
        for( half = 0; half < 2 && count > 0; ++half ) {
            p = (BYTE)(half ? v >> 8 : v);
            if( (p == colour) == want ) {
                if( half == 0 ? count > 2 : count > 1 ) {
                    m8_engine_reset();
                }
                ++m8_stats[M8_STAT_READBACK];
                return( (WORD)pos );
            }
            pos += step;
            --count;
        }
    }
    ++m8_stats[M8_STAT_READBACK];
    return( 0xFFFF );
}

/* Solid rectangles one after another: the set-up once, then five writes each.
 * Nothing else may touch the engine in between. */
void m8_hw_fill_begin( BYTE color, WORD mix )
{
    if( m8_hw_off ) {
        return;
    }
    m8_wait_fifo_empty();
    m8_plain();
    m8_put( M8_SH_FRGD_MIX, mix );
    M8_SET( M8_SH_FRGD_COLOR, color );
    m8_scan_y = -1;
}

void m8_hw_fill_span( int x, int y, int w, int h )
{
    if( m8_hw_off || w <= 0 || h <= 0 ) {
        return;
    }
    if( h == 1 ) {
        m8_room_for( 3 );
        if( y != m8_scan_y ) {
            outpw( M8_CUR_Y, y );
            --m8_room;
            m8_scan_y = y;
        }
        outpw( M8_CUR_X, x );
        outpw( M8_SCAN_TO_X, x + w );
        m8_room -= 2;
        ++m8_stats[M8_STAT_FILL_HW];
        return;
    }
    m8_scan_y = -1;
    m8_room_for( 5 );
    outpw( M8_CUR_X, x );
    outpw( M8_CUR_Y, y );
    outpw( M8_MAJ_AXIS_PCNT, w - 1 );
    outpw( M8_MULTIFUNC_CNTL, M8_MF_MIN_AXIS | (h - 1) );
    outpw( M8_CMD, M8_CMD_FILL );
    m8_room -= 5;
    ++m8_stats[M8_STAT_FILL_HW];
}

/* Screen-to-screen copy. Overlapping areas copy from the far end: bottom-up
 * when moving down, right-to-left when moving right on the same rows. */
void m8_hw_copy( int sx, int sy, int dx, int dy, int w, int h )
{
    m8_hw_copy_mix( sx, sy, dx, dy, w, h, M8_MIX_SRC_VRAM | M8_MIX_REPLACE );
}

/* As m8_hw_copy, through any mix of source and destination (a full mix-register
 * value with the VRAM source). */
void m8_hw_copy_mix( int sx, int sy, int dx, int dy, int w, int h, WORD mix )
{
    WORD cmd = M8_CMD_BLIT | M8_CMD_INC_X | M8_CMD_INC_Y;

    if( m8_hw_off ) {
        return;
    }
    if( w <= 0 || h <= 0 ) {
        return;
    }
    if( dy > sy ) {
        cmd &= ~M8_CMD_INC_Y;
        sy += h - 1;
        dy += h - 1;
    } else if( dy == sy && dx > sx ) {
        cmd &= ~M8_CMD_INC_X;
        sx += w - 1;
        dx += w - 1;
    }
    m8_wait_fifo_empty();
    m8_plain();
    m8_put( M8_SH_FRGD_MIX, mix );
    m8_room_for( 7 );
    outpw( M8_CUR_X, sx );
    outpw( M8_CUR_Y, sy );
    outpw( M8_DESTX_DIASTP, dx );
    outpw( M8_DESTY_AXSTP, dy );
    outpw( M8_MAJ_AXIS_PCNT, w - 1 );
    outpw( M8_MULTIFUNC_CNTL, M8_MF_MIN_AXIS | (h - 1) );
    outpw( M8_CMD, cmd );
    ++m8_stats[M8_STAT_COPY_HW];
}

/* Host transfers move two pixels per word, so x and w must be even: the caller
 * aligns the rectangle. Rows are copied to or from pitch-spaced RAM. */
void m8_hw_read_rect( int x, int y, int w, int h, BYTE FAR *dst, WORD pitch )
{
    int     row;
    WORD    words = (WORD)w / 2;
    WORD    n;

    if( m8_hw_off || w <= 0 || h <= 0 ) {
        return;
    }
    m8_wait_fifo_empty();
    m8_hw_wait_idle();
    m8_plain();
    m8_put( M8_SH_FRGD_MIX, M8_MIX_SRC_HOST | M8_MIX_REPLACE );
    m8_room_for( 5 );
    outpw( M8_CUR_X, x );
    outpw( M8_CUR_Y, y );
    outpw( M8_MAJ_AXIS_PCNT, w - 1 );
    outpw( M8_MULTIFUNC_CNTL, M8_MF_MIN_AXIS | (h - 1) );
    outpw( M8_CMD, M8_CMD_HOST_READ );
    /* One wait for the first word, then the whole rectangle unpolled, as
     * ATIM8 reads it: the card keeps pace with the bus once data flows. A read
     * of PIX_TRANS before DATA_READY locks the FIFO (guide 8-14), so without it
     * the read is abandoned and the engine reset. */
    for( n = 0; n < M8_WAIT_LIMIT; ++n ) {
        if( inpw( M8_GP_STAT ) & M8_GP_DATA_READY ) {
            break;
        }
    }
    if( n == M8_WAIT_LIMIT ) {
        m8_engine_reset();
        return;
    }
    for( row = 0; row < h; ++row ) {
        m8_insw( (WORD FAR *)(dst + (DWORD)row * pitch), words );
    }
}

/* Pixel data goes to PIX_TRANS without FIFO checks between words, as M8TIME's
 * raw upload does on the card. */
void m8_hw_write_rect( int x, int y, int w, int h, const BYTE FAR *src, WORD pitch )
{
    int         row;
    WORD        words = (WORD)w / 2;

    if( m8_hw_off || w <= 0 || h <= 0 ) {
        return;
    }
    /* Host transfers start from an idle engine, as ATIM8's do: the set-up
     * rewrites DP_CONFIG, which halts a blit still running (guide 9-41). */
    m8_hw_wait_idle();
    m8_wait_fifo_empty();
    m8_plain();
    m8_put( M8_SH_FRGD_MIX, M8_MIX_SRC_HOST | M8_MIX_REPLACE );
    m8_room_for( 5 );
    outpw( M8_CUR_X, x );
    outpw( M8_CUR_Y, y );
    outpw( M8_MAJ_AXIS_PCNT, w - 1 );
    outpw( M8_MULTIFUNC_CNTL, M8_MF_MIN_AXIS | (h - 1) );
    outpw( M8_CMD, M8_CMD_HOST_WRITE );
    for( row = 0; row < h; ++row ) {
        m8_outsw( (const WORD FAR *)(src + (DWORD)row * pitch), words );
    }
    /* Idle before any colour write: while a command still takes host data,
     * FRGD_COLOR and BKGD_COLOR writes are consumed as pixels (guide 8-14). */
    m8_hw_wait_idle();
    m8_sh_ok[M8_SH_BKGD_COLOR] = 0;
}

/* Mono expansion through the Mach8 extended path (DP_CONFIG 3251: a 1 bit takes
 * the foreground mix, a 0 bit the background mix), as ATIM8 draws text. The
 * bits run on across rows with no padding (M8DPM, card = bed), so the expansion
 * is `width_bytes` x 8 wide and the buffer streams as it is; the extended
 * scissors cut it back to the clip (inclusive edges), and they clip this path
 * (bed, 10-16: no text past window edges). Mixes are full mix-register values. */
static void m8_mono( int x, int y, WORD width_bytes, WORD height, const BYTE FAR *bits,
                     WORD fg_mix, BYTE fg, WORD bg_mix, BYTE bg,
                     int clip_l, int clip_t, int clip_r, int clip_b, WORD wrt_mask )
{
    DWORD       bytes = (DWORD)width_bytes * height;
    WORD        words = (WORD)(bytes / 2);

    /* An empty blit is indeterminate (guide 9-40), and one left waiting for
     * host data wedges the engine. */
    if( width_bytes == 0 || height == 0 ) {
        return;
    }
    m8_hw_wait_idle();
    m8_wait_fifo_empty();
    m8_scissors( clip_l, clip_t, clip_r, clip_b );
    M8_SET( M8_SH_WRT_MASK, wrt_mask );
    M8_SET( M8_SH_RD_MASK, 0x00FF );
    m8_put( M8_SH_PIX_CNTL, M8_MF_PIX_CNTL );
    m8_put( M8_SH_FRGD_MIX, fg_mix );
    M8_SET( M8_SH_FRGD_COLOR, fg );
    m8_put( M8_SH_BKGD_MIX, bg_mix );
    M8_SET( M8_SH_BKGD_COLOR, bg );
    /* Written every time, and after FRGD_MIX: a DP_CONFIG write is what halts
     * and flushes the blit source FIFO (guide p. 9-41), so without it this
     * blit carries on from the last one's data and waits for host data that
     * never comes. Written last, it also gives DP_CONFIG the choice of
     * foreground source over FRGD_MIX. */
    m8_put( M8_SH_DP_CONFIG, M8_DP_MONO_HOST );
    m8_room_for( 5 );
    outpw( M8_CUR_X, x );
    outpw( M8_DEST_X_START, x );
    outpw( M8_DEST_X_END, x + width_bytes * 8 );
    outpw( M8_CUR_Y, y );
    outpw( M8_DEST_Y_END, y + height );
    m8_outsw( (const WORD FAR *)bits, words );
    /* An odd byte count: the last byte alone, never a word read past the end
     * of the caller's buffer. */
    if( bytes & 1 ) {
        outpw( M8_PIX_TRANS, bits[bytes - 1] );
    }
    m8_hw_wait_idle();
    /* PIX_TRANS is BKGD_COLOR under another address: data the engine does not
     * take as pixels lands there (guide 8-14, 9-42). */
    m8_sh_ok[M8_SH_BKGD_COLOR] = 0;
    ++m8_stats[M8_STAT_TEXT_HW];
}

void m8_hw_mono( int x, int y, WORD width_bytes, WORD height, const BYTE FAR *bits,
                 WORD fg_mix, BYTE fg, WORD bg_mix, BYTE bg,
                 int clip_l, int clip_t, int clip_r, int clip_b )
{
    if( m8_hw_off ) {
        return;
    }
    m8_mono( x, y, width_bytes, height, bits, fg_mix, fg, bg_mix, bg,
             clip_l, clip_t, clip_r, clip_b, 0x00FF );
}

/* A polyline through the Mach8's direct linedraw register (guide 9-21, as
 * XFree86's mach8line.c uses it): one write per coordinate and the card runs
 * Bresenham. Index 0 makes the first point a move; after each end point the
 * index returns to 2, so the next pair draws on. LAST_PEL_OFF leaves each
 * segment's end pixel to the next segment, and the last point undrawn, as GDI
 * wants. The extended scissors clip (inclusive edges). The caller keeps the
 * points on the 0-1023 grid. LINEDRAW is an extended register: on a full FIFO
 * the write waits (guide 8-14), but bursts are still counted.
 *
 * Styles 1-4 (dash, dot, dash-dot, dash-dot-dot) are drawn as ATIM8 draws them
 * (seg08:03a5): a 32-bit mono pattern picks foreground or background per pixel,
 * the background left alone unless the mode is opaque. The pattern position
 * runs on from segment to segment, so it is written before each one and moved
 * on by the segment's length. Clip mode 3 is not used: a line partly outside
 * the device would make the engine ignore LINEDRAW writes (guide 9-34). */
static const WORD m8_dash[4][2] = {
    { 0xFCFC, 0xFCFC }, { 0xCCCC, 0xCCCC }, { 0x30FC, 0x30FC }, { 0xCCFC, 0xCCFC }
};

void m8_hw_polyline( const POINT FAR *pts, WORD count, BYTE color, WORD mix,
                     WORD style, int opaque, BYTE bg,
                     int clip_l, int clip_t, int clip_r, int clip_b )
{
    WORD    i;
    int     room;
    WORD    phase = 0;
    int     dx;
    int     dy;

    if( m8_hw_off || count < 2 || style > 4 ) {
        return;
    }
    m8_wait_fifo_empty();
    M8_SET( M8_SH_WRT_MASK, 0x00FF );
    M8_SET( M8_SH_RD_MASK, 0x00FF );
    m8_put( M8_SH_PIX_CNTL, M8_MF_PIX_CNTL );
    m8_scissors( clip_l, clip_t, clip_r, clip_b );
    if( style == 0 ) {
        m8_put( M8_SH_DP_CONFIG, M8_DP_8514 );
        m8_put( M8_SH_FRGD_MIX, M8_MIX_SRC_FRGD | mix );
        M8_SET( M8_SH_FRGD_COLOR, color );
    } else {
        m8_put( M8_SH_DP_CONFIG, M8_DP_MONO_PATT );
        M8_SET( M8_SH_FRGD_COLOR, color );
        if( opaque ) {
            M8_SET( M8_SH_BKGD_COLOR, bg );
        }
        /* The ALU registers are the mixes' function half: the data path. */
        m8_sh_ok[M8_SH_FRGD_MIX] = 0;
        m8_sh_ok[M8_SH_BKGD_MIX] = 0;
        m8_room_for( 7 );
        outpw( M8_ALU_FG_FN, mix );
        outpw( M8_ALU_BG_FN, opaque ? mix : M8_MIX_LEAVE );
        outpw( M8_PATT_LENGTH, 31 );
        outpw( M8_PATT_DATA_INDEX, 0x10 );
        outpw( M8_PATT_DATA, m8_dash[style - 1][0] );
        outpw( M8_PATT_DATA, m8_dash[style - 1][1] );
        outpw( M8_PATT_INDEX, 0 );
        m8_room -= 7;
    }
    /* Not cached: a CMD write sets LINEDRAW_OPT's line bits from its own. */
    m8_room_for( 2 );
    outpw( M8_LINEDRAW_OPT, M8_LINEDRAW_LAST_OFF );
    outpw( M8_LINEDRAW_INDEX, 0 );
    room = m8_room - 2;
    for( i = 0; i < count; ++i ) {
        if( room < 3 ) {
            m8_wait_fifo_empty();
            room = m8_room;
        }
        if( style != 0 && i > 0 ) {
            outpw( M8_PATT_INDEX, phase );
            --room;
        }
        outpw( M8_LINEDRAW, pts[i].xcoord );
        outpw( M8_LINEDRAW, pts[i].ycoord );
        room -= 2;
        if( style != 0 && i > 0 ) {
            dx = pts[i].xcoord - pts[i - 1].xcoord;
            dy = pts[i].ycoord - pts[i - 1].ycoord;
            if( dx < 0 ) {
                dx = -dx;
            }
            if( dy < 0 ) {
                dy = -dy;
            }
            phase = (phase + (dx > dy ? dx : dy)) & 31;
        }
    }
    /* No idle wait: nothing after a line reads its pixels before the next
     * routine's own wait, and commands run in FIFO order. */
    ++m8_stats[M8_STAT_LINE_HW];
}

/* Write 1-bit data into one bit-plane of video memory (the string cache): the
 * mono expansion with WRT_MASK limited to that plane, 1 bits as FFh and 0 bits
 * as 00h, so only the plane's bit changes. */
void m8_hw_mono_plane( int x, int y, WORD width_bytes, WORD height, const BYTE FAR *bits,
                       int plane )
{
    if( m8_hw_off ) {
        return;
    }
    m8_mono( x, y, width_bytes, height, bits,
             M8_MIX_SRC_FRGD | M8_MIX_REPLACE, 0xFF,
             M8_MIX_SRC_BKGD | M8_MIX_REPLACE, 0x00,
             x, y, x + width_bytes * 8 - 1, y + height - 1, 1 << plane );
}

/* Draw 1-bit data from a bit-plane of video memory: the plane-select blit
 * (M8TIME test A on the card, 51 us a glyph). PIX_CNTL lets the plane that
 * RD_MASK selects pick the foreground or background mix; RD_MASK is the plane's
 * bit rotated left by one (plane 5 -> 40h, measured). */
void m8_hw_plane_blit( int sx, int sy, int dx, int dy, int w, int h, int plane,
                       BYTE fg, BYTE bg, int transparent )
{
    if( m8_hw_off || w <= 0 || h <= 0 ) {
        return;
    }
    m8_wait_fifo_empty();
    M8_SET( M8_SH_WRT_MASK, 0x00FF );
    m8_put( M8_SH_DP_CONFIG, M8_DP_8514 );
    m8_scissors( M8_EXT_SCISSOR_MIN, M8_EXT_SCISSOR_MIN, M8_EXT_SCISSOR_MAX, M8_EXT_SCISSOR_MAX );
    m8_put( M8_SH_PIX_CNTL, M8_MF_PIX_CNTL | 0x00C0 );
    M8_SET( M8_SH_RD_MASK, (1 << ((plane + 1) & 7)) & 0xFF );
    m8_put( M8_SH_FRGD_MIX, M8_MIX_SRC_FRGD | M8_MIX_REPLACE );
    M8_SET( M8_SH_FRGD_COLOR, fg );
    if( transparent ) {
        m8_put( M8_SH_BKGD_MIX, M8_MIX_LEAVE );
    } else {
        M8_SET( M8_SH_BKGD_COLOR, bg );
        m8_put( M8_SH_BKGD_MIX, M8_MIX_SRC_BKGD | M8_MIX_REPLACE );
    }
    m8_room_for( 7 );
    outpw( M8_CUR_X, sx );
    outpw( M8_CUR_Y, sy );
    outpw( M8_DESTX_DIASTP, dx );
    outpw( M8_DESTY_AXSTP, dy );
    outpw( M8_MAJ_AXIS_PCNT, w - 1 );
    outpw( M8_MULTIFUNC_CNTL, M8_MF_MIN_AXIS | (h - 1) );
    outpw( M8_CMD, M8_CMD_PLANE_BLIT );
    ++m8_stats[M8_STAT_TEXT_HW];
}

/* Off-screen to screen, leaving the screen where the source is `key`: the
 * Graphics Ultra's source compare (92EEh bits 3-5; EAEEh the colour) sends a
 * pixel it holds true for through the background mix, here leave. 28h is
 * "source equal to the colour" (fork model 8283ed2bc, matched to the card).
 * The writes and their order are TEST.COM's extended blit (tools/m8seq/
 * M8SCMP.ASM). An engine reset does not clear 92EEh: it goes back to 0 before
 * anything else draws. */
void m8_hw_copy_keyed( int sx, int sy, int dx, int dy, int w, int h, BYTE key )
{
    if( m8_hw_off || w <= 0 || h <= 0 ) {
        return;
    }
    m8_wait_fifo_empty();
    M8_SET( M8_SH_WRT_MASK, 0x00FF );
    M8_SET( M8_SH_RD_MASK, 0x00FF );
    m8_put( M8_SH_PIX_CNTL, M8_MF_PIX_CNTL );
    m8_scissors( M8_EXT_SCISSOR_MIN, M8_EXT_SCISSOR_MIN, M8_EXT_SCISSOR_MAX, M8_EXT_SCISSOR_MAX );
    m8_put( M8_SH_DP_CONFIG, M8_DP_VRAM_BLIT );
    /* The ALU registers are the mixes' function half: the data path. */
    m8_sh_ok[M8_SH_FRGD_MIX] = 0;
    m8_sh_ok[M8_SH_BKGD_MIX] = 0;
    m8_room_for( 15 );
    outpw( M8_ALU_FG_FN, M8_MIX_REPLACE );
    outpw( M8_ALU_BG_FN, M8_MIX_LEAVE );
    outpw( M8_SRC_CMP_COLOR, key );
    outpw( M8_SRC_CMP_FN, 0x0028 );
    outpw( M8_SRC_Y_DIR, 1 );
    outpw( M8_CUR_X, dx );
    outpw( M8_CUR_Y, dy );
    outpw( M8_DEST_X_START, dx );
    outpw( M8_DEST_X_END, dx + w );
    outpw( M8_DESTX_DIASTP, sx );
    outpw( M8_DESTY_AXSTP, sy );
    outpw( M8_SRC_X_START, sx );
    outpw( M8_SRC_X_END, sx + w );
    /* 92EEh is an extended register: its 0 queues behind the blit. */
    outpw( M8_DEST_Y_END, dy + h );
    outpw( M8_SRC_CMP_FN, 0 );
    m8_room -= 15;
}

/* As m8_hw_plane_blit, transparent, with any foreground mix: where the plane
 * holds 1, fg through fg_mix; where 0, the screen is left. */
void m8_hw_plane_mix( int sx, int sy, int dx, int dy, int w, int h, int plane,
                      BYTE fg, WORD fg_mix )
{
    if( m8_hw_off || w <= 0 || h <= 0 ) {
        return;
    }
    m8_wait_fifo_empty();
    M8_SET( M8_SH_WRT_MASK, 0x00FF );
    m8_put( M8_SH_DP_CONFIG, M8_DP_8514 );
    m8_scissors( M8_EXT_SCISSOR_MIN, M8_EXT_SCISSOR_MIN, M8_EXT_SCISSOR_MAX, M8_EXT_SCISSOR_MAX );
    m8_put( M8_SH_PIX_CNTL, M8_MF_PIX_CNTL | 0x00C0 );
    M8_SET( M8_SH_RD_MASK, (1 << ((plane + 1) & 7)) & 0xFF );
    m8_put( M8_SH_FRGD_MIX, fg_mix );
    M8_SET( M8_SH_FRGD_COLOR, fg );
    m8_put( M8_SH_BKGD_MIX, M8_MIX_LEAVE );
    m8_room_for( 7 );
    outpw( M8_CUR_X, sx );
    outpw( M8_CUR_Y, sy );
    outpw( M8_DESTX_DIASTP, dx );
    outpw( M8_DESTY_AXSTP, dy );
    outpw( M8_MAJ_AXIS_PCNT, w - 1 );
    outpw( M8_MULTIFUNC_CNTL, M8_MF_MIN_AXIS | (h - 1) );
    outpw( M8_CMD, M8_CMD_PLANE_BLIT );
    m8_hw_wait_idle();
}

/* A value the tap logs between operations, so a capture splits per GDI call. */
void m8_hw_marker( WORD value )
{
    outpw( M8_SCRATCH_PAD_1, value );
}
