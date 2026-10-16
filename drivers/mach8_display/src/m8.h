/* Mach8 display driver for Windows 95 - shared definitions.
 *
 * Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
 * The driver's frame (Enable, DIB Engine glue, resources) follows Michal Necasek's
 * boxv9x as carried in JHRobotics' vmdisp9x (MIT, see LICENSE-MIT.txt).
 */

#ifndef M8_H
#define M8_H

/* Screen layout. The engine addresses 1 MB as 1024 x 1024 at 8 bpp; the visible
 * 800 x 600 sits top left, the rest is off-screen memory for caches. */
#define M8_SCREEN_X         800
#define M8_SCREEN_Y         600
#define M8_PITCH            1024

/* 8514/A registers (IBM 8514/A Technical Reference; Mach32 programmer's guide ch. 8). */
#define M8_DISP_CNTL        0x22E8
#define M8_ADVFUNC_CNTL     0x4AE8
#define M8_SUBSYS_STAT      0x42E8      /* read */
#define M8_CUR_Y            0x82E8
#define M8_CUR_X            0x86E8
#define M8_DESTY_AXSTP      0x8AE8
#define M8_DESTX_DIASTP     0x8EE8
#define M8_MAJ_AXIS_PCNT    0x96E8
#define M8_GP_STAT          0x9AE8      /* read */
#define M8_CMD              0x9AE8      /* write */
#define M8_FRGD_COLOR       0xA6E8
#define M8_WRT_MASK         0xAAE8
#define M8_FRGD_MIX         0xBAE8
#define M8_MULTIFUNC_CNTL   0xBEE8
#define M8_PIX_TRANS        0xE2E8

#define M8_BKGD_COLOR       0xA2E8
#define M8_BKGD_MIX         0xB6E8

/* Mach8 extended registers. */
#define M8_EXT_FIFO_STATUS  0x9AEE      /* read: one bit per occupied FIFO entry */
#define M8_DP_CONFIG        0xCEEE
#define M8_DEST_X_START     0xA6EE
#define M8_DEST_X_END       0xAAEE
#define M8_DEST_Y_END       0xAEEE      /* writing it starts an extended blit */
#define M8_EXT_SCISSOR_L    0xDAEE
#define M8_EXT_SCISSOR_T    0xDEEE
#define M8_EXT_SCISSOR_R    0xE2EE
#define M8_EXT_SCISSOR_B    0xE6EE
#define M8_EXT_SCISSOR_MIN  0x0800      /* -2048 in 12 bits: the values ATIM8 leaves */
#define M8_EXT_SCISSOR_MAX  0x07FF

/* DP_CONFIG: host mono data selects foreground/background (ATIM8's text), and
 * the value the mode set leaves for the 8514/A commands. */
#define M8_DP_MONO_HOST     0x3251
#define M8_DP_8514          0x2211
#define M8_SCRATCH_PAD_1    0x56EE      /* marker writes for the tap (A10) */

/* GP_STAT bits. */
#define M8_GP_DATA_READY    0x0100
#define M8_GP_BUSY          0x0200

/* MULTIFUNC_CNTL indexes, in the top nibble. */
#define M8_MF_MIN_AXIS      0x0000
#define M8_MF_SCISSOR_T     0x1000
#define M8_MF_SCISSOR_L     0x2000
#define M8_MF_SCISSOR_B     0x3000
#define M8_MF_SCISSOR_R     0x4000
#define M8_MF_PIX_CNTL      0xA000

/* Mixes: source in bits 6-5, function in bits 3-0. */
#define M8_MIX_SRC_BKGD     0x0000
#define M8_MIX_SRC_FRGD     0x0020
#define M8_MIX_SRC_HOST     0x0040
#define M8_MIX_SRC_VRAM     0x0060
#define M8_MIX_REPLACE      0x0007
#define M8_MIX_LEAVE        0x0003      /* the result is the destination */
#define M8_MIX_XOR          0x0005

/* Off-screen memory: the strip right of the 800-wide screen. */
#define M8_CURSOR_SAVE_X    800         /* 32 x 32 under the cursor */
#define M8_CURSOR_SAVE_Y    0

/* Commands. Rectangle (010), LSB_FIRST (bit 12: first pixel in the low byte), 16-bit
 * transfers (bit 9), wait for host data (bit 8), x and y increasing, draw. */
