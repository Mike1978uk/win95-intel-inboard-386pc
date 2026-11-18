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
void    m8_tri( int x0, int y0, int x1, int y1, int x2, int y2, int col );
/* A trapezoid: rows y0..y1 inclusive, edges at xl/xr on row y0, stepping dxl/dxr a row
 * (16.16 fixed point). The form VBE/AF and BRender's span generators produce. */
void    m8_trap( int y0, int y1, long xl, long xr, long dxl, long dxr, int col );

void    m8_stats( M8STATS *s );
void    m8_stats_reset( void );

/* Seconds and keyboard, for programs that run without DOS calls in their loop. */
unsigned long m8_ticks( void );     /* BIOS ticks, 18.2 a second */
int     m8_key( void );             /* a key waiting; drained */

#endif
