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

/* Every engine call starts from an empty FIFO, so up to M8_FIFO_DEPTH writes
 * follow without another check: a write to a full FIFO is dropped, not held. */
static int m8_wait_fifo_empty( void )
{
    WORD n;

    m8_room = M8_FIFO_DEPTH;
    for( n = 0; n < M8_WAIT_LIMIT; ++n ) {
        if( inpw( M8_EXT_FIFO_STATUS ) == 0 ) {
            return( 1 );
        }
    }
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
 * set. Coordinates, MIN_AXIS and CMD go every time. The cache is emptied when
 * anything but these routines may have set the card: mode set, VGA through. */
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

/* Whichever of DP_CONFIG, FRGD_MIX and CMD was written last chooses the
 * foreground source of an extended operation (card, as 86Box models it), so the
 * extended paths write DP_CONFIG again once either of the others has gone. */
static int m8_dp_last;

static void m8_sh_clear( void )
{
    int i;

    for( i = 0; i < M8_SH_COUNT; ++i ) {
        m8_sh_ok[i] = 0;
    }
}

static void m8_put( int r, WORD v )
{
    m8_room_for( 1 );
    outpw( m8_sh_port[r], v );
    --m8_room;
    m8_sh_val[r] = v;
    m8_sh_ok[r] = 1;
    if( r == M8_SH_DP_CONFIG ) {
        m8_dp_last = 1;
    } else if( r == M8_SH_FRGD_MIX ) {
        m8_dp_last = 0;
    }
}

/* The unchanged case is inline: it runs a dozen times per operation, and as a
 * call it cost more than the writes it saved. */
#define M8_SET( r, v )     do {         WORD m8_v_ = (v);         if( !m8_sh_ok[r] || m8_sh_val[r] != m8_v_ ) {             m8_put( r, m8_v_ );         }     } while( 0 )

static void m8_cmd( WORD cmd )
{
    outpw( M8_CMD, cmd );
    m8_dp_last = 0;
}

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
    M8_SET( M8_SH_DP_CONFIG, M8_DP_8514 );
    M8_SET( M8_SH_PIX_CNTL, M8_MF_PIX_CNTL );
    m8_scissors( M8_EXT_SCISSOR_MIN, M8_EXT_SCISSOR_MIN, M8_EXT_SCISSOR_MAX, M8_EXT_SCISSOR_MAX );
}

int m8_hw_wait_idle( void )
{
    WORD n;

    for( n = 0; n < M8_WAIT_LIMIT; ++n ) {
        if( !(inpw( M8_GP_STAT ) & M8_GP_BUSY) ) {
            return( 1 );
        }
    }
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
    M8_SET( M8_SH_FRGD_MIX, mix );
    M8_SET( M8_SH_FRGD_COLOR, color );
    m8_room_for( 5 );
    outpw( M8_CUR_X, x );
    outpw( M8_CUR_Y, y );
    outpw( M8_MAJ_AXIS_PCNT, w - 1 );
    outpw( M8_MULTIFUNC_CNTL, M8_MF_MIN_AXIS | (h - 1) );
    m8_cmd( M8_CMD_FILL );
    ++m8_stats[M8_STAT_FILL_HW];
}

void m8_hw_fill( int x, int y, int w, int h, BYTE color )
{
    m8_hw_fill_mix( x, y, w, h, color, M8_MIX_SRC_FRGD | M8_MIX_REPLACE );
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
    M8_SET( M8_SH_FRGD_MIX, mix );
    m8_room_for( 7 );
    outpw( M8_CUR_X, sx );
    outpw( M8_CUR_Y, sy );
    outpw( M8_DESTX_DIASTP, dx );
    outpw( M8_DESTY_AXSTP, dy );
    outpw( M8_MAJ_AXIS_PCNT, w - 1 );
    outpw( M8_MULTIFUNC_CNTL, M8_MF_MIN_AXIS | (h - 1) );
    m8_cmd( cmd );
    ++m8_stats[M8_STAT_COPY_HW];
}

/* Host transfers move two pixels per word, so x and w must be even: the caller
 * aligns the rectangle. Rows are copied to or from pitch-spaced RAM. */
