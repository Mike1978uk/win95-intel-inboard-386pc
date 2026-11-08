/* M8LORES.EXE - can the 8514 CRTC show 320x240 (and 640x240) at 640x480's line and frame
 * rate? Decides lever Z6: a quarter of the pixels for every fill.
 *
 * DISP_CNTL bit 3 DOUBLE_SCAN shows each line twice; CLOCK_SEL bit 6 halves the pixel
 * clock, so each pixel shows twice. The vertical registers are halved (skip-2 encoded,
 * guide 8-10) and, with the divide, the horizontal ones too, so the monitor gets the same
 * line and frame rate as 640x480: nothing it has not seen before.
 *
 * Each stage lasts STAGE_SECONDS; grey boxes at the top left count it:
 *   1 box    640x480 as the driver set it: red frame round the whole screen, white frame
 *            round the top-left 320x240
 *   2 boxes  double scan only (640x240): the top half of memory, rows doubled
 *   3 boxes  double scan and clock divide (320x240): the white frame at the screen's edges,
 *            stripes in the middle 2 pixels wide and 2 tall
 *   then 640x480 timing back and the desktop.
 * Frame time per stage appends to C:\M8LORES.TXT: about the same in every stage, or the
 * monitor is getting a new rate.
 *
 * Build: wcl386 -q -bt=nt -l=nt_win -ox -i=<watcom>h -i=<watcom>h\nt -i=<watcom>h\nt\directx
 *        -fe=M8LORES.EXE M8LORES.C ddraw.lib
 */

#define INITGUID
#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <ddraw.h>

#define SCR_W           640
#define SCR_H           480
#define STAGE_SECONDS   8.0
#define BLANKS_TO_TIME  30
#define BOX             12

#define SUBSYS_STAT     0x42E8
#define SUBSYS_CNTL     0x42E8
#define DISP_CNTL       0x22E8
#define ADVFUNC_CNTL    0x4AE8
#define CLOCK_SEL       0x4AEE
#define SHADOW_CTL      0x46EE
#define SHADOW_SET      0x5AEE
#define EXT_GE_CONFIG   0x7AEE
#define CUR_Y           0x82E8
#define CUR_X           0x86E8
#define MAJ_AXIS_PCNT   0x96E8
#define GP_STAT         0x9AE8
#define EXT_FIFO        0x9AEE
#define CMD             0x9AE8
#define FRGD_COLOR      0xA6E8
#define WRT_MASK        0xAAE8
#define RD_MASK         0xAEE8
#define FRGD_MIX        0xBAE8
#define MULTIFUNC_CNTL  0xBEE8
#define DP_CONFIG       0xCEEE
#define SCISSOR_L       0xDAEE
#define SCISSOR_T       0xDEEE
#define SCISSOR_R       0xE2EE
#define SCISSOR_B       0xE6EE

/* H_TOTAL, H_DISP, H_SYNC_STRT, H_SYNC_WID (bytes), V_TOTAL, V_DISP, V_SYNC_STRT (words),
 * V_SYNC_WID (byte), DISP_CNTL, CLOCK_SEL. Row 0 is the driver's 640x480 (ATIM8's values,
 * m8hw.c); the others keep its sync positions at half the count. */
typedef struct {
    const char  *name;
    WORD        crtc[8];
    WORD        disp;
    WORD        clock;
} TIMING;

static const TIMING timing[3] = {
    { "640x480", { 0x69, 0x4F, 0x53, 0x05, 0x0412, 0x03BF, 0x03D4, 0x03 }, 0x23, 0x1824 },
    { "640x240", { 0x69, 0x4F, 0x53, 0x05, 0x0209, 0x01DF, 0x01EC, 0x03 }, 0x2B, 0x1824 },
    { "320x240", { 0x34, 0x27, 0x29, 0x03, 0x0209, 0x01DF, 0x01EC, 0x03 }, 0x2B, 0x1864 },
};

