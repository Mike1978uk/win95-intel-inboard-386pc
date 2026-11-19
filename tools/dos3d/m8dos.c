/* m8dos - the Mach8's 8514 side from DOS. See m8dos.h.
 *
 * Two builds from this file. 32-bit (wcl386, DOS/32A): hosted, restores the screen itself
 * on exit and Ctrl-C. 16-bit (wcc, small or tiny model, DS = SS): for real-mode drivers
 * such as FS3's CUSTOM.GRA, which run inside the game with no C library, so it calls none
 * (Watcom's long-arithmetic helpers aside) and leaves exit handling to its host. Define
 * M8_HOSTED in a 16-bit program that has the library, to get the 32-bit build's hooks.
 *
 * Layout: pitch 1024, page 0 at row 0, page 1 at row 512, shown by CRT_OFFSET; the scratch
 * for polygon outlines is the off-screen column x 640-1023 of rows 0-1023.
 *
 * Between batches the engine is in the plain state: masks 0xFF, DP_CONFIG 8514, mix replace,
 * LINEDRAW_OPT last pixel off. Every path that leaves it restores it before returning, so a
 * rectangle or span needs only its colour and coordinates.
 *
 * Costs in port writes (~7.6 us each on the 5160): rect 5 (+1 colour), span 3, line 6,
 * polygon by polygon fill 5 an edge + 2 a corner + ~21, polygon by spans 3 a row + 1.
 *
 * GPL-3.0-or-later, as drivers/mach8_display.
 */
#if defined( __386__ ) && !defined( M8_HOSTED )
#define M8_HOSTED
#endif
#ifdef M8_HOSTED
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#endif
#include <i86.h>
#include "m8dos.h"

typedef unsigned char   BYTE;
typedef unsigned short  WORD;
typedef unsigned long   DWORD;

#define M8_SUBSYS           0x42E8      /* read: bit 0 vertical blank latch; write 1 acknowledges it */
#define M8_ADVFUNC_CNTL     0x4AE8
#define M8_CRT_OFFSET_LO    0x2AEE
#define M8_CRT_OFFSET_HI    0x2EEE
#define M8_GE_STAT          0x9AE8      /* read: bit 9 engine busy */
#define M8_EXT_FIFO_STATUS  0x9AEE      /* read: one bit per occupied FIFO entry */
#define M8_WRT_MASK         0xAAE8
#define M8_RD_MASK          0xAEE8
#define M8_FRGD_COLOR       0xA6E8
#define M8_FRGD_MIX         0xBAE8
#define M8_MULTIFUNC_CNTL   0xBEE8
#define M8_MIN_AXIS         0xBEE8
#define M8_CUR_X            0x86E8
#define M8_CUR_Y            0x82E8
#define M8_MAJ_AXIS_PCNT    0x96E8
#define M8_CMD              0x9AE8
#define M8_CMD_FILL         0x40F3
#define M8_SCAN_TO_X        0xCAEE      /* draws CUR_X up to (not including) it on CUR_Y */
#define M8_DESTX_DIASTP     0x8EE8
#define M8_DESTY_AXSTP      0x8AE8
#define M8_DP_CONFIG        0xCEEE
#define M8_ALU_FG_FN        0xBAEE
#define M8_LINEDRAW         0xFEEE
#define M8_LINEDRAW_INDEX   0x9AEE
#define M8_LINEDRAW_OPT     0xA2EE
#define M8_COLOR_CMP        0xB2E8      /* the value DEST_CMP_FN compares the screen pixel with */
#define M8_DEST_CMP_FN      0xEEEE      /* compare code; TRUE keeps the screen pixel (guide 9-50) */
#define M8_CMP_LESS         0x0018      /* code 3: screen pixel < COLOR_CMP */
#define M8_LINEDRAW_LAST_OFF 0x0004
#define M8_LINEDRAW_POLY    0x0002
#define M8_SRC_X_START      0xB2EE
#define M8_SRC_X_END        0xBEEE
#define M8_SRC_Y_DIR        0xC2EE
#define M8_DEST_X_START     0xA6EE
#define M8_DEST_X_END       0xAAEE
#define M8_DEST_Y_END       0xAEEE
#define M8_DP_8514          0x2211
#define M8_DP_POLY_FILL     0x2213      /* FRGD_COLOR between the blit source's RD_MASK-plane pixels */
#define M8_MIX_FRGD_REPLACE 0x0027
#define M8_MIX_XOR          0x0005
#define M8_MIX_REPLACE      0x0007
#define M8_DAC_MASK         0x02EA
#define M8_DAC_R_INDEX      0x02EB
#define M8_DAC_W_INDEX      0x02EC
#define M8_DAC_DATA         0x02ED

#define WAIT_LIMIT      60000u          /* status reads before the engine counts as wedged */
#define PAGE1_Y         512
#define PAGE1_HI        2               /* 512 rows x 1024 bytes / 4 = 0x20000 */
#define SCR_X           640
#define SCR_W           384
#define SCR_H           1024
#define MAXV            64              /* FS3 hands over polygons of 31 corners and more */
#define PASS            8               /* fill groups a batch: one scratch bit-plane each */
#ifdef __386__
#define BATCH           64              /* polygons a batch, joined into PASS groups */
#else
#define BATCH           16
#endif
#define JOIN_PX         1200L           /* box pixels a join may add: ~one fill's writes of engine time */
#define Q_SLACK         (7 * MAXV + 64) /* the most one polygon, span row or other call adds */
#define Q_CHUNK         8

#ifdef __386__
#define Q_MAX           65536
#define BIOS_TICKS      (*(volatile DWORD *)0x46C)
#define KB_HEAD         (*(volatile WORD *)0x41A)
#define KB_TAIL         (*(volatile WORD *)0x41C)

extern void io_outw( unsigned port, unsigned v );
#pragma aux io_outw = "out dx, ax" parm [edx] [eax] modify exact [];
extern void io_outb( unsigned port, unsigned v );
#pragma aux io_outb = "out dx, al" parm [edx] [eax] modify exact [];
extern unsigned io_inw( unsigned port );
#pragma aux io_inw = "xor eax, eax" "in ax, dx" parm [edx] value [eax] modify exact [eax];
extern unsigned io_inb( unsigned port );
#pragma aux io_inb = "xor eax, eax" "in al, dx" parm [edx] value [eax] modify exact [eax];

/* Each entry is port | value << 16. */
extern void q_send_run( const DWORD *p, int n );
#pragma aux q_send_run = \
    "q_next:"               \
    "mov eax, [esi]"        \
    "mov edx, eax"          \
    "shr eax, 16"           \
    "out dx, ax"            \
    "add esi, 4"            \
    "dec ecx"               \
    "jnz q_next"            \
    parm [esi] [ecx] modify [eax edx esi ecx];

/* About 84 us at 83.5 MHz, registers only: a loop that stores would hold the bus. */
extern void spin( void );
#pragma aux spin = "mov ecx, 1000" 0xE2 0xFE modify [ecx];

#else
/* The queue shares one 64 KB segment with the code and the host's needs; a frame of FS3
 * is about 2,000 writes, so a frame still goes out whole. */
#define Q_MAX           6144
#define BIOS_TICKS      (*(volatile DWORD __far *)MK_FP( 0x40, 0x6C ))
#define KB_HEAD         (*(volatile WORD __far *)MK_FP( 0x40, 0x1A ))
#define KB_TAIL         (*(volatile WORD __far *)MK_FP( 0x40, 0x1C ))

extern void io_outw( unsigned port, unsigned v );
#pragma aux io_outw = "out dx, ax" parm [dx] [ax] modify exact [];
extern void io_outb( unsigned port, unsigned v );
#pragma aux io_outb = "out dx, al" parm [dx] [ax] modify exact [];
extern unsigned io_inw( unsigned port );
#pragma aux io_inw = "in ax, dx" parm [dx] value [ax] modify exact [ax];
extern unsigned io_inb( unsigned port );
#pragma aux io_inb = "xor ax, ax" "in al, dx" parm [dx] value [ax] modify exact [ax];

