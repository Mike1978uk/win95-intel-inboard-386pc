/* Mach8 display driver for Windows 95 - shared definitions.
 *
 * Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
 * The driver's frame (Enable, DIB Engine glue, resources) follows Michal Necasek's
 * boxv9x as carried in JHRobotics' vmdisp9x (MIT, see LICENSE-MIT.txt).
 */

#ifndef M8_H
#define M8_H

/* Screen layout. The engine addresses 1 MB as 1024 x 1024 at 8 bpp; the visible
 * screen (640x480, 800x600 or 1024x768) sits top left, the rows below it are
 * off-screen memory. */
#define M8_PITCH            1024
#define M8_SCREEN_X         m8_scr_x
#define M8_SCREEN_Y         m8_scr_y

/* Modes, by index (m8hw.c's table). */
#define M8_MODE_640         0
#define M8_MODE_800         1
#define M8_MODE_1024        2
#define M8_MODE_COUNT       3

extern int m8_scr_x;
extern int m8_scr_y;

/* Rectangle, right and bottom exclusive. */
typedef struct {
    int     l;
    int     t;
    int     r;
    int     b;
} M8_RECT;

/* Engine registers whose last value m8hw.c keeps (m8_sh_*, see there). */
enum {
    M8_SH_FRGD_MIX, M8_SH_BKGD_MIX, M8_SH_FRGD_COLOR, M8_SH_BKGD_COLOR,
    M8_SH_WRT_MASK, M8_SH_RD_MASK, M8_SH_DP_CONFIG, M8_SH_PIX_CNTL,
    M8_SH_SC_L, M8_SH_SC_T, M8_SH_SC_R, M8_SH_SC_B,
    M8_SH_COUNT
};

/* What the short paths read, kept together: each GDI call starts with the L1
 * empty (the last call's port writes flushed it), and a field elsewhere in the
 * data segment costs a line fill of its own. Engine state is m8hw.c's, the
 * screen and cursor screen.c's; both use their own names for the fields. */
typedef struct {
    LPVOID      screen;         /* the screen PDevice, as GDI passes it */
    int         room;
    WORD        dp_mix;
    BYTE        room_ok;
    BYTE        dp_ok;
    BYTE        dp_plain;
    BYTE        hw_off;
    BYTE        busy;
    BYTE        cur_shown;
    BYTE        cur_pending;
    BYTE        rd_ok;          /* the engine holds m8_read_scan's set-up */
    M8_RECT     cur_rect;
    BYTE        sh_ok[M8_SH_COUNT];
    WORD        sh_val[M8_SH_COUNT];
} M8_HOT;

extern M8_HOT m8_hot;
#define m8_hw_off           m8_hot.hw_off