static const WORD crtc_port[8] = {
    0x02E8, 0x06E8, 0x0AE8, 0x0EE8, 0x12E8, 0x16E8, 0x1AE8, 0x1EE8
};

static FILE     *out;
static double   qpf;

static double now( void )
{
    LARGE_INTEGER t;

    QueryPerformanceCounter( &t );
    return( (double)t.LowPart + 4294967296.0 * (double)t.HighPart );
}

static double ms_since( double t0 )
{
    return( (now() - t0) * 1000.0 / qpf );
}

static void wait_idle( void )
{
    long n;

    for( n = 0; n < 200000L; ++n ) {
        if( inpw( EXT_FIFO ) == 0 && !(inpw( GP_STAT ) & 0x0200) ) {
            return;
        }
    }
    fprintf( out, "engine not idle: GP_STAT %04x EXT_FIFO %04x\n", inpw( GP_STAT ), inpw( EXT_FIFO ) );
}

static void fill( int x, int y, int w, int h, int colour )
{
    wait_idle();
    outpw( WRT_MASK, 0x00FF );
    outpw( RD_MASK, 0x00FF );
    outpw( MULTIFUNC_CNTL, 0xA000 );
    outpw( SCISSOR_L, 0x0800 );
    outpw( SCISSOR_T, 0x0800 );
    outpw( SCISSOR_R, 0x07FF );
    wait_idle();
    outpw( SCISSOR_B, 0x07FF );
    outpw( DP_CONFIG, 0x2211 );
    outpw( FRGD_MIX, 0x0027 );
    outpw( FRGD_COLOR, colour );
    outpw( CUR_X, x );
    outpw( CUR_Y, y );
    outpw( MAJ_AXIS_PCNT, w - 1 );
    outpw( MULTIFUNC_CNTL, h - 1 );
    outpw( CMD, 0x40F3 );
}

static void frame_box( int x, int y, int w, int h, int colour )
{
    fill( x, y, w, 1, colour );
    fill( x, y + h - 1, w, 1, colour );
    fill( x, y, 1, h, colour );
    fill( x + w - 1, y, 1, h, colour );
}

/* m8_hw_set_mode's order for shadow set 1 (640x480), with this stage's values. */
static void set_timing( const TIMING *t )
{
    int i;

    wait_idle();
    outpw( DISP_CNTL, 0x0060 );
    outpw( ADVFUNC_CNTL, 0x0002 );
    outp( EXT_GE_CONFIG, 0x0A );
    outpw( DISP_CNTL, 0x0020 );
    outpw( SHADOW_SET, 0x0001 );
    outpw( SHADOW_CTL, 0x0000 );
    for( i = 0; i < 8; ++i ) {
        if( i < 4 || i == 7 ) {
            outp( crtc_port[i], (BYTE)t->crtc[i] );
        } else {
            outpw( crtc_port[i], t->crtc[i] );
        }
    }
    outp( DISP_CNTL, (BYTE)t->disp );
    outpw( CLOCK_SEL, t->clock );
    outpw( SHADOW_CTL, 0x003F );
    outpw( SHADOW_SET, 0x0000 );
    outpw( SHADOW_CTL, 0x0000 );
    outpw( DISP_CNTL, 0x0060 );
    outpw( ADVFUNC_CNTL, 0x0003 );
    outp( EXT_GE_CONFIG, 0x0A );
    outpw( DISP_CNTL, t->disp & 0x0008 ? 0x0028 : 0x0020 );
}

static void pace( void )
{
    volatile int    i;

    for( i = 0; i < 200; ++i ) {
    }
}

static void wait_blank( void )
{
    double  t0 = now();

    do {
        outpw( SUBSYS_CNTL, 0x0001 );
        pace();
    } while( (inpw( SUBSYS_STAT ) & 1) && ms_since( t0 ) < 40.0 );
    while( !(inpw( SUBSYS_STAT ) & 1) && ms_since( t0 ) < 40.0 ) {
        pace();
    }
}

