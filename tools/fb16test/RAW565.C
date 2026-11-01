/* RAW565 l t r b: copies that part of the screen into a 5-6-5 DIB section
 * and prints each distinct raw pixel value with its count, so what the
 * driver's dither sees can be read without a 24-bit conversion between. */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#define MAXV 64

int main( int argc, char **argv )
{
    struct {
        BITMAPINFOHEADER    h;
        DWORD               m[3];
    }               bi;
    HDC             sdc;
    HDC             mdc;
    HBITMAP         bm;
    WORD            *bits;
    WORD            val[MAXV];
    long            cnt[MAXV];
    int             nv = 0;
    int             l, t, w, h, i, k;

    if( argc < 5 ) {
        printf( "RAW565 l t r b\n" );
        return( 1 );
    }
    l = atoi( argv[1] );
    t = atoi( argv[2] );
    w = atoi( argv[3] ) - l;
    h = atoi( argv[4] ) - t;
    memset( &bi, 0, sizeof( bi ) );
    bi.h.biSize = sizeof( bi.h );
    bi.h.biWidth = w;
    bi.h.biHeight = -h;
    bi.h.biPlanes = 1;
    bi.h.biBitCount = 16;
    bi.h.biCompression = BI_BITFIELDS;
    bi.m[0] = 0xF800;
    bi.m[1] = 0x07E0;
    bi.m[2] = 0x001F;
    sdc = GetDC( NULL );
    mdc = CreateCompatibleDC( sdc );
    bm = CreateDIBSection( sdc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0 );
    SelectObject( mdc, bm );
    BitBlt( mdc, 0, 0, w, h, sdc, l, t, SRCCOPY );
    GdiFlush();
    for( i = 0; i < w * h; ++i ) {
        for( k = 0; k < nv && val[k] != bits[i]; ++k ) {
        }
        if( k == nv ) {
            if( nv == MAXV ) {
                continue;
            }
            val[nv] = bits[i];
            cnt[nv++] = 0;
        }
        ++cnt[k];
    }
    for( k = 0; k < nv; ++k ) {
        printf( "%04X r%2d g%2d b%2d  %ld\n", val[k], val[k] >> 11, (val[k] >> 5) & 63,
                val[k] & 31, cnt[k] );
    }
    return( 0 );
}