/* 8514/A registers (IBM 8514/A Technical Reference; Mach32 programmer's guide ch. 8). */
#define M8_DISP_CNTL        0x22E8
#define M8_ADVFUNC_CNTL     0x4AE8
#define M8_SUBSYS_STAT      0x42E8      /* read */
#define M8_SUBSYS_CNTL      0x42E8      /* write */
#define M8_SUBSYS_RESET     0x900F      /* engine reset, status flags acknowledged */
#define M8_SUBSYS_RUN       0x500F      /* ATIM8 writes these two to abort a transfer */
#define M8_CUR_Y            0x82E8
#define M8_CUR_X            0x86E8
#define M8_DESTY_AXSTP      0x8AE8
#define M8_DESTX_DIASTP     0x8EE8
#define M8_MAJ_AXIS_PCNT    0x96E8
#define M8_GP_STAT          0x9AE8      /* read */
#define M8_CMD              0x9AE8      /* write */
#define M8_FRGD_COLOR       0xA6E8
#define M8_WRT_MASK         0xAAE8
#define M8_RD_MASK          0xAEE8
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
#define M8_SCAN_TO_X        0xCAEE      /* draws CUR_X up to (not including) it on CUR_Y */
#define M8_SRC_X_START      0xB2EE
#define M8_SRC_X_END        0xBEEE
#define M8_SRC_Y_DIR        0xC2EE
#define M8_SRC_CMP_FN       0x92EE      /* Mach8 only: bits 3-5 compare each source pixel */
#define M8_SRC_CMP_COLOR    0xEAEE
#define M8_EXT_SCISSOR_L    0xDAEE
#define M8_EXT_SCISSOR_T    0xDEEE
#define M8_EXT_SCISSOR_R    0xE2EE
#define M8_EXT_SCISSOR_B    0xE6EE
#define M8_LINEDRAW         0xFEEE      /* one coordinate per write, by LINEDRAW_INDEX */
#define M8_LINEDRAW_INDEX   0x9AEE      /* write; 0 = move to, then line ends from 2 */
#define M8_LINEDRAW_OPT     0xA2EE
#define M8_LINEDRAW_LAST_OFF 0x0004     /* LAST_PEL_OFF: each segment leaves its end pixel */
#define M8_ALU_FG_FN        0xBAEE      /* the mix function alone, sources untouched */
#define M8_ALU_BG_FN        0xB6EE
#define M8_PATT_LENGTH      0xD2EE      /* length - 1 of a linear pattern */
#define M8_PATT_DATA_INDEX  0x82EE      /* 10h: the monochrome pattern words */
#define M8_PATT_DATA        0x8EEE
#define M8_PATT_INDEX       0xD6EE      /* next pattern bit to use */
#define M8_EXT_SCISSOR_MIN  0x0800      /* -2048 in 12 bits: the values ATIM8 leaves */
#define M8_EXT_SCISSOR_MAX  0x07FF

/* DP_CONFIG: host mono data selects foreground/background (ATIM8's text), and
 * the value the mode set leaves for the 8514/A commands. */
#define M8_DP_MONO_HOST     0x3251
#define M8_DP_8514          0x2211
#define M8_DP_MONO_PATT     0x2231      /* the mono pattern picks foreground or background */
#define M8_DP_READ_SCAN     0x5210      /* SCAN_TO_X reads to PIX_TRANS, two pixels a word */
#define M8_DP_VRAM_BLIT     0x6011      /* extended blit, video memory as the source */
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

/* Off-screen memory: the bottom-right corner holds what is under the cursor
 * (32 x 32), below every mode's screen; menu saves use the rows above it. */
#define M8_CURSOR_SAVE_X    (M8_PITCH - 32)
#define M8_CURSOR_SAVE_Y    (M8_PITCH - 32)
/* The cursor's masks as bit-planes, above the save area: right of the string
 * cache's 992 columns, inside its rows, so no cache reaches it in any mode. */
#define M8_CURSOR_MASK_X    M8_CURSOR_SAVE_X
#define M8_CURSOR_MASK_Y    (M8_CURSOR_SAVE_Y - 32)

/* Commands. Rectangle (010), LSB_FIRST (bit 12: first pixel in the low byte), 16-bit
 * transfers (bit 9), wait for host data (bit 8), x and y increasing, draw. */
#define M8_CMD_FILL         0x40F3      /* rectangle from FRGD_COLOR */
#define M8_CMD_HOST_WRITE   0x53B1      /* rectangle from PIX_TRANS, two pixels per word */
#define M8_CMD_HOST_READ    0x53B0      /* rectangle to PIX_TRANS, two pixels per word */
#define M8_CMD_BLIT         0xC053      /* screen-to-screen, add the increment bits */
#define M8_CMD_INC_X        0x0020
#define M8_CMD_INC_Y        0x0080
#define M8_CMD_PLANE_BLIT   0xC0B3      /* blit, source mixes chosen by a plane (M8TIME A) */

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
#define M8_STAT_COVERED     17      /* strips drawn without a read-back (m8_covered) */
#define M8_STAT_LINE_HW     18      /* polylines drawn by LINEDRAW */
#define M8_STAT_COUNT       19