/* Each entry is port, then value: the same layout as the 32-bit build. */
extern void q_send_run( const DWORD *p, int n );
#pragma aux q_send_run = \
    "cld"                   \
    "q_next:"               \
    "lodsw"                 \
    "mov dx, ax"            \
    "lodsw"                 \
    "out dx, ax"            \
    "loop q_next"           \
    parm [si] [cx] modify [ax dx si cx];

extern void spin( void );
#pragma aux spin = "mov cx, 1000" 0xE2 0xFE modify [cx];

/* Some BIOSes do not keep BP across a mode set. */
extern void bios_text_mode( void );
#pragma aux bios_text_mode = "push bp" "mov ax, 3" "int 10h" "pop bp" modify [ax bx cx dx si di es];
#endif

/* The tick count as one value: in 16-bit code its two halves are read apart. */
static DWORD ticks_now( void )
{
    DWORD   a;
    DWORD   b;

    do {
        a = BIOS_TICKS;
        b = BIOS_TICKS;
    } while( a != b );
    return( a );
}

static M8STATS  st;

/* ---- perf mailbox (m8dos.h) ----
 * The PIT's channel 0 runs in mode 3 under the BIOS, where the count goes round twice a tick
 * and an 8253 cannot say which half it is in. Hosted builds switch it to mode 2, same
 * divisor and same 18.2 Hz IRQ 0, so the count is linear, and put mode 3 back on exit. A
 * driver inside a game leaves the timer alone (the game may own it) and times in ticks. */
#define PIT_CH0         0x40
#define PIT_CMD         0x43
#define US_PER_TICK     54925ul
#ifdef __386__
#define PERF_ROWS_MAX   3600            /* an hour */
#else
#define PERF_ROWS_MAX   300             /* the small model's heap shares DGROUP with the queue */
#endif

#ifdef __386__
#define IAC_PTR         (*(volatile DWORD *)0x4F0)
static volatile M8PERF  *mb;
#else
#define IAC_PTR         (*(volatile DWORD __far *)MK_FP( 0x40, 0xF0 ))
static M8PERF           mb_block;
static volatile M8PERF  *mb = &mb_block;
#endif
static int      gfx_on;                 /* the exit state below also reads these */
static int      engine_bad;
static DWORD    iac_old;
static int      iac_set;
static int      pit_linear;
static DWORD    clk_us;                 /* since m8_open */
static DWORD    clk_tick;
static WORD     clk_count;
static DWORD    clk_frac;               /* PIT counts not yet turned into whole us */
static DWORD    last_end_us;
static DWORD    win_start_us;
static DWORD    win_frames;
static DWORD    win_max_us;
static DWORD    win_outs;
static DWORD    win_ins;

#ifdef M8_HOSTED
typedef struct {
    DWORD   run, frames, fps_x100, avg_us, max_us, outs, ins;
} PERFROW;
static PERFROW  *rows;
static int      nrows;
static int      runs;
static int      rows_dropped;
#endif

/* Elapsed time since the last call is added to clk_us. Called once a frame and at open. */
static void clock_read( DWORD *tick, WORD *count )
{
    DWORD   t;
    WORD    c;

    do {
        t = ticks_now();
        c = 0;
        if( pit_linear ) {
            io_outb( PIT_CMD, 0x00 );   /* latch channel 0 */
            c = (WORD)io_inb( PIT_CH0 );
            c |= (WORD)( io_inb( PIT_CH0 ) << 8 );
        }
    } while( ticks_now() != t );
    *tick = t;
    *count = c;
}

static void clock_advance( void )
{
    DWORD   t;
    WORD    c;
    DWORD   counts;

    clock_read( &t, &c );
    if( !pit_linear ) {
        clk_us += ( t - clk_tick ) * US_PER_TICK;
        clk_tick = t;
        return;
    }
    /* The count runs down from 65536; a reload can land before IRQ 0 has moved the tick,
     * which reads as time going back by one tick, so that tick is added. */
    counts = ( t - clk_tick ) * 65536ul + (DWORD)clk_count - (DWORD)c;
    if( (long)counts < 0 ) {
        counts += 65536ul;
    }
    clk_tick = t;
    clk_count = c;
    /* 1 count = 838.096 ns: 1,000,000 / 1,193,182. */
    clk_frac += counts * 838ul + counts / 10ul;
    clk_us += clk_frac / 1000ul;
    clk_frac %= 1000ul;
}

static void mb_publish( void )
{
    if( mb == 0 ) {
        return;
    }
    ++mb->seq;
    mb->frames = st.frames;
    mb->outs = st.outs;
    mb->ins = st.ins;
    mb->flip_polls = st.flip_polls;
    mb->flip_timeouts = st.flip_timeouts;
    mb->polys = st.polys_fill + st.polys_span;
    mb->rects = st.rects;
    mb->lines = st.lines;
    mb->clock_us = clk_us;
    mb->flags = ( gfx_on ? M8PF_OPEN : 0 ) | ( pit_linear ? M8PF_PIT_US : 0 )
              | ( engine_bad ? M8PF_WEDGED : 0 );
    ++mb->seq;
}

static void perf_open( void )
{
    int     i;

#ifdef __386__
    if( mb == 0 ) {
        union REGS  r;

        for( i = 0; i < sizeof( r ); ++i ) {
            ((BYTE *)&r)[i] = 0;
        }
        r.w.ax = 0x0100;                /* DPMI: allocate DOS memory, 4 paragraphs */
        r.w.bx = ( sizeof( M8PERF ) + 15 ) / 16;
        int386( 0x31, &r, &r );
        if( r.w.cflag == 0 ) {
            mb = (volatile M8PERF *)( (DWORD)r.w.ax << 4 );
        }
    }
#endif
    if( mb == 0 ) {
        return;
    }
    for( i = 0; i < sizeof( M8PERF ); ++i ) {
        ((volatile BYTE *)mb)[i] = 0;
    }
    mb->sig[0] = 'M'; mb->sig[1] = '8'; mb->sig[2] = 'P'; mb->sig[3] = 'F';
    mb->version = 1;
    mb->size = sizeof( M8PERF );
#ifdef M8_HOSTED
    io_outb( PIT_CMD, 0x34 );           /* channel 0, mode 2, divisor 65536 as the BIOS's */
    io_outb( PIT_CH0, 0 );
    io_outb( PIT_CH0, 0 );
    pit_linear = 1;
    if( rows == 0 ) {
        rows = (PERFROW *)malloc( PERF_ROWS_MAX * sizeof( PERFROW ) );
    }
    ++runs;
#endif
    clk_us = 0;
    clk_frac = 0;
    clock_read( &clk_tick, &clk_count );
    last_end_us = 0;
    win_start_us = 0;
    win_frames = 0;
    win_max_us = 0;
    win_outs = st.outs;
    win_ins = st.ins;
    if( !iac_set ) {
        iac_old = IAC_PTR;
#ifdef __386__
        IAC_PTR = (DWORD)mb << 12;      /* offset 0, segment = linear / 16 */
#else
        IAC_PTR = (DWORD)(void __far *)mb;
#endif
        iac_set = 1;
    }
    mb_publish();
}

