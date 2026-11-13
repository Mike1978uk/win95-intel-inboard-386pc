/* M8MON: what the display driver does over time, while another program runs.
 *
 * Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
 *
 *   M8MON <seconds> <label>
 *
 * Every half second, the change since the last sample in M8DD32.DLL's m8d3d_stat (8 bpp Lock,
 * Unlock, Flip; shared memory, so any process can read it) and in the 16-bit driver's m8_stats
 * (GDI calls that reached the screen, by entry point; escape M8_ESC_STATS). Quiet samples are not
 * written. Appends to C:\M8MON.TXT. Move times are clock ticks; in a bed they are not the card's,
 * the counts are.
 *
 * Build: wcl386 -q -bt=nt -l=nt_win -ox -i=<watcom>h -i=<watcom>h\nt "-fe=M8MON.EXE" M8MON.C
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define M8_DLL_BASE     0xB0C00000UL
#define STAT_N          88
#define SAMPLE_MS       500

#define M8_ESC_STATS    0x4D41
#define M8_STAT_COUNT   20

static const char *st_names[M8_STAT_COUNT] = { "bitblt", "output", "pixel", "strblt", "exttextout", "scanlr",
                                               "stretchblt", "stretchdib", "dibtodev", "fastborder", "updatecol",
                                               "fill_hw", "copy_hw", "readback", "text_hw", "text_fail",
                                               "run_fill", "covered", "line_hw", "cur_hide" };

static const void *find_export( const char *name )
{
    const BYTE  *b = (const BYTE *)M8_DLL_BASE;
    const BYTE  *pe;
    const BYTE  *ex;
    const DWORD *names;
    const WORD  *ords;
    const DWORD *funcs;
    DWORD       i;

    if( IsBadReadPtr( b, 0x400 ) ) {
        return( NULL );
    }
    pe = b + *(const DWORD *)(b + 0x3C);
    ex = b + *(const DWORD *)(pe + 0x78);
    names = (const DWORD *)(b + *(const DWORD *)(ex + 32));
    ords = (const WORD *)(b + *(const DWORD *)(ex + 36));
    funcs = (const DWORD *)(b + *(const DWORD *)(ex + 28));
    for( i = 0; i < *(const DWORD *)(ex + 24); ++i ) {
        if( strcmp( (const char *)(b + names[i]), name ) == 0 ) {
            return( b + funcs[ords[i]] );
        }
    }
    return( NULL );
}

static int gdi_read( DWORD *t )
{
    HDC     dc = GetDC( NULL );
    int     ok;

    memset( t, 0, M8_STAT_COUNT * sizeof( DWORD ) );
    ok = ExtEscape( dc, M8_ESC_STATS, 0, NULL, M8_STAT_COUNT * sizeof( DWORD ), (LPSTR)t ) > 0;
    ReleaseDC( NULL, dc );
    return( ok );
}

int PASCAL WinMain( HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show )
{
    static DWORD    st0[STAT_N];
    static DWORD    st1[STAT_N];
    static DWORD    g0[M8_STAT_COUNT];
    static DWORD    g1[M8_STAT_COUNT];
    const DWORD     *st;
    FILE            *o;
    LARGE_INTEGER   f;
    DWORD           start;
    DWORD           end;
    int             secs;
    int             havegdi;
    int             i;
    char            line[1024];
    char            *p;

    (void)inst;
    (void)prev;
    (void)show;
    secs = atoi( cmd );
    if( secs <= 0 ) {
        secs = 300;
    }
    while( *cmd != '\0' && *cmd != ' ' ) {
        ++cmd;
    }
    while( *cmd == ' ' ) {
        ++cmd;
    }
    o = fopen( "C:\\M8MON.TXT", "a" );
    if( o == NULL ) {
        return( 1 );
    }
    QueryPerformanceFrequency( &f );
    st = (const DWORD *)find_export( "m8d3d_stat" );
    havegdi = gdi_read( g0 );
    fprintf( o, "== M8MON %s: %d s, clock %lu, ddraw %s, gdi %s\n", cmd, secs, (unsigned long)f.LowPart,
             st != NULL ? "yes" : "not loaded yet", havegdi ? "yes" : "no (driver predates M8_ESC_STATS)" );
    fprintf( o, "ms | dd: lock n/whole/KB-read unlock n/KB-written move-ticks flip8 | gdi: counters that changed\n" );
    fflush( o );
    if( st != NULL ) {
        memcpy( st0, st, sizeof( st0 ) );
    }
    start = GetTickCount();
    end = start + (DWORD)secs * 1000UL;
    while( (long)(GetTickCount() - end) < 0 ) {
        Sleep( SAMPLE_MS );
        p = line;
        p += sprintf( p, "%6lu |", (unsigned long)(GetTickCount() - start) );
        i = 0;
        /* M8DD32.DLL is mapped only while a program uses DirectDraw. */
        if( st == NULL && (st = (const DWORD *)find_export( "m8d3d_stat" )) != NULL ) {
            memcpy( st0, st, sizeof( st0 ) );
            p += sprintf( p, " ddraw loaded |" );
            i = 1;
        } else if( st != NULL && IsBadReadPtr( st, sizeof( st1 ) ) ) {
            st = NULL;
            p += sprintf( p, " ddraw gone |" );
            i = 1;
        }
        if( st != NULL ) {
            memcpy( st1, st, sizeof( st1 ) );
            if( st1[72] != st0[72] || st1[74] != st0[74] || st1[82] != st0[82] ) {
                p += sprintf( p, " dd lock %lu/%lu/%lu unlock %lu/%lu move %lu flip8 %lu |",
                              (unsigned long)(st1[72] - st0[72]), (unsigned long)(st1[83] - st0[83]),
                              (unsigned long)((st1[73] - st0[73]) >> 10), (unsigned long)(st1[74] - st0[74]),
                              (unsigned long)((st1[75] - st0[75]) >> 10), (unsigned long)(st1[76] - st0[76]),
                              (unsigned long)(st1[82] - st0[82]) );
                i = 1;
            }
            memcpy( st0, st1, sizeof( st0 ) );
        }
        if( havegdi && gdi_read( g1 ) ) {
            int k;

            for( k = 0; k < M8_STAT_COUNT; ++k ) {
                if( g1[k] != g0[k] ) {
                    p += sprintf( p, " %s %lu", st_names[k], (unsigned long)(g1[k] - g0[k]) );
                    i = 1;
                }
            }
            memcpy( g0, g1, sizeof( g0 ) );
        }
        if( i ) {
            fprintf( o, "%s\n", line );
            fflush( o );
        }
    }
    fprintf( o, "\n" );
    fclose( o );
    return( 0 );
}
