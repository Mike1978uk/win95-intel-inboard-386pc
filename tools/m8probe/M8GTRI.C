/* M8GTRI.EXE - a whole triangle filled from stored colours, timed against a flat one.
 *
 * M8GRAD showed DP_CONFIG 6213h takes each pixel's colour from the blit source, on two rows. This
 * is the real sequence. A ramp sits in spare card memory, plane 7 clear. For one triangle: its
 * edges are XORed into plane 7 of the ramp, the polygon fill copies the ramp through them with plane
 * 7 write-protected, the same XOR takes the edges out again, and an outline in the colour at the
 * triangle's centre covers the top and bottom rows the fill leaves empty.
 *
 * Flat triangles go the driver's way: edges in one plane of a scratch rectangle, fill, outline, the
 * scratch cleared every 8 triangles.
 *
 * Log, C:\M8GTRI.TXT: microseconds a triangle at 32 and 100 pixels, flat and gradient, and two
 * checks that can fail: the centre pixel of each gradient triangle read back against the ramp, and
 * plane 7 of the ramp read back clear afterwards.
 *
 * For whoever is watching, about 12 s: flat triangles, then gradient, at each size. The gradient ones
 * should show smooth colour across each triangle, the flat ones one colour each. Palette entries
 * 1-127 become a colour ramp while it runs; they are put back, and the driver is asked to set the
 * card up again, since this draws behind its back.
 *
 * A Win32 console on Windows 95 is a DOS VM, so this is a window program.
 *
 * Build: wcl386 -q -bt=nt -l=nt_win -ox -i=<watcom>h -i=<watcom>h\nt -i=<watcom>h\nt\directx
 *        -fe=M8GTRI.EXE M8GTRI.C ddraw.lib
 */

#define INITGUID
#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <ddraw.h>

#define SCR_W           640
#define SCR_H           480
/* Right of the screen and below where a page would go, clear of the cursor's corner at x 992. */
#define RAMP_X          640
#define RAMP_Y          512
#define RAMP_W          352
#define RAMP_H          256
#define SCRATCH_Y       768
#define SCRATCH_H       192
#define NTRI            16
#define PASSES          10
#define HOLD_MS         2000

#define GP_STAT         0x9AE8
#define CMD             0x9AE8
#define EXT_FIFO        0x9AEE
#define SUBSYS_CNTL     0x42E8
#define CUR_Y           0x82E8
#define CUR_X           0x86E8
#define SRC_Y           0x8AE8
#define SRC_X           0x8EE8
#define MAJ_AXIS_PCNT   0x96E8
#define FRGD_COLOR      0xA6E8
#define WRT_MASK        0xAAE8
#define RD_MASK         0xAEE8
#define FRGD_MIX        0xBAE8
#define MULTIFUNC_CNTL  0xBEE8
#define PIX_TRANS       0xE2E8
#define LINEDRAW_INDEX  0x9AEE
#define LINEDRAW_OPT    0xA2EE
#define DEST_X_START    0xA6EE
#define DEST_X_END      0xAAEE
#define DEST_Y_END      0xAEEE
#define SRC_X_START     0xB2EE
#define ALU_FG_FN       0xBAEE
#define SRC_X_END       0xBEEE
#define SRC_Y_DIR       0xC2EE
#define SCAN_TO_X       0xCAEE
#define DP_CONFIG       0xCEEE
#define SCISSOR_L       0xDAEE
#define SCISSOR_T       0xDEEE
#define SCISSOR_R       0xE2EE
#define SCISSOR_B       0xE6EE
#define LINEDRAW        0xFEEE

#define DP_PLAIN        0x2211
#define DP_FILL_FLAT    0x2213
#define DP_FILL_VRAM    0x6213
#define MIX_XOR         5
#define MIX_REPLACE     7
#define OPT_LAST_OFF    4
#define OPT_POLY        2
#define ESC_RESYNC      0x4D3E

static FILE     *out;
static double   qpf;
static int      lost;           /* a wait that ran out: the timings are not trustworthy */

static double now( void )
{
    LARGE_INTEGER t;

    QueryPerformanceCounter( &t );
    return( (double)t.LowPart + 4294967296.0 * (double)t.HighPart );
}

