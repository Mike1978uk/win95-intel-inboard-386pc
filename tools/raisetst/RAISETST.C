/* RAISETST.EXE - calls RaiseException once, under a filter that handles it, so a
   debugger breakpoint on KERNEL32!RaiseException can be shown to fire. Exits 0. */
#include <windows.h>

static LONG WINAPI filt( EXCEPTION_POINTERS *p )
{
    (void)p;
    ExitProcess( 0 );
    return EXCEPTION_EXECUTE_HANDLER;
}

int main( void )
{
    DWORD args[2] = { 0x11111111, 0x22222222 };
    SetUnhandledExceptionFilter( filt );
    RaiseException( 0xE0C0FFEE, 0, 2, args );
    return 1;
}
