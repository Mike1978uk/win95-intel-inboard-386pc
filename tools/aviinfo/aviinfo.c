/* AVIINFO: what an AVI file asks of this machine, and which video codecs
 * Windows 95 here can use to play it.
 *
 *   AVIINFO file.avi
 *
 * Prints, per stream: video codec (FourCC), size, bit depth, frame rate and
 * frames; audio format, rate, channels and bits. Then the data rate the file
 * needs (bytes a second from the disk) and what one uncompressed 8 bpp frame
 * of that size costs on the way to the Mach8 (3.00 us a byte, M8UPRATE). Then
 * the VIDC entries in SYSTEM.INI [drivers32] and [drivers], the codecs Video
 * for Windows can load. Appends to C:\AVIINFO.TXT.
 *
 * Reads only the start of the file (the 'hdrl' list). Read only.
 * No C runtime; KERNEL32 and USER32!wvsprintfA only. Built with -DHOSTTEST it
 * runs on a PC against a file, for checking the parser. */
#ifdef HOSTTEST
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int DWORD;
#else
#include <windows.h>
#endif

#define HDR_BYTES 65536

static BYTE hdr[HDR_BYTES];

#ifdef HOSTTEST
static void say( const char *fmt, ... )
{
    va_list ap;
    va_start( ap, fmt );
    vprintf( fmt, ap );
    va_end( ap );
}
#else
static HANDLE out_file, out_con;

void *memset( void *d, int c, unsigned n )
{
    BYTE *p = d;
    while( n-- ) *p++ = (BYTE)c;
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
#endif

static DWORD rd32( const BYTE *p ) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((DWORD)p[3] << 24); }
static WORD  rd16( const BYTE *p ) { return (WORD)(p[0] | (p[1] << 8)); }
static int   is4( const BYTE *p, const char *s ) { return p[0] == s[0] && p[1] == s[1] && p[2] == s[2] && p[3] == s[3]; }

/* FourCC as printable text; codes below 32 (e.g. BI_RGB = 0) print as numbers. */
static void fourcc( DWORD v, char *out )
{
    int i;

    if( v < 32 ) {
        out[0] = (char)('0' + (int)v); out[1] = 0;
        if( v == 0 ) { out[0] = 'R'; out[1] = 'G'; out[2] = 'B'; out[3] = 0; }
        if( v == 1 ) { out[0] = 'R'; out[1] = 'L'; out[2] = 'E'; out[3] = '8'; out[4] = 0; }
        if( v == 2 ) { out[0] = 'R'; out[1] = 'L'; out[2] = 'E'; out[3] = '4'; out[4] = 0; }
        return;
    }
    for( i = 0; i < 4; ++i ) {
        BYTE c = (BYTE)(v >> (8 * i));
        out[i] = (c >= 32 && c < 127) ? (char)c : '?';
    }
    out[4] = 0;
}

static const char *known( const char *f )
{
    static const struct { const char *cc, *name; } k[] = {
        { "RGB",  "uncompressed RGB: no codec, the most bytes per frame" },
        { "RLE8", "Microsoft RLE, 8 bpp" },
        { "CRAM", "Microsoft Video 1" }, { "cram", "Microsoft Video 1" },
        { "MSVC", "Microsoft Video 1" }, { "msvc", "Microsoft Video 1" },
        { "cvid", "Cinepak" }, { "CVID", "Cinepak" },
        { "IV31", "Indeo 3.1" }, { "IV32", "Indeo 3.2" }, { "iv32", "Indeo 3.2" },
        { "YUY2", "uncompressed YUV 4:2:2" }, { "YVU9", "Intel YVU9 (planar YUV, 9 bits a pixel)" },
        { "MJPG", "Motion JPEG" }, { "mjpg", "Motion JPEG" },
        { "IJPG", "Intel JPEG" }, { "Y41P", "YUV 4:1:1 packed" },
    };
    unsigned i;

    for( i = 0; i < sizeof( k ) / sizeof( k[0] ); ++i ) {
        const char *a = k[i].cc, *b = f;
        while( *a && *a == *b ) { ++a; ++b; }
        if( *a == 0 && *b == 0 ) return( k[i].name );
    }
    return( "not one this tool knows" );
}

