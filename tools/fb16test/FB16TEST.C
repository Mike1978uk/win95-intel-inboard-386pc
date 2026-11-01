/* FB16TEST.EXE - what each kind of GDI call costs the 16 bpp mode (fb16.c):
 * access calls, sends, pixels sent, read with ExtEscape 0x4D3A around one
 * call of each kind.
 * Build: wcl386 -q -bt=nt -l=nt -ox -fe=FB16TEST.EXE FB16TEST.C
 */
#include <windows.h>
#include <stdio.h>

static HDC      dc;
static DWORD    last[4];

static void show( const char *what )
{
    DWORD   now[4];

    GdiFlush();
    if( ExtEscape( dc, 0x4D3A, 0, NULL, sizeof( now ), (LPSTR)now ) <= 0 ) {
        printf( "%-14s no counters\n", what );
        return;
    }
    printf( "%-14s access %5lu  sends %5lu  pixels %8lu  cursor %lu\n", what,
            now[0] - last[0], now[1] - last[1], now[2] - last[2], now[3] - last[3] );
    memcpy( last, now, sizeof( last ) );
}

int main( void )
{
    HPEN    pen;
    RECT    rc;
    HBRUSH  br;
    int     i;

    setvbuf( stdout, NULL, _IONBF, 0 );
    dc = GetDC( NULL );
    show( "start" );
    PatBlt( dc, 100, 100, 64, 32, BLACKNESS );
    show( "patblt 64x32" );
    PatBlt( dc, 0, 0, 400, 300, WHITENESS );
    show( "patblt 400x300" );
    MoveToEx( dc, 10, 10, NULL );
    LineTo( dc, 200, 150 );
    show( "line" );
    pen = SelectObject( dc, GetStockObject( BLACK_PEN ) );
    for( i = 0; i < 10; ++i ) {
        MoveToEx( dc, 10, 20 + i * 4, NULL );
        LineTo( dc, 300, 20 + i * 4 );
    }
    SelectObject( dc, pen );
    show( "10 h lines" );
    TextOut( dc, 20, 200, "Hello, 16 bpp", 13 );
    show( "textout" );
    BitBlt( dc, 300, 300, 100, 100, dc, 0, 0, SRCCOPY );
    show( "blt 100x100" );
    SetRect( &rc, 200, 200, 260, 240 );
    br = CreateSolidBrush( RGB( 255, 0, 0 ) );
    FillRect( dc, &rc, br );
    DeleteObject( br );
    show( "fillrect red" );
    GetPixel( dc, 5, 5 );
    show( "getpixel" );
    SetPixel( dc, 5, 5, RGB( 0, 0, 255 ) );
    show( "setpixel" );
    ReleaseDC( NULL, dc );
    InvalidateRect( NULL, NULL, TRUE );
    return( 0 );
}