static void perf_frame( void )
{
    DWORD   frame_us;
    DWORD   span;

    clock_advance();
    frame_us = clk_us - last_end_us;
    last_end_us = clk_us;
    if( st.frames <= 1 ) {              /* the first frame end starts the first second */
        win_start_us = clk_us;
        win_frames = 0;
        win_outs = st.outs;
        win_ins = st.ins;
        mb_publish();
        return;
    }
    if( st.frames > 1 && frame_us > win_max_us ) {
        win_max_us = frame_us;
    }
    ++win_frames;
    if( mb != 0 && st.frames > 1 ) {
        mb->last_us = frame_us;
    }
    span = clk_us - win_start_us;
    if( span >= 1000000ul ) {
        DWORD   fps_x100 = win_frames * 100000ul / ( span / 1000ul );
        DWORD   avg_us = span / win_frames;

        if( mb != 0 ) {
            mb->fps_x100 = fps_x100;
            mb->avg_us = avg_us;
        }
#ifdef M8_HOSTED
        if( rows != 0 && nrows < PERF_ROWS_MAX ) {
            rows[nrows].run = runs;
            rows[nrows].frames = win_frames;
            rows[nrows].fps_x100 = fps_x100;
            rows[nrows].avg_us = avg_us;
            rows[nrows].max_us = win_max_us;
            rows[nrows].outs = st.outs - win_outs;
            rows[nrows].ins = st.ins - win_ins;
            ++nrows;
        } else {
            ++rows_dropped;
        }
#endif
        win_start_us = clk_us;
        win_frames = 0;
        win_max_us = 0;
        win_outs = st.outs;
        win_ins = st.ins;
    }
    mb_publish();
}

#ifdef M8_HOSTED
/* One row a whole second of open card, every open of the run, written at exit. */
static void perf_csv( void )
{
    FILE    *f;
    int     i;

    if( nrows == 0 ) {
        return;
    }
    f = fopen( "PERF.CSV", "w" );
    if( f == 0 ) {
        return;
    }
    fprintf( f, "run,second,frames,fps,avg_ms,max_ms,port_writes,status_reads\n" );
    for( i = 0; i < nrows; ++i ) {
        const PERFROW *p = &rows[i];

        fprintf( f, "%lu,%d,%lu,%lu.%02lu,%lu.%03lu,%lu.%03lu,%lu,%lu\n", p->run, i + 1, p->frames,
                 p->fps_x100 / 100ul, p->fps_x100 % 100ul, p->avg_us / 1000ul, p->avg_us % 1000ul,
                 p->max_us / 1000ul, p->max_us % 1000ul, p->outs, p->ins );
    }
    if( rows_dropped ) {
        fprintf( f, "# %d later seconds not kept\n", rows_dropped );
    }
    fclose( f );
    nrows = 0;
}
#endif

/* The mailbox stays readable with OPEN clear; 40:F0 and the PIT get their old state back. */
static void perf_close( void )
{
    if( mb != 0 ) {
        mb_publish();
    }
    if( iac_set ) {
        IAC_PTR = iac_old;
        iac_set = 0;
    }
#ifdef M8_HOSTED
    if( pit_linear ) {
        io_outb( PIT_CMD, 0x36 );       /* channel 0, mode 3, divisor 65536: the BIOS's setting */
        io_outb( PIT_CH0, 0 );
        io_outb( PIT_CH0, 0 );
        pit_linear = 0;
    }
#endif
}

static void outw( unsigned port, unsigned v ) { io_outw( port, v ); ++st.outs; }
static void outb( unsigned port, unsigned v ) { io_outb( port, v ); ++st.outs; }
static unsigned inw( unsigned port ) { ++st.ins; return( io_inw( port ) ); }
static unsigned inb( unsigned port ) { ++st.ins; return( io_inb( port ) ); }

/* ---- state the exit path must undo ---- */
static int          pal_saved;
static BYTE         pal_old[768];
static BYTE         dac_mask_old;
static const char   *err;
#ifdef M8_HOSTED
static int          hooked;
#endif

static int      in_frame;

static void restore( void )
{
#ifdef __386__
    union REGS  r;
#endif
    int         i;

    if( !gfx_on ) {
        return;
    }
    gfx_on = 0;
    perf_close();
    if( engine_bad ) {
        io_outw( M8_SUBSYS, 0x900F );   /* reset, flags acknowledged; a locked FIFO ignores all else */
        io_outw( M8_SUBSYS, 0x500F );
    }
    io_outw( M8_CRT_OFFSET_LO, 0 );
    io_outw( M8_CRT_OFFSET_HI, 0 );
    if( pal_saved ) {
        io_outb( M8_DAC_W_INDEX, 0 );
        for( i = 0; i < 768; ++i ) {
            io_outb( M8_DAC_DATA, pal_old[i] );
        }
        io_outb( M8_DAC_MASK, dac_mask_old );
    }
    io_outw( M8_ADVFUNC_CNTL, 2 );      /* VGA */
#ifdef __386__
    for( i = 0; i < sizeof( r ); ++i ) {
        ((BYTE *)&r)[i] = 0;
    }
    r.w.ax = 0x0003;
    int386( 0x10, &r, &r );
#else
    bios_text_mode();
#endif
}

#ifdef M8_HOSTED
static void on_signal( int sig )
{
    restore();
    printf( "m8dos: stopped by signal %d, text mode restored\n", sig );
    exit( 3 );
}
#endif

/* The engine is wedged: nothing more can be drawn, so restore the screen. A hosted program
 * leaves; in a driver every later call does nothing until the next m8_open. */
static void fail( const char *msg )
{
    engine_bad = 1;
    err = msg;
    in_frame = 0;
    restore();
#ifdef M8_HOSTED
    printf( "m8dos: %s\n", msg );
    exit( 2 );
#endif
}

const char *m8_error( void )
{
    return( err );
}

/* ---- FIFO room: room for n (1..15) while bit 15 - n is clear ---- */
static int      fifo_free;

static void room( int n )
{
    unsigned    i;
    unsigned    s;

    if( n <= fifo_free ) {
        fifo_free -= n;
        return;
    }
    for( i = 0; i < WAIT_LIMIT; ++i ) {
        s = inw( M8_EXT_FIFO_STATUS );
        if( !(s & (0x8000u >> n)) ) {
            int f = n;

            while( f < 15 && !(s & (0x8000u >> (f + 1))) ) {
                ++f;
            }
            fifo_free = f - n;
            return;
        }
    }
    fail( "engine FIFO did not drain (wedged?)" );
}

static void engine_idle( void )
{
    unsigned i;

    for( i = 0; i < WAIT_LIMIT; ++i ) {
        if( inw( M8_EXT_FIFO_STATUS ) == 0 && !(inw( M8_GE_STAT ) & 0x0200) ) {
            fifo_free = 15;
            return;
        }
    }
    fail( "engine did not go idle (wedged?)" );
}

/* ---- the frame's writes, built whole before any is sent: every OUT and status read
 * empties the L1, so sending as we go would leave the drawing code running from RAM ---- */
static DWORD    q[Q_MAX];
static int      qn;

#define Q( port, value ) ( q[qn++] = (DWORD)(port) | (DWORD)(WORD)(value) << 16 )

static void q_send( void )
{
    int i = 0;

    while( i < qn && !engine_bad ) {
        int n = qn - i < Q_CHUNK ? qn - i : Q_CHUNK;

        room( n );
        if( engine_bad ) {
            break;
        }
        q_send_run( q + i, n );
        st.outs += n;
        i += n;
    }
    qn = 0;
}

/* ---- pages ---- */
static int      shown;                  /* the page CRT_OFFSET was last set to */
static int      dy;                     /* row of the page being drawn */
static int      vsync_last = 1;
static int      page_pending;
static int      page_in_blank;          /* the flip was written inside a blank */
static DWORD    page_tick;
static DWORD    t_first;

/* The last flip has taken effect, found without waiting: page_wait's quick exits. */
static int page_done( void )
{
    if( !page_pending ) {
        return( 1 );
    }
    if( ( !page_in_blank && ( inw( M8_SUBSYS ) & 1 ) ) || ticks_now() - page_tick >= 2 ) {
        page_pending = 0;
        return( 1 );
    }
    return( 0 );
}

/* Until the page last shown has taken effect. The latch was acknowledged at the write, so set
 * now means a blank has begun since. A write inside a blank misses that blank, and the latch
 * sets again for as long as a blank lasts, so then it must be seen to stay clear first. */