/* Walk the RIFF header: avih, then each strl's strh and strf. */
static void parse( const BYTE *b, DWORD n, DWORD file_bytes )
{
    DWORD off = 12, us_frame = 0, frames = 0, w = 0, h = 0;
    int   stream = 0;

    if( n < 12 || !is4( b, "RIFF" ) || !is4( b + 8, "AVI " ) ) { say( "not an AVI (no RIFF/AVI header)\r\n" ); return; }
    while( off + 8 <= n ) {
        const BYTE *c = b + off;
        DWORD size = rd32( c + 4 );

        if( is4( c, "LIST" ) && off + 12 <= n && (is4( c + 8, "hdrl" ) || is4( c + 8, "strl" )) ) {
            off += 12;                      /* descend into the list */
            continue;
        }
        if( is4( c, "avih" ) && off + 8 + 40 <= n ) {
            us_frame = rd32( c + 8 );
            frames = rd32( c + 8 + 16 );
            w = rd32( c + 8 + 32 );
            h = rd32( c + 8 + 36 );
            say( "file: %u bytes, %u x %u, %u frames, %u us a frame, %u streams\r\n",
                 file_bytes, w, h, frames, us_frame, rd32( c + 8 + 24 ) );
        } else if( is4( c, "strh" ) && off + 8 + 36 <= n ) {
            char ty[5], hd[5];
            DWORD scale = rd32( c + 8 + 20 ), rate = rd32( c + 8 + 24 );

            fourcc( rd32( c + 8 ), ty );
            fourcc( rd32( c + 8 + 4 ), hd );
            if( rd32( c + 8 + 4 ) == 0 ) { hd[0] = '-'; hd[1] = 0; }   /* no handler named */
            ++stream;
            say( "stream %d: type %s, handler %s, rate %u/%u, length %u\r\n", stream, ty, hd,
                 rate, scale ? scale : 1, rd32( c + 8 + 32 ) );
        } else if( is4( c, "strf" ) && off + 8 + 16 <= n ) {
            const BYTE *f = c + 8;

            if( size >= 40 && rd32( f ) >= 40 ) {          /* BITMAPINFOHEADER */
                char cc[5];
                DWORD bw = rd32( f + 4 ), bh = rd32( f + 8 );

                fourcc( rd32( f + 16 ), cc );
                say( "  video: codec %s (%s), %u x %u, %u bits a pixel\r\n", cc, known( cc ),
                     bw, bh, rd16( f + 14 ) );
                if( bw && bh ) {
                    DWORD px = bw * (bh & 0x7FFFFFFF);
                    say( "  one 8 bpp frame of that size to the Mach8: %u bytes, %u ms at 3.00 us a byte\r\n",
                         px, px * 3 / 1000 );
                }
            } else {                                        /* WAVEFORMATEX */
                WORD tag = rd16( f );
                say( "  audio: format %u (%s), %u Hz, %u channel(s), %u bits, %u bytes a second\r\n",
                     tag, tag == 1 ? "PCM" : tag == 2 ? "MS ADPCM" : tag == 0x11 ? "IMA ADPCM" :
                     tag == 0x55 ? "MPEG layer 3" : tag == 0x50 ? "MPEG" : "other",
                     rd32( f + 4 ), rd16( f + 2 ), size >= 16 ? rd16( f + 14 ) : 0, rd32( f + 8 ) );
            }
        } else if( is4( c, "LIST" ) && off + 12 <= n && is4( c + 8, "movi" ) ) {
            break;                          /* the frames: the header is done */
        }
        off += 8 + size + (size & 1);
    }
    if( us_frame && frames ) {
        DWORD secs10 = (DWORD)((frames * (double)us_frame) / 100000.0);   /* tenths */
        say( "plays for %u.%u s; needs %u bytes a second from the disk (XT-CF reads ~385,000)\r\n",
             secs10 / 10, secs10 % 10, secs10 ? (DWORD)((double)file_bytes * 10 / secs10) : 0 );
    }
}

#ifdef HOSTTEST
int main( int argc, char **argv )
{
    FILE *f;
    DWORD n, total;

    if( argc < 2 || (f = fopen( argv[1], "rb" )) == NULL ) { printf( "usage: aviinfo file.avi\n" ); return 1; }
    n = (DWORD)fread( hdr, 1, HDR_BYTES, f );
    fseek( f, 0, SEEK_END );
    total = (DWORD)ftell( f );
    fclose( f );
    parse( hdr, n, total );
    return 0;
}
#else
/* VIDC.* lines from one SYSTEM.INI section. */
static void codecs( const char *section )
{
    static char buf[8192];
    DWORD n = GetPrivateProfileSectionA( section, buf, sizeof( buf ), "SYSTEM.INI" );
    char *p = buf;
    int   any = 0;

    say( "SYSTEM.INI [%s] video codecs:\r\n", section );
    while( p < buf + n && *p ) {
        if( (p[0] == 'v' || p[0] == 'V') && (p[1] == 'i' || p[1] == 'I') &&
            (p[2] == 'd' || p[2] == 'D') && (p[3] == 'c' || p[3] == 'C') ) {
            say( "  %s\r\n", p );
            any = 1;
        }
        p += lstrlenA( p ) + 1;
    }
    if( !any ) say( "  (none)\r\n" );
}

void entry( void )
{
    char  *p = GetCommandLineA(), *name;
    HANDLE f;
    DWORD  n = 0, total;

    out_con = GetStdHandle( STD_OUTPUT_HANDLE );
    out_file = CreateFileA( "C:\\AVIINFO.TXT", GENERIC_WRITE, 0, NULL, OPEN_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, NULL );
    SetFilePointer( out_file, 0, NULL, FILE_END );

    if( *p == '"' ) { ++p; while( *p && *p != '"' ) ++p; if( *p ) ++p; }
    else while( *p && *p != ' ' ) ++p;
    while( *p == ' ' ) ++p;
    name = p;
    if( *name == '"' ) { ++name; p = name; while( *p && *p != '"' ) ++p; *p = 0; }

    if( *name ) {
        say( "AVIINFO %s\r\n", name );
        f = CreateFileA( name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
        if( f == INVALID_HANDLE_VALUE ) {
            say( "cannot open the file\r\n" );
        } else {
            total = GetFileSize( f, NULL );
            ReadFile( f, hdr, HDR_BYTES, &n, NULL );
            CloseHandle( f );
            parse( hdr, n, total );
        }
    } else {
        say( "AVIINFO (no file given: codecs only)\r\n" );
    }
    codecs( "drivers32" );
    codecs( "drivers" );
    say( "\r\n" );
    CloseHandle( out_file );
    ExitProcess( 0 );
}
#endif
