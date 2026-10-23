/* DAYNAPRB: prove that Windows 95's ASPI layer carries DaynaPORT SCSI/Link
 * commands to a BlueSCSI behind T130.MPD, and measure what reading packets at
 * a fixed interval costs, so a protected-mode NDIS driver can pick its interval
 * from numbers rather than guesses (#50). Writes C:\DAYNAPRB.TXT.
 *
 *   DAYNAPRB [ha target lun] [-arp a.b.c.d] [-s seconds]
 *
 * Without ha/target/lun it sends INQUIRY to LUN 0 of every target and uses the
 * first one whose vendor is "Dayna". -arp sends an ARP probe (sender IP 0.0.0.0,
 * so no address is needed) for a.b.c.d, normally the router, and waits for the
 * reply: one frame out and one in proves both directions.
 *
 * No C runtime: only KERNEL32 and USER32 calls that Windows 95 has, so a
 * cross-compiler's CRT cannot import something the 5160 lacks. WNASPI32.DLL is
 * loaded at run time. */
#include <windows.h>

#pragma pack(1)
typedef struct {
    BYTE  Cmd, Status, HaId, Flags;
    DWORD HdrRsvd;
    BYTE  Target, Lun;
    WORD  Rsvd1;
    DWORD BufLen;
    BYTE  *BufPointer;
    BYTE  SenseLen, CDBLen, HaStat, TargStat;
    void  *PostProc;
    BYTE  Rsvd2[20];
    BYTE  CDB[16];
    BYTE  Sense[18];
} SRB_EXEC;
#pragma pack()

#define SC_EXEC_SCSI_CMD    0x02
#define SRB_DIR_IN          0x08
#define SRB_DIR_OUT         0x10
#define SS_PENDING          0x00
#define SS_COMP             0x01

#define READ_SIZE           4096    /* room for two full frames per READ */
#define MORE_FLAG           0x10    /* header byte 5: another frame follows */

typedef DWORD (__cdecl *ASPI_INFO)( void );
typedef DWORD (__cdecl *ASPI_SEND)( void * );

static ASPI_SEND send_srb;
static HANDLE    out_file, out_con;
static LARGE_INTEGER qpf;
static BYTE      rbuf[READ_SIZE];

/* -nostdlib: GCC may still emit calls to these. */
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
    WriteFile( out_file, line, lstrlenA( line ), &n, NULL );
}

static DWORD us_since( LARGE_INTEGER *t0 )
{
    LARGE_INTEGER t;

    QueryPerformanceCounter( &t );
    return( (DWORD)((t.QuadPart - t0->QuadPart) * 1000000 / qpf.QuadPart) );
}

/* One command, spun to completion: a Sleep would round every timing up to the
 * Windows tick (13.7 ms on the 5160). The spin costs CPU, not bus time. */
static int exec( int ha, int t, int lun, BYTE *cdb, BYTE dir, BYTE *buf, DWORD len,
                 SRB_EXEC *s, DWORD *us )
{
    LARGE_INTEGER t0;

    memset( s, 0, sizeof( *s ) );
    s->Cmd = SC_EXEC_SCSI_CMD;
    s->HaId = (BYTE)ha;
    s->Flags = dir;
    s->Target = (BYTE)t;
    s->Lun = (BYTE)lun;
    s->BufLen = len;
    s->BufPointer = buf;
    s->SenseLen = 14;
    s->CDBLen = 6;
    memcpy( s->CDB, cdb, 6 );
    /* T130.MPD sends no IDENTIFY message, so CDB byte 1 is the only place the
     * LUN reaches the target. */
    s->CDB[1] = (BYTE)((s->CDB[1] & 0x1F) | (lun << 5));
    QueryPerformanceCounter( &t0 );
    send_srb( s );
    while( *(volatile BYTE *)&s->Status == SS_PENDING && us_since( &t0 ) < 5000000 ) {
        Sleep( 0 );
    }
    if( us != NULL ) {
        *us = us_since( &t0 );
    }
    return( s->Status == SS_COMP );
}

