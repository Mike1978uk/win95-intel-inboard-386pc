/* m8dos - the Mach8's 8514 side from DOS under a 32-bit flat extender (DOS/32A).
 * The card layer of docs/playable_3d_plan.md P3.2: every front end (our own demos, a VBE/AF
 * driver, a BRender device) draws through these calls.
 *
 * The CPU never touches card memory: every primitive becomes engine commands, queued for the
 * whole frame and sent at the end. On the 5160 each port write costs ~7.6 us, so the calls
 * are priced in writes (see m8dos.c) and the cheaper form is chosen per shape.
 *
 * Frame model: m8_frame_begin() returns the back page; draw; m8_frame_end() sends the queue
 * and shows the page. Shapes are drawn in submission order (painter's order).
 *
 * GPL-3.0-or-later, as drivers/mach8_display.
 */
#ifndef M8DOS_H
#define M8DOS_H

#define M8_W        640
#define M8_H        480

typedef struct {
    short   x, y;
} M8PT;

/* Counters since m8_open or the last m8_stats_reset. */
typedef struct {
    unsigned long   frames;
    unsigned long   outs;           /* port writes */
    unsigned long   ins;            /* status reads */
    unsigned long   polys_fill;     /* polygons by the engine's polygon fill */
    unsigned long   polys_span;     /* polygons as one span a row */
    unsigned long   joins;          /* polygons that shared another's plane and fill */
    unsigned long   rects;
    unsigned long   lines;
    unsigned long   flip_polls;
    unsigned long   flip_timeouts;
    unsigned long   ticks;          /* BIOS ticks between the first frame_begin and the last frame_end */
} M8STATS;

/* Open 640x480x8 on the 8514 side; restored on exit, Ctrl-C and m8_close. 0 on success. */
int     m8_open( void );
void    m8_close( void );
const char *m8_error( void );       /* why the engine was given up on, or NULL */

/* Palette: n entries from first, 3 bytes each, 0-63. */
void    m8_palette( int first, int n, const unsigned char *rgb );

/* Frame. vsync 0: show without waiting for the previous flip to take effect (tears). */
void    m8_frame_begin( void );
void    m8_frame_end( int vsync );

/* Clipping rectangle, inclusive; defaults to the whole screen. */
void    m8_clip( int l, int t, int r, int b );

/* Primitives, in screen coordinates of the page being drawn. */
void    m8_clear( int col );                                    /* the whole back page */
void    m8_rect( int l, int t, int r, int b, int col );         /* inclusive */
void    m8_hline( int y, int x0, int x1, int col );             /* inclusive */
void    m8_line( int x0, int y0, int x1, int y1, int col );
void    m8_poly( int n, const M8PT *v, int col );               /* convex, 3..16 corners */
/* Hardware depth test for the polygons that follow: z 0 (nearest) to 15, -1 off. With it on,
 * col must carry the depth too: col = z x 16 + colour (16 colours; the palette can shade
 * each depth band). Clear to depth 15. */
void    m8_depth( int z );
void    m8_tri( int x0, int y0, int x1, int y1, int x2, int y2, int col );
/* A trapezoid: rows y0..y1 inclusive, edges at xl/xr on row y0, stepping dxl/dxr a row
 * (16.16 fixed point). The form VBE/AF and BRender's span generators produce. */
void    m8_trap( int y0, int y1, long xl, long xr, long dxl, long dxr, int col );

/* Parity fill: each segment goes into the scratch planes in its mask, then for each plane
 * k < n one fill of col[k] over the segments' box, set where an odd number of that plane's
 * segments lie to the left on the row. Edges drawn once colour alternate tiles of a tiling
 * (L6 P5): one line an edge and one fill a colour. Segments must lie on screen, in a box at
 * most 382 pixels wide; nothing is drawn otherwise. */
typedef struct {
    short           x0, y0, x1, y1;
    unsigned char   planes;
} M8SEG;
void    m8_parity( int nseg, const M8SEG *s, int n, const int *col );
/* Once, when a texture is built: reorders segments so m8_parity sends chains (2 writes a
 * segment instead of 5) and changes plane mask least. */
void    m8_parity_order( int nseg, M8SEG *s );

/* Perf mailbox: 64 bytes in conventional memory, so a reader outside the program (a TSR, the
 * DOS agent's memory probe, DEBUG) can watch a run live. Found through the far pointer at
 * 40:F0 (offset, then segment) while the card is open; 40:F0 gets its old value back at close.
 * seq is odd while the block is being written: read until it is even and unchanged. */
#define M8PF_OPEN       1ul             /* flags: the card is open */
#define M8PF_PIT_US     2ul             /* times from the PIT (~1 us); else BIOS ticks (55 ms) */
#define M8PF_WEDGED     4ul             /* the engine was given up on */
typedef struct {
    char            sig[4];             /* "M8PF" */
    unsigned short  version;            /* 1 */
    unsigned short  size;               /* 64 */
    unsigned long   seq;
    unsigned long   frames;
    unsigned long   last_us;            /* the last frame, frame_end to frame_end */
    unsigned long   avg_us;             /* mean frame over the last whole second */
    unsigned long   fps_x100;           /* frames a second x 100, last whole second */
    unsigned long   outs;
    unsigned long   ins;
    unsigned long   flip_polls;
    unsigned long   flip_timeouts;
    unsigned long   polys;
    unsigned long   rects;
    unsigned long   lines;
    unsigned long   clock_us;           /* since m8_open */
    unsigned long   flags;
} M8PERF;

/* Card probes (worklist L6, docs/l6_card_for_3d_2026_11_19.md): each bit drops writes the
 * polygon fill may not need. Off by default until the card shows the same pixels. */
#define M8_OPT_EDGES        1   /* scratch outline one edge at a time, 5 writes an edge */
#define M8_OPT_CHAINS       2   /* scratch outline as left and right sides, top down */
#define M8_OPT_NOOUTLINE    4   /* probe only: no outline on the page after a fill */
#define M8_OPT_OUTLINE      8   /* the earlier rule: closed outline, rows between, page outline */
#define M8_OPT_NOJOIN       16  /* probe only: no joining of same colour and depth neighbours */
extern int  m8_opt;

/* Hosted builds: the shown page into buf (640 x 480 bytes), a few seconds. 0 on success. */
int     m8_read_shown( unsigned char *buf );

void    m8_stats( M8STATS *s );
void    m8_stats_reset( void );

/* Seconds and keyboard, for programs that run without DOS calls in their loop. */
unsigned long m8_ticks( void );     /* BIOS ticks, 18.2 a second */
int     m8_key( void );             /* a key waiting; drained */

#endif
