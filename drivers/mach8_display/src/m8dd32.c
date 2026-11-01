/* M8DD32.DLL: the 32-bit DirectDraw HAL (docs/mach8_directdraw_plan.md).
 *
 * Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
 * The DriverInit hand-over follows JHRobotics' vmhal9x (MIT, see
 * LICENSE-MIT.txt).
 *
 * DDRAW loads this into the shared arena and calls DriverInit with the
 * linear address of dd16.c's block; the callbacks written there go into the
 * tables DDRAW reads. Milestone 1 sets none: DDRAW sees a HAL and does
 * everything else in software.
 */

#include <windows.h>

#define M8DD_MARKER     0x44443855UL

/* The head of dd16.c's M8DD_SHARE. */
#pragma pack( push, 1 )
typedef struct {
    DWORD   dwSize;
    DWORD   marker;
    DWORD   hInstance;
    DWORD   hot;
    DWORD   width;
    DWORD   height;
    DWORD   pitch;
    DWORD   destroy_driver;
    DWORD   create_surface;
    DWORD   can_create_surface;
    DWORD   wait_vblank;
    DWORD   destroy_surface;
    DWORD   flip;
    DWORD   lock;
    DWORD   unlock;
    DWORD   blt;
} M8DD_SHARE;
#pragma pack( pop )

static HINSTANCE    m8dd_inst;
static M8DD_SHARE   *m8dd;

BOOL WINAPI DllMain( HINSTANCE inst, DWORD reason, LPVOID reserved )
{
    if( reason == DLL_PROCESS_ATTACH ) {
        m8dd_inst = inst;
        DisableThreadLibraryCalls( inst );
    }
    return( TRUE );
}

DWORD __stdcall DriverInit( LPVOID context )
{
    if( context == NULL ) {
        return( 1 );
    }
    m8dd = context;
    m8dd->hInstance = (DWORD)m8dd_inst;
    m8dd->marker = M8DD_MARKER;
    return( 1 );
}