static void page_wait( void )
{
    unsigned n = 480;                   /* ~40 ms */

    if( !page_pending ) {
        return;
    }
    page_pending = 0;
    if( !page_in_blank && (inw( M8_SUBSYS ) & 1) ) {
        return;
    }
    if( ticks_now() - page_tick >= 2 ) {
        return;
    }
    if( page_in_blank ) {
        do {
            outw( M8_SUBSYS, 0x0001 );
            spin();
            ++st.flip_polls;
        } while( (inw( M8_SUBSYS ) & 1) && --n != 0 );
    }
    while( n != 0 && !(inw( M8_SUBSYS ) & 1) ) {
        spin();
        ++st.flip_polls;
        --n;
    }
    if( n == 0 ) {
        ++st.flip_timeouts;
    }
}

/* ---- polygon batches ---- */
typedef struct {
    int     n;
    short   x[MAXV];
    short   y[MAXV];
    int     col;
    short   l, t, r, b;                 /* box, inclusive, page coordinates */
    int     fill;                       /* by polygon fill (else spans) */
    int     z;                          /* depth 0-15 for the hardware test, -1 none */
    int     g;                          /* fill group (scratch plane), -1 spans */
} POLY;

/* Polygons of one colour and depth that share a plane and one fill: their fill pixel sets
 * must be disjoint, as the plane's parity is the XOR of theirs. */
typedef struct {
    short   l, t, r, b;                 /* box of the members */
    int     col;
    int     z;
    long    area;                       /* the members' own boxes, summed */
} GROUP;

static POLY     batch[BATCH];
static int      nbatch;
static GROUP    grp[PASS];
static int      ngrp;
static int      cur_col = -1;           /* FRGD_COLOR as last queued, -1 unknown */

static void batch_flush( void );

/* Room for one more call's writes; a full queue goes out early. The page being drawn may
 * still be on screen until the last flip has taken effect, so wait for that first. */
static void q_room( void )
{
    if( qn > Q_MAX - Q_SLACK ) {
        if( vsync_last ) {
            page_wait();
        }
        q_send();
    }
}

static void q_col( int col )
{
    if( col != cur_col ) {
        Q( M8_FRGD_COLOR, col );
        cur_col = col;
    }
}

static void q_rect( int l, int t, int r, int b )
{
    Q( M8_CUR_X, l );
    Q( M8_CUR_Y, t );
    Q( M8_MAJ_AXIS_PCNT, r - l - 1 );
    Q( M8_MIN_AXIS, b - t - 1 );
    Q( M8_CMD, M8_CMD_FILL );
}

static short    span_lo[1024];
static short    span_hi[1024];

/* A convex polygon one row at a time: 3 writes a row. Colour and plain state set. */
static void q_spans( const POLY *p )
{
    int tl;
    int i;
    int y;
    int t = p->t;
    int rows = p->b - p->t;

    for( y = 0; y <= rows; ++y ) {
        span_lo[y] = 0x7FFF;
        span_hi[y] = -0x7FFF;
    }
    for( i = 0; i < p->n; ++i ) {
        int     a = i;
        int     c = (i + 1) % p->n;
        long    x;
        long    step;

        if( p->y[a] > p->y[c] ) {
            int s = a;

            a = c;
            c = s;
        }
        step = p->y[c] == p->y[a] ? 0 : ((long)(p->x[c] - p->x[a]) << 16) / (p->y[c] - p->y[a]);
        x = ((long)p->x[a] << 16) + 0x8000;
        for( y = p->y[a] - t; y <= p->y[c] - t; ++y ) {
            int px = (int)(x >> 16);

            if( px < span_lo[y] ) span_lo[y] = (short)px;
            if( px > span_hi[y] ) span_hi[y] = (short)px;
            x += step;
        }
        y = p->y[c] - t;
        if( p->x[c] < span_lo[y] ) span_lo[y] = p->x[c];
        if( p->x[c] > span_hi[y] ) span_hi[y] = p->x[c];
    }
    /* Under the top-left rule the bottom row and each row's last pixel belong to the
     * neighbour, as with the fill. */
    tl = !( m8_opt & M8_OPT_OUTLINE );
    for( y = 0; y <= rows - tl; ++y ) {
        if( span_hi[y] + 1 - tl <= span_lo[y] ) {
            continue;
        }
        Q( M8_CUR_Y, t + y );
        Q( M8_CUR_X, span_lo[y] );
        Q( M8_SCAN_TO_X, span_hi[y] + 1 - tl );
        q_room();                       /* a wide polygon can take every row of the screen */
    }
}

/* The batch. Phases: every outline into its group's scratch plane, then one fill a group in
 * submission order, then spans (and, under the outline rule, on-screen outlines), then one clear
 * of the planes used, so the shared registers are written once a phase. Outlines go into the
 * scratch with XOR, each edge top to bottom without its last row, so every row strictly between
 * top and bottom holds exactly two pixels; members of a group share no pixel, so the plane's
 * parity is the XOR of theirs. Each group's box sits at the scratch's corner. */
int         m8_opt;

/* Scratch outlines. Chains: the left and right sides, each one connected line from the top row
 * down, so a top vertex's row gets two pixels that cancel (horizontal edges are left out: a
 * one-row edge adds nothing to the fill). Edges: one at a time, kept as a probe reference. */
static void q_chain( const POLY *p, int sx, int sy, int from, int step, int ymax )
{
    int i = from;
    int px = p->x[i];
    int py = p->y[i];

    Q( M8_LINEDRAW_INDEX, 0 );
    Q( M8_LINEDRAW, sx + px );
    Q( M8_LINEDRAW, sy + py );
    do {
        i = ( i + step + p->n ) % p->n;
        if( p->x[i] != px || p->y[i] != py ) {
            px = p->x[i];
            py = p->y[i];
            Q( M8_LINEDRAW, sx + px );
            Q( M8_LINEDRAW, sy + py );
        }
    } while( py != ymax );
}

static void q_chains( const POLY *p, int sx, int sy )
{
    int i;
    int ymin = p->y[0];
    int ymax = p->y[0];
    int down = -1;
    int up = -1;

    for( i = 1; i < p->n; ++i ) {
        if( p->y[i] < ymin ) ymin = p->y[i];
        if( p->y[i] > ymax ) ymax = p->y[i];
    }
    if( ymin == ymax ) {
        return;
    }
    for( i = 0; i < p->n; ++i ) {
        if( p->y[i] != ymin ) {
            continue;
        }
        if( p->y[( i + 1 ) % p->n] != ymin ) down = i;
        if( p->y[( i + p->n - 1 ) % p->n] != ymin ) up = i;
    }
    q_chain( p, sx, sy, down, 1, ymax );
    q_chain( p, sx, sy, up, -1, ymax );
}

/* The reference for the probe: each edge on its own, top to bottom (5 writes an edge). */
static void q_edges( const POLY *p, int sx, int sy )
{
    int i;

    for( i = 0; i < p->n; ++i ) {
        int a = i;
        int c = ( i + 1 ) % p->n;

        if( p->y[a] == p->y[c] ) {
            continue;
        }
        if( p->y[a] > p->y[c] ) {
            int s = a;

            a = c;
            c = s;
        }
        Q( M8_LINEDRAW_INDEX, 0 );
        Q( M8_LINEDRAW, sx + p->x[a] );
        Q( M8_LINEDRAW, sy + p->y[a] );
        Q( M8_LINEDRAW, sx + p->x[c] );
        Q( M8_LINEDRAW, sy + p->y[c] );
    }
}

