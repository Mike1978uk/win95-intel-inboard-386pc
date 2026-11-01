/* ICONTEST: draws a standard icon over a red square on the screen and
 * prints what GetPixel reads in its transparent corner and its middle. A
 * working mask leaves the corner red. */

#include <windows.h>
#include <stdio.h>

int main( void )
{
    HDC     dc = GetDC( NULL );
    HBRUSH  red = CreateSolidBrush( RGB( 255, 0, 0 ) );
    RECT    r = { 100, 100, 164, 164 };
    HICON   icons[2];
    int     k;

    icons[0] = LoadIcon( NULL, IDI_APPLICATION );
    icons[1] = LoadIcon( NULL, IDI_EXCLAMATION );
    for( k = 0; k < 2; ++k ) {
        FillRect( dc, &r, red );
        DrawIcon( dc, 116, 116, icons[k] );
        GdiFlush();
        printf( "icon %d corner %06lX middle %06lX outside %06lX\n", k,
                (unsigned long)GetPixel( dc, 116, 116 ),
                (unsigned long)GetPixel( dc, 132, 132 ),
                (unsigned long)GetPixel( dc, 104, 104 ) );
        fflush( stdout );
        Sleep( 1000 );
    }
    DeleteObject( red );
    ReleaseDC( NULL, dc );
    return( 0 );
}
