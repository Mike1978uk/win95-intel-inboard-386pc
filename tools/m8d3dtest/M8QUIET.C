/* M8QUIET.EXE - what is running on the card, and a bench with some of it stopped.
 *
 *   M8QUIET list
 *       Every process with its threads' base priorities, and memory, on stdout.
 *   M8QUIET run <names|-> <command line>
 *       Detaches from whoever started it, so the bench does not run under the agent's
 *       exec. Stops the processes named (comma separated, by file name, - for none), lets
 *       the machine settle, runs the command, starts the stopped ones again. The agent
 *       is one of them, so this logs to C:\V9XREMOTE\M8QUIET.TXT, not to stdout.
 *
 * Windows 9x has no per-thread CPU time (no GetThreadTimes), so taking a process away and
 * benching again is the only way to learn what it costs.
 *
 * Build: wcl386 -q -bt=nt -l=nt -ox -i=<watcom>h -i=<watcom>h\nt -fe=M8QUIET.EXE M8QUIET.C
 */

#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <string.h>

#define LOG_PATH    "C:\\V9XREMOTE\\M8QUIET.TXT"
#define DETACH_MS   5000                /* the host's exec sees us exit and hangs up */
#define SETTLE_MS   5000
#define BENCH_MS    600000UL            /* a hung bench must not leave the agent stopped */
#define MAX_STOPPED 16

static char stopped[MAX_STOPPED][MAX_PATH];
static int  stopped_n;

static const char *base_name( const char *path )
{
    const char *s = strrchr( path, '\\' );

    return( s != NULL ? s + 1 : path );
}

static void list( FILE *f )
{
    HANDLE          snap;
    PROCESSENTRY32  p;
    THREADENTRY32   t;
    MEMORYSTATUS    m;
    BOOL            more;
    BOOL            tmore;

    m.dwLength = sizeof( m );
    GlobalMemoryStatus( &m );
    fprintf( f, "memory phys %lu KB, free %lu KB, page file used %lu KB\n", m.dwTotalPhys / 1024,
             m.dwAvailPhys / 1024, (m.dwTotalPageFile - m.dwAvailPageFile) / 1024 );
    snap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS | TH32CS_SNAPTHREAD, 0 );
    if( snap == INVALID_HANDLE_VALUE ) {
        fprintf( f, "no snapshot, error %lu\n", GetLastError() );
        return;
    }
    p.dwSize = sizeof( p );
    for( more = Process32First( snap, &p ); more; more = Process32Next( snap, &p ) ) {
        fprintf( f, "%08lx class %2ld threads %2lu  %s\n   priorities", p.th32ProcessID, p.pcPriClassBase,
                 p.cntThreads, p.szExeFile );
        t.dwSize = sizeof( t );
        for( tmore = Thread32First( snap, &t ); tmore; tmore = Thread32Next( snap, &t ) ) {
            if( t.th32OwnerProcessID == p.th32ProcessID ) {
                fprintf( f, " %ld", t.tpBasePri + t.tpDeltaPri );
            }
        }
        fprintf( f, "\n" );
    }
    CloseHandle( snap );
}

static int named( const char *names, const char *file )
{
    char        one[MAX_PATH];
    const char  *e;
    size_t      n;

    while( *names != '\0' ) {
        e = strchr( names, ',' );
        n = e != NULL ? (size_t)(e - names) : strlen( names );
        if( n < sizeof( one ) ) {
            memcpy( one, names, n );
            one[n] = '\0';
            if( stricmp( one, file ) == 0 ) {
                return( 1 );
            }
        }
        names += n + (e != NULL);
    }
    return( 0 );
}

static void stop_named( FILE *f, const char *names )
{
    HANDLE          snap;
    HANDLE          h;
    PROCESSENTRY32  p;
    BOOL            more;

    snap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
    if( snap == INVALID_HANDLE_VALUE ) {
        return;
    }
    p.dwSize = sizeof( p );
    for( more = Process32First( snap, &p ); more; more = Process32Next( snap, &p ) ) {
        if( stopped_n < MAX_STOPPED && named( names, base_name( p.szExeFile ) ) ) {
            h = OpenProcess( PROCESS_TERMINATE, FALSE, p.th32ProcessID );
            if( h != NULL && TerminateProcess( h, 0 ) ) {
                strcpy( stopped[stopped_n++], p.szExeFile );
                fprintf( f, "stopped %s\n", p.szExeFile );
            } else {
                fprintf( f, "could not stop %s, error %lu\n", p.szExeFile, GetLastError() );
            }
            if( h != NULL ) {
                CloseHandle( h );
            }
        }
    }
    CloseHandle( snap );
    fflush( f );
}