static void batch_flush( void )
{
    POLY    *p = batch;
    int     np = nbatch;
    int     ng = ngrp;
    int     j;
    int     i;
    int     g;
    int     cw = 0;
    int     ch = 0;
    int     planes;
    int     any_z = 0;
    int     first_fill = 1;
    int     mask = -1;

    if( np == 0 ) {
        return;
    }
    nbatch = 0;
    ngrp = 0;
    for( j = 0; j < np; ++j ) {
        if( p[j].z >= 0 ) {
            any_z = 1;
        }
    }
    planes = ( 1 << ng ) - 1;
    if( planes ) {
        Q( M8_FRGD_COLOR, 0x00FF );
        Q( M8_ALU_FG_FN, M8_MIX_XOR );
        Q( M8_LINEDRAW_OPT, M8_LINEDRAW_POLY | M8_LINEDRAW_LAST_OFF );
        for( j = 0; j < np; ++j ) {
            int sx;
            int sy;

            if( !p[j].fill ) {
                continue;
            }
            /* A group's box sits at the scratch's corner: the planes keep groups apart. */
            g = p[j].g;
            sx = SCR_X - grp[g].l;
            sy = -grp[g].t;
            /* The fill is left-inclusive, right-exclusive on a row; filled from the top row to
             * the row above the bottom, the polygon follows the top-left rule and needs no outline
             * on the page: neighbours share edges with every pixel written once (L6 P3b). That
             * needs the top row to cancel cleanly, so the outline goes as top-down chains. The
             * outline rule fills only the rows between and draws the outline after: there a
             * closed figure (2 writes a corner) fills the same as edges one by one (L6 P2). */
            if( mask != g ) {
                Q( M8_WRT_MASK, 1 << g );
                mask = g;
            }
            if( m8_opt & M8_OPT_EDGES ) {
                q_edges( &p[j], sx, sy );
            } else if( m8_opt & M8_OPT_CHAINS || !( m8_opt & M8_OPT_OUTLINE ) ) {
                q_chains( &p[j], sx, sy );
            } else {
                Q( M8_LINEDRAW_INDEX, 0 );
                for( i = 0; i <= p[j].n; ++i ) {
                    Q( M8_LINEDRAW, sx + p[j].x[i % p[j].n] );
                    Q( M8_LINEDRAW, sy + p[j].y[i % p[j].n] );
                }
            }
            q_room();                   /* Q_SLACK covers one polygon, not a batch */
        }
        Q( M8_WRT_MASK, 0x00FF );
        Q( M8_DP_CONFIG, M8_DP_POLY_FILL );
        Q( M8_ALU_FG_FN, M8_MIX_REPLACE );
        Q( M8_SRC_Y_DIR, 1 );
        if( any_z ) {
            Q( M8_DEST_CMP_FN, M8_CMP_LESS );
        }
        for( g = 0; g < ng; ++g ) {
            GROUP   *gp = &grp[g];
            int     w = gp->r - gp->l + 1;

            if( gp->r - gp->l + 2 > cw ) cw = gp->r - gp->l + 2;
            if( gp->b - gp->t + 1 > ch ) ch = gp->b - gp->t + 1;
            Q( M8_RD_MASK, 1 << g );
            Q( M8_FRGD_COLOR, gp->col );
            if( any_z ) {
                Q( M8_COLOR_CMP, gp->z >= 0 ? gp->z << 4 : 0 );
            }
            /* Of the fill's registers the engine keeps only SRC_X_START from one fill to the
             * next; the rest go with every fill (L6 P1 and the register probe). */
            Q( M8_DESTX_DIASTP, SCR_X );
            Q( M8_DESTY_AXSTP, ( m8_opt & M8_OPT_OUTLINE ) ? 1 : 0 );
            if( first_fill ) {
                Q( M8_SRC_X_START, SCR_X );
                first_fill = 0;
            }
            Q( M8_SRC_X_END, SCR_X + w );
            Q( M8_CUR_X, gp->l );
            Q( M8_DEST_X_START, gp->l );
            Q( M8_DEST_X_END, gp->r + 1 );
            Q( M8_CUR_Y, ( m8_opt & M8_OPT_OUTLINE ) ? gp->t + 1 : gp->t );
            Q( M8_DEST_Y_END, gp->b );           /* starts the fill */
            q_room();
        }
        Q( M8_RD_MASK, 0x00FF );
        Q( M8_DP_CONFIG, M8_DP_8514 );
        Q( M8_LINEDRAW_OPT, M8_LINEDRAW_LAST_OFF );
    }
    cur_col = -1;
    if( any_z && !planes ) {
        Q( M8_DEST_CMP_FN, M8_CMP_LESS );
    }
    for( j = 0; j < np; ++j ) {
        if( p[j].fill && ( ( m8_opt & M8_OPT_NOOUTLINE ) || !( m8_opt & M8_OPT_OUTLINE ) ) ) {
            ++st.polys_fill;
            continue;
        }
        if( any_z ) {
            Q( M8_COLOR_CMP, p[j].z >= 0 ? p[j].z << 4 : 0 );
        }
        q_col( p[j].col );
        if( p[j].fill ) {
            Q( M8_LINEDRAW_INDEX, 0 );
            for( i = 0; i <= p[j].n; ++i ) {               /* the last closes the figure */
                Q( M8_LINEDRAW, p[j].x[i % p[j].n] );
                Q( M8_LINEDRAW, p[j].y[i % p[j].n] );
            }
            ++st.polys_fill;
            q_room();
        } else {
            q_spans( &p[j] );
            ++st.polys_span;
        }
    }
    if( any_z ) {
        Q( M8_DEST_CMP_FN, 0 );         /* scratch writes and later calls are not compared */
    }
    if( planes ) {
        /* The FIFO runs in order, so the fills have read the scratch before this clears it. */
        Q( M8_WRT_MASK, planes );
        Q( M8_FRGD_MIX, M8_MIX_FRGD_REPLACE );
        Q( M8_FRGD_COLOR, 0 );
        q_rect( SCR_X, 0, SCR_X + cw, ch );
        Q( M8_WRT_MASK, 0x00FF );
        cur_col = 0;
    }
    q_room();
}

/* ---- joined patches ---- */

/* r lies on the outer side of one of p's edges and meets that edge's line at one corner, or
 * only at the edge's own ends (a shared whole edge, which the top-left rule splits). Any other
 * contact along the line is a T-junction, where two edges' line pixels can differ by one. */
static int apart( const POLY *p, const POLY *r )
{
    long    area = 0;
    int     i;
    int     m;

    for( i = 0; i < p->n; ++i ) {
        int b = ( i + 1 ) % p->n;

        area += (long)p->x[i] * p->y[b] - (long)p->x[b] * p->y[i];
    }
    for( i = 0; i < p->n; ++i ) {
        int     b = ( i + 1 ) % p->n;
        long    ex = p->x[b] - p->x[i];
        long    ey = p->y[b] - p->y[i];
        int     on = -1;
        int     multi = 0;
        int     foreign = 0;
        int     ok = 1;

        if( ex == 0 && ey == 0 ) {
            continue;
        }
        for( m = 0; m < r->n && ok; ++m ) {
            long c = ex * ( r->y[m] - p->y[i] ) - ey * ( r->x[m] - p->x[i] );

            if( area < 0 ) {
                c = -c;
            }
            if( c > 0 ) {
                ok = 0;
            } else if( c == 0 ) {
                if( on < 0 ) {
                    on = m;
                } else if( r->x[m] != r->x[on] || r->y[m] != r->y[on] ) {
                    multi = 1;
                }
                if( ( r->x[m] != p->x[i] || r->y[m] != p->y[i] ) &&
                    ( r->x[m] != p->x[b] || r->y[m] != p->y[b] ) ) {
                    foreign = 1;
                }
            }
        }
        if( ok && !( multi && foreign ) ) {
            return( 1 );
        }
    }
    return( 0 );
}

/* p and r share no pixel: their boxes share none (fills cover [l, r) x [t, b)), or a line
 * separates them. */
static int independent( const POLY *p, const POLY *r )
{
    if( p->r <= r->l || r->r <= p->l || p->b <= r->t || r->b <= p->t ) {
        return( 1 );
    }
    return( apart( p, r ) || apart( r, p ) );
}

