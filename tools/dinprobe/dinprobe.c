/* DINPROBE: does DirectInput work on the 5160, and what does reading each
 * device cost (#58)? Opens the keyboard, the mouse and every attached joystick
 * or wheel through DirectInput (the DirectX 5 interface, so DirectX 5-7 runtimes
 * all serve it), then reads each every 20 ms for `seconds` (default 15) while
 * you press keys, move the mouse and move each stick. Reports what it saw and
 * the time each read took. Appends to C:\DINPROBE.TXT.
 *
 *   DINPROBE [seconds]
 *
 * A game-port joystick is polled: DirectInput times each axis by reading port
 * 201h in a loop, and every port read empties the L1
 * (docs/captures/io_cache_flush_2026_09_21.md), so its per-read time is the
 * cost a game pays per frame per stick.
 *
 * DINPUT.DLL is loaded at run time so the program can say so if DirectX is
 * missing. No C runtime; KERNEL32, USER32 and DirectInput only. */
#define DIRECTINPUT_VERSION 0x0500
#include <windows.h>
#include <dinput.h>

#define MAXJOY  4
#define DEVS    (2 + MAXJOY)

typedef HRESULT (WINAPI *DICREATE)( HINSTANCE, DWORD, LPDIRECTINPUTA *, LPUNKNOWN );

static HANDLE out_file, out_con;
static LARGE_INTEGER qpf;
static GUID   joy_guid[MAXJOY];
static char   joy_name[MAXJOY][MAX_PATH];
static int    njoy;

void *memset( void *d, int c, unsigned n )
{
    BYTE *p = d;
    while( n-- ) *p++ = (BYTE)c;
    return( d );
}

void *memcpy( void *d, const void *s, unsigned n )
{
    BYTE *p = d;
    const BYTE *q = s;
    while( n-- ) *p++ = *q++;
    return( d );
}

static void say( const char *fmt, ... )
{
    char    line[512];
    DWORD   n;
    va_list ap;

    va_start( ap, fmt );
    n = (DWORD)wvsprintfA( line, fmt, ap );
    va_end( ap );
    WriteFile( out_con, line, n, &n, NULL );
    WriteFile( out_file, line, n, &n, NULL );
}

static DWORD us_since( LARGE_INTEGER *t0 )
{
    LARGE_INTEGER t;

    QueryPerformanceCounter( &t );
    return( (DWORD)((t.QuadPart - t0->QuadPart) * 1000000 / qpf.QuadPart) );
}

static BOOL CALLBACK enum_joy( LPCDIDEVICEINSTANCEA d, LPVOID ctx )
{
    (void)ctx;
    if( njoy < MAXJOY ) {
        joy_guid[njoy] = d->guidInstance;
        lstrcpynA( joy_name[njoy], d->tszProductName, MAX_PATH );
        ++njoy;
    }
    return( DIENUM_CONTINUE );
}

typedef struct {
    LPDIRECTINPUTDEVICEA  dev;
    LPDIRECTINPUTDEVICE2A dev2;     /* for Poll; NULL if the runtime lacks it */
    const char *name;
    DWORD reads, fails, sum_ms, sum_us, max_us;
    DWORD events;
} DEV;

static int open_dev( LPDIRECTINPUTA di, HWND hwnd, REFGUID guid, LPCDIDATAFORMAT fmt,
                     const char *name, DEV *d )
{
    HRESULT hr;

    memset( d, 0, sizeof( *d ) );
    d->name = name;
    hr = IDirectInput_CreateDevice( di, guid, &d->dev, NULL );
    if( FAILED( hr ) ) { say( "  %s: CreateDevice failed %08lX\r\n", name, hr ); return( 0 ); }
    hr = IDirectInputDevice_SetDataFormat( d->dev, fmt );
    if( FAILED( hr ) ) { say( "  %s: SetDataFormat failed %08lX\r\n", name, hr ); return( 0 ); }
    hr = IDirectInputDevice_SetCooperativeLevel( d->dev, hwnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE );
    if( FAILED( hr ) ) { say( "  %s: SetCooperativeLevel failed %08lX\r\n", name, hr ); return( 0 ); }
    hr = IDirectInputDevice_Acquire( d->dev );
    if( FAILED( hr ) ) { say( "  %s: Acquire failed %08lX\r\n", name, hr ); return( 0 ); }
    if( FAILED( IDirectInputDevice_QueryInterface( d->dev, &IID_IDirectInputDevice2A, (void **)&d->dev2 ) ) ) {
        d->dev2 = NULL;
    }
    say( "  %s: open%s\r\n", name, d->dev2 ? "" : " (no IDirectInputDevice2: not polled)" );
    return( 1 );
}