#define M8_CMD_FILL         0x40F3      /* rectangle from FRGD_COLOR */
#define M8_CMD_HOST_WRITE   0x53B1      /* rectangle from PIX_TRANS, two pixels per word */
#define M8_CMD_HOST_READ    0x53B0      /* rectangle to PIX_TRANS, two pixels per word */
#define M8_CMD_BLIT         0xC053      /* screen-to-screen, add the increment bits */
#define M8_CMD_INC_X        0x0020
#define M8_CMD_INC_Y        0x0080

/* The FIFO is 16 entries deep and drops writes when full (card, 10-10). */
#define M8_FIFO_DEPTH       16

/* Counters per GDI entry point at the fallback: what to accelerate next. */
#define M8_STAT_BITBLT      0
#define M8_STAT_OUTPUT      1
#define M8_STAT_PIXEL       2
#define M8_STAT_STRBLT      3
#define M8_STAT_EXTTEXTOUT  4
#define M8_STAT_SCANLR      5
#define M8_STAT_STRETCHBLT  6
#define M8_STAT_STRETCHDIB  7
#define M8_STAT_DIBTODEV    8
#define M8_STAT_FASTBORDER  9
#define M8_STAT_UPDATECOL   10
#define M8_STAT_FILL_HW     11
#define M8_STAT_COPY_HW     12
#define M8_STAT_READBACK    13      /* fallback strips that needed the screen read */
#define M8_STAT_TEXT_HW     14      /* mono expansions: text and cursor */
#define M8_STAT_TEXT_FAIL   15      /* engine text declined, drawn by the fallback */
#define M8_STAT_RUN_FILL    16      /* write-back runs sent as fills */
#define M8_STAT_COUNT       17

extern DWORD m8_stats[M8_STAT_COUNT];

/* m8hw.c */
extern int  m8_hw_off;
extern int  m8_hw_set_mode( void );
extern void m8_hw_passthrough( void );
extern void m8_hw_set_palette( UINT start, UINT count, RGBQUAD FAR *pal );
extern int  m8_hw_wait_idle( void );
extern void m8_hw_fill( int x, int y, int w, int h, BYTE color );
extern void m8_hw_fill_mix( int x, int y, int w, int h, BYTE color, WORD mix );
extern void m8_hw_copy( int sx, int sy, int dx, int dy, int w, int h );
extern void m8_hw_mono( int x, int y, WORD width_bytes, WORD height, const BYTE FAR *bits,
                        WORD fg_mix, BYTE fg, WORD bg_mix, BYTE bg,
                        int clip_l, int clip_t, int clip_r, int clip_b );
extern void m8_hw_read_rect( int x, int y, int w, int h, BYTE FAR *dst, WORD pitch );
extern void m8_hw_write_rect( int x, int y, int w, int h, const BYTE FAR *src, WORD pitch );
extern void m8_hw_marker( WORD value );

/* Bring-up trace (build.ps1 -Trace): step numbers the tap logs as SCRATCH_PAD_1
 * writes. Port I/O only, so it works before the driver has a PDevice. */
#ifdef M8_TRACE
#define M8_STEP( v )        outpw( M8_SCRATCH_PAD_1, (v) )
#else
#define M8_STEP( v )
#endif

/* Count a GDI entry that reached the screen; in a -Trace build also mark it, so
 * the tap log can charge the bus traffic that follows to this entry (B0xx). */
#define M8_ENTRY( i )       { ++m8_stats[i]; M8_STEP( 0xB000 | (i) ); }
#define M8_MARK_SCR2MEM     0xB00E      /* BitBlt from the screen to memory */
#define M8_MARK_CURSOR_MOVE 0xB0C0
#define M8_MARK_CURSOR_SET  0xB0C1
#define M8_MARK_TEXT_FALLBACK 0xB0E0  /* engine text declined; the fallback follows */
#define M8_MARK_CURSOR_SHOW 0xA100  /* then x, y of the area saved */
#define M8_MARK_CURSOR_HIDE 0xA101  /* then x, y of the area restored */
#define M8_MARK_SSB_SAVE    0xB0D0
#define M8_MARK_SSB_RESTORE 0xB0D1
#define M8_MARK_SSB_CORRUPT 0xEBAD  /* then l, t, r, b of the overwritten rectangle */

#endif
