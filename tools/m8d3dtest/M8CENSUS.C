/* M8CENSUS: what programs have asked of the Direct3D HAL since Windows started.
 *
 * Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
 *
 * Counters live in M8DD32.DLL (m8d3d.c, M8D3D_CENSUS), which stays loaded after a program exits,
 * so run this straight after it. They add up until restart: compare two reads for one program.
 * Appends to C:\M8CENSUS.TXT, under the label on the command line.
 *
 * Build: wcl386 -q -bt=nt -l=nt_win -ox -i=<watcom>h -i=<watcom>h\nt "-fe=M8CENSUS.EXE" M8CENSUS.C
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>

#define M8_DLL_BASE 0xB0C00000UL
#define CENSUS_MARK 0x4E43384DUL

/* As m8d3d.c. */
typedef struct {
    DWORD   mark;
    DWORD   state_hits[100];
    DWORD   state_values[100];
    DWORD   state_blocks[8];
    DWORD   calls_exec;
    DWORD   calls_one;
    DWORD   calls_indexed;
    DWORD   calls_prims;
    DWORD   prim_types[8];
    DWORD   other_ops;
    DWORD   tris;
    DWORD   tris_textured;
    DWORD   tris_z;
    DWORD   tris_blend;
    DWORD   tris_alphatest;
    DWORD   tris_colorkey;
    DWORD   tris_fog;
    DWORD   tris_perspective;
    DWORD   tris_clipped;
    DWORD   tri_sizes[10];
    DWORD   batch_sizes[10];
    DWORD   contexts;
    DWORD   contexts_z;
    DWORD   targets_set;
    DWORD   tex_created;
    DWORD   tex_sizes[12];
    DWORD   info_data1[16];
    DWORD   info_hits[16];
} CENSUS;

static const CENSUS *find_census( void )
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
        if( strcmp( (const char *)(b + names[i]), "m8d3d_census" ) == 0 ) {
            return( (const CENSUS *)(b + funcs[ords[i]]) );
        }
    }
    return( NULL );
}

static void buckets( FILE *o, const char *name, const DWORD *v, int n )
{
    int i;

    fprintf( o, "%s", name );
    for( i = 0; i < n; ++i ) {
        if( v[i] != 0 ) {
            fprintf( o, "  %d:%lu", 1 << i, (unsigned long)v[i] );
        }
    }
    fprintf( o, "\n" );
}

int PASCAL WinMain( HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show )
{
    FILE            *o;
    const CENSUS    *c;
    int             i;

    (void)inst;
    (void)prev;
    (void)show;
    o = fopen( "C:\\M8CENSUS.TXT", "a" );
    if( o == NULL ) {
        return( 1 );
    }
    fprintf( o, "== M8CENSUS %s\n", cmd );
    c = find_census();
    if( c == NULL || IsBadReadPtr( c, sizeof( *c ) ) || c->mark != CENSUS_MARK ) {
        fprintf( o, "no census in this driver\n\n" );
        fclose( o );
        return( 2 );
    }
    fprintf( o, "contexts %lu with Z %lu  targets set %lu  textures %lu\n", (unsigned long)c->contexts,
             (unsigned long)c->contexts_z, (unsigned long)c->targets_set, (unsigned long)c->tex_created );
    fprintf( o, "calls exec %lu one %lu indexed %lu prims %lu  other ops %lu\n", (unsigned long)c->calls_exec,
             (unsigned long)c->calls_one, (unsigned long)c->calls_indexed, (unsigned long)c->calls_prims,
             (unsigned long)c->other_ops );
    fprintf( o, "tris %lu textured %lu z %lu blend %lu alphatest %lu colorkey %lu fog %lu perspective %lu clipped %lu\n",
             (unsigned long)c->tris, (unsigned long)c->tris_textured, (unsigned long)c->tris_z,
             (unsigned long)c->tris_blend, (unsigned long)c->tris_alphatest, (unsigned long)c->tris_colorkey,
             (unsigned long)c->tris_fog, (unsigned long)c->tris_perspective, (unsigned long)c->tris_clipped );
    fprintf( o, "primitive types (1 point, 2 line, 3 strip, 4 list, 5 tri strip, 6 fan):" );
    for( i = 0; i < 8; ++i ) {
        if( c->prim_types[i] != 0 ) {
            fprintf( o, "  %d:%lu", i, (unsigned long)c->prim_types[i] );
        }
    }
    fprintf( o, "\n" );
    buckets( o, "triangle size, longer side from", c->tri_sizes, 10 );
    buckets( o, "triangles a call from", c->batch_sizes, 10 );
    buckets( o, "state pairs a block from", c->state_blocks, 8 );
    buckets( o, "texture longer side from", c->tex_sizes, 12 );
    fprintf( o, "states set (type:times/values seen):" );
    for( i = 0; i < 100; ++i ) {
        if( c->state_hits[i] != 0 ) {
            fprintf( o, "  %d:%lu/%lx", i, (unsigned long)c->state_hits[i], (unsigned long)c->state_values[i] );
        }
    }
    fprintf( o, "\nGetDriverInfo asked:" );
    for( i = 0; i < 16; ++i ) {
        if( c->info_data1[i] != 0 ) {
            fprintf( o, "  %08lx:%lu", (unsigned long)c->info_data1[i], (unsigned long)c->info_hits[i] );
        }
    }
    fprintf( o, "\n\n" );
    fclose( o );
    return( 0 );
}
