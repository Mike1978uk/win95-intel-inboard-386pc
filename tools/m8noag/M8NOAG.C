/* M8NOAG.EXE - run the 5160 without the V9x agent, for whoever is sitting at it.
 *
 * The name it runs under picks the job, so a copy on the desktop needs no arguments:
 *   AGENTOFF.EXE   takes the agent out of the boot, and says what it read back
 *   AGENTON.EXE    puts it back
 *   <NAME>.EXE     runs the list C:\V9XREMOTE\<NAME>.LST, one program at a time
 * A first argument of "quiet" drops the message boxes, for a remote test of the tool.
 *
 * The off switch is a program because REGEDIT on Windows 95 4.00.950 a ignores both delete
 * forms in a .REG file, "value"=- and [-key], and reports success.
 *
 * It is a GUI program because a Win32 console on Windows 95 is a DOS VM, and one held open
 * through a bench would compete with what is being measured.
 *
 * List lines: a command line, run to its end. A leading & starts it and moves on. # is a
 * comment. Log: C:\V9XREMOTE\NOAG.TXT, each line forced to disk before the step it names
 * starts, so after a hang the last line is the step that hung.
 *
 * Build: wcl386 -q -bt=nt -l=nt_win -ox -i=<watcom>h -i=<watcom>h\nt -fe=M8NOAG.EXE M8NOAG.C
 */

#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <string.h>

#define RUN_KEY     "Software\\Microsoft\\Windows\\CurrentVersion\\RunServices"
#define AGENT_VALUE "V9xRemoteAgent"
#define AGENT_CMD   "C:\\V9XREMOTE\\V9XAGNT.EXE -service"
#define AGENT_FILE  "V9XAGNT.EXE"
#define LIST_DIR    "C:\\V9XREMOTE\\"
#define LOG_PATH    "C:\\V9XREMOTE\\NOAG.TXT"
#define WORK_DIR    "C:\\"
#define STEP_MS     (15UL * 60UL * 1000UL)  /* a hung step must not stop the rest of the list */
#define TITLE       "M8NOAG"

static int  quiet;

static void say( const char *text )
{
    if( !quiet ) {
        MessageBox( NULL, text, TITLE, MB_OK | MB_ICONINFORMATION );
    }
}

static int agent_running( void )
{
    HANDLE          snap;
    PROCESSENTRY32  p;
    BOOL            more;
    const char      *s;
    int             found = 0;

    snap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
    if( snap == INVALID_HANDLE_VALUE ) {
        return( -1 );
    }
    p.dwSize = sizeof( p );
    for( more = Process32First( snap, &p ); more; more = Process32Next( snap, &p ) ) {
        s = strrchr( p.szExeFile, '\\' );
        if( stricmp( s != NULL ? s + 1 : p.szExeFile, AGENT_FILE ) == 0 ) {
            found = 1;
        }
    }
    CloseHandle( snap );
    return( found );
}

/* Returns 1 if the boot will start the agent, 0 if not, -1 if the key cannot be read. */
static int agent_in_boot( void )
{
    HKEY    k;
    char    v[MAX_PATH];
    DWORD   n = sizeof( v );
    LONG    r;

    if( RegOpenKeyEx( HKEY_LOCAL_MACHINE, RUN_KEY, 0, KEY_READ, &k ) != ERROR_SUCCESS ) {
        return( -1 );
    }
    r = RegQueryValueEx( k, AGENT_VALUE, NULL, NULL, (BYTE *)v, &n );
    RegCloseKey( k );
    return( r == ERROR_SUCCESS ? 1 : 0 );
}

static int set_agent( int on )
{
    HKEY    k;
    char    msg[256];
    int     now;

    if( RegOpenKeyEx( HKEY_LOCAL_MACHINE, RUN_KEY, 0, KEY_ALL_ACCESS, &k ) != ERROR_SUCCESS ) {
        say( "Could not open the RunServices key. Nothing changed." );
        return( 2 );
    }
    if( on ) {
        RegSetValueEx( k, AGENT_VALUE, 0, REG_SZ, (const BYTE *)AGENT_CMD, sizeof( AGENT_CMD ) );
    } else {
        RegDeleteValue( k, AGENT_VALUE );
    }
    RegCloseKey( k );
    now = agent_in_boot();
    sprintf( msg, "%s\n\nStarts at the next restart: %s\nRunning now: %s\n\nUse Start > Shut Down > Restart.",
             now == on ? "Done." : "IT DID NOT TAKE.", now == 1 ? "YES" : "NO",
             agent_running() == 1 ? "YES" : "NO" );
    say( msg );
    return( now == on ? 0 : 1 );
}