static void wait_idle( void )
{
    long n;

    for( n = 0; n < 200000L; ++n ) {
        if( inpw( EXT_FIFO ) == 0 && !(inpw( GP_STAT ) & 0x0200) ) {
            return;
        }
    }
    ++lost;
}

/* A full FIFO can lock the card. */
static void room( int n )
{
    long i;

    for( i = 0; i < 200000L; ++i ) {
        if( !(inpw( EXT_FIFO ) & (0x8000 >> n)) ) {
            return;
        }
    }
    ++lost;
}

static void engine_setup( void )
{
    wait_idle();
    outpw( WRT_MASK, 0x00FF );
    outpw( RD_MASK, 0x00FF );
    outpw( MULTIFUNC_CNTL, 0xA000 );
    outpw( SCISSOR_L, 0x0800 );
    outpw( SCISSOR_T, 0x0800 );
    outpw( SCISSOR_R, 0x07FF );
    outpw( SCISSOR_B, 0x07FF );
    outpw( FRGD_MIX, 0x0027 );
    wait_idle();
}

static void rect( int x, int y, int w, int h, int colour )
{
    room( 12 );
    outpw( WRT_MASK, 0x00FF );
    outpw( RD_MASK, 0x00FF );
    outpw( DP_CONFIG, DP_PLAIN );
    outpw( FRGD_MIX, 0x0027 );
    outpw( FRGD_COLOR, colour );
    outpw( CUR_X, x );
    outpw( CUR_Y, y );
    outpw( MAJ_AXIS_PCNT, w - 1 );
    outpw( MULTIFUNC_CNTL, h - 1 );
    outpw( CMD, 0x40F3 );
}

/* The ramp's colour at a place in it, 1-127, rising and falling so there is no jump. */
static int ramp_at( int x, int y )
{
    int c = (x / 2 + y / 3) % 252;

    return( 1 + (c < 126 ? c : 251 - c) );
}

static void ramp_build( void )
{
    int x;
    int y;

    wait_idle();
    outpw( DP_CONFIG, DP_PLAIN );
    outpw( ALU_FG_FN, MIX_REPLACE );
    outpw( WRT_MASK, 0x00FF );
    for( y = 0; y < RAMP_H; ++y ) {
        for( x = 0; x < RAMP_W; x += 4 ) {
            room( 4 );
            outpw( FRGD_COLOR, ramp_at( x, y ) );
            outpw( CUR_Y, RAMP_Y + y );
            outpw( CUR_X, RAMP_X + x );
            outpw( SCAN_TO_X, RAMP_X + x + 4 );
        }
    }
    wait_idle();
}

/* Blue through green to red, in 6-bit DAC values. */
static void palette_ramp( void )
{
    int i;

    outp( 0x2EC, 1 );
    for( i = 1; i < 128; ++i ) {
        int r = i < 64 ? 0 : i - 64;
        int g = i < 64 ? i : 127 - i;
        int b = i < 64 ? 63 - i : 0;

        outp( 0x2ED, r );
        outp( 0x2ED, g );
        outp( 0x2ED, b );
    }
}

static BYTE pal_saved[128 * 3];

static void palette_save( void )
{
    int i;

    outp( 0x2EB, 0 );
    for( i = 0; i < 128 * 3; ++i ) {
        pal_saved[i] = (BYTE)inp( 0x2ED );
    }
}

static void palette_restore( void )
{
    int i;

    outp( 0x2EC, 0 );
    for( i = 0; i < 128 * 3; ++i ) {
        outp( 0x2ED, pal_saved[i] );
    }
}

static void outline( int ox, int oy, const short *v, int colour )
{
    int i;

    room( 14 );
    outpw( WRT_MASK, 0x00FF );
    outpw( FRGD_COLOR, colour );
    outpw( DP_CONFIG, DP_PLAIN );
    outpw( ALU_FG_FN, MIX_REPLACE );
    outpw( LINEDRAW_OPT, OPT_LAST_OFF );
    outpw( LINEDRAW_INDEX, 0 );
    for( i = 0; i < 4; ++i ) {
        outpw( LINEDRAW, ox + v[2 * (i % 3)] );
        outpw( LINEDRAW, oy + v[2 * (i % 3) + 1] );
    }
}

