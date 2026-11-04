/* IDLECOST: what the machine's background I/O costs a running program.
 *
 *   IDLECOST label [seconds]
 *
 * Every port access flushes the CPU's L1 (docs/captures/io_cache_flush_2026_09_21.md:
 * 90.7 us per access, 5.7 us of it bus), so a source of background I/O - the timer
 * tick, CD-ROM media polling, the NIC, a driver's timer - is paid mostly by
 * whatever runs next, refilling its cache. A loop over a tiny working set, like
 * TIMERRES's, barely notices; real programs do. This walks working sets of 1, 4, 8
 * and 12 KB (all inside the 486BL3's 16 KB L1), one read per 16-byte line, for
 * `seconds` each (default 5), and reports passes per second. Change one background
 * source, run again with another label, and compare: the bigger the working set,
 * the more each flush costs. Appends to C:\IDLECOST.TXT.
 *
 * Normal thread priority on purpose: the background sources must run as usual.
 * No C runtime; KERNEL32 and USER32!wvsprintfA only, all present in Windows 95. */
#include <windows.h>

#define LINE    16
#define MAXKB   12

static HANDLE out_file, out_con;
static LARGE_INTEGER qpf;
static volatile DWORD ws[MAXKB * 1024 / 4];

void *memset( void *d, int c, unsigned n )
{
    BYTE *p = d;
    while( n-- ) *p++ = (BYTE)c;
    return( d );
}

static void say( const char *fmt, ... )
{
    char    line[256];
    DWORD   n;
    va_list ap;

    va_start( ap, fmt );
    n = (DWORD)wvsprintfA( line, fmt, ap );
    va_end( ap );
    WriteFile( out_con, line, n, &n, NULL );
    WriteFile( out_file, line, n, &n, NULL );
}

static DWORD ms_since( LARGE_INTEGER *t0 )
{
    LARGE_INTEGER t;

    QueryPerformanceCounter( &t );
    return( (DWORD)((t.QuadPart - t0->QuadPart) * 1000 / qpf.QuadPart) );
}

/* One pass touches every 16-byte line of the working set once. */
static DWORD pass( DWORD lines )
{
    DWORD i, sum = 0;

    for( i = 0; i < lines; ++i ) sum += ws[i * (LINE / 4)];
    return( sum );
}

static int num( const char *p )
{
    int v = 0;
    while( *p >= '0' && *p <= '9' ) v = v * 10 + (*p++ - '0');
    return( v );
}

void entry( void )
{
    static const int sizes[] = { 1, 4, 8, 12 };
    char  *p = GetCommandLineA(), *label = "run";
    int    secs = 5, i;
    DWORD  sink = 0;

    out_con = GetStdHandle( STD_OUTPUT_HANDLE );
    out_file = CreateFileA( "C:\\IDLECOST.TXT", GENERIC_WRITE, 0, NULL, OPEN_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, NULL );
    SetFilePointer( out_file, 0, NULL, FILE_END );
    QueryPerformanceFrequency( &qpf );

    /* Skip the program name, then take the label and the seconds. */
    if( *p == '"' ) { ++p; while( *p && *p != '"' ) ++p; if( *p ) ++p; }
    else while( *p && *p != ' ' ) ++p;
    while( *p == ' ' ) ++p;
    if( *p ) {
        label = p;
        while( *p && *p != ' ' ) ++p;
        if( *p ) { *p++ = 0; while( *p == ' ' ) ++p; if( *p ) secs = num( p ); }
    }
    if( secs < 1 ) secs = 1;

    say( "IDLECOST %s, %d s per size, QPC %lu Hz\r\n", label, secs, (DWORD)qpf.QuadPart );
    say( "  KB  passes/s(mean)  min/s  max/s  ns/line(mean)\r\n" );
    for( i = 0; i < (int)(sizeof( sizes ) / sizeof( sizes[0] )); ++i ) {
        DWORD lines = (DWORD)sizes[i] * 1024 / LINE;
        DWORD total = 0, mn = 0xFFFFFFFF, mx = 0, s;
        LARGE_INTEGER t0;

        sink += pass( lines );                      /* load the set once */
        for( s = 0; s < (DWORD)secs; ++s ) {
            DWORD n = 0;

            QueryPerformanceCounter( &t0 );
            while( ms_since( &t0 ) < 1000 ) {
                int k;
                for( k = 0; k < 16; ++k ) sink += pass( lines );
                n += 16;
            }
            total += n;
            if( n < mn ) mn = n;
            if( n > mx ) mx = n;
        }
        say( "  %2d  %14lu  %5lu  %5lu  %13lu\r\n", sizes[i], total / secs, mn, mx,
             total ? (1000000000UL / lines) / (total / secs) : 0 );
    }
    say( "(checksum %lu)\r\n\r\n", sink );
    CloseHandle( out_file );
    ExitProcess( 0 );
}
