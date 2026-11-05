/* M8PAGE.EXE - does CRT_OFFSET flip the display on this Mach8, and when does a write take
 * effect? Decides whether Direct3D can draw on a hidden page and show it whole.
 *
 * At 640x480 the engine holds two pages at pitch 1024: page 0 at row 0 is white, page 1
 * at row 512 is black. Grey boxes at the top left of both pages count the stage, for
 * whoever is watching. Each stage lasts STAGE_SECONDS:
 *   1 box    page 0 shown: all white
 *   2 boxes  page 1 shown: all black, or the offset's unit is not the guide's
 *   3 boxes  a flip at each blank, slowly: white and black in turn, no split
 *   4 boxes  page 1 written mid-frame, page 0 written at the blank. Top white and bottom
 *            black: a write takes effect at once. All white: it waits for the frame.
 *   5 boxes  page 1 written just after the blank, page 0 just before the next. Black and
 *            white mixed: the frame latches early. All white: late, or at once.
 *
 * Timings append to C:\M8PAGE.TXT. A Win32 console on Windows 95 is a DOS VM, so this is
 * a window program.
 *
 * Build: wcl386 -q -bt=nt -l=nt_win -ox -i=<watcom>h -i=<watcom>h\nt -i=<watcom>h\nt\directx
 *        -fe=M8PAGE.EXE M8PAGE.C ddraw.lib
 */

#define INITGUID
#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <ddraw.h>

#define SCR_W           640
#define SCR_H           480
#define PAGE1_Y         512
#define PAGE1_OFFSET_HI 2               /* 512 x 1024 bytes / 4 = 0x20000 */
#define STAGE_SECONDS   6.0
#define SLOW_FLIPS      36
#define BLANKS_TO_TIME  36
#define BOX             24

#define CRT_OFFSET_LO   0x2AEE
#define CRT_OFFSET_HI   0x2EEE
#define SUBSYS_STAT     0x42E8
#define SUBSYS_CNTL     0x42E8
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

static void show( int page )
{
    outpw( CRT_OFFSET_LO, 0 );
    outpw( CRT_OFFSET_HI, page ? PAGE1_OFFSET_HI : 0 );
}

/* The latch holds through the blank, so it is read every ~90 us, not back to back: a tight
 * loop would keep the bus from the Sound Blaster's DMA and the XT-CF for up to a frame. */
static void wait_blank( void )
{
    double          t0 = now();
    volatile int    i;

    outpw( SUBSYS_CNTL, 0x0001 );
    while( !(inpw( SUBSYS_STAT ) & 1) && ms_since( t0 ) < 40.0 ) {
        for( i = 0; i < 200; ++i ) {
        }
    }
}

static void stage_boxes( int n )
{
    int i;
    int y;

    for( y = 0; y <= PAGE1_Y; y += PAGE1_Y ) {
        for( i = 0; i < n; ++i ) {
            fill( 8 + i * (BOX + 8), y + 8, BOX, BOX, 128 );
        }
    }
    wait_idle();
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
    double          t0;
    double          frame;
    double          t;
    double          worst = 0.0;
    int             n;

    out = fopen( "C:\\M8PAGE.TXT", "a" );
    if( out == NULL ) {
        return( 1 );
    }
    QueryPerformanceFrequency( &f );
    qpf = (double)f.LowPart;
    SetPriorityClass( GetCurrentProcess(), HIGH_PRIORITY_CLASS );
    memset( &wc, 0, sizeof( wc ) );
    wc.lpfnWndProc = wndproc;
    wc.hInstance = inst;
    wc.lpszClassName = "M8PAGE";
    RegisterClass( &wc );
    wnd = CreateWindowEx( WS_EX_TOPMOST, "M8PAGE", "M8PAGE", WS_POPUP | WS_VISIBLE, 0, 0, SCR_W, SCR_H,
                          NULL, NULL, inst, NULL );
    hr = DirectDrawCreate( NULL, &dd, NULL );
    fprintf( out, "== M8PAGE  ddraw %08lx\n", (unsigned long)hr );
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

    fill( 0, 0, SCR_W, SCR_H, 255 );
    fill( 0, PAGE1_Y, SCR_W, SCR_H, 0 );
    stage_boxes( 1 );
    show( 0 );
    Sleep( (DWORD)(STAGE_SECONDS * 1000.0) );

    stage_boxes( 2 );
    t0 = now();
    show( 1 );
    fprintf( out, "two offset writes %.3f ms\n", ms_since( t0 ) );
    Sleep( (DWORD)(STAGE_SECONDS * 1000.0) );

    show( 0 );
    stage_boxes( 3 );
    frame = frame_ms();
    fprintf( out, "frame %.3f ms, %.1f Hz\n", frame, 1000.0 / frame );
    fflush( out );
    t0 = now();
    n = 0;
    while( ms_since( t0 ) < STAGE_SECONDS * 1000.0 ) {
        int i;

        for( i = 0; i < SLOW_FLIPS; ++i ) {
            wait_blank();
        }
        show( ++n & 1 );
    }

    show( 0 );
    stage_boxes( 4 );
    t0 = now();
    while( ms_since( t0 ) < STAGE_SECONDS * 1000.0 ) {
        double b;

        wait_blank();
        show( 0 );
        b = now();
        while( ms_since( b ) < frame / 2.0 ) {
        }
        show( 1 );
    }

    show( 0 );
    stage_boxes( 5 );
    t0 = now();
    while( ms_since( t0 ) < STAGE_SECONDS * 1000.0 ) {
        double b;

        wait_blank();
        b = now();
        show( 1 );
        while( ms_since( b ) < frame * 0.9 ) {
        }
        t = ms_since( b );
        if( t > worst ) {
            worst = t;
        }
        show( 0 );
    }
    fprintf( out, "stage 5 worst time before the write back %.2f ms\n", worst );

    show( 0 );
    ShowCursor( TRUE );
    dd->lpVtbl->RestoreDisplayMode( dd );
    dd->lpVtbl->SetCooperativeLevel( dd, wnd, DDSCL_NORMAL );
    dd->lpVtbl->Release( dd );
    fprintf( out, "done\n\n" );
    fclose( out );
    DestroyWindow( wnd );
    return( 0 );
}