/* The group p can join: same colour and depth, no pixel shared with any member, and none
 * with a later group's member, as the group fills before those. Worth it while the box
 * pixels added cost less than the fill saved. -1 for a new group. Only members of groups
 * from a candidate on are tested: a test costs more CPU than the fill writes it can save. */
static int join_find( const POLY *p )
{
    int lo = ngrp;                      /* groups from lo on hold no member sharing a pixel */
    int g;
    int j;

    if( m8_opt & ( M8_OPT_NOJOIN | M8_OPT_OUTLINE ) ) {
        return( -1 );
    }
    for( g = ngrp - 1; g >= 0; --g ) {
        const GROUP *gp = &grp[g];
        int         l;
        int         t;
        int         r;
        int         b;

        if( gp->col != p->col || gp->z != p->z ) {
            continue;
        }
        for( j = 0; j < nbatch; ++j ) {
            if( batch[j].g >= g && batch[j].g < lo && !independent( p, &batch[j] ) ) {
                return( -1 );
            }
        }
        lo = g;
        l = p->l < gp->l ? p->l : gp->l;
        t = p->t < gp->t ? p->t : gp->t;
        r = p->r > gp->r ? p->r : gp->r;
        b = p->b > gp->b ? p->b : gp->b;
        if( r - l + 2 <= SCR_W && b - t + 1 <= SCR_H &&
            (long)( r - l + 1 ) * ( b - t + 1 ) <=
            gp->area + (long)( p->r - p->l + 1 ) * ( p->b - p->t + 1 ) + JOIN_PX ) {
            return( g );
        }
    }
    return( -1 );
}

/* ---- clipping ---- */
static int  cl_l, cl_t, cl_r = M8_W - 1, cl_b = M8_H - 1;
static int  cur_z = -1;

void m8_clip( int l, int t, int r, int b )
{
    cl_l = l < 0 ? 0 : l;
    cl_t = t < 0 ? 0 : t;
    cl_r = r > M8_W - 1 ? M8_W - 1 : r;
    cl_b = b > M8_H - 1 ? M8_H - 1 : b;
}

/* One edge of the clip, Sutherland-Hodgman. side: 0 left, 1 right, 2 top, 3 bottom. */
static int clip_edge( int n, const long *ix, const long *iy, long *ox, long *oy, int side, long e )
{
    int i;
    int m = 0;

    for( i = 0; i < n; ++i ) {
        long    ax = ix[i], ay = iy[i];
        long    bx = ix[(i + 1) % n], by = iy[(i + 1) % n];
        long    da = side == 0 ? ax - e : side == 1 ? e - ax : side == 2 ? ay - e : e - ay;
        long    db = side == 0 ? bx - e : side == 1 ? e - bx : side == 2 ? by - e : e - by;

        if( da >= 0 ) {
            ox[m] = ax;
            oy[m] = ay;
            ++m;
        }
        if( (da >= 0) != (db >= 0) && m < MAXV ) {
            if( side < 2 ) {
                ox[m] = e;
                oy[m] = ay + (by - ay) * da / (da - db);
            } else {
                ox[m] = ax + (bx - ax) * da / (da - db);
                oy[m] = e;
            }
            ++m;
        }
        if( m >= MAXV ) {
            break;
        }
    }
    return( m );
}

/* Writes each way would take, so the cheaper is chosen per polygon. Polygon fill: the
 * scratch outline (5 an edge + 1), the fill (11), the outline on screen (2 a corner + 4),
 * and a share of the batch set-up and scratch clear (~6). Spans: 3 a row + 1. */
static void choose( POLY *p )
{
    int rows = p->b - p->t + 1;
    int edges = 0;
    int i;
    int fill_cost;

    for( i = 0; i < p->n; ++i ) {
        edges += p->y[i] != p->y[(i + 1) % p->n];
    }
    fill_cost = 1 + 5 * edges + 11 + 4 + 2 * p->n + 6;
    /* Under the top-left rule every polygon the scratch can take goes by fill, the engine's
     * own rule: span rows are worked out on the CPU and can differ from it by a pixel on an
     * edge, which shows as holes and overlaps where a span polygon meets a filled one. */
    p->fill = rows >= 3 && p->r - p->l + 2 <= SCR_W && rows <= SCR_H &&
              ( !( m8_opt & M8_OPT_OUTLINE ) || fill_cost < 1 + 3 * rows );
}

void m8_poly( int n, const M8PT *v, int col )
{
    long    ax[MAXV], ay[MAXV], bx[MAXV], by[MAXV];
    POLY    *p;
    int     i;
    int     j;

    if( !in_frame || n < 3 ) {
        return;
    }
    if( n > MAXV ) {
        n = MAXV;
    }
    for( i = 0; i < n; ++i ) {
        ax[i] = v[i].x;
        ay[i] = v[i].y;
    }
    n = clip_edge( n, ax, ay, bx, by, 0, cl_l );
    n = clip_edge( n, bx, by, ax, ay, 1, cl_r );
    n = clip_edge( n, ax, ay, bx, by, 2, cl_t );
    n = clip_edge( n, bx, by, ax, ay, 3, cl_b );
    if( n < 3 ) {
        return;
    }
    if( nbatch == BATCH ) {
        batch_flush();
    }
    p = &batch[nbatch];
    p->n = n;
    p->col = col & 0xFF;
    p->z = cur_z;
    p->l = p->t = 0x7FFF;
    p->r = p->b = -0x7FFF;
    for( i = 0; i < n; ++i ) {
        p->x[i] = (short)ax[i];
        p->y[i] = (short)(ay[i] + dy);
        if( p->x[i] < p->l ) p->l = p->x[i];
        if( p->x[i] > p->r ) p->r = p->x[i];
        if( p->y[i] < p->t ) p->t = p->y[i];
        if( p->y[i] > p->b ) p->b = p->y[i];
    }
    /* Fills in a batch go in order, so overlapping fills layer as submitted. Spans and page
     * outlines go after every fill, so a polygon that overlaps one already in the batch is sent
     * in a new batch when either is drawn by spans, or under the outline rule. */
    choose( p );
    for( j = 0; j < nbatch; ++j ) {
        if( ( ( m8_opt & M8_OPT_OUTLINE ) || !p->fill || !batch[j].fill ) &&
            p->l <= batch[j].r && batch[j].l <= p->r && p->t <= batch[j].b && batch[j].t <= p->b ) {
            POLY t = *p;

            batch_flush();
            batch[0] = t;
            p = &batch[0];
            break;
        }
    }
    p->g = -1;
    if( p->fill ) {
        int g = join_find( p );

        if( g < 0 ) {
            if( ngrp == PASS ) {
                POLY t = *p;

                batch_flush();
                batch[0] = t;
                p = &batch[0];
            }
            g = ngrp++;
            grp[g].l = p->l;
            grp[g].t = p->t;
            grp[g].r = p->r;
            grp[g].b = p->b;
            grp[g].col = p->col;
            grp[g].z = p->z;
            grp[g].area = 0;
        } else {
            if( p->l < grp[g].l ) grp[g].l = p->l;
            if( p->t < grp[g].t ) grp[g].t = p->t;
            if( p->r > grp[g].r ) grp[g].r = p->r;
            if( p->b > grp[g].b ) grp[g].b = p->b;
            ++st.joins;
        }
        grp[g].area += (long)( p->r - p->l + 1 ) * ( p->b - p->t + 1 );
        p->g = g;
    }
    ++nbatch;
}

/* Hardware depth test (L6 P4): pixel values are depth x 16 + colour, the caller giving
 * both in col. The engine keeps a screen pixel whose value is below COLOR_CMP = z x 16, that
 * is any nearer depth; equal depth is overwritten, as in painter's order. 2 writes a polygon. */
void m8_depth( int z )
{
    cur_z = ( z >= 0 && z <= 15 ) ? z : -1;
}