void m8_hw_read_rect( int x, int y, int w, int h, BYTE FAR *dst, WORD pitch )
{
    int     row;
    WORD    words = (WORD)w / 2;
    WORD    n;

    if( m8_hw_off ) {
        return;
    }
    m8_wait_fifo_empty();
    m8_hw_wait_idle();
    m8_plain();
    M8_SET( M8_SH_FRGD_MIX, M8_MIX_SRC_HOST | M8_MIX_REPLACE );
    m8_room_for( 5 );
    outpw( M8_CUR_X, x );
    outpw( M8_CUR_Y, y );
    outpw( M8_MAJ_AXIS_PCNT, w - 1 );
    outpw( M8_MULTIFUNC_CNTL, M8_MF_MIN_AXIS | (h - 1) );
    m8_cmd( M8_CMD_HOST_READ );
    /* One wait for the first word, then the whole rectangle unpolled, as
     * ATIM8 reads it: the card keeps pace with the bus once data flows. */
    for( n = 0; n < M8_WAIT_LIMIT; ++n ) {
        if( inpw( M8_GP_STAT ) & M8_GP_DATA_READY ) {
            break;
        }
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

    if( m8_hw_off ) {
        return;
    }
    m8_wait_fifo_empty();
    m8_plain();
    M8_SET( M8_SH_FRGD_MIX, M8_MIX_SRC_HOST | M8_MIX_REPLACE );
    m8_room_for( 5 );
    outpw( M8_CUR_X, x );
    outpw( M8_CUR_Y, y );
    outpw( M8_MAJ_AXIS_PCNT, w - 1 );
    outpw( M8_MULTIFUNC_CNTL, M8_MF_MIN_AXIS | (h - 1) );
    m8_cmd( M8_CMD_HOST_WRITE );
    for( row = 0; row < h; ++row ) {
        m8_outsw( (const WORD FAR *)(src + (DWORD)row * pitch), words );
    }
    m8_hw_wait_idle();
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

    m8_wait_fifo_empty();
    m8_scissors( clip_l, clip_t, clip_r, clip_b );
    M8_SET( M8_SH_WRT_MASK, wrt_mask );
    M8_SET( M8_SH_RD_MASK, 0x00FF );
    M8_SET( M8_SH_PIX_CNTL, M8_MF_PIX_CNTL );
    M8_SET( M8_SH_FRGD_MIX, fg_mix );
    M8_SET( M8_SH_FRGD_COLOR, fg );
    M8_SET( M8_SH_BKGD_MIX, bg_mix );
    M8_SET( M8_SH_BKGD_COLOR, bg );
    if( !m8_dp_last ) {
        m8_sh_ok[M8_SH_DP_CONFIG] = 0;
    }
    M8_SET( M8_SH_DP_CONFIG, M8_DP_MONO_HOST );
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
 * points on the 0-1023 grid. A full FIFO drops writes, so it is drained every
 * M8_FIFO_DEPTH writes. */
void m8_hw_polyline( const POINT FAR *pts, WORD count, BYTE color, WORD mix,
                     int clip_l, int clip_t, int clip_r, int clip_b )
{
    WORD    i;
    int     room;

    if( m8_hw_off || count < 2 ) {
        return;
    }
    m8_wait_fifo_empty();
    M8_SET( M8_SH_WRT_MASK, 0x00FF );
    M8_SET( M8_SH_RD_MASK, 0x00FF );
    M8_SET( M8_SH_DP_CONFIG, M8_DP_8514 );
    M8_SET( M8_SH_PIX_CNTL, M8_MF_PIX_CNTL );
    m8_scissors( clip_l, clip_t, clip_r, clip_b );
    M8_SET( M8_SH_FRGD_MIX, M8_MIX_SRC_FRGD | mix );
    M8_SET( M8_SH_FRGD_COLOR, color );
    /* Not cached: a CMD write sets LINEDRAW_OPT's line bits from its own. */
    m8_room_for( 2 );
    outpw( M8_LINEDRAW_OPT, M8_LINEDRAW_LAST_OFF );
    outpw( M8_LINEDRAW_INDEX, 0 );
    room = m8_room - 2;
    for( i = 0; i < count; ++i ) {
        if( room < 2 ) {
            m8_wait_fifo_empty();
            room = M8_FIFO_DEPTH;
        }
        outpw( M8_LINEDRAW, pts[i].xcoord );
        outpw( M8_LINEDRAW, pts[i].ycoord );
        room -= 2;
    }
    m8_hw_wait_idle();
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
    M8_SET( M8_SH_DP_CONFIG, M8_DP_8514 );
    m8_scissors( M8_EXT_SCISSOR_MIN, M8_EXT_SCISSOR_MIN, M8_EXT_SCISSOR_MAX, M8_EXT_SCISSOR_MAX );
    M8_SET( M8_SH_PIX_CNTL, M8_MF_PIX_CNTL | 0x00C0 );
    M8_SET( M8_SH_RD_MASK, (1 << ((plane + 1) & 7)) & 0xFF );
    M8_SET( M8_SH_FRGD_MIX, M8_MIX_SRC_FRGD | M8_MIX_REPLACE );
    M8_SET( M8_SH_FRGD_COLOR, fg );
    if( transparent ) {
        M8_SET( M8_SH_BKGD_MIX, M8_MIX_LEAVE );
    } else {
        M8_SET( M8_SH_BKGD_COLOR, bg );
        M8_SET( M8_SH_BKGD_MIX, M8_MIX_SRC_BKGD | M8_MIX_REPLACE );
    }
    m8_room_for( 7 );
    outpw( M8_CUR_X, sx );
    outpw( M8_CUR_Y, sy );
    outpw( M8_DESTX_DIASTP, dx );
    outpw( M8_DESTY_AXSTP, dy );
    outpw( M8_MAJ_AXIS_PCNT, w - 1 );
    outpw( M8_MULTIFUNC_CNTL, M8_MF_MIN_AXIS | (h - 1) );
    m8_cmd( M8_CMD_PLANE_BLIT );
    m8_hw_wait_idle();
    ++m8_stats[M8_STAT_TEXT_HW];
}

/* A value the tap logs between operations, so a capture splits per GDI call. */
void m8_hw_marker( WORD value )
{
    outpw( M8_SCRATCH_PAD_1, value );
}