static int start( char *cmd, DWORD flags, HANDLE *proc )
{
    STARTUPINFO         si;
    PROCESS_INFORMATION pi;

    memset( &si, 0, sizeof( si ) );
    si.cb = sizeof( si );
    if( !CreateProcess( NULL, cmd, NULL, NULL, FALSE, flags, NULL, NULL, &si, &pi ) ) {
        return( 0 );
    }
    CloseHandle( pi.hThread );
    if( proc != NULL ) {
        *proc = pi.hProcess;
    } else {
        CloseHandle( pi.hProcess );
    }
    return( 1 );
}

static void restart_stopped( FILE *f )
{
    char    cmd[MAX_PATH + 16];
    int     i;

    for( i = 0; i < stopped_n; ++i ) {
        strcpy( cmd, stopped[i] );
        /* The agent runs as a service process, and only says so on its command line. */
        if( stricmp( base_name( cmd ), "V9XAGNT.EXE" ) == 0 ) {
            strcat( cmd, " -service" );
        }
        fprintf( f, "%s %s\n", start( cmd, 0, NULL ) ? "started" : "COULD NOT START", cmd );
    }
    fflush( f );
}

static int run( char *names, char *cmd )
{
    FILE    *f;
    HANDLE  bench = NULL;
    DWORD   r;

    Sleep( DETACH_MS );
    f = fopen( LOG_PATH, "a" );
    if( f == NULL ) {
        return( 1 );
    }
    fprintf( f, "== stop %s, run %s\n", names, cmd );
    stop_named( f, names );
    Sleep( SETTLE_MS );
    list( f );
    fflush( f );
    if( start( cmd, 0, &bench ) ) {
        r = WaitForSingleObject( bench, BENCH_MS );
        if( r == WAIT_TIMEOUT ) {
            TerminateProcess( bench, 1 );
        }
        CloseHandle( bench );
        fprintf( f, "bench %s\n", r == WAIT_TIMEOUT ? "TIMED OUT, terminated" : "finished" );
    } else {
        fprintf( f, "could not start the bench, error %lu\n", GetLastError() );
    }
    fflush( f );
    restart_stopped( f );
    fprintf( f, "done\n\n" );
    fclose( f );
    return( 0 );
}

int main( int argc, char **argv )
{
    char    *cl = GetCommandLine();
    char    *rest;
    char    self[MAX_PATH];
    char    cmd[512];

    if( argc >= 2 && stricmp( argv[1], "list" ) == 0 ) {
        list( stdout );
        return( 0 );
    }
    /* Second instance: no handles shared with the first, so no pipe keeps exec waiting. */
    if( argc >= 4 && strcmp( argv[1], "detached" ) == 0 ) {
        rest = strstr( cl, argv[2] );
        rest = rest != NULL ? strchr( rest, ' ' ) : NULL;
        if( rest == NULL ) {
            return( 1 );
        }
        return( run( argv[2], rest + 1 ) );
    }
    if( argc >= 4 && stricmp( argv[1], "run" ) == 0 ) {
        rest = strstr( cl, argv[2] );
        rest = rest != NULL ? strchr( rest, ' ' ) : NULL;
        GetModuleFileName( NULL, self, sizeof( self ) );
        if( rest == NULL || strlen( self ) + strlen( argv[2] ) + strlen( rest ) + 16 > sizeof( cmd ) ) {
            return( 1 );
        }
        sprintf( cmd, "%s detached %s%s", self, argv[2], rest );
        /* A Win32 console on Windows 95 is a DOS VM (WINOA386.MOD), and one held open
         * through the bench would compete with what is being measured. */
        if( !start( cmd, DETACHED_PROCESS, NULL ) ) {
            printf( "could not detach, error %lu\n", GetLastError() );
            return( 1 );
        }
        printf( "detached; log %s\n", LOG_PATH );
        return( 0 );
    }
    printf( "M8QUIET list | M8QUIET run <names|-> <command line>\n" );
    return( 1 );
}