void m8_tri( int x0, int y0, int x1, int y1, int x2, int y2, int col )
{
    M8PT v[3];

    v[0].x = (short)x0; v[0].y = (short)y0;
    v[1].x = (short)x1; v[1].y = (short)y1;
    v[2].x = (short)x2; v[2].y = (short)y2;
    m8_poly( 3, v, col );
}

void m8_trap( int y0, int y1, long xl, long xr, long dxl, long dxr, int col )
{
    M8PT    v[4];
    long    h = y1 - y0;

    v[0].x = (short)(xl >> 16);              v[0].y = (short)y0;
    v[1].x = (short)((xr >> 16) - 1);        v[1].y = (short)y0;
    v[2].x = (short)(((xr + dxr * h) >> 16) - 1); v[2].y = (short)y1;
    v[3].x = (short)((xl + dxl * h) >> 16);  v[3].y = (short)y1;
    m8_poly( 4, v, col );
}

/* Parity fill (L6 P5): the segments go into the scratch planes they name, each top to bottom
 * without its bottom row, as the polygon fill's outlines do; then one fill a plane. The fill
 * toggles at each pixel it meets on a row, so a tiling's edges drawn once colour alternate
 * tiles: one line an edge and one fill a colour, whatever the tile count. */
void m8_parity( int nseg, const M8SEG *s, int n, const int *col )
{
    int l = 0x7FFF;
    int t = 0x7FFF;
    int r = -0x7FFF;
    int b = -0x7FFF;
    int planes = 0;
    int mask = -1;
    int first = 1;
    int i;
    int k;

    if( !in_frame || nseg <= 0 || n <= 0 ) {
        return;
    }
    if( n > 8 ) {
        n = 8;
    }
    for( i = 0; i < nseg; ++i ) {
        if( s[i].x0 < l ) l = s[i].x0;
        if( s[i].x1 < l ) l = s[i].x1;
        if( s[i].x0 > r ) r = s[i].x0;
        if( s[i].x1 > r ) r = s[i].x1;
        if( s[i].y0 < t ) t = s[i].y0;
        if( s[i].y1 < t ) t = s[i].y1;
        if( s[i].y0 > b ) b = s[i].y0;
        if( s[i].y1 > b ) b = s[i].y1;
        planes |= s[i].planes;
    }
    planes &= ( 1 << n ) - 1;
    /* Not clipped: a toggle cut off by the clip would run the fill to the box's edge. */
    if( !planes || l < 0 || t < 0 || r >= M8_W || b >= M8_H || r - l + 2 > SCR_W || b - t + 1 > SCR_H ) {
        return;
    }
    batch_flush();
    Q( M8_FRGD_COLOR, 0x00FF );
    Q( M8_ALU_FG_FN, M8_MIX_XOR );
    Q( M8_LINEDRAW_OPT, M8_LINEDRAW_POLY | M8_LINEDRAW_LAST_OFF );
    for( i = 0; i < nseg; ++i ) {
        int m = s[i].planes & planes;
        int a = s[i].y0 <= s[i].y1;

        if( s[i].y0 == s[i].y1 || !m ) {
            continue;
        }
        if( m != mask ) {
            Q( M8_WRT_MASK, m );
            mask = m;
        }
        Q( M8_LINEDRAW_INDEX, 0 );
        Q( M8_LINEDRAW, SCR_X - l + ( a ? s[i].x0 : s[i].x1 ) );
        Q( M8_LINEDRAW, ( a ? s[i].y0 : s[i].y1 ) - t );
        Q( M8_LINEDRAW, SCR_X - l + ( a ? s[i].x1 : s[i].x0 ) );
        Q( M8_LINEDRAW, ( a ? s[i].y1 : s[i].y0 ) - t );
        q_room();
    }
    Q( M8_WRT_MASK, 0x00FF );
    Q( M8_DP_CONFIG, M8_DP_POLY_FILL );
    Q( M8_ALU_FG_FN, M8_MIX_REPLACE );
    Q( M8_SRC_Y_DIR, 1 );
    for( k = 0; k < n; ++k ) {
        if( !( planes & 1 << k ) ) {
            continue;
        }
        Q( M8_RD_MASK, 1 << k );
        Q( M8_FRGD_COLOR, col[k] & 0xFF );
        Q( M8_DESTX_DIASTP, SCR_X );
        Q( M8_DESTY_AXSTP, 0 );
        if( first ) {
            Q( M8_SRC_X_START, SCR_X );
            first = 0;
        }
        Q( M8_SRC_X_END, SCR_X + r - l + 1 );
        Q( M8_CUR_X, l );
        Q( M8_DEST_X_START, l );
        Q( M8_DEST_X_END, r + 1 );
        Q( M8_CUR_Y, dy + t );
        Q( M8_DEST_Y_END, dy + b );     /* starts the fill */
        ++st.polys_fill;
        q_room();
    }
    Q( M8_RD_MASK, 0x00FF );
    Q( M8_DP_CONFIG, M8_DP_8514 );
    Q( M8_LINEDRAW_OPT, M8_LINEDRAW_LAST_OFF );
    Q( M8_WRT_MASK, planes );
    Q( M8_FRGD_MIX, M8_MIX_FRGD_REPLACE );
    Q( M8_FRGD_COLOR, 0 );
    q_rect( SCR_X, 0, SCR_X + r - l + 2, b - t + 1 );
    Q( M8_WRT_MASK, 0x00FF );
    cur_col = 0;
    q_room();
}

void m8_rect( int l, int t, int r, int b, int col )
{
    if( !in_frame ) {
        return;
    }
    if( l < cl_l ) l = cl_l;
    if( t < cl_t ) t = cl_t;
    if( r > cl_r ) r = cl_r;
    if( b > cl_b ) b = cl_b;
    if( r < l || b < t ) {
        return;
    }
    batch_flush();
    q_col( col & 0xFF );
    q_rect( l, dy + t, r + 1, dy + b + 1 );
    ++st.rects;
    q_room();
}

void m8_clear( int col )
{
    int l = cl_l, t = cl_t, r = cl_r, b = cl_b;

    m8_clip( 0, 0, M8_W - 1, M8_H - 1 );
    m8_rect( 0, 0, M8_W - 1, M8_H - 1, col );
    m8_clip( l, t, r, b );
    /* A page clear is a few writes but milliseconds of engine time: sent now, the engine clears
     * while the CPU builds the rest of the frame. Only when the page is already off screen;
     * otherwise it waits for the flip with the rest of the frame. */
    if( in_frame && ( !vsync_last || page_done() ) ) {
        q_send();
    }
}

void m8_hline( int y, int x0, int x1, int col )
{
    if( !in_frame ) {
        return;
    }
    if( x0 > x1 ) {
        int s = x0;

        x0 = x1;
        x1 = s;
    }
    if( y < cl_t || y > cl_b ) {
        return;
    }
    if( x0 < cl_l ) x0 = cl_l;
    if( x1 > cl_r ) x1 = cl_r;
    if( x1 < x0 ) {
        return;
    }
    batch_flush();
    q_col( col & 0xFF );
    Q( M8_CUR_Y, dy + y );
    Q( M8_CUR_X, x0 );
    Q( M8_SCAN_TO_X, x1 + 1 );
    q_room();
}

/* Cohen-Sutherland against the clip rectangle. */
static int outcode( long x, long y )
{
    return( (x < cl_l) | (x > cl_r) << 1 | (y < cl_t) << 2 | (y > cl_b) << 3 );
}