static HANDLE   log_h = INVALID_HANDLE_VALUE;

static void log_line( const char *text )
{
    DWORD   n;

    if( log_h != INVALID_HANDLE_VALUE ) {
        WriteFile( log_h, text, strlen( text ), &n, NULL );
        WriteFile( log_h, "\r\n", 2, &n, NULL );
        FlushFileBuffers( log_h );
    }
}

static void log_state( const char *what, const char *cmd )
{
    MEMORYSTATUS    m;
    char            line[512];

    m.dwLength = sizeof( m );
    GlobalMemoryStatus( &m );
    sprintf( line, "%10lu ms  free %lu KB  paged %lu KB  %s %.300s", GetTickCount(), m.dwAvailPhys / 1024,
             (m.dwTotalPageFile - m.dwAvailPageFile) / 1024, what, cmd );
    log_line( line );
}

static void run_step( char *cmd, int wait )
{
    STARTUPINFO         si;
    PROCESS_INFORMATION pi;
    DWORD               t0;
    DWORD               r;
    DWORD               code = 0;
    char                line[64];

    log_state( wait ? "start" : "start, not waited for:", cmd );
    memset( &si, 0, sizeof( si ) );
    si.cb = sizeof( si );
    t0 = GetTickCount();
    if( !CreateProcess( NULL, cmd, NULL, NULL, FALSE, 0, NULL, WORK_DIR, &si, &pi ) ) {
        sprintf( line, "  COULD NOT START, error %lu", GetLastError() );
        log_line( line );
        return;
    }
    CloseHandle( pi.hThread );
    if( wait ) {
        r = WaitForSingleObject( pi.hProcess, STEP_MS );
        if( r == WAIT_TIMEOUT ) {
            TerminateProcess( pi.hProcess, 1 );
            log_line( "  TIMED OUT, terminated" );
        }
        GetExitCodeProcess( pi.hProcess, &code );
        sprintf( line, "  exit %lu after %lu ms", code, GetTickCount() - t0 );
        log_line( line );
    }
    CloseHandle( pi.hProcess );
}

static int run_list( const char *name )
{
    char    path[MAX_PATH];
    char    line[512];
    char    head[128];
    char    *s;
    char    *e;
    FILE    *f;
    int     steps = 0;

    sprintf( path, "%s%.32s.LST", LIST_DIR, name );
    f = fopen( path, "r" );
    if( f == NULL ) {
        sprintf( line, "No list at %s.", path );
        say( line );
        return( 2 );
    }
    log_h = CreateFile( LOG_PATH, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
    if( log_h != INVALID_HANDLE_VALUE ) {
        SetFilePointer( log_h, 0, NULL, FILE_END );
    }
    sprintf( head, "== %.32s  agent running: %s  agent in boot: %s", name, agent_running() == 1 ? "YES" : "no",
             agent_in_boot() == 1 ? "YES" : "no" );
    log_line( head );
    while( fgets( line, sizeof( line ), f ) != NULL ) {
        s = line + strspn( line, " \t" );
        e = s + strcspn( s, "\r\n" );
        *e = '\0';
        if( *s == '\0' || *s == '#' ) {
            continue;
        }
        if( *s == '&' ) {
            run_step( s + 1, 0 );
        } else {
            run_step( s, 1 );
        }
        ++steps;
    }
    fclose( f );
    log_state( "finished", name );
    log_line( "" );
    if( log_h != INVALID_HANDLE_VALUE ) {
        CloseHandle( log_h );
    }
    sprintf( line, "%s finished: %d steps.\n\nLog: %s", name, steps, LOG_PATH );
    say( line );
    return( 0 );
}

int PASCAL WinMain( HINSTANCE inst, HINSTANCE prev, LPSTR args, int show )
{
    char    self[MAX_PATH];
    char    *name;
    char    *dot;

    (void)prev;
    (void)show;
    quiet = stricmp( args, "quiet" ) == 0;
    GetModuleFileName( inst, self, sizeof( self ) );
    name = strrchr( self, '\\' );
    name = name != NULL ? name + 1 : self;
    dot = strrchr( name, '.' );
    if( dot != NULL ) {
        *dot = '\0';
    }
    if( stricmp( name, "AGENTOFF" ) == 0 ) {
        return( set_agent( 0 ) );
    }
    if( stricmp( name, "AGENTON" ) == 0 ) {
        return( set_agent( 1 ) );
    }
    return( run_list( name ) );
}
