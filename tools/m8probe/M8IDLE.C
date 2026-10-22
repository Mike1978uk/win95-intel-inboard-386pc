/* M8IDLE.EXE - wait until Windows 95 has finished starting up: the CPU use
 * System Monitor shows (HKEY_DYN_DATA PerfStats, KERNEL\CPUUsage, percent)
 * stays at or below a limit for a number of seconds in a row. A bench run
 * while the start-up programs still run is slow in every test.
 *   M8IDLE [limit [seconds [timeout]]]     default 10 % for 10 s, give up at 600 s
 * Exit code 0 when idle, 1 on timeout. Prints one line a second.
 * Build: wcl386 -q -bt=nt -l=nt -ox -fe=M8IDLE.EXE M8IDLE.C
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#define COUNTER "KERNEL\\CPUUsage"

static int read_stat( const char *key, DWORD *v )
{
    HKEY    h;
    DWORD   type;
    DWORD   size = sizeof( *v );
    LONG    r;

    if( RegOpenKeyEx( HKEY_DYN_DATA, key, 0, KEY_READ, &h ) != ERROR_SUCCESS ) {
        return( 0 );
    }
    r = RegQueryValueEx( h, COUNTER, NULL, &type, (BYTE *)v, &size );
    RegCloseKey( h );
    return( r == ERROR_SUCCESS );
}

int main( int argc, char **argv )
{
    DWORD   limit = argc > 1 ? atoi( argv[1] ) : 10;
    int     need = argc > 2 ? atoi( argv[2] ) : 10;
    int     timeout = argc > 3 ? atoi( argv[3] ) : 600;
    int     quiet = 0;
    int     t;
    DWORD   v;

    /* The counter only runs between StartStat and StopStat reads. */
    if( !read_stat( "PerfStats\\StartStat", &v ) ) {
        printf( "no %s counter\n", COUNTER );
        return( 1 );
    }
    for( t = 1; t <= timeout; ++t ) {
        Sleep( 1000 );
        if( !read_stat( "PerfStats\\StatData", &v ) ) {
            printf( "counter read failed\n" );
            break;
        }
        quiet = v <= limit ? quiet + 1 : 0;
        printf( "%3d s  cpu %3lu %%  quiet %d\n", t, v, quiet );
        if( quiet >= need ) {
            read_stat( "PerfStats\\StopStat", &v );
            return( 0 );
        }
    }
    read_stat( "PerfStats\\StopStat", &v );
    return( 1 );
}