void m8_line( int x0, int y0, int x1, int y1, int col )
{
    long    ax = x0, ay = y0, bx = x1, by = y1;
    int     ca = outcode( ax, ay );
    int     cb = outcode( bx, by );

    if( !in_frame ) {
        return;
    }
    while( ca | cb ) {
        long    x, y;
        int     c;

        if( ca & cb ) {
            return;
        }
        c = ca ? ca : cb;
        if( c & 1 )      { y = ay + (by - ay) * (cl_l - ax) / (bx - ax); x = cl_l; }
        else if( c & 2 ) { y = ay + (by - ay) * (cl_r - ax) / (bx - ax); x = cl_r; }
        else if( c & 4 ) { x = ax + (bx - ax) * (cl_t - ay) / (by - ay); y = cl_t; }
        else             { x = ax + (bx - ax) * (cl_b - ay) / (by - ay); y = cl_b; }
        if( c == ca ) { ax = x; ay = y; ca = outcode( ax, ay ); }
        else          { bx = x; by = y; cb = outcode( bx, by ); }
    }
    batch_flush();
    q_col( col & 0xFF );
    Q( M8_LINEDRAW_OPT, 0 );                /* last pixel on: the line is inclusive */
    Q( M8_LINEDRAW_INDEX, 0 );
    Q( M8_LINEDRAW, ax );
    Q( M8_LINEDRAW, dy + ay );
    Q( M8_LINEDRAW, bx );
    Q( M8_LINEDRAW, dy + by );
    Q( M8_LINEDRAW_OPT, M8_LINEDRAW_LAST_OFF );
    ++st.lines;
    q_room();
}

/* ---- palette, frame, mode ---- */
void m8_palette( int first, int n, const unsigned char *rgb )
{
    int i;

    outb( M8_DAC_W_INDEX, first );
    for( i = 0; i < n * 3; ++i ) {
        outb( M8_DAC_DATA, rgb[i] );
    }
}

void m8_frame_begin( void )
{
    if( !gfx_on || in_frame ) {
        return;
    }
    if( st.frames == 0 ) {
        DWORD t = ticks_now();

        while( ticks_now() == t ) {      /* time from a tick edge */
        }
        t_first = ticks_now();
    }
    in_frame = 1;
    dy = shown ? 0 : PAGE1_Y;
}

/* The show goes after the frame's last write, in the same queue. */
void m8_frame_end( int vsync )
{
    if( !in_frame ) {
        return;
    }
    batch_flush();
    in_frame = 0;
    vsync_last = vsync;
    if( vsync ) {
        page_wait();
    }
    Q( M8_CRT_OFFSET_HI, dy ? PAGE1_HI : 0 );
    Q( M8_SUBSYS, 0x0001 );
    q_send();
    page_in_blank = inw( M8_SUBSYS ) & 1;
    page_tick = ticks_now();
    page_pending = 1;
    shown = dy != 0;
    ++st.frames;
    st.ticks = ticks_now() - t_first;
    perf_frame();
}

int m8_open( void )
{
    int i;

    if( gfx_on ) {
        return( 0 );
    }
#ifdef M8_HOSTED
    if( !hooked ) {
        atexit( restore );
        atexit( perf_csv );
        signal( SIGINT, on_signal );
        signal( SIGBREAK, on_signal );
        hooked = 1;
    }
#endif
    engine_bad = 0;
    err = 0;
    /* Engine reset and set-up block of tools/m8seq/M8DOS8.ASM, proven from a cold DOS prompt. */
    outw( M8_SUBSYS, 0x9000 );
    outw( M8_SUBSYS, 0x5000 );
    gfx_on = 1;
    engine_idle();
    outw( M8_MULTIFUNC_CNTL, 0x5006 );
    outw( 0x76EE, 0x0080 );             /* GE pitch 1024 */
    outw( 0x72EE, 0 );
    outw( 0x6EEE, 0 );
    outw( M8_MULTIFUNC_CNTL, 0x1000 );  /* scissors 0..1023 */
    outw( M8_MULTIFUNC_CNTL, 0x2000 );
    outw( M8_MULTIFUNC_CNTL, 0x33FF );
    outw( M8_MULTIFUNC_CNTL, 0x43FF );
    outw( M8_WRT_MASK, 0x00FF );
    outw( M8_MULTIFUNC_CNTL, 0xA000 );
    outw( 0xEEEE, 0 );
    outw( M8_CRT_OFFSET_LO, 0 );
    outw( M8_CRT_OFFSET_HI, 0 );

    dac_mask_old = (BYTE)inb( M8_DAC_MASK );
    outb( M8_DAC_R_INDEX, 0 );
    for( i = 0; i < 768; ++i ) {
        pal_old[i] = (BYTE)inb( M8_DAC_DATA );
    }
    pal_saved = 1;
    outb( M8_DAC_MASK, 0xFF );

    /* The plain state, then both pages and the scratch cleared. */
    fifo_free = 0;
    Q( M8_WRT_MASK, 0x00FF );
    Q( M8_RD_MASK, 0x00FF );
    Q( M8_DP_CONFIG, M8_DP_8514 );
    Q( M8_FRGD_MIX, M8_MIX_FRGD_REPLACE );
    Q( M8_ALU_FG_FN, M8_MIX_REPLACE );
    Q( M8_LINEDRAW_OPT, M8_LINEDRAW_LAST_OFF );
    Q( M8_FRGD_COLOR, 0 );
    cur_col = 0;
    q_rect( 0, 0, 1024, 1024 );
    q_send();
    engine_idle();
    outw( M8_ADVFUNC_CNTL, 3 );         /* 8514 on, shadow set 1: 640x480 */
    shown = 0;
    in_frame = 0;
    nbatch = 0;
    ngrp = 0;
    m8_clip( 0, 0, M8_W - 1, M8_H - 1 );
    perf_open();
    return( 0 );
}

void m8_close( void )
{
    if( gfx_on && !engine_bad ) {
        engine_idle();
        page_wait();
    }
    restore();
}

void m8_stats( M8STATS *s )
{
    *s = st;
}

void m8_stats_reset( void )
{
    int i;

    for( i = 0; i < sizeof( st ); ++i ) {
        ((BYTE *)&st)[i] = 0;
    }
    win_outs = 0;
    win_ins = 0;
}

#ifdef M8_HOSTED
/* The shown page into buf (640 x 480 bytes) through the engine's 16-bit pixel read, the
 * sequence of tools/m8seq/M8CMP8.ASM readrow (same result on the card and in the bed). About
 * 7 us a word, so a page takes a few seconds: for checking pixels, not for drawing. */
int m8_read_shown( unsigned char *buf )
{
    int         y;
    int         x;
    int         base;
    unsigned    w;
    unsigned    n;

    if( !gfx_on || engine_bad || in_frame ) {
        return( -1 );
    }
    engine_idle();
    base = shown ? PAGE1_Y : 0;
    for( y = 0; y < M8_H; ++y ) {
        outw( M8_FRGD_MIX, 0x0047 );    /* source: pixel transfer, replace */
        outw( M8_CUR_X, 0 );
        outw( M8_CUR_Y, base + y );
        outw( M8_MAJ_AXIS_PCNT, M8_W - 1 );
        outw( M8_MULTIFUNC_CNTL, 0 );   /* MIN_AXIS_PCNT 0: one row */
        engine_idle();
        outw( M8_CMD, 0x53B0 );         /* read, 16-bit transfers */
        for( x = 0; x < M8_W; x += 2 ) {
            for( n = 0; !( inw( M8_GE_STAT ) & 0x100 ); ) {
                if( ++n > WAIT_LIMIT ) {
                    fail( "pixel read stalled" );
                    return( -1 );
                }
            }
            w = inw( 0xE2E8 );          /* PIX_TRANS */
            buf[y * M8_W + x] = (BYTE)w;
            buf[y * M8_W + x + 1] = (BYTE)( w >> 8 );
        }
        engine_idle();
    }
    outw( M8_FRGD_MIX, M8_MIX_FRGD_REPLACE );
    return( 0 );
}
#endif

unsigned long m8_ticks( void )
{
    return( ticks_now() );
}

/* A key waiting, discarded without DOS seeing a Ctrl-C in it. */
int m8_key( void )
{
    if( KB_HEAD == KB_TAIL ) {
        return( 0 );
    }
    _disable();
    KB_HEAD = KB_TAIL;
    _enable();
    return( 1 );
}