static double frame_ms( void )
{
    double  t0;
    int     i;

    wait_blank();
    t0 = now();
    for( i = 0; i < BLANKS_TO_TIME; ++i ) {
        wait_blank();
    }
    return( ms_since( t0 ) / BLANKS_TO_TIME );
}

static void stage_boxes( int n )
{
    int i;

    for( i = 0; i < 3; ++i ) {
        fill( 8 + i * (BOX + 6), 8, BOX, BOX, i < n ? 128 : 0 );
    }
    wait_idle();
}

/* Red frame round 640x480, white round 320x240, a grey grid every 40 inside it, and two
 * blocks of one-pixel stripes: rows at (120,100), columns at (190,100). */
static void picture( void )
{
    int i;

    fill( 0, 0, 1024, SCR_H, 0 );
    frame_box( 0, 0, SCR_W, SCR_H, 249 );
    for( i = 40; i < 320; i += 40 ) {
        fill( i, 0, 1, 240, 128 );
    }
    for( i = 40; i < 240; i += 40 ) {
        fill( 0, i, 320, 1, 128 );
    }
    frame_box( 0, 0, 320, 240, 255 );
    for( i = 0; i < 64; i += 2 ) {
        fill( 120, 100 + i, 64, 1, 255 );
        fill( 190 + i, 100, 1, 64, 255 );
    }
    wait_idle();
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
    int             s;

    out = fopen( "C:\\M8LORES.TXT", "a" );
    if( out == NULL ) {
        return( 1 );
    }
    QueryPerformanceFrequency( &f );
    qpf = (double)f.LowPart;
    memset( &wc, 0, sizeof( wc ) );
    wc.lpfnWndProc = wndproc;
    wc.hInstance = inst;
    wc.lpszClassName = "M8LORES";
    RegisterClass( &wc );
    wnd = CreateWindowEx( WS_EX_TOPMOST, "M8LORES", "M8LORES", WS_POPUP | WS_VISIBLE, 0, 0, SCR_W, SCR_H,
                          NULL, NULL, inst, NULL );
    hr = DirectDrawCreate( NULL, &dd, NULL );
    fprintf( out, "== M8LORES  ddraw %08lx\n", (unsigned long)hr );
    fflush( out );
    if( hr != DD_OK ) {
        return( 1 );
    }
    hr = dd->lpVtbl->SetCooperativeLevel( dd, wnd, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN );
    fprintf( out, "coop %08lx\n", (unsigned long)hr );
    hr = dd->lpVtbl->SetDisplayMode( dd, SCR_W, SCR_H, 16 );
    fprintf( out, "setmode %08lx\n", (unsigned long)hr );
    fflush( out );
    if( hr != DD_OK ) {
        return( 1 );
    }
    ShowCursor( FALSE );
    Sleep( 1000 );
    picture();

    for( s = 0; s < 3; ++s ) {
        double  frame;

        if( s > 0 ) {
            set_timing( &timing[s] );
        }
        stage_boxes( s + 1 );
        frame = frame_ms();
        fprintf( out, "%s frame %.3f ms, %.1f Hz\n", timing[s].name, frame, 1000.0 / frame );
        fflush( out );
        Sleep( (DWORD)(STAGE_SECONDS * 1000.0) );
    }

    set_timing( &timing[0] );
    fprintf( out, "back to 640x480 frame %.3f ms\n", frame_ms() );
    ShowCursor( TRUE );
    dd->lpVtbl->RestoreDisplayMode( dd );
    dd->lpVtbl->SetCooperativeLevel( dd, wnd, DDSCL_NORMAL );
    dd->lpVtbl->Release( dd );
    fprintf( out, "done\n\n" );
    fclose( out );
    DestroyWindow( wnd );
    return( 0 );
}
