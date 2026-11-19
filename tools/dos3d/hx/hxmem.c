/* HXMEM.EXE - a Win32 console program for the HX DOS extender test (worklist X29 step 1):
 * what GlobalMemoryStatus reports under HDPMI32 on the 5160, and whether a 2 MB block can be
 * allocated and every page of it written and read back.
 * Build: wcl386 -q -bt=nt -l=nt -ox hxmem.c. GPL-3.0-or-later, as drivers/mach8_display.
 */
#include <stdio.h>
#include <windows.h>

int main( void )
{
    MEMORYSTATUS    ms;
    unsigned char   *p;
    DWORD           size = 2ul * 1024 * 1024;
    DWORD           i;
    DWORD           bad = 0;

    ms.dwLength = sizeof( ms );
    GlobalMemoryStatus( &ms );
    printf( "HXMEM: phys total %lu KB, avail %lu KB; virtual total %lu KB, avail %lu KB\n",
            ms.dwTotalPhys / 1024, ms.dwAvailPhys / 1024, ms.dwTotalVirtual / 1024,
            ms.dwAvailVirtual / 1024 );
    p = (unsigned char *)VirtualAlloc( NULL, size, MEM_COMMIT, PAGE_READWRITE );
    if( p == NULL ) {
        printf( "HXMEM: VirtualAlloc 2 MB failed, error %lu\n", GetLastError() );
        return( 1 );
    }
    for( i = 0; i < size; i += 4096 ) {
        p[i] = (unsigned char)( i >> 12 );
    }
    for( i = 0; i < size; i += 4096 ) {
        if( p[i] != (unsigned char)( i >> 12 ) ) {
            ++bad;
        }
    }
    printf( "HXMEM: 2 MB at %08lX, %lu pages, %lu bad\n", (DWORD)p, size / 4096, bad );
    VirtualFree( p, 0, MEM_RELEASE );
    return( bad != 0 );
}