static int parse_ip( const char *p, BYTE *ip )
{
    int i, v;

    for( i = 0; i < 4; ++i ) {
        if( *p < '0' || *p > '9' ) return( 0 );
        for( v = 0; *p >= '0' && *p <= '9'; ++p ) v = v * 10 + (*p - '0');
        if( v > 255 ) return( 0 );
        ip[i] = (BYTE)v;
        if( i < 3 && *p++ != '.' ) return( 0 );
    }
    return( 1 );
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

static int num( const char *p )
{
    int v = 0;
    while( *p >= '0' && *p <= '9' ) v = v * 10 + (*p++ - '0');
    return( v );
}

/* Walk the frames in one READ's data: 6-byte header (length, flags), frame. */
static int count_frames( DWORD got, DWORD *bytes, int *more )
{
    DWORD off = 0;
    int   n = 0;

    *more = 0;
    while( off + 6 <= got ) {
        DWORD len = ((DWORD)rbuf[off] << 8) | rbuf[off + 1];
        BYTE  fl = rbuf[off + 5];

        if( len == 0 ) break;
        ++n;
        *bytes += len;
        off += 6 + len;
        if( !(fl & MORE_FLAG) ) break;
        *more = 1;
    }
    return( n );
}

static void run( int argc, char **argv )
{
    HINSTANCE dll = LoadLibraryA( "WNASPI32.DLL" );
    ASPI_INFO info;
    SRB_EXEC  s;
    BYTE      inq[36], mac[18], frame[60], arp_ip[4];
    BYTE      c_inq[6]  = { 0x12, 0, 0, 0, 36, 0 };
    BYTE      c_mac[6]  = { 0x09, 0, 0, 0, 18, 0 };
    BYTE      c_on[6]   = { 0x0E, 0, 0, 0, 0, 0x80 };
    BYTE      c_off[6]  = { 0x0E, 0, 0, 0, 0, 0x00 };
    BYTE      c_read[6] = { 0x08, 0, 0, READ_SIZE >> 8, READ_SIZE & 0xFF, 0xC0 };
    BYTE      c_send[6] = { 0x0A, 0, 0, 0, sizeof( frame ), 0x00 };
    static const int intervals[] = { 0, 14, 28, 55, 110, 220 };
    int       ha = -1, t = -1, lun = 0, secs = 10, do_arp = 0, nha, i;
    DWORD     r, us;

    for( i = 1; i < argc; ++i ) {
        if( lstrcmpiA( argv[i], "-arp" ) == 0 && i + 1 < argc ) {
            do_arp = parse_ip( argv[++i], arp_ip );
            if( !do_arp ) { say( "bad IP %s\r\n", argv[i] ); return; }
        } else if( lstrcmpiA( argv[i], "-s" ) == 0 && i + 1 < argc ) {
            secs = num( argv[++i] );
            if( secs < 1 ) secs = 1;
        } else if( i + 2 < argc && ha < 0 ) {
            ha = num( argv[i] ); t = num( argv[i + 1] ); lun = num( argv[i + 2] );
            i += 2;
        }
    }

    if( dll == NULL ) { say( "WNASPI32.DLL not loaded\r\n" ); return; }
    info = (ASPI_INFO)GetProcAddress( dll, "GetASPI32SupportInfo" );
    send_srb = (ASPI_SEND)GetProcAddress( dll, "SendASPI32Command" );
    if( info == NULL || send_srb == NULL ) { say( "ASPI entry points missing\r\n" ); return; }
    r = info();
    nha = (int)(r & 0xFF);
    say( "ASPI status %02X, adapters %d, QPC %lu Hz\r\n", (r >> 8) & 0xFF, nha, (DWORD)qpf.QuadPart );

    /* 1. Find the device. */
    if( ha < 0 ) {
        int a, b;
        for( a = 0; a < nha && ha < 0; ++a ) {
            for( b = 0; b < 8 && ha < 0; ++b ) {
                memset( inq, 0, sizeof( inq ) );
                if( exec( a, b, 0, c_inq, SRB_DIR_IN, inq, 36, &s, NULL ) &&
                    inq[8] == 'D' && inq[9] == 'a' && inq[10] == 'y' && inq[11] == 'n' && inq[12] == 'a' ) {
                    ha = a; t = b;
                }
            }
        }
        if( ha < 0 ) { say( "FAIL: no target answers INQUIRY with vendor Dayna on LUN 0\r\n" ); return; }
    }
    memset( inq, 0, sizeof( inq ) );
    if( !exec( ha, t, lun, c_inq, SRB_DIR_IN, inq, 36, &s, &us ) ) {
        say( "FAIL: INQUIRY to %d:%d:%d, status %02X sense %X/%02X\r\n", ha, t, lun,
             s.Status, s.Sense[2] & 0x0F, s.Sense[12] );
        return;
    }
    say( "device %d:%d:%d type %02X \"%.8s\" \"%.16s\" \"%.4s\", INQUIRY %lu us\r\n",
         ha, t, lun, inq[0] & 0x1F, inq + 8, inq + 16, inq + 32, us );

    /* 2. MAC address, then enable the interface. */
    memset( mac, 0, sizeof( mac ) );
    if( !exec( ha, t, lun, c_mac, SRB_DIR_IN, mac, 18, &s, &us ) ) {
        say( "FAIL: read MAC (09h), status %02X\r\n", s.Status );
        return;
    }
    say( "MAC %02X:%02X:%02X:%02X:%02X:%02X\r\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );
    if( !exec( ha, t, lun, c_on, 0, NULL, 0, &s, &us ) ) {
        say( "FAIL: enable (0Eh), status %02X\r\n", s.Status );
        return;
    }
    say( "interface enabled\r\n" );

    /* 3. One frame out, one in. */
    if( do_arp ) {
        LARGE_INTEGER t0;
        int got_reply = 0;

        memset( frame, 0, sizeof( frame ) );
        memset( frame, 0xFF, 6 );
        memcpy( frame + 6, mac, 6 );
        frame[12] = 0x08; frame[13] = 0x06;          /* ARP */
        frame[15] = 1; frame[16] = 0x08;              /* Ethernet, IPv4 */
        frame[18] = 6; frame[19] = 4; frame[21] = 1;  /* request */
        memcpy( frame + 22, mac, 6 );                 /* sender IP stays 0.0.0.0 */
        memcpy( frame + 38, arp_ip, 4 );
        if( !exec( ha, t, lun, c_send, SRB_DIR_OUT, frame, sizeof( frame ), &s, &us ) ) {
            say( "FAIL: send (0Ah), status %02X\r\n", s.Status );
        } else {
            say( "ARP probe for %d.%d.%d.%d sent, %lu us\r\n", arp_ip[0], arp_ip[1], arp_ip[2], arp_ip[3], us );
            QueryPerformanceCounter( &t0 );
            while( !got_reply && us_since( &t0 ) < 3000000 ) {
                DWORD off = 0;

                if( !exec( ha, t, lun, c_read, SRB_DIR_IN, rbuf, READ_SIZE, &s, NULL ) ) break;
                while( off + 6 <= READ_SIZE ) {
                    DWORD len = ((DWORD)rbuf[off] << 8) | rbuf[off + 1];
                    BYTE  fl = rbuf[off + 5];
                    BYTE *f = rbuf + off + 6;

                    if( len == 0 ) break;
                    if( len >= 42 && f[12] == 0x08 && f[13] == 0x06 && f[21] == 2 &&
                        f[28] == arp_ip[0] && f[29] == arp_ip[1] && f[30] == arp_ip[2] && f[31] == arp_ip[3] ) {
                        say( "PASS: ARP reply from %02X:%02X:%02X:%02X:%02X:%02X after %lu us\r\n",
                             f[22], f[23], f[24], f[25], f[26], f[27], us_since( &t0 ) );
                        got_reply = 1;
                        break;
                    }
                    off += 6 + len;
                    if( !(fl & MORE_FLAG) ) break;
                }
            }
            if( !got_reply ) say( "no ARP reply within 3 s\r\n" );
        }
    }

    /* 4. Read at fixed intervals: what each costs and what it misses. */
    say( "interval  reads  empty  frames  bytes  more  cmd_us(min/mean/max)  busy%%\r\n" );
    for( i = 0; i < (int)(sizeof( intervals ) / sizeof( intervals[0] )); ++i ) {
        LARGE_INTEGER t0;
        DWORD reads = 0, empty = 0, frames = 0, bytes = 0, mores = 0;
        DWORD cmin = 0xFFFFFFFF, cmax = 0, csum_ms = 0, csum_us = 0, elapsed;

        QueryPerformanceCounter( &t0 );
        while( us_since( &t0 ) < (DWORD)secs * 1000000 ) {
            int n, more;

            if( !exec( ha, t, lun, c_read, SRB_DIR_IN, rbuf, READ_SIZE, &s, &us ) ) {
                say( "FAIL: read (08h), status %02X sense %X/%02X\r\n", s.Status,
                     s.Sense[2] & 0x0F, s.Sense[12] );
                break;
            }
            ++reads;
            if( us < cmin ) cmin = us;
            if( us > cmax ) cmax = us;
            csum_us += us;
            if( csum_us >= 1000 ) { csum_ms += csum_us / 1000; csum_us %= 1000; }
            n = count_frames( READ_SIZE, &bytes, &more );
            if( n == 0 ) ++empty;
            frames += n;
            mores += more;
            if( intervals[i] ) Sleep( intervals[i] );
        }
        elapsed = us_since( &t0 ) / 1000;
        say( "%5d ms  %5lu  %5lu  %6lu  %6lu  %4lu  %lu/%lu/%lu  %lu\r\n", intervals[i], reads, empty,
             frames, bytes, mores, reads ? cmin : 0, reads ? (csum_ms * 1000 + csum_us) / reads : 0,
             cmax, elapsed ? csum_ms * 100 / elapsed : 0 );
    }

    exec( ha, t, lun, c_off, 0, NULL, 0, &s, NULL );
    say( "interface disabled, done\r\n" );
}

void entry( void )
{
    char *argv[16];
    int   argc;

    out_con = GetStdHandle( STD_OUTPUT_HANDLE );
    out_file = CreateFileA( "C:\\DAYNAPRB.TXT", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, NULL );
    QueryPerformanceFrequency( &qpf );
    argc = split( GetCommandLineA(), argv, 16 );
    run( argc, argv );
    CloseHandle( out_file );
    ExitProcess( 0 );
}
