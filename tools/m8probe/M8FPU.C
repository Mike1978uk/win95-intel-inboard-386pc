/* M8FPU.EXE - what floating point costs a Win32 program on this CPU, against
 * the same work in 16.16 fixed point: with no FPU fitted, every float
 * instruction is emulated by Windows. M8CUBE does its 3D maths in floats.
 * Build: wcl386 -q -bt=nt -l=nt -ox -fe=M8FPU.EXE M8FPU.C
 */
#include <windows.h>
#include <stdio.h>

#define N 20000L

static volatile float   fsink;
static volatile long    isink;

int main( void )
{
    LARGE_INTEGER   f, a, b;
    float           x = 1.0001f;
    float           acc = 0.0f;
    long            xi = 65543L;
    long            acci = 0;
    long            i;
    double          tf;
    double          ti;

    QueryPerformanceFrequency( &f );
    QueryPerformanceCounter( &a );
    for( i = 0; i < N; ++i ) {
        acc = acc * x + 0.5f;
    }
    QueryPerformanceCounter( &b );
    fsink = acc;
    tf = (double)(b.QuadPart - a.QuadPart) * 1e6 / (double)f.QuadPart;
    QueryPerformanceCounter( &a );
    for( i = 0; i < N; ++i ) {
        acci = (long)(((__int64)acci * xi) >> 16) + 32768L;
    }
    QueryPerformanceCounter( &b );
    isink = acci;
    ti = (double)(b.QuadPart - a.QuadPart) * 1e6 / (double)f.QuadPart;
    printf( "float multiply-add   %8.3f us each\n", tf / N );
    printf( "16.16 multiply-add   %8.3f us each\n", ti / N );
    printf( "ratio                %8.1f\n", tf / ti );
    return( 0 );
}
