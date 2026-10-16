/* Mach8 hardware access: mode set, palette, and the engine primitives the
 * driver builds on.
 *
 * Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
 */

#include "winhack.h"
#include <conio.h>
#include <gdidefs.h>
#include "m8.h"

#define M8_WAIT_LIMIT       60000u      /* polls; ~0.4 s of GP_STAT reads on the 5160 */

/* One register write: port, value, and whether the port takes a byte. */
typedef struct {
    WORD    port;
    WORD    value;
    WORD    is_byte;
} M8_REG;

/* 800 x 600 at pitch 1024, as ATIM8 sets it on this card. Shadow set 2 is
 * loaded with the 800 x 600 timings and ADVFUNC_CNTL bit 2 selects it. The
 * values are the monitor timings ATIM8 programs for 800 x 600; the EEPROM has
 * no Mach8 record for this mode (docs/mach8_levers.md).
 * Source: docs/captures/2026-10-16_atim8_tap/atim8_800x600_modeset.txt. */
static const M8_REG m8_mode_800x600[] = {
    { 0x22E8, 0x0060, 0 },      /* DISP_CNTL: display off */
    { 0x4AE8, 0x0002, 0 },      /* ADVFUNC_CNTL: VGA passes through while the set loads */
    { 0x7AEE, 0x000A, 1 },      /* EXT_GE_CONFIG */
    { 0x22E8, 0x0020, 0 },
    { 0x5AEE, 0x0002, 0 },      /* SHADOW_SET: set 2 */
    { 0x46EE, 0x0000, 0 },      /* SHADOW_CTL: unlocked */
    { 0x02E8, 0x0082, 1 },      /* H_TOTAL */
    { 0x06E8, 0x0063, 1 },      /* H_DISP: 800 */
    { 0x0AE8, 0x0066, 1 },      /* H_SYNC_STRT */
    { 0x0EE8, 0x002E, 1 },      /* H_SYNC_WID */
    { 0x12E8, 0x0535, 0 },      /* V_TOTAL */
    { 0x16E8, 0x04AB, 0 },      /* V_DISP: 600 */
    { 0x1AE8, 0x04C9, 0 },      /* V_SYNC_STRT */
    { 0x1EE8, 0x0002, 1 },      /* V_SYNC_WID */
    { 0x22E8, 0x0023, 1 },      /* DISP_CNTL */
    { 0x4AEE, 0x1E10, 0 },      /* CLOCK_SEL */
    { 0x46EE, 0x003F, 0 },      /* SHADOW_CTL: locked */
    { 0x5AEE, 0x0000, 0 },
    { 0x46EE, 0x0000, 0 },
    { 0x22E8, 0x0060, 0 },
    { 0x4AE8, 0x0007, 0 },      /* ADVFUNC_CNTL: 8514 output, shadow set 2 */
    { 0x7AEE, 0x000A, 1 },
    { 0x22E8, 0x0020, 0 },      /* display on */
    { 0x76EE, 0x0080, 0 },      /* GE_PITCH: 1024 / 8 */
    { 0x6EEE, 0x0000, 0 },      /* GE_OFFSET */
    { 0x72EE, 0x0000, 0 },
    { 0xB2E8, 0x0000, 0 },      /* BKGD_COLOR */
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

/* Every engine call starts from an empty FIFO, so up to M8_FIFO_DEPTH writes
 * follow without another check: a write to a full FIFO is dropped, not held. */
static int m8_wait_fifo_empty( void )
{
    WORD n;

    for( n = 0; n < M8_WAIT_LIMIT; ++n ) {
        if( inpw( M8_EXT_FIFO_STATUS ) == 0 ) {
            return( 1 );
        }
    }
    return( 0 );
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

int m8_hw_set_mode( void )
{
    m8_write_regs( m8_mode_800x600, sizeof( m8_mode_800x600 ) / sizeof( m8_mode_800x600[0] ) );
    m8_hw_fill( 0, 0, M8_SCREEN_X, M8_SCREEN_Y, 0 );
    return( m8_hw_wait_idle() );
}

/* Hand the monitor back to the VGA half (DOS, Disable). */
void m8_hw_passthrough( void )
{
    m8_hw_wait_idle();
    outpw( M8_ADVFUNC_CNTL, 0x0002 );
}

/* The 8514 DAC ports; the RAMDAC is an IMSG176, 6 bits per gun. */
void m8_hw_set_palette( UINT start, UINT count, RGBQUAD FAR *pal )
{
    UINT i;

    outp( 0x02EA, 0xFF );
    outp( 0x02EC, (BYTE)start );
    for( i = 0; i < count; ++i ) {
        outp( 0x02ED, pal[i].rgbRed >> 2 );
        outp( 0x02ED, pal[i].rgbGreen >> 2 );
        outp( 0x02ED, pal[i].rgbBlue >> 2 );
    }
}

void m8_hw_fill( int x, int y, int w, int h, BYTE color )
{
    if( w <= 0 || h <= 0 ) {
        return;
    }
    m8_wait_fifo_empty();
    outpw( M8_FRGD_MIX, M8_MIX_SRC_FRGD | M8_MIX_REPLACE );
    outpw( M8_FRGD_COLOR, color );
    outpw( M8_CUR_X, x );
    outpw( M8_CUR_Y, y );
    outpw( M8_MAJ_AXIS_PCNT, w - 1 );
    outpw( M8_MULTIFUNC_CNTL, M8_MF_MIN_AXIS | (h - 1) );
    outpw( M8_CMD, M8_CMD_FILL );
    ++m8_stats[M8_STAT_FILL_HW];
}

/* Screen-to-screen copy. Overlapping areas copy from the far end: bottom-up
 * when moving down, right-to-left when moving right on the same rows. */
void m8_hw_copy( int sx, int sy, int dx, int dy, int w, int h )
{
    WORD cmd = M8_CMD_BLIT | M8_CMD_INC_X | M8_CMD_INC_Y;

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
    outpw( M8_FRGD_MIX, M8_MIX_SRC_VRAM | M8_MIX_REPLACE );
    outpw( M8_CUR_X, sx );
    outpw( M8_CUR_Y, sy );
    outpw( M8_DESTX_DIASTP, dx );
    outpw( M8_DESTY_AXSTP, dy );
    outpw( M8_MAJ_AXIS_PCNT, w - 1 );
    outpw( M8_MULTIFUNC_CNTL, M8_MF_MIN_AXIS | (h - 1) );
    outpw( M8_CMD, cmd );
    outpw( M8_FRGD_MIX, M8_MIX_SRC_FRGD | M8_MIX_REPLACE );
    ++m8_stats[M8_STAT_COPY_HW];
}

/* Host transfers move two pixels per word, so x and w must be even: the caller
 * aligns the rectangle. Rows are copied to or from pitch-spaced RAM. */
void m8_hw_read_rect( int x, int y, int w, int h, BYTE FAR *dst, WORD pitch )
{
    int     row;
    int     col;
    WORD    words = (WORD)w / 2;
    WORD    FAR *p;
    WORD    n;

    m8_wait_fifo_empty();
    m8_hw_wait_idle();
    outpw( M8_FRGD_MIX, M8_MIX_SRC_HOST | M8_MIX_REPLACE );
    outpw( M8_CUR_X, x );
    outpw( M8_CUR_Y, y );
    outpw( M8_MAJ_AXIS_PCNT, w - 1 );
    outpw( M8_MULTIFUNC_CNTL, M8_MF_MIN_AXIS | (h - 1) );
    outpw( M8_CMD, M8_CMD_HOST_READ );
    for( row = 0; row < h; ++row ) {
        p = (WORD FAR *)(dst + (DWORD)row * pitch);
        for( col = 0; col < (int)words; ++col ) {
            for( n = 0; n < M8_WAIT_LIMIT; ++n ) {
                if( inpw( M8_GP_STAT ) & M8_GP_DATA_READY ) {
                    break;
                }
            }
            p[col] = inpw( M8_PIX_TRANS );
        }
    }
    outpw( M8_FRGD_MIX, M8_MIX_SRC_FRGD | M8_MIX_REPLACE );
}

/* Pixel data goes to PIX_TRANS without FIFO checks between words, as M8TIME's
 * raw upload does on the card. */
void m8_hw_write_rect( int x, int y, int w, int h, const BYTE FAR *src, WORD pitch )
{
    int         row;
    int         col;
    WORD        words = (WORD)w / 2;
    const WORD  FAR *p;

    m8_wait_fifo_empty();
    outpw( M8_FRGD_MIX, M8_MIX_SRC_HOST | M8_MIX_REPLACE );
    outpw( M8_CUR_X, x );
    outpw( M8_CUR_Y, y );
    outpw( M8_MAJ_AXIS_PCNT, w - 1 );
    outpw( M8_MULTIFUNC_CNTL, M8_MF_MIN_AXIS | (h - 1) );
    outpw( M8_CMD, M8_CMD_HOST_WRITE );
    for( row = 0; row < h; ++row ) {
        p = (const WORD FAR *)(src + (DWORD)row * pitch);
        for( col = 0; col < (int)words; ++col ) {
            outpw( M8_PIX_TRANS, p[col] );
        }
    }
    m8_hw_wait_idle();
    outpw( M8_FRGD_MIX, M8_MIX_SRC_FRGD | M8_MIX_REPLACE );
}

/* A value the tap logs between operations, so a capture splits per GDI call. */
void m8_hw_marker( WORD value )
{
    outpw( M8_SCRATCH_PAD_1, value );
}
