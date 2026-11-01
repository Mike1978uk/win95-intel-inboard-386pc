/* SCRGRAB [keys]: optionally opens the Start menu and types the given
 * letters (each opens a submenu), waits, then saves the screen as GDI sees
 * it to C:\SCRGRAB.BMP (24 bpp). At 16 bpp that is the driver's frame
 * buffer, to compare with what the card shows. */

#include <windows.h>
#include <stdio.h>
#include <ctype.h>

static void key( BYTE vk, int up )
{
    keybd_event( vk, 0, up ? KEYEVENTF_KEYUP : 0, 0 );
}

int main( int argc, char **argv )
{
    HDC                 sdc;
    HDC                 mdc;
    HBITMAP             bm;
    BITMAPINFO          bi;
    BITMAPFILEHEADER    fh;
    void                *bits;
    FILE                *f;
    int                 w = GetSystemMetrics( SM_CXSCREEN );
    int                 h = GetSystemMetrics( SM_CYSCREEN );
    DWORD               size;
    const char          *p;

    if( argc > 1 ) {
        key( VK_CONTROL, 0 );
        key( VK_ESCAPE, 0 );
        key( VK_ESCAPE, 1 );
        key( VK_CONTROL, 1 );
        Sleep( 1500 );
        for( p = argv[1]; *p; ++p ) {
            key( (BYTE)toupper( *p ), 0 );
            key( (BYTE)toupper( *p ), 1 );
            Sleep( 1500 );
        }
    }
    Sleep( 2000 );
    sdc = GetDC( NULL );
    mdc = CreateCompatibleDC( sdc );
    memset( &bi, 0, sizeof( bi ) );
    bi.bmiHeader.biSize = sizeof( bi.bmiHeader );
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 24;
    bm = CreateDIBSection( sdc, &bi, DIB_RGB_COLORS, &bits, NULL, 0 );
    SelectObject( mdc, bm );
    BitBlt( mdc, 0, 0, w, h, sdc, 0, 0, SRCCOPY );
    GdiFlush();
    size = (DWORD)((w * 3 + 3) & ~3) * h;
    memset( &fh, 0, sizeof( fh ) );
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof( fh ) + sizeof( bi.bmiHeader );
    fh.bfSize = fh.bfOffBits + size;
    f = fopen( "C:\\SCRGRAB.BMP", "wb" );
    if( f == NULL ) {
        printf( "cannot write\n" );
        return( 1 );
    }
    fwrite( &fh, sizeof( fh ), 1, f );
    fwrite( &bi.bmiHeader, sizeof( bi.bmiHeader ), 1, f );
    fwrite( bits, size, 1, f );
    fclose( f );
    printf( "%dx%d saved\n", w, h );
    return( 0 );
}