extern DWORD m8_stats[M8_STAT_COUNT];

/* m8hw.c */
extern int  m8_hw_set_mode( int mode );
extern int  m8_hw_mode_size( int mode, int *x, int *y );
extern void m8_hw_passthrough( void );
extern void m8_hw_set_palette( UINT start, UINT count, RGBQUAD FAR *pal );
extern int  m8_hw_wait_idle( void );
extern void m8_hw_fill( int x, int y, int w, int h, BYTE color );
extern void m8_hw_fill_mix( int x, int y, int w, int h, BYTE color, WORD mix );
extern int  m8_hw_fill_fast( int x, int y, int w, int h, BYTE color, WORD mix );
extern int  m8_hw_copy_fast( int sx, int sy, int dx, int dy, int w, int h, WORD mix );
extern void m8_hw_polyline_plain( const POINT FAR *pts, WORD count, BYTE color, WORD mix );
extern int  m8_hw_line_fast( const POINT FAR *pts, WORD count, BYTE color, WORD mix );
extern void m8_hw_copy( int sx, int sy, int dx, int dy, int w, int h );
extern void m8_hw_pattern_rows( int x, int y, int w, int h, const BYTE *pat,
                                BYTE fg, BYTE bg, WORD fg_fn, WORD bg_fn );
extern void m8_hw_pattern_begin( BYTE fg, BYTE bg, WORD fg_fn, WORD bg_fn );
extern void m8_hw_pattern_span( int x, int y, int w, int h, const BYTE *pat );
extern void m8_hw_fill_begin( BYTE color, WORD mix );
extern void m8_hw_fill_span( int x, int y, int w, int h );
extern WORD m8_hw_scanlr( int x, int y, BYTE colour, WORD style, int width );
extern BYTE m8_hw_getpixel( int x, int y );
extern int  m8_hw_getpixel_fast( int x, int y );
extern void m8_hw_copy_keyed( int sx, int sy, int dx, int dy, int w, int h, BYTE key );
extern void m8_hw_plane_mix( int sx, int sy, int dx, int dy, int w, int h, int plane,
                             BYTE fg, WORD fg_mix );
extern void m8_hw_scan_spans( int poly, WORD count, const short FAR *w,
                              int l, int t, int r, int b, const BYTE *pat );
extern void m8_hw_copy_mix( int sx, int sy, int dx, int dy, int w, int h, WORD mix );
extern void m8_hw_polyline( const POINT FAR *pts, WORD count, BYTE color, WORD mix,
                            WORD style, int opaque, BYTE bg,
                            int clip_l, int clip_t, int clip_r, int clip_b );
extern void m8_hw_mono_plane( int x, int y, WORD width_bytes, WORD height, const BYTE FAR *bits,
                              int plane );
extern void m8_hw_plane_blit( int sx, int sy, int dx, int dy, int w, int h, int plane,
                              BYTE fg, BYTE bg, int transparent );
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

/* Profile build (-Prof): time spent in each timed entry point, read with
 * ExtEscape( M8_ESC_PROF ). Input: one WORD, 1 = clear after reading. The
 * layout is shared with tools/m8bench/M8BENCH.C. PIT ticks are 0.838 us. A call
 * that crossed a BIOS tick (55 ms) cannot be timed exactly from the 16-bit PIT
 * count, so it goes to `long_*`, as the shorter of its two possible lengths. */
#define M8_ESC_PROF         0x4D38
#define M8_PF_N             10      /* BitBlt ExtTextOut Output Pixel StrBlt DibToDevice
                                     * StretchBlt StretchDIBits SaveScreenBitmap ScanLR */
typedef struct {
    DWORD   calls;
    DWORD   ticks;
    DWORD   long_calls;
    DWORD   long_ticks;
} M8_PF_ENT;

