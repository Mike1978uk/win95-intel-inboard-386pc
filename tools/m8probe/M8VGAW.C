/* M8VGAW.EXE (Win16) - what a CPU access to the card's VGA memory costs on the
 * 5160's bus, beside the 8514 port path (PIX_TRANS, ~1.9 us a pixel as
 * words): worklist X8. Times 32 KB blocks written as bytes, words and dwords,
 * and read as bytes, at B8000 (the VGA's text memory, mapped while the 8514
 * side is shown, so nothing on screen changes; the block is saved and put
 * back). Also the same in system RAM, for the CPU's own share. Appends to
 * C:\M8VGAW.TXT.
 * Build: wcl -q -bt=windows -l=windows -ml -3 -fe=M8VGAW.EXE M8VGAW.C
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <dos.h>

#define BLOCK   32768U
#define REPS    40

static void fill_b( WORD sel, WORD n );
#pragma aux fill_b = "push es" "mov es, dx" "xor di, di" "mov al, 55h" "cld" "rep stosb" "pop es" \
    parm [dx] [cx] modify [ax cx di];
static void fill_w( WORD sel, WORD n );
#pragma aux fill_w = "push es" "mov es, dx" "xor di, di" "mov ax, 5555h" "cld" "rep stosw" \
    "pop es" parm [dx] [cx] modify [ax cx di];
static void fill_d( WORD sel, WORD n );
#pragma aux fill_d = ".386" "push es" "mov es, dx" "xor di, di" "db 66h" "mov ax, 5555h" "dw 5555h" "cld" \
    "db 66h" "rep stosw" "pop es" parm [dx] [cx] modify [ax cx di];
static void read_b( WORD sel, WORD n );
#pragma aux read_b = "push ds" "mov ds, dx" "xor si, si" "cld" "rep lodsb" "pop ds" \
    parm [dx] [cx] modify [ax cx si];
static void read_w( WORD sel, WORD n );
#pragma aux read_w = "push ds" "mov ds, dx" "xor si, si" "cld" "rep lodsw" "pop ds" \
    parm [dx] [cx] modify [ax cx si];

static FILE *out;

static void timed( const char *what, WORD sel, int kind, WORD n, WORD per )
{
    DWORD   t0;
    DWORD   t;
    int     i;

    t0 = GetTickCount();
    for( i = 0; i < REPS; ++i ) {
        switch( kind ) {
        case 0: fill_b( sel, n ); break;
        case 1: fill_w( sel, n ); break;
        case 2: fill_d( sel, n ); break;
        case 3: read_b( sel, n ); break;
        case 4: read_w( sel, n ); break;
        }
    }
    t = GetTickCount() - t0;
    fprintf( out, "%-24s %6lu ms  %7.3f us/byte\n", what, t,
             t * 1000.0 / ((double)REPS * n * per) );
}

static WORD sel_at( DWORD base )
{
    WORD    s = AllocSelector( HIWORD( (DWORD)(void FAR *)&out ) );

    SetSelectorBase( s, base );
    SetSelectorLimit( s, 0xFFFFUL );
    return( s );
}

int PASCAL WinMain( HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show )
{
    static BYTE save[BLOCK];
    HGLOBAL     h = GlobalAlloc( GMEM_FIXED, BLOCK );
    WORD        ram = HIWORD( (DWORD)GlobalLock( h ) );
    WORD        vga = sel_at( 0xB8000UL );

    out = fopen( "C:\\M8VGAW.TXT", "a" );
    if( out == NULL || ram == 0 || vga == 0 ) {
        return( 1 );
    }
    _fmemcpy( save, MK_FP( vga, 0 ), BLOCK );
    fprintf( out, "== %s  %u x 32 KB each\n", cmd, REPS );
    timed( "VGA B8000 write byte", vga, 0, BLOCK, 1 );
    timed( "VGA B8000 write word", vga, 1, BLOCK / 2, 2 );
    timed( "VGA B8000 write dword", vga, 2, BLOCK / 4, 4 );
    timed( "VGA B8000 read byte", vga, 3, BLOCK, 1 );
    timed( "VGA B8000 read word", vga, 4, BLOCK / 2, 2 );
    timed( "RAM write byte", ram, 0, BLOCK, 1 );
    timed( "RAM write dword", ram, 2, BLOCK / 4, 4 );
    timed( "RAM read byte", ram, 3, BLOCK, 1 );
    _fmemcpy( MK_FP( vga, 0 ), save, BLOCK );
    fclose( out );
    FreeSelector( vga );
    GlobalUnlock( h );
    GlobalFree( h );
    return( 0 );
}