/* One timed read: Poll (polled devices need it), then the state. */
static int read_dev( DEV *d, void *state, DWORD size )
{
    LARGE_INTEGER t0;
    HRESULT hr;
    DWORD us;

    QueryPerformanceCounter( &t0 );
    if( d->dev2 ) IDirectInputDevice2_Poll( d->dev2 );
    hr = IDirectInputDevice_GetDeviceState( d->dev, size, state );
    us = us_since( &t0 );
    if( hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED ) {
        IDirectInputDevice_Acquire( d->dev );
    }
    ++d->reads;
    if( FAILED( hr ) ) { ++d->fails; return( 0 ); }
    d->sum_us += us;
    if( d->sum_us >= 1000 ) { d->sum_ms += d->sum_us / 1000; d->sum_us %= 1000; }
    if( us > d->max_us ) d->max_us = us;
    return( 1 );
}

static void run( int secs )
{
    HINSTANCE   inst = GetModuleHandleA( NULL );
    HMODULE     dll = LoadLibraryA( "DINPUT.DLL" );
    DICREATE    create;
    LPDIRECTINPUTA di;
    WNDCLASSA   wc;
    HWND        hwnd;
    HRESULT     hr;
    DEV         dev[DEVS];
    BYTE        keys[256], keys_old[256];
    DIMOUSESTATE ms;
    DIJOYSTATE  js[MAXJOY], js_old[MAXJOY];
    long        mx = 0, my = 0;
    DWORD       mbuttons = 0;
    LARGE_INTEGER t0;
    int         i, k, ok[DEVS];

    if( dll == NULL ) { say( "DINPUT.DLL not found: DirectX is not installed\r\n" ); return; }
    create = (DICREATE)GetProcAddress( dll, "DirectInputCreateA" );
    if( create == NULL ) { say( "DirectInputCreateA missing from DINPUT.DLL\r\n" ); return; }
    hr = create( inst, DIRECTINPUT_VERSION, &di, NULL );
    if( FAILED( hr ) ) { say( "DirectInputCreate failed %08lX\r\n", hr ); return; }

    /* Cooperative levels need a window; a hidden one is enough in the background. */
    memset( &wc, 0, sizeof( wc ) );
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = inst;
    wc.lpszClassName = "DINPROBE";
    RegisterClassA( &wc );
    hwnd = CreateWindowExA( 0, "DINPROBE", "DINPROBE", WS_OVERLAPPED, 0, 0, 1, 1, NULL, NULL, inst, NULL );

    IDirectInput_EnumDevices( di, DIDEVTYPE_JOYSTICK, enum_joy, NULL, DIEDFL_ATTACHEDONLY );
    say( "DirectInput created (interface 0x0500), %d joystick(s) attached\r\n", njoy );

    ok[0] = open_dev( di, hwnd, &GUID_SysKeyboard, &c_dfDIKeyboard, "keyboard", &dev[0] );
    ok[1] = open_dev( di, hwnd, &GUID_SysMouse, &c_dfDIMouse, "mouse", &dev[1] );
    for( i = 0; i < njoy; ++i ) {
        ok[2 + i] = open_dev( di, hwnd, &joy_guid[i], &c_dfDIJoystick, joy_name[i], &dev[2 + i] );
    }

    say( "reading every 20 ms for %d s: press keys, move the mouse, move each stick\r\n", secs );
    memset( keys_old, 0, sizeof( keys_old ) );
    memset( js_old, 0, sizeof( js_old ) );
    QueryPerformanceCounter( &t0 );
    while( us_since( &t0 ) < (DWORD)secs * 1000000 ) {
        MSG m;

        while( PeekMessageA( &m, NULL, 0, 0, PM_REMOVE ) ) DispatchMessageA( &m );
        if( ok[0] && read_dev( &dev[0], keys, sizeof( keys ) ) ) {
            for( k = 0; k < 256; ++k ) {
                if( (keys[k] & 0x80) && !(keys_old[k] & 0x80) ) {
                    say( "  key down, DIK %02X\r\n", k );
                    ++dev[0].events;
                }
            }
            memcpy( keys_old, keys, sizeof( keys ) );
        }
        if( ok[1] && read_dev( &dev[1], &ms, sizeof( ms ) ) ) {
            mx += ms.lX; my += ms.lY;
            if( ms.lX || ms.lY ) ++dev[1].events;
            for( k = 0; k < 4; ++k ) {
                if( ms.rgbButtons[k] & 0x80 ) mbuttons |= 1u << k;
            }
        }
        for( i = 0; i < njoy; ++i ) {
            if( ok[2 + i] && read_dev( &dev[2 + i], &js[i], sizeof( js[i] ) ) ) {
                if( js[i].lX != js_old[i].lX || js[i].lY != js_old[i].lY ||
                    js[i].lZ != js_old[i].lZ || js[i].lRz != js_old[i].lRz ||
                    *(DWORD *)js[i].rgbButtons != *(DWORD *)js_old[i].rgbButtons ) {
                    ++dev[2 + i].events;
                }
                js_old[i] = js[i];
            }
        }
        Sleep( 20 );
    }

    say( "mouse: moved %ld, %ld in total; buttons seen %lX\r\n", mx, my, mbuttons );
    for( i = 0; i < njoy; ++i ) {
        if( ok[2 + i] ) {
            say( "%s: last X %ld Y %ld Z %ld Rz %ld buttons %08lX\r\n", joy_name[i], js_old[i].lX,
                 js_old[i].lY, js_old[i].lZ, js_old[i].lRz, *(DWORD *)js_old[i].rgbButtons );
        }
    }
    say( "device             reads  fails  changes  us/read(mean)  max\r\n" );
    for( i = 0; i < 2 + njoy; ++i ) {
        DWORD good;

        if( !ok[i] ) continue;
        good = dev[i].reads - dev[i].fails;
        say( "%-18.18s %5lu  %5lu  %7lu  %13lu  %lu\r\n", dev[i].name, dev[i].reads, dev[i].fails,
             dev[i].events, good ? (dev[i].sum_ms * 1000 + dev[i].sum_us) / good : 0, dev[i].max_us );
        IDirectInputDevice_Unacquire( dev[i].dev );
        if( dev[i].dev2 ) IDirectInputDevice2_Release( dev[i].dev2 );
        IDirectInputDevice_Release( dev[i].dev );
    }
    IDirectInput_Release( di );
    DestroyWindow( hwnd );
    say( "done\r\n\r\n" );
}

void entry( void )
{
    char *p = GetCommandLineA();
    int   secs = 15;

    out_con = GetStdHandle( STD_OUTPUT_HANDLE );
    out_file = CreateFileA( "C:\\DINPROBE.TXT", GENERIC_WRITE, 0, NULL, OPEN_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, NULL );
    SetFilePointer( out_file, 0, NULL, FILE_END );
    QueryPerformanceFrequency( &qpf );

    if( *p == '"' ) { ++p; while( *p && *p != '"' ) ++p; if( *p ) ++p; }
    else while( *p && *p != ' ' ) ++p;
    while( *p == ' ' ) ++p;
    if( *p >= '0' && *p <= '9' ) {
        secs = 0;
        while( *p >= '0' && *p <= '9' ) secs = secs * 10 + (*p++ - '0');
    }
    if( secs < 1 ) secs = 1;

    run( secs );
    CloseHandle( out_file );
    ExitProcess( 0 );
}