typedef struct {
    WORD        version;            /* 2 */
    WORD        n;                  /* M8_PF_N */
    WORD        stamp_ticks;        /* the timer's own cost in one call's time */
    WORD        hits;               /* calls that took a short path */
    M8_PF_ENT   e[M8_PF_N];
    WORD        checked;            /* short-path reads read again the long way */
    WORD        bad;                /* ... that disagreed */
} M8_PF_TAB;

#ifdef M8_PROF
extern LONG m8_pf_escape( LPVOID in, LPVOID out );
#endif

/* Count a GDI entry that reached the screen; in a -Trace build also mark it, so
 * the tap log can charge the bus traffic that follows to this entry (B0xx). */
#define M8_ENTRY( i )       { ++m8_stats[i]; M8_STEP( 0xB000 | (i) ); }
#define M8_MARK_SCR2MEM     0xB00E      /* BitBlt from the screen to memory */
#define M8_MARK_CURSOR_MOVE 0xB0C0
#define M8_MARK_CURSOR_SET  0xB0C1
#define M8_MARK_TEXT_FALLBACK 0xB0E0  /* engine text declined; the fallback follows */
#define M8_MARK_TEXT_WHY    0xA0E0  /* | reason the text callback declined (screen.c) */
#define M8_MARK_CURSOR_SHOW 0xA100  /* then x, y of the area saved */
#define M8_MARK_CURSOR_HIDE 0xA101  /* then x, y of the area restored */
#define M8_MARK_BC_HIT      0xB0F3  /* bitmap cache: one engine copy */
#define M8_MARK_BC_STORE    0xB0F4  /* bitmap cache: uploaded, then copied */
#define M8_MARK_BC_EVICT    0xA0F5  /* bitmap cache: shelves in use cleared */
#define M8_MARK_BC_FIRST    0xA0F6  /* bitmap cache: first sight, not stored */
#define M8_MARK_PX_SAVE     0xA0F7  /* screen to memory kept on the card */
#define M8_MARK_PX_HIT      0xA0F8  /* memory to screen from the card */
#define M8_MARK_PX_BACK     0xA0F9  /* then why: proxy written back to RAM */
#define M8_MARK_PX_LOST     0xA0FA  /* proxy dropped: bitmap changed or freed */
#define M8_MARK_MEM2MEM     0xA0FC  /* trace: then dst, src, rop, w, h as byte pairs */
#define M8_MARK_SCR2MEM_ARGS 0xA0FD /* trace: the same for a screen-to-memory copy */
#define M8_MARK_MONO_BLT    0xA0FE  /* monochrome bitmap expanded by the engine */
#define M8_MARK_PX_M2M      0xA0FF  /* memory to memory done between slots */
#define M8_MARK_PX_FILL     0xA0F1  /* solid fill into a held bitmap, on the card */
#define M8_MARK_PX_UPLOAD   0xA0E1  /* whole bitmap uploaded to a proxy */
#define M8_MARK_TK_HIT      0xA0E2  /* string replayed by its key, no layout */
#define M8_MARK_TIME_IN     0xA0F2  /* trace: entry starts; then PIT count (2 bytes), BIOS tick (1) */
#define M8_MARK_TIME_OUT    0xA0F3  /* trace: entry returns; the same */
#define M8_MARK_TIME_POINT  0xA0F4  /* trace: a checkpoint: then its id, then A0F4 and the timer as above */
#define M8_MARK_PX_M2M_NO   0xA0F0  /* trace: then why m8_px_m2m declined: 1 no dest proxy, 2 source not 8-bit, 3 not cached, 4 dest evicted */
#define M8_MARK_SC_HIT      0xB0F1  /* string cache: drawn from video memory */
#define M8_MARK_SC_STORE    0xB0F2  /* string cache: stored, then drawn */
#define M8_MARK_SSB_SAVE    0xB0D0
#define M8_MARK_SSB_RESTORE 0xB0D1
#define M8_MARK_SSB_CORRUPT 0xEBAD  /* then l, t, r, b of the overwritten rectangle */

#endif