/* Each edge top to bottom without its last row, so every row between top and bottom holds exactly
 * two pixels; XOR, so an apex row's two pixels cancel. */
static void edges( int ox, int oy, const short *v, int plane )
{
    int i;

    room( 5 );
    outpw( WRT_MASK, plane );
    outpw( FRGD_COLOR, 0x00FF );
    outpw( DP_CONFIG, DP_PLAIN );
    outpw( ALU_FG_FN, MIX_XOR );
    outpw( LINEDRAW_OPT, OPT_POLY | OPT_LAST_OFF );
    for( i = 0; i < 3; ++i ) {
        int a = 2 * i;
        int b = 2 * ((i + 1) % 3);

        if( v[a + 1] == v[b + 1] ) {
            continue;
        }
        if( v[a + 1] > v[b + 1] ) {
            int t = a;

            a = b;
            b = t;
        }
        room( 5 );
        outpw( LINEDRAW_INDEX, 0 );
        outpw( LINEDRAW, ox + v[a] );
        outpw( LINEDRAW, oy + v[a + 1] );
        outpw( LINEDRAW, ox + v[b] );
        outpw( LINEDRAW, oy + v[b + 1] );
    }
}

/* Source at (sx, sy) for the box's top left; the destination is v itself. */
static void fill( const short *v, int sx, int sy, int dp, int mask, int rd, int colour )
{
    int l = v[0];
    int r = v[0];
    int t = v[1];
    int b = v[1];
    int i;

    for( i = 1; i < 3; ++i ) {
        if( v[2 * i] < l ) l = v[2 * i];
        if( v[2 * i] > r ) r = v[2 * i];
        if( v[2 * i + 1] < t ) t = v[2 * i + 1];
        if( v[2 * i + 1] > b ) b = v[2 * i + 1];
    }
    room( 15 );
    outpw( WRT_MASK, mask );
    outpw( RD_MASK, rd );
    outpw( DP_CONFIG, dp );
    outpw( ALU_FG_FN, MIX_REPLACE );
    outpw( FRGD_COLOR, colour );
    outpw( SRC_X, sx );
    outpw( SRC_Y, sy + 1 );
    outpw( SRC_X_START, sx );
    outpw( SRC_X_END, sx + r - l + 1 );
    outpw( SRC_Y_DIR, 1 );
    outpw( CUR_X, l );
    outpw( DEST_X_START, l );
    outpw( DEST_X_END, r + 1 );
    outpw( CUR_Y, t + 1 );
    outpw( DEST_Y_END, b );
    room( 1 );
    outpw( RD_MASK, 0x00FF );
}

static short tri[NTRI][6];

static void make_tris( int size )
{
    int i;

    for( i = 0; i < NTRI; ++i ) {
        short x = (short)(10 + (i % 4) * 158);
        short y = (short)(6 + (i / 4) * 118);

        tri[i][0] = x;
        tri[i][1] = y;
        tri[i][2] = (short)(x + size);
        tri[i][3] = (short)(y + size / 8);
        tri[i][4] = (short)(x + size / 3);
        tri[i][5] = y + (short)(size > 108 ? 108 : size);
    }
}

static void box_of( const short *v, int *l, int *t )
{
    *l = v[0];
    *t = v[1];
    if( v[2] < *l ) *l = v[2];
    if( v[4] < *l ) *l = v[4];
    if( v[3] < *t ) *t = v[3];
    if( v[5] < *t ) *t = v[5];
}

/* Where triangle i's box sits in the ramp: a different stretch for each. */
static void ramp_place( int i, int *x, int *y )
{
    *x = RAMP_X + (i % 4) * 60;
    *y = RAMP_Y + (i / 4) * 30;
}

