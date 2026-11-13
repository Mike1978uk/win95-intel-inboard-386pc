/* XTSECT: time absolute sector reads through Windows 95's disk stack, to
 * compare XT-CF miniport builds by their cost per command. Read only: it
 * never writes a sector.
 *
 *   XTSECT label [drive] [count] [start] [stride]
 *
 * drive: letter, default C. count: reads per pass, default 500. start: first
 * logical sector of the volume, default 20000. stride: sectors between reads,
 * default 37, so neither the card nor a read-ahead serves the next read from
 * the last one. Each pass reads `count` sectors one at a time, then eight at a
 * time; three passes, each over a different sector range. Appends to
 * C:\XTSECT.TXT.
 *
 * A read of one sector moves 512 bytes at 3.82 us a byte (XTIDETR.ASM), so
 * anything under ~2 ms a sector was served from a cache, not the card; the
 * output says so.
 *
 * Uses VWIN32's INT 25h service (absolute disk read, the large-volume packet
 * form). No C runtime; KERNEL32 and USER32!wvsprintfA only. */
#include <windows.h>

#define VWIN32_DIOC_DOS_INT25 2
#define MAXSEC 8

typedef struct {
    DWORD reg_EBX, reg_EDX, reg_ECX, reg_EAX, reg_EDI, reg_ESI, reg_Flags;
} DIOC_REGS;

#pragma pack(1)
typedef struct {
    DWORD start;
    WORD  count;
    DWORD buffer;
} DISKIO;
#pragma pack()

static HANDLE out_file, out_con, vwin32;
static LARGE_INTEGER qpf;
static BYTE buf[MAXSEC * 512];

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

static DWORD us_since( LARGE_INTEGER *t0 )
{
    LARGE_INTEGER t;

    QueryPerformanceCounter( &t );
    return( (DWORD)((t.QuadPart - t0->QuadPart) * 1000000 / qpf.QuadPart) );
}

static int read_abs( int drive, DWORD sector, int n )
{
    DIOC_REGS r;
    DISKIO    d;
    DWORD     got;

    d.start = sector;
    d.count = (WORD)n;
    d.buffer = (DWORD)buf;
    memset( &r, 0, sizeof( r ) );
    r.reg_EAX = (DWORD)drive;               /* 0 = A: */
    r.reg_EBX = (DWORD)&d;
    r.reg_ECX = 0xFFFF;                     /* packet form */
    if( !DeviceIoControl( vwin32, VWIN32_DIOC_DOS_INT25, &r, sizeof( r ), &r,
                          sizeof( r ), &got, NULL ) ) {
        return( 0 );
    }
    return( !(r.reg_Flags & 1) );           /* carry set = error */
}

static int num( const char *p )
{
    int v = 0;
    while( *p >= '0' && *p <= '9' ) v = v * 10 + (*p++ - '0');
    return( v );
}

/* Split the command line in place; argv[0] is the program. */
static int split( char *p, char **argv, int max )
{
    int argc = 0;

    while( *p && argc < max ) {
        while( *p == ' ' || *p == '\t' ) ++p;
        if( *p == 0 ) break;
        if( *p == '"' ) {
            argv[argc++] = ++p;
            while( *p && *p != '"' ) ++p;
        } else {
            argv[argc++] = p;
            while( *p && *p != ' ' && *p != '\t' ) ++p;
        }
        if( *p ) *p++ = 0;
    }
    return( argc );
}

static void run( int argc, char **argv )
{
    char *label = argc > 1 ? argv[1] : "run";
    int   drive = 2, count = 500, pass, size;
    DWORD start = 20000, stride = 37;

    if( argc > 2 ) {
        char c = argv[2][0];
        if( c >= 'a' && c <= 'z' ) c -= 'a' - 'A';
        if( c >= 'A' && c <= 'Z' ) drive = c - 'A';
    }
    if( argc > 3 ) count = num( argv[3] );
    if( argc > 4 ) start = (DWORD)num( argv[4] );
    if( argc > 5 ) stride = (DWORD)num( argv[5] );
    if( count < 1 ) count = 1;
    /* The eight-sector reads sit after each one-sector read: keep them apart. */
    if( stride < 2 * MAXSEC ) stride = 2 * MAXSEC;

    say( "XTSECT %s, drive %c:, %d reads, start %lu, stride %lu, QPC %lu Hz\r\n", label,
         'A' + drive, count, start, stride, (DWORD)qpf.QuadPart );
    say( "  pass  sectors/read  us/read(mean)  min  max  us/sector\r\n" );
    for( pass = 0; pass < 3; ++pass ) {
        for( size = 1; size <= MAXSEC; size += MAXSEC - 1 ) {
            DWORD mn = 0xFFFFFFFF, mx = 0, sum_ms = 0, sum_us = 0, us, mean;
            DWORD base = start + (DWORD)pass * (DWORD)count * stride;
            int   i;

            for( i = 0; i < count; ++i ) {
                LARGE_INTEGER t0;

                QueryPerformanceCounter( &t0 );
                if( !read_abs( drive, base + (DWORD)i * stride + (size == 1 ? 0 : MAXSEC), size ) ) {
                    say( "FAIL: read of sector %lu\r\n", base + (DWORD)i * stride );
                    return;
                }
                us = us_since( &t0 );
                if( us < mn ) mn = us;
                if( us > mx ) mx = us;
                sum_us += us;
                if( sum_us >= 1000 ) { sum_ms += sum_us / 1000; sum_us %= 1000; }
            }
            mean = (sum_ms * 1000 + sum_us) / (DWORD)count;
            say( "  %4d  %12d  %13lu  %4lu  %4lu  %9lu%s\r\n", pass + 1, size, mean, mn, mx,
                 mean / (DWORD)size, mean / (DWORD)size < 1800 ? "  (cached?)" : "" );
        }
    }
    say( "\r\n" );
}

void entry( void )
{
    char *argv[8];
    int   argc;

    out_con = GetStdHandle( STD_OUTPUT_HANDLE );
    out_file = CreateFileA( "C:\\XTSECT.TXT", GENERIC_WRITE, 0, NULL, OPEN_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, NULL );
    SetFilePointer( out_file, 0, NULL, FILE_END );
    QueryPerformanceFrequency( &qpf );
    vwin32 = CreateFileA( "\\\\.\\vwin32", 0, 0, NULL, 0, FILE_FLAG_DELETE_ON_CLOSE, NULL );
    argc = split( GetCommandLineA(), argv, 8 );
    if( vwin32 == INVALID_HANDLE_VALUE ) {
        say( "cannot open VWIN32\r\n" );
    } else {
        run( argc, argv );
        CloseHandle( vwin32 );
    }
    CloseHandle( out_file );
    ExitProcess( 0 );
}