static void draw_flat( int i, int *drawn, int *dirty_h )
{
    int l;
    int t;
    int plane = 1 << (*drawn & 7);
    int colour = 1 + i * 8;
    int b = tri[i][5];

    box_of( tri[i], &l, &t );
    edges( RAMP_X - l, SCRATCH_Y - t, tri[i], plane );
    fill( tri[i], RAMP_X, SCRATCH_Y, DP_FILL_FLAT, 0x00FF, plane, colour );
    outline( 0, 0, tri[i], colour );
    if( b - t + 1 > *dirty_h ) {
        *dirty_h = b - t + 1;
    }
    if( ++*drawn == 8 ) {
        rect( RAMP_X, SCRATCH_Y, RAMP_W, *dirty_h + 1, 0 );
        *drawn = 0;
        *dirty_h = 0;
    }
}

static void draw_grad( int i )
{
    int l;
    int t;
    int sx;
    int sy;
    int ox;
    int oy;

    box_of( tri[i], &l, &t );
    ramp_place( i, &sx, &sy );
    ox = sx - l;
    oy = sy - t;
    edges( ox, oy, tri[i], 0x0080 );
    /* Plane 7 protected at the destination, so the edge marks do not land there. */
    fill( tri[i], sx, sy, DP_FILL_VRAM, 0x007F, 0x0080, 0 );
    edges( ox, oy, tri[i], 0x0080 );
    outline( 0, 0, tri[i], ramp_at( sx - RAMP_X + 20, sy - RAMP_Y + 20 ) );
}

static int read_pixel( int x, int y )
{
    int v;
    long n;

    wait_idle();
    outpw( WRT_MASK, 0x00FF );
    outpw( RD_MASK, 0x00FF );
    outpw( DP_CONFIG, DP_PLAIN );
    outpw( FRGD_MIX, 0x0047 );
    outpw( CUR_X, x );
    outpw( CUR_Y, y );
    outpw( MAJ_AXIS_PCNT, 1 );
    outpw( MULTIFUNC_CNTL, 0 );
    wait_idle();
    outpw( CMD, 0x53B0 );
    for( n = 0; n < 200000L && !(inpw( GP_STAT ) & 0x0100); ++n ) {
    }
    v = inpw( PIX_TRANS ) & 0xFF;
    wait_idle();
    outpw( FRGD_MIX, 0x0027 );
    return( v );
}

static double time_flat( void )
{
    double  t0;
    int     p;
    int     i;
    int     drawn = 0;
    int     dirty = 0;

    wait_idle();
    t0 = now();
    for( p = 0; p < PASSES; ++p ) {
        for( i = 0; i < NTRI; ++i ) {
            draw_flat( i, &drawn, &dirty );
        }
    }
    wait_idle();
    t0 = now() - t0;
    rect( RAMP_X, SCRATCH_Y, RAMP_W, SCRATCH_H, 0 );
    return( t0 * 1e6 / qpf / (PASSES * NTRI) );
}

static double time_grad( void )
{
    double  t0;
    int     p;
    int     i;

    wait_idle();
    t0 = now();
    for( p = 0; p < PASSES; ++p ) {
        for( i = 0; i < NTRI; ++i ) {
            draw_grad( i );
        }
    }
    wait_idle();
    return( (now() - t0) * 1e6 / qpf / (PASSES * NTRI) );
}

/* The centre of a triangle against the ramp, and plane 7 of the ramp at its first vertex, which
 * the edges passed through. */
static void check_grad( int size )
{
    int i;
    int bad_colour = 0;
    int bad_plane = 0;

    for( i = 0; i < NTRI; ++i ) {
        int l;
        int t;
        int sx;
        int sy;
        int cx = (tri[i][0] + tri[i][2] + tri[i][4]) / 3;
        int cy = (tri[i][1] + tri[i][3] + tri[i][5]) / 3;
        int got;
        int want;
        int want_x;

        box_of( tri[i], &l, &t );
        ramp_place( i, &sx, &sy );
        got = read_pixel( cx, cy );
        want_x = sx - RAMP_X + cx - l;
        /* The ramp is drawn in steps of 4 columns. */
        want = ramp_at( want_x & ~3, sy - RAMP_Y + cy - t );
        if( got != want ) {
            if( bad_colour < 3 ) {
                fprintf( out, "  triangle %d centre (%d,%d): read %02x, ramp %02x\n", i, cx, cy, got, want );
            }
            ++bad_colour;
        }
        if( read_pixel( sx + tri[i][0] - l, sy + tri[i][1] - t ) & 0x80 ) {
            ++bad_plane;
        }
    }
    fprintf( out, "size %3d gradient checks: %d of %d centres differ, %d of %d ramp pixels kept plane 7\n",
             size, bad_colour, NTRI, bad_plane, NTRI );
}

static void one_size( int size )
{
    double  flat;
    double  grad;
    int     drawn = 0;
    int     dirty = 0;
    int     i;

    make_tris( size );
    rect( 0, 0, SCR_W, SCR_H, 0 );
    for( i = 0; i < NTRI; ++i ) {
        draw_flat( i, &drawn, &dirty );
    }
    rect( RAMP_X, SCRATCH_Y, RAMP_W, SCRATCH_H, 0 );
    wait_idle();
    Sleep( HOLD_MS );
    flat = time_flat();

    rect( 0, 0, SCR_W, SCR_H, 0 );
    for( i = 0; i < NTRI; ++i ) {
        draw_grad( i );
    }
    wait_idle();
    check_grad( size );
    Sleep( HOLD_MS );
    grad = time_grad();
    fprintf( out, "size %3d  flat %8.1f us/tri  gradient %8.1f us/tri  ratio %.2f\n", size, flat, grad, grad / flat );
    fflush( out );
}

static LRESULT CALLBACK wndproc( HWND w, UINT m, WPARAM a, LPARAM b )
{
    return( DefWindowProc( w, m, a, b ) );
}

int PASCAL WinMain( HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show_cmd )
{
    WNDCLASS        wc;
    HWND            wnd;
    LPDIRECTDRAW    dd;
    LARGE_INTEGER   f;
    HRESULT         hr;
    HDC             dc;

    out = fopen( "C:\\M8GTRI.TXT", "a" );
    if( out == NULL ) {
        return( 1 );
    }
    QueryPerformanceFrequency( &f );
    qpf = (double)f.LowPart;
    memset( &wc, 0, sizeof( wc ) );
    wc.lpfnWndProc = wndproc;
    wc.hInstance = inst;
    wc.lpszClassName = "M8GTRI";
    RegisterClass( &wc );
    wnd = CreateWindowEx( WS_EX_TOPMOST, "M8GTRI", "M8GTRI", WS_POPUP | WS_VISIBLE, 0, 0, SCR_W, SCR_H,
                          NULL, NULL, inst, NULL );
    hr = DirectDrawCreate( NULL, &dd, NULL );
    fprintf( out, "== M8GTRI  ddraw %08lx\n", (unsigned long)hr );
    fflush( out );
    if( hr != DD_OK ) {
        return( 1 );
    }
    dd->lpVtbl->SetCooperativeLevel( dd, wnd, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN );
    hr = dd->lpVtbl->SetDisplayMode( dd, SCR_W, SCR_H, 16 );
    fprintf( out, "setmode %08lx\n", (unsigned long)hr );
    fflush( out );
    if( hr != DD_OK ) {
        return( 1 );
    }
    ShowCursor( FALSE );
    Sleep( 1000 );

    palette_save();
    engine_setup();
    rect( 0, 0, SCR_W, SCR_H, 0 );
    rect( RAMP_X, RAMP_Y, RAMP_W, RAMP_H + SCRATCH_H, 0 );
    wait_idle();
    ramp_build();
    palette_ramp();
    one_size( 32 );
    one_size( 100 );
    fprintf( out, "waits that ran out: %d\n", lost );

    palette_restore();
    rect( 0, 0, SCR_W, SCR_H, 0 );
    wait_idle();
    ShowCursor( TRUE );
    dd->lpVtbl->RestoreDisplayMode( dd );
    dd->lpVtbl->SetCooperativeLevel( dd, wnd, DDSCL_NORMAL );
    dd->lpVtbl->Release( dd );
    dc = GetDC( NULL );
    if( dc != NULL ) {
        ExtEscape( dc, ESC_RESYNC, 0, NULL, 0, NULL );
        ReleaseDC( NULL, dc );
    }
    fprintf( out, "done\n\n" );
    fclose( out );
    DestroyWindow( wnd );
    return( 0 );
}
